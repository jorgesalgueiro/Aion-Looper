# Aion-Looper — native JUCE restart

This is the **architecture/build foundation**, not a completed audio port or a commercial release.
The previous Rust migration is intentionally not restored. See [ARCHITECTURE](docs/ARCHITECTURE.md)
for the module blueprint, [BUILDING](docs/BUILDING.md) for exact commands,
and [SDKS](docs/SDKS.md) for the SDK inventory.

- JUCE **9.0.4**, pinned to `3fb5cbc0340758596dac39f7c8de47d562b4896e`.
- Standalone + VST3 + CLAP on desktop; native JUCE LV2 on Linux; AU on macOS;
  AUv3 + Standalone on iOS via Xcode. Optional macOS AUv3 also uses Xcode.
- Original reference runtime HTML/CSS/JS/icon/AFX assets are preserved byte-for-byte
  in `resources/ui`, with a checked SHA256 manifest and deterministic embedded archive.
- The first editor is a **read-only reference preview**. Legacy scripts, inline audio
  handlers and CDN loading are blocked. Dynamic loop/synth panels are not initialized yet.
  This deliberately prevents a second Web Audio engine from running behind the C++ processor.
- Current native DSP is a muted-by-default stereo pass-through with smoothed master gain,
  finite-value protection, sample-timestamped test commands and host tempo/PPQ observation.
- `masterGain` is backed by APVTS, and the native browser RPC validates `hello` and
  `setParameter`. Hardware MIDI learn, looper/synth/FX parity and full clock sync are **pending**.

Quick core test (no JUCE download):

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

Do not distribute this preview as a finished instrument. See [PARITY](docs/PARITY.md)
and [LICENSING](docs/LICENSING.md). Dependencies, reference checkouts, build caches,
audio recordings and user projects are excluded from Git.
