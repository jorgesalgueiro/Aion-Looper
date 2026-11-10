# Arp Delay migration

Active slice, 10 November 2026: isolated native port of the complete original
`ArpDelayProcessor`, before enabling the A checkbox or adding disconnected controls.
Baseline: clean `f56d20c`; compressor gates are retained in COMPRESSOR_MIGRATION.
Reference sibling is absent; `.reference/Ascii-Looper` and matching preserved
`resources/ui/Modules/effects.js` / `loopTracks.js` are read-only inputs.

Owner decisions: persistent per-loop delay/STAY/modulation state across playback
restarts, and live controls during OD/SUB/MULT/scheduled STOP. These are intentional
connected-stage extensions; EQ/compressor approvals alone were not reused.
The owner also requests a more organic/natural sound. Preserve the reference
baseline first; the additional preset/new-instance policy is awaiting selection.
Saved sounds must not change silently.

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
This fixture is never installed/packaged. Linux and sanitizer checks are next.

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
