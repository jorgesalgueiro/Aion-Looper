// SPDX-License-Identifier: AGPL-3.0-only
#include "ReverbStorage.h"
#include <algorithm>
#include <limits>
namespace aion {
bool ReverbStorage::collectBankSlots(const Bank& settings,std::array<Slot*,trackCount>& slotsToUse) const noexcept {
    if (!rate || bankActive) return false;
    unsigned needed = 0, available = 0; std::size_t required = 0, reserved = 0;
    for (unsigned id = 0; id < trackCount; ++id) {
        if (pending[id] || remaining[id] || !validReverbSettings(settings[id])
            || installations[id] == std::numeric_limits<std::uint64_t>::max()) return false;
        if (!settings[id].enabled) continue;
        const auto bytes = reservationBytes(rate, settings[id]);
        if (!bytes || bytes > budget - required) return false;
        required += bytes; ++needed;
    }
    for (auto& slot : *slots) {
        if (slot.phase.load(std::memory_order_acquire) == Phase::free) { if (available < needed) slotsToUse[available++] = &slot; }
        else { if (slot.bytes > budget - reserved) return false; reserved += slot.bytes; }
    }
    if (available < needed || required > budget - reserved) return false;
    return true;
}
bool ReverbStorage::canRequestBank(const Bank& settings) const noexcept {
    std::array<Slot*, trackCount> slotsToUse{}; return collectBankSlots(settings, slotsToUse);
}
bool ReverbStorage::requestBank(const Bank& settings) noexcept {
    std::array<Slot*, trackCount> slotsToUse{}; if (!collectBankSlots(settings, slotsToUse)) return false;
    // Preflight completed before publishing ANY work. Only this audio owner can
    // claim free slots; worker reclamation can only increase available capacity.
    unsigned slotIndex = 0;
    for (unsigned id = 0; id < trackCount; ++id) {
        if (!settings[id].enabled) { bankSlots[id] = nullptr; continue; }
        auto& slot = *slotsToUse[slotIndex++]; slot.track = id; slot.settings = settings[id]; slot.seed = 0;
        slot.bytes = reservationBytes(rate, settings[id]); slot.bankMember = true; slot.request = 0;
        slot.revisions = intents[id].revisions;
        slot.cancelled.store(false, std::memory_order_relaxed); bankSlots[id] = &slot;
        slot.phase.store(Phase::queued, std::memory_order_release);
    }
    bankPrepared = false; bankActive = true; return true;
}
void ReverbStorage::cancelBank() noexcept {
    if (!bankActive) return;
    for (auto*& slot : bankSlots) { if (slot) slot->cancelled.store(true, std::memory_order_release); slot = nullptr; }
    bankActive = bankPrepared = false;
}
ReverbStorage::BankStatus ReverbStorage::bankStatus() const noexcept {
    if (!bankActive) return BankStatus::none;
    bool ready = true, anyFailed = false;
    for (auto* slot : bankSlots) if (slot) {
        const auto phase = slot->phase.load(std::memory_order_acquire);
        ready &= phase == Phase::ready; anyFailed |= phase == Phase::failed;
    }
    return anyFailed ? BankStatus::failed : ready ? BankStatus::ready : BankStatus::pending;
}
bool ReverbStorage::prepareBankCommit() noexcept {
    bankPrepared = false; if (bankStatus() != BankStatus::ready) return false;
    // No old owner has been touched above. A refused shape would abort the whole
    // bank before any audible change.
    for (auto* slot : bankSlots) if (slot) {
        if (!slot->graph->set(slot->settings)) return false;
        slot->graph->reset();
    }
    bankPrepared = true; return true;
}
void ReverbStorage::commitBank() noexcept {
    if (!bankActive || !bankPrepared) return;
    // Commit the entire target bank here; off targets fade old -> dry without
    // allocating a dummy disabled processor.
    for (unsigned id = 0; id < trackCount; ++id) {
        previous[id] = active[id]; active[id] = bankSlots[id];
        remaining[id] = hasOwner(id) ? fadeFrames : 0;
        failed[id] = false; ++installations[id];
        if (auto* slot = active[id]) slot->phase.store(Phase::active, std::memory_order_release);
    }
    bankSlots.fill(nullptr); bankActive = bankPrepared = false;
}
void ReverbStorage::pollBank(Completion& result) noexcept {
    const auto status = bankStatus(); if (status == BankStatus::none || status == BankStatus::pending) return;
    if (status == BankStatus::failed || !prepareBankCommit()) { cancelBank(); result.bankFailed = true; return; }
    commitBank(); result.bankInstalled = true;
}
}
