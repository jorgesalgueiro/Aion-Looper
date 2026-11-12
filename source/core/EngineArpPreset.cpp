// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
namespace aion {
bool Engine::enqueueArpPreset(LoopCommand command,const ArpSettings& settings,double bpm) noexcept {
 if(command.action!=LoopAction::arpDelay||!command.requestId||!settings.preset)return false;
 ArpRenderSettings render;
 if(!compileArpSettings(settings,bpm,command.arp.seed,render))return false;
 command.arp=render;command.arpMask=arpAllParameters;command.arpPreset=true;
 return enqueueLoop(command);
}
}
