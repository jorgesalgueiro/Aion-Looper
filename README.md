# Aion Looper

Windows test download: [portable x64 preview](https://github.com/jorgesalgueiro/Aion-Looper/releases/tag/v0.1.0-preview.20261016.3).
Extract the entire ZIP and run `Aion-Looper.exe`. Keep its DLLs beside it.
First-recording finalization now yields during PCM transfer; short physical tests
pass without new reported gaps. See [verification and limits](docs/RECORDING_TRANSFER.md).
Build a verified ZIP with `scripts/package-windows.ps1`.

Native Rust migration of Jorge Salgueiro's Ascii-Looper. Targets: Linux,
Windows, macOS and iOS, with JACK and ASIO support on appropriate desktop hosts.
Plugin targets are VST3 instrument and CLAP on desktop, plus LV2 on Linux, with
REAPER as the first DAW acceptance target. See [the plugin delivery plan](docs/PLUGINS.md).

A first Windows [CLAP audio-effect prototype](docs/CLAP.md) is available at
`dist/Aion-Looper-CLAP-Windows/Aion-Looper-Prototype.clap` and passes REAPER 7.43
scan/load checks. It uses the host's generic parameter editor; the original
plugin editor, MIDI mapping and synth/FX are still pending. Completed takes and
settings now persist through native plugin state; see [state limits](docs/PLUGIN_STATE.md).
Build it with `scripts/build-clap.ps1`.

**Status: a working Windows desktop UI is available; native audio/DSP parity is in progress.**

Run `dist/Aion-Looper-Windows/Aion-Looper.exe`; keep all packaged DLLs beside it.
Temporary launchers and PowerShell hosting have been removed.
Physical input now opens on E2x2/WASAPI and Ubuntu WSLg. Device enumeration
no longer blocks capture reads; see [input test results](docs/INPUT_TESTING.md).
The original interface is embedded in a standalone window, with offline assets.
Build it using `scripts/build-desktop.ps1`. See `docs/DESKTOP_PREVIEW.txt`.

The graphical app currently retains the original Web Audio/AudioWorklet engine
to keep its existing workflows usable during migration. Stopped-loop normalization,
complete overdub/substitute finalization, loop merging, stopped-loop instant doubling,
interactive MULT capture extension,
and sampler reversal/normalization now run through a Rust worker.
Supported mono/stereo WAV imports at the context's sample rate and individual
loop WAV exports also use Rust. Export remains byte-identical PCM16; files needing
resampling or other codecs retain browser decoding.
Project SAVE now uses Rust to encode embedded loop and sampler WAVs, keeping
the original `.alp` schema and settings. The bounded encoding queue preserves
each requested audio snapshot and retains original encoding on overflow/errors.
Offline PCM uses binary float32 transport, with per-buffer operation ordering
and stale-context rejection. MIC/LINE now captures through native CPAL devices,
with the original device selector, L/R/ST routing, MON, meters and input FX.
A bounded AudioWorklet transport feeds that input into the compatibility graph;
playback and recording timing still use Web Audio. APP/SYS remains browser capture.

MIDI input now uses native midir device connections with bounded callback queues,
acknowledged event delivery, hotplug recovery and held-note cleanup. The original
note, CC learn, synth and transport controls receive these native messages.
See [native MIDI details](docs/MIDI.md); physical controller testing remains pending.

The unchanged local Ascii-Looper interface and JavaScript behavior are preserved
in `ui/` as the migration baseline. Opening `ui/index.html` runs the original
browser engine. It does not run the Rust audio core or provide native drivers.

`crates/looper-core` contains the device-independent Rust audio component:
20 stereo tracks, bounded recording storage, playback, circular overdub with
the original 80% feedback default, substitute, gain, mute, stop and clear.
Its processing path uses an integer sample clock and no heap operations or locks.
It now also provides a fixed-capacity sample scheduler, retrigger offsets and
worker-side normalization, reverse, instant multiply, seam processing, latency
compensation and circular punch mixing. Golden fixtures execute the original
JavaScript to compare the migrated buffer operations. The live recording
workflow is still preliminary and does not yet use that offline punch finalizer.

`crates/looper-files` imports and exports mono/stereo PCM16, PCM24 and float32
RIFF/WAVE. Float export preserves headroom without quantization. PCM16 export
matches the original encoder byte for byte. No resampling or dithering is added.
Native import also accepts WAVE_FORMAT_EXTENSIBLE PCM16/24 and float32,
including 16/24-bit PCM left-aligned in wider containers. See
[native WAV import](docs/WAV_IMPORT.md) for channel layouts and limits.
This import extension passes codec and desktop worker tests on Windows and Ubuntu.
The project now resides outside Dropbox; see [workspace](docs/WORKSPACE.md).

`crates/looper-audio` adds native input/output, bounded SPSC audio/control queues, callback
rendering, master gain ramps and output protection. Device enumeration and
zero-gain WASAPI streams passed here. JACK compilation has been checked; ASIO
SDK/toolchain and driver validation are still pending. Input capture preserves
finite float headroom and marks dropped frames with the input-device sample clock.
Native capture hardware and duplex clock alignment have not yet been verified.

## Native sample tools

The first runnable native tool processes WAV files without opening an audio device:

```sh
cargo run -p looper-cli -- input.wav output.wav --normalize --reverse --repeat 2 --format float32
```

Run `cargo run -p looper-cli -- --help` for latency compensation, seam processing
and export options. The tool preserves the input and refuses to overwrite an
existing output unless `--force` is supplied. Native device capture is available
in the CLI and desktop MIC/LINE path. Fully native graphical playback is pending.

To enumerate devices and play a loop through native audio:

```sh
cargo run -p looper-cli -- devices
cargo run -p looper-cli -- play input.wav --seconds 5 --gain 0.25
cargo run -p looper-cli -- record take.wav --seconds 5 --rate 48000 --format float32
```

See `docs/NATIVE_AUDIO.md` for host/device selection, buffer requests, build
features and the limits of the device checks performed so far.

To exercise the native scheduler and renderer:

```sh
cargo run -p looper-files --example render -- native-loop.wav
python scripts/check-render.py native-loop.wav
```

The example renders five seconds at 48 kHz in stereo float32, including a precisely
scheduled mute interval. On this Windows workspace use `scripts/cargo.ps1`
in place of `cargo`; it selects the project-local toolchain and sets its paths.
Quote `'--'` when passing Cargo's argument separator through that PowerShell script.

## Verification

With Rust, a C++17 compiler and Node.js installed:

```sh
cargo test --workspace
cd ui
node --test "tests/*.test.cjs"
```

On this Windows workspace a project-local GNU Rust toolchain is available under
`.toolchain/`. `scripts/check.ps1` uses it when present. Node.js must be on PATH.
The GNU target uses Rust's bundled linker via `.cargo/config.toml`.
The native stretcher also needs a C++17 compiler. For this project-local Windows
toolchain, run `scripts/setup-native-cxx.ps1` once; it downloads checksum-pinned
official MSYS2 packages into `.toolchain/`, without system installation.
Toolchains and build products are excluded from Git.

The copied browser suite has 600 passing tests. The native workspace has 127
passing tests, including golden DSP comparisons, scheduler behavior, WAV formats,
CLI workflows and a measured zero-heap-operation check across 20 tracks.
Silent WASAPI device checks passed. Physical latency, audible sound quality,
JACK/ASIO drivers and sustained performance have not been verified. CI definitions
cover Windows, Linux, macOS, JACK compilation and compilation for iOS; those
hosted jobs have not yet run.
77 desktop bridge/input/MIDI tests pass. The packaged GUI integration test passes 119 checks in
WebView2, covering the actual start button, recording through a synthetic input,
native WAV import/export, Rust normalization and punch mixing, sample-exact undo/redo after
overdub, substitute, merge and multiply, sampler reversal/normalization,
play/mute/stop/clear, track expansion and all
four workspace tabs. No microphone or audible output was used by that test.
Project SAVE/reload also passes, including native WAV payloads, twenty-track
layout, sampler settings and dynamic-recording metadata.
Interactive MULT now removes captured DC offset, applies the 8 ms splice fade
and appends the take in Rust for mono/stereo buffers up to one million combined
frames. The actual MULT button, prefix preservation and undo/redo pass in WebView2.
Cleared takes and replacement recording sessions reject delayed worker replies.
Overdub/substitute now send raw capture through one Rust operation: DC removal,
the existing system/manual latency correction, 8 ms splice fading and circular
mixing. Input PCM and latency settings are captured at submission. Undo history
is saved only when the completed result can still be committed to the same take
and recording session. Unsupported layouts/rates and larger buffers retain
original processing. Physical latency calibration remains unverified.
First-recording finalization also uses Rust: free capture, dynamic bar trimming,
precise synchronized extraction, late-capture padding, overlapping-tail folding,
latency correction, fades and DC removal. Timing and PCM are frozen at submission;
replaced takes, changed sessions and stopped transport reject delayed results.
Mono/stereo captures at the context rate use the worker when input plus requested
target fit one million frames. Other cases keep the original finalizer, generated
from the unchanged source, operating on saved PCM and timing settings.
EXP ALL, EXP MASTER and EXP INPUT now use Rust for PCM16 WAV encoding. Master,
input and recorded stem exports apply the original linear fades in that worker
job. Dry takes are captured before the first bulk-export wait; filenames, file
order, tracker sidecars and per-file errors retain their original behavior.
Worker failure and unsupported sizes/layouts retain source encoding of saved
PCM. Compressed-recording decoding and live effects bouncing still use Web Audio.
WAV import calibration now also matches the host's reciprocal rounding; every
PCM16 integer value matches WebView2 exactly, including after export fades.
Sample Lab REV/NORM/CROP and SAVE WAV now use Rust, with one-slot undo,
original zero-crossing selection and no-op normalization history preserved.
Its PROCESS control now uses **Rubber Band 4.0.0 R3** for native offline
time-stretching and independent pitch shifting. Processing runs on the worker,
with coupled stereo channels, exact requested output length and tail flushing.
Mono/stereo input and output are each limited to one million frames, at stored
rates from 8–192 kHz. Larger/wider jobs or worker errors retain the bundled
SoundTouch JS compatibility path. No-op ratio/pitch retains the original shortcut.
Source/context changes and in-place normalization cancel stale processing results.
The original interface is unchanged. Listening comparisons and non-Windows
validation remain outstanding. See `docs/TIME_STRETCH.md`.
Rust-generated synthetic native input passes MIC/LINE setup, MON, stereo recording,
overdub/substitute, L/R selection, device changes and cleanup. A previous hosted
test recorded the USB microphone at 48 kHz stereo, but hosting is no longer part
of the product. Standalone physical microphone use remains failing/unverified.
Transport latency, clock-drift quality and audible monitoring need measurement.

See `docs/MIGRATION.md` for requirements, architecture and remaining work.
See `docs/VERIFICATION.md` for what has actually been tested.
Original author attribution and GPL 2.0 license are preserved in `LICENSE` and
the copied sources.

Linux/Ubuntu builds and packages: [instructions](docs/LINUX.md).
Latest migration audit and platform limits: [audit](docs/AUDIT.md).
