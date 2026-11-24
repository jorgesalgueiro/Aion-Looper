// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ReverbStorage.h"
#include "core/ReverbImpulse.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

namespace {
bool watching = false;
unsigned heap = 0;
void check(bool ok, const char* why) { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
struct Guard {
    Guard() { heap = 0; watching = true; }
    ~Guard() { watching = false; check(heap == 0, "scalar/streaming callback allocated or freed"); }
};
}
void* operator new(std::size_t n) { if (watching) ++heap; if (auto* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if (watching && p) ++heap; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
using Stereo = std::array<std::vector<float>, 2>;
static Stereo zeros(std::size_t n) { return {std::vector<float>(n), std::vector<float>(n)}; }
static aion::ReverbSettings settings() { aion::ReverbSettings s; s.enabled = true; s.values[2] = .1; return s; }
int main() {
    for (unsigned length : {1u, 127u, 128u, 129u, 1023u, 1024u, 1025u, 16385u}) {
        std::vector<float> ir(length), input(length + 2304), scalar(input.size()), blocked(input.size());
        for (unsigned i = 0; i < length; ++i) ir[i] = static_cast<float>(.003 * std::cos(.17 * i));
        for (unsigned i = 0; i < 1701; ++i) input[i] = static_cast<float>(.3 * std::sin(.029 * i));
        aion::NonUniformConvolver conv; check(conv.prepare(ir), "streaming kernel prepare");
        std::size_t nearTransforms = 0, tailTransforms = 0;
        for (unsigned i = 0; i < input.size(); ++i) {
            float value = input[i];
            { Guard guard; check(conv.tick(value, value), "scalar in-place kernel"); }
            scalar[i] = value;
            const auto near = conv.lastNearWork(), tail = conv.lastTailWork();
            check(near.transforms == (length > 128 && (i + 1) % 128 == 0 ? 2u : 0u), "near FFT only at 128-frame boundaries");
            check(tail.transforms == (length > 1024 && (i + 1) % 1024 == 0 ? 2u : 0u), "tail FFT only at 1024-frame boundaries");
            check(conv.lastDirectProducts() <= 128 && near.partitionProducts <= 2, "bounded immediate/near work per sample");
            const auto partitions = length > 1024 ? (length - 1024 + 1023) / 1024 : 0;
            check(tail.partitionProducts <= (partitions ? (partitions - 1 + 1023) / 1024 + 1 : 0), "bounded scalar tail product work");
            nearTransforms += near.transforms; tailTransforms += tail.transforms;
        }
        check(nearTransforms == (length > 128 ? 2 * (input.size() / 128) : 0), "near transform total independent of caller size");
        check(tailTransforms == (length > 1024 ? 2 * (input.size() / 1024) : 0), "tail transform total independent of caller size");
        for (unsigned block : {64u, 128u, 257u}) {
            { Guard guard; conv.reset(); }
            for (std::size_t at = 0; at < input.size();) {
                const auto n = std::min<std::size_t>(block, input.size() - at);
                { Guard guard; check(conv.process(std::span<const float>(input).subspan(at, n), std::span<float>(blocked).subspan(at, n)), "blocked streaming kernel"); }
                at += n;
            }
            check(scalar == blocked, "scalar kernel PCM must equal block PCM bit-exactly");
        }
        { Guard guard; conv.reset(); }
        check(!conv.lastDirectProducts() && !conv.lastNearWork().transforms && !conv.lastTailWork().transforms, "reset streaming diagnostics");
    }
    constexpr std::array<std::size_t, 9> edits{0, 71, 1207, 2301, 4003, 7009, 24001, 28003, 32000};
    for (double rate : {8000., 48000.}) {
        auto initial = settings(); aion::ReverbImpulse ir;
        check(ir.prepare(rate, initial, 123), "streaming stereo IR");
        auto input = zeros(32000), baseline = zeros(32000);
        for (unsigned i = 0; i < input[0].size(); ++i) { input[0][i] = static_cast<float>(.2 * std::sin(i * .019)); input[1][i] = static_cast<float>(.15 * std::cos(i * .031)); }
        for (unsigned block : {0u, 64u, 128u}) {
            aion::Reverb r; check(r.prepare(rate, initial, ir.channel(0), ir.channel(1)), "streaming stereo prepare");
            auto output = zeros(32000); auto s = initial;
            for (unsigned event = 0; event + 1 < edits.size(); ++event) {
                if (event == 1) { s.values[0] = 1.8; s.values[5] = .17; }
                if (event == 2) { s.values[4] = 500; s.values[1] = 1; }
                if (event == 3) s.enabled = false;
                if (event == 4) { s.values[4] = 20000; s.values[0] = .5; }
                if (event == 5) s.values[5] = .005125;
                if (event == 6) s.enabled = true;
                if (event == 7) { s.values[1] = .15; s.enabled = false; }
                { Guard guard; check(r.set(s), "streaming sample-offset stereo edit"); }
                for (auto at = edits[event]; at < edits[event + 1];) {
                    const auto n = std::min<std::size_t>(block ? block : 1, edits[event + 1] - at);
                    { Guard guard;
                      if (!block) check(r.tick(input[0][at], input[1][at], output[0][at], output[1][at]), "scalar stereo wet tick");
                      else check(r.process(std::span<const float>(input[0]).subspan(at, n), std::span<const float>(input[1]).subspan(at, n),
                                           std::span<float>(output[0]).subspan(at, n), std::span<float>(output[1]).subspan(at, n)), "blocked wet path"); }
                    at += n;
                }
            }
            if (!block) baseline = output; else check(output == baseline, "live stereo scalar/block PCM must match bit-exactly");
        }
        // Same ownership requests/edits at the same sample offsets, including
        // adoption, reset, cancellation/clear and fresh-result recovery.
        constexpr std::array<std::size_t, 12> events{0, 71, 1001, 1107, 1207, 1231, 2101, 2203, 2301, 2401, 2701, 32000};
        for (unsigned block : {0u, 64u, 128u}) {
            aion::ReverbStorage pool; check(pool.prepareStopped(rate), "streaming ownership prepare");
            { Guard guard; check(pool.request(19, initial, 123), "streaming first request"); }
            pool.serviceWorker(); { Guard guard; check(pool.poll().installed == (1u << 19), "streaming first adoption"); }
            auto output = zeros(32000);
            for (unsigned event = 0; event + 1 < events.size(); ++event) {
                const auto frame = events[event];
                { Guard guard;
                  if (frame == 71) check(pool.setControl(19, 0, .7), "scalar volume edit");
                  if (frame == 1001 || frame == 2203) { auto next = initial; next.values[2] = .2; check(pool.request(19, next, 99), "streaming replacement request"); }
                  if (frame == 1107) check(pool.setControl(19, 1, .3), "pending scalar mixer edit");
                  if (frame == 1231) check(pool.setControl(19, 4, 500) && pool.setControl(19, 5, .017) && pool.setEnabled(19, false), "live fade edits");
                  if (frame == 2101) pool.reset(19);
                  if (frame == 2301) pool.clear(19);
                  if (frame == 2401) check(pool.request(19, initial, 102), "request after cancelled clear");
                  if (frame == 2701) check(pool.setEnabled(19, true), "fresh enable"); }
                if (frame == 1207) { pool.serviceWorker(); Guard guard; check(pool.poll().installed == (1u << 19), "prepared replacement adoption"); }
                if (frame == 2401) {
                    pool.serviceWorker(); { Guard guard; check(pool.poll().installed == 0, "cancelled pending result cannot revive"); }
                    pool.serviceWorker(); { Guard guard; check(pool.poll().installed == (1u << 19), "only fresh owner installs"); }
                }
                for (auto at = frame; at < events[event + 1];) {
                    const auto n = std::min<std::size_t>(block ? block : 1, events[event + 1] - at);
                    { Guard guard;
                      if (!block) check(pool.tick(19, input[0][at], input[1][at], output[0][at], output[1][at]), "scalar ownership tick");
                      else check(pool.process(19, std::span<const float>(input[0]).subspan(at, n), std::span<const float>(input[1]).subspan(at, n),
                                              std::span<float>(output[0]).subspan(at, n), std::span<float>(output[1]).subspan(at, n)), "blocked owner render"); }
                    at += n;
                }
            }
            if (!block) baseline = output; else check(output == baseline, "scalar/block ownership lifecycle PCM must match bit-exactly");
        }
    }
    aion::NonUniformConvolver empty; float sample = 7;
    { Guard guard; check(!empty.tick(.5f, sample) && sample == 7, "unprepared scalar rejects without mutation"); }
    std::cout << "Streaming reverb passed: direct/128/1024 timing, bounded scalar work, bit-exact scalar/64/128 kernel, wet DSP and ownership lifecycle at 8/48kHz; no audio new/delete\n";
}
