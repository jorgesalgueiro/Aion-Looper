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
entries: a former loop press may survive transport-binding cancellation until its
NoteOff. Reassigning that key to OD/SUB/SUS must bypass the old entry immediately,
without finishing its former take. Retain unchanged original reassignment dispatch,
exact nonzero stereo WAV versus direct capture at 64/128, real clear/learn/MidiBuffer
and mouse reassignment/repeat/release checks. Preserve ordinary loop debounce,
CC rearm, PC burst filtering and old-release cancellation protection.
