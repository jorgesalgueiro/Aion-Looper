# Aion Looper CLAP prototype

This is a native processing prototype with the host's generic parameter editor.
The original ASCII editor, MIDI mapping, synth/FX and undo
are not connected yet. It is currently advertised as an audio effect, not a
finished instrument. VST3 instrument and LV2 builds remain planned.

## Windows bundle

Build with `scripts/build-clap.ps1`. The resulting file is
`dist/Aion-Looper-CLAP-Windows/Aion-Looper-Prototype.clap`. Its identifier is
`org.jorgesalgueiro.rust-looper.prototype`, deliberately separate from future
production IDs. This build is Windows x64; it is not a Linux/macOS binary.

To try it in REAPER, add the containing directory to REAPER's CLAP search paths
and rescan. Insert **Aion-Looper Prototype** on a track and use the generic
parameter controls. No plugin directories or normal REAPER settings are changed
by the build script. Each instance gets one stereo input and one stereo output.
Audio devices and input routing belong to REAPER. The plugin opens no devices.

## Current controls

| Parameter | ID | Values |
| --- | --- | --- |
| Master gain | 0 | 0–4, default 1 |
| Input monitor | 1 | 0–1, default 0/off |
| Loop 1–20 command | 100–119 | Idle, Record, Finish, Play, Stop, Clear, Overdub, Substitute |

Route an audio item or host input to the track. Set a loop command to **Record**,
then **Finish**: the recorded loop plays. **Stop** stops it; **Play** restarts it;
**Clear** empties it before another recording. **Overdub** and **Substitute**
require a playing loop, and **Finish** exits those modes. Idle changes the
parameter value without changing the loop. State-incompatible commands are
ignored by the core. This explicit command interface is temporary, not the
original pad/hold/quantize workflow. Automation uses host-provided sample offsets.

The two gain controls use 64-frame ramps during processing. Monitoring defaults
off to avoid duplicate dry input. Output preserves float headroom and does not
clip at unity. Initial/re-activation gains are restored without a ramp. Parameter
values are atomically readable by the main thread while audio processing runs.

## Limits and lifecycle

- 8–192 kHz, float32 stereo ports, up to 65,536 frames and 1,024 incoming events
  per processing call. No float64 ports, extra buses or host note ports yet.
- Eight seconds of recording storage per loop, allocated during activation:
  about 59 MiB at 48 kHz and 234 MiB at 192 kHz, plus block scratch. Reaching
  capacity finishes the recording. This fixed prototype limit is not the final
  storage design.
- Input is copied into preallocated scratch before outputs are borrowed, including
  when the host reuses input memory. Output channels must not overlap each other.
- Deactivation retains recordings in the same instance. Same-rate reactivation
  accepts a different maximum block size. A sample-rate change is currently
  rejected; host rate-change resampling is pending.
- Host reset stops capture/playback and cancels scheduled commands without
  clearing recorded PCM. A subsequent Play can restart a saved take.
- Block-start transport is translated to the shared engine's transport view.
  Host-synchronized playback, tempo ramps and transport changes within a block
  remain unfinished. The engine's sample clock runs independently of song seeks.
- Native plugin state now saves/restores completed takes and settings. Active
  restore uses the host restart cycle. Capture/punch saves fail explicitly;
  finish first. See `PLUGIN_STATE.md` for format, concurrency and limits.
- Raw pointers are confined to the CLAP crate, using pinned MIT-licensed
  `clap-sys` 0.5.0 (CLAP 1.2.2 ABI). Hosts must follow CLAP pointer lifetimes and
  serialized lifecycle/process/flush threading. Callback panics are contained
  at the ABI boundary; there are no locks, device calls or heap operations on
  the measured normal processing path.

## Verification

The Rust ABI tests cover discovery, ports, parameter metadata/text, sample-offset
capture, replay, reset, suspension/reactivation, separate instances, in-place
buffers, malformed events/transport and active/inactive parameter flush. The
measured callback/flush/reset test records all twenty tracks with zero allocator
operations. `cargo run -p looper-clap --example host -- <plugin-file>` loads the
actual shared-library export and checks deterministic stereo record/play,
instance isolation, reset and reactivation without opening hardware or playing
sound. This test is also run on the packaged Windows `.clap` by the build script.

REAPER 7.43/x64 also passed 33 isolated checks: discovery, two native
instances, names, all 22 parameter names and exact state after project save/reopen.
The host project had no takes; recorded PCM restore passed the C-ABI test.
The test used a copied executable
with an explicit configuration/resource folder under `verification/`, muted
master output and unarmed tracks. Report:
`verification/reaper-clap.json` (with tested binary hash).
That report predates the final automation snapshot refinement; the final package
passes C-ABI restore checks. A subsequent hidden host repeat stalled before progress.
The first attempt did not isolate resources and stopped before testing;
explicit `-cfgfile` fixed isolation. Scan configuration then needed the native
CLAP path and test plugin folder. A deferred parameter round-trip attempt stalled
in the hidden test session and was terminated; successful scan/load checks do
not verify host parameter delivery or playback timing.

Full CLAP specification compliance is not established. REAPER playback/render,
the official clap-validator, Linux/macOS execution, audible output, long sessions
and the embedded editor remain separate validation gates. Hosted CI includes
Windows/Linux/macOS shared-library tests but has not been run here.

Primary references: [CLAP API](https://github.com/free-audio/clap),
[CLAP validator](https://github.com/free-audio/clap-validator),
[pinned Rust bindings](https://docs.rs/clap-sys/0.5.0/clap_sys/).

## Linux bundle

Build the Linux desktop, CLI and CLAP package with `bash scripts/build-linux.sh`.
The plugin is `dist/Aion-Looper-Linux/Aion-Looper-Prototype.clap` and passed
its exported-ABI integration test on Ubuntu/WSL2. See [Linux setup](LINUX.md).
