// SPDX-License-Identifier: AGPL-3.0-only
#include "ReverbImpulse.h"
#include <algorithm>
#include <cmath>
#include <new>
#include <numbers>
namespace aion {
std::size_t ReverbImpulse::requiredFrames(double sr,const ReverbSettings& s) noexcept {
 if(!std::isfinite(sr)||sr<8000||sr>384000||!validReverbSettings(s))return 0;
 return static_cast<std::size_t>(std::floor(sr*s.values[2]));
}
bool ReverbImpulse::prepare(double sr,const ReverbSettings& s,std::uint32_t seed,const std::atomic<bool>* cancelled) noexcept {
 const auto stop=[&]() noexcept {return cancelled&&cancelled->load(std::memory_order_relaxed);};
 const auto frames=requiredFrames(sr,s);if(!frames||stop())return false;
 if(rate==sr&&duration==s.values[2]&&decay==s.values[3]&&noiseSeed==seed&&!samples[0].empty())return true;
 try {
  std::array<std::vector<float>,2> candidate;
  // Dense, decorrelated deterministic noise; smooth endpoints avoid truncating
  // a nonzero tail. Higher Decay shortens energy distribution across its entire
  // range. Unit channel energy avoids length/rate-dependent wet gain explosion.
  const double exponent=1.5+1.5*s.values[3];
  const auto edge=std::min(frames/4,static_cast<std::size_t>(std::ceil(sr*.01)));
  const auto envelope=[&](std::size_t i) noexcept {
   const auto distance=std::min(i,frames-1-i);
   const double taper=distance<edge?.5-.5*std::cos(std::numbers::pi*static_cast<double>(distance)/static_cast<double>(edge)):1;
   return std::exp(-exponent*static_cast<double>(i)/static_cast<double>(frames))*taper;
  };
  for(unsigned c=0;c<2;++c){
   if(stop())return false;
   candidate[c].resize(frames);
   std::uint32_t noise=seed^(c?0x9e3779b9u:0x85ebca6bu);if(!noise)noise=0x6d2b79f5u;
   double sum=0,weight=0;
   for(std::size_t i=0;i<frames;++i){
    if((i&1023u)==0&&stop())return false;
    noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;
    const double white=(static_cast<double>(noise)/4294967295.)*2-1;
    const double shape=envelope(i);const auto value=static_cast<float>(white*shape);
    candidate[c][i]=value;sum+=value;weight+=shape;
   }
   if(weight<=0)return false;
   // Remove the weighted DC component without flattening the decay envelope or
   // reopening either tapered endpoint. This work is entirely off audio.
   const double mean=sum/weight;double energy=0;
   for(std::size_t i=0;i<frames;++i){
    if((i&1023u)==0&&stop())return false;
    const auto value=static_cast<float>(candidate[c][i]-mean*envelope(i));candidate[c][i]=value;energy+=static_cast<double>(value)*value;
   }
   if(!std::isfinite(energy)||energy<=0)return false;
   const double gain=1/std::sqrt(energy);
   for(std::size_t i=0;i<frames;++i){if((i&1023u)==0&&stop())return false;candidate[c][i]=static_cast<float>(candidate[c][i]*gain);}
  }
  if(stop())return false;
  samples.swap(candidate);rate=sr;duration=s.values[2];decay=s.values[3];noiseSeed=seed;return true;
 }catch(const std::bad_alloc&){return false;}
}
}
