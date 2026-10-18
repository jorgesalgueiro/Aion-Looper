#include "core/ReferenceDsp.h"
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
int main(){
 std::string action;std::size_t count,takeCount;double rate,offset,feedback,fade;
 while(std::cin>>action>>count>>takeCount>>rate>>offset>>feedback>>fade){
  if(count>1000000||takeCount>1000000)return 2;
  std::vector<float> base(count),take(takeCount);
  for(auto& sample:base)if(!(std::cin>>sample))return 2;
  for(auto& sample:take)if(!(std::cin>>sample))return 2;
  bool ok=true;
  if(action=="dc")aion::reference::removeDc(base);
  else if(action=="fade")ok=aion::reference::crossfade(base,rate,fade);
  else if(action=="normalize"){(void)aion::reference::normalize(base,take);base.insert(base.end(),take.begin(),take.end());}
  else if(action=="mix")ok=aion::reference::mixCircular(base,take,rate,offset,feedback);
  else return 2;
  std::cout<<ok<<std::setprecision(9);for(float sample:base)std::cout<<' '<<sample;std::cout<<'\n';
 }
}
