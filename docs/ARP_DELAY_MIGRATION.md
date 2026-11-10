# Arp Delay migration

Active slice, 10 November 2026: isolated native port of the complete original
`ArpDelayProcessor`, before enabling the A checkbox or adding disconnected controls.
Baseline: clean `f56d20c`; compressor gates are retained in COMPRESSOR_MIGRATION.
Reference sibling is absent; `.reference/Ascii-Looper` and matching preserved
`resources/ui/Modules/effects.js` / `loopTracks.js` are read-only inputs.

Owner decisions: persistent per-loop delay/STAY/modulation state across playback
restarts, and live controls during OD/SUB/MULT/scheduled STOP. These are intentional
connected-stage extensions; EQ/compressor approvals alone were not reused.
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

Next, before any connected enablement: define the 128-frame reference/64-frame
native adapter's idle/wakeup semantics without hiding latency; implement original
AudioParam smoothing and post-pan; then bounded storage ownership and shared
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
audio engine or the product CLI. Product A stays disabled. The zero-latency versus
buffered exact idle-wakeup decision was requested from the owner and remains
pending; do not infer approval from elapsed time or implement that policy yet.

Windows results: **22 actual WebView2 fixtures / 66 comparisons / 2,162,688 stereo
frames**, with 64/128/37 subdivisions and rates 8..384 kHz. Maximum audio peak
**2.2724270820617676e-7**, maximum normalized parameter error
**4.4373207837449527e-7**; all original thresholds retained. Focused CTests pass
**2/2 (2.39s)**, including original assets. Twenty automated modulation/post-pan
stages at 48 kHz (512 warm / 2048 measured) show mean/p99/max **146.065/190.7/224.7us**
at 64, **281.738/409.2/577.8us** at 128, zero observed overruns. This excludes
worklet processing, streaming, Engine/devices and round-trip latency; it is not
a combined callback deadline guarantee. Logs `build/arp-mod-{browser,ctest,benchmark}-windows.log`.
