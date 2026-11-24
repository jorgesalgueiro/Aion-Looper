// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ReverbStorage.h"
#include "core/ReverbImpulse.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#include <vector>

namespace {
thread_local bool watched = false, recording = false, workerThread = false;
thread_local unsigned heap = 0;
thread_local int failAfter = -1;
thread_local std::size_t allocated = 0;
thread_local std::array<std::size_t, 100> allocationSizes{};
thread_local unsigned allocationCount = 0;
std::atomic<bool> pauseWorker{false}, reached{false}, resumeWorker{false};
void check(bool ok, const char* why) { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
struct Guard {
    Guard() { heap = 0; watched = true; }
    ~Guard() { watched = false; check(!heap, "reverb storage audio path allocated or freed"); }
};
}
void* operator new(std::size_t n) {
    if (watched) ++heap;
    if (workerThread && pauseWorker.exchange(false)) {
        reached.store(true);
        while (!resumeWorker.load()) std::this_thread::yield();
    }
    if (failAfter == 0) throw std::bad_alloc();
    if (failAfter > 0) --failAfter;
    if (auto* p = std::malloc(n ? n : 1)) { if (recording) { allocated += n; if (allocationCount < allocationSizes.size()) allocationSizes[allocationCount++] = n; } return p; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if (watched && p) ++heap; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
using aion::ReverbStorage;
using aion::ReverbSettings;
using Stereo = std::array<std::vector<float>, 2>;
static Stereo zeros(std::size_t n) { return {std::vector<float>(n), std::vector<float>(n)}; }
static ReverbSettings setting(double duration = .1) {
    ReverbSettings s; s.enabled = true; s.values[2] = duration; s.values[1] = .7; return s;
}
static void reference(aion::Reverb& r, double rate, const ReverbSettings& s, std::uint32_t seed) {
    aion::ReverbImpulse ir; check(ir.prepare(rate, s, seed), "reference IR preparation");
    check(r.prepare(rate, s, ir.channel(0), ir.channel(1)), "reference graph preparation");
}
static ReverbStorage::Completion poll(ReverbStorage& s) { Guard guard; return s.poll(); }
static void render(ReverbStorage& s, unsigned id, const Stereo& in, Stereo& out, unsigned block,
                   std::size_t begin = 0, std::size_t end = 0) {
    if (!end) end = in[0].size();
    for (std::size_t at = begin; at < end;) {
        const auto n = std::min<std::size_t>(block, end - at);
        Guard guard;
        check(s.process(id, std::span<const float>(in[0]).subspan(at, n), std::span<const float>(in[1]).subspan(at, n),
                        std::span<float>(out[0]).subspan(at, n), std::span<float>(out[1]).subspan(at, n)), "storage render");
        at += n;
    }
}
static void render(aion::Reverb& s, const Stereo& in, Stereo& out, unsigned block,
                   std::size_t begin = 0, std::size_t end = 0) {
    if (!end) end = in[0].size();
    for (std::size_t at = begin; at < end;) {
        const auto n = std::min<std::size_t>(block, end - at);
        Guard guard;
        check(s.process(std::span<const float>(in[0]).subspan(at, n), std::span<const float>(in[1]).subspan(at, n),
                        std::span<float>(out[0]).subspan(at, n), std::span<float>(out[1]).subspan(at, n)), "reference render");
        at += n;
    }
}
static void equal(const Stereo& a, const Stereo& b, double tolerance = 2.e-6) {
    for (unsigned c = 0; c < 2; ++c) for (unsigned i = 0; i < a[c].size(); ++i)
        check(std::isfinite(a[c][i]) && std::abs(double(a[c][i]) - b[c][i]) <= tolerance, "prepared-owner stereo PCM mismatch");
}
static void install(ReverbStorage& bank, unsigned id, const ReverbSettings& s, std::uint32_t seed) {
    { Guard guard; check(bank.request(id, s, seed), "request admission"); }
    bank.serviceWorker(); check(poll(bank).installed == (1u << id), "successful installation receipt");
}
static void finishFade(ReverbStorage& bank, unsigned id, unsigned frames, unsigned block) {
    auto in = zeros(frames), out = zeros(frames); render(bank, id, in, out, block);
    check(!bank.view(id).transitioning, "fade completes at frame count");
}
int main() {
    // Reserve all requested payload: temporary IR + stereo FFTs/working arrays
    // + pre-delay, including allocation-independent fixed implementation slack.
    for (double rate : {8000., 48000., 384000.}) for (double duration : {.1, .128, .128125, 5.}) {
        const auto s = setting(duration); ReverbStorage bank;
        check(bank.prepareStopped(rate), "prepare memory accounting owner");
        const auto reservation = ReverbStorage::reservationBytes(rate, s);
        { Guard guard; check(bank.request(0, s, 42) && bank.reservedBytes() == reservation, "reserve before worker"); }
        allocated = 0; allocationCount = 0; recording = true; bank.serviceWorker(); recording = false;
        if (allocated > reservation) { std::cerr << "rate=" << rate << " duration=" << duration << " payload=" << allocated << " reserved=" << reservation << '\n';
            for (unsigned i = 0; i < allocationCount; ++i) std::cerr << allocationSizes[i] << ','; std::cerr << '\n'; }
        check(allocated > 0 && allocated <= reservation, "actual preparation payload exceeds admitted reservation");
        check(poll(bank).installed == 1 && bank.graph(0), "accounted graph installed");
    }
    // Exact stereo PCM for dry->prepared and old->new 20ms crossfades. The two
    // independently rendered components continue their histories across swap.
    for (unsigned block : {64u, 128u}) {
        auto old = setting(), next = setting(.2); old.values[0] = .3; next.values[0] = 1.4; next.values[1] = .2;
        ReverbStorage bank; check(bank.prepareStopped(8000), "prepare audio owner");
        install(bank, 19, old, 12);
        aion::Reverb oldGraph, newGraph; reference(oldGraph, 8000, old, 12); reference(newGraph, 8000, next, 90);
        auto in = zeros(2048), out = zeros(2048), oldOut = zeros(2048), newOut = zeros(2048), expected = zeros(2048);
        for (unsigned i = 0; i < 2048; ++i) { in[0][i] = static_cast<float>(.3 * std::sin(i * .13)); in[1][i] = static_cast<float>(.2 * std::cos(i * .17)); }
        render(oldGraph, in, oldOut, block); render(bank, 19, in, out, block);
        for (unsigned c = 0; c < 2; ++c) for (unsigned i = 0; i < 2048; ++i) {
            const double blend = std::min(1., i / 160.);
            expected[c][i] = static_cast<float>(in[c][i] + blend * (double(oldOut[c][i]) - in[c][i]));
        }
        equal(out, expected);
        const auto oldInstallation = bank.view(19).installation;
        { Guard guard; check(bank.request(19, next, 90), "replacement request"); check(bank.setControl(19, 0, .5), "live gain during preparation"); }
        old.values[0] = .5; { Guard guard; check(oldGraph.set(old), "reference old live control"); }
        bank.serviceWorker(); check(poll(bank).installed == (1u << 19), "replacement adoption");
        const auto applied = bank.graph(19)->settings();
        check(applied.values[0] == .5 && applied.values[1] == next.values[1], "replay only fields edited after request");
        check(bank.view(19).installation > oldInstallation && bank.view(19).seed == 90, "installed request/IR identity");
        next.values[0] = .5; { Guard guard; check(newGraph.set(next), "reference new live control"); newGraph.reset(); }
        render(oldGraph, in, oldOut, block, 0, 64); render(newGraph, in, newOut, block, 0, 64); render(bank, 19, in, out, block, 0, 64);
        { Guard guard;
          check(bank.setControl(19, 4, 500) && bank.setControl(19, 5, .017) && bank.setEnabled(19, false), "live edits reach both crossfade owners");
          old.values[4] = next.values[4] = 500; old.values[5] = next.values[5] = .017; old.enabled = next.enabled = false;
          check(oldGraph.set(old) && newGraph.set(next), "reference transition controls"); }
        render(oldGraph, in, oldOut, block, 64); render(newGraph, in, newOut, block, 64); render(bank, 19, in, out, block, 64);
        for (unsigned c = 0; c < 2; ++c) for (unsigned i = 0; i < 2048; ++i) {
            const double blend = std::min(1., i / 160.);
            expected[c][i] = static_cast<float>(oldOut[c][i] + blend * (double(newOut[c][i]) - oldOut[c][i]));
        }
        equal(out, expected);
        bank.serviceWorker(); // destroys only retired old graph, no callback free
        check(bank.reservedBytes() == ReverbStorage::reservationBytes(8000, next), "old owner reservation released on worker");
        { Guard guard; check(bank.setControl(19, 4, 500) && bank.setControl(19, 5, .25), "post-adoption controls");
          check(!bank.setControl(19, 2, .3) && !bank.setControl(19, 0, NAN), "shape/invalid live edits reject"); }
    }
    // Admission rejection is atomic; ready cancellation is stale even after
    // worker completion. Retired bytes stay reserved until actual reclamation.
    {
        const auto s = setting(); const auto bytes = ReverbStorage::reservationBytes(8000, s);
        ReverbStorage tooSmall(bytes - 1); check(tooSmall.prepareStopped(8000), "small owner prepare");
        { Guard guard; check(!tooSmall.request(0, s, 1) && !tooSmall.view(0).pending && !tooSmall.reservedBytes(), "budget rejects before mutation"); }
        ReverbStorage bank(bytes * 2); check(bank.prepareStopped(8000), "budget owner prepare");
        install(bank, 0, s, 1); finishFade(bank, 0, 160, 64);
        { Guard guard; check(bank.request(0, s, 2), "two-owner budget admission"); }
        bank.serviceWorker(); { Guard guard; bank.cancel(0); }
        check(poll(bank).installed == 0 && bank.view(0).seed == 1, "ready cancellation cannot replace old graph");
        { Guard guard; check(!bank.request(1, s, 3), "retired candidate still reserves memory"); }
        bank.serviceWorker(); { Guard guard; check(bank.request(1, s, 3), "memory recovers after worker reclamation"); bank.clearAll(); }
        bank.serviceWorker(); (void)poll(bank); bank.serviceWorker();
        check(!bank.reservedBytes() && !bank.view(0).active && !bank.view(1).active, "clear/poll/reclaim no resurrection");
    }
    // Bounded 40-slot exhaustion independently of byte budget, without preparing
    // cancelled graphs. No reset/extra slot allocation to evade backpressure.
    {
        const auto s = setting(); ReverbStorage bank; check(bank.prepareStopped(8000), "capacity owner prepare");
        for (unsigned round = 0; round < 2; ++round) for (unsigned id = 0; id < 20; ++id) {
            Guard guard; check(bank.request(id, s, id), "fill fixed preparation slots"); bank.cancel(id);
        }
        { Guard guard; check(!bank.request(0, s, 9), "full fixed pool rejects"); }
        for (unsigned i = 0; i < 40; ++i) { bank.serviceWorker(); (void)poll(bank); }
        bank.serviceWorker(); check(bank.reservedBytes() == 0, "cancelled capacity fully reclaimed");
        { Guard guard; check(bank.request(0, s, 9), "pool capacity recovery"); }
        bank.serviceWorker(); { Guard guard; check(bank.setControl(0, 1, .9) && bank.setEnabled(0, false), "first-pending controls"); }
        check(poll(bank).installed == 1 && !bank.graph(0)->settings().enabled && bank.graph(0)->settings().values[1] == .9, "ready first-request edit fencing");
    }
    // All 20 tracks can coexist and hold old/new owners. Clear in the middle of
    // multiple transitions/queued work must discard every result and reservation.
    {
        const auto s = setting(); ReverbStorage bank; check(bank.prepareStopped(8000), "20-track owner");
        for (unsigned id = 0; id < 20; ++id) { install(bank, id, s, id); finishFade(bank, id, 160, 128); }
        for (unsigned id = 0; id < 20; ++id) { Guard guard; check(bank.request(id, s, 100 + id), "20 replacement owners admitted"); }
        for (unsigned id = 0; id < 10; ++id) { bank.serviceWorker(); (void)poll(bank); }
        { Guard guard; bank.clearAll(); }
        for (unsigned i = 0; i < 41; ++i) { bank.serviceWorker(); check(poll(bank).installed == 0, "late clear-all result never installs"); }
        check(bank.reservedBytes() == 0, "20-track clear retires all owners");
    }
    // Destructive reset during a replacement retires the previous owner and
    // clears the new owner's history without changing its installed identity.
    for (unsigned block : {64u, 128u}) {
        const auto s = setting(); const auto next = setting(.2);
        ReverbStorage bank; check(bank.prepareStopped(8000), "reset owner prepare");
        install(bank, 0, s, 1); finishFade(bank, 0, 160, block);
        install(bank, 0, next, 2);
        auto in = zeros(64), out = zeros(64); in[0][0] = 1; render(bank, 0, in, out, block);
        { Guard guard; check(!bank.request(0, s, 3), "replacement cannot exceed two pinned owners"); bank.reset(0); }
        check(bank.view(0).active && !bank.view(0).transitioning && bank.view(0).seed == 2, "reset retains current identity only");
        aion::Reverb fresh; reference(fresh, 8000, next, 2);
        auto test = zeros(2048), actual = zeros(2048), expected = zeros(2048); test[1][17] = .5f;
        render(bank, 0, test, actual, block); render(fresh, test, expected, block); equal(actual, expected);
        const auto before = bank.reservedBytes();
        { Guard guard;
          check(!bank.process(20, test[0], test[1], actual[0], actual[1]), "invalid processing track rejects");
          check(!bank.process(0, in[0], in[1], actual[0], actual[1]), "unequal span lengths reject");
          check(!bank.setControl(20, 0, 1) && !bank.setEnabled(20, true), "invalid control track rejects"); }
        check(bank.reservedBytes() == before, "invalid calls cannot retire/reserve storage");
        bank.serviceWorker(); check(bank.reservedBytes() == ReverbStorage::reservationBytes(8000, next), "reset old owner retires on worker");
    }
    // Every worker allocation failure preserves the old owner's audio/settings.
    // Generating a fresh IR adds two allocation sites to the stereo processor.
    unsigned oomSites = 0;
    for (int site = 0; site < 64; ++site) {
        const auto s = setting(); ReverbStorage bank; check(bank.prepareStopped(32000), "OOM owner prepare");
        install(bank, 0, s, 17); finishFade(bank, 0, 640, 64);
        aion::Reverb referenceGraph; reference(referenceGraph, 32000, s, 17);
        auto silence = zeros(640), scratch = zeros(640); render(referenceGraph, silence, scratch, 64);
        auto in = zeros(2047), out = zeros(2047), expected = zeros(2047); in[0][0] = 1; in[1][47] = -.5f;
        render(bank, 0, in, out, 64); render(referenceGraph, in, expected, 64);
        { Guard guard; check(bank.request(0, s, 23), "OOM replacement admitted"); }
        failAfter = site; bank.serviceWorker(); failAfter = -1;
        const auto completion = poll(bank);
        if (completion.installed) break;
        ++oomSites; check(completion.failed == 1 && bank.view(0).failed && bank.view(0).seed == 17, "worker failure retains installed identity");
        render(bank, 0, in, out, 128); render(referenceGraph, in, expected, 128); check(out == expected, "worker OOM preserves running PCM exactly");
        bank.serviceWorker(); { Guard guard; check(bank.request(0, s, 24), "fresh request recovers after OOM"); }
    }
    check(oomSites == 42, "all IR/stereo graph/ring allocation failures covered");
    // Real separate worker paused at its first preparation allocation. Audio
    // cancels it, queues a new identity, and never adopts the obsolete result.
    {
        const auto s = setting(); ReverbStorage bank; check(bank.prepareStopped(8000), "concurrent owner prepare");
        { Guard guard; check(bank.request(0, s, 1), "concurrent first request"); }
        reached.store(false); resumeWorker.store(false); pauseWorker.store(true);
        std::thread worker([&] { workerThread = true; bank.serviceWorker(); workerThread = false; });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!reached.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        check(reached.load(), "worker pause reached");
        { Guard guard; bank.cancel(0); check(bank.request(0, s, 2), "new identity after preparing cancellation"); }
        resumeWorker.store(true); worker.join(); check(poll(bank).installed == 0, "cancelled preparing result not adopted");
        bank.serviceWorker(); check(poll(bank).installed == 1 && bank.view(0).seed == 2, "fresh identity alone adopts");
        const auto installed = bank.view(0).installation;
        check(!bank.prepareStopped(NAN) && bank.view(0).installation == installed, "invalid stopped prepare preserves pool");
        check(bank.prepareStopped(48000) && !bank.reservedBytes() && !bank.view(0).active, "quiescent rate change clears obsolete pool");
        { Guard guard; check(!bank.request(20, s, 1), "invalid track rejects"); }
    }
    std::cout << "Reverb storage passed: 20 tracks/40 slots, stereo 64/128 crossfades, payload admission, live revision fencing, "
              << oomSites << " OOM sites, cancellation/clear/recovery and separate-worker stale-result rejection\n";
}
