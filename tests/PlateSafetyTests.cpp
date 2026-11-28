// SPDX-License-Identifier: AGPL-3.0-only
// Private filter isolation supplements the retained public musical PCM tests.
#include "../source/core/PlateReverb.cpp"
#include <cstdlib>
#include <iostream>
static int failAfter=-1;static bool watched=false,countBytes=false;static unsigned heap=0;static std::size_t bytes=0;
void* operator new(std::size_t n){if(watched)++heap;if(failAfter==0)throw std::bad_alloc();if(failAfter>0)--failAfter;if(countBytes)bytes+=n;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watched&&p)++heap;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
static void modulationHistory(double rate,unsigned block){
 // Independent chronological history is deliberately longer than the ring.
 // A positive excursion must read the requested old sample, never a newer
 // sample obtained by reducing an overlong delay modulo undersized storage.
 for(float ms:aion::lineMs){
  const auto length=std::max(8u,static_cast<unsigned>(ms*.001f*float(rate)));
  std::vector<float> ring(aion::tankFrames(length,rate),0.f);
  const auto frames=3*ring.size()+block;
  std::vector<float> history(frames);
  for(size_t i=0;i<frames;++i)history[i]=float(int(i%997)-498)/512.f;
  unsigned pos=0,valid=0;
  for(size_t start=0;start<frames;start+=block)for(size_t n=start;n<std::min(frames,start+block);++n){
   for(float excursion:{-1.f,-.5f,0.f,.5f,1.f}){
    const float delay=float(length)+excursion*aion::modulationSeconds*float(rate);
    const double at=double(n)-double(delay);const auto first=static_cast<long long>(std::floor(at));
    const auto sample=[&](long long i){return i<0?0.:double(history[static_cast<size_t>(i)]);};
    const double f=at-double(first),expected=sample(first)*(1-f)+sample(first+1)*f;
    watched=true;const float actual=aion::fractionalTap(ring,pos,valid,delay);watched=false;
    check(!heap,"modulated delay reads have no heap operations");
    if(std::abs(double(actual)-expected)>2e-5){std::cerr<<"rate="<<rate<<" block="<<block<<" length="<<length<<" frame="<<n<<" delay="<<delay<<" expected="<<expected<<" actual="<<actual<<'\n';check(false,"modulated tap equals chronological fractional-delay oracle");}
   }
   ring[pos]=history[n];pos=(pos+1)%ring.size();if(valid<ring.size())++valid;
  }
 }
}
int main(){
 for(double rate:{8000.,48000.,384000.})for(unsigned block:{64u,128u})modulationHistory(rate,block);
 aion::Peak filter;filter.setup(1000,48000,1.2f);filter.gainDb(-12);
 check(filter.process(1,0)!=0,"left impulse filtered");check(filter.process(0,1)==0,"left history cannot enter right channel");check(filter.process(0,0)!=0,"left keeps its history");
 aion::PlateSettings s;s.enabled=true;s.mix=1;s.unmask=1;
 for(double rate:{8000.,48000.,384000.}){
  aion::PlateReverb prepared;countBytes=true;bytes=0;check(prepared.prepare(rate,s),"counted preparation");countBytes=false;
  check(bytes<=aion::PlateReverb::preparationBytes(rate,s),"reservation covers every stereo allocation");
  for(int site=0;site<15;++site){failAfter=site;check(!prepared.prepare(rate,s),"every preparation OOM returns false");failAfter=-1;check(prepared.sampleRate()==rate&&prepared.settings().enabled,"OOM preserves previous graph");}
  for(unsigned block:{64u,128u}){
   aion::PlateReverb dirty,fresh;check(dirty.prepare(rate,s)&&fresh.prepare(rate,s),"paired reset graphs");
   std::array<float,128> l{},r{},ol{},orr{},fl{},fr{};
   for(unsigned n=0;n<static_cast<unsigned>(rate)/block+1;++n){for(unsigned i=0;i<block;++i){l[i]=float(.2*std::sin((n*block+i)*.071));r[i]=float(.1*std::cos((n*block+i)*.029));}dirty.process(std::span(l).first(block),std::span(r).first(block),std::span(ol).first(block),std::span(orr).first(block));}
   watched=true;dirty.reset();watched=false;check(!heap,"reset has no heap operations");
   for(unsigned n=0;n<static_cast<unsigned>(rate*.3)/block+2;++n){for(unsigned i=0;i<block;++i){l[i]=n==0&&i==0?1.f:0.f;r[i]=n==0&&i==3?.3f:0.f;}
    watched=true;dirty.process(std::span(l).first(block),std::span(r).first(block),std::span(ol).first(block),std::span(orr).first(block));fresh.process(std::span(l).first(block),std::span(r).first(block),std::span(fl).first(block),std::span(fr).first(block));watched=false;check(!heap,"reset playback has no heap operations");
    for(unsigned i=0;i<block;++i)check(ol[i]==fl[i]&&orr[i]==fr[i],"lazy reset equals fresh stereo PCM through wraps");
   }
  }
 }
 std::cout<<"PASS plate safety: chronological modulated taps, independent stereo, 15 OOM sites/rate, conservative reservations, lazy reset/fresh PCM at 8/48/384k and 64/128\n";
}
