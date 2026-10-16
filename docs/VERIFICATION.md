# Verification — 2026-10-15

- Extensible WAV import update: 12 native WAV tests and all 77 desktop
  bridge/input/MIDI tests pass. All 600 original browser tests pass. Changed
  Rust files pass rustfmt; the bridge and packaged self-test pass JavaScript
  syntax checks. Independent fixtures verify PCM16/24 in wider containers,
  float32 headroom/signed zero, mono/stereo, malformed extensions and capacity
  limits. The desktop worker and WebView2 suite include additional checks, but
  those native/package checks have not completed for this update.
- Full workspace rebuild is blocked by Windows cloud-file errors, not a failing
  WAV assertion. Two attempts encountered temporary archive file locks. Moving
  build output to a temporary directory then exposed error 362, "The cloud file
  provider is not running", reading existing files including
  `crates/looper-core/tests/checkpoint.rs`, `crates/looper-audio/src/capture.rs`,
  Wry/URL dependency sources and GNU C++ headers. Starting the installed Dropbox
  client did not immediately restore those files. No missing source was replaced
  or deleted. The existing packaged executable still represents the earlier
  migration, not this import extension. Hydrate source/toolchain files before
  copying the workspace to an unsynced location and rebuilding. The desktop
  packager now honors `CARGO_TARGET_DIR`; see `WAV_IMPORT.md`.

- Dropbox input recovery update: all 75 desktop bridge tests pass, including
  failed/concurrent worklet load retries and replies arriving after context
  replacement or closure. The release package rebuilt successfully after one
  transient Windows object-file lock; its muted synthetic-input self-test passes
  all 120 checks. All 99 original UI baseline hashes remain unchanged. Physical
  microphone initialization has not been fixed or revalidated by these tests.

- Current package: direct `Rust-Looper.exe`, without PowerShell hosting or the
  temporary desktop DLL/launcher. GUI integration tests run the actual executable
  and use synthetic input: all 119 checks pass in `verification/desktop-ui-standalone.json`.
- Physical microphone: the standalone executable still fails initialization with
  0x80070057. Windows microphone access reports Allowed. Shared WASAPI, raw mode,
  IAudioClient3 and native AudioGraph input creation fail in standalone probes.
  Report: `verification/windows-input-diagnostics.log`. Kaspersky is installed;
  its application-rule export is protected and was not inspected. The error
  pattern matches a vendor-documented Kaspersky microphone block; this is an
  evidence-based inference, not a confirmed read of the installed policy.
  See `NATIVE_AUDIO.md` for source links and diagnostic commands. No security
  settings were changed or bypassed.
- Historical evidence only: `verification/physical-input-host.json` recorded
  finite nonzero USB microphone PCM at 48 kHz stereo in a previous hosted test,
  with muted output, discarded audio and zero reported transport xruns. That
  removed host is not part of the current product or its validation gates.

- Copied local reference: 99 files, SHA-256 comparison, zero differences.
- Original JavaScript behavior suite: 600 passed, zero failed (Node 24).
- Native workspace: 127 tests passed on Windows using Rust 1.99.0, GNU target
  and the bundled Rust linker: 10 core unit tests, 11 buffer tests, 5 scheduling
  tests, one core allocation test, 9 WAV codec tests, 4 CLI workflow tests,
  3 CLI capture tests, 8 callback tests, 8 input capture tests, 4 configuration
  tests, one combined device-renderer/input allocation test,
  six desktop asset tests, eleven native worker bridge tests, three native
  input service tests, four native Rubber Band tests, five MIDI transport/hub
  tests, four native MIDI service tests, nine device-free host processor tests
  nine CLAP ABI/allocation tests, four atomic checkpoint tests and eight state tests.
- Shared plugin-host processing tests verify planar stereo/mono/silent buffers,
  sample-offset control ordering, recording/playback, block-independent automation,
  malformed-block rejection, instance isolation, transport discontinuities and
  zero-frame flush. The measured 20-track processing path has zero heap operations.
  The CLAP wrapper now exercises the same processor; the original plugin editor
  is not implemented yet.
- Packaged Windows CLAP prototype loads through its real exported `clap_entry`.
  The custom test host verifies deterministic stereo capture/replay, in-place
  buffers, isolated instances, reset and repeated activation with retained audio.
  The measured CLAP processing/parameter-flush/reset path performs zero heap
  operations while recording twenty tracks. Build/ABI log: `verification/build-clap.log`.
- REAPER 7.43/x64 passes 29 scan/load checks with the packaged binary copied into
  an isolated test profile: discovery, two instances, names and all 22 parameter
  names. No playback, physical recording or normal project edits were requested.
  Report: `verification/reaper-clap-portable/reaper-clap-smoke.json`.
  Initial profile/discovery attempts failed, then passed with explicit test
  configuration and plugin search setup. A deferred parameter round-trip attempt
  stalled and its test process was terminated. Parameter delivery, REAPER audio
  processing/render, official clap-validator and plugin GUI remain unverified.
- Native completed-take state round-trips exact PCM/settings through partial CLAP
  streams, rejects corrupt input transactionally and saves concurrently with
  playback. Capture/punch saves fail explicitly. Active restore is installed on
  the main thread during host restart, with zero PCM reclamation in processing.
  REAPER additionally passed project save/reopen with an exact matching empty-take
  state chunk: 33 checks in `verification/reaper-clap.json`. A subsequent hidden
  repeat stalled before progress; the passing report retains the tested binary
  hash from before the final automation snapshot refinement. The final package
  passes its updated C-ABI restore checks. See `PLUGIN_STATE.md`; REAPER recorded-PCM
  render remains unverified.
- Desktop JavaScript adapter suites: 71 passed. Native MIDI tests cover exact
  messages, hotplug generations, held-note release, bounded overflow, document
  ownership, push acknowledgements, replay rejection and page cleanup.
  The packaged UI checks exercise the original note/global/synth/transport routes,
  CC learn, reconnect/rebinding and native push using a synthetic MIDI device.
  Read-only Windows MIDI discovery found zero physical inputs; hardware MIDI
  behavior and audible synth output remain unverified.
- The allocation test exercises 20 tracks recording, playing, overdubbing,
  substituting, muting, stopping, clearing and scheduling. During those operations the
  allocator records zero allocations, reallocations or frees.
- Stereo channels, circular phase, feedback, substitute punch boundaries,
  muted transport, recording capacity, clearing, callback partitioning,
  invalid requests and nonfinite/extreme input were tested.
- Buffer fixtures are generated by executing the unchanged source with
  `scripts/generate-dsp-fixtures.cjs`. Seam crossfades, normalization, circular
  overdub/substitute and latency rotation match within 0.000002 per sample.
- PCM16 export matches the original encoder bytes exactly. PCM24 sign extension,
  saturation and quantization, mono padding, float32 headroom, unknown RIFF
  chunks, truncation, bad configuration and nonfinite samples were checked.
- Five export-fade fixtures execute the original `AudioEngine.applyFades` and
  WAV encoder for long, short and tiny takes, zero fade and a fade longer than
  the take. The worker's faded PCM16 WAVs match byte for byte. Invalid durations
  are rejected before changing PCM. Bridge tests execute original EXP ALL,
  master-only and input-only methods with runtime adapters, checking dry snapshot
  ownership, six source paths, names/order, tracker sidecars, per-file failure
  recovery, malformed native output, mono/wide layouts and queue overflow.
  Actual WebView2 export buttons pass with synthetic master/input and loop/drone
  stem recordings. Downloads are intercepted; no live master capture is started.
- CLI integration checks exercise WAV import, normalization, reversal, doubling,
  export-format conversion, output overwrite protection and input preservation.
- The native renderer produced `verification/native-loop.wav`: five seconds,
  stereo float32, 48 kHz. Independent Python standard-library inspection checks
  RIFF metadata, repeated takes, exact scheduled silence and finite samples.
- Rust formatting and Clippy checks passed for all targets in this workspace.
  Clippy was run with warnings treated as errors.
- The Windows GNU CLI now builds with native audio dependencies. Device
  enumeration detected WASAPI endpoints. Zero-gain output checks at 48 kHz
  stereo float32 passed with zero stream errors for default and Fixed(480)
  buffer requests. Both observed callbacks up to 1,056 frames.
- New callback tests check queue work/backpressure, cross-thread delivery,
  master ramps, scheduled failures, mono routing, integer conversion, malformed
  buffers, output bounds and sparse track storage.
- The device-renderer allocation test processes 20 tracks and control messages
  plus input capture/consumption, overflow and malformed input, with zero
  allocations, reallocations or frees in the measured region.
- Native capture tests cover float headroom and signed zero, mono duplication,
  stereo float32/float64/int32/int16/uint16 conversion, exact device-clock gaps
  after overflow, malformed blocks, invalid samples, cross-thread handoff and
  independence from callback partitioning. Synthetic capture round-trips through
  float32 WAV with exactly the selected frame count. CLI tests reject existing
  files and malformed options before any input-device startup. No physical
  input device was opened by these checks.
- `cargo check -p looper-cli --features jack` and the desktop JACK feature check
  passed on Windows GNU. JACK server
  use and ASIO feature compilation have not been validated.
- All 99 original UI/test files still match the saved baseline hashes. Regenerating
  the native DSP fixtures from the original source produces identical fixture bytes.
- Sixty-two JavaScript bridge/input regression tests pass, covering stale take/context/document
  ownership. New interactive MULT checks cover original-finalizer integration,
  history saved only on commit, recorder cleanup, cleared-take cancellation,
  replacement recorder/context ownership and snapshot-preserving fallback.
  Rust output is compared with fixtures generated by running the unchanged
  `LoopManager.processMultiplyData`, including DC removal and its 8 ms splice.
  The actual WebView2 MULT button passes capture, append, prefix preservation
  and sample-exact undo/redo. Other bridge checks cover stale take/context/document
  ownership and full punch finalization. New fixtures execute the unchanged
  `LoopManager.processOverdubData` for overdub and substitute with zero, short
  and longer-than-take latency corrections. Rust DC removal, rotation/fades and
  mixing match within 0.000002 per sample. Worker payload tests enforce combined
  frame capacity and valid latency. Frontend tests verify deferred history/commit,
  PCM/latency snapshots, original fallback parity and obsolete-recorder rejection.
  Twelve new fixtures execute unchanged `LoopManager.processRecordedData`: free
  capture at zero/short/full latency, dynamic trim/short/invalid-bar, precision
  extraction with overlapping tail, late start, short capture and tiny target,
  plus synchronized fallback folding/short capture. Rust matches within 0.000002
  per sample. Worker tests reject bad modes, empty captures, invalid latency and
  excess allocation before creating output PCM. Frontend tests cover frozen
  frame metadata/PCM/BPM, exact original fallback samples in all twelve cases,
  wide-channel fallback and cancellation after take/graph/context/recorder or
  transport changes. The actual Record/Stop UI asserts Rust first-recording DSP.
  The normal executable passes the actual overdub/substitute controls and exact
  undo/redo through this combined Rust operation. Other checks cover stale
  rejection, serialized mixes, malformed PCM fallback and unsupported layouts.
  Native binary transport preserves float32 bits, including signed zero. Native
  mixing matches the original golden fixtures for overdub and substitute.
  Stopped MULT preserves mono/stereo channels, sample rate and consecutive undo
  states across rapid presses. Sampler transforms stop playback, preserve the
  original source buffer, serialize edits and reject replies after import or play.
  Worker errors and unsupported channel layouts retain the source behavior.
  Sample Lab checks cover deferred undo, no-op normalization, normalization
  identity, rapid serialized reversal, crop selection/ZC behavior and exact undo,
  source/context/preview/selection cancellation, snapshot fallback and native WAV
  save names/rates. Native slice matches original floor boundaries without fades.
  Four further frontend checks execute native stretch dispatch from the actual
  source method, frozen metadata, stale source/context/in-place-normalization
  rejection, private fallback snapshots and unsupported/neutral shortcuts.
  Native Rubber Band tests measure pitch independently of output duration,
  stereo relationship RMS error, mono channel identity, final-tail energy,
  silence/short takes and invalid/capacity metadata. The packaged PROCESS control
  passes actual R3 engine identification, exact 1.5x duration, octave pitch,
  source preservation and processing-state cleanup. Actual Lab edit/undo/save
  controls pass too. Auditory comparisons are not yet verified.
  WAV checks cover native import ownership transfer, mono layout/rate preservation,
  browser fallback for rate conversion, compressed input and callback overloads,
  independent fallback bytes after native import failure, native loop export and
  export snapshots after a take changes. Worker codec tests exercise PCM16/24/
  float32 mono/stereo import and byte-identical original PCM16 loop export.
  Integer-scale probes are cached per context and format; tests cover symmetric,
  positive division and positive reciprocal multiplication, plus fallback for
  unknown results. The native file codec keeps
  its symmetric scale unless the desktop explicitly requests browser compatibility.
  Direct WebView2 comparison found positive PCM16 decoding uses a rounded float32
  reciprocal of 32767. Peak-only calibration had missed a one-float-step interior
  difference which became a PCM16 quantization difference after fading. Two-frame
  calibration now detects this arithmetic. All 65,536 PCM16 integer inputs match
  the original WebView2 decoder exactly; existing PCM24/float32 fixtures also pass,
  including float headroom and signed zero. CLI default scaling remains symmetric.
  A twenty-loop project save preserves the original schema, metadata and embedded
  loop/mono-sampler PCM16 bytes while serializing native encode requests. Further
  checks exercise captured snapshots, failed-job recovery and overflow fallback
  after 32 queued jobs. An asset test verifies the served project script differs
  from the source only at the save-audio and export-finalization adapter regions.
- The packaged Windows graphical executable passed its real WebView2 self-test:
  original stylesheet, secure audio context, offline SoundTouch, actual START
  button, ten loop controls, WAV import through Rust, waveform-click native export
  matching the original encoder bytes, normalization through Rust, play/mute/
  stop/clear, AudioWorklet recording, Rust overdub/substitute/merge finalization,
  sample-exact undo/redo after each mix, the actual MULT button with sample-exact
  doubling and undo/redo, sampler reversal/normalization and stored sample-rate
  preservation, expansion to 20 tracks and all four workspace
  tabs. Capture used a Rust synthetic oscillator; physical microphone startup was bypassed and
  the master gain stayed at zero. Report: `verification/desktop-ui-standalone.json`.
  The actual SAVE control and full project reload pass with native WAV payloads,
  original ALP naming/schema, twenty-track/tracker layout, no repeated seam fade,
  mono sampler audio, tempo, feedback, dynamic-recording metadata and sampler
  settings. SAVE/load controls recover their normal state after completion.
- Native desktop input tests exercise session ownership, a four-source limit,
  closure on stream errors and rejection of synthetic capture in normal mode.
  Input adapter tests cover device identity, first-pair routing on wider interfaces
  and packed int24 samples. Frontend/worklet checks cover packet continuity,
  in-flight stop, context/page cleanup, suspended-audio flushing, mono/stereo
  headroom, prefill, overflow, underruns and bounded clock correction.
  The served-source test verifies only capture/device-selection call sites change.
- WebView2 recording now uses Rust-generated synthetic native input. The actual
  MIC/LINE and MON paths, stereo recording balance, overdub/substitute, L/R routing,
  device switching and removal pass. Monitoring stays off by default; output was
  muted. No physical microphone was opened. Physical capture and long drift/latency
  tests remain pending.
- The GNU desktop build requires `WebView2Loader.dll` beside its executable.
  The build script now packages that dependency. The original stop operation
  has a 25 ms teardown timer; integration checks wait for the resulting state.

Initial browser test invocation from the repository root produced four path
errors because those upstream tests resolve `./Modules/effects.js` against the
working directory. Running the suite from `ui/`, as documented and configured
in CI, passed all 600 tests. No reference code was changed to fix these paths.

The default MSVC test build could not link because Visual Studio C++ tools
are absent. The project-local GNU toolchain successfully compiled and ran the
tests instead. No system PATH or Visual Studio installation was modified.

Not yet verified: audible playback/monitoring, sustained physical capture, JACK/ASIO driver use, macOS execution, iOS device
execution, latency, sound quality of the full DSP chain, exact screenshot
comparisons, all project dialogs, session compatibility, long performance sessions
or hosted CI. The desktop UI is runnable, but full native audio parity remains
unfinished. No Git commit or push was performed.

2026-10-16: Windows and Ubuntu full workspace tests passed (131 Rust tests per
platform); strict Clippy passed on both. Ubuntu additionally passed all 77 bridge
and 600 original UI tests, JACK compilation, Linux package/CLAP ABI checks and
130 muted graphical integration checks. See [audit](AUDIT.md) and
[Linux instructions](LINUX.md). Hardware latency/capture remains unverified.


Input follow-up: native input lifecycle, device enumeration and physical capture
were tested on Windows/E2x2 and Ubuntu/WSLg. See [input testing](INPUT_TESTING.md)
for fixes, passing checks and the remaining Windows post-recording underrun.


Responsive first-recording PCM transfer: 81 desktop JS regressions pass and
Windows physical capture no longer reproduces the earlier finalization gap
in short local trials. See [transfer verification](RECORDING_TRANSFER.md);
WSLg still reports a gap and long-session testing remains pending.

