# Loaded loop-engine performance

17 November 2026 development observations. This follow-up reduces T/O/F CPU
cost while preserving the accepted native sound. It does **not** certify a
release, hardware latency or every 64/128-sample workload.

## Scope and method

The existing `aion_effects_engine_benchmark --drives` renders 40 configurations:
10/20 playing loops, 48/96 kHz, 64/128 frames, bypass, T/F/O, and Q/C/T/F/O;
enabled chains include steady controls and live edits every sixteen callbacks.
Q uses original preset 8, C preset 3, and T/F/O use untouched native defaults.
The explicit benchmark order is a fixture; product chain order remains editable.

Each configuration warms 256 callbacks, then measures 1,024. Setup, recording,
input generation, command production, output checks and telemetry consumption
are outside the timed interval. Processing includes the actual Engine, mixer,
sample scheduling, accepted edits and bounded telemetry production. Every loop
must retain its clip identity/length, playing state and reported effect settings;
PCM must be finite and nonzero. Watched ordinary/aligned C++ new/delete calls
must remain zero. This counter does not instrument arbitrary C malloc calls.

This is a developer diagnostic, not another product audio engine. It excludes
Arp, drone/sampler engines, the JUCE processor wrapper, physical devices, actual
WebView traffic and round-trip latency. Exit zero establishes fixture integrity,
not deadline compliance. Original no-argument Q/C benchmark coverage is retained.

Host: Intel Xeon E5-2680 v4, 14 cores / 28 logical processors. Windows x64
10.0.19045 used MSVC 19.44.35229, Release `/O2 /Ob2 /DNDEBUG`; Linux/WSL used
GCC 13.3, Release `-O3 -DNDEBUG`. Ratios are within each platform. No CPU affinity
or realtime priority was imposed. Baseline and final timing runs were serialized
without competing test builds; first-time configure/build work is excluded.
These are single warmed observations per configuration, without a cross-run
confidence interval. Maxima include scheduling effects; no physical underrun
measurement or cause analysis of every timing outlier was performed.

## Results

The budget below is **666.67 us** (64 / 96,000 seconds). Times are microseconds;
overruns count measured callbacks exceeding that budget, out of 1,024.

| Platform / ten-loop chain | Mean before → after | p99 before → after | Overruns before → after |
| --- | --- | --- | --- |
| Windows, T/F/O | 727.12 → 474.93 | 894.50 → 577.40 | 1024 → 0 |
| Linux, T/F/O | 722.06 → 394.51 | 901.95 → 532.85 | 837 → 0 |
| Windows, Q/C/T/F/O | 905.39 → 668.37 | 1144.10 → 797.90 | 1024 → 222 |
| Linux, Q/C/T/F/O | 862.52 → 552.17 | 1033.58 → 702.33 | 1024 → 36 |

Ten-loop T/F/O mean time falls about **35% on Windows and 45% on Linux** in this
specific 96 kHz/64 case. Full Q/C/T/F/O at 48 kHz/64 has more headroom: final
Windows/Linux means 620.52/547.75 us, p99 777.20/712.13 us, against 1333.33 us,
with zero observed overruns in those two configurations.

**Capacity remains limited.** Stacking Q/C/T/F/O across ten loops at 96 kHz/64
still overruns; twenty loops with all three drives at 96 kHz also exceed the
budget. Other rows include isolated 48 kHz overruns. Zero observations in a short
test are not a hard realtime guarantee. Do not silently reduce oversampling,
force a sample rate, limit existing track capacity or claim full-workspace
acceptance to conceal these limits. More CPU profiling and actual Standalone
device/MIDI/UI load checks are needed before release.

All 40 before/after cases, including 128 frames, automation, maxima, bypass and
expanded capacity, are committed as [Windows CSV](benchmarks/drive-2026-11-17-windows.csv)
and [Linux CSV](benchmarks/drive-2026-11-17-linux.csv). Within each platform every
reported PCM checksum matches and every watched callback C++ heap count is zero.
Rounded aggregate checksums support fixture integrity; they are not the sound
equivalence oracle.

## Implementation and verification

Independent stereo lanes share SSE2 arithmetic on x64, with a portable C++ path
elsewhere. Previous antiderivatives and shared bias terms are reused exactly.
Filter storage contains only the live five/eight state entries and required
downsample delays. Prepared, fully bypassed processors skip stateless dispatch;
first preparation and pending fades remain intact. No new dependency, reduced
4x oversampling, changed curve/tone/smoothing/dry alignment, parameter ID or
state-format change is involved.

`DriveScalarReference.*` freezes accepted equations from `4e2a9d4`. Both optimized
and forced-portable paths compare **1,634,448 samples**, maximum finite-sample
error **zero**, across eight rates, unequal channel histories, variable blocks,
rapid controls, resets, sleeping edits/wake-up and reset during a bypass fade.
Existing integration/spectral limits remain unchanged. Connected tests retain
all six T/F/O orders, subsets/reinsertion, post-clear preparation, live OD/SUB/
MULT/scheduled STOP, history/RTRG, panic, clip ownership and callback allocation
checks. Preserved UI/reference assets remain unchanged.

Executed on final product code `d90ec37`:

- Windows core/reference/Arp/chain: 7/7, 11.84s; unchanged-JS state/DSP/sync/
  Multiply/history/reverse/normalization: 7/7, 132.22s.
- Windows linked processor/CLI/MIDI/state/chain/restore: 6/6, 43.11s; actual
  WebView2 pointer/key/learn/reload/bypass/DEL and nonzero PCM at 64/128 pass.
- Linux corresponding core/reference/processor checks: 20/20, 253.76s.
- Both platforms' unchanged-JS capture/OD/SUB audit: 126 mono/stereo cases,
  965,826 compared frames, peak 7.394757495e-8 against the retained 1e-6 limit.
- Linux ASan/UBSan/leak detection: 5/5, 149.45s; optimized/portable DSP, connected
  ownership, Arp and chain, no suppressions, leak detection and halt-on-UB on.

Logs use `build/drive-dispatch-*`, `drive-stereo-sanitized-*` and the raw timing
files below. An initial sanitizer build could not resolve the newly introduced
portable target in its stale Makefile. Explicit reconfiguration repaired that
build prerequisite; no sanitizer assertion or tolerance was changed.

## Reproduce

Use existing Release caches. Configure again when adding new CMake targets.

```powershell
cmake --build build/windows-core --config Release --target aion_effects_engine_benchmark --parallel 2
./build/windows-core/Release/aion_effects_engine_benchmark.exe --drives > build/drive-workspace-dispatch.jsonl
```

```sh
cmake --build build/linux --target aion_effects_engine_benchmark --parallel 2
./build/linux/aion_effects_engine_benchmark --drives > build/drive-workspace-optimized-serial.jsonl
```

The Linux scalar baseline uses a clean, detached local snapshot. It does not
reset the working mirror, fetch dependencies or change publishing history:

```sh
git worktree add --detach build/drive-baseline-3572c72 3572c72
cmake -G Ninja -S build/drive-baseline-3572c72 -B build/drive-baseline-build -DAION_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/drive-baseline-build --target aion_effects_engine_benchmark --parallel 2
./build/drive-baseline-build/aion_effects_engine_benchmark --drives > build/drive-workspace-baseline-serial.jsonl
```

If that worktree already exists, verify its commit and clean state rather than
overwriting it. Match compiler/optimization flags and run baseline/final programs
sequentially after other builds/tests finish. Windows's original scalar data is
`build/drive-workspace-baseline.jsonl`; Linux's earlier concurrent-run data is
retained separately and excluded from the committed comparison. Listening,
physical devices, Linux WebKit and full instrument/release validation remain open.
