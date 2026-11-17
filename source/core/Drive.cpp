// SPDX-License-Identifier: AGPL-3.0-only
#include "Drive.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace aion {
namespace {
// Elliptic polyphase design evaluated from pinned JUCE 9.0.4 FilterDesign's
// published halfband method: first stage width .06/100 dB, second .12/90 dB.
// Recurrences follow the standard allpass polyphase equations. See docs/DRIVE.md.
constexpr std::array<double,8> first{.03215564541801152,.24915790807065266,.5404473758837134,.8074738479396524,.12131531895144629,.3944572085190311,.6783496547449189,.9340219588361485};
constexpr std::array<double,8> second{.04790845477363561,.3678092845875239,.8484147912248269,.17951160525949356,.5907180164735703,0,0,0};
double finite(double x){return std::isfinite(x)?std::clamp(x,-1.e6,1.e6):0;}
double root(double x){return std::abs(x)<1.e100?std::sqrt(1+x*x):std::hypot(1.,x);}
double branch(double x,std::array<double,8>& state,bool stage2,bool odd){
 const auto& coefficients=stage2?second:first;const unsigned count=stage2?5:8,split=(count+1)/2;
 for(unsigned i=odd?split:0;i<(odd?count:split);++i){const double y=coefficients[i]*x+state[i];state[i]=x-coefficients[i]*y;if(std::abs(state[i])<1.e-30)state[i]=0;x=y;}return x;
}
}
std::array<double,2> Drive::Halfband::up(double x,bool s)noexcept{return {branch(x,state,s,false),branch(x,state,s,true)};}
double Drive::Halfband::down(std::array<double,2> x,bool s)noexcept{const auto direct=branch(x[0],state,s,false),odd=branch(x[1],state,s,true);const auto y=.5*(direct+delayed);delayed=odd;return y;}
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
 rate=sr;kind=k;smoothing=1-std::exp(-1/(.01*rate));dcPole=std::exp(-2*std::numbers::pi*15/rate);current=target=mapped(s);bypass=bypassTarget=s.enabled?1:0;ready=true;reset();return true;
}
void Drive::reset()noexcept{channels={};sleeping=true;}
bool Drive::set(const DriveSettings& s)noexcept{if(!ready||!validDriveSettings(kind,s))return false;target=mapped(s);bypassTarget=s.enabled?1:0;if(sleeping&&bypass==0&&bypassTarget==0)current=target;return true;}
double Drive::channel(Channel& c,double x)noexcept{
 const auto a=c.up1.up(x,false);std::array<double,2> wet{},dry{};
 for(unsigned n=0;n<2;++n){auto b=c.up2.up(a[n],true);std::array<double,2> w{},d{};
  for(unsigned j=0;j<2;++j){const double value=b[j]*current[0]+current[1];w[j]=antialiased(kind,value,c.previous)-antialiased(kind,current[1],c.zeroPrevious);c.previous=value;c.zeroPrevious=current[1];d[j]=.5*(b[j]+c.dryPrevious);c.dryPrevious=b[j];}
  wet[n]=c.down2.down(w,true);dry[n]=c.dry2.down(d,true);
 }
 double w=c.down1.down(wet,false);const double d=c.dry1.down(dry,false);
 const double dc=w-c.dcInput+dcPole*c.dcOutput;c.dcInput=w;c.dcOutput=std::abs(dc)<1.e-30?0:dc;
 const double g=current[2],a1=1/(1+g*(g+std::numbers::sqrt2));
 const double v1=a1*(c.ic1+g*(dc-c.ic2)),v2=c.ic2+g*v1;
 c.ic1=2*v1-c.ic1;c.ic2=2*v2-c.ic2;if(std::abs(c.ic1)<1.e-30)c.ic1=0;if(std::abs(c.ic2)<1.e-30)c.ic2=0;
 w=current[3]*v2;return d*(1-current[4])+w*current[4];
}
std::array<float,2> Drive::tick(float l,float r)noexcept{
 if(!ready)return {l,r};
 if(sleeping&&bypass==0&&bypassTarget==0)return {l,r};
 for(unsigned p=0;p<5;++p){current[p]+=smoothing*(target[p]-current[p]);if(std::abs(target[p]-current[p])<1.e-14)current[p]=target[p];}
 bypass+=smoothing*(bypassTarget-bypass);if(std::abs(bypass-bypassTarget)<1.e-12)bypass=bypassTarget;
 if(bypass==0&&bypassTarget==0){if(!sleeping)reset();return {l,r};}
 sleeping=false;
 const double left=finite(l),right=finite(r),wl=channel(channels[0],left),wr=channel(channels[1],right);
 return {static_cast<float>(finite(left+bypass*(wl-left))),static_cast<float>(finite(right+bypass*(wr-right)))};
}
bool Drive::process(const float* l,const float* r,float* ol,float* orr,unsigned frames)noexcept{
 if(!ready||frames>4096||(!l&&frames)||(!r&&frames)||(!ol&&frames)||(!orr&&frames)||(frames&&ol==orr))return false;
 for(unsigned i=0;i<frames;++i){const auto y=tick(l[i],r[i]);ol[i]=y[0];orr[i]=y[1];}return true;
}
}
