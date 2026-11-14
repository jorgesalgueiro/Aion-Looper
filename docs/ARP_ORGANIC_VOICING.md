# Organic Arp DSP preview — 14 November 2026

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
