# Migration requirements and acceptance

## Reference

Authoritative reference supplied by the user:
`C:\Users\jorge.salgueiro\Documents\Codex\Ascii-Looper`.
The UI and tests were copied from this local folder on 2026-10-14.
`ui-baseline.json` records SHA-256 hashes for the copied files, including tests.
The original sources remain untouched. Public upstream:
https://github.com/jorgesalgueiro/Ascii-Looper.

The UI must retain its HTML structure, ASCII graphics, CSS, controls, layouts,
translations and responsive behavior. Native functionality must eventually
replace Web Audio, MediaRecorder and browser device APIs behind those controls.
Preserving the assets alone does not establish identical rendering on platform
webviews; screenshot comparison at fixed viewport sizes is required.

## Architecture direction

### Current delivery priority

The user's latest direction is a working standalone program. Finish the current
CLAP persistence change, then defer additional plugin-format work. Prioritize
successful physical audio input, clear startup/device recovery, reliable
record/play/overdub, imports and project save/reopen through the original UI.
Keep the normal executable as the product; no temporary host/launcher workaround.
Synthetic checks do not count as proof that physical capture works on this PC.
The Windows microphone initialization failure remains the leading functional
blocker. Continue from existing diagnostics rather than repeating unproven
security-policy explanations or asking the user to change administrator settings.

### Shared architecture

- One Rust DSP engine owns the sample clock and all audio state.
- Deliver VST3 instrument, CLAP and Linux LV2 plugins alongside standalone apps,
  with REAPER host validation and the original editor UI. Plugins use host audio,
  MIDI, automation and transport rather than opening system devices. The shared
  `looper-host` processor and Windows CLAP audio-effect prototype are implemented;
  REAPER scan/load and state save/reopen pass. Instrument MIDI, editor and VST3/LV2 wrappers
  remain pending. See `PLUGINS.md` for the format plan and acceptance gates.
- A Wry/Tao desktop window now embeds the original HTML/CSS UI and offline
  assets. Its worker bridge processes stopped-loop normalization, overdub and
  substitute finalization, loop merging, stopped-loop instant doubling and
  sampler reversal/normalization and supported WAV import/individual-loop export
  through Rust. Offline stereo float32
  PCM travels as little-endian binary data encoded in base64. Compatibility audio
  retains the original Web Audio/AudioWorklets until the native paths are connected.
- Commands take effect at declared sample positions via a 256-entry scheduler.
  The audio owner queues them in nondecreasing timestamp order; equal-frame
  commands preserve insertion order. Full queues, past timestamps and invalid
  execution states have explicit outcomes. A 256-entry SPSC control bridge now
  feeds the audio owner, capped at 64 messages per callback.
- The callback performs bounded work, with no locks, I/O, logging, allocation,
  freeing memory, UI calls or DSP graph construction.
- Buffer storage and graph replacements are prepared on a control worker.
  Retired objects must be reclaimed outside the callback.
- Desktop backends must expose JACK and ASIO where supported, plus system
  audio backends. CPAL output is implemented: WASAPI smoke checked, JACK
  compilation checked, ASIO feature wiring added. Native input and CLI WAV capture
  are implemented with synthetic validation. A removed hosted experiment passed
  short physical capture, but standalone input still fails on this Windows PC.
  Sustained input and non-Windows execution remain to be validated.
- Device buffer size, sample rate, channel layout, measured latency and xrun
  counts must be visible. Duplex clock drift and disconnects need explicit handling.
- Float PCM recording replaces compressed browser capture in the native engine.
  WAV import/export must preserve 24-bit PCM and 32-bit float compatibility.

## Feature parity matrix

All rows require behavior tests against the original, UI integration and platform
validation before being marked complete. Passing the copied JavaScript tests
does not verify a Rust implementation.

| Area | Source | Rust status |
| --- | --- | --- |
| Recording/playback, overdub/substitute, mute | `Modules/loopTracks.js` | First stereo core; GUI first-recording and overdub/substitute finalization use Rust; capture clock and playback retain compatibility engine |
| Sustain, synchronized/dynamic capture, prebuffer and count-in | `Modules/loopTracks.js`, `main.js` | Dynamic bar trim and precise synchronized extraction/tail folding use Rust; arming, prebuffer, count-in and sustain retain compatibility engine |
| Phase, reverse, varispeed, half speed, multiply, merge, retrigger, stutter | `Modules/loopTracks.js` | GUI merge, stopped instant MULT and interactive MULT finalization use Rust; sampler reverse connected; retrigger offsets added; other native controls pending |
| Undo/redo, gain ramps, pan, solo, seam fades | `Modules/loopTracks.js`, `main.js` | Compatibility undo/redo verified sample-exact after native punch/merge; native history and other controls pending |
| Samplers, imports, normalization and offline Sample Lab | `Modules/loopTracks.js`, `Modules/tracker.js`, `main.js` | Sampler/loop transforms and supported WAV decode use Rust; Lab REV/NORM/CROP/save and native Rubber Band R3 offline stretch/pitch connected; general browser resampling and sampler playback remain |
| Drone voices, oscillators, FM, envelopes, filters, LFO, rhythms | `Modules/droneSynth.js`, `Modules/scales.js` | Pending |
| Tracker, song arrangement, scheduling and automation | `Modules/tracker.js` | Pending |
| Every built-in effect, presets and ordered signal chains | `Modules/effects.js`, `Modules/loopTracks.js` | Pending |
| Custom `.afx` effects | `Modules/effects.js` | Pending; JS AudioWorklets cannot directly execute as Rust DSP |
| Input routing, monitoring, master processing and recording | `Modules/audioEngine.js`, `main.js` | Desktop MIC/LINE native capture feeds existing routing/monitoring/recording through a bounded worklet transport; fully native duplex, output UI and master bus pending |
| Latency compensation/calibration, metronome and meters | `Modules/audioEngine.js`, `main.js` | Source-compatible latency correction connected to overdub/substitute finalization; calibration, metronome and meters pending |
| MIDI learn/hotplug, gamepad and editable keyboard controls | `main.js` | Native MIDI input and hotplug feed original mapping/learn/synth/transport handlers; gamepad and keyboard retain compatibility code; physical MIDI validation pending |
| `.alp`/JSON sessions, audio assets, stems and master WAV | `Modules/projectManager.js`, `main.js` | Project-save WAVs, supported decoding, individual/bulk dry-loop exports and master/input/recorded-stem fade+PCM16 encoding use Rust; metadata, compressed decode and live effects bounce retain compatibility code |
| UI, translations and diagnostics | `index.html`, `styles.css`, `Modules/i18n.js`, `Modules/debug.js` | Desktop window and native normalization/punch/merge bridge integration tested; full native audio bridge pending |
| REAPER instrument / VST3, CLAP, Linux LV2 | New delivery requirement, same UI and engine | Windows CLAP prototype and completed-take state tested; REAPER state save/reopen passes; host MIDI, embedded editor, VST3/LV2 and DAW playback validation pending |

## Quality gates

1. Port source behavior and regression fixtures component by component. Keep
   race-condition fixes and project compatibility from the local reference.
2. Measure callback duration distribution at actual device configurations,
   including 20 loops plus synths and effects. Buffer duration is the deadline;
   no absolute latency guarantee can be inferred from an offline unit test.
3. Validate sample accurate scheduling independently of callback partitioning.
   Test input/output compensation using measured loopback, including device changes.
4. Compare DSP output using impulses, sweeps, silence and deterministic fixtures;
   test clipping, denormals, resampling, seam clicks and long-session stability.
5. Compare the rendered interface on each target and exercise every control.
6. Run JACK/ASIO hardware tests, iOS device tests, interruption/hotplug tests,
   project round trips and a sustained live-performance soak.
7. Validate plugin scan/lifecycle, automation timing, host MIDI/transport, offline
   bounce, multiple instances, embedded editor and host save/reopen in REAPER;
   additionally verify Linux LV2 in a second host. Format validators must pass.

## Present core limits

Stereo core; track capacities chosen at startup. `Engine::new` reserves
`20 * capacity_frames * 8` bytes; `with_capacities` can reserve selected tracks.
Reaching capacity automatically finishes a take.
There is no input monitoring, full master limiter, effect processing, undo,
resampler or full native device integration with the desktop UI yet. Per-track gain and mute
changes are instantaneous and require ramps before live use. Recording and punch
behavior is preliminary. Source seam fades, latency compensation and punch mixing
are now available as worker-side functions. The desktop connects native punch
mixing to compatibility capture finalization; native device capture still requires
buffer handoff without pausing the live device. Worker edits accept up to one
million frames (destination plus take for mixing), mono/stereo at matching rates.
Instant doubling accepts up to 500,000 input frames; sampler transforms accept
up to one million mono/stereo frames. Active loop doubling retains the original
timing behavior. Larger or unsupported buffers retain the original JavaScript behavior. Edits on
the same destination are serialized; replies from obsolete documents or audio
contexts are rejected. MULT and sampler edits are queued per owner, cancel after
take replacement or resumed playback, and preserve the original undo/waveform
updates. Native input capture now feeds desktop MIC/LINE through a separate input
worker. Interactive MULT finalization also uses Rust for captured DC removal,
8 ms splice fades and concatenation, bounded to one million combined frames.
The existing history, sync-source and playback restart behavior is preserved.
First recordings use one Rust worker operation for free capture, dynamic bar
trimming and synchronized extraction, including source-compatible latency
correction, zero padding, tail folding, fades and final DC removal. Frame offsets
are resolved from frozen clock/settings values at submission. PCM, BPM and mode
are captured before awaiting the worker; commit requires the same take, graph,
recorder, audio context and transport generation. Input plus requested target are
bounded to one million frames. Unsupported rates/layouts/sizes and worker failures
use the exact original DSP region on private PCM and frozen settings.
`scripts/generate-recording-adapter.cjs` creates the runtime substitution and its
reference fallback without modifying `ui/`. Scheduling and capture remain Web Audio.
Delayed replies cannot restore a cleared take or finish a replacement recorder.
Unsupported layouts/rates and oversized buffers retain original processing.
The original input graph receives captured audio through a bounded AudioWorklet
transport. Raw overdub/substitute capture is finalized by one bounded Rust job:
DC removal, source-compatible latency rotation/wrap fades, 8 ms seam fading,
then circular mixing with feedback (zero for substitute). PCM and latency values
are frozen at submission. Undo and in-place PCM commit occur only if the take,
audio context and recorder still belong to this operation. Failed native jobs
use original processing on submitted snapshots; unsupported sizes/layouts/rates
retain that path. `assets/punch-edits.json` changes only this finalizer region.
The graph
handles MON, L/R/ST, gain, pan, meters, effects and recording.
The UI and original files remain
unchanged; `assets/input-edits.json` records the runtime capture/selection adapters.
Up to four native sources are supported, with first-pair routing for interfaces
up to 128 device channels and packed int24 capture. Explicit native selections
do not silently switch hosts. APP/SYS retains browser capture. Transport prefill,
IPC and bounded cubic clock correction add delay and need hardware validation;
fully native loop transport and output remain pending. See `NATIVE_AUDIO.md`.
The Windows package now contains the normal executable without PowerShell
hosting or a separate desktop DLL. Standalone microphone initialization remains
unresolved (0x80070057), despite Windows reporting microphone permission Allowed.
The WAV bridge accepts up to 16 MB of RIFF data and one million decoded frames,
rejecting excess frame counts before PCM allocation. Mono/stereo PCM16, PCM24
and float32 files use Rust only when their stored rate matches the context rate.
Compressed, unsupported-layout, oversized and rate-converted imports retain the
browser decoder. Callback-style decoding retains its browser overload; the native
promise path transfers the input buffer and retains an independent fallback copy.
Integer scaling is calibrated once per context and PCM bit depth using a two-frame
browser decode: positive full scale and an interior sample. Rust selects symmetric,
positive-full-scale division or float32 reciprocal multiplication to match the
host's rounding; unknown scaling retains browser decoding. Native CLI codec defaults
remain symmetric. PCM16/24 and float32 fixtures match WebView2 sample for sample.
Individual loop export uses native PCM16 with the original naming and byte layout.
Fallback export uses the captured PCM snapshot, even if the take changes while
waiting. Project SAVE awaits native loop/sampler WAV encoding through two runtime
call-site substitutions, listed in `crates/looper-desktop/assets/project-edits.json`.
All other serialization remains the source implementation, including the `.alp`
schema, metadata, base64 payloads and controls. The served-source test verifies
those substitutions plus the export adapter are the only project script changes.
`assets/export-edits.json`, generated by `scripts/generate-export-adapter.cjs`,
connects EXP ALL dry loops and six master/input/recorded-stem finalization sites
to the shared native encoding queue. Linear export fades preserve the source's
strict non-overlap rule for short buffers and differ from loop seam fades.
Bulk dry takes/names are captured before awaiting any worker job; individual
failures do not discard later files or leave the controls busy. WAV bytes,
download order/names and tracker performance sidecars retain source behavior.
The worker leaves input PCM unchanged. Fallback applies fades to a private
snapshot, including wide layouts and queue/capacity overflow. Live dry/wet bounce
and compressed performance decoding remain in the compatibility engine.
The shared encoding queue submits one worker request at a time and reserves
at most 32 jobs and four million PCM frames. Each job snapshots PCM at submission.
Unsupported layouts, larger takes and excess jobs retain source encoding; failed
jobs encode their saved snapshot through the original encoder and release their
queue reservation. A full desktop SAVE/load round trip verifies native assets,
twenty-track layout, tempo, feedback, dynamic recording and sampler settings.
Extreme finite values are bounded to floating-point
range, which is not speaker output protection.

Sample Lab destructive edits are queued through the Rust worker for mono/stereo
takes up to one million frames. Undo commits only after validation; normalization
retains raw-buffer identity and does not replace history for a no-op. Crop keeps
the source's selection alerts/zero-crossing snapping and uses a native frame slice.
Cleared/replaced sources, changed audio contexts, new capture/processing and changed
crop selection/preview reject delayed edits. SAVE WAV snapshots processed/raw audio
and filename before waiting for native encoding.
Sample Lab PROCESS uses vendored Rubber Band 4.0.0 R3 in two-pass offline mode,
with stereo coupling and independent pitch/stretch. Input and output each fit one
million frames, 8–192 kHz mono/stereo. Unsupported jobs and failures keep SoundTouch
JS fallback; neutral settings keep the original slice-only shortcut. Native engine
construction, allocation and processing are outside the audio callback. The original
source is unchanged; `stretch-edits.json` replaces asynchronous dispatch and stale
result guards only. See `TIME_STRETCH.md` for limits and validation.

MIDI input is now native through midir, on a dedicated owner thread with bounded
short-message queues and acknowledged push delivery. Original App mapping/learn,
synth and transport handlers remain unchanged. Hotplug generations and overflow
release held notes. This is input migration; audio scheduling remains in the
compatibility graph. See `MIDI.md` for limits and verification.

The scheduler is owned by the audio engine. The new control bridge synchronizes
messages from one control producer, but the desktop's compatibility audio is
not connected to this native device engine yet.
Cancellation is explicit per track. Source
tempo quantization, sustain, auto-next recording and dynamic prebuffering remain.
WAV files are decoded at their stored sample rate; adapters must resample before
loading into a device at a different rate. Extensible PCM16/24 and float32 imports
are supported, including left-aligned PCM in wider containers, with ordinary
mono/stereo speaker assignments. See `WAV_IMPORT.md`. Compressed/RF64 imports,
full 32-bit integer PCM and files wider than stereo are not implemented yet.

Standalone input recovery (Dropbox workspace): failed AudioWorklet module loads
are evicted from the per-context cache, allowing MIC/LINE retries to reload.
Concurrent attempts still share one load. Input replies recheck context ownership
after native IPC completes; replacement/closed contexts close capture before PCM
can reach the retired graph. The original UI assets remain unchanged. These
recovery fixes do not resolve the physical Windows device initialization failure.
