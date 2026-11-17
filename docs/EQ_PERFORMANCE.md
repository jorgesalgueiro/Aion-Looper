# Stereo EQ optimization — 17 November 2026

The accepted EQ now evaluates independent left/right SVF recurrences together.
It shares the existing two-double SSE2/portable arithmetic helper with the drives;
`DriveFrame.h` remains a compatibility alias. No dependencies, presets, IDs,
control clocks, smoothing, frequency limits, response curve or routing changed.
No allocation or synchronization was added to processing. Invalid output clears
only the affected channel, preserving the other channel's filter history.

`tests/EqScalarReference.*` freezes the complete accepted EQ and automation from
`80433e4`, with only its include/namespace changed. Both optimized and explicitly
portable tests compare **1,271,264 channel samples exactly**, across eight sample
rates, variable blocks, rapid extreme-value edits, resets, missing inputs and
non-finite input recovery. Existing in-place, overflow, magnitude, bandwidth,
atomic-rejection, silence/tail and allocation checks remain unchanged.

## Executed acceptance

| Gate | Result |
| --- | --- |
| Windows EQ/portable/scalar/connected, drive kernels, unchanged-JS EQ | 11/11, 52.97s |
| Windows drive/chain connected ownership | 2/2, 5.64s |
| Windows production EQ/drive GUI/CLI/MIDI/state, 64/128 | 4/4, 51.21s |
| Linux corresponding core/reference/processor checks | 17/17, 66.91s |
| Linux ASan/UBSan/leaks, EQ and both drive arithmetic paths | 6/6, 146.38s, no suppressions |
| Actual WebView2 EQ pointer/key/MIDI/reload/DEL/nonzero PCM | 64 and 128 pass; caveat below |
| Actual WebView2 unchanged EQ worklet and smoothSetParam | 48 cases, 270,336 frames, peak error 8.570541420310773e-8, unchanged 1e-6 limit |
| Windows/Linux connected capture/OD/SUB audio audit | Each 126 cases, 965,826 frames, peak error 7.394757495227111e-8, unchanged 1e-6 limit |

The first UI run passed 64, then failed the 128-sample assertion that an old
injected send-error message had disappeared 100ms after successful native state
was observed. It ran during concurrent sanitizer/audit work. The exact unchanged
128 test passed in isolation. No product/test fix or tolerance change was made;
this remains a timing-sensitive UI observation, not proof of its cause or a
universally reliable 100ms status-update bound. Original failed log is retained.
Linux WebKit, listening, physical drivers and full application device load were
not tested. Standalone builds succeeded on both platforms. No wrappers/packages.

## Measurements and reproduction

Use Release and the same environment/limitations documented in
[DRIVE_PERFORMANCE.md](DRIVE_PERFORMANCE.md). No builds or tests run concurrently
with timing. Measurements are processing costs, not device/round-trip latency.
The EQ-only benchmark has all ten bands active on 20 stereo kernels. Windows
48k/64 steady mean fell **196.906 → 90.8623us**; automated **190.495 → 98.8496us**.
At 128, steady **360.9 → 183.354us**, automated **382.514 → 197.578us**. The before
profile was taken before edits this session, using the existing unchanged target.
Checksums match; the exact sample regression, rather than rounded benchmark
checksums, establishes sound equivalence.

Initial full Engine comparisons against the previous session are retained in
`benchmarks/eq-stereo-2026-11-17-{windows,linux}.csv`: all 40 cases per platform,
including slower cases/outliers, unchanged reported PCM checksums and zero watched
callback C++ new/delete (aligned included). They do **not** establish a universal
full-chain gain. Windows ten QCTFO loops, 96k/64, steady mean 668.37 → 637.579us,
overruns 222 → 178; automated mean 661.675 → 647.053us but overruns **214 → 329**.
Linux's initial comparison had higher full-chain times than its prior-session
run, prompting a fresh sequential comparison. A clean detached `80433e4`
worktree at `build/eq-baseline-80433e4`, built Release/plugin-OFF in its own
`build/eq-baseline-build`, was timed immediately before the optimized binary.
Both use GCC 13.3 and `-O3 -DNDEBUG`. All 40 PCM checksums match and both report
zero watched callback C++ allocations/deallocations. Complete results, including
outliers and slower cases, are in
`benchmarks/eq-stereo-2026-11-17-linux-fresh.csv`.

| Linux fresh sequential comparison | Mean us, before → after | p99 us, before → after | Overruns /1024, before → after |
| --- | --- | --- | --- |
| Ten QCTFO loops, 96k/64, steady | 609.218 → 578.184 | 737.502 → 769.863 | 136 → 85 |
| Same, automated | 606.084 → 575.419 | 725.973 → 778.674 | 133 → 86 |

The freshly paired Linux EQ-only 48k/64 steady mean is **207.624 → 93.1926us**;
automated **207.62 → 99.5287us**. At 128 it is **399.752 → 181.096us** steady,
**412.2 → 194.519us** automated. Full-chain mean savings are much smaller than
all-band kernel savings; p99 is worse in the shown full-chain samples. These
single sequential runs do not prove a universal deadline or tail-latency gain.
The high-rate capacity issue remains open. Twenty-track overload is not solved.

```powershell
cmake --build build/windows-core --config Release --target aion_eq_tests aion_eq_stereo_tests aion_eq_portable_tests aion_eq_connected_tests aion_eq_probe aion_eq_benchmark aion_effects_engine_benchmark aion_drive_tests aion_drive_portable_tests --parallel 2
ctest --test-dir build/windows-core -C Release -R '^(eq_.*contract|js_native_eq_.*|drive_(dsp|portable_dsp)_contract)$' --output-on-failure --no-tests=error
cmake --build build/windows --config Release --target AionLooper_Standalone aion_eq_processor_tests aion_drive_processor_tests aion_mouse_host --parallel 2
ctest --test-dir build/windows -C Release -R '^native_(eq|drive)_processor_(64|128)$' --output-on-failure --no-tests=error
./tools/TestEqUi.ps1
./tools/TestEqWebAudio.ps1
./build/windows-core/Release/aion_eq_benchmark.exe
./build/windows-core/Release/aion_effects_engine_benchmark.exe --drives
```

Linux uses committed snapshots via `tools/SetupWslWorkspace.sh`, its independent
`build/linux` cache and the corresponding targets/CTest expressions without
`-C Release`. For sanitizers explicitly reconfigure the existing
`build/drive-sanitized` cache after adding targets, build the EQ and drive gates,
then run with `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1`.

To reproduce the fresh Linux baseline, verify the existing detached worktree's
commit and clean status; create it only if absent, never reset/overwrite it:

```sh
git worktree add --detach build/eq-baseline-80433e4 80433e4
cmake -G Ninja -S build/eq-baseline-80433e4 -B build/eq-baseline-build -DAION_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/eq-baseline-build --target aion_effects_engine_benchmark aion_eq_benchmark --parallel 2
./build/eq-baseline-build/aion_effects_engine_benchmark --drives > build/eq-stereo-baseline-fresh-linux.jsonl
./build/linux/aion_effects_engine_benchmark --drives > build/eq-stereo-after-fresh-linux.jsonl
./build/eq-baseline-build/aion_eq_benchmark > build/eq-stereo-kernel-before-fresh-linux.jsonl
./build/linux/aion_eq_benchmark > build/eq-stereo-kernel-after-fresh-linux.jsonl
```

Logs in each checkout use `build/eq-stereo-*`. Timing JSONL, initial UI failure,
isolated retry and browser output are retained separately. No reference or
preserved presentation assets changed.
