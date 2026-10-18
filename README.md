# Aion-Looper — native JUCE migration

Version 0.2.0 is a **native loop preview**, not a completed port or commercial release.
See [ARCHITECTURE](docs/ARCHITECTURE.md), [BUILDING](docs/BUILDING.md),
[SDKS](docs/SDKS.md) and [PARITY](docs/PARITY.md).

- JUCE 9.0.4 and CLAP extension sources are pinned. CMake targets cover Standalone,
  VST3, CLAP, LV2, AU and AUv3 on supported platforms.
- Twenty stereo loop slots now record, finish into playback, stop/restart, overdub,
  substitute, mute, adjust volume/feedback and clear through the C++ engine.
- Audio uses a shared 64 MiB fixed-block pool allocated before processing. Capture,
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
