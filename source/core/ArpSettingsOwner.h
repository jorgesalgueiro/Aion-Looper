// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Engine.h"
namespace aion {
struct ArpOwnedSettings {
 ArpSettings controls{};
 double bpm=120;
 std::uint32_t seed=123456789; // Explicit known-session identity, not a product random policy.
 bool operator==(const ArpOwnedSettings&) const noexcept=default;
};
struct ArpOwnedTrack {
 // Last confirmed original values. Check owner.bound() before treating this as
 // current audio after a generation change or an unexplained external edit.
 ArpOwnedSettings committed{};
 std::uint64_t clearEpoch=0,revision=0;
 std::uint32_t requestId=0;
 ArpPreparationStatus status=ArpPreparationStatus::ready;
 bool pending=false;
};
// Internal sole message-owner adapter, not a new audio engine or state importer.
// Own all Arp request/result traffic for this Engine; do not mix other receipt
// consumers. Host restore must use the external non-audio owner lock/intent path.
// No methods may run on audio. Product GUI/CLI/state binding remains separate.
class ArpSettingsOwner {
public:
 using Bank=std::array<ArpOwnedSettings,LoopBank::trackCount>;
 explicit ArpSettingsOwner(Engine& backend) noexcept:engine(backend){}
 // Bind ONLY known original values, never infer doubles/preset names from raw
 // Float32 render targets. Reject the complete bind on mismatch or pending work.
 bool bind(const EngineView&,const Bank& known=Bank{}) noexcept;
 void service(const EngineView&) noexcept;
 bool set(unsigned track,unsigned parameter,double value,double bpm,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool preset(unsigned track,unsigned preset,double bpm,std::uint32_t request,std::uint64_t frame=0) noexcept;
 bool enable(unsigned track,bool enabled,double bpm,std::uint32_t request,std::uint64_t frame=0) noexcept;
 void cancel(unsigned track=LoopBank::trackCount) noexcept;
 bool bound() const noexcept{return attached&&generation==engine.generation();}
 const ArpOwnedTrack* track(unsigned id) const noexcept{return id<records.size()?&records[id]:nullptr;}
private:
 struct Pending {ArpOwnedSettings candidate{};std::uint64_t transaction=0,epoch=0,revision=0;std::uint32_t generation=0,request=0;bool invalidated=false;};
 bool submit(unsigned,const ArpOwnedSettings&,std::uint32_t mask,bool preset,bool prepare,std::uint32_t request,std::uint64_t frame) noexcept;
 Engine& engine;
 std::array<ArpOwnedTrack,LoopBank::trackCount> records{};
 std::array<Pending,LoopBank::trackCount> pending{};
 std::uint32_t generation=0;
 bool attached=false;
};
}
