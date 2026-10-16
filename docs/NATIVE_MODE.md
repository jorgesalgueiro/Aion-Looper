# Native audio mode

Launch `Start-Native-Audio.cmd` beside the Windows executable, choose
`TOPPING Pro USB Audio Device`, then press START AUDIO. The executable also accepts:

```powershell
& './dist/Aion-Looper-Windows/Aion-Looper.exe' --native-audio --input-host ASIO --input-device 'TOPPING Pro USB Audio Device'
```

This is an opt-in native session, separate from the compatibility interface.
It loads no AudioContext, AudioWorklet, browser PCM transport or SoundTouch.
The webview displays controls and meters; Rust owns capture, recording storage,
loop playback, monitoring, gain, feedback, mixing and the ASIO output stream.
The ordinary launch still opens the existing compatibility interface.

## Controls and current limits

- Ten loops: REC, FINISH, PLAY, STOP, OVERDUB, SUBSTITUTE, MUTE and CLEAR.
- FINISH ends capture/punch and leaves the loop stopped. PLAY starts playback.
- Per-loop level and overdub feedback; input gain, master gain and monitoring.
- Monitoring starts off and only affects speakers. Recording works with MON off.
- Each normal native loop reserves 30 seconds. At capacity it starts playing;
  capture never grows storage on the audio thread.
- CLOSE & CLEAR releases both streams and discards all native takes.

**There is no session save/import/export in native mode yet.** Use it to test the
native audio path; closing the window or audio session loses its takes. Existing
compatibility projects are not loaded or modified by this mode. Synths, effects,
MIDI mapping, musical quantization, waveform editing, undo/redo and the original
recording latency/seam/DC finalization behavior still require migration. Controls
apply at native callback boundaries, not browser timestamps. Audio ownership follows the native window, not UI polling. Minimizing or pausing
the webview does not stop transport; explicit close, document replacement and
window shutdown release the streams.

Physical native duplex currently requires ASIO on Windows. The ASIO input and
output use the same selected driver/device at the exact requested sample rate;
there is no implicit fallback or resampling. ALSA/JACK/CoreAudio native duplex
adapters are pending. The core, synthetic native mode and GUI compile on other
desktop platforms. iOS app integration remains pending.

## Callback and control boundary

`looper-audio::duplex` connects the allocation-free capture ring to looper-core
and renders directly into the device output buffer. All loop buffers are
allocated before opening streams. Commands use a 256-slot SPSC queue with at most
64 controls processed per callback. Status/meter reads use atomics. The UI sends
no PCM and receives no take buffers. Master/input/monitor levels ramp on the
native sample clock, with finite output protection.

The native service checks document/session tokens, bounds total loop storage to
32 million stereo frames (256 MB), and releases devices on owner shutdown. Device refresh
lists registered ASIO names without opening every driver. A synthetic native
source is available only in hidden test mode, never a production fallback.

## Verification

```powershell
./scripts/build-desktop.ps1 -Asio
./scripts/test-native-audio.ps1
./scripts/test-native-audio.ps1 -PhysicalInput -Seconds 30
```

Physical tests keep master output at zero and discard all takes. They verify
record/play/overdub/substitute/mute/clear, both stream release and continued
capture through a deliberate 250 ms UI stall. They do not establish audible
quality, round-trip latency, long-session drift, hardware hotplug or full feature
parity. A counting allocator checks that the Rust audio callback, including command
rejection and malformed blocks, performs no heap operations; deterministic tests compare recorded and punched PCM.

ASIO binary redistribution remains subject to the SDK licensing issue described
in ASIO.md. These packages remain local tests; this change does not relicense the
project or publish a new ASIO binary release.

Verified checkpoint (2026-10-16): Windows/TOPPING native duplex at 48 kHz, 128-frame callbacks passed a 30-second recording and a separate 31-second pause in all UI meter polling (about 32.5 seconds captured). Both reported zero input underrun frames, dropped frames, discontinuities and stream errors. Clear, close and reopen passed. A deliberate 250 ms UI stall was included. Master output stayed at zero; audible playback and latency remain unverified. Windows physical native GUI: 18 checks normally, 19 with the extended idle phase; the synthetic native suite has 17 checks; 34 desktop unit tests, 25 audio tests (including allocation accounting), and 97 compatibility JavaScript tests pass.

Ubuntu final verification: 17 native GUI checks and 132 compatibility GUI checks passed, along with audio/desktop Rust tests and strict Clippy. Linux desktop/CLI/CLAP packaging and the packaged CLAP ABI host check passed. Native Linux hardware duplex is still pending.
