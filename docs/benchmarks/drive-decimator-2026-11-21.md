# Measured drive decimator scheduling

Final source: `ea07783`. The planned intra-drive attribution was completed first,
then one bounded implementation was tried. The all-platform candidate is
`8e335b8`; **its Windows scheduling change was rejected** after full-callback
measurements. Final Windows/MSVC and unvalidated platforms retain the accepted
schedule. Only GNU/Linux x64 uses the paired decimator recurrence.

No equations, coefficients, state/layout, oversampling, latency alignment,
smoothing, control/parameter IDs, presets, routing, persistence or UI changed.
Wet and dry filters still advance every sample, even at full wet/dry mix.
Interleaving independent recurrences is an instruction-scheduling change, not
an event/transport scheduling change. No new memory, locks or callback services.

## Attribution before implementation

The first recorded gmon time histograms from the preceding profiling step were
resolved with existing GNU addr2line inline stacks, without another audio run.
All 123 and 468 samples are conserved. Scope remains **coarse instrumented
executable samples**, including fixture/snapshot work, excluding shared-library
time. These are not native callback latency or exact instruction costs.

| Identified drive-family sample group | 10 loops / 96k / 64 | 20 loops / 96k / 128 |
| --- | --- | --- |
| Downsampling, wet and dry combined | 31 / 72 (43.06%) | 106 / 296 (35.81%) |
| Upsampling | 8 / 72 (11.11%) | 43 / 296 (14.53%) |
| Nonlinear processing | 3 / 72 (4.17%) | 13 / 296 (4.39%) |
| DC/tone/mix | 3 / 72 (4.17%) | 20 / 296 (6.76%) |
| Control smoothing | 2 / 72 (2.78%) | 9 / 296 (3.04%) |
| Other drive work / unresolved within drive | 25 / 72 (34.72%) | 105 / 296 (35.47%) |

Downsampling is the largest **resolved** group; the other-drive fraction prevents
an exact total cost ranking. Wet versus dry decimation cannot be split by these
samples. Three additional samples in the larger executable histogram have no
resolved stack and are explicitly retained outside the identified drive count.
All stage rows, including other executable/unresolved counts, are in
`drive-stage-attribution-2026-11-21.csv`.

`tools/AttributeDriveProfile.cjs` supports only the first primary-executable GNU
histogram in ELF64 little-endian profiles. It bounds file/bucket/stack/output
sizes, address arithmetic and resolver runtime, validates format, verifies both
source and executable SHA256, and records every nonzero bucket/inline stack.
Unsupported data fails clearly. A frozen source argument lets historical data
be analyzed after current code changes without editing any working source.
Malformed/truncated profiles, address order, inline classification, unknown
locations and ambiguous source anchors have a retained Node/CTest contract.
Actual CLI checks also reject stale source/executable hashes.
GNU [addr2line documentation](https://sourceware.org/binutils/docs/binutils/addr2line.html)
and the installed `sys/gmon_out.h` define the formats used; no library code copied.

Original bundles remain in native WSL storage:
`build/callback-profile-{yIhOXGVe,OvMWs5Ku}`. Guarded JSON is `stages-guarded.json`.
The first bundle also archives `executable` and `source-CdrWumpr.cpp`, so later
rebuilds cannot silently replace the measured executable/source. Sampling source
is a512d8a Drive.cpp, SHA256
`951da30eae2ef2a293d3754a1f1afadd82c60d60f124ae7e28331b7d7bcff20e`.

## Acceptance and verification

The candidate pairs wet/dry sections at each decimation stage, retaining each
path's original arithmetic and state order. Initial same-process accepted/current
component probes check exact PCM and unchanged class size for all nine T/O/F
steady/tone-mix/bias-or-gain cases, 20 instances at 96k/64. Mean reductions were
about 1–4% on Windows and 3–6.5% on GNU/Linux in this fixture. Component gains
alone were not used to accept the Windows implementation.

| Gate | Windows | Linux / native WSL storage |
| --- | --- | --- |
| Exact native + forced-portable reference | Each 4,818,576 samples, max error 0 | Same |
| Core/reference/connected + offline parser | Candidate 6/6, 9.73s; final fallback 6/6, 11.06s | Candidate 10/10, 17.13s including processor/fixture cases |
| Production processor/CLI/MIDI, 64/128 | Final fallback 2/2, 56.51s | Included above |
| Final normal benchmark fixtures, 64/128 | 2/2, 0.91s | Normal artifacts rebuilt; selected body unchanged |
| ASan/UBSan/leak detection, no suppressions | Not executed | 4/4, 256.95s |
| Candidate JS capture/OD/SUB audit | 126 cases, 965,826 samples, max error 7.394757495227111e-8 | Same |
| Standalone build | Pass, final fallback | Pass |

Final selector leaves validated GNU/Linux native AND portable preprocessed token
streams byte-identical to candidate 8e335b8 (`build/drive-decimator-token-zRDZlerT`).
Linux passing suites were not repeated merely for this inactive-platform guard.
Windows was rebuilt/retested after restoring its old schedule. No original case
or tolerance was deleted/loosened. GUI/IPC unchanged: prior real UI evidence is
reused, without claiming newly executed UI/device/listening/DAW/Apple acceptance.

## Complete timing evidence and rejection

These CSVs measure **candidate 8e335b8**, not a new Windows gain in final ea07783:

- `drive-decimator-paired-2026-11-21-{windows,linux}.csv`: all nine component cases.
- `drive-decimator-processor-2026-11-21-{windows,linux}.csv`: all 24 production
  fixtures per OS, 10/20 loops, 48/96k, 64/128, bypass/QCTFO/QCTFO+32 CCs.
- `drive-decimator-abba-2026-11-21-{windows,linux}.csv`: all twelve rows per OS,
  fixed accepted/current/current/accepted sequence at ten loops/96k/64.

Full fixtures pass state/nonzero/finite PCM, real MIDI counts and zero watched
callback C++ new/delete, with every checksum matching the prior accepted normal
matrix. These integrity checks do not establish callback deadline compliance.
All timing runs were serial after builds/audits/sanitizers; OS services were not
isolated. Baseline is 6b24460, with header compatibility and explicit snapshot
object ownership. Warm 256 / measure 1024 callbacks per processor profile.

| Platform / CCs per callback | Baseline mean us | Candidate mean us | Baseline overruns /2048 | Candidate overruns /2048 |
| --- | --- | --- | --- | --- |
| Windows / 0 | 623.092 | 638.892 | 418 | 500 |
| Windows / 32 | 632.257 | 626.936 | 491 | 437 |
| Linux / 0 | 503.985 | 498.174 | 13 | 12 |
| Linux / 32 | 520.052 | 513.021 | 18 | 12 |

Means pool two equally sized runs; individual p99/maxima remain in the CSV,
never averaged into a pooled percentile. Budget: 666.667us. Linux mean gains are
about 1.2–1.4% here, not a general guarantee. Windows no-MIDI observations were
slower; no cause for the variance is established. Therefore MSVC retains its
original schedule, despite the attractive component result. No best-of reruns
or favorable-case filtering. High-rate loaded-workspace capacity remains open.

## Reproduce and resume

Use existing pinned Release caches. Exact build/normal reference/audio-audit/
sanitizer commands are the preceding midpoint note, adding
`drive_profile_attribution_contract` to the focused regex. Current raw logs use
`build/drive-decimator-*`; final Windows fallback logs use
`build/drive-decimator-final-*`. Offline contract: `node tests/DriveProfileAttributionTests.cjs`.

Equivalent consolidated focused commands (session logs retain separate rebuilds
and the final Windows fallback rerun):

```powershell
cmake --build build/windows-core --config Release --target aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests aion_reference_probe --parallel 2
ctest --test-dir build/windows-core -C Release -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract|original_drive_settings_contract|drive_profile_attribution_contract)$' --output-on-failure --no-tests=error
cmake --build build/windows --config Release --target AionLooper_Standalone aion_drive_processor_tests aion_processor_effects_benchmark --parallel 2
ctest --test-dir build/windows -C Release -R '^native_(drive_processor|effects_benchmark_fixture)_(64|128)$' --output-on-failure --no-tests=error
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
```

```sh
cmake --build build/linux --target AionLooper_Standalone aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests aion_drive_processor_tests aion_reference_probe aion_processor_effects_benchmark --parallel 2
ctest --test-dir build/linux -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract|original_drive_settings_contract|drive_profile_attribution_contract|native_(drive_processor|effects_benchmark_fixture)_(64|128))$' --output-on-failure --no-tests=error
node tests/AudioParityAudit.cjs build/linux/aion_reference_probe
cmake --build build/drive-sanitized --target aion_drive_tests aion_drive_portable_tests aion_drive_connected_tests aion_signal_chain_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(drive_(dsp|portable_dsp|connected)_contract|signal_chain_connected_contract)$' --output-on-failure --no-tests=error
```

Use the pinned Windows CMake/CTest path documented in the preceding note; Linux
runs in `/home/jorge/projects/Aion-Looper` after committed snapshot sync. The
existing sanitizer cache is Debug, plugin builds off, CXX flags
`-fsanitize=address,undefined -fno-omit-frame-pointer`, linker flags
`-fsanitize=address,undefined`. Profiling remains separate and OFF.

Historical attribution, inside the native WSL checkout:

```sh
base=build/callback-profile-yIhOXGVe
frozen=$(mktemp "$base/source-XXXXXXXX.cpp")
git show a512d8a:source/core/Drive.cpp > "$frozen"
expected=$(sed -n '3s/ .*//p' "$base/metadata.txt")
for run in "$base" build/callback-profile-OvMWs5Ku; do
    data=$(find "$run" -maxdepth 1 -name 'gmon.*' -type f)
    node tools/AttributeDriveProfile.cjs "$base/executable" "$data" \
      951da30eae2ef2a293d3754a1f1afadd82c60d60f124ae7e28331b7d7bcff20e \
      "$expected" "$frozen" > "$run/stages-reproduced.json"
done
```

For new comparisons explicitly set `AION_DRIVE_SNAPSHOT_REVISION=6b24460` with
`AION_BUILD_DRIVE_SNAPSHOT_BENCHMARK=ON`, build the normal and snapshot processor
benchmarks, then use the existing full/`--case 96000 64 10` commands. Historical
Windows always-paired data requires candidate 8e335b8; final Windows deliberately
uses its previous path. Do not mistake the old experimental binary for final
Windows behavior. Normal artifacts were relinked after fallback restoration.
Restore snapshot OFF/default revision fcef1a9; callback profiling stays OFF.
No packaging, version/dependency change, push or release-capacity promotion.
