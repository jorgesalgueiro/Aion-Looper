# Native audio mode

Launch `Start-Native-Audio.cmd` beside the Windows executable, choose
`TOPPING Pro USB Audio Device`, then press START AUDIO. The executable also accepts:

```powershell
& './dist/Aion-Looper-Windows/Aion-Looper.exe' --native-audio --input-host ASIO --input-device 'TOPPING Pro USB Audio Device'
```

Windows ASIO builds open this native session by default.
It loads no AudioContext, AudioWorklet, browser PCM transport or SoundTouch.
The webview loads the original presentation and a native controller; Rust owns capture, recording storage,
loop playback, monitoring, gain, feedback, mixing and the ASIO output stream.
Use `Start-Compatibility-Audio.cmd` or `--compatibility-audio` for the complete legacy behavior. Both modes now use the original visual design. Non-ASIO builds retain the compatibility default.

## Controls and current limits

- Original HTML, CSS, ASCII logos and loop markup are reused without modifying `ui/`.
- Press the original loop action button or number keys 1–0: RECORD, FINISH, PLAY, STOP.
- The UI FINISH action completes capture/punch and starts playback. The underlying
  engine `finish` command still leaves a loop stopped for explicit API callers.
- Select OVERDUB or SUBSTITUTE in the loop section ACTION selector before pressing
  a playing loop. Press FINISH to return to playback.
- Original MUTE/UNM, DEL, volume and feedback controls operate the Rust engine.
- Drop matching-rate mono/stereo WAV onto a stopped loop, or use the native WAV
  path controls. Rust prepares and commits it while other loops play. See NATIVE_IMPORT.md.
- STOP ALL and DEL ALL use native commands. Loop names are local session labels.
- Grey controls (FX, pan, speed, sync, synths, MIDI, etc.) are disabled
  until their native implementations are connected. Waveforms are not fabricated.
- Per-loop level and overdub feedback; input gain, master gain and monitoring.
- Monitoring starts off and only affects speakers. Recording works with MON off.
- Each normal native loop reserves 30 seconds. At capacity it starts playing;
  capture never grows storage on the audio thread.
- CLOSE & CLEAR releases both streams and discards all native takes.

**Native project save/reopen and dry WAV export are now available.** See
[NATIVE_PROJECTS.md](NATIVE_PROJECTS.md). SAVE PROJ keeps takes and native settings
in versioned `.aion` files beside the app. Loop SAVE exports stereo float32 WAV.
Closing still discards unsaved changes. Existing `.alp` projects open through
compatibility mode; their import into the native engine remains pending.
Synths, effects, MIDI mapping, musical quantization, waveform editing, live playback undo
and the original recording latency/seam/DC finalization behavior still require
migration. See [NATIVE_PARITY.md](NATIVE_PARITY.md) for the comparison. Controls
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
no PCM and receives no take buffers. Project/WAV encoding uses Rust-owned checkpoint data on the service worker. Master/input/monitor levels ramp on the
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

## Original UI migration boundary

`native_view.rs` removes the reference page scripts and extracts only the pure
loop-markup methods from the original UIManager. The native controller removes
all original inline handlers before binding supported actions. It does not
import main.js, AudioEngine, LoopManager, SoundTouch or the compatibility capture
bridge. Loop audio remains inside Rust; UI status carries only metadata.

This retains the original appearance without maintaining a second hand-drawn UI.
It does not yet establish complete behavior or pixel parity on every platform.
The original help describes the full legacy product; disabled native controls
and the native-mode notice identify features still pending migration.

Verification: 36 desktop unit tests; 26 synthetic native graphical checks and
27 physical TOPPING ASIO checks (10 seconds, 48 kHz, 128-frame callbacks), including
original action buttons, number keys and a 250 ms UI stall, with zero reported
underruns. Windows capture screenshots were unavailable (capture timeout);
accessibility inspection confirmed the rendered original layout and controls.

## Native persistence checkpoint

Native project save/reopen and dry float32 WAV export pass the Windows and Ubuntu
graphical suites (33 checks), with 131 compatibility GUI checks still passing on
Windows. There are 37 desktop unit tests, 26 audio tests including callback heap
accounting and concurrent snapshot/playback, and 13 file tests. A 30-second
physical TOPPING/ASIO test at 48 kHz and 128-frame callbacks reports zero underrun
or dropped frames, with output muted and no saved microphone recordings.

## Native normalization and punch history

Stop a recorded or imported loop, then press its original **NORM** button. Rust
normalizes stereo PCM using a shared channel peak; gain, feedback and mute stay
unchanged. Other loops can keep playing, but finish all recording/punch operations
before editing. **UNDO** and **REDO** restore normalization edits exactly.
Silent or already-normalized takes do not add history.

Overdub and substitute snapshot the previous take in Rust before starting a pass.
Finish or stop the target, then use UNDO to recover its exact pre-pass PCM and
REDO to restore the punch. Gain, feedback and mute retain their current settings.
Other loops may keep playing. Starting a new pass clears redo for that loop.

History is limited to five entries per loop per stack and 8 million stereo frames
(64 MB) total. Oldest undo entries are evicted first when the memory limit is
reached. Takes exceeding this limit cannot be edited. History is reset by recording,
successful WAV import, clear and closing audio. If another loop is capturing or
punching, or the take exceeds the history budget, the punch still starts without
undo: the original interface reports the reason and clears stale target history.
Finish all captures before starting an undoable pass. Pending controls drain
before the snapshot; a stalled callback rejects the request after 100 ms. Projects save
the resulting PCM, without history. The original reverse-on-next-play toggle
still awaits migration.

Editing verification: Windows and Ubuntu each pass 41 native GUI checks, plus
131 Windows / 132 Ubuntu compatibility GUI checks. Rust tests cover exact normalize/undo/redo PCM, mix
settings, unrelated playback, rejected edits, history invalidation and memory bounds.
The allocation test checks zero callback heap operations for import and edit success,
rejection and cancellation. A silent 30-second TOPPING ASIO regression at 48 kHz /
128 frames reports zero underruns/dropped input frames; microphone audio was not saved.

Native WAV import now converts 8–192 kHz source audio to the session rate in Rust.
Drop a WAV onto a stopped loop, or use the Project Menu path controls. Duration
and pitch are retained and the panel reports rate conversion. The converted take
must fit the track duration; see [NATIVE_IMPORT.md](NATIVE_IMPORT.md).

Punch-history verification: 42 desktop Rust tests pass on Windows and Ubuntu,
with strict Clippy and the audio/core regression suites. Both native webviews pass
49 checks, including the original UNDO/REDO buttons for overdub and substitute,
concurrent-capture continuation and the visible history-unavailable notice.
Compatibility checks pass (131 Windows / 132 Ubuntu). A silent 30-second TOPPING
ASIO regression at 48 kHz / 128 frames passes 35 checks, with zero underruns or
dropped input frames and no saved microphone recordings. History remains bounded,
worker-owned and session-only; live playback undo remains pending.

## Global undo and redo

The original master UNDO / REDO buttons now use Rust history. Their labels name
the next loop, such as UNDO L2. Each action restores the latest eligible edit or
punch pass across all loops, using the same PCM snapshots as per-loop history.
Stop the named loop and finish recording/punches first; other loops may keep playing.
Ctrl/Cmd+Z undoes and Ctrl/Cmd+Shift+Z redoes while audio is open. Text fields keep
their normal text-editing undo. A new edit clears the global redo timeline while
other loops retain their own per-loop redo, matching Ascii-Looper. No additional
PCM history copies or project-format changes are needed for global history.

Global-history verification: 44 desktop Rust tests and strict Clippy pass on
Windows and Ubuntu, alongside the audio/core regressions. Both native webviews
pass 55 checks, including master labels/buttons, keyboard undo/redo, text-field
isolation, stopped-target gating and session reset. Compatibility checks pass
(131 Windows / 132 Ubuntu). The silent 30-second TOPPING ASIO regression at
48 kHz / 128 frames passes 36 checks with zero underruns or dropped input frames;
no microphone recordings are saved. Source PCM and history remain owned by Rust.

## Native retrigger and stutter

Use the original **RTRG** button to start/restart a take at its first frame.
Hold **STUT** during playback to repeat a short fragment from the current phase;
release it to continue full-loop playback from that phase. Rust owns the fragment
and sample clock. Stop, clear, punch and retrigger cancel a hold. Pointer cancel,
lost pointer capture, window blur and hiding the page also release held controls.

Stutter uses the reference formula at 120 BPM: 125 ms, at most half the take, at
least 10 ms, clipped at the loop end. A one-frame remainder repeats safely. Native
BPM/sync and quantized retrigger are still pending. Neither control edits PCM or
adds undo entries; holds are not persisted in projects and do not restart other loops.

Transport verification: Windows and Ubuntu pass the core/audio/host regressions,
45 desktop Rust tests and strict Clippy. Both native webviews pass 60 checks,
including original RTRG/STUT controls, pointer release/cancel/lost capture and
window blur. Sample-exact tests cover held ranges, end clipping, one-frame takes,
release phase, retrigger, unchanged PCM and other-track clock continuity. Heap
accounting confirms no allocations/frees for these controls in the callback.
Compatibility checks pass (131 Windows / 132 Ubuntu). The silent 30-second
TOPPING ASIO regression at 48 kHz / 128 frames passes 41 checks with zero
underruns or dropped input frames and no saved microphone recordings.

## Native instant multiply

Stop a take and press its original **MULT** button to double it in Rust. The
result contains two exact copies, retaining gain, feedback, mute and the loop
name. Local/master UNDO and REDO restore the original or doubled take, including
its length. SAVE PROJ keeps the doubled PCM, without applying another fade.

The doubled take must fit the existing loop capacity and the 8 million frame
editing budget. Failed operations retain PCM and history. Other loops may keep
playing, but finish all captures before editing. MULT is disabled during playback
because interactive multiply capture is still pending; a playing take is never
silently replaced by instant doubling. Growing a running track remains pending.

Instant-multiply verification: 47 desktop Rust tests and strict Clippy pass on
Windows and Ubuntu, alongside the core/audio regressions. Both native webviews
pass 64 checks, including the original MULT button, local/global length restoration
and project save/reopen of the doubled take. Tests cover exact sample bits,
headroom, mix/mute preservation, uninterrupted other-track playback and rejected
capacity/history bounds without losing PCM or redo. Compatibility checks pass
(131 Windows / 132 Ubuntu). The silent 30-second TOPPING ASIO regression at
48 kHz / 128 frames passes 44 checks with zero underruns/dropped input frames and
no saved microphone recordings. Live multiply capture remains pending.
