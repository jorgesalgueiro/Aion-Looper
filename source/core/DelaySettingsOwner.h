// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Engine.h"
namespace aion {
struct DelayOwnedTrack {
 DelaySettings committed{};
 std::uint64_t clearEpoch=0,revision=0;
 std::uint32_t requestId=0;
 DelayEditStatus status=DelayEditStatus::ready;
 bool pending=false;
};
// Sole non-audio request/result owner. The enclosing Processor must serialize
// calls with its existing intent lock. No JSON, persistence or DSP duplication.
// Own all D request/result traffic for this Engine; do not mix receipt consumers.
// Explicit known-state binding prevents guessing across prepare/external edits.
class DelaySettingsOwner {
public:
 using Bank=LoopBank::DelayBank;
 explicit DelaySettingsOwner(Engine& backend) noexcept:engine(backend){}
 ~DelaySettingsOwner(){engine.delayMidiOwner(0);} // Engine outlives its sole non-audio owner.
 bool bind(const EngineView&,const Bank& known=Bank{}) noexcept;
 void service(const EngineView&) noexcept;
 void suspendMidi(bool suspended) noexcept {midiSuspended=suspended;engine.delayMidiOwner(bound()&&!midiSuspended?generation:0);}
 bool set(unsigned track,unsigned parameter,double value,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool preset(unsigned track,unsigned preset,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool enable(unsigned track,bool enabled,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool reset(unsigned track,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool repeats(unsigned track,DelayRepeatMode,unsigned count,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool restore(const Bank&,std::uint32_t request,std::uint64_t frame=0) noexcept;
 // Enclosing sole restore owner must first match this Arp receipt to its own
 // admitted request/candidate. Validate the entire fact + fresh view before
 // rebinding D; never consume the Arp queue here or infer unknown originals.
 bool acceptEffectsRestore(const EngineView&,const Bank&,const ArpBankResult&,
                           const LoopBank::DelayEpochs&,const LoopBank::DelayEpochs&) noexcept;
 void cancel(unsigned track=LoopBank::trackCount) noexcept;
 bool bound() const noexcept{return attached&&generation==engine.generation();}
 bool restoring() const noexcept{return bank.active;}
 const DelayOwnedTrack* track(unsigned id) const noexcept{return id<records.size()?&records[id]:nullptr;}
 const DelaySettings* intent(unsigned id) const noexcept{return id<records.size()?(bank.active?&bank.candidate[id]:records[id].pending?&pending[id].candidate:&records[id].committed):nullptr;}
 const DelayBankResult& restoreReceipt() const noexcept{return bankReceipt;}
private:
 struct Pending {DelaySettings candidate{};std::uint64_t transaction=0,epoch=0,revision=0;std::uint32_t generation=0,request=0;};
 bool submit(unsigned,const DelaySettings&,std::uint32_t,std::uint64_t) noexcept;
 void serviceBank() noexcept;
 Engine& engine;
 std::array<DelayOwnedTrack,LoopBank::trackCount> records{};
 std::array<Pending,LoopBank::trackCount> pending{};
 struct PendingBank {Bank candidate{};LoopBank::DelayEpochs epochs{},revisions{};std::uint64_t transaction=0;std::uint32_t generation=0,request=0;bool active=false;} bank{};
 DelayBankResult bankReceipt{};
 std::uint32_t generation=0;
 bool attached=false;
 bool midiSuspended=false;
};
}
