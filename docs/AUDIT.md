# Migration code audit — 2026-10-16

Scope: the existing Rust workspace and desktop/native bridge, build scripts,
platform CI and actual Windows/Ubuntu builds. This is a migration checkpoint,
not a claim of complete audio parity or a finished multi-platform instrument.

## Findings and changes

- **Linux startup crash (fixed):** `run()` constructed `WebContext` before Tao
  initialized GTK. Running the Linux release executable reproduced a panic in
  WebKitGTK's `ApplicationInfo`. Linux now calls `gtk::init()` before constructing
  the context, propagating initialization errors through the existing result.
- **Linux window content attachment (fixed):** the WebView was added directly
  to a GTK window that already contained Tao's content box. It now attaches to
  `default_vbox()`, matching the pinned Wry examples and eliminating the warning.
- **Wrong Windows CLAP artifact with a custom target directory (fixed):**
  `build-clap.ps1` built using Cargo's target directory but always packaged
  `target/release/looper_clap.dll`. It now resolves `CARGO_TARGET_DIR`, including
  relative paths, before copying the DLL. The desktop packager already did so.
- **Linux package absent (implemented):** `build-linux.sh` produces the Linux
  desktop, JACK-enabled CLI and CLAP prototype, includes project and bundled
  dependency licenses, and tests the packaged shared library through its actual
  CLAP entry point. CI now runs this packaging step on Linux.
- **Reproducible Ubuntu audit (implemented):** `check-linux.sh` checks formatting,
  strict Clippy, all Rust tests, JACK compilation, bridge tests and original UI
  tests. Linux build artifacts are kept apart from Windows artifacts.
- **iOS coverage:** the pinned Wry/Tao dependencies do support iOS. Retained the
  full workspace compile check and added `--locked`; compilation alone does not
  provide an installable app, audio session setup, signing or device validation.

## Validation

Ubuntu 24.04 under WSL2, Rust 1.99.1:

- Full workspace: **131 Rust tests passed**, including allocation checks, DSP
  parity, WAV extensible layouts, capture continuity and CLAP state concurrency.
- Strict Clippy (`--workspace --all-targets -- -D warnings`) and formatting passed.
- JACK compilation passed for both CLI and desktop.
- **77 bridge tests and 600 original UI tests passed.**
- Linux release package built. Packaged CLAP ABI smoke passed: stereo capture and
  playback, in-place buffers, isolated instances, reactivation, reset, saved state
  and active-load restart.
- The Linux graphical integration suite passed **130 checks**, using synthetic
  input and muted output: WAV/native DSP, recording, undo/redo, project save/load
  and MIDI lifecycle. GTK startup and content attachment fixes are verified.
  WebKit returned empty audio for a seven-frame PCM24 fixture; repeating the
  same pattern over a longer buffer preserves strict sample comparisons and
  passes. Native decoder behavior was unchanged.
- Native Linux device enumeration completed without starting capture or playback.
- Windows GNU full workspace tests and strict Clippy passed, including the new extensible WAV worker
  test. The relocated caches emitted stale-path incremental warnings; assertions
  passed. The existing packaged Windows executable has not been rebuilt here.

Complete Linux logs are in `verification/linux-audit.log`, `linux-build.log` and
`linux-devices.log`. Desktop integration status is recorded separately in
`linux-desktop-ui.json` when the WebView test reaches its report callback.

## Remaining migration work

Subsequent physical input tests now open E2x2/WASAPI and Ubuntu WSLg; see
[input checkpoint](INPUT_TESTING.md). The compatibility transport still intermittently reports a gap around
recording finalization on both platforms.
WSLg audio is virtualized and does not validate Linux hardware capture or latency.
The desktop retains Web Audio playback/scheduling and substantial compatibility
DSP. The CLAP is still an effect prototype with a generic host editor. VST3
instrument, LV2, the embedded plugin editor, complete native transport/synth/FX,
macOS execution and an installable iOS app remain unfinished.
