# Migration status

| Capability | Foundation status | Release acceptance |
| --- | --- | --- |
| Original runtime assets | 16 files preserved with SHA256 | Automated byte identity checks |
| Native build formats | CMake targets/platform guards configured | Compile/load/validate each OS+format |
| Windows VST3 host interface | Actual compiled VST3 passes pinned pluginval 1.0.4 level 9: scan/load, GUI, parameter/state/thread/fuzz and rate/block matrix; opt-in reproducible CTest gate | Vendor build omits allocation tests; external Steinberg validator, active recorded-loop/MIDI workflows, actual DAWs/hardware and other formats/OS remain separate |
| Windows CLAP host interface | Actual module passes all supported official validator 0.4.1 tests after state-restore host-rescan repair: 33 passes, 10 explicit skips, one retained JUCE void-load warning; opt-in CTest gate | GUI/allocation, actual DAW/MIDI/recorded-loop sessions, other OSes and unsupported extensions remain separate |
| Linux production MIDI/audio processor | Same production JUCE processor test as Windows passes in Debug at 64/128 with no ALSA sequencer device; strict assertion/leak gate; host/synthetic instances skip OS MIDI watching, actual Standalone retains it | Tracked callback new/delete and synthetic routing only; physical controllers/hotplug, GTK UI and driver latency remain separate |
| Linux VST3 host interface | Actual Debug VST3 passes pinned official pluginval 1.0.4 level 9, including editor lifecycle, automation, state, thread/fuzz and 15 rate/buffer combinations; optional reproducible CTest | HTML interaction, actual DAW/device/recorded-loop sessions, Release latency/CPU, omitted allocation and external Steinberg validator remain separate |
| Linux LV2 host/audio interface | Actual Debug bundle passes official pluginval 1.0.4 level 9 editor/automation/state/thread/fuzz matrix; public LV2 ABI patch master/monitor stereo probe at 44.1/48/96 kHz and 64/128, with module/RDF identity checks | Settled monitor PCM only; full HTML/loop/MIDI/project DAW workflows, physical drivers, gain-ramp waveform, allocation and Release latency remain separate |
| Linux native build and CLAP interface | Ubuntu 24.04.3 x86_64 Debug Standalone/VST3/CLAP/LV2 rebuilt at 0.4.3; actual CLAP validator: 34 passes, nine unsupported-extension skips, one retained JUCE void-load warning; Unix RTLD_NOW and all three state restores pass | GTK/WebKit UI, real JACK/ALSA/controllers/DAWs, LV2/VST3 host workflows and Release latency/CPU acceptance remain separate |
| Original UI runtime | Original loop cards, module placement, tabs and keyboard gestures; legacy audio JS blocked | Full view adapter and original interaction tests |
| IPC/APVTS | Coherent snapshots, generation/expiry validation, per-document reply correlation, loop commands, master/monitor/auto-play APVTS and persisted MIDI bindings | Reused-ID/obsolete-reply, native echo and real WebView2 reload gates; broader automation/state stress remains |
| Audio owner | 20-slot native looper, bounded pool, smoothed gain/mute, finite protection | Hardware/DAW audio, 64/128 stress and allocation instrumentation |
| Host transport | Audio-owned, block-stamped BPM/PPQ/sample/seconds, meter/bar/cycle and play/record observations; validated optional fields and coherent native IPC | Fake-host/portable 64/128 and nonzero PCM-isolation gates; actual DAW/wrapper validation, arbitration and sample-accurate host sync scheduling remain pending |
| Loop recording/play/stop/overdub/substitute | Single capture owner and worker finalization; manual and static/dynamic zero-latency committed PCM comparisons pass | Hardware capture, latency compensation and long-session tests |
| Internal static/dynamic sync and main gestures | Sample-clock arm/cancel, fixed capture, dynamic bar/second-tap finish, queued play/stop, visual count-in, original grid controls; unified mouse/keyboard | Native WebView2 input, reference timing/PCM and 64/128 gates; loop-source/DAW/MIDI clock and physical latency pending |
| Loop pan/varispeed | Native reference pan law, manual 0.25x-2x and reference sync-ratio rates with smoothing; original synchronized Speed thresholds and focused thumb snapping; half/reset | Antialiasing, browser-rendered audio and live/hardware tempo changes |
| Loop reverse | Original Rvers toggle; requested direction latches on play/retrigger; independent reversed source retained across NORM | Reference source PCM and state tests; hardware/browser rendering remain separate |
| Loop normalization | Immutable worker NORM with shared stereo peak; unchanged-JS PCM/threshold tests | Hardware/listening, full browser playback and long-session stress |
| Loop Undo/Redo | Per-loop five-snapshot history, global order and original controls/shortcuts; immutable handle swaps | Long-session/memory/hardware stress; restoration temporarily disabled on a track being captured/finalized |
| Loop Multiply | Original MULT/shortcut captures and appends >=128 frames, or doubles stopped/no-input loops; immutable local history and bounded growth | JS committed PCM/state, worker lifecycle and 64/128 checks; full browser/driver/sync acceptance pending |
| Loop names | Original name fields, C++ message-thread session labels on all 20 tracks; Unicode, Enter-to-blur, editor reload and acknowledged deletion | Temporary session metadata; project persistence/export remains unported; 4096 UTF-8 byte IPC limit |
| Continuous metronome | Native port of original worklet, sample-grid accents/decay, existing button/N/volume/flash, independent monitor output excluded from loop capture and MASTER meter | Unchanged-worklet and connected Engine PCM, 64/128 and native UI gates; DAW/MIDI clock remains separate |
| Static SOUND / VISUAL count-in | Original checkboxes; audio-owned 440/880 Hz triangle cues, 100ms sound/150ms flash, independent MASTER bypass and capture exclusion; cancellation and latched timing | Unchanged scheduler + original arm/grid, analytical waveform and actual WebView2 WebAudio PCM gates; physical listening/driver latency remains separate |
| Loop Start slider | Original slider/Q/nudge control/display; audio-owned sixteenth rounding, full-precision phase metadata, release schedules source at +50ms, reverse/phase latches and bounded FIFO; OFF/static/dynamic source offsets | Unchanged setter/Q/restart/AudioGraph + committed PCM, 64/128 and native slider drag/Q/nudge gates; multi-cycle tiny-clip wrapping repaired; exact browser transition envelopes remain separate |
| Loop RTRG / `/` | Original user-level Start reset/sixteenth snap, effective-rate master phase, reverse/solo source rules, playing +5ms versus immediate stopped play; bounded cancellation/ordering | Unchanged Loop.retrigger and committed source PCM, exact 64/128 and native mouse checks; browser transition envelopes and DAW/physical timing remain separate |
| Loop STUT | Original hold-to-fragment/release-to-full-loop button and independent original playhead clock; native phase/BPM region, fractional/reverse interpolation, queue-independent cancellation and editor lifecycle cleanup | Unchanged JS source PCM/state, actual WebAudio 1x ordinary/tail fragments at zero/nonzero clock origins, native 64/128 mouse gates; full resampling/envelopes and driver latency remain separate |
| Recorded-loop waveform | Worker-cached original 720-bin stereo peaks; bounded generation-tagged delivery, identity-aware UI cache; original colors/bars/playhead/HiDPI | Unchanged JS peak and canvas comparisons, native pixel/PCM, history/reset/backpressure gates; DYNAMIC live trace covered separately; additional interactions remain separate |
| DYNAMIC recording live trace | Native 512-sample input window; capture/generation tags, bounded queue, original red trace/fill, four-bar markers and sync-aware playhead | Actual WebAudio downmix, chronological Engine windows, original canvas/progress and native 64/128 gates; browser scheduling/physical latency and other live visualizers remain separate |
| Loop WAV export | Original waveform click, raw stored stereo PCM16, original filename; bounded worker snapshot and native save picker | Exact original encoder/conditioned-PCM bytes, pin/DEL/reset/OOM, host-picker mouse/file checks at 64/128; OS picker and Apple/Linux UI validation tracked separately |
| Loop Merge | Original MERGE selector/GO, stopped clone into empty destination or circular raw-PCM mix at feedback 1; preserved destination phase/length, local/global Undo/Redo and original optional source-clear confirmation | Unchanged user-level JS, exact stereo WAV and <=1e-6 playback; worker pins/OOM/DEL/full-pool, dynamic metadata, native 64/128 PCM/mouse/confirmation and stale-source guards; full browser transition envelopes/physical drivers remain separate |
| Other loop editing/import/export | Remaining editing, import and wet/mixed exports not ported | Reference PCM/state/UI fixtures |
| Drone voices/sampler/tracker/FX | Not ported | Algorithm/timing/CPU and saved-state fixtures |
| Direct loop-action MIDI Learn | Requested native right-click learn/Shift-right-click clear on original twenty primary loop buttons, main header/state surfaces and LIVE headers; shares existing mapping fields/targets | Actual context-menu/status matrices and 64/128 synthetic MidiBuffer stop/play audio, handoff/cancellation/persistence/clip gates; other buttons and physical direct-learn acceptance remain separate |
| MIDI looper control/learn | Native Note/CC/Program Change actions and independent per-loop MUTE; malformed CC rejected before panic, original mapping focus/blur/Tab, active loop-note lost-release recovery after >10s, immediate cancelled-Note/full-pool recovery, OD/SUB/SUS, standalone routing and persisted bindings; works without editor | Original default-note oracle, processor/allocation and native 64/128 tests pass; physical AudioBox USB PC 0 learn/record/play/stop confirmed after triple-PC burst repair; broader hardware/reconnect tests pending |
| Independent loop MUTE MIDI Learn | Original main MUTE/UNM and LIVE M share discrete Note/CC/PC targets 73..92; latched action independent of OD/SUB/SUS, original UI retained; version 3 loads legacy 33-/73-slot mappings | 240 real-PCM/immutable-WAV cases, offset/capture/held-owner checks, unchanged toggleMute action oracle, native saved-state/no-editor and actual 64/128 pointer/save/muted-reload checks; REAPER 1x VST3/CLAP Note/CC plus per-channel PC0/PC127 record/MUTE/recall pass after bounded wrapper repair; supplied-offset portable tests pass, but REAPER PC timing is quantized; full-speed offline finalization passes the documented REAPER VST3/CLAP gate; physical controllers remain pending |
| MIDI loop volume/pan expression | Original slider right-click learn, zero/repeated CC values, native sample timing, pan integer steps, saved mappings and legacy state migration | Original handler/setter and exact connected stereo PCM, no-editor processor and 64/128 mouse gates; physical AudioBox USB channel-1 CC27 volume learn/response/display confirmed; calibration/listening/other hardware remain separate |
| LIVE loop mixer controls | Twenty original loop volume/M/S strips share native gain, mute and solo; no new DSP or parameters | Original complete-renderer and wheel oracles plus native 64/128 audio/UI checks; LIVE faders share main-view native CC learn/clear/save bindings; RMS/held-peak metering and master/input/drone/sampler strips remain pending |
| LIVE performance loop cards | Twenty original loop cards, native header short actions and shared Rvers/state/name display; original panel state priority and tab-entry layout | 400 unchanged markup / 1,600 updater / 121 panel-state cases plus real native 64/128 audio/UI gate; live master mix, drones/samplers and other effects remain unported |
| Global performance status | Original master-header SUB > SUS > OD precedence, independent active capture colours, queued flag and YES (Loop N)/NO recording label; initial master label retained | Full unchanged updateStatus oracle across 7,920 combinations; native MIDI mode-off-during-overdub/header/PCM/history gate at 64/128; unported master recorder, keyboard SUS and other engines remain separate |
| Loop status presentation | Original pending STOP `[.]`, stopped `[S]`, state labels and mute/duration spacing; native asynchronous `[..]` / SAVING retained | Complete unchanged display-updater oracle: 6,400 state/mute/duration/speed cases; actual 64/128 queued-stop/audio preservation gate; no DSP or transport change |
| Focused loop slider resets | Original Speed/Feedback double-click moves the focused range to 1.0x/80% while the existing handler dispatches its native command | Unchanged original reset methods across 560 cases/all 20 loops; trusted WebView2 DOM/native/recorded-PCM checks at 64/128; Apple/Linux physical browser interaction remains separate |
| Slider/number wheel protection | Original capture-phase non-passive wheel guard: non-Shift scrolling prevents changes and blurs range/number inputs; Shift and text/other targets pass through | Unchanged original listener matrix and trusted WebView2 64/128 focus/value/native-parameter checks; Apple/Linux browser/device behavior remains separate |
| Numeric keypad loop keys | Original numeric character mapping to loops 1-10 with or without Shift; NumLock navigation ignored, physical-key release ownership | Unchanged original dispatcher matrix and native 64/128 audio/gesture gates; physical OS/layout matrix remains separate |
| Keyboard missed-release recovery | Original fresh-key recovery strictly after ten seconds, no automatic expiry; stale per-key callbacks/queued cuts invalidated | Unchanged original dispatcher/press boundaries for all 20 loops, scope/repeat/focus guards and real native 64/128 recording/playback; custom mappings/SUS remain separate |
| Keyboard DEL ALL / `X` | Original default physical-key fallback, confirmation and shared native button command; accepted clear cancels held/queued performance work | Unchanged original keyboard oracle and real WebView2 dialog/PCM/gesture checks at 64/128; custom key mappings and master recording remain separate |
| Keyboard STOP ALL / Space repeat | Original default Space/Shift-Space fallback handles every keydown, including OS repeats; modifiers and text-editing guards retained | 130 unchanged original dispatcher cases; trusted WebView2 repeat interleaved with synthetic MIDI playback at 64/128, silence/nonzero PCM and clip/history preservation; physical keyboard/OS and DAW shortcut-focus routing remain separate |
| Keyboard SUS / custom keyboard mappings | Original `*` SUS shortcut and editable/persisted keyboard mappings remain unported; native MIDI SUS is implemented separately | Original keyboard/held-gesture/audio parity required before enabling this increment |
| Other MIDI expression/synth/HID | Other continuous targets, remaining button Learn targets, synth keyboard/pitch/modulation and gamepad not ported | Controller matrix, voice/parameter behavior and original reference fixtures |
| Zero-sample MIDI callbacks | Offset-zero control events execute at the unchanged audio frame; held capture/learn preserved, NoteOff/PC/CC/Stop dispatched | Core exact stereo WAV and production zero-/two-channel MidiBuffer checks between 64/128 blocks; specific DAW/wrapper delivery remains unverified |
| MIDI clock I/O | Designed, not implemented | PLL jitter/timeout/start/stop/SPP/host-loop tests |
| `.alp` / native projects | Not ported | Existing project fixtures, no missing features/data |
| Windows ASIO | Windows binaries compile with bundled SDK; not hardware-tested | TOPPING/other driver duplex at 64/128 |
| macOS/iOS/AU/AUv3 | Conditional targets/presets only | Xcode, devices, permissions, host lifecycle and signing |
| Commercial distribution | Not ready | Licensing, signing, installers, validation and complete parity |

This phase makes no production-readiness or completed feature-parity claim.

See `REFERENCE_AUDIT.md`: the latest worker integration passes the committed-PCM diagnostic; historical direct-write failures are retained for context. These are separate acceptance gates.

Real DAW baseline (23 October 2026): REAPER 7.82 loads the actual Windows VST3
and CLAP builds in isolated profiles; seven target stereo renders per format
match the settled reference PCM, including deliberate parameter mutation/restore
and RPP reopen. Ardour 8.4's libardour/Lua session engine loads LV2 with all twelve
APVTS numeric property-automation controls, stereo gain/mute/recovery and saved
session recall at 48kHz/64/128. Ardour checks peaks/ratios, not per-sample PCM or
GTK editor interaction. Both retain synthetic devices/audio and module identity/
hash evidence. This adds no synth/FX feature and does not establish host-follow
scheduling, MIDI clock, recorded-loop persistence, physical drivers or Apple
acceptance. Linux CLAP DAW validation remains separate because Ardour 8.4 does
not host CLAP. See VALIDATION.md for exact evidence.


Offline-only completion follows the explicit user-approved exception on
24 October 2026. It synchronizes the native worker with offline virtual time;
this is native host behavior rather than unchanged browser scheduling parity.
The original UI/assets and DSP kernels remain unchanged. Complete offline PCM
must be identical across 1/64/128 block partitions, including simultaneous
capture handoffs and existing immutable editing/history operations. Keep live
unchanged-JS and real-PCM gates independent. REAPER's VST3 PC timestamp
quantization, other hosts/Apple platforms and recorded-loop project persistence
remain separate limits; a passing offline render is not physical audio evidence.


Default keyboard RTRG repeat parity (24 October 2026): '/' and NumpadDivide now
retain original repeat dispatch before loop-key debounce. The 1,300 unchanged
original-dispatcher cases retain modifier/text/control/Shift guards; actual
WebView2 key repeat checks cover playing/stopped loops, Start reset, nonzero PCM
and clip/history at 64/128. Existing native selected-loop routing is retained;
custom keymaps, keyboard SUS, broader FX-tab selection and physical OS/layout
acceptance are not established by this increment.


Default keyboard Multiply repeats (24 October 2026): original backslash repeat
dispatch now precedes ordinary loop-key debounce. Complete original-dispatcher
comparisons retain text/modifier/character/Shift guards. Native 64/128 trusted
repeat checks require real appended stereo audio, unchanged prefix, exact stopped
doubling and Undo/Redo WAV bytes. This retains existing native Multiply DSP and
input fallback; it does not establish custom mapping, keyboard SUS, broader
selected-FX, physical keyboard or DAW shortcut-focus parity.


WAV lifecycle repair (24 October 2026): native navigation/destruction cancellation
replaces redundant frontend bootstrap/pagehide RPCs. A real-controller delayed-
cleanup regression reproduces cancellation of a newer NORM picker before the
repair and passes at 64/128 afterwards, retaining original WAV/transport gates.
This repairs native IPC ownership without changing original export semantics,
audio, UI or explicit Cancel. OS picker interaction and Apple/Linux native UI
acceptance remain separate from the synthetic Windows picker test.


Meter calculation component (24 October 2026): native MeterFollower matches
the complete unchanged readMeter method on 1,000 supplied-window observations,
including RMS release, peak/clip holds, clock/history/source resets and nonfinite
input. Fixed storage and no tracked new/delete are tested. It is not connected
to Engine/IPC/UI: original meter rendering, real native analyser windows/downmix
and audible latency remain pending. Existing product meter placeholders and
the LIVE RMS/held-peak migration status above remain unchanged.


Raw native meter PCM subset (24 October 2026): actual native post-loop-gain/pan
collection now runs in Engine with 2048-frame chronological windows. Browser
OfflineAudioContext comparisons pass 40 cases/81,920 samples with zero error,
including mono, signed stereo/antiphase, prefix and wrap. Core fixtures cover
actual pan/gain audio and MASTER/monitor/click isolation at 64/128. This closes
the raw collection/downmix subset only. MeterFollower is still exercised with
collected windows in tests, not production; source/analyser lifecycle identities,
audible-time transfer, IPC and original visible rendering remain pending. Raw
native fade tails are not a claim of original disconnected-analyser behavior.
Original UI, bindings and product meter placeholders remain unchanged.


Raw PCM transfer subset (25 October 2026): Engine can now pass requested windows
to one message owner with bounded SPSC copies, end-frame/rate/reset/clear/clip
metadata, queue-full recovery and generation filtering. Actual nonzero Engine
PCM/tap comparisons cover 8/48 kHz and 64/128 blocks, with exact stereo output
unchanged by transfer. Thread tests check 4000 complete packets. Same-generation
clear/clip staleness is exposed by metadata and must be rejected using a current
view before future rendering. Raw fade tails are retained, then become silent.
This is native thread-ownership acceptance, not original JS bridge behavior.
The original meter math/window gates still pass, but playback source/analyser
lifecycle, audible-time scheduling, IPC and renderer integration remain pending.
No production follower or frontend meter consumer is connected in this increment.


Native meter source metadata (25 October 2026): snapshots now distinguish applied
source transitions within the same clip/clear/reset generation. Native tests
cover exact RTRG deadline stamps, cancellation, queued grid stop/play and raw
history preservation; stale same-clip packets can be identified with a coherent
view. The unchanged reference oracle verifies 136 object-lifecycle/input-gating
cases, including stable analyser/data objects through healthy retriggers and
fresh objects after teardown/play. These are complementary contracts, not a
claim that native sourceRevision equals a JS analyser ID or graph state.
Native sound/fade behavior and the original UI remain unchanged. Complete
graph lifetime/teardown, audible-time scheduling, follower/IPC and original
renderer integration remain pending; no visible meter or package is enabled.

Meter graph/read composition (25 October 2026): LoopMeterGraph now matches the
unchanged Loop/AudioGraph methods, exact main.js input gate and complete
AudioEngine.readMeter across 377 trace observations at 1e-12 relative tolerance.
These cover retained retrigger identities, stop/explicit late timer delivery,
fresh play, attached versus detached clear, destroyed graphs, all twelve states,
suspended/replaced contexts, held/backwards audible clocks and history wrap.
The queued-during-stop timer case first failed and was corrected in the native
model without changing the reference behavior. Audio nodes/timer IO are stubbed;
supplied Float32 windows are compared numerically, not browser-generated audio.
Separate native guards reject stale/short required windows atomically, preserve
64/128 supplied-PCM invariance and report zero tracked new/delete. This does not
certify native graph-event binding or live audible-time/renderer integration.
No product UI, audio processing or original assets change in this increment.

Audible-clock policy subset (25 October 2026): the complete original playbackTime
getter and complete readMeter match a native PlaybackClock/LoopMeterGraph
composition across 616 snapshots (maximum error 1.4210854715202004e-14 at 1e-12
relative tolerance). Cases retain original timestamp precedence, startup zero
handling, exception/missing API fallbacks, invalid/overflow/subnormal values,
base/output latency, context loss/suspension, backwards jitter and history wrap.
Delayed clipping selects earlier audible PCM rather than the newest silent
window; native tests repeat that behavior at 64/128 and retain stale-window
atomic rejection with zero tracked new/delete. Snapshot clock/analyser IO is
stubbed, not hardware evidence. Native device/host timestamp acquisition and
transport, graph-tagged PCM binding, IPC and original renderer remain pending.
No DSP, UI, reference assets or package changes accompany this policy port.

Native callback-clock transport (25 October 2026) now carries actual optional
JUCE callback timestamps with local sample intervals in coherent EngineView and
meter packets. This is a native metadata contract, not a JS audible-clock parity
claim. Tests retain exact uint64 and engaged-zero/missing semantics, zero-frame
serial changes, generation/reset and queue-full recovery. All fields and PCM
remain coherent across 4000 concurrent packets; 64/128 Engine tests compare real
monitored output bit-for-bit against an engine without host timestamps. Actual
JUCE processor tests exercise optional capture, DAW seeks, loss, reset, zero frames
and offline mode. The unchanged original getter/meter/graph oracles remain gates.
No conversion of raw host timestamps to audible time, device latency measurement,
frontend JSON, graph binding, renderer or package is enabled in this increment.

Prepared device-output latency (25 October 2026) adds the owning Standalone
device's reported output sample latency to the coherent native callback/PCM
metadata. Native tests distinguish known zero from unavailable, negative,
throwing and rate-mismatched reports; preparation refreshes rate/generation.
Controlled-device tests through the actual JUCE processor prove that queries
occur during preparation and do not recur in processing. Engine output remains
bit-identical at 64/128, and saved parameter/MIDI state remains unchanged.
Every-field concurrent packet tests now include latency availability/sample count.
The original UI/JS clock, graph and meter oracles are retained. This is native
metadata acceptance, not measured device latency or audible-time parity; clock
origin/block-edge alignment, graph/renderer integration and packaging remain pending.

Callback audible-time model (25 October 2026): a controlled delayed-output FIFO
timeline checks 19,200 observations at 8/44.1/48/96 kHz with 64/128 blocks and
0/1/32/256/4096 reported latency samples. It independently derives emitted
frames from the simulated hardware position, including startup and observations
beyond the last rendered block. Tests detect a full-block lead from using the
rendered end at entry and premature startup from unsigned/clamped subtraction.
Wrong origin, stale generation, mismatched callback, offline/zero-size callback,
missing latency and nonfinite inputs reject atomically. Raw host nanoseconds
cannot affect an otherwise paired estimate. These are model tests, not physical
DAC measurements or original-browser device equivalence. Existing complete JS
getter/graph/meter and actual PCM gates remain separate. No IO acquisition,
renderer, IPC, saved state, interface or production audio behavior changes.

Processor-entry clock capture (25 October 2026): core tests carry normalized
processor observations through coherent views/PCM at 64/128 and zero frames,
including queue saturation, recovery, reset, missing/invalid/known-zero input
and every-field 4000-packet concurrent transfer. Actual monitored Engine output
is bit-identical with and without these observations. Native JUCE processor
tests count one clock read per callback, preparation warm-up, no message/state
reads through this reader and exact saved parameter/MIDI bytes. Default native readings are
bracketed by two observations of the same JUCE counter, independently of
optional/raw host time and seeks. This is processor-entry capture acceptance,
not a device-delivery or acoustic timing measurement. No audible correlation,
visible meter, UI/JSON or audio scheduling change is enabled.

Standalone IO dispatch (26 October 2026) adds raw holder-entry capture with a
single-use owner/block/thread scope. Portable tests exercise wrong owner,
wrong frame count, known zero, invalid/nonfinite clocks, nesting/restoration,
cross-thread rejection, duplicate processing, reset and 64/128/zero/offline
callback/PCM pairing. Actual monitored Engine PCM remains bit-identical with and
without the observation; 4000 concurrent packets vary the new fields too.
Actual JUCE processor tests cover the owned scope, one-use rule, missing/error/
stop/reprepare paths and byte-identical saved parameter/MIDI state. The original
JS clock/graph/meter policies remain unchanged and separate acceptance gates.
Generator tests prove that removing the scoped insertion and lifecycle member restores the
exact pinned holder bytes and reverting one include restores the original
Standalone application source; unexpected upstream layouts fail closed.
These tests do not measure physical output latency or validate visible meters.
The holder edge follows device-manager work and is not automatically fed into
the audible-time model. No plugin UI, MIDI assignment, JSON or audio scheduling
behavior changes; no physical controller/driver or DAW timing claim is made.

Lifecycle review added a native regression for reprepare inside an unconsumed
IO scope. It failed before the binding-token fix. Atomic token validation now
rejects both a scope waiting across prepare and an epoch snapshot taken before
prepare but followed by a later clock read. The clock read stays sequenced after
the token snapshot. Initial full gates are retained; only changed core/generator
gates and native wrapper/processor checks need rerunning after this correction.

Split-buffer timing (26 October 2026): the existing whole-callback FIFO oracle is
retained. A separate independent timeline covers enclosing device buffers split
into 64/128-sample blocks with one-sample/short/partial tails, startup, known-zero
and long latency, delayed consumers and malformed/overflow-prone ranges. The
offset correction is internal to the pure model and changes no JS behavior.
Native tests use JUCE's actual extracted splitter with production processors,
nonzero monitored PCM and saved parameter/MIDI byte comparisons. They cover
empty buffers, repeated preparation, exact split ranges, original optional raw
context forwarding, and the zero-maximum guard that silences output without
calling the processor. Both paths retain zero tracked callback new/delete.
This is controlled splitter/model acceptance, not physical delivery correlation.
The holder scope still describes only its own dispatch edge; production meter
selection, graph binding, IPC and visible-meter acceptance remain unfinished.
