# Arp Delay migration

Active slice, 10 November 2026: isolated native port of the complete original
`ArpDelayProcessor`, before enabling the A checkbox or adding disconnected controls.
Baseline: clean `f56d20c`; compressor gates are retained in COMPRESSOR_MIGRATION.
Reference sibling is absent; `.reference/Ascii-Looper` and matching preserved
`resources/ui/Modules/effects.js` / `loopTracks.js` are read-only inputs.

Owner decisions: persistent per-loop delay/STAY/modulation state across playback
restarts, and live controls during OD/SUB/MULT/scheduled STOP. These are intentional
connected-stage extensions; EQ/compressor approvals alone were not reused.
The owner subsequently selected **zero added latency with causal wake-up**, rather
than buffering 128 samples to reproduce the original lookahead on small callbacks.
The owner also requests a more organic/natural sound and selected **organic
voicing by default for new instances**, preserving existing presets and saved
sounds. Keep the faithful reference baseline for compatibility; apply the new
default only when the connected layer creates a new instance. That voicing is
not yet implemented/enabled by this kernel checkpoint.

## Acceptance defined before implementation

- Complete worklet: stereo/mono/tail routing, four-second Float32 delay storage,
  two 4096-frame grain heads, Hann table, pitch glide/pluck/wander, 16-note STAY,
  twenty original interval tables, bounce/octave range, feedback HP/LP/FTZ and
  nearest-subdivision sync. Preserve all eight presets and original parameter data.
- Unchanged worklet oracle runs identical Float32 input/control and deterministic
  random streams (test injection only). Require <=1e-6 peak and <=1e-5 relative
  RMS; retain failures and thresholds. Compare note/sequence/clock metadata too.
- Explicit 64/128 direct-worklet calls and arbitrary short blocks, rates, all
  scales, mono/right-only/no input, silence/tails, STAY, sync/BPM/control changes,
  ring wrap and reset. A direct 64-frame worklet comparison does not establish
  equivalence to the browser's usual 128-frame render quantum or a connected
  64-sample device callback.
- No process/control/reset allocation, free, locks or unbounded work. Prepare
  storage off audio; failed prepare preserves the previous owner. Reset invalidates
  old samples with generation tags rather than clearing seconds of PCM on audio.
  Invalid controls preserve state; non-finite/overflowing audio is quarantined.
- Windows/Linux focused original/kernel/storage tests and Linux sanitizers before
  moving to the connected A slice. No package or dependency upgrade.

## Reference details that must not be guessed

The original has eleven user controls (nine ranges plus Sync/STAY), eight presets,
and a separate post-worklet StereoPanner driven by a sine oscillator/depth gain.
The worklet reads only the first value of each AudioParam array per invocation.
It consumes three random numbers per note even when Feel is zero. Test seeding
does not prescribe identical randomness for live tracks.

Scale labels differ from the actual interval tables at several indices. Preserve
both; correcting labels/tuning changes musical behavior. The idle gate scans
only left input, pauses internal state on silence, and decays its detector once
per worklet call. Oscillator normalization also depends on invocation boundaries.
These facts constrain the later native streaming adapter; do not hide a new
buffer delay or claim callback-size independence from a kernel-only comparison.

Connected GUI/CLI/MIDI/state, 50ms AudioParam automation, post-worklet pan, lifetime/
clear/tail ownership, storage budgeting and the organic treatment remain pending.
The original A control stays disabled until those acceptance checks pass.

## Windows checkpoint

The complete worklet kernel passes **84 unchanged-JS cases / 5,527,296 channel
samples**, peak **3.725290298461914e-9**. Exact note/sequence/random ownership
and bounded state tolerances are checked alongside PCM. All twenty interval
tables and eight original presets are included, plus live direct-worklet controls,
STAY/range/scale/sync/BPM/feedback, ring wrap, reset, left-only idle and mono/tails.
Final Windows CTests pass **3/3 (31.24s)**, including original asset bytes.
The native allocation/OOM/reset tests cover rates 8..384 kHz, 64/128 invocations,
in-place stereo and arbitrary short mono blocks with real ring wrap.

Actual original AudioWorklet in WebView2 **151.0.4129.107** passes **15 cases /
1,003,904 stereo frames**, peak **3.725290298461914e-9**, without changing the
1e-6 peak / 1e-5 relative-RMS limits. A seeded PRNG prefix makes this test
repeatable; the original worklet body is unchanged. The ignored fixture
`build/arp-browser-fixtures.json` stores its SHA256 and browser identity.

Product-document navigation/CSP and an insecure blank document prevented the
first browser harness attempts from loading a worklet. A separate excluded
`aion_effects_browser_host` now serves a secure test-only document with explicit
Blob worklet permission. It owns no processor, audio device or product controller;
no product CSP, UI, resource or native audio behavior changed. A test-host entry
point and test-source string-encoding defect were fixed before the browser pass.
This fixture is never installed/packaged.

Manual warmed Release measurement, twenty isolated stereo worklet kernels at
48 kHz, 512 warm / 2048 measured invocations: 64-frame mean/p99/max
**317.154/423.2/503us**; 128-frame **605.986/752.9/849.9us**. Nonzero PCM and
zero observed overruns against 1333.33/2666.67us periods. These exclude automation,
post-pan, Engine/mixing/telemetry, devices and round-trip latency. They are not
connected callback results or a worst-case guarantee.

Commands (VS CMake/CTest paths follow DEVELOPMENT_WORKFLOW):

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_tests aion_arp_probe
ctest --test-dir build/windows-core -C Release --output-on-failure -R '^(arp_delay_realtime_contract|js_native_arp_delay|original_ui_bytes)$'
cmake --build build/windows --config Release --parallel 2 --target aion_effects_browser_host
tools/TestArpDelayWebAudio.ps1
build/windows-core/Release/aion_arp_tests.exe --benchmark
```

The browser processing model is documented in the official
[Web Audio specification](https://www.w3.org/TR/webaudio-1.0/#rendering-loop).
Matching a worklet's PCM does not prove the separate native streaming/automation/
post-pan implementation, musical organic voicing, UI, hardware or release scope.

## Linux and ownership checkpoint

On committed source **551d021**, native WSL Release build and focused CTests pass
**3/3 (27.32s)**, including the complete 84-case oracle and original asset bytes.
Replay of all **15 actual Windows WebView2 fixtures** against Linux C++ passes,
with the same peak **3.725290298461914e-9**. This is not Linux WebKit rendering.
ASan/UBSan/leaks pass **1/1 (2.18s)** without diagnostics. The subsequent header
guard deletes copy/move operations so a future track assignment cannot copy or
free this ring on audio; a compile-time regression protects that restriction.
Final ownership source **3ae8cd0** builds on Windows and Linux. Its focused Linux
Release test passes **1/1 (0.40s)** and ASan/UBSan/leaks **1/1 (2.25s)** without
diagnostics; Windows unit coverage also passes. This header-only ownership guard
does not change PCM calculations, so unchanged passing sound suites were reused.

Linux manual warmed Release timing, same twenty-instance configuration: 64-frame
mean/p99/max **254.777/336.325/426.066us**, 128-frame
**513.185/622.509/1098.53us**, zero observed overruns. All timing is isolated
worklet processing, excluding the connected chain/adapter and physical latency.

```sh
cmake --build build/linux --parallel 2 --target aion_arp_tests aion_arp_probe
ctest --test-dir build/linux --output-on-failure -R '^(arp_delay_realtime_contract|js_native_arp_delay|original_ui_bytes)$'
node tests/ArpDelayWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-browser-fixtures.json
build/linux/aion_arp_tests --benchmark
cmake -S . -B build/overlap-asan
cmake --build build/overlap-asan --parallel 2 --target aion_arp_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --output-on-failure -R '^arp_delay_realtime_contract$'
```

Use the retained Debug/plugin-OFF sanitizer cache with
`-fsanitize=address,undefined -fno-omit-frame-pointer`. Copy only the generated
ignored browser fixture to the native WSL build directory; source changes travel
through committed snapshots. Logs: `build/arp-{final-ctest-windows,ctest-linux}.log`,
`arp-browser-windows.log`, `arp-browser-replay-linux.log`, `arp-asan-test.log`,
`arp-benchmark-{windows,linux}.log`, `arp-owner-unit-{windows,linux}.log`,
`arp-owner-asan-test.log`.

Before any connected enablement: define the 128-frame reference/64-frame
native adapter's idle/wakeup semantics without hiding latency; combine the
accepted worklet and automation/post-pan stages, then bounded storage ownership and shared
GUI/CLI/MIDI/state, preserving dry PCM and accepted persistent/live policies.
The worklet's feedback descriptor default **0.18** differs from the factory's
**Float32(2 / 11.1)**; connected defaults must use the factory mapping. The stronger
organic voicing must remain distinguishable from the faithful baseline and must
not rewrite saved sounds. No later effect has started; no package or push.

## Automation / post-pan slice — 10 November 2026

Acceptance: preserve original `smoothSetParam` (cancel, anchor current `.value`,
50ms exponential target), all ten a-rate worklet parameters plus pan rate/depth,
first-value worklet sampling on a global 128-frame clock, repeated gestures and
stereo crossfeed pan after the complete wet/dry output. Compare actual browser
AudioParams and nonzero stereo output at unchanged **1e-6 peak / 1e-5 relative
RMS** audio limits; normalized parameter error <=1e-6. Cover 64/128 and arbitrary
subdivision, static/running/stopped LFO, retarget/reset, malformed controls and
zero callback new/delete. This is a DSP stage, not enabled product controls.

`ArpModulation` adds fixed-storage automation and post-pan, reusing the verified
Compressor timeline arithmetic without changing that effect. A single read-only
sine table is constructed before processing, with rate-dependent strided views.
Chromium's Float32 phase increments, static four-lane phase and interpolation
matter: a plain analytic sine failed the retained 5 Hz browser case at peak
**1.564621925354004e-6**. The corrected oscillator passes that case without a
tolerance change. Initial failure remains in `build/arp-mod-initial-failure-windows.log`.
A later pan-rate target of zero preserves the phase already reached; it does
not force the pan back to center. Only a fresh/reset oscillator starts at center.

The actual browser harness uses a test-only parameter tap with the original
worklet descriptors, unchanged original `smoothSetParam`, and real Oscillator,
Gain and StereoPanner nodes. It tests the factory's always-present post-pan
routing, separately from the unchanged worklet PCM fixture. It does not yet prove
their connected chain or streaming idle behavior. Original resources/UI stay
unchanged. Sources used for the oscillator correction retain Chromium's BSD
notice in the new source file:
[oscillator](https://raw.githubusercontent.com/chromium/chromium/main/third_party/blink/renderer/modules/webaudio/oscillator_handler.cc),
[periodic wave](https://raw.githubusercontent.com/chromium/chromium/main/third_party/blink/renderer/modules/webaudio/periodic_wave_handler.cc),
[stereo pan specification](https://www.w3.org/TR/webaudio-1.1/#stereopanner-panning).

Run the owned hardware-free browser host with
`tools/TestArpDelayWebAudio.ps1 -Modulation`; replay its ignored, source-hash-checked
fixture with `node tests/ArpModulationWebAudio.cjs --probe PROBE --fixtures build/arp-modulation-browser-fixtures.json`.
The probe's `modprepare/modtarget/modrender` commands are test-only, not a second
audio engine or the product CLI. Product A stays disabled. The owner has now
selected zero-latency causal wake-up; the following streaming slice implements it.

Windows results: **22 actual WebView2 fixtures / 66 comparisons / 2,162,688 stereo
frames**, with 64/128/37 subdivisions and rates 8..384 kHz. Maximum audio peak
**2.2724270820617676e-7**, maximum normalized parameter error
**4.4373207837449527e-7**; all original thresholds retained. Focused CTests pass
**2/2 (2.39s)**, including original assets. Twenty automated modulation/post-pan
stages at 48 kHz (512 warm / 2048 measured) show mean/p99/max **146.065/190.7/224.7us**
at 64, **281.738/409.2/577.8us** at 128, zero observed overruns. This excludes
worklet processing, streaming, Engine/devices and round-trip latency; it is not
a combined callback deadline guarantee. Logs `build/arp-mod-{browser,ctest,benchmark}-windows.log`.

Committed source **f6e9e2f** builds in native WSL. Linux focused tests pass **2/2
(0.85s)**, and all **66** recorded Windows-browser comparisons replay against
Linux C++ with identical maximum errors above. This is not Linux WebKit audio/UI
validation. Linux ASan/UBSan/leaks pass **1/1 (3.19s)** with no diagnostics.
Twenty automated stages, same warmed configuration: 64-frame mean/p99/max
**119.752/187/255.109us**, 128-frame **243.422/331.008/369.416us**, zero observed
overruns. The checksum agrees with Windows. Runtime source remains unchanged
after this acceptance; Windows additionally reran ownership/allocation tests
after making sine-table initialization explicitly prepare-only.

```sh
cmake --build build/linux --parallel 2 --target aion_arp_tests aion_arp_probe
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_delay_realtime_contract|original_ui_bytes)$'
node tests/ArpModulationWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-modulation-browser-fixtures.json
cmake --build build/overlap-asan --parallel 2 --target aion_arp_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^arp_delay_realtime_contract$'
build/linux/aion_arp_tests --benchmark-modulation
```

Windows used the same focused CTest selection with `-C Release`,
`build/windows-core/Release/aion_arp_tests.exe --benchmark-modulation`, and the
browser command above. Linux logs: `build/arp-mod-{build,ctest,replay,benchmark}-linux.log`,
`arp-mod-asan-{build,test}.log`. Only the generated ignored fixture was copied;
source traveled through the safe committed-snapshot helper. No wrappers,
Standalone/editor changes, dependency upgrade, product voicing, package or push.

Next is the accepted causal streaming chain and bounded per-track storage
admission/failure/lifecycle handling before
shared GUI/CLI/MIDI/versioned persistence. Do not enable A or advance to another
effect until those end-to-end gates pass. The original kernel's 84-case JS and
15-case actual-worklet evidence is reused unchanged; this stage does not replace it.

## Causal streaming slice — acceptance

The owner selected no added buffering latency. `ArpDelayGraph` combines the same
worklet math, first-a-rate parameter latch and stereo post-pan; controls and LFO
run on a continuous render timeline. The worklet detector decays once per **128
audio samples**, and internal rotations normalize once per active quantum, not
once per callback or event subdivision. Controls never restart delay/STAY/LFO.

When tail <1e-5, preserve the original left-only wake trigger, but inspect input
causally: freeze worklet history until the first left sample >=1e-5, emit zero
before it, and process that triggering sample immediately. Normalize at wake,
then keep processing the rest of that quantum; accumulate the detector peak
across all slices. The skipped prefix does not advance note/grain/delay state.
This intentionally differs from original lookahead during that wake quantum;
there is no promise of a small bound on later feedback/sequence divergence.
The separate original whole-invocation kernel/oracle remains available and its
regressions must still pass. Direct and streaming APIs require reset between modes.

Acceptance: exact native PCM/state under 1/37/64/128/257 partitions and arbitrary
sample-offset controls; separate explicit causal wake/no-added-latency assertions;
unchanged original factory/worklet/automation/pan browser PCM at existing 1e-6
peak / 1e-5 relative RMS for awake/tail cases; mono/stereo/no input, idle/right-only,
in-place stereo, rate changes, zero blocks, reset, bounded invalid requests, whole
graph prepare OOM rollback, zero callback new/delete, Windows/Linux and sanitizers.
Use fixed 128-sample pan scratch; reject >4096-frame calls before state mutation.
Standalone GUI/CLI/MIDI, storage admission/retirement, persisted voice settings,
editor/track integration and organic defaults remain later connected acceptance.

### Windows streaming evidence

Native exact PCM/state partition tests pass for rates 8..384 kHz, 0/1/2 channels,
1/37/64/128/257 samples, matching in-place stereo, non-quantized control gestures,
real idle/right-only/wake intervals, reset and complete graph OOM rollback.
The causal fixture drains the real detector below 1e-5, sends right-only input
while idle, then introduces a left impulse 17 samples into a 64-frame slice.
The impulse is output at sample 17; only the remaining 47 samples advance the
worklet clock/ring. No processing/control/reset new/delete is observed.

The unchanged original worklet **84-case** CTest passes after extracting the shared
render primitive; its limits were not changed. Full actual original factory,
worklet, smoothSetParam and auto-pan now pass **21 fixtures / 63 comparisons /
4,128,768 stereo frames**, peak **2.384185791015625e-7**, including eight presets,
mono, six rates and live controls at 22.05/44.1/48/96 kHz. Initial combined 96k
automation failed at **2.16066837310791e-6**: Float32 subtraction at event anchors
and deferred-event lane/tail handling must be preserved. The fix affects only
ArpModulation, not the accepted Compressor. A browser-derived `.35 -> 1` anchor
golden regression protects the one-ULP difference. Existing **66** isolated pan/
parameter fixtures still pass, with **zero parameter-sample error** now observed.

The test-only wrapper records first/last worklet parameters for diagnosis; the
original process body and parameters/PCM are unchanged. All original versus
observed factory PCM was compared **bit-identically (0 differing samples)**,
with identical applied-event frames. The initial failing fixture/log is retained
in `build/arp-graph-original-fixtures.json` / `arp-graph-initial-failure-windows.log`.
No gate or tolerance was weakened or failed case removed.

Windows focused CTests passed **3/3 (3.84s)**; the subsequent final anchor/kernel/
streaming regression selection passed **2/2 (2.20s)**. Actual browser log:
`build/arp-graph-final-browser-windows.log`; fixture `arp-graph-browser-fixtures.json`.
Replay requires unchanged original-source hashes. Commands:

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_tests aion_arp_stream_tests aion_arp_probe
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(arp_delay_realtime_contract|arp_delay_streaming_contract|js_native_arp_delay|original_ui_bytes)$'
tools/TestArpDelayWebAudio.ps1 -Graph
node tests/ArpModulationWebAudio.cjs --fixtures build/arp-modulation-browser-fixtures.json
build/windows-core/Release/aion_arp_stream_tests.exe --benchmark
```

Nominal AudioParam clipping also matters: `.01 -> 4` can briefly cancel to below
the worklet's .01 minimum. The original clips arrays and their exposed last value
after timeline calculation. Native now does the same, preventing valid gestures
from failing midway through processing. Native and actual browser endpoint cases
retain this regression at 8/48k, including real ring wrap and full feedback.

Twenty combined DSP graphs at 48k, 512 warm / 2048 measured callbacks initially
showed **13 overruns** at 128 with LoopBank's single-sample call pattern
(mean/p99/max **1497.69/2173.3/3792.6us**). Coefficient caching now computes
tempo/rotation/exp constants only when the latched worklet values change, keeping
the same arithmetic. The retained 84-case unchanged worklet, kernel and stream
CTest selection passes **3/3 (31.80s)** after this optimization.

Final warmed timing: grouped 64 mean/p99/max **492.605/603.1/947.6us**, grouped 128
**957.985/1149.1/1519.9us**; single-sample dispatch **606.741/725.6/821.7us** at 64,
**1172.29/1336.8/1594.4us** at 128. Nonzero PCM, matching checksums, zero observed
overruns/allocations in all four optimized runs. This excludes Engine/EQ/compressor/
mixing/devices and round-trip latency; full-bank headroom still requires its own
gate. Failed timing log `build/arp-graph-precache-overruns-windows.log`, final
`arp-graph-cached-benchmark-windows.log`; do not discard the failed observation.
