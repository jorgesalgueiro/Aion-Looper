// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopBank.h"
namespace aion {
bool LoopBank::requestReverbBank(const ReverbBank& settings,const ReverbEpochs& epochs,const ReverbEpochs& revisions,
                                 std::uint32_t request,std::uint64_t transaction) noexcept {
 if(!request||!transaction||pendingReverbBank->active)return false;
 for(unsigned id=0;id<trackCount;++id)
  if(pendingReverbs[id].active||epochs[id]!=tracks[id].clearEpoch||revisions[id]!=tracks[id].info.reverbSettingsRevision
   ||revisions[id]==UINT64_MAX||!validReverbSettings(settings[id]))return false;
 if(!reverbStorage->requestBank(settings))return false;
 auto& pending=*pendingReverbBank;pending.settings=settings;pending.epochs=epochs;pending.revisions=revisions;
 pending.request=request;pending.transaction=transaction;pending.active=true;
 bankReverbResult={request,transaction,ReverbBankStatus::pending};return true;
}
void LoopBank::cancelReverbBank() noexcept {
 if(!pendingReverbBank->active)return;
 reverbStorage->cancelBank();
 bankReverbResult={pendingReverbBank->request,pendingReverbBank->transaction,ReverbBankStatus::cancelled};
 pendingReverbBank->active=false;
}
}
