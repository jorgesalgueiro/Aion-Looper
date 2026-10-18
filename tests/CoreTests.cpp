#include "core/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#include <vector>
static std::atomic<bool> watch{false};
static std::atomic<int> allocations{0};
void* operator new(std::size_t n) { if(watch.load()) ++allocations; if(auto* p=std::malloc(n == 0 ? 1 : n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { if(watch.load()) ++allocations; std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
static void check(bool ok, const char* what) { if(!ok) { std::cerr<<what<<'\n'; std::exit(1); } }
int main() {
    aion::SpscQueue<unsigned, 8> q; unsigned value=0;
    for(unsigned i=0;i<7;++i) check(q.push(i),"queue capacity");
    check(!q.push(99),"overflow must reject");
    for(unsigned i=0;i<7;++i) {check(q.pop(value)&&value==i,"FIFO");} check(!q.pop(value),"underflow");
    aion::SpscQueue<unsigned,1024> threaded;
    std::thread producer([&]{for(unsigned i=0;i<100000;++i)while(!threaded.push(i))std::this_thread::yield();});
    for(unsigned i=0;i<100000;++i){while(!threaded.pop(value))std::this_thread::yield();check(value==i,"concurrent FIFO");} producer.join();
    aion::Engine a,b; a.prepare(48000); b.prepare(48000);
    check(a.enqueue({32,1.0f})&&b.enqueue({32,1.0f}),"timestamped gain");
    check(!a.enqueue({0,std::numeric_limits<float>::quiet_NaN()}),"reject nonfinite control");
    std::vector<float> x(2048,0.5f), y=x;
    float* ax[]={x.data()}; float* by[]={y.data()};
    watch=true;
    for(int offset=0;offset<2048;offset+=64){float* block[]={ax[0]+offset};a.process(block,1,64);}
    for(int offset=0;offset<2048;offset+=128){float* block[]={by[0]+offset};b.process(block,1,128);}
    watch=false;
    check(allocations==0,"callback allocation/free"); check(x==y,"64/128 block invariance");
    check(x[31]==0.0f&&x[32]>0.0f,"sample accurate command boundary");
    check(std::abs(x.back()-0.5f)<1e-5f,"smoothing reaches target"); check(a.clock()==2048,"sample clock");
    x[0]=std::numeric_limits<float>::infinity(); a.process(ax,1,1);check(x[0]==0.0f,"nonfinite audio");
    std::cout<<"PASS: queue bounds/concurrency, sample timing, smoothing, 64/128 invariance, no callback new/delete, finite output\n";
}
