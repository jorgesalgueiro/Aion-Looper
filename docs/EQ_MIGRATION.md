# Original Q/eQ migration

Active slice: ten-band per-loop eQ, before the other unported FX/SAVE controls.
The filter kernel was a prerequisite. Connected preview acceptance is recorded
below; the final toggle-gain repair needs a focused Linux rerun in this snapshot. Do not infer
full-chain, wet SAVE, hardware, deferred-wrapper or release acceptance.

## Connected implementation and current verification — 9 November 2026

Owner selection: persistent EQ per track. Healthy original Start/retrigger/history
already reuse their graph; separate graphs are a reconstruction fallback. Native
source edits retain filter state, while clear/panic/reconfiguration reset runtime
filters. Stopped source tails remain bounded by the existing source lifetime;
silent EQ cannot revive stopped playback. DEL resets EQ to original defaults/off.
This intentional persistence choice is separate from unchanged-worklet parity.

EQ runs before queued volume gating, loop volume/pan and MASTER, after source
mixing. Raw record/OD/SUB capture and stored-PCM WAV export exclude it. Toggle
insertion/removal retains actual original settled gain (see the repair below); controls use the proven
50ms target smoother and global 128-frame coefficient clock. Same-rate timeline
reset now explicitly resets that clock; a connected non-quantum target regression
guards the defect found during review. Overlapping browser graphs/transition
envelopes beyond this persistent model are not claimed bit-identical.

The dedicated `equalizer` RPC validates track/generation/expiry and bounded named
parameter batches. Existing GUI scheduling orders it with edits/DEL/panic; CLI
uses the same message owner/processor. Full preset/XY batches reject atomically.
Commands carry clear epochs; message intent reconciles to real audio completion,
including dropped MIDI-panic work. State is a strict version-1 `LoopEqualizers`
child of the existing APVTS tree (20 tracks, enable/preset/24 values), with legacy
default/off loading. Host restore never publishes to the SPSC queue from a host
thread: the message timer admits a complete 20-command bank or retries. Existing
parameter/action IDs, MIDI schema and presets remain compatible. Effect MIDI
Learn and wrapper-host automation are still deferred; no disabled mapping target
was enabled. Project-file PCM persistence and other FX/full/chain presets remain
independent work.

The original eQ toggles, FX/eQ navigation, preset names and three-column numeric
controls are adapted in native presentation code, leaving preserved assets intact.
The approved graph uses a separate message-owned native filter for bounded
256-point response/6-range queries, throttled to <=10Hz with document/selection/
settings fencing. Ten points can be selected even when they coincide; drag XY,
Shift for fine changes, Alt for gain only, Ctrl for frequency only. Selected peak
handles change Q through the same native warped bandwidth calculation. Arrow
keys edit frequency/gain; `[`/`]` edit Q; original numeric double-click resets
remain. Editor reload retains native settings and guards obsolete curve replies.

Windows evidence in `build/eq-*` (ignored local logs):

- `eq-core-windows.log`: **8/8 PASS, 48.06s** including prior kernel/automation,
  new preset table (240 Float32 settings), asset bytes, frontend and bridge gates.
- `EqConnectedParity.cjs`: **2 stereo 64/128 cases / 16,384 frames**, actual Engine
  capture/playback/control output versus unchanged original worklet; XY/preset
  target smoothing and actual toggle gain; final peak **7.40154114e-9 <= 1e-6**. The connected
  fixture also proves exact dry WAV through record/OD/SUB, masks/epochs, panic/
  DEL, full-queue bank admission, same-rate clock reset and zero tracked callback
  new/delete. Existing 481 kernel + 15 automation + 48 actual-browser sound cases
  remain unchanged; these different layers are not a claim of all transition PCM.
- `eq-processor-windows-{64,128}.log`: actual production processor and external
  Python CLI PASS, strict bad-batch/state rollback, save-before-audio, legacy/
  host-thread restore, full queue, MIDI-panic cancellation, DEL, track 20,
  same/changed sample rates, zero tracked callback new/delete.
- `eq-ui-final-windows.log`, `eq-ui-{64,128}.png`: actual compositor pointers and
  keys PASS on original toggles, XY/range graph, keyboard, native nonzero PCM,
  immutable dry identity, reload, flat reset, bypass and DEL; no runtime exceptions.
  Preset change handler is exercised directly; OS select popup interaction and
  listening/physical drivers are not claimed.
- Final `eq-final-ui-windows.log` adds injected-send rejection/recovery and actual
  focused-slider external updates/double-click resets at both block sizes. The
  production adapter reuses the existing pointer/key edit owner, so focus alone
  never hides native changes and an active drag/key repeat stays protected.
- `eq-core-regression-windows.log`: complete retained core realtime executable
  PASS. It preceded the final timeline-reset-only repair; the added connected
  clock regression covers that repair without repeating unrelated suites.
- `eq-engine-benchmark-windows.log`: 20 stereo playing loops/all ten bands,
  512 warm + 2048 measured calls per steady/rapid-target scenario at 48kHz.
  Worst 64/128 **590.8/947.9us** (p99 target **475.9/680.2us**), deadlines
  **1333.33/2666.67us**, 0 observed overruns. Full Engine, not JUCE/device/round-trip
  latency or a worst-case guarantee. Prior isolated-kernel timings remain below.

Failures were retained and repaired: initial fixture read a saturated old snapshot;
it now drains every block. Panic fixture initially left MIDI disabled; enable is
now explicit. JUCE's stopped message loop cannot be restarted for a second block
fixture; processor CLI tests now run as separate 64/128 processes. Pointer tests
exposed scrolling/stale coordinates and old-document reload readiness; the product
reveals the graph below the sticky header and tests await the actual new document.
No audio limit/tolerance or preserved reference was weakened. An approval-service
usage-limit failure initially prevented the UI test from executing; review later
recovered and the actual test then ran. No package/push was made.

### Browser-discovered toggle-gain repair (supersedes the prototype envelope)

The first connected prototype modeled the comments' apparent 20ms down/up gate.
An actual browser run of **unchanged `AudioGraph.rebuild` and
`AudioEngine.scheduledFade`** showed that the second `cancelScheduledValues` at
the shared endpoint removes the first ramp. At settled gains, original output
stays flat. A native duck was therefore an audible parity defect, not an owner-
approved improvement. `eq-toggle-failing-native.log` retains the failing strict
native comparison: **2.22958903e-6 at frame 1/channel 0**, over **1e-6**.

The extra native envelope is removed. The replacement regression compares the
same native captured PCM against the unchanged worklet with actual original flat
gain, preserving the **1e-6** limit. It adds exact bypass PCM and fresh-filter
re-enable checks; it does not retain the incorrect prototype dip as a product
contract. This is a justified replacement of an assumption-based test with a
stronger observed-sound oracle, not a tolerance change or deletion of a failing
behavioral case.

`tools/TestEqWebAudio.ps1 -ToggleOnly` now runs **24 actual-browser gain cases /
80,048 mono frames** at 8/22.05/32/44.1/48/96kHz and gains .25/.7/1/1.6, then
requires the actual connected native 64/128 comparison to pass. It executes only
the unchanged gain schedule; topology helpers are inert and no effect DSP is
substituted. The allowed empty test document uses built-in Gain/ConstantSource,
with no product CSP changes or product AudioContext. The initial synthetic
resource navigation was aborted by the test host; those attempts establish no
sound result. The final blank-document test passed. Non-settled interleaved fader/
mute/solo envelopes, other browser engines and full overlapping graph transitions
remain separate from this settled-gain proof.

Linux before that final sound-only repair: **9/9 CTests PASS, 54.20s**; actual
production EQ/external CLI at 64/128 and existing production MIDI/state regressions
PASS; `aion_core_tests --loop-edits` and `--meter-panic` PASS with OOM/history/
ownership protection. ASan/UBSan/leaks **2/2 PASS, 3.47s**, no diagnostics. The
exact final gain repair is awaiting its focused Linux rerun in this checkpoint.

Exact development commands (installed CMake executable may be under Visual Studio):

```powershell
cmake --build build/windows-core --config Release --target aion_eq_tests aion_eq_probe aion_eq_connected_tests aion_core_tests aion_eq_engine_benchmark
ctest --test-dir build/windows-core -C Release --output-on-failure -R '^(eq_.*contract|js_native_eq_.*|native_frontend_commands|native_bridge_lifecycle|original_ui_bytes)$'
node tests/EqConnectedParity.cjs build/windows-core/Release/aion_eq_connected_tests.exe
cmake --build build/windows --config Release --target aion_eq_processor_tests aion_mouse_host AionLooper_Standalone
build/windows/Release/aion_eq_processor_tests.exe 64
build/windows/Release/aion_eq_processor_tests.exe 128
tools/TestEqUi.ps1
tools/TestEqWebAudio.ps1 -ToggleOnly
build/windows-core/Release/aion_core_tests.exe
build/windows-core/Release/aion_eq_engine_benchmark.exe
```

Running-app CLI (1-based tracks):

```sh
python tools/aion.py eq 1 preset --preset Radio
python tools/aion.py eq 1 on
python tools/aion.py eq 1 set --values '{"p1Freq":1800,"p1Gain":4,"p1Q":0.7}'
python tools/aion.py eq 1 get
python tools/aion.py eq 1 curve
python tools/aion.py eq 1 reset
python tools/aion.py eq 1 off
```

Next gate in this snapshot: commit locally, safe WSL sync and rerun final Linux
connected/bypass/processor/sanitizer checks after the toggle-gain repair. Native EQ
MIDI Learn is still pending: the original `main.js` learns effect ranges and routes
them through the selected `EffectManager` tab; do not claim that workflow is ported
or enable disconnected mappings. Linux WebKit pointers, listening/physical devices,
long-session release checks and deferred plugin wrappers also remain separate.

## Observable acceptance, defined before integration

- Run the unchanged `EQProcessor` and `SVF` from the preserved assets. Compare
  finite stereo output at **1e-6 absolute PCM**: defaults, every band, all ten
  original EQ presets, gain/Q/frequency limits, impulses, tones/noise, silence
  tails, absent channels, rapid updates, bypass entry/re-entry and reset.
- Preserve Float32 AudioParams, double filter state/intermediate arithmetic,
  the original `<0.01 dB` bypass boundary and `1e-9` state flushing. Default EQ
  includes low/high cuts; it is not a unity bypass.
- Preserve 50 ms `smoothSetParam` automation sampled once per original 128-frame
  worklet quantum, independent of host block size. Validate actual browser
  rendering in addition to deterministic worklet execution before integration.
- Prove finite output and recovery from invalid settings/input, atomic rejection
  and zero kernel allocation/free, with 64/128 and irregular blocks on both OSes.
- Then integrate before loop volume/pan with bounded shared GUI/CLI commands,
  original controls/curve/presets, lifecycle/reset and compatible versioned state.
  Verify source ownership, record/OD/SUB capture exclusion, rapid bypass/gain,
  editor reload, multiple instances, queue rejection and real UI at 64/128.
  Passing the isolated filter does not establish these connected behaviors.

## Approved low-rate safety repair

Unchanged default worklet, 0.1 impulse followed by silence, stereo, 128-frame
quanta: first non-finite Float32 output at frame **224 at 22050 Hz**, **167 at
32000 Hz**. The default 20 kHz high-cut exceeds Nyquist and yields a negative
SVF integrator coefficient. At 8/16 kHz the same invalid setting can alias into
apparently finite but musically incorrect response. 44.1/48/96 kHz were finite
in this reproduction. This is reference behavior, not native acceptance.

The owner approved limiting effective frequencies below Nyquist while retaining
stored values on 9 November 2026. Native coefficient calculation uses
`min(requested, sampleRate * 0.5 * (1 - 1e-6))`, then retains the original positive
`g <= 1000` limit. The 44.1 kHz and higher descriptor range is unchanged. The
unchanged-worklet comparisons receive this approved effective frequency as input;
the raw failing cases remain separate regressions, not relabeled parity.

## Owner-approved graphic editing addition

The owner also requested study of open-source plugins/JSFX and direct graph
manipulation like MEqualizer: draggable points and adjustable horizontal ranges.
This is approved EQ interaction scope, not a request for new filter types,
dynamic EQ, linear-phase latency, oversampling or analog coloration.

| Studied primary source | Applicable approach | Decision |
| --- | --- | --- |
| [ZL Equalizer source](https://github.com/ZL-Audio/ZLEqualizer/blob/main/source/gui/dragger/dragger_component.cpp), [panel](https://github.com/ZL-Audio/ZLEqualizer/blob/main/source/panel/curve_panel/response_panel/dragger_panel/dragger_panel.cpp), [manual](https://zl-audio.github.io/plugins/zlequalizer/manual/) | Log-frequency mapping, drag start/end, fine/axis-constrained movement, explicit parameter gestures, sample-rate-aware limits | Use these interaction principles in the existing ASCII WebView; no JUCE GUI transplant or code copied. Source license inspected: AGPLv3; logos excluded by upstream |
| [chokehold EQ 560 JSFX](https://github.com/chkhld/jsfx/blob/main/Equalizer/eq_560.jsfx), [license](https://github.com/chkhld/jsfx/blob/main/LICENSE) | Ten-band SVF, fixed-storage state; also has gain-dependent Q, oversampling and coloration | Retain our original SVF/Q/sound instead of copying those musical changes. Source inspected, MIT license checked; no code/assets copied |
| [Andrew Simper's SVF analysis](https://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf) | Bounded state-space realization and bilinear frequency transform underlying the original worklet | Keep original recurrence/coefficients; derive response directly from that recurrence |
| [MEqualizer features](https://www.meldaproduction.com/MEqualizer/features?sort=lo) | Direct frequency/Q editing in a curve display | Interaction reference only; no proprietary code/assets used |

Planned controls: keep ten original bands and numeric sliders; log-frequency
point drag edits frequency/gain, selected peak-band horizontal edge handles edit
Q (0.1–10). Cuts have frequency-only points; shelves retain original fixed Q.
Show individual/combined response and effective low-rate limits. Native
`eqPeakRange/eqPeakQ` use bilinear-warped half-gain edges, not a misleading
constant-Hz width. `Equalizer::magnitude` evaluates its actual state-space
transfer function; tests compare it with rendered Float32 steady-tone DFT.
The graph, native processor integration and GUI/CLI commands remain pending;
no disconnected new product controls are exposed.

## Isolated kernel checkpoint

Initial Windows sound baseline: **480** unchanged-worklet cases, **2,968,872 stereo
frames**, **972 nonzero tail segments**, maximum **4.99755863e-8** under **1e-6**.
Includes all ten presets, every active band/gain/Q boundary, mono/absent input,
silence tails, bypass re-entry/reset and 64/128/37 processing subdivisions.
The final Windows and Linux corpus adds the retained full unit impulse/tail:
**481 cases / 3,034,408 stereo frames per OS**, same **4.99755863e-8** peak.
Analytical automation: **15 cases**, **184,335 stereo frames** (rerun after the
browser correction below), maximum **4.99999986e-10** on both OSes; it is
separate from actual browser evidence.

Actual WebView2: **48 comparisons**, **270,336 mono/stereo frames**, maximum
**8.57054142e-8**, **1e-6** peak / **1e-5** relative-RMS limits. Defaults/Radio/
Master Air at 8/22.05/32/44.1/48/96 kHz, and live original `smoothSetParam`
retargeting at 44.1/48/96 kHz, mono/stereo, native 64/128. This is an isolated
worklet sound oracle, not product routing or physical driver acceptance.

The actual browser initially failed smoothing at **5.62220756e-5** peak. The
original handler's `AudioParam.value` anchors from the **previous rendered
sample**, not the next sample at `currentTime`. Native retargeting and the
analytical model were corrected; the original method/worklet, PCM limits and
failing retarget sequence remain intact. Example 44.1 kHz: target +12 dB at
frame 256, retarget at 1024 anchors **3.5255067348480225**, matching age 767,
not age 768. Same-sample repeated edits retain their previous anchor.

Product CSP correctly blocked the initial worklet module load. A debugger-only
CSP bypass did not unblock the worklet either. The sound harness now serves
exactly one test fixture document via CDP Fetch in its owned WebView2 instance.
No product CSP/source changes, original bootstrap, network module fetching or
product AudioContext. The helper owns/guards port 9236 and closes its own app.

Curve acceptance: **48 frequency points** from actual Float32 steady-tone output
at 8/22.05/48/96 kHz, flat/all-active settings, maximum amplitude-response error
**2.01394e-9** under **1e-6** on both OSes. **60 bandwidth ranges / 120 inverse
edge checks** cover Q 0.1/0.707/1/10, 20/1000/22000 Hz and five sample rates,
including the approved effective-frequency limit. Filter construction, validation,
configuration, reset, smoothing, rendering and curve/handle math show zero
`new/delete` in the scoped allocation guard. This is not a measured production
callback or a full-bank performance claim.
Added **240 half-gain response checks**, boost/cut at both edges, maximum
**6.7677e-12** under **1e-6** on both OSes, so a self-consistent but incorrect
range/inverse pair cannot pass alone. Finite Float32 overflow is contained
without contaminating the other channel. Both expanded unit cases also pass
Linux ASan/UBSan/leaks. Single-owner instances are explicit: UI curve calculation
must use its own instance/settings snapshot, never read a live audio filter.

One newly introduced curve test was corrected rather than changing sound:
an ideal state-space transfer function was incorrectly asserted equal to a
unit-impulse spectrum at **1e-6**. Original tiny-state flushing makes the system
slightly nonlinear: 8 kHz, 65536-sample impulse DFT, bin 131 differed by
**1.13081e-6**. No PCM or curve tolerance was loosened. The exact full impulse/
tail is retained as an unchanged-original **1e-6 PCM** regression, while plotted
linear response is checked against six simultaneous low-level steady tones
after 65536 warm samples, at the same **1e-6** amplitude-response limit. This
retains observable flushing and adds a valid steady-state curve measurement;
it does not claim an ideal response predicts the flush floor at arbitrary levels.

Reproduction (existing caches; no packages):

```powershell
cmake --build build/windows-core --config Release --target aion_eq_tests aion_eq_probe --parallel 2
./build/windows-core/Release/aion_eq_tests.exe
node tests/EqParity.cjs build/windows-core/Release/aion_eq_probe.exe
node tests/EqAutomationParity.cjs build/windows-core/Release/aion_eq_probe.exe
./tools/TestEqWebAudio.ps1
```

Linux commands after safe committed-snapshot sync:

```sh
# Regenerate once when newly adding CMake targets; retain existing cache options.
cmake -S . -B build/linux
cmake --build build/linux --target aion_eq_tests aion_eq_probe -j2
ctest --test-dir build/linux -R '^(eq_realtime_contract|js_native_eq_kernel|js_native_eq_automation)$' --no-tests=error --output-on-failure
cmake -S . -B build/overlap-asan
cmake --build build/overlap-asan --target aion_eq_tests aion_eq_probe -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/overlap-asan -R '^(eq_realtime_contract|js_native_eq_automation)$' --no-tests=error --output-on-failure
```

Windows **4/4** focused CTests: **42.59s**. Linux **4/4**: **42.28s** (the identical
set was inadvertently repeated after its first result was collected late;
**43.74s**, not extra coverage). Linux existing Makefile sanitizer cache required
one CMake regeneration for the new targets; its initial missing-target error was
setup, not a DSP failure. ASan/UBSan/leak subset **2/2**, **6.83s**, no diagnostics.
After adding overflow/half-gain checks, only the affected unit target was rebuilt
and rerun on Windows and Linux Release plus Linux sanitizers; PASS. Existing
sound/browser suites were reused because DSP did not change.

Warmed Release measurement, identical `tests/EqBenchmark.cpp`, separate OS
caches and no concurrent task test/build job: 48 kHz, **20 stereo kernels**, all
ten bands active, **512 warm + 2048 measured callbacks per scenario**, steady and
all-parameter targets every 16 callbacks. No production routing/mixing, driver,
other engines, worst-case/long-session or round-trip-latency claim.

| Block | Windows worst observed kernel time | Linux worst observed kernel time | Buffer deadline |
| --- | --- | --- | --- |
| 64 | 318.5 us | 369.077 us | 1333.33 us |
| 128 | 534.5 us | 2254.67 us | 2666.67 us |

No observed overrun in these scoped runs. Linux's 128-frame steady run included
the 2254.67 us outlier (p99 503.699 us); do not hide it or infer an OS ranking.
Build/run the manual utility with `--target aion_eq_benchmark`, then
`build/windows-core/Release/aion_eq_benchmark.exe` or
`build/linux/aion_eq_benchmark`. It is intentionally not a CTest timing gate.

Implementation **78fa041**, strengthened acceptance **fc6edc5**. Logs in each
checkout: `build/eq-{final,build,benchmark}-OS.log`, Windows browser
`build/eq-webaudio-windows.log`, Linux `build/eq-sanitizer-linux.log` and final
expanded units `build/eq-unit-final{-sanitizer}-linux.log`.

Next is connected acceptance: shared bounded GUI/CLI commands, versioned state,
original panel/presets plus approved graph points/ranges. Align the component's
128-frame clock with the connected graph's render origin, preserve graph reuse/
teardown and bypass fades, and cover record/OD/SUB exclusion, source/clear/reset,
queue rejection and editor lifecycle. Until then the product effect stays
disabled. Reuse prior accepted loop/source regressions: this checkpoint does not
yet change the production callback, UI, IDs or state. Linux WebKit interaction,
physical drivers/hardware, listening, deferred wrappers and release remain open.
