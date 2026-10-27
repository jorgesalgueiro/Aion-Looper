# JavaScript/native parity audit — 2026-10-18

**0.4.3 (capture gate established in 0.4.2): committed manual capture/overdub/substitute PCM now passes its reference
comparison. Full application sound/functionality parity is NOT established.**
After the capture and command-ordering gates passed, NORM was added as a separate
worker transaction. After its live no-op repair passed, the original Rvers control
was ported and compared separately, followed by Undo/Redo after reverse gates passed, then MULT after the history gates passed. Hardware/listening and the remaining items below still need
acceptance work; the original interface remains the contract.

## Reference and comparison scope

The unchanged resources/ui loop, audio, main/input, project, tracker, synth and
effect sources define the target. All 16 runtime files remain byte/SHA identical
to the preserved reference. State setters/clear/mute/solo execute from the actual
JS classes in a Node VM; browser/audio IO is stubbed, not the tested calculations.
508 traces compare 10,160 native loop snapshots (the original 408 traces are retained, with 100 reverse/clear traces appended). Seeded state is fixture setup,
not a reference browser recording integration test.

The offline DSP test executes unchanged crossfade/circular-mix functions plus the
actual recorded-loop DC-removal block. It covers 544 cases (including 72 stereo normalization threshold fixtures) with
an unchanged 1e-6 tolerance. The earlier raw 512-frame 0.2+0.1 overdub regression
is retained here, including its tapered boundary behavior.

`AudioParityAudit.cjs` now drives the connected native path: record input frames,
FINISH, wait for worker commit, optionally capture an offset overdub/substitution,
FINISH and render the committed clip through center pan. The oracle applies the
reference fresh-take crossfade then DC removal, or edit DC removal then crossfade
and circular mix. 126 mono/stereo cases compare 965,826 frames at 8/48/96 kHz,
lengths 1-48,001, feedback 0/0.8/1 and wrapped takes longer than the base. Maximum
observed error is 7.395e-8; tolerance stays 1e-6. Both output channels are checked.
Playback attack/release, browser resampling, latency correction, sync and hardware
IO are explicitly outside this committed-PCM comparison.

## NORM comparison scope

The unchanged AudioEngine.normalizeBuffer scans both channels for one maximum,
then applies the same double-precision ratio with Float32 writes to each channel.
Its 0.0001 silence/unity thresholds are unchanged. Kernel tests include asymmetric
channels, silence, threshold neighbors, tiny buffers and peaks above unity.
A separate connected gate runs reference capture conditioning then NORM against
native record/FINISH/worker-normalize/render for 42 mono/stereo cases, three sample
rates and lengths 1-48,001. The capture audit remains unchanged, preserving all
its earlier cases. Playback envelopes and browser resampling remain excluded.

The old clip stays audible until commit. NORM keeps source phase and controls;
Stop/clear/reset and worker allocation failure follow the same cancellation and
rollback rules as capture edits. Stopped clips never autoplay. Only one worker
transaction/capture may run at a time, so NORM rejects active capture/finalization.
No extra capture blocks are reserved, but a private immutable result temporarily
uses additional memory. Unit tests cover live phase, repeated/idempotent NORM,
failed allocation, 32 clear cancellations, reset, empty/capturing rejection and
normalized playback at 64/128 samples. No physical listening claim follows.

The live NORM follow-up also covers a case the committed-PCM fixture did not:
when normalizeBuffer reports no change, the native commit must preserve the
current playback envelope. A failing paired-playback regression exposed a fade
restart on repeated NORM. Worker publication now carries the changed flag, and
the regression checks exact stereo output during attack, steady playback and wrap.
See VALIDATION.md for the repair and test-build scope; older ZIPs predate it.

## Reverse source acceptance

The next feature is the original Rvers checkbox. Actual unchanged Loop.toggleEffect
and AudioGraph.build/retrigger methods execute in the comparison VM; audio nodes
are IO stubs. The source buffer comes from the unchanged getReversedBuffer method.
The native path uses reverse sample indices on immutable forward PCM, with no
allocation/copy on the audio callback. Requested direction applies to the next
play/retrigger, never immediately to the currently playing source.

The JS source is an independent copy when reversed. Normalization changes stored
forward PCM but cannot mutate that playing copy. Native normalization therefore
retains the old raw clip lease while a reversed source is playing, and releases it
on restart/stop/clear/reset. A completed overdub/substitution restarts using the
current requested direction and the newly committed stored PCM.

30 mono/stereo cases at 8/48/96 kHz and lengths 1/4/17/512/4097 compare 201,918
rendered frames with tolerance 1e-6. The fixture covers pending toggles, retrigger,
wrap, normalization/source independence, quarter/half/fractional/double rates,
and wrapped overdub/substitution. Maximum Windows error is 1.029e-7. The fractional
read comparison uses a linear interpolation model on the actual JS source samples;
it does not establish Web Audio resampler, envelope or scheduled-start equivalence.
The original capture and NORM fixtures and thresholds remain intact.

## Undo/Redo acceptance

The unchanged Loop.saveStateForUndo/undo/redo/clear, LoopManager.pushUndoState,
cloneBuffer and capture/edit DSP execute in the history oracle. The native path
records and finalizes actual takes, restores immutable clip leases and renders
committed PCM. Nineteen mono/stereo and global-order cases compare 828 history
snapshots (all 20 slots plus global heads) and 666,144 frames at 8/48/96 kHz.
Lengths are 17/512/4097, with eight edits to exercise eviction, five undo/redo
steps, excess no-ops, NORM, replacement, redo branching and clear. The global
fixture runs 120 edits over 20 slots, covering the 50-entry cap and local/global
interactions. Maximum Windows error is 1.249e-7 at the unchanged 1e-6 tolerance.

Stable playing/stopped restoration is supported. Same-track capture/finalization
is temporarily guarded to prevent racing an edit transaction; unrelated-track
capture can continue. Failed or canceled edits commit no new checkpoint and keep
the redo branch, improving failure recovery. Successful overdub/substitute edits clear that
loop's redo and the global redo order; other loops' local redo remains available,
matching the JS. NORM creates no checkpoint. Playing restoration uses the existing
transition envelope; stopped restoration stays stopped and keeps all controls.

A native UI regression exposed empty Ctrl+Z sending rejected commands instead of
silently doing nothing. Dispatch now consults acknowledged history depth; the
empty and repeated-Undo tests pass. Original per-loop/global buttons, target
labels, Ctrl/Cmd+Z and Shift+Z redo are used without new markup. Text editing keeps
its own shortcuts, while nontext controls allow the reference global gestures.
Source PCM and state comparisons do not establish full browser envelopes, host
lifecycle, memory-pressure or hardware acceptance. Histories are not persisted.

## Historical failure and repair

0.4.1 directly overwrote playing PCM while recording an edit. The isolated raw
mixer fixture differed in 102/512 frames (maximum error 0.0424264). Simply dropping
that case or widening tolerance would have hidden the defect. Its regression now
lives in the kernel layer; the expanded connected gate adds reference capture
conditioning rather than comparing conditioned native audio to an unconditioned
JS buffer.

0.4.2 records edits into a separate bounded take pool. A worker finalizes a private
result; the old clip is immutable and audible until commit. Reference DC/fades,
tapered mix and soft clipping now execute in this path. The audio owner commits
at beginBlock and queues retired clip leases for worker-only deletion. A job pins
its base independently, so clear before worker pickup cannot free that base.
Stop prevents late autoplay, clear invalidates a revision, and rate reset joins
the worker before changing storage. Allocation failure preserves old PCM exactly.

Previously fixed state mismatches remain: clear preserves mute; pan truncates
like parseInt; only one capture/finalization owns the recorder; zero-frame finish
cannot strand it; DEL ALL asks the original confirmation; stop cancels hold timers
and gets priority over unsent controls.

## Validation and limits

- Windows Release and Ubuntu Debug CTest pass 4/4, plus the separate connected
  PCM gate. Existing queue/deadline, pan/rate/fade and 64/128 playback regressions
  remain after committed clips are ready. FINISH-to-play worker latency is not
  deterministic across block sizes; the reference also finalizes asynchronously.
- Ownership tests cover immutable playback during capture, forced worker OOM,
  old-PCM preservation, stop/clear/reset, full pool and 32 repeated canceled edits,
  all 20 playback slots and observed audio-thread C++ new/delete=0. The allocation
  watch is thread-local: worker allocations are intentional, not realtime failures.
- AddressSanitizer/UBSan pass 4/4. ThreadSanitizer core tests pass with process-local
  `setarch x86_64 -R`; its normal WSL launch failed with an address-map conflict.
  No global ASLR or system security setting was changed.
- Windows native WebView2 tests pass the original controls and a test-only SAVING
  reply fixture. Stop All waits for the audio request acknowledgement. The initial
  early-script fixture was ineffective under this WebView; the final fixture wraps
  completion messages and restores the bridge. App output stays muted and no
  microphone recording starts. This checks UI presentation, not physical capture.
- Windows Standalone/VST3/CLAP build with ASIO enabled. The test/probe executables
  are not runtime package contents. DAW loading/validators and physical ASIO/Topping,
  JACK/CoreAudio, Apple/device/memory/listening acceptance remain pending.

## Remaining feature and sound gaps

| Area | Remaining work |
| --- | --- |
| Capture timing/input | Latency compensation, multi-input routing, calibration, hardware duplex/underruns |
| Browser playback equivalence | Attack/release, resampler/antialiasing and full browser-rendered audio fixtures |
| Performance gestures | Ordered/coalesced bounded queue now tested; broader physical multi-controller integration remains |
| Loop features | SUS, stutter, start offsets, merge, names, WAV import/export |
| Projects/master recording | ALP/PCM persistence and master/input recording unported; session audio remains temporary |
| Timing | Loop-source sync, audible count-in/metronome, cascade and MIDI/host clock unported; native Master BPM static/dynamic workflow now has scoped gates |
| Instruments/effects | Drone, sampler, tracker, effects and preset DSP unported |
| Controllers | MIDI learn, CC/note/PC/expression, gamepad and keyboard remapping unported |

## Reproduce

Build the core with Node available before configuring, then run CTest and the
separate audio acceptance command:

```
node tests/AudioParityAudit.cjs build/core/aion_reference_probe
```

On Windows use `build/windows/Release/aion_reference_probe.exe`. All acceptance
steps must pass; passing state or kernels alone is insufficient. The CI capture
workflow runs the same connected gate without continue-on-error. See AGENTS.md.

## Multiply comparison scope

The oracle executes the unchanged `Loop.multiply/multiplyInstant`,
`LoopManager.startMultiply/stopMultiply/processMultiplyData/createBufferFromChunks`,
`saveStateForUndo/undo/redo`, and audio conditioning functions. Graph restarts,
AudioWorklet IO and DOM updates are stubbed; the tested DSP and control methods
are unchanged. Native tests record real synthetic takes through the bounded pool,
finish the worker job, then audition committed PCM through the existing center pan.

Ninety mono/stereo cases compare 832 metadata snapshots and 1,546,108 PCM frames
(maximum error 2.015e-8) at 8/48/96 kHz. They cover base lengths 1/17/512/4097,
captures 0/1/127/128/513/8193, twice-repeated append, stopped/no-input doubling and
unequal-length local Undo/Redo. A mixed overdub/undo/Multiply case preserves the
reference's unusual local/global redo interaction. Tolerance stays 1e-6. These
checks do not establish browser-rendered fades/resampling, physical input routing,
latency compensation, DAW sync, full waveform visualization or mobile memory use.

Core checks separately cover exact 64/128 timestamped capture/render equivalence,
zero observed callback new/delete, short-capture discard, varispeed guards,
allocation failure with redo retained, Stop/Clear/reset, reverse-held source
cancellation, current-PCM exhaustion, rejected oversized doubles and larger history
restores, and growth reservations while another track has a pending job. Failed
or canceled transactions never publish an undo checkpoint. This intentionally
improves allocation-failure recovery over JS's pre-allocation checkpoint timing.

## Main arm / record / play acceptance scope

The main workflow is now implemented for the internal Master BPM source. The
unchanged `LoopManager.startRecording/stopRecording/processRecordedData`,
`SyncManager.getNextGridTime/getNextBarTime`, actual recorder chunk conversion,
reference extraction/DC/fade code and `Loop.effectivePlaybackRate` execute in the
new oracle. IO, AudioWorklet delivery, visual timers and DOM updates are stubbed;
no tested audio calculation is replaced by a hand-written JS substitute.

The comparison covers OFF/STATIC/DYNAMIC rate selection, static arm/count-in /
cancel timing across default and uneven meters, manual early stop, fixed-size
extraction/padding/folded tail, dynamic first/second-tap and forced finish, short
and multi-bar trim, mono/stereo and 8/48/96 kHz, including fractional BPM. The native
source receives the same input stream as the original recorder, including its
pre-arm prefix. Audio tolerance remains 1e-6. JS floating times are converted to
sample deadlines with at most a one-sample representation difference; native core
regressions additionally require exact intended integer-grid boundaries.

Zero latency is the oracle scope: system/manual compensation, physical input
routing and listening, browser sample interpolation/envelopes, DAW/MIDI clock,
loop-source sync, count-in sound/metronome and pre-tap snap are not accepted by
these tests. As in the reference, dynamic takes start at the tap and can remain
shorter than a whole bar; the native UI does not pretend its disabled SNAP/REC TIME
controls provide recovered pre-tap audio. Timing settings are latched for a take,
an intentional safety improvement over mutable browser-global finalization state.

Two live regressions are retained: fractional-tempo static extraction initially
lost the reference's floor-prefix sample, fixed with the two-sample POD history;
and exact grid arithmetic could yield a deadline one sample late, fixed by
removing only integer representation noise. Real native input also exposed
`Illegal invocation` from browser timer functions called as controller-object
methods. Their corrected invocation and release/cancel cleanup are checked with
both strict timer mocks and actual WebView2 pointer/key events. Node mocks alone
are insufficient acceptance for main performance gestures.

Final main-workflow report: 112 cases, 224 metadata snapshots, 108 native-float
rate checks and 388,772 PCM frames pass on Windows and Ubuntu, with maximum audio
error 6.094e-8 at unchanged tolerance 1e-6. Initial cases are retained; loud-input
fixtures were appended to cover the static tail clamp and dynamic trimming.
Core timing/playback is exact at both 64/128 buffers after commit, and the real
browser repeated-key/mouse hold/release acceptance passes after the timer fix.
See VALIDATION.md for failures, repairs, platform gates and the separate hardware
acceptance still required.


## User RTRG phase and source timing — 2026-10-20

The previous native command implemented only the graph source restart, leaving
Start unchanged. The unchanged `Loop.retrigger` additionally clears Start with
sync OFF and computes/sixteenth-snaps the effective-rate master phase with sync
ON. It then replaces a playing source at +5ms or starts a stopped source at the
tap, rather than waiting for the next PLAY grid. Native RTRG now follows those
paths without adding controls or creating a product AudioContext.

`StartPhaseParity.cjs` retains all previous setter/Q/nudge/source PCM checks and
adds 300 user RTRGs across 30 mono/stereo, 8/48/96kHz and 1/17/512/4097/20003-frame
fixtures: OFF/STATIC/DYNAMIC, fractional BPM/rate, reverse, rapid taps, stopped
play and solo exclusion. Metadata tolerance stays 1e-14 and committed source PCM
stays 1e-6. Existing immediate graph/kernel comparisons remain on `sourceRestart`;
they must not substitute for the added user-level action oracle.

Native tests additionally cover bounded queue overflow/rollback, inter-track
5ms/50ms ordering, ring wrap, stale phase cancellation, panic/DEL, exact Engine
64/128 stereo output and callback allocation/free instrumentation. Accepted RTRG
supersedes that track's older pending Start/Q/nudge sources, an intentional safety
repair; reference overlapping pending-source envelopes are not reproduced or
claimed. Browser resampling/envelopes, physical latency, DAW/MIDI and other
platform acceptance remain separate work.


## STUT hold/release — 2026-10-20

`StutterParity.cjs` calls unchanged `Loop.stutterStart`/`stutterStop`, retaining
reference-conditioned source PCM and its graph-time/effective-rate arithmetic.
Thirty mono-forward/stereo-reverse fixtures cover rates 8/48/96kHz, lengths
1/17/512/4097/20003, OFF/STATIC/DYNAMIC, fractional BPM, 1x/0.37x/2x, repeat presses
and successive holds/resumed cursors. There are 1,350 region checks and 321,192
rendered samples. Audio tolerance remains 1e-6; region frame tolerance remains 1e-8.
State/IO stubs alone do not
establish actual browser sound parity.

Actual WebAudio initially exposed a tail-boundary mismatch despite those tests
passing: a fractional cursor's neighbour came from the full file instead of the
fragment. A deferred-wrap experiment failed at another rate and was discarded;
all failing cases remain. The source interpolation now wraps its neighbour inside
the fragment, with tiny-region extrapolation. This is also consistent with the
[Chromium source's region interpolation](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/modules/webaudio/audio_buffer_source_handler.cc);
the installed browser's measured output, rather than HEAD-version assumptions,
is the acceptance oracle.

`StutterWebAudio.cjs` executes the unchanged methods on actual OfflineAudioContext
sources and compares to the native-conditioned stereo-fold source. It retains the
original zero-origin cases and adds identical nonzero source clocks: 12 cases,
86,016 samples, 8/48/96kHz, ordinary/tail regions and 1x quantum-aligned holds and
release. Maximum error is 1.49012e-8 at unchanged 1e-6. Broader rates, sub-quantum
browser control timing, cross-engine rendering and transition envelopes are not
accepted by this browser subset.

Native cancellation is intentionally stronger than reference cleanup: blur,
editor reload/destruction, STOP ALL/DEL/reset and source replacement cannot strand
a temporary region. Audio-owned per-track revisions invalidate already queued
starts, and frontend predicates invalidate unsent released/deleted gestures.
A release does not consume command-queue capacity or wait for another command's
acknowledgement. PCM and history remain unchanged.


The same 1,350 hold/release snapshots now also call unchanged `Loop.getProgress`
and compare the native visual graph/master clock at 1e-12 circular-progress
precision. The original white playhead continues around the full loop during
STUT and after release, independently of the fragment's audible cursor. Native
source replacement/stop resets this visual override. Frontend routing and the
actual mouse host's draw stream check that the playhead leaves the held fragment;
committed waveform drawing itself remains the unchanged accepted renderer.

## Waveform WAV export — 2026-10-20

`WavParity.cjs` executes unchanged `AudioEngine.bufferToWAV`/`encodeWAV` and
`ProjectManager.exportLoop`. Encoder coverage includes mono/stereo, 0/1/17/4097
frames, clipping, asymmetric signed PCM16 scaling/truncation, nonfinite PCM,
8/44.1/48/96/192 kHz, fractional-rate header truncation and NaN fallback. Connected
coverage uses unchanged crossfade/DC conditioning on mono/stereo captures at
8/48/96 kHz and 1/17/512/4097/20003 frames. All 86 header/data comparisons (767,734
bytes) are exact. Thirty repeated exports after playback-only reverse/rate/Start/
STUT changes remain identical. Filename tests cover empty names, punctuation,
accents and surrogate pairs against original exportLoop. No preserved asset or
tolerance was changed.

`ExportTests.cpp` covers empty/stale clips, full pool, one-outstanding bound, pinned
DEL/prepare, worker allocation failure/recovery, retained PCM/history, export behind
a future performance command, generation reset, zero-block requests and exact
64/128 captured WAV bytes with zero callback new/delete. Browser export tests use
the production processor/editor/worker/writer with a test-host destination picker.
That seam replaces destination selection only; it is not OS-dialog proof. Native
navigation cancellation is required: unload/bootstrap-only cleanup failed the
retained 128-sample reload/late-picker test.

The final picker fixture waits for explicit completion markers rather than a
400ms timeout. A reload check disables JavaScript before navigation and releases
the old picker before reenabling it, proving the native cancellation boundary.
Rapid NORM followed by waveform export must contain the normalized committed
samples; pending export follows the gesture FIFO and an unsent DEL cancels it.

Retained 240 BPM SOUND mouse checks exposed another test limitation: native
snapshot polling could sample exclusively between the 100ms cues. Passive
callback evidence recorded a 0.49428 output peak while armed, before the capture
deadline, with MASTER/flash zero. The GUI gate now reads actual rendered PCM and
observes the original displayed CANCEL state, retaining both mouse cancellations
and absence of later sound. The previous exact block-outputPeak contract remains
checked for every block in CountInTests.cpp, alongside read-only diagnostic
consistency and zero callback allocation. No DSP, tempo or tolerance was changed.

This compares the existing reference capture pipeline. It does not establish
physical input fidelity, wet/mixed export, project restore, DAW or other platform
acceptance. Actual OS-picker observations are recorded separately in VALIDATION.md.

## MERGE user action and connected PCM

`MergeParity.cjs` executes unchanged `LoopManager.mergeLoops`, its global
`pushUndoState`, `AudioEngine.sliceBuffer` and `mixBuffersCircular`, with original
capture crossfade/DC conditioning. It compares full stereo PCM16 bytes, source
preservation and Undo/Redo, then renders committed playback at the unchanged
1e-6 threshold. The 72 cases cover 8/48/96kHz, duplicated mono/distinct stereo,
empty destination clone, 1-frame/tiny/4097-frame clips and multi-wrap sources up
to 48001 frames, including clipping/chunked reference mixing. Exact stored-byte
comparison covers 1785720 bytes; maximum playback error is 7.907836940823643e-8.

Native tests separately retain clone tempo/dynamic metadata and stopped state,
existing playing phase, independent reversed playback sources, global/local
history, pin/DEL/prepare/OOM/full-pool behavior, actual Engine stereo PCM at 64/128
and zero callback new/delete. GO/confirmation accept/decline, exact exported
clones/history and panic/new-capture deletion protection run through actual
WebView2 input and the production processor/worker. Destination selection for
these saved PCM checks uses the existing test picker.

Native merges are serialized against recording/finalization. Failed work keeps
old audio/history; the original updates Undo before attempting its mixer. The
native delayed-clear handshake additionally rejects stale generations, cleared
or replaced takes, active new capture and panic/reset. These are documented
safety adaptations, not claims of identical original races. Smooth forward
source transitions retain native blending; exact overlapping WebAudio source
envelopes, physical drivers and Apple/Linux UI remain separate acceptance work.

### Native MIDI looper increment (2026-10-20)

MidiParity.cjs executes unchanged App.handleMIDIMessage for 16384 default-map
messages across all 16 channels, note numbers and release/velocity forms. It
also retains exact original mapping-layout/table markup. The original handler's
unmapped CC/PC behavior is compared; learned CC/PC looper actions are an explicit
native extension, not a claim of ported original tracker/CC slider behavior.

Core tests compare MIDI versus directly scheduled recording at sample offsets
17/900: exact stereo PCM16 WAV bytes, length 883, at both 64/128 blocks. Native
processor tests exercise actual JUCE MidiBuffer offsets, PC 0 learn/consume,
persist/restore while paused, duplicate-state rollback, restored performance
without editor, overflow and zero processBlock new/delete. Native mouse tests
exercise original fields, learning, duplicate preservation, clear, reload,
save/disable and PC pulse safety. Fixed input bounds and cancellation cover
panic, stale future GUI actions, SUS restoration and held-note boundaries.

Physical AudioBox USB learning of PC 0 is independently confirmed by the user
and native telemetry. That observation does not prove other controller models,
reconnect, expression, clock synchronization, host/plugin routing or audio
hardware latency. See VALIDATION.md for exact regression evidence and limits.

Physical pedal testing exposed a triple-PC burst that narrower synthetic
single-message tests missed. Raw native sample timestamps confirmed the cause;
20ms identical-program/channel burst suppression repairs it. User verification
and native six-message/two-action telemetry now confirm stopped playback without
PCM deletion. This is a documented native PC extension, not an original JS parity
claim; existing Note/CC behavior and unchanged source comparisons are retained.

### Continuous MIDI loop mixer (2026-10-21)

In addition to the retained 16384 default-note/message comparisons,
MidiParity.cjs executes unchanged App.handleMIDIMessage plus original
UIManager.setLoopVolume/setLoopPan for 4256 continuous CC messages: all values,
all 16 channels, zeros, repeated positive and nonmonotonic expression movement.
Maximum native float metadata error is 5.913531686552176e-8 (limit 1e-7); pan's
original parseInt steps are exact. Existing committed sound gates remain at 1e-6.

The connected Engine test captures nonconstant distinct stereo PCM, then compares
sample-timed MIDI volume/pan with directly scheduled native controls. Output is
exact across 64/128 blocks, including mute-volume zero, opposite pan endpoints,
center steps and rapid expression changes. An explicit nonzero-signal assertion
prevents a silent test oracle. Actual JUCE MidiBuffer tests cover learn-zero,
performance without editor, state roundtrip and migration of old 33-slot state.

Continuous CC is restricted to the already implemented loop volume/pan sliders.
The original supports learning more effect/input/master/synth targets; those
remain unported. Native bindings reject overlapping CC button/slider ownership
instead of silently triggering transport or reassigning another target. This is
a documented safety improvement, not a claim that every original CC mapping is
ported. Physical expression response/listening, other controllers, DAWs and
non-Windows native UI remain separate acceptance work.

### Immediate movement after MIDI learn (2026-10-21)

A new same-block regression failed on b7c0992: CC 13 value 0 learned loop volume,
but value 127 one sample later was discarded until the message thread accepted
the mapping. The unchanged JavaScript learns from the first packet and applies
the next packet immediately. MidiParity now retains that actual handler/setter
comparison in addition to the 16384 default and 4256 assigned-CC comparisons.

An audio-owned provisional target now permits subsequent movement after one
bounded conflict check, without changing persisted configuration or allocating.
Only the learn packet is consumed; other assigned continuous controls still work.
Mode-only settings preserve pending movement and do not publish duplicate learn
results. Global cancellation and ownership changes clear the provisional target;
an overlapping channel-specific mapping cannot be stolen by an omni learn.
The existing exact stereo PCM test retains both prior assigned/direct cases and
adds provisional learning with no message-thread service, at 64/128 blocks.
Actual production MidiBuffer and GUI tests retain the two-event burst.

This repairs an original-behavior mismatch within the existing MIDI feature. No
new UI or feature/package was introduced, and the original source remains intact.
Browser automation-envelope identity and physical expression listening/response
remain separate acceptance work.

### Rapid MIDI enable/disable audit (2026-10-21)

The native button previously inverted its last audio snapshot. Two rapid clicks
could both request enabled=true, losing one gesture. A deterministic regression
using the actual frontend listener failed (actual true versus expected false,
build/midi-toggle-repro.log). The native message owner now resolves relative
toggles from the latest accepted settings. Existing explicit enable flags and
original interface markup remain intact; this is a repair to the native toggle
extension, not an original-browser toggle comparison.

Frontend tests retain double/triple clicks from both initial states without any
snapshot update. Actual processor tests enqueue two toggles before another audio
callback and inspect desired persisted state after each, then final audio state.
Native CDP mouse tests send press/release/press/release without waiting for IPC
and require both revisions and final state, at 64/128 blocks. Prior learning,
same-block expression, PC burst and no-editor tests remain in place.

The saved-state decimal-overflow probe passed on the current pinned JUCE build:
18446744073709551616 is rejected without losing mappings or partially replacing
state. It is retained as a regression; no parser bug or parser rewrite is claimed.
Physical expression verification remains deferred to the user's scheduled test.

### MIDI Escape/cancellation audit (2026-10-21)

Four related cases were reproduced and retained before repair: Escape before the
learn snapshot arrived was ignored; cancelLearn failed with all 31 configuration
entries occupied while callbacks were paused; a cancelled indicator remained
active until another callback; and a focused loop-name field blocked Escape
bubbling. Original App.handleKeyboardPress cancels MIDI learn before its text-
field guard; the unchanged method is now explicitly compared for range and text
inputs, with all prior MIDI/default/continuous/learn-burst comparisons retained.

The UI sends Escape cancellation in capture phase without requiring telemetry.
The RPC shares existing native editor-close/timeout cancellation fallback, and
displays accepted learn intent from the message owner. Actual processor tests
fill the queue without processing, cancel, resume, reject stale assignment and
successfully learn again; a separate assertion checks immediate paused status.
Frontend tests compose the production name-field and MIDI handlers with standard
capture/bubble ordering. Actual CDP tests retain right-click/immediate Escape and
focused-name cancellation at 64/128. This repairs existing behavior only; physical
expression verification remains deferred to the scheduled user check.

### Mapping-field focus and keyboard navigation (2026-10-21)

Unchanged KeyMapManager.handleInputFocus and handleInputBlur arm learning on focus
and cancel when leaving. The native click-only handler had no blur cancellation:
actual mouse focus loss left target 16 armed and able to learn a later packet
(`build/midi-field-focus-repro.log`). Native mapping fields now use focus/blur,
including Tab. Accepted completion releases focus; re-clicking an already-focused
field restarts learning before the next snapshot. Revision/reply ownership fences
prevent stale completion from blurring a newer field, and optional target-scoped
cancel requests cannot cancel a different active field/slider. Escape remains
unscoped and keeps the full-queue fallback. The original table is unchanged.

This repairs existing input behavior only; it adds no DSP feature. The original
note/CC mixer comparisons remain. Separately, Jorge's physical AudioBox USB CC27
volume response/display was confirmed after the focused-slider repair at a21f8b5;
that supersedes the earlier deferred-expression notes above for that setup only.
Calibration, audible performance and other drivers/controllers remain separate.

### Held loop note reassigned to a global mode (2026-10-21)

The original MIDI handler directly dispatches global modes before the loop press
map. Native code fixed new global presses at 2e8cc4f but still consulted a former
loop entry before resolving a changed binding. Clearing a held loop assignment,
then learning its key as OD/SUB/SUS, could therefore ignore the new mode command
until NoteOff or stale-entry recovery. The retained core regression reproduces
this (`build/midi-reassigned-mode-repro.log`). Routing now resolves the current
binding first and bypasses former held entries for global NoteOns only.

The unchanged original handler, press/release and global toggle methods verify
reassignment to each of OD/SUB/SUS. Native tests also preserve the former ongoing
recording through repeated mode presses and eventual old NoteOff, with exact
stereo WAV equality against direct capture at 64/128. Existing loop debounce,
10s recovery, CC edge and PC burst contracts remain. No alternative interaction,
new interface or next feature was introduced.


### LIVE loop mixer control projection (23 October 2026)

The unchanged original MasterMixManager.renderLive is the markup oracle. Native
volume/M/S use existing C++ commands and settled audio paths, rather than any
legacy inline audio code. Inline handlers are stripped before listeners attach.
All twenty loop strips retain original range (0..2, step .01), wheel step (.05),
labels, styles and mute/solo colours; only ported loop strips are enabled.
Original renderer inputs/drone/sampler arrays are empty in this projection and
the unported master strip is removed before paint.

Current original UIManager.renderMeter consumes RMS and held-peak objects, while
native track telemetry exposes a block sample peak. This increment deliberately
leaves the original meter placeholders empty with aria-disabled and an explicit
unavailable title. It does not mislabel block peak as RMS or claim full mixer
parity. Master-mix routing, metering, additional engines and live-slider learning
remain independent acceptance work. Original assets and all DSP are unchanged.

Official JUCE guidance was rechecked: SmoothedValue describes avoiding audio
glitches, and AudioProcessorValueTreeState explicitly distinguishes realtime raw
parameter reads from locking state copies. Existing native gain smoothing and
command ownership are reused; no state copy is added to the callback. Sources:
[SmoothedValue](https://docs.juce.com/master/classjuce_1_1SmoothedValue.html),
[AudioProcessorValueTreeState](https://docs.juce.com/master/classjuce_1_1AudioProcessorValueTreeState.html).


### LIVE volume MIDI Learn aliases (23 October 2026)

The original App.setupTouchSafety parses UIManager.setLoopVolume from the LIVE
fader's inline oninput and learns loop_vol with the same loop ID and 0..2 range.
The native DOM deliberately strips inline handlers; its explicit slider-ID
resolver omitted live_mm_slider_l_N, making that original gesture unavailable.
The new regression reproduces rejection (-1 instead of target 33) before repair
(`build/live-midi-before.log`). Native LIVE IDs now alias the existing 33..52
targets, without changing binding ownership or automatically editing assignments.
Twenty unchanged original context-menu invocations verify this routing. Shared
status is tested by executing the actual native renderer in 720 track/CC/channel/
learn/bound combinations, checking main/LIVE parity and immutable mapping state.

Official JUCE MidiMessage documentation confirms controller values span 0..127;
zero is a valid value, not a button-release filter for expression learning. The
existing native MIDI engine already preserves this behavior; no callback change
is needed. [JUCE MidiMessage](https://docs.juce.com/develop/classjuce_1_1MidiMessage.html).
Physical controller/driver behavior for these newly enabled aliases has not been
newly observed; synthetic MidiBuffer tests do not constitute hardware evidence.


### Requested direct transport MIDI learning (23 October 2026)

Right-click learning on primary action buttons is an explicitly requested native
MIDI UI extension. The unchanged JS reference supplies the mapping-table/trigger
behavior; it does not provide a direct-action context-menu oracle. Existing core
original-note/press/learn/audio comparisons therefore remain acceptance gates,
while the new frontend tests check the direct gesture adapter independently.

AionMidiLearnTarget maps loop-action-0..19 to existing 0..19 targets and retains
the 60 main/LIVE expression aliases through AionMidiSliderTarget. Unsupported
controls stay excluded. Tests execute the actual production context-menu listener
across 320 target/modifier/disabled cases, and the actual mapping/button status
renderer across 640 target/kind/number/channel/learning cases, preserving binding
state. Note, CC and PC classification/values follow the existing JUCE MidiMessage
contract ([official source](https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_basics/midi/juce_MidiMessage.h));
no audio callback or event parsing change is necessary.

The MIDI test restores an assigned nonmaximum volume before its retained disabled
expression check. New LIVE fixtures had cleared the former assignment and ended
at maximum volume, which made that final legacy assertion ineffective. This is a
fixture repair, with no change to product disable behavior or tolerance.


### LIVE header direct MIDI Learn (23 October 2026)

The unchanged generateLiveLoopHTML owns each LIVE header and its short-action
behavior; original markup/updater tests remain unchanged. The reference does not
provide the requested direct-button/header context-menu learning gesture, so its
acceptance is tested separately from original audio/trigger parity. Native LIVE
headers alias existing 0..19 mappings; no new event parser, callback or state
schema is introduced. Existing Note/CC/PC interpretation follows the official
[JUCE MidiMessage contract](https://docs.juce.com/master/classjuce_1_1MidiMessage.html).

The pre-fix frontend regression reproduces the missing element routing
(`build/live-action-midi-before.log`). Tests invoke the production context-menu
listener for all twenty loops, seven header/child/FX/card hit categories, both
modifiers and enabled/disabled actions (560 cases), plus invalid owners. The
existing 640 immutable mapping/action-status cases also verify LIVE outlines.
Original main-button, slider, markup, gesture and audio comparisons remain.


### Main loop-header/state direct MIDI Learn (23 October 2026)

The original main-header short action and native nested-control exclusion define
the eligible surface. The requested direct MIDI learning gesture itself has no
unchanged-JS context-menu oracle. It aliases existing 0..19 primary bindings,
retaining the original markup, main action/held-note DSP and original MIDI gates.
Note/CC/PC parsing remains the existing C++ implementation following the official
[JUCE MidiMessage contract](https://docs.juce.com/master/classjuce_1_1MidiMessage.html).

The pre-fix listener regression reproduces rejected main-header learning
(`build/main-header-midi-before.log`). Actual production listener tests cover
20 loops, 19 header/state/nested-control/card hit categories, both modifiers and
enabled/disabled actions (1,520 cases), plus invalid owners. Prior 320/560 routing
and 640 immutable status cases remain, with main-header outline coverage added.


### Cancelled MIDI Note ownership recovery (23 October 2026)

Unchanged LoopManager.stopAll clears state.inputPressTimers. Executing it together
with original App.handleMIDIMessage/handleInputPress/handleInputRelease accepts an
immediate new NoteOn on all sixteen channels, while debouncing active repeats.
The native engine kept cancelled entries active/suppressed, blocking that note
until NoteOff or ten seconds. `build/midi-panic-before.log` reproduces the native
failure; `build/midi-panic-oracle-before.log` records the unchanged-JS mismatch.
The new fixture's initial mixed-width auto declaration was corrected before this
behavioral reproduction; its failed build log is retained separately.

A first same-key-only repair passed its narrower tests but failed the added full
64-Note-pool/different-key regression (`build/midi-panic-capacity-before.log`).
The final implementation retires Note entries during cancellation after restoring
momentary actions. Suppressed CC owners stay until zero rearms them. Event parsing
and velocity-zero release handling retain the existing official
[JUCE MidiMessage contract](https://docs.juce.com/master/classjuce_1_1MidiMessage.html).
No new storage, callback allocation/lock, binding schema or UI is introduced.

The original-JS oracle covers MIDI Stop; native CC120/123, device cancellation,
overflow and disable/enable behavior are existing native extensions tested
separately. Tests retain ordinary ten-second missed-release rules for active
notes, PC burst filtering, CC rearm, exact nonzero stored stereo WAV and original
source/gesture/audio comparisons. Earlier validation entries describing retained
cancelled Note entries document the historical implementation superseded here.

### Space STOP ALL repeat parity (23 October 2026)

Unchanged App.handleKeyboardPress handles the default physical Space fallback
before its generic repeat guard. The native adapter checked repeat first, so a
held Space could stop once and then leave later MIDI-triggered playback running.
Both the unchanged-dispatcher comparison and actual WebView2/synthetic-audio
test reproduced the failure before repair (`build/space-repeat-before.log` and
`build/space-repeat-gui-before.log`). Move only Space routing before that guard,
retaining Ctrl/Meta/Alt and text/editable-content protection. This uses the
existing STOP ALL path, without changing UI, DSP, queues or MIDI assignments.

The browser contract is documented in MDN's
[KeyboardEvent key sequence](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key):
holding a key emits further keydown events with repeat set, followed by keyup.
The unchanged original dispatcher is the behavior oracle; the documentation
explains event delivery, not a new alternative product policy. Custom keyboard
mappings and keyboard SUS remain separate, unported features.

### Independent MUTE controller coverage (24 October 2026)

The unchanged reference LoopManager.toggleMute action supplies a 288-packet
state oracle across Note/CC/PC and 16 channels. Native right-click access on main
MUTE/UNM and LIVE M is the requested controller extension, not a claim that the
original browser had this exact context-menu route. 240 real-PCM/immutable-WAV
cases cover 20 loops, three message kinds, channels 1/16 and 64/128 blocks.
Sample-offset comparisons against the existing queued mute action use 1e-7
stereo tolerance; capture tests preserve exact nonzero recorded WAV samples.
New browser transient-envelope equivalence is outside this increment's scope.

Red evidence in build/midi-mute-{before,frontend-before,reload-before,held-before,
outline-before}.log records unsupported targets, alias routing, initial UNM
binding, held SUS cancellation on Learn acknowledgement and erased outlines.
Repairs scope cancellation to changed MUTE owners, retain existing latched DSP,
bind both captions and preserve outlines through snapshot colour refresh.
Version 3 retains legacy 33-/73-binding load contracts.

### Actual host MIDI acceptance boundary (24 October 2026)

No new browser DSP implementation was introduced. tests/ReaperMidiProbe.lua and
TestReaper.py exercise the already-migrated capture/playback/mute through real
host MIDI items. Existing unchanged-JS mute and native PCM oracles remain intact.
Original assets, native UI/DSP, APVTS IDs and hardware assignments are untouched.
The only CMake change registers the portable reaper_midi_fixture test.

Incremental REAPER MIDI_InsertEvt insertion repaired partially constructed notes:
it inserted releases, so a supposed repeated held Note became two genuine taps.
The first muted-Note failure is retained, not a native debounce regression.
Complete sorted MIDI_SetAllEvts publication preserves the repeated raw NoteOns;
strict byte-for-byte readback catches host changes. An empty end record was
rewritten as CC123; the fixture now supplies the real terminator at the intended
item end. CountEvts describes grouped notes rather than raw NoteOn count.

Actual CLAP 1x renders pass Note/CC/PC recording/play/mute/recovery and assignment
recall. Actual VST3 Note/CC pass; its PC render remains audible during requested
mute and therefore fails. That is a host-format compatibility failure, not a new
original-JS parity pass. Full-speed offline finalization also remains unresolved:
a rapid render after project reopen outran background take commit. Retain this
evidence alongside the paced 1x scope; do not relax the silence, sample phase or
nonzero-density assertions to hide it. Physical controllers, driver latency,
recorded-loop projects and Ardour MIDI routes remain separate acceptance work.

### VST3 PC bridge and host-period oracle (24 October 2026)

No original JS audio or visual assets changed. The wrapper now translates the
VST3 ProgramList/parameter protocol into the same existing native MIDI dispatch.
The 4096 portable cases cover sixteen channels, 128 programs and 64/128 blocks,
with wrong-channel rejection, repeated PC pulses, actual silence/recovery,
unchanged clip/history and tracked zero callback new/delete. Production JUCE
checks cover APVTS ID/preset isolation and atomic overflow block rejection/recovery.

The added real-host PC127 record/finish cases initially failed the older phase
oracle's assumption of exactly 150ms/7200 frames. Retained PCM identifies one
exact pre-mute period of 7168 frames in this REAPER VST3 run; comparing four actual
cycles gives error zero. That is a host timing distinction, not evidence of a DSP
mute phase bug. The original Note36 capture cases retain the old fixed-period
comparison. Added PC captures now identify exactly one period from pre-mute PCM
alone and compare 6720 stereo samples four actual cycles later at the same PCM
tolerance. CLAP must still record 7200 frames. Ambiguous-period and phase/pan/
silence/leakage/mapping corruption controls must fail. No recovered signal is
used to choose the expected period and no error tolerance was enlarged.

The initial failure is retained in build/vst3-pc-reaper-matrix.log and its run
folder. Real-host passing results supersede the earlier missing-PC compatibility
gap only for their documented formats/host/scope; raw hardware and sample-accurate
REAPER PC timing, unpaced offline recording and Apple/other-host acceptance remain
separate. The generated-source guard also preserves upstream bytes/license and
rejects drift under optimized Python; it does not replace ABI/DAW validators.


Offline completion is a native host contract, not original-browser scheduling
parity. The explicit user-approved exception waits only for published worker jobs
in a host-declared offline callback. Keep exact whole-render block-partition PCM
and simultaneous MIDI handoff tests alongside the unchanged-JS live/state/DSP
gates. Final 126-case committed capture/OD/SUB center-pan sound audit remains at
maximum 7.394757495227111e-8 under 1e-6. Real REAPER full-speed and retained 1x
Note/CC/PC silence/recovery/phase/reopen matrices pass; none of those claims
extends to full browser envelopes, physical timing, other hosts or Apple builds.

### Meter graph creation precedes queued source playback (27 October 2026)

`tests/MeterSourceReference.cjs` now executes the unchanged `LoopManager.handleAction`
and `AudioEngine.resume`, in addition to its retained 136 Loop/AudioGraph/input-gate
checks. Fourteen scheduling/cancellation cases and twenty-two resume cases cover
tracks 0 and 19 with controlled timer/context IO. This characterizes the reference;
it does not validate a native lifecycle producer or browser PCM.

For a stopped loop with sync enabled, the dispatcher first sets `queued` without
creating a graph. Its timeout is `max(0, (gridTime-currentTime)*1000-100)`.
When that callback runs, `play(gridTime)` builds a fresh graph/analyser immediately,
sets `playing`, and schedules the source at `max(gridTime,currentTime)`. Therefore
graph creation may precede audible source playback by 100ms. A near grid schedules
a zero-delay callback; a late callback starts at the current context time and
computes the corresponding phase. The tests exercise all three paths, including
the actual source start time/offset and main-loop analyser input gate.

A second queued short press, explicit stop, clear or STOP ALL cancels the queued
timer before graph creation. Immediate play waits for the original context resume
method. Failed resume, changed play request, changed transport generation, or a
removed buffer must not create a graph after that await. The dispatcher additionally
rejects a replaced loop slot after resume. Successful play followed by another
play while playing retains the same graph/analyser/data objects.

Native mapping must represent graph creation and scheduled source start as distinct
facts. Do not create an analyser from a `LoopView::state` transition, sourceRevision,
clip replacement, or the raw-window reset receipt alone. In particular, moving
audible playback 100ms earlier is not a fix. Cancellation, accepted buffer ownership,
queued graph lead, delayed stop cleanup and retained healthy retrigger identity
must be handled together before exposing meter readings. Current production meter
owners still lack that complete producer; the renderer remains disabled.

The external comparison inspected
[SooperLooper's engine declarations](https://github.com/essej/sooperlooper/blob/master/src/engine.hpp),
which separate RT event, non-RT update, and loop-management queues, and the
[Freewheeling repository](https://github.com/free-wheeling/freewheeling).
The useful architectural precedent is explicit owner-to-owner event transfer;
it cannot determine this application's JS analyser lifetime. No third-party source
was copied and no dependency changed. Three isolated build-directory mutations
(grid-only birth, missing play-request guard, missing transport guard) each fail
the expanded reference gate. Their logs are under build/meter-scheduling-mutations.
