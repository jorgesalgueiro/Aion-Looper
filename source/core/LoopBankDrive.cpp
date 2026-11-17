// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopBank.h"
namespace aion {
void LoopBank::resetDrives(unsigned id)noexcept{
 auto& bank=(*drives)[id];
 for(unsigned k=0;k<3;++k)(void)bank.filters[k].prepare(rate,static_cast<DriveKind>(k),tracks[id].info.drives[k]);
 bank.prepared=true;
}
bool LoopBank::setDrives(std::uint32_t id,const DriveBank& settings,std::uint32_t request,std::uint64_t epoch,bool checkEpoch,std::uint32_t mask)noexcept{
 if(id>=trackCount||!validDriveBank(settings)||(mask&~allDriveFields)||(checkEpoch&&epoch!=tracks[id].clearEpoch))return false;
 auto& t=tracks[id];auto next=t.info.drives;
 for(unsigned k=0;k<3;++k){for(unsigned p=0;p<5;++p)if(mask&(1u<<(k*6+p)))next[k].values[p]=settings[k].values[p];
  if(mask&(1u<<(k*6+5)))next[k].enabled=settings[k].enabled;
  if(mask&(31u<<(k*6)))next[k].preset=settings[k].preset;
 }if(!validDriveBank(next))return false;
 cancelEffectsRestore();auto& bank=(*drives)[id];
 if(!bank.prepared){t.info.drives=next;resetDrives(id);}
 else for(unsigned k=0;k<3;++k)if(next[k].values!=t.info.drives[k].values||next[k].enabled!=t.info.drives[k].enabled)(void)bank.filters[k].set(next[k]);
 t.info.drives=next;t.info.driveRequest=request;return true;
}
Stereo LoopBank::tickDrive(unsigned id,unsigned k,Stereo input,bool sourceActive)noexcept{
 auto& bank=(*drives)[id];if(!bank.prepared)resetDrives(id);
 const bool active=bank.filters[k].active();const auto out=bank.filters[k].tick(input.left,input.right);
 return sourceActive||!active?Stereo{out[0],out[1]}:Stereo{};
}
}
