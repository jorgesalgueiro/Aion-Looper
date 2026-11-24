// SPDX-License-Identifier: AGPL-3.0-only
#include "core/PartitionedConvolver.h"
#include "core/ReverbImpulse.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

namespace {
bool watching = false;
unsigned heap = 0;
int failAfter = -1;
std::atomic<bool>* cancelOnAllocation = nullptr;
double peakError = 0;
void check(bool ok, const char* why) {
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
struct Guard {
    Guard() { heap = 0; watching = true; }
    ~Guard() { watching = false; check(heap == 0, "process/reset allocated or freed"); }
};
}
void* operator new(std::size_t n) {
    if (watching) ++heap;
    if (failAfter == 0) throw std::bad_alloc();
    if (failAfter > 0) --failAfter;
    if (auto* p = std::malloc(n ? n : 1)) {
        if (cancelOnAllocation) cancelOnAllocation->store(true);
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if (watching && p) ++heap; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
using aion::PartitionedConvolver;

static std::vector<double> direct(const std::vector<float>& in, const std::vector<float>& ir) {
    std::vector<double> out(in.size());
    for (std::size_t n = 0; n < in.size(); ++n)
        for (std::size_t j = 0; j < ir.size() && j <= n; ++j)
            out[n] += static_cast<double>(in[n - j]) * ir[j];
    return out;
}
static void compare(std::span<const float> actual, std::span<const double> expected) {
    check(actual.size() == expected.size(), "comparison size");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const auto error = std::abs(actual[i] - expected[i]);
        peakError = std::max(peakError, error);
        if (!std::isfinite(actual[i]) || error > 3.e-6) {
            std::cerr << "sample=" << i << " actual=" << actual[i] << " expected=" << expected[i] << " error=" << error << '\n';
            check(false, "independent convolution PCM tolerance 3e-6");
        }
    }
}
static void render(PartitionedConvolver& conv, std::span<const float> in,
                   std::span<float> out, unsigned block) {
    std::size_t offset = 0, call = 0;
    constexpr std::array<unsigned, 8> sizes{1, 63, 128, 3, 257, 64, 0, 511};
    while (offset < in.size()) {
        const auto n = std::min<std::size_t>(block ? block : sizes[call++ % sizes.size()], in.size() - offset);
        Guard guard;
        check(conv.process(in.subspan(offset, n), out.subspan(offset, n)), "valid render accepted");
        offset += n;
    }
}
int main() {
    check(PartitionedConvolver::latencyFrames() == 0, "zero declared algorithmic latency");
    // Independent time-domain oracle, partition edges, aliasing, empty calls,
    // irregular host boundaries and ring wraps, with no hidden gain/trim.
    for (unsigned length : {1u, 2u, 127u, 128u, 129u, 255u, 256u, 257u, 1025u}) {
        std::vector<float> ir(length), in(4099), out(in.size());
        for (unsigned i = 0; i < length; ++i) ir[i] = static_cast<float>(.03 * std::cos(i * .71));
        ir.back() = 1.e-8f; // retain quiet late coefficients
        for (unsigned i = 0; i < 1703; ++i) in[i] = static_cast<float>(.6 * std::sin(i * .17));
        const auto expected = direct(in, ir);
        for (unsigned block : {1u, 64u, 128u, 257u, 4096u, 0u}) {
            PartitionedConvolver conv;
            check(conv.prepare(ir), "prepare direct oracle");
            render(conv, in, out, block); compare(out, expected);
            { Guard guard; conv.reset(); conv.reset(); }
            out = in;
            render(conv, out, out, block); compare(out, expected);
        }
    }
    // Two independent channels with unequal IR/input; swapping channels must
    // swap results, and right silence must never leak the left channel.
    for (unsigned block : {64u, 128u}) {
        const std::vector<float> leftIr{.2f, -.1f, .3f}, rightIr{-.4f, .2f};
        std::vector<float> left(2048), right(2048), l(2048), r(2048);
        left[0] = 1; right[7] = -.5f;
        PartitionedConvolver a, b;
        check(a.prepare(leftIr) && b.prepare(rightIr), "stereo prepare");
        render(a, left, l, block); render(b, right, r, block);
        compare(l, direct(left, leftIr)); compare(r, direct(right, rightIr));
        { Guard guard; a.reset(); b.reset(); }
        std::fill(right.begin(), right.end(), 0.f);
        render(a, right, r, block); render(b, left, l, block);
        compare(r, direct(right, leftIr)); compare(l, direct(left, rightIr));
    }
    // Real generated stereo IRs at eight supported rates, both audio block
    // sizes. Unit input must emit the complete IR at its exact sample positions.
    for (double rate : {8000., 22050., 32000., 44100., 48000., 96000., 192000., 384000.}) {
        aion::ReverbSettings settings; settings.values[2] = .1;
        aion::ReverbImpulse ir;
        check(ir.prepare(rate, settings, 42), "generated IR");
        for (unsigned c = 0; c < 2; ++c) for (unsigned block : {64u, 128u}) {
            PartitionedConvolver conv;
            const auto coefficients = ir.channel(c);
            check(conv.prepare(coefficients), "generated IR kernel");
            std::vector<float> in(coefficients.size() + 512), out(in.size());
            std::vector<double> expected(in.size());
            in[0] = 1; std::copy(coefficients.begin(), coefficients.end(), expected.begin());
            render(conv, in, out, block); compare(out, expected);
            // Reset mid-partition must discard every old late contribution.
            std::array<float, 17> partial{}; partial[0] = 1;
            { Guard guard; check(conv.process(partial, partial), "partial history"); conv.reset(); }
            std::fill(in.begin(), in.end(), 0.f);
            std::fill(expected.begin(), expected.end(), 0.);
            render(conv, in, out, block); compare(out, expected);
        }
    }
    // Long dense tail and full history-ring wrap: impulse then isolated pulse
    // after tail expiry. Analytic expected PCM, no FFT-derived oracle.
    {
        std::vector<float> ir(65537), in(ir.size() * 2 + 512), out(in.size());
        std::vector<double> expected(in.size());
        for (std::size_t i = 0; i < ir.size(); ++i) ir[i] = static_cast<float>(.002 * std::cos(.13 * i));
        const auto second = ir.size() + 193;
        in[0] = 1; in[second] = -.7f;
        for (std::size_t i = 0; i < ir.size(); ++i) { expected[i] += ir[i]; expected[second + i] -= static_cast<double>(.7f) * ir[i]; }
        PartitionedConvolver conv; check(conv.prepare(ir), "long kernel");
        render(conv, in, out, 64); compare(out, expected);
    }
    // Every allocation failure must retain exact running history, including
    // partial input. Candidate cleanup stays off audio and must survive ASan.
    unsigned allocationFailures = 0;
    const std::vector<float> ir(513, .001f), replacement(1025, .002f);
    for (int failure = 0; failure < 32; ++failure) {
        PartitionedConvolver a, b;
        check(a.prepare(ir) && b.prepare(ir), "OOM baseline");
        std::array<float, 63> prefix{}; prefix[0] = 1;
        auto other = prefix;
        { Guard guard; check(a.process(prefix, prefix) && b.process(other, other), "OOM history prefix"); }
        failAfter = failure; const bool accepted = a.prepare(replacement); failAfter = -1;
        if (accepted) { check(a.impulseFrames() == replacement.size(), "replacement publication"); break; }
        ++allocationFailures; check(a.impulseFrames() == ir.size(), "OOM keeps old IR metadata");
        std::array<float, 2048> x{}, y{}; x[17] = y[17] = .4f;
        { Guard guard; check(a.process(x, x) && b.process(y, y), "OOM continue history"); }
        check(x == y, "OOM preserves complete running convolution state");
    }
    check(allocationFailures >= 8 && allocationFailures < 32, "all candidate allocation sites exercised");
    {
        PartitionedConvolver a, b;
        check(a.prepare(ir) && b.prepare(ir), "cancel baseline");
        std::array<float, 11> prefix{}; prefix[0] = 1;
        auto other = prefix;
        { Guard guard; check(a.process(prefix, prefix) && b.process(other, other), "cancel partial history"); }
        std::atomic<bool> flag{true};
        { Guard guard; check(!a.prepare(replacement, &flag), "pre-cancel without allocations"); }
        flag.store(false); cancelOnAllocation = &flag;
        const auto accepted = a.prepare(replacement, &flag); cancelOnAllocation = nullptr;
        check(!accepted, "allocation-time cancellation rejects candidate");
        std::vector<float> invalid{NAN}, huge(PartitionedConvolver::maximumImpulseFrames + 1);
        check(!a.prepare(invalid) && !a.prepare(huge) && !a.prepare({}), "invalid IRs rejected");
        std::array<float, 2048> x{}, y{};
        { Guard guard; check(a.process(x, x) && b.process(y, y), "cancel continue history"); }
        check(x == y && a.impulseFrames() == ir.size(), "cancellation/validation preserve running state");
    }
    {
        PartitionedConvolver conv;
        std::array<float, 2> in{.1f, .2f}, out{4, 5};
        { Guard guard; conv.reset(); check(!conv.process(in, out) && out[0] == 4, "unprepared reject"); }
        check(conv.prepare(std::array<float, 1>{1}), "identity IR");
        std::vector<float> tooLarge(4097), sentinel(4097, 7);
        { Guard guard;
          check(!conv.process(tooLarge, sentinel) && sentinel.back() == 7, "oversized callback rejects atomically");
          check(!conv.process(in, std::span<float>(out).first(1)) && out[0] == 4, "mismatched size rejects atomically");
          check(conv.process({}, {}), "empty accepted");
          check(conv.process(in, out) && std::abs(out[0] - .1f) < 1.e-7, "rejected callbacks retain initial history"); }
        conv.reset();
        std::array<float, 128> extreme{}, manuallySanitised{};
        extreme[0] = NAN; extreme[1] = INFINITY; extreme[2] = -INFINITY; extreme[3] = std::numeric_limits<float>::max();
        manuallySanitised[3] = 1.e6f;
        { Guard guard; check(conv.process(extreme, extreme), "nonfinite input handled"); }
        { Guard guard; conv.reset(); check(conv.process(manuallySanitised, manuallySanitised), "explicit sanitised input"); }
        // Test the sanitisation policy independently of float FFT roundoff at
        // 1e6 scale. Ordinary audio retains the independent 3e-6 FIR oracle.
        check(extreme == manuallySanitised, "nonfinite/oversized inputs exactly equal explicitly sanitised PCM");
        for (auto v : extreme) check(std::isfinite(v), "bounded input cannot poison FFT history");
        { Guard guard; conv.reset(); }
        extreme.fill(NAN);
        { Guard guard; check(conv.process(extreme, extreme), "all nonfinite block"); }
        for (auto v : extreme) check(v == 0, "all nonfinite input emits exact silence");
    }
    // Maximum permitted preparation/reset without scanning two million history
    // samples on reset. This is a bound test, not a long-tail CPU benchmark.
    {
        std::vector<float> maxIr(PartitionedConvolver::maximumImpulseFrames); maxIr.back() = .25f;
        PartitionedConvolver conv; check(conv.prepare(maxIr), "maximum IR preparation");
        for (unsigned i = 0; i < 1000; ++i) { Guard guard; conv.reset(); }
        std::array<float, 64> in{}, out{}; in[0] = 1;
        { Guard guard; check(conv.process(in, out), "maximum IR first callback"); }
        for (auto v : out) check(v == 0, "maximum delayed IR has no early output");
    }
    std::cout << "Convolution kernel passed: direct FIR, stereo, 8 rates, 64/128/variable/in-place, reset, cancellation, "
              << allocationFailures << " OOM sites; peak PCM error=" << peakError << '\n';
}
