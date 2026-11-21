# Planned callback attribution: 21 November 2026

This completes the handoff's **profile the remaining full callback costs** step.
No product DSP, UI, parameters, state, routing, formats or dependencies changed.
Do not infer a new CPU improvement or deadline pass from this diagnostic work.

## Method and isolation

Installed tools lacked perf/Valgrind sampling support; GNU gprof was available.
Opt-in `AION_BUILD_CALLBACK_PROFILE=ON` adds an EXCLUDE_FROM_ALL Linux/GNU/Release
target, `aion_processor_effects_profile`. A separate object library compiles the
existing core sources with `-pg -fno-omit-frame-pointer -g`. Explicit objects
resolve core symbols before the ordinary static archive. The production
processor/JUCE remain their ordinary build; normal `aion_core` is never given
profiling flags. The tool is not installed and default builds leave it off.

`moncontrol` excludes capture/setup/finalization and teardown from sampling.
Each continuous profiling window includes the fixture's playback callbacks,
input/MIDI/checksum work and periodic off-audio snapshot service. Keeping the
window continuous avoids resetting the sampling interval for each short callback.
Three profiles aggregate per run: bypass, QCTFO and QCTFO with 32 unassigned CCs.
Warm 256 / measure 1024 callbacks per profile, synthetic captured stereo PCM.

The profiler adds overhead and changes generated code/layout. Its displayed
callback timings/overruns are explicitly tagged **invalid for capacity**. Counts
are approximate, including concurrent worker activity. gprof's percentages are
sampled executable self time, not precise normal callback wall-time percentages;
shared-library time and inlined attribution limit interpretation. The sampling
granularity is 0.01s. Do not compare its times to ordinary Release timings.
See the [GNU compilation guidance](https://sourceware.org/binutils/docs/gprof/Compiling.html)
and [profiling implementation](https://sourceware.org/binutils/docs/gprof/Implementation.html).
No third-party implementation was copied or new runtime dependency added.

## Results

| Sampled self-time hotspot | 10 loops, 96k/64 | 20 loops, 96k/128 |
| --- | --- | --- |
| Drive::tick | 56.10% / 0.69s | 60.04% / 2.81s |
| LoopBank::tick (own work, excluding callees) | 9.76% / 0.12s | 11.75% / 0.55s |
| Compressor::process | 9.76% / 0.12s | 9.19% / 0.43s |
| Equalizer::process | 1.63% / 0.02s | 3.42% / 0.16s |

Complete flat tables, including low/zero-sample entries:
`callback-profile-2026-11-21-linux-{10-96-64,20-96-128}.txt`.
The existing normal Release capacity matrices remain the timing evidence; none
were rerun merely to obtain a more favorable result. No new capacity pass.

The ordinary Linux Drive::tick disassembly already uses packed-double SIMD for
control smoothing (`subpd/mulpd/addpd` and mask selects). Do not start another
manual control-vectorization or settled-cache rewrite on the assumption that
this is a scalar loop. Raw inspection: `build/callback-profile-drive-assembly.txt`.
Drive remains the first attribution target; the current data do NOT distinguish
its inlined oversampling, nonlinear and tone-filter costs. Optional `gprof -l`
failed with `somebody miscounted: ltab.len=30529 instead of 30335`. This is a
profiler-analysis limitation, not a reproduced product defect. No DSP or tests
were changed to work around it, and no line-level conclusion is claimed.

## Executed verification and provenance

- Normal Windows benchmark build and existing 64/128 fixtures: 2/2, 0.97s.
- Normal Linux benchmark build and existing 64/128 fixtures: 2/2, 0.55s.
- Both instrumented workloads: six fixture profiles pass finite/nonzero PCM,
  owners/state/settings/MIDI count checks; all checksums match the accepted
  `drive-midpoint-processor-linux.jsonl` normal matrix. Watched C++ heap counters
  are zero, but that does **not** certify profiler runtime or its signals RT safe.
- Linux `nm -u`: no mcount/moncontrol/monstartup references in normal core or
  normal benchmark; profile Drive object references mcount. Link map resolves
  Drive::tick from `aion_profile_core.dir/source/core/Drive.cpp.o`.
- First opt-in build failed because installed glibc sys/gmon.h omits moncontrol's
  declaration. The exported ABI was checked against the system symbol and
  [glibc source](https://raw.githubusercontent.com/bminor/glibc/master/gmon/gmon.c);
  diagnostic-only declaration fixed it. Successful profile snapshot: `a512d8a`.

Unique raw bundles in the independent WSL checkout:
`build/callback-profile-yIhOXGVe` (10/96/64) and
`build/callback-profile-OvMWs5Ku` (20/96/128). Each retains HEAD, executable SHA256,
fixtures JSONL, empty stderr, PID-specific gmon binary and full function report.
Link map: `build/linux/callback-profile-link.map`. Initial failure and successful
rebuild logs: `build/callback-profile-{build,rebuild}.log`. No packages or pushes.
The committed Release-only guard configured successfully and the optional target
was restored OFF afterward; logs are `build/callback-profile-{release-guard,restore-default}.log`.

## Reproduce

Run in `/home/jorge/projects/Aion-Looper`, after the documented clean committed
snapshot sync. Reuse the current Linux dependency/cache tree; no new dependency
setup is required. The two workloads run serially after other builds/tests finish.

```sh
cmake -S . -B build/linux -DAION_BUILD_CALLBACK_PROFILE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --target aion_processor_effects_profile aion_processor_effects_benchmark --parallel 2
ctest --test-dir build/linux -R '^native_effects_benchmark_fixture_(64|128)$' --output-on-failure --no-tests=error
for spec in "96000 64 10" "96000 128 20"; do
    set -- $spec
    run=$(mktemp -d build/callback-profile-XXXXXXXX)
    printf '%s\n' "$run"
    git rev-parse HEAD > "$run/metadata.txt"
    sha256sum build/linux/aion_processor_effects_profile >> "$run/metadata.txt"
    GMON_OUT_PREFIX="$run/gmon" ./build/linux/aion_processor_effects_profile --case "$@" > "$run/fixtures.jsonl" 2> "$run/stderr.log" &
    child=$!
    wait "$child" || exit 1
    gprof -b ./build/linux/aion_processor_effects_profile "$run/gmon.$child" > "$run/report.txt"
    gprof -b -p ./build/linux/aion_processor_effects_profile "$run/gmon.$child" > "$run/flat.txt"
done
cmake -S . -B build/linux -DAION_BUILD_CALLBACK_PROFILE=OFF
```

Normal Windows verification uses the existing pinned CMake/CTest tools:

```powershell
cmake --build build/windows --config Release --target aion_processor_effects_benchmark --parallel 2
ctest --test-dir build/windows -C Release -R '^native_effects_benchmark_fixture_(64|128)$' --output-on-failure --no-tests=error
```

Next bounded investigation: distinguish the drive's wet/dry resampling and
nonlinear/tone costs using attribution that handles inlining, before selecting a
production change. Retain exact PCM and ownership tests; do not change quality,
smoothness, track capacity, chain order or controls to meet a benchmark. Do not
turn the optional line-tool failure into a new product gate or divert to another
profiler/backend rewrite. Original loop functionality remains the migration scope;
new instruments and release/hardware acceptance retain their roadmap order.
