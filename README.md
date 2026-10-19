# Aion-Looper — native JUCE migration

Version 0.4.3 is a **native loop preview**, not a completed port or commercial release.
See [ARCHITECTURE](docs/ARCHITECTURE.md), [BUILDING](docs/BUILDING.md),
[SDKS](docs/SDKS.md) and [PARITY](docs/PARITY.md).

- JUCE 9.0.4 and CLAP extension sources are pinned. CMake targets cover Standalone,
  VST3, CLAP, LV2, AU and AUv3 on supported platforms.
- Twenty stereo loop slots now record, finish into playback, stop/restart, overdub,
  substitute, mute, adjust volume/feedback and clear through the C++ engine.
- Capture uses a 64 MiB fixed-block pool allocated before processing; immutable
  clips and worker finalization also use memory outside the callback. Capture,
  editing and playback do not allocate/free in the callback. At 48 kHz this pool
  holds about 175 seconds across all tracks. Full memory finishes recording without
  overwriting another loop, and clear makes its blocks reusable.
- Original reference files in `resources/ui` remain byte-for-byte intact. The original
  pure loop-card markup methods are reused in a separate native adapter. Inline
  handlers and legacy audio scripts remain blocked; supported controls use native RPC.
- Output starts muted and input monitoring is off. Standalone input also has JUCE's
  Unmute Input control; choose devices/ASIO and buffer size in the window's Options.
- **Recorded audio is temporary and is not included in DAW/project state yet.**
  Same-rate reprepare retains PCM and stops loops; a sample-rate change clears PCM
  and reports the reset. Export, projects, synths, tracker, effects, MIDI learn
  and full host/MIDI clock synchronization remain pending.

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
node tests/FrontendTests.cjs
```

The native loop tests cover stereo PCM, feedback/substitution, pool exhaustion/reuse,
20 tracks, gain transitions, sample deadlines, 64/128 block invariance, lifecycle
resets and no observed callback C++ new/delete. See [VALIDATION](docs/VALIDATION.md)
for platform and browser checks and their limits. See [LICENSING](docs/LICENSING.md)
before distribution. SDK caches, reference checkouts and build/user data are excluded
from Git. The prior Rust migration remains aborted.

The original layout is the UI contract. Native controls now occupy the existing
PROJECT MENU, MASTER OUTPUT, INPUT MIXER and INPUT MAPPING areas; the separate
preview toolbar has been removed. Original workspace/manual tabs and loop mode
shortcuts are restored. SOLO, manual RTRG, DEL ALL, continuous volume/feedback and
Auto-play after Rec now reach the C++ engine. Full reference parity remains work
in progress; unported controls stay visible and disabled.

Loop pan and varispeed now use the original controls: 0-10 pan, 0.25x-2x speed,
1/2 toggle, 1x reset, double-click speed reset and H on the selected loop. Pan
matches the reference mono-fold/equal-power stage; raw captured stereo PCM stays
intact. Pan/rate changes ramp over 20 ms. DEL resets loop controls to their
reference defaults. Overdub/substitute require unity speed, matching the reference.
The current resampler uses linear interpolation; high-quality antialiasing and
browser-rendered audio comparison remain release gates.

The [reference audit](docs/REFERENCE_AUDIT.md) records actual unchanged-JS state
and DSP comparisons and their limits. Capture/overdub/substitute finalization now
passes its connected manual PCM gate: 126 mono/stereo cases / 965,826 frames at
1e-6 tolerance. FINISH publishes SAVING while a worker applies reference
crossfade/DC/mixing to a private result. Failed edits preserve the original PCM;
block-boundary ownership transfer and worker retirement keep scans/allocation/
deletion off the audio callback. This establishes committed-PCM parity for that
path; browser envelopes/resampling, latency/sync and hardware remain unverified.

0.4.3 adds the original NORM action: linked stereo peak normalization runs as an
immutable worker transaction, preserving playback phase, volume/pan/rate/mute,
and stopped/playing intent. Clear/reset cancel the result; allocation failure
keeps the prior clip. NORM is unavailable during capture or another pending edit.
The original near-silence and near-unity thresholds are retained. It does not add
undo or persistent audio storage. Rapid performance commands now queue in order;
Stop cancels unsent work and survives a pause in the same audio configuration.
Unsent ordinary actions expire after three seconds, and sliders coalesce their
pending values. The original UI files remain unchanged.


Development follow-up: the original **Rvers** checkbox now reaches C++. As in the
reference, it changes direction on the next play/retrigger and leaves the current
source alone. PCM storage stays forward; reversed playback reads the immutable
clip in reverse order without allocating a reversed copy in the callback. If NORM
changes the stored clip during reversed playback, the old source is retained
until restart/stop, matching the reference's independent reversed buffer. Other
effect controls remain unported. This source build extends 0.4.3; old ZIPs do not
contain these follow-ups. Only the incremental standalone test build is updated.


Development follow-up: original per-loop and global **UNDO/REDO** buttons now
restore overdub/substitute checkpoints in C++, with Ctrl/Cmd+Z and Shift+Z redo.
The five-snapshot loop limit and 50-entry global order follow the reference.
NORM does not create a checkpoint. Undo/Redo preserve controls and stopped state;
playing loops restart with the restored PCM. Failed/canceled edits preserve prior
history. Restoration is available on stable playing/stopped tracks; the same
track's capture/finalization temporarily disables it. Audio/history remain
session-only, and retained snapshots add memory beyond the capture pool. No new
package is produced; use the current incremental standalone build for testing.

Development follow-up: the original **MULT** button and `\` shortcut now extend
playing loops with captured input, or double a stopped loop / a playing loop with
no native input source. Takes shorter than 128 frames are discarded. The native
worker applies the original DC removal and splice fade before appending; Undo/Redo
restore the original lengths and PCM. Like JS, MULT adds local undo history rather
than a new global entry. Stop finishes without late autoplay, and failed/canceled
jobs preserve existing audio. Growth and longer history restores must fit the
current session audio budget. No new ZIP or version is made; use the existing
incremental standalone test app. Persistence and full application parity remain
pending.

Development follow-up: the main **arm / record / play** workflow now uses a
native sample clock, with the original OFF / STATIC / DYNAMIC controls, BPM,
bars, meter fields, independent VISUAL/SOUND count-in, timeline and main loop buttons.

- Choose the audio device/input in Options and Unmute Input, then raise MASTER
  VOLUME to hear playback. A missing native input now reports an explicit error.
- Click a loop's main button, or use 1–0 (Shift for loops 11–20). Mouse down and
  key down use the same action path; release cannot duplicate the action.
- STATIC arms for the shared grid, records the fixed length and optionally plays.
  CANCEL disarms it. A stopped loop queues PLAY; pressing CANCEL prevents its start.
- DYNAMIC records immediately. FINISH normally waits for the next bar; a second
  FINISH stops now. As in the reference, the last 50ms of a bar finish immediately
  and short takes are retained rather than padded to a whole bar.
- A synced short STOP waits for the grid; another STOP cuts now. A 600ms hold
  cuts without deleting. Keep holding STOP for 4 seconds to delete (see below).
  Space / STOP ALL cancels queued starts and finishes capture without late autoplay.

The browser timer receiver bug that stranded held keys/pointers is fixed, with
real WebView2 press/release/hold regressions. This increment uses the internal
Master BPM grid. Continuous metronome and static SOUND count-in render in C++,
bypass MASTER and stay out of internal loop capture. SOUND plays 100ms triangle
cues in the final bar (440 Hz, then 880 Hz on the final beat). VISUAL and SOUND
independently request the same lead-in; CANCEL, DEL and STOP ALL cancel pending
cues. OFF and DYNAMIC start immediately. Loop-source sync, DAW/MIDI clock following,
latency calibration and pre-tap snapping remain pending. Recording
settings are latched for the active take. Audio/projects are still session-only.
Only the current standalone test app is rebuilt; no new ZIP or version is made.


Mouse performance: the original loop header/state text also triggers the main
short action, as in Ascii-Looper. The main button supports press/release outside
its bounds without duplicating the action; clicks on nested controls or the
waveform do not toggle recording. Hardware-free real-mouse/native-PCM acceptance
instructions are in [BUILDING](docs/BUILDING.md#hardware-free-mouse-acceptance-windows).

Hold STOP to delete (intentional): continuously hold a playing loop's main STOP
button or assigned number key for **4 seconds**, measured from the initial press.
The tap stops/schedules stopping; 600ms cuts playback but keeps audio. Only the
4-second hold deletes the loop and its Undo/Redo history, regardless of sync mode.
Releasing earlier, losing focus/pointer capture or STOP ALL cancels deletion.
PLAY/FINISH/CANCEL/RECORD holds never delete. DEL deletes directly. The in-app
English, Spanish and Portuguese manuals explain these rules.


The original **Start** slider now adjusts playback phase. Drag to choose the
value and release to apply it 50ms later during playback; STOP ALL cancels pending
restarts and preserves the value, while DEL resets it. Q/nudge remain pending.
