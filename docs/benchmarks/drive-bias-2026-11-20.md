# Drive bias reuse: validation and measurements

Production snapshot: `9bbc11e`. Accepted DSP comparator:
`fcef1a9d838967b6fb34a2371af9548b7cb07af8`.

The only production change is reuse of a pure bias-correction result when its
two double arguments are bit-identical. Numeric equality is insufficient for
signed zero. Drive.h, object size, smoothing, oversampling, routing, control IDs,
presets, persistence and presentation are unchanged. No persistent cache or
approximation was added. Frozen scalar comparison includes the previous cases
and new long-settling, retarget, reset, rate-change and signed-zero boundaries.
New exact-equality assertions supplement the existing unchanged 1e-6 bound.

The earlier persistent-cache (`ca9ea81`) and type-specialization (`0faa24b`)
experiments were rejected. Their logs remain in ignored `build/drive-settled-*`
and `build/drive-kind-*`; do not attribute their results to this implementation.

## Executed verification

| Gate | Windows | Linux / native WSL storage |
| --- | --- | --- |
| Core/reference/connected-chain contracts | 5/5, 9.97s | Included in 9/9, 16.22s |
| Production processor + benchmark fixtures, 64/128 | 4/4, 31.32s | Included in 9/9 above |
| Frozen scalar DSP, native and forced-portable paths | Each: 4,671,120 samples, maximum error 0 | Each: same sample count, maximum error 0 |
| Capture/OD/SUB audio audit | 126 cases, 965,826 samples, maximum error 7.394757495227111e-8 | Same |
| ASan + UBSan, leak detection, halt on UB | Not executed | 4/4, 280.79s; no suppressions |
| Standalone build | Pass | Pass |
| Actual WebView2 pointer/key/MIDI/reload/PCM, 64/128 | 64 passed; 128 passed diagnostic retry; see limitation below | WebKit interaction not executed |

The first final-candidate UI run passed 64 but timed out evaluating drive-control
readiness at 128 (startup or reload was not distinguished in that log). An
unchanged-deadline/assertion retry of **128 only**, with additional browser
lifecycle diagnostics, passed. Earlier rejected candidates also had intermittent
CDP failures. Preserve the failed logs; this is **not a diagnosed or fixed
browser issue**, and a retry does not establish long-session/editor reliability.
The test now distinguishes initial bootstrap from reload and records up to 64
lifecycle events on failure. No product behavior, timeout or tolerance changed.

Local evidence: `build/drive-bias-{core-tests,processor-tests,audio-audit,ui}.log`,
`build/drive-bias-ui-128-diagnostic.log`; Linux
`build/drive-bias-{tests,sanitize-tests,audio-audit,snapshot-fixture}.log`.
Real UI covers original presets, sliders, failed-send recovery, 14 MIDI CC
controls, reopening synchronization, bypass, deletion and nonzero playback.
It uses synthetic callback PCM/MidiBuffer, not physical hardware.

## Measurement scope and provenance

All timings are warmed Release observations, not hard-real-time or round-trip
latency guarantees. Benchmark processes ran serially, without concurrent builds,
tests or owned UI hosts. Ordinary OS services were not isolated. Keep all cases,
including slower automation and tail outliers.

CSV files alongside this note:

- `drive-bias-paired-2026-11-20-{windows,linux}.csv`: nine paired component cases;
  20 processors, 96k/64, 256 warm / 2048 measured callbacks. Native accepted SSE2
  and current code share one executable; order alternates each callback; PCM
  equality and equal object footprint are checked. `automated`: 0 steady,
  1 tone/mix, 2 bias (T/F) or amount (O). This is not a full audio callback.
- `drive-bias-engine-2026-11-20-{windows,linux}.csv`: all 40 Engine fixtures,
  10/20 loops, 48/96k, 64/128, bypass, drives, automated drives, Q/C plus drives,
  automated Q/C plus drives. Arp/instruments/JUCE/device/editor are excluded.
- `drive-bias-processor-2026-11-20-{windows,linux}.csv`: all 24 production
  processBlock fixtures, same rates/blocks/track counts, bypass, QCTFO and QCTFO
  plus 32 unassigned CCs per callback. Includes APVTS, JUCE MIDI and bounded
  telemetry; excludes device, editor, message-dispatch timers and round trip.
- `drive-bias-abba-2026-11-20-{windows,linux}.csv`: fixed same-framework
  baseline/current/current/baseline schedule at ten loops, 96k/64. Each run
  retains bypass and both effects profiles, 256 warm / 1024 measured callbacks.
  Snapshot identity ends in `/none`: original header, no signature adapter.
  Link maps confirm Drive::tick resolves from the accepted snapshot object.

Every full-matrix row has zero tracked callback C++ new/delete (including aligned
forms), finite nonzero PCM and matching accepted baseline fixture checksum.
Checksums are rounded diagnostics; the frozen scalar tests establish the stronger
sample comparison. Existing Engine baseline files are
`compressor-hotpath-after-workspace-windows.jsonl` and
`compressor-hotpath-fresh-after-workspace-linux.jsonl`; processor baseline files
are `processor-effects-benchmark-{windows,linux}.jsonl`, all under `build/`.
These separate historical full matrices are not controlled timing pairs.

Same-framework ABBA observations, ten loops at 96k/64 (nominal budget 666.667us):

| Platform / CCs per callback | Baseline mean us | Current mean us | Baseline overruns /2048 | Current overruns /2048 |
| --- | --- | --- | --- | --- |
| Windows / 0 | 650.574 | 632.759 | 201 | 125 |
| Windows / 32 | 643.076 | 635.600 | 218 | 149 |
| Linux / 0 | 607.742 | 588.769 | 276 | 149 |
| Linux / 32 | 618.731 | 584.561 | 271 | 129 |

Means above pool two equally sized runs; overruns sum those runs. Individual
p99/max values remain in the CSV, **not averaged into a pooled percentile**.
Linux effects/no-CC current p99s (913.411/1007.9us) are worse than the baseline
(812.046/829.32us), despite lower means/overrun counts. The separate later full
Windows matrix has worse ten-loop/96k/64 means and tails than these controlled
runs. Do not cherry-pick timings or infer a universal speedup/capacity pass.
The paired component common steady/tone-mix paths improve modestly; moving-bias
T/F means are slightly slower on both OSes (roughly 0.6–0.8%). This is a bounded,
state-free tradeoff, not justification for weakening smoothing or sound quality.

The Windows paired observation was taken before the candidate commit while the
worktree was dirty. Its three source SHA256 values were verified against 9bbc11e:

| File | SHA256 |
| --- | --- |
| source/core/Drive.cpp | 4e7b5759c72f2181241753b34487698e552db9bba7940d708bff7c88f56ce350 |
| source/core/Drive.h | 69c10b31b0869fe730c7112fdec5faad880049507d406826eefb6ca614cfe99a |
| tests/DriveKindBenchmark.cpp | c05f8f9bb29de2713a279f803ebf6eee05bc1b3a7ba9a02f8ef4640dae617d52 |

Windows paired run UUID: `875cf94e-eafe-48f8-9a1a-e087966e79d9`.
Linux paired run UUID: `c5933408-2dfa-4e44-ac52-0f5bcffbf82b`.
Generated sources, original JSONL and unique metadata/build logs remain in
`build/drive-kind-paired-fcef1a9d8389/` on each OS. The helper refuses modified
generated sources. Raw final matrices are `build/drive-bias-*.jsonl`.

## Reproduction

Use the existing configured Release caches and pinned tools. Windows commands
below use `cmake`/`ctest` from
`.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin`.
Do not copy caches across OSes. Linux runs in `/home/jorge/projects/Aion-Looper`
after a clean committed snapshot sync using `tools/SetupWslWorkspace.sh`.

```powershell
cmake --build build/windows-core --config Release --target aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests aion_reference_probe aion_effects_engine_benchmark --parallel 2
ctest --test-dir build/windows-core -C Release -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract|original_drive_settings_contract)$' --output-on-failure --no-tests=error
cmake --build build/windows --config Release --target AionLooper_Standalone aion_drive_processor_tests aion_mouse_host aion_processor_effects_benchmark --parallel 2
ctest --test-dir build/windows -C Release -R '^native_(drive_processor|effects_benchmark_fixture)_(64|128)$' --output-on-failure --no-tests=error
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
./tools/TestDriveUi.ps1
./build/windows-core/Release/aion_effects_engine_benchmark.exe --drives
./build/windows/Release/aion_processor_effects_benchmark.exe
node tools/BenchmarkDriveSnapshot.cjs fcef1a9
```

```sh
cmake --build build/linux --target AionLooper_Standalone aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests aion_drive_processor_tests aion_reference_probe aion_effects_engine_benchmark aion_processor_effects_benchmark --parallel 2
ctest --test-dir build/linux -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract|original_drive_settings_contract|native_(drive_processor|effects_benchmark_fixture)_(64|128))$' --output-on-failure --no-tests=error
node tests/AudioParityAudit.cjs build/linux/aion_reference_probe
cmake -S . -B build/drive-sanitized -DAION_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/drive-sanitized --target aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract)$' --output-on-failure --no-tests=error
./build/linux/aion_effects_engine_benchmark --drives
./build/linux/aion_processor_effects_benchmark
node tools/BenchmarkDriveSnapshot.cjs fcef1a9
```

The sanitizer configure line records the existing cache setup; the current
session incrementally rebuilt and ran it. For the optional processor
counterfactual, configure the app cache with
`cmake -S . -B build/windows -DAION_BUILD_DRIVE_SNAPSHOT_BENCHMARK=ON`
(Linux: `-B build/linux`), then explicitly build
`aion_processor_effects_snapshot_benchmark`. Run snapshot/current/current/snapshot
executables with `--case 96000 64 10`; save **all** stdout separately. Windows
uses `--config Release` and executables in `build/windows/Release/`; Linux uses
`build/linux/`. The snapshot is EXCLUDE_FROM_ALL and never installed. Restore the
option OFF after measurements with the same configure command and
`-DAION_BUILD_DRIVE_SNAPSHOT_BENCHMARK=OFF`; the evidence files are retained.

No package, wrapper validation, physical device, Linux WebKit, DAW, listening or
long-session acceptance is claimed. High-rate deadline misses and the intermittent
browser timeout remain open. Investigate those before promoting a release or
claiming full workspace capacity; do not lower sound-quality or test thresholds.
