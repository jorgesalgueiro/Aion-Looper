// SPDX-License-Identifier: AGPL-3.0-only
#include "Drive.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#if defined(_MSC_VER)
#define AION_DRIVE_HOT __forceinline
#else
#define AION_DRIVE_HOT inline __attribute__((always_inline))
#endif
namespace aion {
namespace {
using drive_detail::Frame;
// Elliptic polyphase design evaluated from pinned JUCE 9.0.4 FilterDesign's
// published halfband method: first stage width .06/100 dB, second .12/90 dB.
// Recurrences follow the standard allpass polyphase equations. See docs/DRIVE.md.
constexpr std::array<double,8> first{.03215564541801152,.24915790807065266,.5404473758837134,.8074738479396524,.12131531895144629,.3944572085190311,.6783496547449189,.9340219588361485};
constexpr std::array<double,8> second{.04790845477363561,.3678092845875239,.8484147912248269,.17951160525949356,.5907180164735703,0,0,0};
double finite(double x){return std::isfinite(x)?std::clamp(x,-1.e6,1.e6):0;}
double root(double x){return std::abs(x)<1.e100?std::sqrt(1+x*x):std::hypot(1.,x);}
template<std::size_t N,bool Odd>
AION_DRIVE_HOT Frame branch(Frame x,std::array<Frame,N>& state){
 const auto& coefficients=N==5?second:first;constexpr auto split=(N+1)/2;
 for(std::size_t i=Odd?split:0;i<(Odd?N:split);++i){const auto y=coefficients[i]*x+state[i];state[i]=drive_detail::flush(x-coefficients[i]*y);x=y;}return x;
}
AION_DRIVE_HOT Frame transfer(DriveKind k,Frame x){
 x=minimum(maximum(x,Frame(-1)),Frame(1));
 return k==DriveKind::overdrive?1.5*x-.5*x*x*x:x;
}
AION_DRIVE_HOT Frame integral(DriveKind k,Frame x){
 const auto a=absolute(x);const auto inside=lessEqual(a,Frame(1));
 return k==DriveKind::overdrive?select(inside,.75*x*x-.125*x*x*x*x,a-.375):select(inside,.5*x*x,a-.5);
}
AION_DRIVE_HOT Frame aa(DriveKind k,Frame x,Frame previous,Frame& primitive){
 // Only finite, bounded channel inputs reach here; the public scalar primitive
 // retains its hypot overflow fallback for independent extreme-value callers.
 if(k==DriveKind::distortion){const auto next=root(1+x*x),result=(x+previous)/(next+primitive);primitive=next;return result;}
 const auto difference=x-previous;
 const auto close=less(absolute(difference),1.e-5*(1+maximum(absolute(x),absolute(previous))));
 const auto midpoint=transfer(k,.5*(x+previous));
 const auto next=integral(k,x),before=primitive;primitive=next;
 if(drive_detail::both(close))return midpoint;
 // Do not evaluate a zero denominator in a lane selected for the midpoint.
 return select(close,midpoint,(next-before)/select(close,Frame(1),difference));
}
}
template<std::size_t N>
AION_DRIVE_HOT std::array<Drive::Frame,2> Drive::Up<N>::tick(Frame x)noexcept{return {branch<N,false>(x,state),branch<N,true>(x,state)};}
template<std::size_t N>
AION_DRIVE_HOT Drive::Frame Drive::Down<N>::tick(std::array<Frame,2> x)noexcept{const auto direct=branch<N,false>(x[0],state),odd=branch<N,true>(x[1],state);const auto y=.5*(direct+delayed);delayed=odd;return y;}
double Drive::transfer(DriveKind k,double x)noexcept{
 if(k==DriveKind::distortion)return x/root(x);
 if(k==DriveKind::overdrive){x=std::clamp(x,-1.,1.);return 1.5*x-.5*x*x*x;}
 return std::clamp(x,-1.,1.);
}
double Drive::integrated(DriveKind k,double x)noexcept{
 if(k==DriveKind::distortion)return root(x);
 const double a=std::abs(x);
 if(k==DriveKind::overdrive)return a<=1?.75*x*x-.125*x*x*x*x:a-.375;
 return a<=1?.5*x*x:a-.5;
}
double Drive::antialiased(DriveKind k,double x,double previous)noexcept{
 if(k==DriveKind::distortion)return (x+previous)/(root(x)+root(previous));
 const double difference=x-previous;
 if(std::abs(difference)<1.e-5*(1+std::max(std::abs(x),std::abs(previous))))return transfer(k,.5*(x+previous));
 return (integrated(k,x)-integrated(k,previous))/difference;
}
DriveValues Drive::mapped(const DriveSettings& s)const noexcept{
 auto v=s.values;
 v[0]=kind==DriveKind::distortion?1+s.values[0]*.15:kind==DriveKind::overdrive?1+s.values[0]*.12:1+s.values[0]*.6;
 v[1]=kind==DriveKind::distortion?(s.values[1]-.5)*1.5:kind==DriveKind::fuzz?s.values[1]*1.5:0;
 v[2]=std::tan(std::numbers::pi*std::min(s.values[2],rate*.45)/rate);return v;
}
bool Drive::prepare(double sr,DriveKind k,const DriveSettings& s)noexcept{
 if(!std::isfinite(sr)||sr<8000||sr>384000||!validDriveSettings(k,s))return false;
 rate=sr;kind=k;smoothing=1-std::exp(-1/(.01*rate));dcPole=std::exp(-2*std::numbers::pi*15/rate);current=target=mapped(s);controlsMoving=false;bypass=bypassTarget=s.enabled?1:0;ready=true;reset();return true;
}
void Drive::reset()noexcept{path={};path.primitive=Frame(kind==DriveKind::distortion?1:0);sleeping=true;}
bool Drive::set(const DriveSettings& s)noexcept{if(!ready||!validDriveSettings(kind,s))return false;target=mapped(s);controlsMoving=true;bypassTarget=s.enabled?1:0;if(sleeping&&bypass==0&&bypassTarget==0)current=target;return true;}
AION_DRIVE_HOT Drive::Frame Drive::stereo(Frame x)noexcept{
 auto& c=path;const auto a=c.up1.tick(x);std::array<Frame,2> wet{},dry{};
 // Reuse the exact previous primitive instead of recomputing it. The bias
 // input is identical across channels and constant within this base-rate frame.
 const double firstZero=antialiased(kind,current[1],c.zeroPrevious),laterZero=antialiased(kind,current[1],current[1]);c.zeroPrevious=current[1];
 for(unsigned n=0;n<2;++n){auto b=c.up2.tick(a[n]);std::array<Frame,2> w{},d{};
  for(unsigned j=0;j<2;++j){const auto value=b[j]*current[0]+current[1];w[j]=aa(kind,value,c.previous,c.primitive)-(n==0&&j==0?firstZero:laterZero);c.previous=value;d[j]=.5*(b[j]+c.dryPrevious);c.dryPrevious=b[j];}
  wet[n]=c.down2.tick(w);dry[n]=c.dry2.tick(d);
 }
 auto w=c.down1.tick(wet);const auto d=c.dry1.tick(dry);
 const auto dc=w-c.dcInput+dcPole*c.dcOutput;c.dcInput=w;c.dcOutput=drive_detail::flush(dc);
 const double g=current[2];
 if(g!=c.toneIntegrator){c.toneA1=1/(1+g*(g+std::numbers::sqrt2));c.toneIntegrator=g;}
 const double a1=c.toneA1;
 const auto v1=a1*(c.ic1+g*(dc-c.ic2)),v2=c.ic2+g*v1;
 c.ic1=drive_detail::flush(2*v1-c.ic1);c.ic2=drive_detail::flush(2*v2-c.ic2);
 w=current[3]*v2;return d*(1-current[4])+w*current[4];
}
#undef AION_DRIVE_HOT
std::array<float,2> Drive::tick(float l,float r)noexcept{
 if(isBypassed())return {l,r};
 // Once all five values have snapped to their exact destinations, repeating
 // zero-delta smoothing cannot change them. Every accepted set rechecks once.
 if(controlsMoving){
  controlsMoving=false;
  for(unsigned p=0;p<5;++p){current[p]+=smoothing*(target[p]-current[p]);if(std::abs(target[p]-current[p])<1.e-14)current[p]=target[p];else controlsMoving=true;}
 }
 bypass+=smoothing*(bypassTarget-bypass);if(std::abs(bypass-bypassTarget)<1.e-12)bypass=bypassTarget;
 if(bypass==0&&bypassTarget==0){if(!sleeping)reset();return {l,r};}
 sleeping=false;
 const Frame input{finite(l),finite(r)};const auto wet=stereo(input),out=input+bypass*(wet-input);
 return {static_cast<float>(finite(out.lane(0))),static_cast<float>(finite(out.lane(1)))};
}
bool Drive::process(const float* l,const float* r,float* ol,float* orr,unsigned frames)noexcept{
 if(!ready||frames>4096||(!l&&frames)||(!r&&frames)||(!ol&&frames)||(!orr&&frames)||(frames&&ol==orr))return false;
 for(unsigned i=0;i<frames;++i){const auto y=tick(l[i],r[i]);ol[i]=y[0];orr[i]=y[1];}return true;
}
}
