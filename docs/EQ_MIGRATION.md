# Original Q/eQ migration

Active slice: ten-band per-loop eQ, before the other unported FX/SAVE controls.
The filter kernel was a prerequisite. Connected Windows/Linux software acceptance
is recorded below; live controls are owner-approved and EQ MIDI Learn passes
the documented Windows/Linux software checks. Do not infer
full-chain, wet SAVE, hardware, deferred-wrapper or release acceptance.

## Connected implementation and current verification — 9 November 2026

### Selected-tab EQ MIDI Learn — current increment

Acceptance: original continuous CC ranges and selected-tab routing for all 24
parameters; first learn packet consumed, subsequent same-block packets applied
at their sample offsets; no held transport action; shared GUI/CLI access;
conflict/cancellation, editor-free operation and backward-compatible state.
The original context-menu/handler/setter remain unchanged in preserved assets.
Native EQ uses fixed IDs **93..116**, appended after the existing 93 targets.
`MidiSettings` version **4** adds these CC-only bindings and `eqTrack` (0..19).
Versions 1/2/3 retain their 33/73/93 mappings; new fields default unassigned/tab 1.
Malformed new state is rejected before any APVTS/MIDI/EQ mutation. Tab selection
uses the existing bounded MIDI configuration queue, revision-fenced in the UI;
CLI `midi eqTrack` reaches that same message owner. An absent editor retains the
last accepted tab. UI/serialization stay off audio; the existing 256-event input
bound, conflict/provisional learn, cancellation and timeout machinery is reused.

New reference gate `js_native_eq_midi`: **51,072** unchanged handler/setter cases,
all 24 original widget ranges, all 16 channels, tabs 1/20, all 128 values plus
repeated/nonmonotonic/zero extremes. Probe output is compared to Float32 reference
values exactly after Float32 conversion (the probe prints nine significant
digits for round-trip precision). Production Engine tests compare
actual stereo PCM with sample-timed GUI controls at **1e-6**, including learn at
sample 17/following CC at 18 before message acknowledgment, all values/channels,
selected-tab isolation and channel filtering. Callback new/delete remains tracked.

Windows actual processor/external CLI and real WebView2 pointer/key tests pass
at 64/128: learning, focused DOM slider updates, tab routing, conflicts, Escape,
Shift-clear, save/restore and version-3 migration. Existing GUI/graph/reset/CANCEL/
reload and dry audio regressions remain. The first new oracle invocation used a
relative fixture path under CTest; it now resolves from its own directory. A
processor test initially expected the audio binding snapshot in the capture block;
it now separately asserts immediate rendered gain and the following configuration
acknowledgment, retaining both requirements. The legacy-schema test still checks
all old mappings; only the expected writer version and unknown-future version
moved from 3/4 to 4/5. No assertion was deleted or tolerance weakened.

On committed production snapshot **6766d40**, Windows **6/6 CTests PASS (16.47s)**
and Linux **6/6 PASS (13.72s)**; both also pass all retained `aion_core_tests --midi`
and production `aion_midi_processor_tests` regressions. Actual EQ processor/external
CLI pass at 64/128 on both OSes. Connected CC-vs-GUI stereo peak error is **0** at
both block sizes (limit 1e-6). Linux ASan/UBSan/leaks **1/1 PASS (11.35s)** with
no diagnostics. Logs: `build/eq-midi-core-final-windows.log`, `eq-midi-linux.log`,
`eq-midi-processor-windows-{64,128}.log`, `eq-midi-asan-linux.log` and retained
core/processor logs. The reference parameter gate was subsequently strengthened
to exact Float32 equality; both OSes must run that final script after sync.

Final actual Windows UI checks also replace the document with an active EQ learn:
learning cancels, the accepted mapping and selected tab 20 survive, and its actual
slider shows the restored value. The first test clicked before the reconstructed
20-row page layout settled (y=819 outside the 800px viewport); it now uses the
same grid-ready/scroll setup as the earlier reload test. The original hit and
state assertions remain. Final 64/128 run passes (`eq-midi-ui-reload-windows.log`).

Warmed actual Engine measurement, run sequentially after other builds/tests:
20 playing stereo loops / ten EQ bands / 48kHz / 256 routed EQ CCs per block,
512 warm + 2048 measured callbacks per scenario:

| OS | Block | Dense-CC p99 / max (us) | Callback deadline (us) | Observed overruns |
| --- | --- | --- | --- | --- |
| Windows | 64 | 455.7 / 508.2 | 1333.33 | 0/2048 |
| Windows | 128 | 782.6 / 1068.2 | 2666.67 | 0/2048 |
| Linux | 64 | 441.429 / 978.429 | 1333.33 | 0/2048 |
| Linux | 128 | 742.686 / 1493.24 | 2666.67 | 0/2048 |

Steady/GUI-target scenarios also had zero observed overruns in this run. Full
results are in `build/eq-midi-benchmark-{windows,linux}.log`. This measures native
Engine work, not JUCE wrapper/device/round-trip latency or a worst-case guarantee.
No physical pedal, Linux WebKit, plugin or listening acceptance is implied.
CC120/123 retain native panic reservation; this is not arbitrary original effect
mapping compatibility. Reproduce with the existing
core/processor targets, `js_native_eq_midi`, `eq_connected_contract`,
`js_native_eq_connected`, `native_frontend_commands`, `js_native_midi`, then
`tools/TestEqUi.ps1`; use the sanitizer cache for connected audio as above.
Run `aion_eq_engine_benchmark` separately after builds/tests for timing.

### Earlier connected GUI/CLI checkpoint (before EQ MIDI)

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
  These are pre-toggle-repair prototype measurements, retained as historical
  evidence; current final timings are below.

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
python tools/aion.py eq 1 width --band 1 --edge 2500
python tools/aion.py eq 1 reset
python tools/aion.py eq 1 off
```

Final Linux toggle/CANCEL/bandwidth-CLI snapshot **b0ce92d** builds Standalone and
passes **3/3 focused CTests (1.12s)**, actual production/external CLI at **64/128**,
and **1/1 connected ASan/UBSan/leak test (2.03s)**, no diagnostics. Logs:
`eq-cancel-{build,tests}-linux.log`, `eq-cancel-processor-linux-{64,128}.log` and
`eq-cancel-sanitizer-{build,tests}.log`. The earlier post-toggle-only rerun also
passed 2/2 CTests (0.92s), both processor sizes and 1/1 sanitizer (2.04s); it was
followed by the separate CANCEL regression rather than an unchanged suite repeat.
Source/header compilation was verified against the committed native WSL mirror.

Native EQ
At this earlier checkpoint MIDI Learn was pending: the original `main.js` learns effect ranges and routes
them through the selected `EffectManager` tab; do not claim that workflow is ported
or enable disconnected mappings. Linux WebKit pointers, listening/physical devices,
long-session release checks and deferred plugin wrappers also remain separate.

Final warmed Engine benchmark, run sequentially after builds/tests completed:

| OS | Block | Steady p99 / max (us) | Target p99 / max (us) | Deadline (us) | Observed overruns |
| --- | --- | --- | --- | --- | --- |
| Windows | 64 | 360.3 / 437.8 | 359.2 / 447 | 1333.33 | 0/2048 per scenario |
| Windows | 128 | 660.5 / 688.9 | 669.7 / 734.6 | 2666.67 | 0/2048 per scenario |
| Linux | 64 | 388.628 / 513.378 | 393.079 / 507.929 | 1333.33 | 0/2048 per scenario |
| Linux | 128 | 690.887 / 826.279 | 700.045 / 777.795 | 2666.67 | 0/2048 per scenario |

Logs `build/eq-engine-final-{windows,linux}.log`. Same 20 playing stereo loops /
ten bands / 48kHz / 512 warm + 2048 measured calls per scenario. No contention
benchmark, worst-case guarantee, JUCE/device or round-trip latency claim. Prior
prototype Linux maxima **605.891/1941.75us** and Windows **590.8/947.9us** remain
recorded; the final run is after the sound repair, not a claim of OS speedup.

Reproducible native-WSL follow-up commands (existing pinned Linux workspace/cache):

```sh
cmake --build build/linux --parallel 3 --target aion_eq_connected_tests aion_eq_processor_tests aion_eq_engine_benchmark AionLooper_Standalone
ctest --test-dir build/linux --output-on-failure -R '^(eq_connected_contract|js_native_eq_connected|js_native_eq_presets)$'
build/linux/aion_eq_processor_tests 64
build/linux/aion_eq_processor_tests 128
cmake --build build/overlap-asan --parallel 2 --target aion_eq_connected_tests
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --output-on-failure -R '^eq_connected_contract$'
build/linux/aion_eq_engine_benchmark
```

The existing sanitizer cache is Debug, plugin off, with
`-fsanitize=address,undefined -fno-omit-frame-pointer`; configure only when changing
configuration or adding targets. Safe committed-snapshot sync remains exactly the
helper in DEVELOPMENT_WORKFLOW.md. No distribution was created or pushed.

### Armed CANCEL repair and approved live-control behavior

The original `LoopManager.stopRecording` armed branch empties the take and drops
its graph while retaining effect parameters, enable flags and preset. Native
`finish` (the actual main-button CANCEL path), `stop` and panic previously reached
`clear`, erasing audio-side EQ while the message model could retain its old value.
`eq-cancel-failing-windows.log` records the actual-audio assertion before repair.
They now share `cancelArmed`, which preserves EQ fields while clearing runtime
capture/filter ownership. Explicit DEL still resets EQ. Message reconciliation
protects pending accepted intent, otherwise adopts actual audio values even after
an internal cleanup with no DEL epoch. The existing MIDI schema and other controls
are unchanged.

Regression evidence: **22 unchanged original stopRecording cases**, native core
finish/stop/panic/DEL and actual production `inspectAudioThread` fields; Windows
64/128 production/external CLI and actual static-arm/main-CANCEL pointers PASS.
The first fixture restored Off rather than the saved default Dynamic sync mode;
it now restores the original mode before byte comparisons. The first pointer test
revealed that main CANCEL uses finish, beyond the initially tested stop alias;
both paths are now protected. A local test-variable declaration was also repaired;
no assertion or tolerance was relaxed. CLI width/edge coverage uses native
`eqPeakQ` and checks its returned edge within .01Hz for Float32 Q storage.

**Owner approval, 9 November:** EQ controls respond live during OD/SUB/MULT and
scheduled STOP. The owner explicitly selected this separately from persistent
filter tails. Original `EffectManager.update` and `triggerRebuild` only update a
loop graph in `playing`, leaving stored values ahead of audible settings in these
other states. Native live response is an approved musical/workflow difference;
do not label it unchanged-JS parity. No further DSP/UI change was needed.

Observable acceptance for this choice: edits, preset changes and toggles must
affect actual nonzero playback in each state; retain global-128-frame/50ms target
smoothing, exact bypass, untouched stored capture/feedback/history, and the
existing scheduled STOP. `EqConnectedTests.cpp` now adds eight paired production
Engine scenarios (four states at 64/128), separately from the unchanged-JS JSON
cases. An independent `SmoothedEqualizer` checks enabled PCM at **1e-6**;
a frozen-target comparison proves that edits actually change the sound (peak
difference >.001), not merely metadata. Each case edits frequency/gain, bypasses,
re-enables and selects Telephone while the special state remains active.
State/position/length/history/source revision/deadline agree with the dry twin
each block. Recordings finish with the expected length and single history entry;
all resulting dry WAV bytes match exactly. Scheduled STOP playback also matches
the oracle through its original deadline, then becomes silent after its existing
5ms tail. Callback new/delete guards cover every rendered block.

Focused Windows commands (existing Release cache):

```powershell
cmake --build build/windows-core --config Release --target aion_eq_connected_tests
ctest --test-dir build/windows-core -C Release --output-on-failure --no-tests=error -R '^(eq_connected_contract|js_native_eq_connected)$'
```

Linux uses the committed-snapshot helper before building the same target in
`build/linux`; use the same CTest expression without `-C Release`. Rebuild that
target in `build/overlap-asan` and run `eq_connected_contract` with
`ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1` as above.
Windows: **2/2 focused CTests PASS, 6.15s** (`build/eq-live-states-windows.log`).
The eight new scenarios pass with peak enabled-PCM errors **3.73e-9..7.46e-9**
and audible edit differences between **0.00432 and 0.00488**.
Committed snapshot **1f80046** was synced after the helper completed, with the
Linux mirror clean. Linux **2/2 PASS, 1.82s**, with the same eight PCM measurements
(`build/eq-live-states-linux.log`); ASan/UBSan/leak connected coverage **1/1 PASS,
10.66s**, no diagnostics (`build/eq-live-states-sanitizer.log`). Only the affected
test executable was rebuilt; no wrappers, packages or dependency changes.
Prior actual GUI/CLI evidence remains applicable: the shared command paths have
no special-state restriction and were not changed here. This run adds no Linux
WebKit, physical device or listening evidence. EQ MIDI Learn was the next slice
at that checkpoint and is covered above; full-chain/release acceptance remain open.

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
At that isolated-kernel checkpoint, graph/native integration and GUI/CLI were pending;
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

That historical kernel-only checkpoint did not establish connected acceptance.
Current GUI/CLI/state/graph, render-origin, bypass, capture/lifetime and interaction
evidence is above, with remaining owner/controller/platform checks explicit.
