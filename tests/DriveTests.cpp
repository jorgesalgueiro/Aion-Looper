// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Drive.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <limits>
#include <memory>
#include <new>
#include <numbers>
#include <vector>
namespace {thread_local bool watching=false;std::atomic<unsigned> heapCalls{0};
void check(bool ok,const char* s){if(!ok){std::cerr<<s<<'\n';std::exit(1);}}
}
void* operator new(std::size_t n){if(watching)++heapCalls;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(watching)++heapCalls;std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace aion;
void driveStereoReferenceTests(bool&,std::atomic<unsigned>&);
using Complex=std::complex<double>;
void fft(std::vector<Complex>& a){
 const auto n=a.size();for(std::size_t i=1,j=0;i<n;++i){auto bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
 for(std::size_t len=2;len<=n;len<<=1){const auto w=std::polar(1.,-2*std::numbers::pi/len);for(std::size_t i=0;i<n;i+=len){Complex p=1;for(std::size_t j=0;j<len/2;++j){const auto u=a[i+j],v=p*a[i+j+len/2];a[i+j]=u+v;a[i+j+len/2]=u-v;p*=w;}}}
}
double aliasRatio(const std::vector<float>& pcm,unsigned fundamental){
 std::vector<Complex> spectrum(pcm.begin(),pcm.end());fft(spectrum);double alias=0;
 for(unsigned bin=1;bin<pcm.size()/2;++bin)if(bin%fundamental)alias+=std::norm(spectrum[bin]);
 return alias/std::norm(spectrum[fundamental]);
}
int main(int argc,char** argv){
 if(argc==2&&std::string_view(argv[1])=="--catalog"){
  std::cout<<std::setprecision(17)<<'[';
  for(unsigned i=0;i<3;++i){const auto k=static_cast<DriveKind>(i);if(i)std::cout<<',';
   auto values=[&](const DriveValues& v){std::cout<<'{';bool comma=false;for(unsigned p=0;p<5;++p){const auto& d=driveControls(k)[p];if(d.name.empty())continue;if(comma)std::cout<<',';comma=true;std::cout<<std::quoted(std::string(d.name))<<':'<<v[p];}std::cout<<'}';};
   std::cout<<"{\"name\":"<<std::quoted(std::string(driveNames[i]))<<",\"defaults\":";values(driveDefaults(k).values);std::cout<<",\"controls\":[";bool comma=false;
   for(const auto& d:driveControls(k)){if(d.name.empty())continue;if(comma)std::cout<<',';comma=true;std::cout<<"{\"p\":"<<std::quoted(std::string(d.name))<<",\"min\":"<<d.minimum<<",\"max\":"<<d.maximum<<",\"step\":"<<d.step<<'}';}
   std::cout<<"],\"presets\":{";comma=false;for(const auto& preset:drivePresets(k)){if(comma)std::cout<<',';comma=true;std::cout<<std::quoted(std::string(preset.name))<<':';values(preset.values);}std::cout<<"}}";
  }std::cout<<"]\n";return 0;
 }
 constexpr unsigned count=8192;std::vector<float> l(count),r(count),ol(count),orr(count),bl(count),br(count);
 for(unsigned i=0;i<count;++i){l[i]=static_cast<float>(.8*std::sin(i*.071));r[i]=static_cast<float>(.3*std::cos(i*.113));}
 for(unsigned which=0;which<3;++which){
  const auto k=static_cast<DriveKind>(which);auto settings=driveDefaults(k);settings.enabled=true;
  check(validDriveSettings(k,settings),"Original defaults");
  for(unsigned p=1;p<=drivePresets(k).size();++p){auto s=settings;check(applyDrivePreset(k,s,p)&&s.enabled&&validDriveSettings(k,s),"Preset metadata");const auto old=s;check(!applyDrivePreset(k,s,99)&&s==old,"Invalid preset atomic");}
  for(unsigned p=0;p<5;++p){auto s=settings;const auto old=s;check(!setDriveControl(k,s,p,std::numeric_limits<double>::quiet_NaN())&&s==old,"Invalid edits atomic");}
  // Independent dense numerical integration (4096 trapezoids). Predeclared
  // absolute tolerance 3e-6 covers the hard-clip breakpoints in this domain.
  for(unsigned c=0;c<128;++c){const double x=-16+c*.25,previous=std::cos(c*.37)*12;long double sum=0;
   for(unsigned j=0;j<=4096;++j){const long double t=static_cast<long double>(j)/4096;const double v=static_cast<double>(previous+t*(x-previous));sum+=Drive::transfer(k,v)*(j==0||j==4096?.5L:1.L);}
   check(std::abs(Drive::antialiased(k,x,previous)-static_cast<double>(sum/4096))<3.e-6,"Antiderivative vs numerical integral");
  }
  for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.}){
   Drive a,b;check(a.prepare(rate,k,settings)&&b.prepare(rate,k,settings),"Rate preparation");
   watching=true;
   for(unsigned pos=0;pos<count;pos+=64)check(a.process(l.data()+pos,r.data()+pos,ol.data()+pos,orr.data()+pos,64),"64 processing");
   for(unsigned pos=0;pos<count;pos+=128)check(b.process(l.data()+pos,r.data()+pos,bl.data()+pos,br.data()+pos,128),"128 processing");
   check(ol==bl&&orr==br,"64/128 bit identity");a.reset();b.reset();
   for(unsigned pos=0;pos<count;){const unsigned n=std::min(count-pos,1+(pos*13)%233);check(a.process(l.data()+pos,r.data()+pos,ol.data()+pos,orr.data()+pos,n),"Variable processing");pos+=n;}
   for(unsigned pos=0;pos<count;pos+=128)check(b.process(l.data()+pos,r.data()+pos,bl.data()+pos,br.data()+pos,128),"Control processing");
   check(ol==bl&&orr==br,"Variable-block identity");
   for(unsigned i=0;i<count;++i)check(std::isfinite(ol[i])&&std::abs(ol[i])<4,"Finite bounded output");
   watching=false;
  }
  // Reset/silence, biased silence, in-place and rapid extreme controls.
  Drive a,b;check(a.prepare(48000,k,settings)&&b.prepare(48000,k,settings),"Prepare transitions");
  auto il=l,ir=r;watching=true;
  check(a.process(l.data(),r.data(),ol.data(),orr.data(),4096)&&b.process(il.data(),ir.data(),il.data(),ir.data(),4096),"In-place processing");
  check(std::equal(ol.begin(),ol.begin()+4096,il.begin())&&std::equal(orr.begin(),orr.begin()+4096,ir.begin()),"In-place identity");
  a.reset();for(unsigned p=0;p<4096;++p){auto y=a.tick(0,0);check(y[0]==0&&y[1]==0,"Silence after explicit reset");}
  for(unsigned p=0;p<256;++p){auto s=settings;for(unsigned v=0;v<5;++v)s.values[v]=(p&1)?driveControls(k)[v].minimum:driveControls(k)[v].maximum;s.preset=0;check(a.set(s),"Rapid controls");const auto y=a.tick(p&1?1.e30f:std::numeric_limits<float>::quiet_NaN(),-.5f);check(std::isfinite(y[0])&&std::isfinite(y[1]),"Extreme input/control safety");}
  watching=false;
  // Coherent full-path spectral comparison against naive base-rate shaping.
  for(unsigned frequency:{137u,701u,1877u}){
   Drive effect;check(effect.prepare(48000,k,settings),"Spectral preparation");std::vector<float> input(count),naive(count),output(count),right(count);
   const double gain=which==0?1+settings.values[0]*.15:which==1?1+settings.values[0]*.12:1+settings.values[0]*.6;
   for(unsigned i=0;i<count;++i){input[i]=static_cast<float>(.8*std::sin(2*std::numbers::pi*frequency*i/count));naive[i]=static_cast<float>(Drive::transfer(k,gain*input[i]));}
   for(unsigned pass=0;pass<2;++pass)for(unsigned i=0;i<count;i+=128)check(effect.process(input.data()+i,input.data()+i,output.data()+i,right.data()+i,128),"Spectral processing");
   const auto before=aliasRatio(naive,frequency),after=aliasRatio(output,frequency),improvement=10*std::log10(before/after);
   std::cout<<driveNames[which]<<" frequency="<<frequency*48000./count<<" Hz alias improvement="<<improvement<<" dB\n";
   check(improvement>10,"At least 10 dB normalized alias reduction on declared matrix");
  }
  Drive timed;check(timed.prepare(48000,k,settings),"Benchmark prepare");double worst=0,total=0;
  for(unsigned i=0;i<1000;++i){watching=true;const auto start=std::chrono::steady_clock::now();check(timed.process(l.data(),r.data(),ol.data(),orr.data(),64),"Benchmark process");const auto end=std::chrono::steady_clock::now();watching=false;const auto us=std::chrono::duration<double,std::micro>(end-start).count();total+=us;worst=std::max(worst,us);}
  std::cout<<driveNames[which]<<" stereo/64 mean="<<total/1000<<" us worst="<<worst<<" us\n";
 }
 driveStereoReferenceTests(watching,heapCalls);
 check(heapCalls==0,"No callback new/delete");std::cout<<"PASS: three native drive kernels; connected integration is verified separately\n";
}
