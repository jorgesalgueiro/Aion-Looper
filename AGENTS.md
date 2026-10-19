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
workflow with unified mouse/keyboard gestures. Offline DSP and connected
committed-PCM gates pass for their documented scope. Full browser/driver parity
is not established. Each subsequent feature must independently pass the gates
below before another feature starts; keep any newly failing feature under repair.

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
