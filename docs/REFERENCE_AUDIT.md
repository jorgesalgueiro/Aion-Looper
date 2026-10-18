# JavaScript/native parity audit — 2026-10-18

**Verdict: full functionality and sound parity are NOT achieved.** The preserved
interface is not evidence of equivalent DSP. Do not call the current application
production-ready or a complete Ascii-Looper replacement.

## Sources and method

Studied the unchanged `resources/ui/Modules/loopTracks.js` classes Loop,
LoopManager, SoloManager and UIManager; `audioEngine.js` capture-boundary and
circular mix routines; `main.js` input/keyboard/recording state; and the project,
tracker, synth and effect module entry points against the C++/bridge surfaces.
The checked-in runtime bytes are the same as `.reference/Ascii-Looper` where that
checkout is available. Asset identity remains separately tested.

`tests/ReferenceParity.cjs` executes the actual unchanged loop module in a Node
VM. Browser DOM and audio IO are stubbed; the tested setters, clear, mute, solo
and effective-volume getter are real reference methods. The C++ probe applies
the same seeded traces and reports native state. 408 traces compare 8,160 loop
snapshots, including fixed-seed randomized controls and solo/clear interactions.
This proves only that subset of valid manual-mode state semantics. Seeded audio
is fixture setup, not a test of the original recording/playback implementations.
It does not validate rendering, MIDI, host automation or sample output.

## Confirmed mismatches corrected in 0.4.1

- `Loop.clear()` preserves explicit mute; native clear had reset it. The new JS
  differential test fails before the correction and passes afterward.
- `UIManager.setLoopPan()` truncates numeric values using parseInt. Native pan
  now follows this for valid in-range input; nonfinite/out-of-range IPC remains
  rejected. A fractional pan fixture compares the actual reference method.
- Reference LoopManager owns one recording/overdub/substitute capture at a time.
  Native core now rejects overlapping capture entry. Pressing another empty
  card finishes the current capture; it requires a later press to start that
  empty card. All 20 slots can still play together. New capture concurrency is
  covered by C++ contract tests; this is not a browser capture integration test.
- A zero-frame FINISH now releases capture into empty state, preventing a stranded
  recorder when RECORD/FINISH arrive at the same sample boundary.
- DEL ALL now uses the original confirmation text and sends no command on cancel.
- STOP ALL cancels keyboard hold timers and unsent slider changes. Stop remains
  available while another command awaits acknowledgement, and a stop requested
  during that wait receives priority when the command completes.

## Sound comparison: a real failing acceptance gate

`tests/AudioParityAudit.cjs` executes the unchanged
`AudioEngine.mixBuffersCircular()` against a native core overdub probe. The
512-frame fixture (base 0.2, overdub 0.1, feedback 0.8, 48 kHz, center pan)
finds **102 differing frames**, maximum absolute error **0.0424264**. This is
an isolated circular-mix comparison: capture DC correction, capture crossfades,
native fades and browser rendering are excluded. It is not a listening test.
The diagnostic exits 1 on this mismatch and is deliberately not represented by
passing state CTest results.

The reference captures overdub separately, removes DC, fades its clip boundaries,
then mixes on FINISH with tapered feedback/new-signal gains and soft clipping.
Native currently writes feedback directly into PCM while capturing. This changes
when layers become audible, boundary samples and clipping. Native recording also
lacks the reference full-clip DC removal and zero-crossing boundary processing.
Native gain ramps/rate resampling differ from browser DSP.
The preview also serializes UI commands while waiting for acknowledgements; very
rapid multi-button/key gestures are not yet proven equivalent to the reference. These need a bounded,
preallocated capture/finalization transaction before a faithful port is possible;
do not add a full-buffer allocation or unbounded scan to processBlock as a fix.

## Remaining feature audit

| Reference area | Native status / acceptance gap |
| --- | --- |
| Manual loop controls, pan, speed, mute/solo | Partial; state differential and core timing tests pass; sound differences above remain |
| Global SUS, stutter, multiply, start offsets, merge, normalize, names | Unported |
| Undo/redo, WAV import/export, master/input recording | Unported |
| ALP audio/project persistence | Unported; native state saves parameters only and PCM is temporary |
| Static/dynamic sync, count-in, cascade, metronome, latency correction | Unported; native sync remains OFF |
| Drone synth, sampler, tracker, effects/presets | Unported; no matching C++ implementations exist |
| MIDI learn, CC/note/PC, expression, gamepad, keyboard remapping | Unported; only fixed native keyboard gestures and MIDI pass-through exist |
| Host transport / MIDI clock | Host observation only; complete clock/seek/loop behavior unported |
| Input routing/mixing | Basic native stereo/monitor; reference multi-source mixing not ported |
| Platform/driver/host acceptance | Windows binaries and WebView2 smoke tests; physical ASIO/Topping, DAW validators and Apple builds pending |

## Reproduce

Build the core with Node available before configuring. `ctest` then includes
`js_native_state_parity` alongside asset identity and realtime core contracts.
For Windows use the existing `build/windows-core/Release/aion_reference_probe.exe`;
for Ubuntu use `build/core/aion_reference_probe`.

```
node tests/ReferenceParity.cjs build/core/aion_reference_probe
node tests/AudioParityAudit.cjs build/core/aion_reference_probe
```

The second command currently MUST fail with parity=false; removing it or relaxing
its tolerance would not fix sound parity. Native probe formatting and VM browser
stubs are test-only and are not linked into the app. Core allocation tests cover
C++ new/delete, not every allocator in JUCE/driver/browser code.
