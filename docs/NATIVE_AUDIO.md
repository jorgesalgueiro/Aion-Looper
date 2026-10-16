# Native audio adapter

Current input checkpoint: [input tests and fixes](INPUT_TESTING.md). Physical
capture now opens on the E2x2/WASAPI interface and Ubuntu WSLg. Earlier Windows
initialization failures below are historical observations, not the current result.

`looper-audio` connects the Rust engine to CPAL output streams and provides
native input capture through a separate bounded worker transport. The CLI can
loop a WAV or record a new take without browser audio.

```sh
cargo run -p looper-cli -- devices
cargo run -p looper-cli -- play input.wav --seconds 5 --gain 0.25
cargo run -p looper-cli -- play input.wav --host WASAPI --device "DEVICE NAME" --buffer 480
cargo run -p looper-cli -- record take.wav --seconds 5 --rate 48000 --format float32
```

Use a unique device name or the ID printed by `devices`. The WAV rate must be
supported by the selected device; no rate conversion is performed. Selection
prefers stereo float32 and supports mono, float64, int32, int16 and uint16 input/output.
Input also accepts packed int24 and interfaces with up to 128 device channels;
the current stereo adapter captures their first pair. Output still requires mono/stereo.
The Windows executable is `target/release/rust-looper.exe`.

For project-local Windows builds use `scripts/cargo.ps1`. Quote the Cargo
separator when forwarding it through PowerShell:

```powershell
.\scripts\cargo.ps1 run -p looper-cli '--' devices
```

New Windows dependencies require GNU assembler tools. Checksum-verified tools
are installed under `.toolchain/gnu-tools`, with no system installation. The
wrapper temporarily adds their directory to PATH and restores it afterward.
The optional installer is `scripts/install-gnu-tools.ps1`. Normal MSVC builds
use their regular toolchain. Linux builds require ALSA development files.

## Callback and control bridge

The preallocated SPSC queue holds 256 Copy messages. A full queue returns the
unsent message, and each callback consumes at most 64 controls. Immediate
commands execute before its first frame; scheduled commands use engine-frame
timestamps in nondecreasing order. Late/invalid commands are rejected and counted.

Master gain is ramped by sample count. Startup defaults to 25% gain with a 5 ms
fade. Normal timed shutdown requests a fade to silence before dropping the stream.
Output applies the original master waveshaper's static safety curve; oversampling
and the full master bus remain pending. Malformed callback blocks are silenced
without advancing the engine. The measured rendering/control path has no app
allocations, frees, locks, logging or file operations.

Frames, block peak, largest callback and command/stream/malformed-block errors
are individually atomic metrics, not a transactional session snapshot. File
playback reserves PCM only for track 0 rather than allocating its take size for
all 20 tracks. All setup and decoding happen before device startup.

## Native capture

The input callback copies mono/stereo PCM into a preallocated SPSC ring. Mono
duplicates to stereo internally; WAV export is mono for mono devices and stereo
otherwise. Wider device layouts retain their first input pair.
Finite float32 headroom is preserved; capture applies no limiter, normalization,
resampling or seam fade. Invalid samples become silence and increment diagnostics.
Each frame carries its input-device clock position. A full ring drops new frames
and counts them, leaving queued audio intact and exposing the gap to the worker.
The callback performs no app allocations, frees, file I/O, locks or logging.

`record` defaults to five seconds at 48 kHz and float32 WAV. It accepts durations
up to 120 seconds and rates from 8 to 192 kHz, provided the device supports the
exact rate. The worker reserves the entire take before starting the stream, with
a half-second input ring, and saves exactly the requested rounded sample count.
Existing files are rejected before input startup and never overwritten. Dropped,
nonfinite, malformed or interrupted capture is rejected without writing a WAV.
The stream is dropped before encoding or file I/O. A stalled input times out.
PCM16/PCM24 exports are also available. Float32 is the default to retain headroom.

Synthetic tests cover input conversion, overflow, clock continuity, callback
partitioning, cross-thread handoff and WAV round trips. Physical microphone
capture has not been tested. This transport uses the input device clock; it does
does not yet drive a fully native duplex loop engine.

## Desktop MIC/LINE input

Launch `dist/Rust-Looper-Windows/Rust-Looper.exe` directly. Temporary launchers,
PowerShell hosting, the exported host entry point and the desktop DLL were removed.
The desktop and core audio libraries forbid unsafe code. The worker-only
`looper-stretch` adapter isolates one documented native FFI call, with C++
resource ownership and exception containment. Windows API experiments remain
isolated in a development-only diagnostic example.

On this PC, standalone Rust and C# executables fail WASAPI Initialize with
`0x80070057`; WebView2 microphone access and native AudioGraph input creation also
fail. Windows AppCapability reports Allowed. Raw-mode configuration and
IAudioClient3 initialization do not fix it. A previous hosted experiment proved
that this Rust capture/recording path can handle the USB microphone, but that
host is no longer shipped. Those initialization results are historical; current capture opens successfully.

Kaspersky Endpoint Security is installed and its HipsTask component is running.
Its application-rule export requires credentials and could not be inspected.
The observed WASAPI error and WinMM error 11 match an independent software
vendor's documented Kaspersky microphone block:
https://support.estos.de/en/procall-enterprise/kaspersky-blockiert-kamera-und-mikrofon-fuer-procall-enterprise
Kaspersky documents per-application sound-recording controls:
https://support.kaspersky.com/kes-for-windows/11.11.0/134861
This is strong evidence for application-specific protection, not direct proof of
the installed rule. Other applications being allowed does not establish access
for Rust-Looper. No driver, privacy, antivirus or policy settings were changed.

Run `scripts/cargo.ps1 run -p looper-audio --example windows_input_diagnostics`
for paused initialization probes. It checks Windows access, shared WASAPI, raw
mode, IAudioClient3 and AudioGraph independently without starting capture or
saving audio. The normal desktop still uses the unchanged registry CPAL release.

The desktop's existing MIC/LINE control now opens CPAL input rather than browser
getUserMedia. The existing device selector lists native host/device identities,
which also persist in projects. Explicit native selections fail visibly instead
of switching drivers; legacy browser device IDs can fall back to native default.
Four simultaneous native sources are supported. Input rates must match the audio
context; no device-rate conversion is silently introduced. APP/SYS capture stays
on the browser's display-capture path.

A separate native input thread services bounded requests independently of offline
DSP. Each source has a half-second SPSC capture ring. Read packets contain at most
4,096 stereo frames and preserve input-clock positions. A dedicated AudioWorklet
has 16,384 frames of storage, a nominal 64 ms prefill, and cubic interpolation with
a slowly adjusted clock ratio bounded to +/-0.1%. The frontend polls every 4 ms
with one outstanding read per source. These settings are **not measured latency**;
device blocks, IPC, UI stalls and Web Audio output add delay. This is a compatibility
transport, not a sample-accurate native duplex adapter or a bandlimited resampler.

The original gain/pan, L/R/ST, MON, metering, input FX, loop recording, overdub,
substitute and master-recording graph receive an ordinary audio source node.
MON stays off at startup. A separate silent branch keeps input draining while MON
is off and across graph rebuilds. Removed/replaced sources, closed contexts and
page exit stop native capture; suspended contexts discard old captured audio.
Stream errors, invalid samples, packet gaps, native overflow, transport overflow
and stalled input stop the source and use the existing input error banner. Worklet
underruns are counted and reported because recordings can then contain silence.
Diagnostics are available through `window.RustLooper.input.status()`.

Build desktop JACK support with `scripts/build-desktop.ps1 -Jack`, or ASIO with
`-Asio` once its SDK/toolchain requirements are installed. Those hosts appear in
the existing device selector when available. Desktop JACK compilation is checked;
ASIO capture now builds and has passed local TOPPING/E2x2 recording checks.
The public preview still uses WASAPI; see ASIO.md for the local build and limits.
No host output driver is connected to the graphical playback engine yet.

The real WebView2 integration test uses a Rust synthetic source that is enabled
only by `--self-test`. It exercises MIC/LINE, MON, stereo channel balance,
recording, overdub/substitute, L/R routing, device changes and removal without
opening a microphone. `scripts/test-desktop.ps1` runs that suite directly in the executable.
The opt-in `scripts/test-desktop.ps1 -PhysicalInput` attempts to record the physical
microphone into an in-memory loop with the master muted, then discards the take.
It saves only a JSON test report. The current standalone can open the E2x2 input on this PC; see the latest
input checkpoint for capture and transport quality results. Audible monitoring, sustained drift correction and latency
still require hardware testing.

## Backend status

| Backend | Build | Verified here |
| --- | --- | --- |
| WASAPI | Default Windows build | Enumeration and silent output passed; standalone physical input setup fails on this PC |
| JACK | `cargo build -p looper-cli --features jack`, then `--host JACK` | Windows GNU compilation checked; server/hardware pending |
| ASIO | `cargo build -p looper-cli --features asio`, then `--host ASIO` | Local TOPPING/E2x2 ASIO capture verified; output remains unverified |
| ALSA/Core Audio | Default Linux/macOS/iOS CPAL builds | CI definitions updated; execution pending |

JACK on Linux needs development files such as `libjack-jackd2-dev`. ASIO needs
a driver, SDK and C++/LLVM tooling per the [CPAL build instructions](https://github.com/RustAudio/cpal/tree/v0.18.2).
An unavailable requested host is an error; it never silently falls back to WASAPI.

## Device observations and limits

Default-buffer and Fixed(480) zero-gain runs succeeded on the default WASAPI
device at 48 kHz stereo float32 with zero reported stream errors. Both observed
callbacks up to 1,056 frames. Buffer settings are host requests; actual callback
sizes may differ. 1,056 frames at 48 kHz spans 22 ms of audio, **not** measured
output or round-trip latency. No audible playback or microphone capture was used.

Fully native monitoring, duplex clock alignment, calibration,
native loop capture finalization, track snapshots, output UI integration, recovery,
MIDI and the full DSP chain remain. The renderer accepts synchronized input
frames for adapter tests. The new CLI capture opens a separate CPAL input stream;
its callback never drives the output engine. Stream errors stop
the CLI; automatic reconnect and arbitrary input-pair selection are pending.

APIs were checked against [CPAL 0.18.2](https://docs.rs/cpal/0.18.2/cpal/) and
[rtrb 0.4.0](https://docs.rs/rtrb/0.4.0/rtrb/). A counting allocator independently
checks the rendering/control path with 20 playing tracks and input capture,
including overflow and malformed input paths.

## Native duplex checkpoint

The opt-in native audio mode now owns TOPPING ASIO input and output, recording, loop playback, overdub/substitute, mixing and monitoring entirely in Rust. Only controls and meters cross the webview boundary. See NATIVE_MODE.md for launch instructions, muted hardware verification, unsaved-session limits and remaining parity work. Compatibility playback remains Web Audio; the statement above about no graphical native output applies to that compatibility path.
