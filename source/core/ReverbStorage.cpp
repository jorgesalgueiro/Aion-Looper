// SPDX-License-Identifier: AGPL-3.0-only
// Ownership protocol adapted from Aion's existing DelayStorage; convolution
// remains HiFi-LoFi AudioFFT / Takuya Ooura (CREDITS.md, full retained notices).
#include "ReverbStorage.h"
#include "ReverbImpulse.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace aion {
namespace {
PlateSettings plateFrom(const ReverbSettings& s) noexcept {
    PlateSettings p; p.enabled = s.enabled;
    p.decay = static_cast<float>(s.plate[0]); p.damping = static_cast<float>(s.plate[1]);
    p.lowCut = static_cast<float>(s.plate[2]); p.predelay = static_cast<float>(s.plate[3]);
    p.mix = static_cast<float>(s.plate[4]); p.unmask = static_cast<float>(s.plate[5]);
    return p;
}
}
ReverbStorage::ReverbStorage(std::size_t bytes)
    : slots(std::make_unique<std::array<Slot, capacity>>()), budget(bytes) {}
bool ReverbStorage::prepareStopped(double sr) noexcept {
    if (!std::isfinite(sr) || sr < 8000 || sr > 384000) return false;
    releaseStopped(); rate = sr; fadeFrames = static_cast<unsigned>(std::ceil(.02 * sr)); return true;
}
void ReverbStorage::releaseStopped() noexcept {
    for (auto& slot : *slots) { slot.graph.reset(); slot.plate.reset(); slot.phase.store(Phase::free, std::memory_order_relaxed); }
    active.fill(nullptr); pending.fill(nullptr); previous.fill(nullptr); remaining.fill(0);
    failed.fill(false); installations.fill(0); intents = {}; rate = 0; cursor = fadeFrames = 0; serial = 0;
    bankSlots.fill(nullptr); bankActive = bankPrepared = false;
}
std::size_t ReverbStorage::reservationBytes(double sr, const ReverbSettings& s) noexcept {
    if (s.engine == ReverbEngine::machine) {
        const auto bytes = PlateReverb::preparationBytes(sr, plateFrom(s));
        return bytes ? bytes + 64 : 0;
    }
    const auto bytes = Reverb::preparationBytes(sr, s);
    return bytes ? bytes + ReverbImpulse::requiredBytes(sr, s) + 2 * 64 : 0;
}
std::size_t ReverbStorage::reservedBytes() const noexcept {
    std::size_t total = 0;
    for (const auto& slot : *slots)
        if (slot.phase.load(std::memory_order_acquire) != Phase::free) total += slot.bytes;
    return total;
}
bool ReverbStorage::request(unsigned id, const ReverbSettings& s, std::uint32_t seed) noexcept {
    if (id >= trackCount || !rate || bankActive || pending[id] || remaining[id]
        || serial == std::numeric_limits<std::uint64_t>::max()
        || installations[id] == std::numeric_limits<std::uint64_t>::max()) return false;
    const auto bytes = reservationBytes(rate, s);
    if (!bytes) return false;
    std::size_t used = 0; Slot* available = nullptr;
    for (auto& slot : *slots) {
        if (slot.phase.load(std::memory_order_acquire) == Phase::free) { if (!available) available = &slot; }
        else { if (slot.bytes > budget - used) return false; used += slot.bytes; }
    }
    if (!available || bytes > budget - used) return false;
    auto& slot = *available; slot.track = id; slot.bytes = bytes; slot.seed = seed;
    slot.settings = s; slot.revisions = intents[id].revisions; slot.plateRevisions = intents[id].plateRevisions; slot.request = ++serial;
    slot.cancelled.store(false, std::memory_order_relaxed);
    pending[id] = &slot; failed[id] = false;
    slot.phase.store(Phase::queued, std::memory_order_release); return true;
}
void ReverbStorage::cancel(unsigned id) noexcept {
    if (id >= trackCount) return;
    if (auto* slot = pending[id]) slot->cancelled.store(true, std::memory_order_release);
    pending[id] = nullptr; failed[id] = false;
}
void ReverbStorage::clear(unsigned id) noexcept {
    if (id >= trackCount) return;
    cancel(id);
    if (auto* slot = active[id]) { active[id] = nullptr; slot->phase.store(Phase::retired, std::memory_order_release); }
    if (auto* slot = previous[id]) { previous[id] = nullptr; slot->phase.store(Phase::retired, std::memory_order_release); }
    remaining[id] = 0;
}
void ReverbStorage::clearAll() noexcept { for (unsigned id = 0; id < trackCount; ++id) clear(id); }
void ReverbStorage::reset(unsigned id) noexcept {
    if (id >= trackCount) return;
    cancel(id);
    if (auto* slot = previous[id]) { previous[id] = nullptr; slot->phase.store(Phase::retired, std::memory_order_release); }
    remaining[id] = 0;
    if (auto* slot = active[id]) { if (slot->plate) slot->plate->reset(); else slot->graph->reset(); }
}
bool ReverbStorage::setControl(unsigned id, unsigned control, double value) noexcept {
    if (id >= trackCount || serial == std::numeric_limits<std::uint64_t>::max()) return false;
    auto* owner = active[id] ? active[id] : pending[id];
    if (!owner) return false;
    if (owner->settings.engine == ReverbEngine::machine) {
        if (control >= 6) return false;
        auto s = active[id] ? active[id]->settings : owner->settings;
        if (!setPlateControl(s, control, value)) return false;
        if (active[id]) {
            active[id]->settings = s;
            if (active[id]->plate) (void)active[id]->plate->set(plateFrom(s));
        }
        if (auto* old = previous[id]; old && old->plate) {
            auto oldSettings = old->settings; (void)setPlateControl(oldSettings, control, value);
            (void)old->plate->set(plateFrom(oldSettings)); old->settings = oldSettings;
        }
        intents[id].plateValues[control] = value; intents[id].plateRevisions[control] = ++serial; return true;
    }
    const auto found = std::find(liveControls.begin(), liveControls.end(), control);
    if (found == liveControls.end()) return false;
    auto s = active[id] ? active[id]->graph->settings() : owner->settings;
    if (!setReverbControl(s, control, value)) return false;
    if (active[id]) (void)active[id]->graph->set(s);
    if (auto* old = previous[id]) {
        auto oldSettings = old->graph->settings(); (void)setReverbControl(oldSettings, control, value);
        (void)old->graph->set(oldSettings);
    }
    const auto index = static_cast<std::size_t>(found - liveControls.begin());
    intents[id].values[index] = value; intents[id].revisions[index] = ++serial; return true;
}
bool ReverbStorage::setEnabled(unsigned id, bool enabled) noexcept {
    if (id >= trackCount || (!active[id] && !pending[id]) || serial == std::numeric_limits<std::uint64_t>::max()) return false;
    if (active[id] && active[id]->settings.engine == ReverbEngine::machine) {
        for (auto* slot : {active[id], previous[id]}) if (slot) {
            if (!slot->plate) continue;
            auto s = slot->settings; s.enabled = enabled; (void)slot->plate->set(plateFrom(s)); slot->settings = s;
        }
        intents[id].enabled = enabled; intents[id].revisions[4] = ++serial; return true;
    }
    for (auto* slot : {active[id], previous[id]}) if (slot) {
        auto s = slot->graph->settings(); s.enabled = enabled; (void)slot->graph->set(s);
    }
    intents[id].enabled = enabled; intents[id].revisions[4] = ++serial; return true;
}
bool ReverbStorage::set(unsigned id, const ReverbSettings& settings) noexcept {
    if (id >= trackCount || !active[id] || !validReverbSettings(settings)
        || serial > std::numeric_limits<std::uint64_t>::max() - 8) return false;
    if (active[id]->settings.engine != settings.engine) return false; // engine change needs a prepared request
    if (settings.engine == ReverbEngine::machine) {
        if (!active[id]->plate || !active[id]->plate->set(plateFrom(settings))) return false;
        active[id]->settings = settings;
        if (auto* old = previous[id]; old && old->plate) {
            auto oldSettings = old->settings; oldSettings.plate = settings.plate; oldSettings.enabled = settings.enabled;
            (void)old->plate->set(plateFrom(oldSettings)); old->settings = oldSettings;
        }
        for (unsigned i = 0; i < 6; ++i) intents[id].plateValues[i] = settings.plate[i];
        intents[id].enabled = settings.enabled; intents[id].revisions[4] = ++serial; return true;
    }
    // Reject a shape edit before mutating either pinned owner or its intents.
    if (!active[id]->graph->set(settings)) return false;
    if (auto* old = previous[id]) {
        auto oldSettings = old->graph->settings();
        for (const auto control : liveControls) (void)setReverbControl(oldSettings, control, settings.values[control]);
        oldSettings.enabled = settings.enabled;
        (void)old->graph->set(oldSettings);
    }
    for (std::size_t i = 0; i < liveControls.size(); ++i) {
        intents[id].values[i] = settings.values[liveControls[i]]; intents[id].revisions[i] = ++serial;
    }
    intents[id].enabled = settings.enabled; intents[id].revisions[4] = ++serial; return true;
}
ReverbStorage::Completion ReverbStorage::poll(bool deferBank) noexcept {
    Completion completion;
    if (!deferBank) pollBank(completion);
    for (auto& slot : *slots) {
        const auto phase = slot.phase.load(std::memory_order_acquire);
        if (phase != Phase::ready && phase != Phase::failed) continue;
        const auto id = slot.track;
        if (slot.bankMember && bankActive && bankSlots[id] == &slot) continue;
        if (pending[id] != &slot) { slot.phase.store(Phase::retired, std::memory_order_release); continue; }
        pending[id] = nullptr;
        if (phase == Phase::ready) {
            if (slot.settings.engine == ReverbEngine::machine) {
                auto s = slot.settings;
                for (unsigned i = 0; i < 6; ++i)
                    if (slot.plateRevisions[i] != intents[id].plateRevisions[i]) (void)setPlateControl(s, i, intents[id].plateValues[i]);
                if (slot.revisions[4] != intents[id].revisions[4]) s.enabled = intents[id].enabled;
                if (slot.plate) { (void)slot.plate->set(plateFrom(s)); slot.plate->reset(); }
                slot.settings = s;
            } else {
                auto s = slot.graph->settings();
                for (unsigned i = 0; i < liveControls.size(); ++i)
                    if (slot.revisions[i] != intents[id].revisions[i]) (void)setReverbControl(s, liveControls[i], intents[id].values[i]);
                if (slot.revisions[4] != intents[id].revisions[4]) s.enabled = intents[id].enabled;
                (void)slot.graph->set(s);
                // This graph has never rendered. Latch the latest accepted targets
                // before its one adoption fade, rather than ramping obsolete preset
                // values (especially a now-disabled wet path) into the transition.
                slot.graph->reset();
            }
            previous[id] = active[id]; remaining[id] = fadeFrames;
            // Installation is a per-track adoption counter, for BOTH individual
            // requests and banks. Mixing the request serial with bank increments
            // can repeat a stamp and turn a committed edit into a false cancel.
            active[id] = &slot; failed[id] = false; ++installations[id];
            slot.phase.store(Phase::active, std::memory_order_release); completion.installed |= 1u << id;
        } else {
            failed[id] = true; completion.failed |= 1u << id;
            slot.phase.store(Phase::retired, std::memory_order_release);
        }
    }
    return completion;
}
bool ReverbStorage::process(unsigned id, std::span<const float> left, std::span<const float> right,
                             std::span<float> outLeft, std::span<float> outRight) noexcept {
    if (id >= trackCount || left.size() != right.size() || left.size() != outLeft.size()
        || left.size() != outRight.size() || left.size() > Reverb::maximumBlockFrames) return false;
    for (std::size_t offset = 0; offset < left.size();) {
        const auto n = std::min(scratch[0].size(), left.size() - offset);
        std::copy_n(left.data() + offset, n, scratch[0].data()); std::copy_n(right.data() + offset, n, scratch[1].data());
        const auto l = std::span<const float>(scratch[0]).first(n), r = std::span<const float>(scratch[1]).first(n);
        const auto ol = outLeft.subspan(offset, n), oright = outRight.subspan(offset, n);
        if (auto* slot = active[id]) { if (slot->plate) (void)slot->plate->process(l, r, ol, oright); else (void)slot->graph->process(l, r, ol, oright); }
        else { std::copy_n(l.data(), n, ol.data()); std::copy_n(r.data(), n, oright.data()); }
        if (remaining[id]) {
            auto oldL = std::span<float>(scratch[2]).first(n), oldR = std::span<float>(scratch[3]).first(n);
            if (auto* old = previous[id]) { if (old->plate) (void)old->plate->process(l, r, oldL, oldR); else (void)old->graph->process(l, r, oldL, oldR); }
            else { std::copy_n(l.data(), n, oldL.data()); std::copy_n(r.data(), n, oldR.data()); }
            for (std::size_t i = 0; i < n && remaining[id]; ++i) {
                const double blend = 1. - static_cast<double>(remaining[id]) / fadeFrames;
                ol[i] = static_cast<float>(oldL[i] + blend * (double(ol[i]) - oldL[i]));
                oright[i] = static_cast<float>(oldR[i] + blend * (double(oright[i]) - oldR[i]));
                if (!--remaining[id]) if (auto* old = previous[id]) {
                    previous[id] = nullptr; old->phase.store(Phase::retired, std::memory_order_release);
                }
            }
        }
        offset += n;
    }
    return true;
}
ReverbStorage::View ReverbStorage::view(unsigned id) const noexcept {
    return id < trackCount ? View{active[id] != nullptr, pending[id] != nullptr || bankActive, failed[id], remaining[id] != 0, installations[id], active[id] ? active[id]->seed : 0} : View{};
}
bool ReverbStorage::tick(unsigned id, float left, float right, float& outLeft, float& outRight) noexcept {
    return process(id, std::span<const float>(&left, 1), std::span<const float>(&right, 1),
                   std::span<float>(&outLeft, 1), std::span<float>(&outRight, 1));
}
const Reverb* ReverbStorage::graph(unsigned id) const noexcept {
    return id < trackCount && active[id] && active[id]->graph ? &*active[id]->graph : nullptr;
}
const PlateReverb* ReverbStorage::plateGraph(unsigned id) const noexcept {
    return id < trackCount && active[id] && active[id]->plate ? &*active[id]->plate : nullptr;
}
void ReverbStorage::serviceWorker() noexcept {
    for (auto& slot : *slots) if (slot.phase.load(std::memory_order_acquire) == Phase::retired) {
        slot.graph.reset(); slot.plate.reset(); slot.phase.store(Phase::free, std::memory_order_release);
    }
    for (unsigned i = 0; i < capacity; ++i) {
        auto& slot = (*slots)[cursor]; cursor = (cursor + 1) % capacity;
        if (slot.phase.load(std::memory_order_acquire) != Phase::queued) continue;
        slot.phase.store(Phase::preparing, std::memory_order_relaxed); bool ready = false;
        if (!slot.cancelled.load(std::memory_order_acquire)) {
            if (slot.settings.engine == ReverbEngine::machine) {
                slot.plate.emplace(); ready = slot.plate->prepare(rate, plateFrom(slot.settings), &slot.cancelled);
            } else {
                ReverbImpulse impulse;
                if (impulse.prepare(rate, slot.settings, slot.seed, &slot.cancelled)) {
                    slot.graph.emplace(); ready = slot.graph->prepare(rate, slot.settings, impulse.channel(0), impulse.channel(1), &slot.cancelled);
                }
            }
        }
        slot.phase.store(ready ? Phase::ready : Phase::failed, std::memory_order_release); break;
    }
}
}
