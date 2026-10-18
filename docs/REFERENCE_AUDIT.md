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
| Timing | Static/dynamic sync, count-in, cascade, metronome and MIDI/host clock unported |
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
