#include "core/LiveInput.h"
#include <iomanip>
#include <iostream>
int main(){unsigned channels,frames;if(!(std::cin>>channels>>frames)||channels<1||channels>2)return 2;aion::LiveInput monitor;monitor.start(0,1,0);for(unsigned i=0;i<frames;++i){float left,right;if(!(std::cin>>left))return 2;right=left;if(channels==2&&!(std::cin>>right))return 2;monitor.push(left,right);}aion::LiveInputEvent event{};monitor.publish(frames);if(!monitor.pop(event))return 2;for(auto sample:event.samples)std::cout<<std::setprecision(17)<<sample<<' ';}
