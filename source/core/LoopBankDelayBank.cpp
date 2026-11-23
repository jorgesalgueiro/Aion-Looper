// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopBank.h"
namespace aion {
bool LoopBank::requestDelayBank(const DelayBank& settings,const DelayEpochs& epochs,const DelayEpochs& revisions,
                              std::uint32_t request,std::uint64_t transaction) noexcept {
 if(!request||!transaction||pendingDelayBank->active)return false;
 for(unsigned id=0;id<trackCount;++id)
  if(pendingDelays[id].active||epochs[id]!=tracks[id].clearEpoch||revisions[id]!=tracks[id].info.delaySettingsRevision
   ||revisions[id]==UINT64_MAX||!validDelaySettings(settings[id]))return false;
 if(!delayStorage->requestBank(settings,sync.settings.bpm))return false;
 auto& pending=*pendingDelayBank;pending.settings=settings;pending.epochs=epochs;pending.revisions=revisions;
 pending.request=request;pending.transaction=transaction;pending.active=true;
 bankDelayResult={request,transaction,DelayBankStatus::pending};return true;
}
void LoopBank::cancelDelayBank() noexcept {
 if(pendingArpBank.active&&pendingArpBank.restoreEffects&&pendingArpBank.effects.restoreDelays)cancelArpBank();
 if(!pendingDelayBank->active)return;
 delayStorage->cancelBank();bankDelayResult={pendingDelayBank->request,pendingDelayBank->transaction,DelayBankStatus::cancelled};
 pendingDelayBank->active=false;
}
}
