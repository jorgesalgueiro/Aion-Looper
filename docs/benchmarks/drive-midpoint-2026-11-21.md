# Unused drive midpoint elimination

Production snapshot: `49c1001`. Accepted comparator:
`9b874ef657b71ffd91f7389da69607aebffe2e4a` (already includes bias reuse).

Overdrive/fuzz use a midpoint transfer when consecutive oversamples are very
close, otherwise the divided difference of the antiderivative. Previously the
midpoint transfer was also calculated when neither stereo lane selected it.
The candidate skips only that unused calculation and denominator masking. Both
close and mixed-lane cases retain the safe fallback. Primitive history still
updates every oversample. No approximations, quality reductions, storage/layout,
controls, IDs, state, UI, routing or dependency changes. Distortion is unchanged.

The only shared-frame change adds a pure `none(mask)` operation for SSE2 and
portable paths; every existing frame operation is unchanged. All four mask
combinations and independent silent/changing channel histories now have explicit
regressions. Existing cases and tolerances remain; new lane-history PCM requires
exact Float32 equality. No failing case was removed or weakened.

## Verification

| Gate | Windows | Linux / native WSL storage |
| --- | --- | --- |
| Core/reference/connected chain | 5/5, 10.42s | Included in 9/9, 14.08s |
| Production processor/CLI/MIDI and callback fixtures, 64/128 | 4/4, 33.25s | Included in 9/9 above |
| Frozen accepted scalar DSP, native and forced portable | Each: 4,818,576 samples, maximum error 0 | Same |
| Capture/OD/SUB audio audit | 126 cases / 965,826 samples, maximum error 7.394757495227111e-8 | Same |
| ASan + UBSan / leak detection, halt on UB | Not executed | 4/4, 259.05s; no suppressions |
| Standalone build | Pass | Pass |

This is DSP-only work. GUI/bridge/persistence commands are unchanged; reuse the
previous 40 actual document-reload/control checks and current production
processor/CLI/MIDI regressions rather than rerun unaffected UI suites. No new
physical-device, Linux WebKit, listening, DAW, Apple or long-session claim.

## Controlled observation

All results are warmed Release observations, never hard-real-time guarantees or
round-trip latency measurements. Run timing only after all builds/tests/owned UI
hosts finish, serially across Windows/Linux; ordinary OS services are not isolated.

The existing paired helper compiles accepted and current DSP in the same process,
alternates invocation order per callback, checks exact PCM and equal class size.
All nine cases are retained: T/O/F, steady, tone/mix edits, and bias (T/F) or drive
amount (O) edits. Twenty processors, 96k/64, warm 256 / measure 2048 callbacks.
O/F means improve in all six cases on both OSes: approximately 1.7–5.2% in this
component fixture. Distortion arithmetic is unchanged, but its observed Linux
mean increases approximately 0.8–1.9%; Windows variation is within 0.3%. Retain
these slower cases; this is not a universal speedup claim.

Complete durable data alongside this note:

- `drive-midpoint-paired-2026-11-21-{windows,linux}.csv`: all nine component rows.
- `drive-midpoint-processor-2026-11-21-{windows,linux}.csv`: all 24 production
  callback fixtures per OS; 10/20 loops, 48/96k, 64/128, bypass/QCTFO/QCTFO+32 CCs.
- `drive-midpoint-abba-2026-11-21-{windows,linux}.csv`: all twelve rows per OS,
  fixed baseline/current/current/baseline schedule, ten loops at 96k/64. Each run
  includes bypass and both effects profiles. Identity is the accepted hash plus
  `/none`, meaning no private-signature adapter was needed.

All 48 full-matrix fixtures pass finite/nonzero PCM, stable state/owners/settings,
actual MIDI counts and zero watched callback C++ new/delete. Every fixture's
rounded PCM checksum matches the prior accepted `drive-bias-processor-*` matrix;
stronger sample equality comes from the frozen scalar tests, not those checksums.
No additional 40-case Engine timing run was made: the production callback matrix
includes Engine plus APVTS/JUCE MIDI and is the relevant application measurement.

Same-framework ABBA observations, ten loops/96k/64, nominal budget 666.667us:

| Platform / CCs per callback | Baseline mean us | Current mean us | Baseline overruns /2048 | Current overruns /2048 |
| --- | --- | --- | --- | --- |
| Windows / 0 | 626.753 | 623.687 | 80 | 89 |
| Windows / 32 | 634.722 | 632.689 | 88 | 214 |
| Linux / 0 | 534.721 | 520.586 | 1 | 1 |
| Linux / 32 | 540.749 | 527.631 | 1 | 16 |

Means pool equally sized runs, overruns sum them; individual p99/maxima remain
in the CSV. Full-callback mean changes are small, especially Windows (under 1%),
and deadline misses do NOT consistently improve. Linux/current with CCs has one
p99 outlier of 1125.37us despite its lower mean. The full Windows 20-loop/96k/64
QCTFO profiles miss all 1024 observed deadlines. Keep the kernel improvement,
but do not claim reliable high-rate workspace capacity, better worst-case timing
or solved underruns. No best-of reruns or looser timing/audio acceptance.

The optional processor snapshot now accepts `AION_DRIVE_SNAPSHOT_REVISION` (default
fcef1a9 retained), to isolate this change against 9b874ef. Header compatibility,
immutable generated sources, explicit snapshot object linkage, EXCLUDE_FROM_ALL
and no-install guards remain. The accepted code uses the current shared-frame
header; its existing operations are unchanged and it never calls the new helper.
Check Drive::tick ownership in `drive-snapshot-link.map` and `/none` source identity.

Windows paired run `670628cb-dff1-42d7-86ee-57f3f7729987` was measured before commit
while dirty. Its Drive.cpp hash matches the committed candidate:
`951da30eae2ef2a293d3754a1f1afadd82c60d60f124ae7e28331b7d7bcff20e`.
StereoFrame.h SHA256:
`f52a830e325423dfbbdf41cf2813bfdab554fe70a1c2d3ef8b83a5e258e895ab`.
Drive.h/probe hashes and generated sources/results/metadata remain in
`build/drive-kind-paired-9b874ef657b7/`. All raw current logs use
`build/drive-midpoint-*`. Do not discard slower cases or unsuccessful observations.
Linux paired run `03017cca-895e-4c77-845e-1c2ce2191ae9` records clean HEAD
49c1001 and the same Drive.cpp hash. Both link maps were checked against the
explicit accepted snapshot object. After measuring, both normal app caches were
restored to benchmark OFF and revision fcef1a9.

## Commands

Use pinned Windows CMake/CTest under
`.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin`.
Use existing configured Release caches. Linux source/build/dependencies stay in
`/home/jorge/projects/Aion-Looper`, synced from a coherent committed Windows
snapshot with `tools/SetupWslWorkspace.sh`. Do not copy caches across OSes.

```powershell
cmake --build build/windows-core --config Release --target aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests aion_reference_probe --parallel 2
ctest --test-dir build/windows-core -C Release -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract|original_drive_settings_contract)$' --output-on-failure --no-tests=error
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
cmake -S . -B build/windows -DAION_BUILD_DRIVE_SNAPSHOT_BENCHMARK=ON -DAION_DRIVE_SNAPSHOT_REVISION=9b874ef
cmake --build build/windows --config Release --target AionLooper_Standalone aion_drive_processor_tests aion_processor_effects_benchmark aion_processor_effects_snapshot_benchmark --parallel 2
ctest --test-dir build/windows -C Release -R '^native_(drive_processor|effects_benchmark_fixture)_(64|128)$' --output-on-failure --no-tests=error
node tools/BenchmarkDriveSnapshot.cjs 9b874ef
```

```sh
cmake -S . -B build/linux -DAION_BUILD_DRIVE_SNAPSHOT_BENCHMARK=ON -DAION_DRIVE_SNAPSHOT_REVISION=9b874ef
cmake --build build/linux --target AionLooper_Standalone aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests aion_drive_processor_tests aion_reference_probe aion_processor_effects_benchmark aion_processor_effects_snapshot_benchmark --parallel 2
ctest --test-dir build/linux -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract|original_drive_settings_contract|native_(drive_processor|effects_benchmark_fixture)_(64|128))$' --output-on-failure --no-tests=error
node tests/AudioParityAudit.cjs build/linux/aion_reference_probe
cmake --build build/drive-sanitized --target aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract)$' --output-on-failure --no-tests=error
node tools/BenchmarkDriveSnapshot.cjs 9b874ef
```

Sanitizer cache: Debug, plugin builds off, CXX flags
`-fsanitize=address,undefined -fno-omit-frame-pointer`, linker flags
`-fsanitize=address,undefined`; no suppressions. Exact original configure command
is retained in the preceding bias-reuse note. Current session uses incremental
builds, not a fresh dependency setup.

For callback observation, run the ordinary processor benchmark without arguments
for all 24 cases (10/20 loops, 48/96k, 64/128, bypass/QCTFO/QCTFO+32 CCs). For a
same-framework comparison, run snapshot/current/current/snapshot, each with
`--case 96000 64 10`, saving every stdout separately. Each profile warms 256 /
measures 1024 callbacks. Snapshots are serviced off audio every 16 callbacks and
excluded from timing. Fixtures check finite/nonzero PCM, stable clip/state/FX,
delivered MIDI counts and zero watched callback C++ new/delete (aligned included).
Device/editor/message-dispatch timers/Arp/instruments/roundtrip are excluded.
Do not average p99s and call that a pooled percentile.

After measurements restore the optional benchmark OFF and revision fcef1a9 in
each app cache. No package, wrapper build, push or release/capacity promotion.
