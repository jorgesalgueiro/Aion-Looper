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
  and reports the reset. Export, undo, projects, synths, tracker, effects, MIDI learn
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
