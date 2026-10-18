# JavaScript/native parity audit — 2026-10-18

**0.4.2: committed manual capture/overdub/substitute PCM now passes its reference
comparison. Full application sound/functionality parity is NOT established.**
No other feature was added during this repair. Hardware/listening and the remaining
items below are still acceptance work; the original interface remains the contract.

## Reference and comparison scope

The unchanged resources/ui loop, audio, main/input, project, tracker, synth and
effect sources define the target. All 16 runtime files remain byte/SHA identical
to the preserved reference. State setters/clear/mute/solo execute from the actual
JS classes in a Node VM; browser/audio IO is stubbed, not the tested calculations.
408 traces compare 8,160 native loop snapshots. Seeded state is fixture setup,
not a reference browser recording integration test.

The offline DSP test executes unchanged crossfade/circular-mix functions plus the
actual recorded-loop DC-removal block. It covers 472 cases / 396,284 samples with
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
| Performance gestures | Rapid multi-key/button acknowledgement behavior needs wider integration coverage |
| Loop features | SUS, stutter, multiply, start offsets, merge, normalize, names, undo/redo, WAV import/export |
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
