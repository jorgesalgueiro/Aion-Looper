# Organic Arp voicing

## Product integration — 15 November 2026

The native Arp module now has an **Original / Loose** voicing selector, with the
same command/state implementation available to the running-app CLI. Fresh
effects select Loose on their first successful enable. Bypass/re-enable,
playback/history restarts, MIDI slider edits and ordinary save/reload retain the
selected mode and seed. DEL creates a fresh effect; the following first enable
selects Loose again. Original presets explicitly select Original; choosing a
voicing explicitly clears the named preset identity but retains its controls.

Seeds for fresh effects are deterministic per track:
`123456789 ^ (0x9e3779b9 * (zeroBasedTrack + 1))`, with unsigned 32-bit wrap.
Different tracks receive different seeds; clearing a track repeats its initial
identity. This is not entropy-based seed selection. Persisted seeds, including
original ones, are never replaced merely on bypass, restart or load.

`LoopArps` version **2** persists explicit `voicing`, `fresh` and the existing
exact controls/BPM/seed. Version 1 and absent banks load Original with
`fresh=false`, including state loaded before prepare. Invalid/missing mode,
invalid fresh/enabled/preset combinations and future versions reject atomically.
No parameter/action IDs, MIDI binding slots, original presets or capacity change.

Live mode changes keep the graph/ring, STAY sequence, envelope and reference RNG
ownership. Returning to Original releases stereo motion over the existing 80ms
smoothing rather than jumping to centre; the tiny tail snaps to zero to avoid
denormals. Onset policy changes at the live gesture, without lookahead. Ordinary
control and preset smoothing remains unchanged. Fresh classification by itself
does not schedule an unnecessary startup audio restore; an explicitly loaded
legacy bank still restores its original classification.

```sh
python tools/aion.py arp 1 get
python tools/aion.py arp 1 voice --voicing loose
python tools/aion.py arp 1 voice --voicing original
```

These use the existing private Standalone mailbox and exact Arp receipts. GUI
and CLI do not create another audio engine. The selector is not a new continuous
CC target; the existing nine Arp MIDI sliders retain IDs 124..132.

Windows and Linux development Standalones build. Focused core/worker/owner and
production state/CLI checks pass at 64/128; actual WebView2 selector, nonzero PCM,
presets, arbitrary chain reordering, MIDI and reload pass separately at 64 and
128. Exact prepared/restored/live stereo comparisons and a separately prepared
Loose graph oracle protect actual worker rendering, not just metadata.
Detailed commands/results and remaining validation are in MIGRATION_CURRENT.md.
This is a working preview; subjective listening, physical devices, Linux WebKit,
broader long-session and release checks remain separate.

## Historical DSP checkpoint — 14 November 2026

The owner selected loose timing, wider dynamics and natural stereo movement for
**new** instances, preserving original presets and saved sound. This checkpoint
implements and tests the optional DSP path; it does **not** change product
defaults, session formats, GUI/CLI commands or the original controls. It is not
complete product acceptance. All existing calls still select `original`.

## Audible policy and acceptance

`ArpDelay` and the existing streaming `ArpDelayGraph` accept an explicit trailing
`ArpVoicing::loose` at off-audio prepare. Reset retains the mode and current
controls, clears delay/motion history, and restarts the explicit seed. A separate
fixed-state random stream preserves the reference kernel's random draws.

- First onset stays on the original grid. Following note offsets are bounded by
  +/-25% of the interval times Feel. Intervals use differences of successive
  offsets: at constant tempo there is no cumulative random walk.
- The existing envelope target receives a per-note factor from
  `1 + Feel * (uniform(0.35, 1.25) - 1)`. Pitch scales, STAY sequence, glide,
  original delay-read wander and the existing floor of the wet gain remain.
- Random stereo targets are bounded by +/-0.85 times Feel, with 80ms smoothing.
  Wet/feedback channels use gains `sqrt(1-pan)` and `sqrt(1+pan)`; dry routing and
  the original post-effect pan remain unchanged. This is stereo balance, not a
  stereo widening or decorrelation processor.
- Feel zero on a fresh graph is exactly the original PCM. Lowering Feel during
  playback lets prior pan motion settle smoothly; it does not reset the graph.
- Delay/tempo changes re-anchor the next interval and discard old jitter offset,
  preventing a former long delay from imposing a long jitter interval after a
  short-delay edit. Quantized worklet controls retain their existing cadence.
- Idle suspends note/pan motion; reset clears it. There is no lookahead/buffering,
  callback allocation/deallocation, lock, I/O or new product audio engine.

The motion regression checks 100,000 intervals, exact grid identities and both
dynamic/stereo extremes. Actual kernel onset traces must follow the generated
intervals. Streaming comparisons require exact PCM/state at blocks 1/37/64/128,
seven rates 8–192kHz, live timing/Feel/STAY/feedback edits, tails/mono/stereo,
in-place processing, reset, invalid mode rejection and different seeds.
The unchanged original JS/DSP and existing streaming regressions remain gates.

## Reproduce and audition

Use the existing configured core build; CMake regenerates for the added test.

```powershell
cmake --build build/windows-core --config Release --target aion_arp_organic_tests aion_arp_tests aion_arp_stream_tests aion_arp_probe --parallel 2
ctest --test-dir build/windows-core -C Release -R '^(arp_organic_dsp_contract|arp_delay_realtime_contract|arp_delay_streaming_contract|js_native_arp_delay)$' --output-on-failure --no-tests=error
build/windows-core/Release/aion_arp_organic_tests.exe --audition build/arp-organic
build/windows-core/Release/aion_arp_organic_tests.exe --benchmark
```

```sh
cmake --build build/linux --target aion_arp_organic_tests aion_arp_tests aion_arp_stream_tests aion_arp_probe --parallel 2
ctest --test-dir build/linux -R '^(arp_organic_dsp_contract|arp_delay_realtime_contract|arp_delay_streaming_contract|js_native_arp_delay)$' --output-on-failure --no-tests=error
build/linux/aion_arp_organic_tests --audition build/arp-organic
build/linux/aion_arp_organic_tests --benchmark
```

`--audition PREFIX` writes 12-second stereo PCM16 dry/original/loose WAVs with the
same synthetic pluck, controls and seed (Feel 0.35). It calls the same DSP graph;
it is an offline developer diagnostic, **not** the running-app CLI. Synthetic
renders and numerical tests do not establish listening preference or hardware
acceptance. Benchmark: 20 parallel graphs, 48kHz, 512 warm-up and 1,024 measured
callbacks per mode/block. Wall time includes OS scheduling; it is not total
latency, processor/chain/MIDI load, or a hard real-time guarantee.

## Remaining product slice

Before enabling this sound, integrate an explicit versioned persisted voicing
and seed with the original-value owner, worker graph preparation, shared GUI/CLI
commands, snapshots and restore. Old banks/absent banks and original presets
must select original sound. Define fresh-instance versus disable/re-enable/DEL
semantics explicitly, retain seed across ordinary restarts, and verify save/
reload, MIDI edits, arbitrary user chains and editor reopening. Do not infer a
mode from Float32 controls, or silently retune original presets. Real UI and both
Standalone builds are required at that integration checkpoint. Physical audio,
Linux WebKit interaction and subjective listening remain separate checks.

## Executed evidence

Snapshot `969b933`, unchanged DSP in `8faab5e` (stronger delay-jump regression):

- Windows focused CTest selection above: **4/4, 30.11s**, log
  `build/arp-organic-windows-tests.log`. Additional real-kernel four-second to
  minimum-delay onset regression passes via the rebuilt test executable.
- Linux same four checks: **4/4, 30.69s**, native mirror log
  `build/arp-organic-linux-tests.log`.
- Linux existing ASan/UBSan/leak cache, organic + original streaming tests:
  **2/2, 19.13s**, `build/arp-organic-asan-tests.log`, no diagnostics. The first
  build could not find the newly added target; `cmake -S . -B
  build/overlap-asan` regenerated that existing cache before the passing run.
- The stronger `8faab5e` onset regression subsequently passes normally on Linux
  and under ASan/UBSan/leaks via the rebuilt `aion_arp_organic_tests` executable.
  Windows and Linux `cmake --build --preset windows-dev/linux-dev --parallel 2`
  Standalone builds both pass. No wrapper/package or actual GUI run was needed
  for this DSP-only opt-in path; GUI/state integration is still pending.
- Saved browser graph/preset/tempo fixture replay: **93 comparisons, 6,094,848
  stereo frames**, maximum error **2.3841858e-7**, unchanged 1e-6 limit.
  `build/arp-organic-original-browser-replay.log`. No fresh browser/GUI run.

Fixture replay commands (append output to the same log if desired):

```powershell
node tests/ArpGraphWebAudio.cjs --fixtures build/arp-graph-browser-fixtures.json
node tests/ArpGraphWebAudio.cjs --presets --fixtures build/arp-preset-browser-fixtures.json
node tests/ArpGraphWebAudio.cjs --tempo --fixtures build/arp-tempo-browser-fixtures.json
```

Windows warmed wall-time benchmark, microseconds, 20 Arp graphs alone:

| Block | Mode | Mean | p99 | Maximum | Deadline overruns |
| --- | --- | ---: | ---: | ---: | ---: |
| 64 | original | 422.655 | 537.2 | 619.1 | 0 |
| 64 | loose | 421.734 | 550.2 | 646.1 | 0 |
| 128 | original | 858.890 | 1032.3 | 1124.7 | 0 |
| 128 | loose | 847.840 | 1011.4 | 1080.9 | 0 |

Log: `build/arp-organic-windows-benchmark.jsonl`. Each case has 1,024 measured
callbacks; 48kHz deadlines are 1333.33/2666.67us. These sequential single runs do
not show a statistically established performance difference or full-engine load.
PCM16 listening renders have 576,000 stereo frames each, original/loose peak
0.231323/0.191254, zero clipped samples; no loudness normalization or limiter was
applied. Their sound still needs subjective audition.

Linux same warmed fixture (`build/arp-organic-linux-benchmark.jsonl`):

| Block | Mode | Mean | p99 | Maximum | Deadline overruns |
| --- | --- | ---: | ---: | ---: | ---: |
| 64 | original | 421.901 | 570.120 | 988.449 | 0 |
| 64 | loose | 414.738 | 484.308 | 561.721 | 0 |
| 128 | original | 785.802 | 939.861 | 1013.24 | 0 |
| 128 | loose | 805.619 | 958.910 | 1024.66 | 0 |

The same single-run/scheduling/full-application limitations apply on Linux;
cross-OS numbers do not establish a platform speedup.
