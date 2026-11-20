// SPDX-License-Identifier: AGPL-3.0-only
// Run through tools/BenchmarkDriveSnapshot.cjs; generated baseline is test-only.
#include "core/Drive.h"
#include "BaselineDrive.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>
int main(){
 static_assert(sizeof(aion::Drive)==sizeof(aion::BaselineDrive),"Specialization must preserve processor storage footprint");
 for(unsigned kind=0;kind<3;++kind)for(bool edits:{false,true}){
  auto old=std::make_unique<std::array<aion::BaselineDrive,20>>();auto now=std::make_unique<std::array<aion::Drive,20>>();
  const auto k=static_cast<aion::DriveKind>(kind);auto settings=aion::driveDefaults(k);settings.enabled=true;
  for(unsigned i=0;i<20;++i){old->at(i).prepare(96000,k,settings);now->at(i).prepare(96000,k,settings);}
  std::array<float,64> l{},r{},a{},b{},c{},d{};std::vector<double> x(2048),y(2048);
  for(unsigned block=0;block<256+2048;++block){
   for(unsigned i=0;i<64;++i){l[i]=static_cast<float>(.13*std::sin((block*64+i)*.13));r[i]=static_cast<float>(.07*std::cos((block*64+i)*.17));}
   if(edits&&block%16==0){settings.values[2]=block%32?1800:8500;settings.values[4]=block%32?.3:.9;for(unsigned i=0;i<20;++i){old->at(i).set(settings);now->at(i).set(settings);}}
   auto before=[&]{const auto t=std::chrono::steady_clock::now();for(auto& f:*old)f.process(l.data(),r.data(),a.data(),b.data(),64);return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();};
   auto after=[&]{const auto t=std::chrono::steady_clock::now();for(auto& f:*now)f.process(l.data(),r.data(),c.data(),d.data(),64);return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();};
   double u=0,v=0;if(block%2){v=after();u=before();}else{u=before();v=after();}
   if(a!=c||b!=d){std::cerr<<"PCM mismatch\n";return 1;}
   if(block>=256){x[block-256]=u;y[block-256]=v;}
  }
  std::sort(x.begin(),x.end());std::sort(y.begin(),y.end());double sx=0,sy=0;for(unsigned i=0;i<2048;++i){sx+=x[i];sy+=y[i];}
  std::cout<<"{\"kind\":"<<kind<<",\"automated\":"<<edits<<",\"oldMean\":"<<sx/2048<<",\"newMean\":"<<sy/2048<<",\"oldMedian\":"<<x[1024]<<",\"newMedian\":"<<y[1024]<<"}\n";
 }
}
