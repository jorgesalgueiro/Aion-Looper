// SPDX-License-Identifier: AGPL-3.0-only
#include "core/Reverb.h"
#include "core/ReverbImpulse.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <vector>

namespace {
bool watching = false;
unsigned heap = 0;
int failAfter = -1, cancelAfter = -1;
std::atomic<bool>* cancelOnAllocation = nullptr;
double peakError = 0;
void check(bool ok, const char* why) { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
struct Guard {
    Guard() { heap = 0; watching = true; }
    ~Guard() { watching = false; check(!heap, "Reverb audio-owner path allocated/freed"); }
};
}
void* operator new(std::size_t n) {
    if (watching) ++heap;
    if (failAfter == 0) throw std::bad_alloc();
    if (failAfter > 0) --failAfter;
    if (auto* p = std::malloc(n ? n : 1)) {
        if (cancelOnAllocation && (cancelAfter < 0 || cancelAfter-- == 0)) cancelOnAllocation->store(true);
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if (watching && p) ++heap; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
using aion::Reverb;
using aion::ReverbSettings;
using Stereo = std::array<std::vector<float>, 2>;
static Stereo zeros(std::size_t n) { return {std::vector<float>(n), std::vector<float>(n)}; }
static Stereo impulse(double rate, const ReverbSettings& s) {
    auto ir = zeros(aion::ReverbImpulse::requiredFrames(rate, s));
    ir[0][0] = 1; ir[1][0] = -.4f;
    ir[0][7] = .2f; ir[1][3] = .3f;
    if (ir[0].size() > 1100) { ir[0][1099] = .1f; ir[1][1051] = -.2f; }
    return ir;
}
static void prepare(Reverb& r, double rate, const ReverbSettings& s, const Stereo& ir) {
    check(r.prepare(rate, s, ir[0], ir[1]), "stereo prepare");
}
static void render(Reverb& r, const Stereo& in, Stereo& out, unsigned block,
                   std::size_t begin = 0, std::size_t end = 0) {
    if (!end) end = in[0].size();
    constexpr std::array<unsigned, 6> sizes{1, 63, 0, 257, 128, 511};
    std::size_t call = 0;
    for (auto at = begin; at < end;) {
        const auto n = std::min<std::size_t>(block ? block : sizes[call++ % sizes.size()], end - at);
        Guard guard;
        check(r.process(std::span<const float>(in[0]).subspan(at, n), std::span<const float>(in[1]).subspan(at, n),
                        std::span<float>(out[0]).subspan(at, n), std::span<float>(out[1]).subspan(at, n)), "bounded stereo process");
        at += n;
    }
}
static void equal(const Stereo& actual, const Stereo& expected, double tolerance = 3.e-6) {
    for (unsigned c = 0; c < 2; ++c) for (std::size_t i = 0; i < actual[c].size(); ++i) {
        const auto error = std::abs(static_cast<double>(actual[c][i]) - expected[c][i]);
        peakError = std::max(peakError, error);
        if (!std::isfinite(actual[c][i]) || error > tolerance) {
            std::cerr << "channel=" << c << " frame=" << i << " actual=" << actual[c][i] << " expected=" << expected[c][i] << " error=" << error << '\n';
            check(false, "stereo reference tolerance");
        }
    }
}
// Independent offline oracle: direct sparse FIR, fractional delay and direct-
// form biquad bilinear coefficients. No production convolver/TPT/read helper.
static Stereo oracle(double rate, const ReverbSettings& s, const Stereo& ir, const Stereo& in) {
    auto out = zeros(in[0].size());
    const double delay = s.values[5] * rate, fraction = delay - std::floor(delay);
    const auto integer = static_cast<std::size_t>(delay);
    const double g = std::tan(std::numbers::pi * std::min(s.values[4], .45 * rate) / rate);
    const double d = 1 + 2 * g + g * g, b0 = g * g / d, b1 = 2 * b0, b2 = b0;
    const double a1 = 2 * (g * g - 1) / d, a2 = (1 - 2 * g + g * g) / d;
    for (unsigned c = 0; c < 2; ++c) {
        std::vector<double> delayed(in[c].size()), convolved(in[c].size());
        for (std::size_t i = 0; i < in[c].size(); ++i) {
            const double first = i >= integer ? in[c][i - integer] : 0.;
            const double second = i > integer ? in[c][i - integer - 1] : 0.;
            delayed[i] = first + fraction * (second - first);
        }
        for (std::size_t j = 0; j < ir[c].size(); ++j) if (ir[c][j] != 0)
            for (std::size_t i = j; i < convolved.size(); ++i) convolved[i] += ir[c][j] * delayed[i - j];
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (std::size_t i = 0; i < in[c].size(); ++i) {
            const double x = convolved[i], y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            out[c][i] = s.enabled ? static_cast<float>(s.values[0] * ((1 - s.values[1]) * in[c][i] + s.values[1] * y)) : in[c][i];
        }
    }
    return out;
}
int main() {
    unsigned cases = 0;
    for (double rate : {8000., 22050., 32000., 44100., 48000., 96000., 192000., 384000.}) {
        for (double delay : {0., 3.375 / rate, .25}) {
            ReverbSettings s; s.enabled = true; s.values[2] = .1;
            s.values[0] = 1.7; s.values[1] = .63; s.values[4] = delay == 0 ? 20000 : 500; s.values[5] = delay;
            const auto ir = impulse(rate, s);
            auto in = zeros(static_cast<std::size_t>(std::ceil(rate * delay)) + 5000);
            for (unsigned i = 0; i < 1801; ++i) {
                in[0][i] = static_cast<float>(.3 * std::sin(i * .17));
                in[1][i] = static_cast<float>(.2 * std::cos(i * .071));
            }
            const auto expected = oracle(rate, s, ir, in);
            for (unsigned block : {64u, 128u, 0u}) {
                Reverb r; prepare(r, rate, s, ir); auto out = zeros(in[0].size());
                render(r, in, out, block); equal(out, expected);
                check(r.sampleRate() == rate && r.predelayStorageFrames() == static_cast<std::size_t>(std::ceil(rate * .25)) + 2, "rate and prepared delay capacity");
                { Guard guard; r.reset(); }
                out = in; render(r, out, out, block); equal(out, expected);
                ++cases;
            }
        }
    }
    // All 17 original preset values are admitted to the native wet processor.
    // This is control/stability coverage, not a listening approval of presets.
    for (unsigned p = 1; p <= aion::reverbPresetNames.size(); ++p) {
        ReverbSettings s; check(aion::applyReverbPreset(s, p), "preset setting"); s.enabled = true;
        const auto ir = impulse(8000, s); Reverb r; prepare(r, 8000, s, ir);
        auto in = zeros(4096), out = zeros(4096); in[0][0] = 1; in[1][3] = -.5f;
        render(r, in, out, 64); equal(out, oracle(8000, s, ir, in));
    }
    // Complete generated-IR -> stereo wet processor path against direct FIR,
    // not only sparse synthetic coefficients or kernel-only impulse checks.
    for (double rate : {8000., 48000.}) {
        ReverbSettings s; s.enabled = true; s.values[2] = .1; s.values[1] = .8; s.values[5] = 3.375 / rate; s.values[4] = 2500;
        aion::ReverbImpulse generated; check(generated.prepare(rate, s, 8123), "generated stereo IR preparation");
        const Stereo ir{std::vector<float>(generated.channel(0).begin(), generated.channel(0).end()),
                        std::vector<float>(generated.channel(1).begin(), generated.channel(1).end())};
        auto in = zeros(ir[0].size() + 512), out = zeros(in[0].size());
        for (unsigned i = 0; i < 127; ++i) { in[0][i] = static_cast<float>(.3 * std::sin(i * .17)); in[1][i] = static_cast<float>(.25 * std::cos(i * .03)); }
        const auto expected = oracle(rate, s, ir, in);
        for (unsigned block : {64u, 128u}) { Reverb r; prepare(r, rate, s, ir); render(r, in, out, block); equal(out, expected); }
    }
    // Silent opposite channel must stay silent; exact aligned swapped in-place
    // buffers must read both originals before either channel is overwritten.
    {
        ReverbSettings s; s.enabled = true; s.values[2] = .1;
        const auto ir = impulse(48000, s); Reverb r; prepare(r, 48000, s, ir);
        auto in = zeros(4096), out = zeros(4096); in[0][7] = 1;
        render(r, in, out, 64);
        for (auto v : out[1]) check(v == 0, "no stereo crossfeed");
        { Guard guard; r.reset(); }
        auto swapped = in;
        { Guard guard; check(r.process(swapped[0], swapped[1], swapped[1], swapped[0]), "swapped in-place"); }
        std::swap(swapped[0], swapped[1]); equal(swapped, out);
    }
    // Analytic one-pole gain/mix/bypass ramps with a silent wet IR.
    for (unsigned block : {64u, 128u}) {
        constexpr double rate = 8000;
        ReverbSettings s; s.enabled = true; s.values[2] = .1; s.values[1] = 0;
        const auto ir = zeros(800); Reverb r; prepare(r, rate, s, ir);
        auto in = zeros(12000), out = zeros(12000), expected = zeros(12000);
        std::fill(in[0].begin(), in[0].end(), .3f); std::fill(in[1].begin(), in[1].end(), -.2f);
        s.values[0] = 2; s.values[1] = .4;
        { Guard guard; check(r.set(s), "live gains"); }
        double mix = 0, gain = 1, enable = 1; const double k = 1 - std::exp(-1 / (.01 * rate));
        render(r, in, out, block, 0, 2000);
        s.enabled = false; { Guard guard; check(r.set(s), "live bypass"); }
        render(r, in, out, block, 2000, 6000);
        s.enabled = true; { Guard guard; check(r.set(s), "reenable without stale tail"); }
        render(r, in, out, block, 6000, 12000);
        for (unsigned i = 0; i < in[0].size(); ++i) {
            mix += k * (.4 - mix); gain += k * (2 - gain);
            if (i == 6000) { enable = 0; mix = .4; gain = 2; }
            const double target = i >= 2000 && i < 6000 ? 0. : 1.;
            enable += k * (target - enable); if (std::abs(target - enable) < 1.e-12) enable = target;
            for (unsigned c = 0; c < 2; ++c) expected[c][i] = static_cast<float>(in[c][i] + enable * (gain * (1 - mix) * in[c][i] - in[c][i]));
        }
        equal(out, expected);
    }
    // A settled bypass must retire late pre-delay/convolution/filter memory.
    // Reenable on silence before a former 7000-frame IR tap would have sounded.
    for (unsigned block : {64u, 128u}) {
        ReverbSettings s; s.enabled = true; s.values[1] = 1;
        auto ir = zeros(9600); ir[0][7000] = 1; ir[1][7003] = -.7f;
        Reverb r; prepare(r, 8000, s, ir);
        auto in = zeros(15000), out = zeros(15000); in[0][0] = in[1][0] = 1;
        render(r, in, out, block, 0, 1300);
        s.enabled = false; { Guard guard; check(r.set(s), "late-tail bypass"); }
        render(r, in, out, block, 1300, 5000);
        s.enabled = true; { Guard guard; check(r.set(s), "late-tail reenable"); }
        render(r, in, out, block, 5000, 15000);
        for (const auto& channel : out) for (std::size_t i = 5000; i < channel.size(); ++i)
            check(channel[i] == 0, "settled bypass cannot resurrect old late tail");
    }
    // Dry endpoint keeps the original post-mix volume; bypass ignores volume.
    for (bool enabled : {false, true}) {
        ReverbSettings s; s.enabled = enabled; s.values[2] = .1; s.values[1] = 0; s.values[0] = 2.3;
        const auto ir = impulse(8000, s); Reverb r; prepare(r, 8000, s, ir);
        auto in = zeros(4096), out = zeros(4096), expected = zeros(4096);
        for (unsigned i = 0; i < 4096; ++i) for (unsigned c = 0; c < 2; ++c) {
            in[c][i] = static_cast<float>(.2 * std::sin(.31 * i + c));
            expected[c][i] = static_cast<float>(in[c][i] * (enabled ? 2.3 : 1.));
        }
        render(r, in, out, 128); equal(out, expected);
    }
    // Constant-sum pre-delay changes must not create a gain bump on steady DC.
    for (unsigned block : {64u, 128u}) {
        ReverbSettings s; s.enabled = true; s.values[2] = .1; s.values[1] = 1;
        auto ir = zeros(800); ir[0][0] = ir[1][0] = 1;
        Reverb r; prepare(r, 8000, s, ir);
        auto in = zeros(10000), out = zeros(10000);
        std::fill(in[0].begin(), in[0].end(), .1f); std::fill(in[1].begin(), in[1].end(), -.15f);
        render(r, in, out, block, 0, 4000);
        for (std::size_t at = 4000; at < 8000; at += 20) {
            s.values[5] = (at / 20) % 2 ? .25 : 0.;
            { Guard guard; check(r.set(s), "rapid predelay request"); }
            render(r, in, out, block, at, at + 20);
        }
        render(r, in, out, block, 8000, 10000);
        for (unsigned c = 0; c < 2; ++c) for (unsigned i = 4000; i < 10000; ++i)
            check(std::abs(out[c][i] - in[c][i]) < 2.e-6, "predelay crossfade has no DC boost/drop");
    }
    // Live commands at identical sample offsets must not depend on block size.
    {
        ReverbSettings initial; initial.enabled = true; initial.values[2] = .1;
        const auto ir = impulse(32000, initial); auto in = zeros(28000);
        for (unsigned i = 0; i < in[0].size(); ++i) { in[0][i] = static_cast<float>(.2 * std::sin(i * .01)); in[1][i] = static_cast<float>(.15 * std::cos(i * .03)); }
        auto baseline = zeros(in[0].size());
        for (unsigned block : {64u, 128u, 0u}) {
            Reverb r; prepare(r, 32000, initial, ir); auto out = zeros(in[0].size()); auto s = initial;
            constexpr std::array<std::size_t, 9> events{0, 71, 1207, 4003, 6001, 7009, 18003, 21007, 28000};
            for (unsigned j = 0; j + 1 < events.size(); ++j) {
                if (j == 1) { s.values[4] = 500; s.values[5] = .17; }
                if (j == 2) { s.values[4] = 20000; s.values[5] = .00317; s.values[1] = 1; }
                if (j == 3) { s.values[0] = .4; s.values[5] = 0; }
                if (j == 4) s.enabled = false;
                if (j == 5) s.enabled = true; // reverse a bypass ramp before sleep
                if (j == 6) s.enabled = false;
                if (j == 7) s.enabled = true;
                { Guard guard; check(r.set(s), "sample-offset live command"); }
                render(r, in, out, block, events[j], events[j + 1]);
            }
            if (block == 64) baseline = out; else equal(out, baseline);
        }
    }
    // Stereo prepare is transactional through ALL 40 candidate allocations,
    // including second-channel FFT and pre-delay storage. Start from partial
    // old pre-delay/convolution state and compare subsequent PCM bit-exactly.
    unsigned allocationFailures = 0;
    ReverbSettings s; s.enabled = true; s.values[2] = .1; s.values[5] = .011;
    const auto ir = impulse(32000, s); auto replacement = ir; replacement[1][0] = .7f;
    auto prefix = zeros(2047); prefix[0][0] = 1; prefix[1][39] = -.5f;
    auto continuation = zeros(4096); continuation[0][311] = .3f;
    for (int site = 0; site < 64; ++site) {
        Reverb a, b; prepare(a, 32000, s, ir); prepare(b, 32000, s, ir);
        auto scratch = zeros(2047); render(a, prefix, scratch, 64); render(b, prefix, scratch, 64);
        failAfter = site; const bool accepted = a.prepare(32000, s, replacement[0], replacement[1]); failAfter = -1;
        if (accepted) break;
        ++allocationFailures;
        auto x = zeros(4096), y = zeros(4096); render(a, continuation, x, 128); render(b, continuation, y, 128);
        check(x == y && a.settings() == s && a.sampleRate() == 32000, "OOM preserves full stereo old state");
    }
    check(allocationFailures == 40, "every stereo candidate allocation site protected");
    for (unsigned site = 0; site < allocationFailures; ++site) {
        Reverb a, b; prepare(a, 32000, s, ir); prepare(b, 32000, s, ir);
        auto scratch = zeros(2047); render(a, prefix, scratch, 64); render(b, prefix, scratch, 64);
        std::atomic<bool> flag{false}; cancelAfter = static_cast<int>(site); cancelOnAllocation = &flag;
        const bool accepted = a.prepare(32000, s, replacement[0], replacement[1], &flag);
        cancelOnAllocation = nullptr; cancelAfter = -1;
        check(!accepted && flag.load(), "cancel every stereo candidate allocation");
        auto x = zeros(4096), y = zeros(4096); render(a, continuation, x, 128); render(b, continuation, y, 128);
        check(x == y, "cancel preserves complete old stereo history");
    }
    // Rejected edits/calls cannot mutate controls or advance audio history.
    {
        Reverb a, b; prepare(a, 32000, s, ir); prepare(b, 32000, s, ir);
        auto old = s; old.values[2] = .2;
        { Guard guard; check(!a.set(old) && a.settings() == s, "shape edit needs worker replacement"); }
        old = s; old.values[3] = .7;
        { Guard guard; check(!a.set(old) && a.settings() == s, "decay edit needs worker replacement"); }
        for (unsigned control = 0; control < 6; ++control) {
            old = s; old.values[control] = NAN;
            { Guard guard; check(!a.set(old) && a.settings() == s, "nonfinite live control is atomic"); }
            old.values[control] = aion::reverbControls[control].maximum + 1;
            { Guard guard; check(!a.set(old) && a.settings() == s, "out-of-range live control is atomic"); }
        }
        std::atomic<bool> flag{true};
        { Guard guard; check(!a.prepare(32000, s, ir[0], ir[1], &flag), "early cancelled prepare does not allocate"); }
        check(!a.prepare(7999, s, ir[0], ir[1]) && !a.prepare(48000, s, ir[0], ir[1]), "bad rate or wrong IR length");
        auto sentinel = zeros(4097); std::fill(sentinel[0].begin(), sentinel[0].end(), 7.f);
        { Guard guard; check(!a.process(sentinel[0], sentinel[1], sentinel[0], sentinel[1]) && sentinel[0][0] == 7, "oversize rejection no output change");
          check(!a.process(prefix[0], prefix[1], continuation[0], continuation[1]), "mismatched spans rejected");
          check(a.process({}, {}, {}, {}), "empty callback accepted"); }
        auto x = zeros(4096), y = zeros(4096); render(a, continuation, x, 64); render(b, continuation, y, 64); check(x == y, "rejected calls do not advance history");
        render(a, continuation, x, 128); { Guard guard; a.reset(); }
        prepare(b, 32000, s, ir); render(a, continuation, x, 64); render(b, continuation, y, 64); check(x == y, "reset equals fresh complete stereo graph");
        // A device-rate change requires a completely prepared new processor.
        // Both FFT/predelay state and the derived Nyquist-safe damping change.
        const auto newIr = impulse(8000, s);
        prepare(a, 8000, s, newIr); prepare(b, 8000, s, newIr);
        render(a, continuation, x, 64); render(b, continuation, y, 128);
        equal(x, y); check(a.sampleRate() == 8000 && a.predelayStorageFrames() == 2002, "successful rate replacement");
        Reverb empty;
        auto sentinel2 = zeros(64); sentinel2[0][0] = 7;
        { Guard guard; empty.reset(); check(!empty.set(s), "unprepared set rejects");
          check(!empty.process(sentinel2[0], sentinel2[1], sentinel2[0], sentinel2[1]) && sentinel2[0][0] == 7, "unprepared process preserves output"); }
    }
    // Quiet/reset and numerical protection in both active and bypass paths.
    for (bool enabled : {false, true}) {
        auto active = s; active.enabled = enabled; Reverb r; prepare(r, 32000, active, ir);
        auto in = zeros(4096), out = zeros(4096);
        in[0][0] = NAN; in[1][1] = INFINITY; in[0][2] = -INFINITY; in[1][3] = std::numeric_limits<float>::max();
        render(r, in, out, 64);
        for (const auto& c : out) for (auto v : c) check(std::isfinite(v) && std::abs(v) <= 1.e6f, "finite bounded extreme input/output");
        { Guard guard; r.reset(); }
        in = zeros(4096); render(r, in, out, 128);
        for (const auto& c : out) for (auto v : c) check(v == 0, "reset cannot resurrect pre-delay/convolution/damping tail");
    }
    std::cout << "Reverb stereo processor passed: " << cases << " rate/delay/block cases, 17 presets, live ramps, reset/stereo/alias, "
              << allocationFailures << " OOM/cancellation sites; peak PCM error=" << peakError << '\n';
}
