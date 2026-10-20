#include "core/Wav.h"
#include <iostream>
#include <string>
#include <cmath>
int main(){
    unsigned channels,length;std::string rate;if(!(std::cin>>channels>>length>>rate)||channels<1||channels>2)return 2;
    std::vector<float> left(length),right(channels==2?length:0);
    for(unsigned i=0;i<length;++i)for(unsigned c=0;c<channels;++c){std::string sample;if(!(std::cin>>sample))return 2;(c?right:left)[i]=std::stof(sample);}
    const auto bytes=aion::encodeWav(left,right,std::stod(rate),channels);
    const char* hex="0123456789abcdef";for(const auto b:bytes)std::cout<<hex[b>>4]<<hex[b&15];std::cout<<'\n';
}
