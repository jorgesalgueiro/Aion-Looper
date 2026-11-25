// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Engine.h"
namespace aion {
struct ReverbOwnedTrack {
 ReverbSettings committed{};
 std::uint64_t clearEpoch=0,revision=0;
 std::uint32_t requestId=0;
 ReverbEditStatus status=ReverbEditStatus::ready;
 bool pending=false;
};
// Sole non-audio B request/result owner, mirroring DelaySettingsOwner. The
// enclosing Processor must serialize calls with its existing intent lock. No
// JSON, persistence or DSP duplication. Product GUI/CLI/MIDI remain gated.
// Explicit known-state binding prevents guessing across prepare/external edits.
class ReverbSettingsOwner {
public:
 using Bank=LoopBank::ReverbBank;
 explicit ReverbSettingsOwner(Engine& backend) noexcept:engine(backend){}
 bool bind(const EngineView&,const Bank& known=Bank{}) noexcept;
 void service(const EngineView&) noexcept;
 bool set(unsigned track,unsigned parameter,double value,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool room(unsigned track,ReverbRoom,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool preset(unsigned track,unsigned preset,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool enable(unsigned track,bool enabled,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool reset(unsigned track,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool restore(const Bank&,std::uint32_t request,std::uint64_t frame=0) noexcept;
 void cancel(unsigned track=LoopBank::trackCount) noexcept;
 bool bound() const noexcept{return attached&&generation==engine.generation();}
 bool restoring() const noexcept{return bank.active;}
 const ReverbOwnedTrack* track(unsigned id) const noexcept{return id<records.size()?&records[id]:nullptr;}
 const ReverbSettings* intent(unsigned id) const noexcept{return id<records.size()?(bank.active?&bank.candidate[id]:records[id].pending?&pending[id].candidate:&records[id].committed):nullptr;}
 const ReverbBankResult& restoreReceipt() const noexcept{return bankReceipt;}
private:
 struct Pending {ReverbSettings candidate{};std::uint64_t transaction=0,epoch=0,revision=0;std::uint32_t generation=0,request=0;};
 bool submit(unsigned,const ReverbSettings&,std::uint32_t,std::uint64_t) noexcept;
 void serviceBank() noexcept;
 Engine& engine;
 std::array<ReverbOwnedTrack,LoopBank::trackCount> records{};
 std::array<Pending,LoopBank::trackCount> pending{};
 struct PendingBank {Bank candidate{};LoopBank::ReverbEpochs epochs{},revisions{};std::uint64_t transaction=0;std::uint32_t generation=0,request=0;bool active=false;} bank{};
 ReverbBankResult bankReceipt{};
 std::uint32_t generation=0;
 bool attached=false;
};
}
