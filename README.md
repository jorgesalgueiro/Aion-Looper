# Aion-Looper — native JUCE migration

Version 0.4.2 is a **native loop preview**, not a completed port or commercial release.
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

The [reference audit](docs/REFERENCE_AUDIT.md) now records verified state parity
and remaining differences. Actual unchanged JS methods are compared to native
state in CTest when Node is available. The separate audio diagnostic currently
fails: native overdub boundary processing and capture finalization do not yet
match reference sound. Passing state/UI tests must not be presented as DSP parity.

Feature progression is now frozen on capture/overdub repair under `AGENTS.md`.
Offline finalization kernels have unchanged-JS sample tests, but the native looper
integration still fails its audio acceptance check. The capture-quality CI job
keeps that gate visible; passing component tests do not authorize the next feature.

Capture repair in 0.4.2: FINISH publishes SAVING while a worker applies the
reference crossfade/DC/mixing path to a private result. Old PCM stays unchanged
while editing and on processing failure. Only the audio owner commits completed
clips at block boundaries; worker-only retirement keeps deletion off the callback.
The connected manual PCM comparison now passes 126 mono/stereo cases / 965,826
frames (tolerance 1e-6). The earlier failed raw-mixer fixture remains in the kernel
suite. These latest results supersede the historical failing-gate notes above.
Full browser envelopes/resampling, latency/sync, listening/hardware and remaining
features are still outside this result. No other feature was added in this repair.
