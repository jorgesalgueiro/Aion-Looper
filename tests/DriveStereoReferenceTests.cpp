// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Drive.h"
#include "DriveScalarReference.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
}
void driveStereoReferenceTests(bool& watch,std::atomic<unsigned>& calls){
 using namespace aion;double maximum=0;std::uint64_t samples=0;
 for(double rate:{8000.,22050.,32000.,44100.,48000.,96000.,192000.,384000.})for(unsigned kind=0;kind<3;++kind){
  const auto k=static_cast<DriveKind>(kind);auto settings=driveDefaults(k);settings.enabled=true;
  Drive packed;ScalarDrive reference;
  check(packed.prepare(rate,k,settings)&&reference.prepare(rate,k,settings),"Stereo/scalar preparation");
  std::array<float,128> l{},r{},pl{},pr{},sl{},sr{};std::uint64_t frame=0;
  const auto compare=[&](unsigned block){for(unsigned i=0;i<block;++i)for(const auto pair:{std::array<float,2>{pl[i],sl[i]},std::array<float,2>{pr[i],sr[i]}}){
   if(std::isfinite(pair[1])){check(std::isfinite(pair[0]),"Finite accepted scalar output stays finite");maximum=std::max(maximum,std::abs(static_cast<double>(pair[0])-pair[1]));}
   else check((std::isnan(pair[0])&&std::isnan(pair[1]))||pair[0]==pair[1],"Disabled direct bypass retains scalar nonfinite semantics");
   ++samples;
  }};
  for(unsigned n=0;n<384;++n){
   // Different lane histories include a near-equal/constant lane beside a
   // changing lane. No lane mask may select the other channel's ADAA result.
   const unsigned block=n%3==0?64:n%3==1?128:1+n%127;
   for(unsigned i=0;i<block;++i){const auto at=frame+i;
    l[i]=n%9==0?0:static_cast<float>(.37*std::sin(at*.071));
    r[i]=n%7==0?1.e-35f:static_cast<float>(.23*std::cos(at*.031));
   }
   if(n%19==0){l[0]=std::numeric_limits<float>::quiet_NaN();r[0]=std::numeric_limits<float>::infinity();}
   if(n%23==0){l[block-1]=1.e30f;r[block-1]=-1.e30f;}
   settings.preset=0;settings.enabled=n%37>3;
   const auto controls=driveControls(k);
   for(unsigned p=0;p<5;++p){const double fraction=(n+p)%11==0?0:(n+p)%11==1?1:((n*13+p*17)%128)/127.;settings.values[p]=controls[p].minimum+fraction*(controls[p].maximum-controls[p].minimum);}
   watch=true;
   check(packed.set(settings)&&reference.set(settings),"Stereo/scalar live controls");
   if(n==128||n==256){packed.reset();reference.reset();}
   check(packed.process(l.data(),r.data(),pl.data(),pr.data(),block)&&reference.process(l.data(),r.data(),sl.data(),sr.data(),block),"Stereo/scalar variable blocks");
   watch=false;
   compare(block);frame+=block;
  }
  // Also cover a genuinely sleeping processor: controls edited while asleep
  // must snap without priming its audio history, then wake with the old ramp.
  settings=driveDefaults(k);
  check(packed.prepare(rate,k,settings)&&reference.prepare(rate,k,settings),"Sleeping prepare");
  settings.values[2]=2500;settings.values[4]=.75;
  if(kind!=1)settings.values[1]=.2;
  for(unsigned n=0;n<17;++n){settings.enabled=n!=0;
   for(unsigned i=0;i<64;++i){l[i]=static_cast<float>(.37*std::sin((n*64+i)*.071));r[i]=static_cast<float>(.23*std::cos((n*64+i)*.031));}
   if(!n){l[0]=std::numeric_limits<float>::quiet_NaN();r[0]=std::numeric_limits<float>::infinity();}
   watch=true;
   check(packed.set(settings)&&reference.set(settings),"Sleeping edit/wake controls");
   check(packed.process(l.data(),r.data(),pl.data(),pr.data(),64)&&reference.process(l.data(),r.data(),sl.data(),sr.data(),64),"Sleeping and wake PCM");
   watch=false;compare(64);
  }
  settings.enabled=false;
  watch=true;check(packed.set(settings)&&reference.set(settings),"Start bypass fade before reset");
  packed.reset();reference.reset();watch=false;
  check(!packed.isBypassed(),"Reset during a pending bypass fade is not a stateless bypass");
  for(unsigned n=0;n<4;++n){watch=true;
   check(packed.process(l.data(),r.data(),pl.data(),pr.data(),64)&&reference.process(l.data(),r.data(),sl.data(),sr.data(),64),"Reset preserves pending bypass fade processing");
   watch=false;compare(64);
  }
  // Run long settling phases at every rate, then retarget just one field,
  // repeat the same gesture, reset, and reprepare at another rate. Derived
  // caches must never change the accepted PCM, even after long steady runs.
  settings=driveDefaults(k);settings.enabled=true;
  check(packed.prepare(rate,k,settings)&&reference.prepare(rate,k,settings),"Steady-cache preparation");
  settings.values[2]=7500;
  watch=true;check(packed.set(settings)&&reference.set(settings),"Start settling tone");watch=false;
  const unsigned steadyBlocks=static_cast<unsigned>(std::ceil(rate*.6/128));
  for(unsigned n=0;n<steadyBlocks+8;++n){
   for(unsigned i=0;i<128;++i){l[i]=static_cast<float>(.19*std::sin((n*128+i)*.13));r[i]=static_cast<float>(.11*std::cos((n*128+i)*.17));}
   watch=true;
   if(n==steadyBlocks){settings.values[4]=.35;check(packed.set(settings)&&reference.set(settings),"Retarget one field after settling");}
   if(n==steadyBlocks+1)check(packed.set(settings)&&reference.set(settings),"Repeated accepted gesture after settling");
   if(n==steadyBlocks+2){packed.reset();reference.reset();}
   if(n==steadyBlocks+4)check(packed.prepare(48000,k,settings)&&reference.prepare(48000,k,settings),"Rate reprepare invalidates cached tone coefficient");
   check(packed.process(l.data(),r.data(),pl.data(),pr.data(),128)&&reference.process(l.data(),r.data(),sl.data(),sr.data(),128),"Steady/settling/reset cached PCM");
   watch=false;compare(128);
   check(pl==sl&&pr==sr,"Settled-work optimization retains exact accepted float PCM");
  }

 }
 check(maximum<=1e-6,"Stereo optimization preserves accepted scalar PCM within 1e-6");
 check(calls.load()==0,"Stereo/scalar rendering performs zero tracked new/delete");
 std::cout<<"PASS accepted scalar/stereo reference: "<<samples<<" samples, maximum error="<<maximum<<'\n';
}
