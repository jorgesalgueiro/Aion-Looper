# Aion-Looper quality gate

The user requires one feature at a time: implement, compare, test, fix and retest;
only continue to another feature when the current feature is as good as or better
than the unchanged Ascii-Looper behavior. Passing state/UI tests does not prove
sound parity. Preserve original interface and functionality; new UI is limited
to needed native audio/input/MIDI configuration using the original style.

Latest accepted work: native capture/overdub finalization, ordered frontend
performance commands, the original NORM action (including its no-op envelope
repair), the loop Rvers control, per-loop/global Undo/Redo, and original MULT
capture/doubling, and the internal Master BPM static/dynamic arm-record-play
workflow with unified mouse/keyboard gestures, native session naming and the
continuous monitor-only metronome, static SOUND/VISUAL count-in and the original
Start slider/Q/nudge buttons and committed-loop waveform display, DYNAMIC live recording trace, and original RTRG/STUT source and visual-clock behavior. Offline DSP and connected
committed-PCM gates pass for their documented scope. Full browser/driver parity
is not established. Each subsequent feature must independently pass the gates
below before another feature starts; keep any newly failing feature under repair.

MERGE acceptance retains unchanged LoopManager.mergeLoops, sliceBuffer and
mixBuffersCircular at offset zero/feedback one: clone empty destinations stopped
with source BPM/dynamic metadata, otherwise preserve destination length/phase and
add one local/global Undo entry. Pin immutable source/base clips, allocate/mix on
the worker and keep source audio unchanged until explicit confirmation. Preserve
playing reversed source ownership, worker OOM/history rollback, full-pool merge,
clone capacity checks and DEL/prepare cleanup. Confirmation is success-only with
the original text/50ms delay; STOP ALL/reset/DEL/reload and replaced/recording
sources must never receive stale deletion. Undo/Redo or a subsequent destination
PCM edit invalidates prior completion; retain both frontend waveform-identity
and audio-owner serial guards. Retain exact stereo WAV comparisons, 1e-6
committed playback, actual 64/128 Engine PCM and GO/confirm/Undo/Redo tests.
Snapshot queue storage belongs to construction-time heap allocation; do not
increase Windows stack limits or allocate telemetry on the callback.

Before advancing:

1. Run unchanged-JS comparisons for state and DSP and the live core audio audit.
2. Integrate bounded ownership transfer/finalization without full-buffer work,
   allocations or locks on the audio callback. Preserve old PCM on failed edits.
3. Test empty/short/long clips, stereo and mono, offsets/wraps, feedback/replacement,
   cancellation/clear/stop/reset and memory exhaustion. Exercise 64/128 blocks.
4. Run meaningful Windows and Ubuntu regressions and native bridge checks when
   changed. Document precisely which behavior was compared and which was not.
5. Do not loosen tolerances, remove a failing case or change preserved JS to make
   a gate pass. When extending a fixture to include more of the reference pipeline,
   retain the old regression coverage at its appropriate kernel/component layer.
6. Do not mark the feature complete or start another feature while its acceptance
   fails. Keep working on that feature. Do not label a build production-ready on
   the strength of unit tests; driver/DAW/platform validation is separate work.

The native audio diagnostic and connected NORM/reverse/history comparisons are hard acceptance gates, including the unchanged-JS Multiply comparison. It now compares the
connected manual capture/finalization pipeline; the old raw-mixer regression is
retained in the kernel comparison. Do not infer full-feature or hardware parity
from the committed-PCM subset. See docs/REFERENCE_AUDIT.md for current limits.


Build cadence: use incremental core tests and the existing standalone binary by
default. The user explicitly requests the minimum packages needed for testing;
do not routinely bump versions or generate ZIPs for each feature/bug fix. Preserve
the existing interface. A reproduced acceptance failure takes priority over new
features even if a previous narrower comparison passed.

Main-workflow gates: js_native_sync must retain actual original recorder/grid/
extraction/effective-rate methods, including fractional-tempo and loud-input cases.
When changing the performance controller, run real NativeUiSmoke pointer/key
press/release/hold checks as well as strict browser-timer mocks: the prior mocks
missed a WebView2 Illegal invocation receiver bug that stranded held inputs.
Do not reintroduce object-method calls to unbound Window timer functions. Keep
capture deadlines on audio samples and cancel all armed/queued starts on panic,
clear/reset. Internal Master BPM acceptance does not imply DAW/MIDI clock,
latency-calibrated physical input or full browser-rendered audio acceptance.

Mouse-main workflow changes must also run tools/TestMouseWorkflow.ps1 against the
optional aion_mouse_host at 64/128 samples. It links the production processor and
editor and drives actual mouse capture/playback using synthetic PCM, without
snapshot fixtures or physical devices. Preserve header/state-text short actions
and exclude nested controls/waveform/main-button bubbling. This gate supplements
NativeUiSmoke and unchanged-JS DSP/sync gates; it does not establish hardware or
Standalone driver/input-gate acceptance. Do not install/package the test host.

User-confirmed hold behavior (2026-10-19): delete only after 4 seconds of an
uninterrupted press that starts on STOP/STOP NOW (mouse or assigned key).
600ms cuts playback but must never delete. Release before 4000ms, blur, pointer
cancellation/lost capture and STOP ALL cancel deletion; delayed release may
recover a >=4000ms deletion once. PLAY/FINISH/CANCEL/RECORD origins never delete.
Retain 3999/4000ms boundary tests, early/deferred timers, and actual PCM mouse/
keyboard checks at 64/128. Document the distinction in existing EN/ES/PT manuals.

Queued holds must remain cancellable after leaving the pressed-key map. Pass a
validity predicate to queued cut/delete resolvers and check it at dispatch.
Global blur/reset/STOP ALL invalidates all epochs; DEL invalidates that track's
old holds before clearing. Normal >=4s release keeps an uncancelled queued delete
valid. Retain real PCM paused-dispatch/blur and clear/re-record regressions.

Accepted rapid main gestures must not disappear if their resolver sees finalizing.
Keep them in FIFO order using the wait sentinel, original expiry and polling;
priority STOP ALL must still discard deferred actions. Keep the retained native
rapid record/finish/play case and deterministic finalization wait/panic checks.

DEL must supersede all unsent actions owned by that track and bypass deferred
finalization waits, including waits on other tracks. Keep track ownership on
all local main/history/control/hold commands; global actions remain unscoped.
Preserve other-track order, in-flight acknowledgements, the queue bound/expiry,
and global STOP ALL priority. Retain real native PCM deferred-DEL/no-resurrection
acceptance and scoped priority/cancellation unit tests.


Session names are metadata: keep them ahead of ordinary audio/finalization waits,
after panic/deletion barriers, coalesced per track and retained on STOP ALL.
DEL supersedes older local names; DEL ALL clears old work and acknowledges before
names typed afterwards. Preserve the original name handlers, UTF-8 validation,
reload/clear lifecycle, metadata-only capacity protection and native race tests.

The continuous metronome must retain js_native_metronome's complete unchanged
worklet PCM and connected Engine checks. Keep zero callback new/delete, exact
stereo/64/128, MASTER bypass, loop-capture exclusion and separate MASTER/OUTPUT
metering. N must preserve repeat/text-entry guards and rapid toggles must use
the native value. Keep standalone UI click volume at zero during smoke tests;
use the hardware-free host for actual audible-PCM verification. Physical drivers,
DAW/MIDI and Apple platforms remain separate acceptance work.

Static count-in must retain js_native_countin, the expanded unchanged-JS arm/grid
checks and TestCountInUi.ps1's actual OfflineAudioContext sound oracle. Do not
replace browser pitch-band blending/full-wave normalization with a Nyquist-only
peak-normalized triangle: it failed actual sound parity. Keep the 4e-5 analytical
and actual-browser peak limits and <=1e-4 relative browser RMS, all rate/tempo
cases, fixed stereo/64/128 and zero callback new/delete. Wavetables belong to
prepare-time storage, not large stack frames. Keep SOUND/VISUAL independent,
static-only, latched at arm, excluded from MASTER/meter/internal capture, with
all cancellation/reset barriers and unrelated/rejected command protection.
Preserve real mouse SOUND/cancel checks in both block-size workflows. Actual
browser acceptance currently covers WebView2; other engines/hardware remain
separate work. Never create a product AudioContext for native sound.

Start-slider acceptance must retain js_native_start_phase's unchanged setter,
Loop.restart and AudioGraph source offsets/conditioned PCM at <=1e-6, including
rapid future sources, sync/solo exceptions, reverse and fractional rates. Keep
phase values/source offset math double precision; preserve float audio controls.
Do not replace the fixed FIFO with a single pending restart: it loses accepted
releases within 50ms. Preserve cancellation epochs on panic/DEL/reset and source
ownership changes, bounded overflow rejection, metadata-only dragging and the
stopped/empty release guard. Retain zero callback new/delete, full-pool controls,
Engine stereo/64/128 checks and actual native slider drag/PCM tests. Full
bit identity of browser transition envelopes are separate work. Preserve the
user's local logo edits when staging controller changes; avoid blanket formatting.

Q acceptance must retain the unchanged quantizeLoopStart handler and existing
Start PCM coverage. Keep short-duration no-ops, positive half-up rounding, endpoint
1, current-BPM/recorded-duration math, stopped/empty guards and atomic metadata
rollback on FIFO exhaustion. Retain real native Q mouse checks at 64/128.

Nudge acceptance retains unchanged nudgeLoopStart in the Start PCM oracle and all
previous slider/Q coverage. Preserve exact signed IPC directions, current BPM /
recorded-duration steps, single-wrap arithmetic and endpoints, stopped/empty
no-ops, atomic FIFO rollback and panic/DEL cancellation. Multi-cycle tiny clips
are intentionally repaired to [0,1]; document that distinction from reference
parity. NudgeWebAudio.cjs reproduces the original negative-offset RangeError and
checks wrapped playback. Retain actual mouse and bridge checks at 64/128.

Waveform acceptance retains unchanged generateWaveformPeaks (720 bins, ceil
partition, max across stereo channels, padded tail) and original canvas draw
comparisons. Peaks belong to immutable worker clips; callback metadata and bounded
fixed-size SPSC copies must never scan PCM or allocate/free/lock. Keep telemetry
storage off large Windows stack frames. Preserve identity/generation guards,
known-ID payload omission, slow-editor recovery for all 20 tracks and cache
lifecycle through NORM (no history entry), edits/Undo/Redo, Multiply/OOM, DEL/reset.
Retain actual captured-waveform pixel checks at 64/128. Additional live visualizers
and canvas interactions are separate migration work.

DYNAMIC live-trace acceptance retains original 512-sample analyser downmix and
canvas/progress methods, including sync-off ten-second playhead and rolling bar
markers. Never create a product AudioContext. Preserve zero-prefixed chronological
history, capture-start/generation ownership, suppression after finish/stop/reset,
fixed bounded telemetry and no callback allocation/free/lock. Keep exact Engine
64/128 windows, MASTER/monitor/click exclusion, actual WebAudio signed mono/stereo
levels and original live-canvas comparisons. Preserve native red/yellow pixel,
successive-take and mid-take sync-change checks. Browser render-quantum latency
and physical input latency remain separate acceptance work.


RTRG acceptance retains the unchanged user-level Loop.retrigger Start reset and
sixteenth snap, and its playing +5ms versus immediate stopped source timing.
Keep the immediate core graph primitive/source tests separate, with unchanged
PCM thresholds. Preserve rapid taps, reverse/solo/rate cases, 5ms/50ms inter-track
deadline ordering, ring wrap, full-queue metadata rollback, panic/DEL/reset and
zero callback new/delete. Native RTRG intentionally supersedes that track's
older slider/Q/nudge releases; retain other-track sources and rapid RTRGs.
This pending-phase safety repair is documented separately from reference parity;
do not claim identical overlapping browser sources or transition envelopes.
Retain actual RTRG mouse/PCM and original Start/Q/nudge checks at 64/128 samples.


STUT acceptance retains unchanged Loop.stutterStart/Stop graph-clock phase and
fragment bounds, repeated holds and resumed cursor, plus actual WebView2 WebAudio
ordinary/tail fragments at zero/nonzero source clocks. Preserve fractional-region
neighbour wrapping/extrapolation; a full-file neighbour passed the old model but
failed actual browser audio. Keep the 1e-6 PCM and 1e-8-frame metadata thresholds,
mono/stereo/reverse/varispeed/all-sync/tiny-clip cases, and all prior Start/RTRG gates.
STUT release must bypass command-queue capacity/acknowledgement waits. Retain
per-track atomic cancellation revisions, stale queued/unsent start invalidation,
zero-block release, native editor destruction and reload cleanup. Keep real PCM
64/128 pointer hold/outside-release/blur/reload/deferred-release checks, with
exception collection surviving reload, and no product AudioContext or PCM edits.
Source replacement intentionally clears its temporary region; overlapping/future
browser graph transitions and broader browser/hardware timing remain separate.

STUT must also retain unchanged Loop.getProgress: the visual graph/master clock
continues during held fragments and after release. Keep audio and visual cursors
separate, reset the visual override on source replacement/stop, and retain the
1e-12 circular-progress comparison, frontend routing, and actual draw-stream
checks at 64/128. Dynamic recording progress and the unchanged canvas renderer
must retain their existing gates.

WAV export must retain unchanged bufferToWAV/encodeWAV bytes and exportLoop's
UTF-16 filename semantics, including freshly typed names before telemetry updates.
Export stored PCM, independent of playback controls; keep the original waveform
click and never bubble it into transport. Audio only pins one identity/generation-
validated immutable lease; worker encodes/releases it and message consumes bytes.
Preserve bounded independent requests, full-pool/DEL/prepare/OOM recovery and zero
callback new/delete. Native navigation and editor destruction cancel an unchosen
destination; do not rely on unload RPC or new-page bootstrap timing. Late picker
callbacks must fail lifetime/sequence checks. Retain the cancel/replacement test:
deliver the obsolete picker's valid destination while the replacement is waiting,
assert no obsolete file/status/transport mutation, then verify replacement WAV
bytes. Keep background temporary-file write/
flush/error/replace checks. Test-host picker injection is not OS-dialog validation.
Keep frontend export in gesture order after edits/finalization, with the latest
committed identity and click-time filename. Retain rapid NORM/export, unsent DEL
cancellation and quiet repeated clicks. Windows OS-picker visibility/save was
confirmed by the user on 2026-10-20; the saved Documents WAV was independently
validated against the live clip's length/rate and stereo PCM16 structure. The
automation helper's missing window was not proof of an invisible dialog. This
closes that Windows gate only; other platforms and OS Cancel/overwrite button
interaction remain separate checks. See docs/VALIDATION.md for exact scope.

Mouse fixtures must retain actual CDP pointer events and 240 BPM SOUND/cancel
assertions. Observe short cues from actual rendered callback PCM and its audio-
owned armed/frame/master/flash evidence, plus the displayed CANCEL state. Slow
IPC can sample only gaps between 100ms cues. Keep the original exact block-meter
gate with an every-block consumer in CountInTests.cpp. No audio/timing mocks or
threshold changes: the host's bounded SPSC evidence logger writes off audio.
Queue ordered press/move/release before awaiting their
acknowledgements so latency cannot turn a 20ms click into a >=600ms hold. Keep all
original 3800/4200ms gestures and the analytical 3999/4000ms boundaries; never relax
an assertion or change product DSP to compensate for test-transport latency.

MIDI looper acceptance retains js_native_midi's unchanged original note handler
and mapping-table markup, actual JUCE MidiBuffer processor checks and original
mouse/keyboard/audio gates. Performance must work with no editor. Keep fixed
256-event processing, bounded queues/press/task storage and zero callback
new/delete/locks; device queries and state parsing belong off audio. Learn
consumes its event, rejects overlapping bindings atomically and persists accepted
queued settings. PC is a single pulse; CC is a positive edge rearmed by zero,
never a held-delete source. Held notes retain the original 600ms cut and user
4s STOP-origin-only delete boundaries. Panic/disable/device changes/overflow must
cancel stale tasks and restore momentary SUS without deleting PCM. Preserve muted/
absent input rejection, sample offsets, finalization FIFO and MIDI Stop's pending
GUI-action purge. Native mode snapshots must not overwrite a newer UI choice:
retain mode revision acknowledgement fencing and the unchanged NativeUiSmoke
keyboard OD/SUB check. Physical PC learn is verified separately from synthetic
PCM and does not establish MIDI clock, continuous expression, synth or HID parity.

Physical MIDI acceptance must retain the confirmed triple-PC foot-controller
burst regression: three identical packets per tap previously stopped/restarted
playback. Keep the fixed 20ms window anchored to the first matching PC/channel/
target, exact 159/160-sample boundary at 8kHz, no extending debounce, sync-off and
static/dynamic stop/play cases, and actual 64/128 MidiBuffer burst tests. Preserve
bounded eight-event raw diagnostic storage and zero callback new/delete.

Continuous MIDI mixer acceptance retains unchanged App.handleMIDIMessage and
UIManager.setLoopVolume/setLoopPan comparisons for all 128 values and 16 channels.
Keep CC zero, repeated positive/nonmonotonic values, original pan parseInt steps
(0..10) and volume 0..2; continuous CC must never enter the held-button state or
transport path. Only existing loop volume/pan sliders support this increment.
Retain original right-click learn, 10s native timeout, Escape/editor cancellation,
shift-right-click clear, duplicate trigger/slider rejection and versioned 73-slot
state with backward-compatible 33-slot loading. MIDI controls work without an
editor and use sample offsets through the existing parameter smoothing. Retain
nonzero stereo PCM equality against scheduled UI controls at 64/128, zero callback
new/delete, native MidiBuffer persistence/legacy rollback, and actual pointer/
keyboard/reload/timeout tests. Unported effect/master/synth/expression targets stay
disabled; this gate does not establish physical expression-pedal or clock parity.

Continuous MIDI learn must retain the failing-then-fixed same-block burst case:
consume only the first valid learn CC, then apply following values at their sample
offsets without waiting for the editor/message thread. A fixed provisional target
may bridge acknowledgement only after a bounded overlap check; never mutate
persisted settings or allocate/lock on audio. Mode-only configurations must not
reset captured learn or enqueue duplicate results. Token/target/generation/binding
changes and global cancellation clear provisional ownership. Keep conflicting
channel/omni mapping protection, unchanged-JS learn comparisons, exact nonzero
stereo PCM for preassigned AND provisionally learned controls, actual MidiBuffer
and 64/128 native mouse burst checks. Physical pedal verification remains deferred
to the user's scheduled check; synthetic messages are not hardware evidence.

MIDI enable/disable clicks are relative gestures: resolve each toggle against
the message owner's latest accepted settings under its non-audio mutex, never
against a stale audio snapshot. Keep explicit enable(bool) idempotent for existing
callers. Both actions cancel learn/deadlines; accepted queue order must preserve
every click before audio resumes. Retain frontend double/triple-click cases from
both initial states, actual processor queued-state/persistence checks and real
64/128 pointer events. Keep the large-decimal saved-state rejection regression;
malformed input must preserve the complete previous mappings/APVTS state.

MIDI learn cancellation must not depend on configuration capacity or a fresh
audio snapshot. Escape calls the native cancellation path even before its learn
snapshot arrives; listen in capture phase so loop-name keyboard isolation cannot
block it. Preserve ordinary typing/shortcut guards. cancelLearn RPC shares editor
close/timeout's bounded-queue fallback, and UI learnTarget reports accepted
message-owner intent so cancellation clears while audio is paused. Retain failed-
then-fixed full 31-entry queue/resume/no-stale-assignment/fresh-learn recovery,
paused indicator, immediate Escape, focused-name, actual 64/128 pointer/key and
unchanged original range/text-field Escape comparisons. No audio-thread locks,
allocations or extra package are permitted.

Slider MIDI learning must not release an already-held performance gesture.
Starting, acknowledging, cancelling, timing out or closing learn preserves held
SUS recording/mute, normal STOP deadlines, deferred tasks and PC burst ownership.
Clear learn/provisional state independently; the full-queue fallback uses its own
atomic learn cancellation, applied after queued configurations. Disable, panic,
device changes, overflow and transport-binding replacement still cancel owned
performance actions. Retain unchanged context-menu/CC/Escape routing, exact
stereo WAV against direct capture at 64/128, full-queue production MidiBuffer
checks and actual mouse-learn/Escape with a held synthetic note. No physical
expression claim follows from these synthetic gates.

CC zero/release must reach its existing performance owner even when slider learn
consumes or rejects that packet. Match exact kind/channel/key; never trigger a
new press or steal an overlapping transport mapping. Preserve first-packet learn
consumption, pending-conflict protection, normal continuous-parameter ordering,
SUS capture/play/mute release and next-positive-edge recovery. Retain the failing
release-during-learn regression, exact stereo WAV against direct capture at
64/128, production MidiBuffer and real original-interface mouse cases. CC button
routing is a native extension; unchanged-JS CC mixer parity is a separate gate.

Audio cancellation must invalidate an unacknowledged MIDI learn result even after
it has left the result queue. Keep an audio-published lock-free cancellation
serial on each captured result and validate both newly popped and pending results
before accepting a mapping. Never depend on snapshot freshness: retain saturated
telemetry, immediately serialized state and next-callback no-revival checks for
MIDI Stop, CC120/123 and overflow at 64/128. Keep fresh-learn recovery, normal
accepted mappings and all held/continuous/PC gates. Cancellation after an already
accepted mapping does not erase that mapping. No audio locks or dynamic storage.

MIDI mixer acceptance must check actual DOM slider positions, not just native
metadata. Focus left after right-click/ordinary selection must not freeze learned
volume/pan sliders. Preserve active primary-pointer drags, range-key repeats,
outside release, pointer cancellation/lost capture and blur; resume hardware
updates when editing ends. Retain focused volume/pan and real mapped-drag tests
at 64/128 plus frontend ownership checks. Preserve the user's logo hunk separately
when editing looper.js; stage only the slider integration hunks.

MIDI mapping fields retain original focus/blur semantics: pointer or Tab focus
arms learning, leaving cancels that field, accepted completion releases focus,
and an immediate re-click can restart even before rendering catches up. Preserve
reply/revision fencing and scoped cancellation so an old field cannot cancel a
different field/slider. Retain unchanged original focus/blur methods, actual Tab,
blur/no-later-assignment, rapid re-click, reload/save, full-queue scoped AND Escape
cancellation, and all MIDI/learned-slider gates. Hidden/unported drone fields must
not become keyboard-navigation traps. Keep original mapping-table markup intact.

Original loop-note lost-release recovery is press-driven, not automatic: repeats
at or before ten seconds remain debounced; a fresh loop NoteOn strictly after it
discards the old press entry and executes the current main action. Do not end an
intentional long hold, reset its age on ignored repeats, or apply this rule to CC
positive edges/PC pulses/global mode notes. Retain unchanged original MIDI/press/
release boundary comparison, exact nonzero stereo WAV against direct capture at
64/128, actual MidiBuffer offsets and real GUI lost-release/long-hold checks.

Global OD/SUB/SUS NoteOns bypass the original loop handleInputPress entirely:
every positive NoteOn toggles, including repeated packets without NoteOff. Do not
store global NoteOns as held loop gestures or apply the loop debounce/10s recovery.
Retain original handler/toggle comparisons on all channels, zero-velocity/releases,
mutual exclusion, actual MidiBuffer offsets and original visible mode indicators at
64/128. Learned global CC buttons keep zero rearm and PC keeps 20ms burst filtering.

Global NoteOn routing must resolve the current binding before consulting held loop
entries. Transport-binding cancellation retires former Note ownership; global
OD/SUB/SUS still bypass ordinary loop debounce. Reassigning that key must act immediately,
without finishing its former take. Retain unchanged original reassignment dispatch,
exact nonzero stereo WAV versus direct capture at 64/128, real clear/learn/MidiBuffer
and mouse reassignment/repeat/release checks. Preserve ordinary loop debounce,
CC rearm, PC burst filtering and old-release cancellation protection.

Bridge replies must carry the requesting document's correlation token. Numeric
request IDs restart on reload and cannot alone identify the owner. Retain stale
completion/reused-ID regressions, native success/error token echo, timeout/128-slot
bounds, pagehide cleanup, legacy untagged direct requests and actual WebView2
reload delivery tests. Strip the private token before exposing replies to UI
callers. Correlation is not authentication or cancellation of old native commands;
keep existing audio generation/expiry and native close/reload cancellation guards.
Do not introduce browser/JSON allocation or locks on audio.

A synchronous bridge send/serialization exception must remove the pending ID and
clear its timer before rejecting. Preserve the original error and full 128-slot
capacity immediately, without waiting for the five-second reply deadline. Retain
transport/circular-payload/getter failures, more-than-capacity retries and actual
WebView2 recovery after injected send failures. Do not weaken existing session,
reply, timeout, queue or pagehide guards.

Native navigation must release temporary STUT through its queue-independent path,
even if blur/pagehide release RPCs are lost and no next-page bootstrap runs.
Retain actual held-fragment -> blank document -> original editor tests at 64/128,
with audio-thread evidence of released state, continuing nonzero playback and
unchanged clip identity/length. Keep runtime exceptions visible across documents;
empty allowed documents must not execute the original mixer bootstrap.

Original X/DEL ALL dispatch uses physical KeyX before the generic repeat guard,
with text/modifier protection and the unchanged confirmation text. Button and
keyboard must share native cleanup. The clear handler must issue no command or
reset queued work on cancellation; acceptance cancels STUT, held timers and older queued actions
before the bank clear. Retain unchanged original keyboard dispatch and real
WebView2 dialog accept/cancel, stored PCM/identity, paused-queue and four-second
no-revival tests at 64/128. Custom key mapping remains a separate feature.

Host timing observations must be read only inside processBlock and published with
one coherent audio snapshot, never queried from IPC/UI/state save. Preserve
explicit missing/invalid-field handling, signed preroll/seek positions, fractional
tempo, meter/bar/cycle flags and zero-sample callbacks. Keep callback-start local
frame distinct from host sample/PPQ positions and snapshot block end. Retain
fake-head and portable saturation/reset/loss tests, bit-identical nonzero monitored
PCM with changing host data, zero callback new/delete and 64/128 checks. Observing
host transport must not overwrite internal sync parameters or imply scheduling,
MIDI clock, sample-offset tempo ramps or actual DAW/wrapper acceptance.

JUCE Standalone supplies partial device PositionInfo. Do not treat hostTransport
available as proof of DAW tempo/sync: preserve real sample/seconds counters while
missing musical timing stays null and unspecified play/record/loop flags stay
false. Keep the actual Standalone IPC regression as well as no-playhead tests.

Keyboard lost-release recovery is fresh-press driven, strictly after 10000ms from
the original press. Repeats/modifiers/text events cannot recover or refresh its
age. Never end an intentional held recording by timeout. Invalidate only the
recovered key's old timers/queued cut-delete predicates before a new current-state
action; other held keys remain valid. Retain unchanged original keyboard/press
boundary comparisons for all 20 loops, monotonic-zero origin, deferred callback
and ownership tests, plus actual >10s recording/no-repeat/fresh-finish/nonzero-PCM
playback at 64/128. Preserve four-second STOP deletion and unported SUS/keymaps.

Validate framed CC data bytes (0..127) before CC120/123 cancellation. Malformed
panic must not finish a held SUS take, restore mute, invalidate MIDI Learn or
alter committed PCM. Retain every-channel 128/255 rejection, exact nonzero stereo
WAV against no-packet capture, real raw juce::MidiBuffer and actual GUI/learn/audio
checks at 64/128, zero callback new/delete, and valid panic/release recovery. Keep
existing accepted seven-bit panic values/omni cancellation; stricter channel-mode
semantics and wire-stream parsing are separate work, not part of this fix.


Keypad loop dispatch follows the original character mapping: only numeric key
values select loops 1-10, even with Shift. NumLock navigation/Unidentified/empty
values must not issue a transport command or swallow navigation. Only main-row
Digit codes use the Shift bank fallback. Keep physical code ownership on release
when NumLock/Shift changes, unchanged-JS all-digit/navigation comparisons, and
actual 64/128 record/finish/play/stop plus >4s PCM/timer preservation. This does not
enable custom keyboard mappings or keyboard SUS; keep those scopes separate.


Zero-sample host callbacks accept only offset-zero MIDI at the current audio
frame. Dispatch bounded MIDI controls without ticking PCM/clocks or draining
ordinary queued UI loop/gain commands; empty zero-sample callbacks retain their
old behavior. Keep invalid positive/negative offsets and >256-event cancellation,
MIDI pass-through, explicit MIDI Stop, CC/PC learn and release ownership. Retain
exact nonzero stereo WAV against ordinary capture, clock/length invariance,
production zero-/two-channel MidiBuffer tests between 64/128 blocks, and zero
callback new/delete. JUCE permits zero-sample buffers; this native flush policy
is separate from original browser-rendered audio, actual DAW/wrapper validation
and MIDI clock synchronization. No UI redesign or package is required.


Windows VST3 wrapper acceptance is opt-in through AION_PLUGINVAL_EXECUTABLE and
vst3_pluginval (tools/TestVst3Pluginval.ps1). Rebuild the VST3 before testing a
processor/editor/state change; do not confuse direct processor tests with format
loading. Keep pinned tool hashes, seed/matrix, exact required completed-test
markers, enabled GUI checks and before/after binary hashes in result.json.
Inherited pluginval environment options must not weaken coverage or leak caller
changes. Portable/default builds require no validator or network download. The
pinned official pluginval build omits ExtremeTests; a level-9 pass is not an
allocation/malloc certificate or a Steinberg-validator/DAW/hardware/other-format
pass. Preserve separate core/processor allocation and original UI/PCM gates.


Successful processor state restoration must notify the host after committing all
accepted APVTS/MIDI state and releasing the configuration mutex. Keep the JUCE
parameter-info refresh flag (mapped to CLAP values/text/info rescan), without
marking non-parameter state dirty. Rejected malformed/empty or full MIDI-queue
restores must not report success. Retain exact saved-state identity and actual
CLAP state-reproducibility basic/binary/buffered acceptance, plus VST3 pluginval.
The opt-in AION_CLAP_VALIDATOR_EXECUTABLE / clap_validator gate pins the official
0.4.1 Windows tool, accounts for all 44 tests, and refuses ancestor TOML filters.
Do not disable failures to pass it. Preserve explicit unsupported/platform skips
and the JUCE void state-load random-data warning; it is not evidence that random
bytes changed the processor state. CLAP GUI, allocation, real DAW/device workflows
and other OS/format acceptance remain separate from this vendor suite.


Original input-wheel hardening belongs in the native adapter because preserved
main.js is not executed: window capture, passive:false, non-Shift INPUT range/
number only, in original preventDefault/stopPropagation/blur order. Shift bypass
must not invent custom wheel increments; text/other targets keep normal behavior.
Retain the unchanged original listener oracle (630 cases) and trusted WebView2
wheel tests at 64/128 with focus/default prevention and unchanged native MASTER,
loop volume, BPM and loop-command counters. No original asset or DSP change.


Linux production-processor coverage uses the same aion_midi_processor_tests
source/production JUCE processor as Windows; target remains EXCLUDE_FROM_ALL.
AION_TEST_NATIVE_PROCESSOR explicitly registers its serial CTest. Linux CLAP
validation is independently opt-in through AION_LINUX_CLAP_VALIDATOR_EXECUTABLE
and TestLinuxClap.py. Pin the official Ubuntu tool/archive and account for all 44
tests, requiring Unix RTLD_NOW rather than accepting Windows's skip. Preserve
all known unsupported-extension skips and the explicit JUCE void-load warning;
never filter a failing test. No GUI, hardware, malloc certification, LV2/VST3 host
workflow or Release latency acceptance follows from a Debug build/CLAP pass.
Do not overwrite a live user's session or create a package for these gates.


OS MIDI device-change subscriptions belong only to an actual Standalone holder,
not every processor instance. A host-routed/synthetic processor must run without
ALSA sequencer hardware (/dev/snd/seq) and without JUCE assertions. Keep the strict
assertion/leak CTest regex; do not disable it to accommodate hardware probes.
Preserve Standalone registration/cancel callback and polling/selection paths.
Retain native midiDevices diagnostics requiring no watch/no OS devices with no
holder, actual Standalone UI smoke requiring deviceWatchRequested, raw MIDI
performance/panic/learn and all 64/128 processor/audio/GUI gates. This diagnostic
means subscription was requested, not that physical hotplug was tested.

Linux VST3 wrapper acceptance is opt-in through AION_LINUX_PLUGINVAL_EXECUTABLE
and linux_vst3_pluginval (tools/TestLinuxVst3.py). Keep official Linux 1.0.4
archive/executable pins, level 9/seed 1337, editor tests enabled and all 15 rate/
buffer combinations plus required completion markers. No display or wrong tool
must fail; inherited pluginval options must not weaken coverage. Preserve child-
only environment sanitization, owned-process timeout and before/after module
hashes in result.json. The vendor binary omits allocation tests and no external
Steinberg validator is configured. Editor lifecycle does not prove HTML workflow,
physical audio/MIDI, real DAW sessions or Release latency. Keep original-JS/PCM
gates independent; do not alter the UI or create packages for this host check.


Linux LV2 acceptance is opt-in through AION_LINUX_LV2_PLUGINVAL_EXECUTABLE,
linux_lv2_pluginval and linux_lv2_audio. Reuse the pinned official Linux runner
without weakening existing VST3 coverage. Keep LV2 format identity, entry-point/
shared-harness and before/after module/RDF hashes, required GUI/matrix completion,
child-only environment sanitization and strict assertion/leak failure checks.
Lv2AudioProbe.py must use the actual public descriptor/ports and patch:Set, not a
mock processor or private API. Preserve default mute, unity/half gain, monitor
mute, master zero and unmute recovery with finite nonzero settled stereo PCM at
44.1/48/96 kHz and 64/128. Check descriptor URI/port metadata and missing metadata/
wrong-format/no-display failures. This is not loop-session/HTML/MIDI/physical
hardware, gain-ramp waveform, arbitrary-malloc or Release latency certification.
Do not modify original assets, DSP, user mappings or create a package for a gate.


Double-click Speed/Feedback reset must move the focused range to 1.0/0.8 like
original UIManager.resetLoopSpeed/resetLoopFeedback. The bounded capture adapter
in controls.js changes only the displayed value; the existing target handler
still owns native command dispatch. Keep focus/event propagation, enabled range/
canonical track-ID guards, the unchanged-JS 560-case oracle and trusted 64/128
WebView2 checks of DOM plus native values, nonzero recorded PCM/clip/history
preservation. Retain wheel hardening and main gesture/transport regressions.
Do not reset unrelated controls or modify the user's separate logo edit.

The GUI queued-DYNAMIC-finish fixture waits for an early phase after screenshot/
mode setup; do not replace its mandatory stopPending assertion with a playing
fallback. Preserve the original <=50ms immediate-finish core test separately.

Real DAW gates (23 October 2026): tools/TestReaper.py launches its own Windows
REAPER profile/process and tests the actual local VST3/CLAP module (process-module
identity plus before/after hashes). Preserve zero master hardware sends, synthetic
stereo, queued-parameter delivery through actual renders, set/mutate/restore value
checks, seven settled PCM cases including project reopen, and clean owned exit.
Never reuse or terminate an existing user REAPER instance. Keep profile guard and
240-second owned-child timeout. Do not copy user ini/license/MIDI settings.

tools/TestArdour.py uses actual ardour8-lua/libardour with isolated HOME/XDG paths
and a Dummy sine backend at 48kHz/64/128. Numeric LV2 patch properties are found
through Automatable property controls, not Plugin.parameter_count (port indices).
MIDI meters precede audio; derive offset from meter input_streams, require stereo,
retained unity/half/recovery ratios, exact mute and session reopen. Require PASS
and reject Lua Error/JUCE assertion/leak output even if luasession exits zero.
Keep bundle/RDF hashes and logs. These are stereo peak checks, not PCM equality,
GTK interaction or physical JACK acceptance. Ardour 8.4 does not host CLAP.
Do not change DSP/UI or weaken reference tests to accommodate host test mistakes.
Keep MIDI clock, host-follow scheduling and recorded-loop persistence explicitly
pending. No packaging or version bump is needed for validation harnesses.

Synchronized Speed UI must retain unchanged UIManager.setLoopSpeed thresholds
(<0.38 -> .25, <.75 -> .5, <1.5 -> 1, otherwise 2) in STATIC/DYNAMIC. Keep free
OFF speeds and its intentional focused-thumb suppression. Shared speedCommand
routes slider/reset/half actions without changing the C++ DSP or queue coalescing.
Retain 1,560 original-setter cases across all tracks, modes, focus and boundaries;
trusted 64/128 range-key input must check both native rate and focused DOM, plus
nonzero recorded PCM and unchanged clip/history. Do not rewrite original assets,
relax comparisons, synthesize extra input/change events or stage the user's logo.


Loop status text must match unchanged UIManager.updateLoopDisplays, including
pending STOP [.] versus stopped [S] and the original leading spaces in muted/
duration text. AionLoopStatus is a browser presentation adapter only: retain
native finalizing [..]/SAVING, leave PCM/state/command dispatch untouched, and
keep all 6,400 complete-reference-updater comparisons (status-only DOM fixtures).
The real 64/128 mouse gate must observe stopping [.] while recorded PCM still
plays, then stopped [S] after second STOP, with clip length/history unchanged.
Do not weaken the mandatory stopping assertion, alter original assets or stage
the separate user logo whitespace edit. This display repair requires no package.


Global performance status follows unchanged UIManager.updateStatus: SUB > SUS >
OD for master-header/mode-class precedence, with independent audio-owner capture
classes (record/armed, OD, SUB, MULT), queued flag and original YES (Loop N)/NO
recording label/colour. Finalizing is not an active capture. Preserve unrelated
body classes and the initial master label; do not enable unported master recorder,
keyboard SUS or synth/FX as part of presentation work. Keep the 7,920 complete
reference-updater cases across all track owners/states/modes/SUS/queued/default
labels. Native 64/128 MIDI must show active OD purple after global OD is switched
off, correct header precedence, unchanged clip identity during mode toggle,
nonzero committed PCM and retained history. Do not alter DSP or original assets.
Run native GUI processes with desktop access: a sandboxed WebView2 startup timeout
must remain a failed/inconclusive test, not be counted as a feature pass. Retain
the failure log and clean up only the verified owned test host before retrying.


LIVE loop cards reuse unchanged generateLiveLoopHTML and updateLivePerformanceState.
Only the loop section is enabled: original header short actions share native
mainAction/FIFO, Rvers shares the existing native reverse command, other FX stay
disabled. Do not add hold-delete to headers or enable the unported live master
mix, drones, samplers or effects. Rebuild cards on LIVE tab entry like the original,
then update in place while active; polling must not replace focused controls.
Use the original updater's name/state/colour values (including quote behavior),
placing names via textContent instead of interpreting session data as HTML.
Keep 400 original markup, 1,600 updater and 121 panel-priority comparisons, copied
method identity and real 64/128 LIVE record/finish/play/stop, keyboard, reverse,
name/tab/disabled-control isolation plus nonzero PCM/clip/history gates. Preserve
existing main-loop/hold/sync/navigation/MIDI tests and the user's logo edit.


LIVE loop mixer controls project unchanged MasterMixManager.renderLive into the
existing LIVE mixer container: enable only the twenty loop volume/M/S strips.
Use existing native FIFO volume/mute/solo commands; do not introduce WebAudio or
change DSP/APVTS/schema. Construct mixer controls once per editor and refresh in
place, retaining MIDI slider edit ownership across tab entry. The original
master/input/drone/sampler strips and RMS/held-peak meter behavior remain unported;
exclude those strips and explicitly mark empty meter placeholders unavailable.
Do not substitute native block peaks for original RMS or claim full mixer parity.
Retain 1,200 complete original-renderer comparisons, copied source identity and
48 production-wheel comparisons against the unchanged original handler, including
clamps/disabled guard. Native 64/128 tests must hit actual visible controls and
observe gain-zero/mute/solo silence and nonzero recovery, main-view mirroring,
unchanged clip/history and stable fader identity. LIVE mixer test targets centre both logical scroll axes for the original
vertical-writing-mode faders below the sticky header; preserve strict hit-testing and
all existing main-workflow assertions. Preserve failures as evidence, user logo
whitespace and original renderer whitespace. This increment needs no package.


LIVE loop-volume MIDI Learn aliases live_mm_slider_l_0..19 to the existing
33..52 targets shared with main loop-volume controls. Keep original right-click
learn, Shift-right-click clear, Escape/10s cancellation and Save Mappings/reload
semantics; do not allocate targets or change native MIDI/APVTS state schema.
The original setupTouchSafety oninput parser is the 20-case alias oracle. Keep
720 actual status-renderer cases across track/CC/channel/learning/bound states,
shared tooltip/outline and immutable mapping state. Native 64/128 raw MidiBuffer
tests must learn CC zero, preserve focused/drag slider ownership, observe real
silence/nonzero recovery, retain shared binding on editor reload and clear it in
both views, without transport commands or clip/history edits. Isolate playback
from other fixture loops with the existing solo control before PCM checks.
Centre both logical scroll axes for original vertical faders and retain strict
pointer hit testing. User hardware assignments and the separate logo edit must
remain untouched. No package/version bump is needed for this adapter repair.


Direct primary-action MIDI Learn is a requested native UI extension, not an
unchanged-JS context-menu feature. Resolve loop-action-0..19 through existing
0..19 mappings and retain all 60 continuous aliases. Right-click learns Note/CC/
PC; Shift-right-click clears; Escape/editor reload cancels. Mirror the original
mapping field's yellow learn outline without changing original markup/CSS, DSP,
state schema or hardware assignments. Keep actual context-menu disabled/unsupported
guards (320 cases), immutable mapping/button highlight tests (640 cases), and
native 64/128 Note/CC/PC stop/play PCM, same-target field handoff, consumed learn,
CC zero/rearm/repeat, PC burst, right-hold/no-delete, Escape/save/reload/clear and
clip/history tests. Direct learning must never synthesize a primary mouse action.
Before the retained final disabled-expression check, restore an assigned CC at a
nonmaximum volume so an erroneous later maximum CC would be observable. Preserve
old regressions and the user's logo. Keyboard SUS remains independently pending.


LIVE header direct MIDI Learn aliases the original loop-header and its descendants
to the existing 0..19 primary bindings. Require a genuine owning live-loop-0..19
card/header and an enabled main action; FX and card backgrounds stay excluded.
Keep main buttons, mapping fields and rebuilt LIVE headers aligned without
changing original markup/CSS or MIDI/DSP schemas. Retain the 560 production
context-menu hit/modifier/disabled cases, existing 320/640 gates (including LIVE
status), and native 64/128 Note/CC/PC consume/burst/edge audio, long right-hold,
Escape/tab rebuild/save/reload/clear and unchanged clip/history checks. This is
the requested native direct-learning extension, not original-JS context-menu
parity. Preserve the user's separate logo edit; no new package is needed.


Main loop-header MIDI learning must share 0..19 primary bindings with main buttons,
LIVE headers and mapping fields. Match the existing short-action surface: exclude
nested buttons/inputs/selects/labels/textareas/links/canvases, FX/control areas and
card backgrounds; retain disabled-primary guards and genuine card/header ownership.
Keep the 1,520 main hit/modifier/disabled cases, prior 320/560 context-menu and 640
immutable status cases, and actual 64/128 header-number/state Note/CC/PC learning,
consume/burst/edge stop/play PCM, STOP-origin right-hold/no-delete, Escape/save/
reload/clear and clip/history checks. No original markup/CSS or DSP/schema change;
this is the requested native MIDI extension, not an original JS context-menu
feature. Preserve the separate user logo and other pending feature boundaries.


Cancelled MIDI Note ownership must end at cancellation, matching unchanged
LoopManager.stopAll/inputPressTimers, not wait for NoteOff or ten seconds. Retire
only Note entries after their momentary restoration; retain suppressed CC owners
until zero rearms them. Keep same/different-key recovery after a full 64-Note pool,
late old release isolation, ordinary active-note >10s/hold boundaries, CC zero and
PC burst rules. Retain the 16-channel unchanged-JS Stop oracle, 224 play/stop + 28
mute exact nonzero stereo WAV cases at 64/128 across seven cancellation sources,
production same-block offsets/repeat/SUS/PCM tests, actual mouse STOP ALL and
MIDI Stop/CC120/123 GUI tests, plus sanitizers and callback allocation gates.
Do not change input mappings, UI/schema or the separate user logo. A preliminary
same-key-only repair is insufficient if cancelled notes still occupy the pool.

Default Space STOP ALL must precede the generic keyboard repeat guard, as in
unchanged App.handleKeyboardPress. Retain the 130 original dispatcher cases for
Space/Shift/repeat/modifiers and text/control focus, plus trusted native WebView2
repeats interleaved with real synthetic MIDI playback at 64/128. Every repeated
Space must stop the new audio, preserving clip identity/length/history and range/
button focus behavior. Keep normal loop-key repeat debounce, custom mappings and
keyboard SUS boundaries, existing panic cancellation and the separate user logo.
Activate the owned test WebView with CDP Page.bringToFront before the mouse
fixture's compositor-routed wheel input. Keep exact event-count/trust/target/
focus/preventDefault/value/PCM assertions and selector/coordinate failure evidence;
never substitute synthetic DOM wheel events to make this gate pass.

Independent loop MUTE MIDI Learn uses targets 73..92, total 93. Continuous CC
is ONLY 33..72; discrete MUTE Learn requires a positive edge. Preserve original
latched mute in OD/SUB/SUS, release debounce and PC burst filtering. Never apply
main-button cut/delete or SUS restoration to independent MUTE. Its bounded Press
owner stores the real track and buttonOnly flag. Remap/clear only that MUTE owner;
retain suppressed CC ownership until zero and never finish unrelated held SUS
capture when learning, acknowledging or clearing a MUTE binding. State version 3
writes 93 bindings; versions 1/2 read exactly 33/73 and leave new slots empty.
Keep rollback for invalid/future states and callback allocation checks.
Retain 240 PCM/immutable-WAV matrix cases, offset/capture/held-owner checks,
288 original toggleMute action oracle packets, 400 context-menu cases and 2880
shared status snapshots, plus actual 64/128 pointer/Save/muted-editor-reload tests.
Bind initial UNM as well as MUTE; update mute colours per property to preserve
MIDI Learn outline. Preserve original reference HTML/CSS and the separate user
logo. Stage only feature hunks if looper.js also contains the user's logo edits.

Real REAPER MIDI gate (24 October 2026) is tools/TestReaper.py --midi. Retain
all existing seven monitor/track-state/project renders plus main Note record/
finish and independent MUTE Note/CC/PC repeats/releases, silence/recovery, center
pan and recovered-loop sample phase, and saved assignment/item reopen. Use only
documented chunk APIs on the owned disposable instance: equal-length XML edits,
opaque wrapper preservation, all 93 bindings and raw MIDI identity/readback/hash.
Publish sorted MIDI_SetAllEvts atomically; incremental InsertEvt repairs incomplete
notes between inserts. Supply explicit end-of-item CC123, not an empty record
REAPER rewrites. CountEvts groups notes; it is not the raw packet contract.
Pace MIDI renders at verified project 1x (projrenderlimit, persist=0); full-speed
offline can outrun worker finalization. Preserve that failed evidence and do not
claim full-speed offline recording acceptance or loosen PCM/window assertions.
Retain the reproduced VST3 PC0 failure as historical evidence; the bounded
per-channel generated-wrapper repair must pass the strict real REAPER gate,
keeping channel, repeated PC0/PC127, supplied sample offsets, burst rules and all
existing parameter IDs/state. Never skip/xfail a newly failing host MIDI case. Do not fabricate 128 preset changes or call UI IPC
from program callbacks to make a mute test pass. Unit/processor MidiBuffer passes
do not establish actual wrapper delivery. Portable reaper_midi_fixture negative
controls must reject silent output, monitor leakage, phase/pan corruption and
lost recalled mappings. No production UI/DSP change, package or version bump is
needed for this diagnostic gate. Preserve the separate user logo edit.

VST3 Program Change bridge acceptance: cmake/Vst3Programs.cmake generates a copy
of the exact pinned JUCE 9.0.4 wrapper using tools/patch_vst3_programs.py. Keep
both normalized SHA256 and exact replacement-count guards mandatory under -O;
never edit dependency sources or silently accept upstream drift. Sixteen hidden
wrapper parameters/program units represent MIDI commands, not application presets.
Keep getNumPrograms()==1, APVTS/schema/old parameter IDs and original HTML unchanged.
Process every host queue point, including identical values, through the existing
MIDI engine. Preserve each supplied offset and channel; no setCurrentProgram/UI
RPC/audio-thread device queries. Decode malformed IDs/values/negative offsets
without allocation. Bound all program queues together to 256 points and signal
excess through OverflowSink's lock-free atomic rejection of the entire MIDI block;
never fabricate MIDI packets. Reset that flag on prepare and preserve raw MIDI.
Reserve new MIDI storage in prepare, not processing. Retain 4096 real-PCM portable
channel/program/64/128 cases, the production overflow/recovery/preset/ID tests,
fail-closed generation tests, validators and the 20-case-per-format REAPER matrix.
REAPER's VST3 PC-driven capture observed a 7168-frame rather than nominal 7200-frame
period. Keep the original Note-timed 7200-frame phase test. For the added PC-driven
cases identify exactly one period using pre-mute PCM alone, then compare a full
6720-sample recovery window four actual cycles later at the unchanged tolerance;
reject ambiguous periods and require raw-MIDI CLAP's period to remain 7200.
Retain the initial failed fixed-period assumption, and do not claim sample-accurate
REAPER PC timing or fix it with audio sleeps. Supplied-offset portable acceptance
is separate. Keep 1x pacing and full-speed offline/physical/Apple/other-host limits.
No packaging/version bump is needed for this compatibility repair.


Offline finalization exception (user-approved 24 October 2026): only a host-declared
non-realtime render may wait for existing worker jobs. AionProcessor forwards
isNonRealtime per callback, with false/default/unknown retaining the nonblocking
live path. Keep all allocation, full-clip DSP and destruction on the worker.
Waiting must not advance audio samples. Commit before the next command/MIDI task,
including same-sample finish/next-track record, and before rendering a scheduled
or pool-exhaustion finish. Retain bounded MIDI task work, ownership/revision
cancellation and zero tracked render-owner new/delete. Keep exact complete mono/
stereo PCM equality at 1/64/128 blocks, ordered capture/OD/SUB/NORM/history/MULT/
MERGE, static auto-finish at 8/48/96 kHz, capacity finish and cancelled live-origin
job tests. Retain the initially failing same-sample MIDI handoff case and real
production host-mode tests. TestReaper.py --midi --full-speed is separate from the
retained default 1x gate: verify projrenderlimit=0, retain every existing nonzero/
silence/channel/repeat/reopen/phase assertion and original tolerances. Never
pace or sleep audio to hide a failure. The offline wait sleeps only while a
published worker job is pending; the virtual sample clock is held. Keep the
unpaced failure evidence and REAPER VST3 supplied-timestamp quantization limit;
this repair does not certify physical latency, host-follow or stored-loop recall.
Do not infer other DAW/platform offline acceptance from REAPER VST3/CLAP tests.


Keyboard RTRG repeats must match unchanged App.handleKeyboardPress: an unmapped
'/' key or NumpadDivide runs before the generic loop-repeat guard, with existing
modifier/text/content-editable protection. Keep ordinary loop and mode repeat
debounce. Retain the 1,300 complete-original-dispatcher comparisons and trusted
WebView2 repeat flags at 64/128: move Start away from zero using actual range-key
input before each retrigger, require one native command, reset phase and nonzero
PCM, and preserve waveform/length/Undo/Redo. Test stopped-loop repeat recovery and
no held cut/delete ownership. Keep the original wheel startup assertion and its
failed evidence; do not synthesize DOM wheel/key events to bypass a failing gate.
This is default shortcut repeat parity, not custom mappings, keyboard SUS, broader
selected-FX routing or physical keyboard/OS layout certification. No DSP/UI markup
change, package or version bump is required; retain the separate user logo edit.


Default keyboard MULT repeat parity: unmapped backslash dispatches before generic
loop-repeat suppression, like unchanged App.handleKeyboardPress. Preserve text/
modifier/editable guards, loop/mode repeat debounce and the accepted RTRG gate.
Retain 1,040 complete-original-dispatcher comparisons, no held cut/delete timers,
and actual trusted nonrepeat/repeat keydowns at 64/128. Repeats must finish a real
interactive append and double stopped PCM through existing native dispatch. Save
stereo WAV via the public export API: original prefix must be byte-identical,
appended input nonzero, stopped halves exact copies, Undo/Redo exact, playback
restored. Keep all prior main/hold/MIDI/export/merge tests and original Multiply
DSP oracle/tolerances; do not redesign finalization/varispeed behavior to pass
a shortcut gate. Host picker injection substitutes only destination selection.
Keyboard SUS, custom mappings, other keys/selected-FX routing and physical/DAW
keyboard focus remain independent. No UI markup/DSP/schema/package/version change;
retain the separate user logo.


Export cancellation on navigation/destruction belongs to the native editor.
Do not restore unscoped cancelExport calls in frontend bootstrap or pagehide:
delayed cleanup can cancel a newer picker. Keep explicit foreground cancellation
and native sequence/lifetime checks. Retain ExportWorkflow's real-controller
debugger interception, delayed packet release after a fresh NORM chooser, zero
startup-cancel count, JS-disabled navigation and all WAV/cancel/replacement tests.
Wait for the reloaded original NORM control to be enabled before clicking;
native cancellation telemetry alone does not establish document readiness.
No callback/DSP/state/UI redesign or package is needed for this lifecycle repair.


MeterFollower is a calculation component only, not enabled product metering.
Retain the complete unchanged AudioEngine.readMeter oracle: RMS, 0.8s hold,
20dB/s decay, 1.2s clip timing, stalled/jittering clocks, disconnected tails,
context/source/data resets and 128-entry eviction. Keep zero tracked new/delete,
32768-sample input cap with atomic oversized-window rejection, and supplied-window
64/128 tests. Do not claim block peaks are original RMS. Native PCM window
collection/downmix, audible-time transport and original renderer integration
must independently pass before enabling the original meters or packaging them.


Native raw meter windows (24 October 2026): MeterWindow stores the latest 2048
mono PCM samples for each loop, after loop gain/pan and before MASTER, monitor
and click. Storage for twenty windows is allocated on the heap at construction;
callback push/copy/reset stays bounded and has no tracked new/delete. Inspection
belongs exclusively to the audio owner; never read this mutable storage directly
from the message thread. Retain actual WebView2 analyser comparisons (including
varying-signal chronology, zero prefix, wrap and signed stereo cancellation),
actual LoopBank/Engine tests and all existing callback/GUI/DAW gates. This tap
contains actual native output/tails, not certified original source identities.
MeterFollower remains unwired in production. Timestamped bounded transfer,
source lifecycle and audible-time scheduling must pass before renderer/IPC/UI
integration. No meter feature package or visible-meter parity claim yet.
