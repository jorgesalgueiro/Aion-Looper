# Plugin delivery plan

Added to the product requirements on 2026-10-15: Aion-Looper must also run as an
instrument/looper plugin in REAPER and Linux plugin hosts. “CALP” is interpreted
as CLAP. Standalone applications remain required. The original ASCII interface
and feature parity apply equally to the plugin editions.

## Formats and platforms

| Delivery | Target | Status |
| --- | --- | --- |
| CLAP instrument with audio input | Windows, Linux, macOS | Windows audio-effect prototype and completed-take state implemented; REAPER save/reopen passes; instrument MIDI/editor pending; Linux/macOS unverified |
| VST3 instrument with audio input | Windows, Linux, macOS; REAPER first | Shared processing boundary implemented; SDK wrapper pending |
| LV2 instrument/looper | Linux; REAPER and another LV2 host | Shared processing boundary implemented; descriptor, Atom/URID, state and UI wrappers pending |
| Standalone | Windows, Linux, macOS, iOS | Existing migration continues |
| AUv3 | Possible later iOS/macOS extension | Not part of the current plugin delivery commitment |

VSTi here means an instrument role; the planned VST format is VST3. Legacy VST2
is not an initial target. Windows binaries will not serve as Linux plugins: each
platform needs its own native bundles and validation.

REAPER lists VST3, CLAP and LV2 support in its [official feature list](https://cockos.com/reaper/about.php).
Use the current [official VST3 SDK](https://www.steinberg.net/developers/vstsdk/),
now MIT-licensed, with a pinned release and shipped notices when integrated.
CLAP's [official API](https://github.com/free-audio/clap) and
[LV2 specifications](https://lv2plug.in/ns/) govern their wrappers. CLAP now uses
pinned MIT-licensed `clap-sys` 0.5.0 bindings; choose the remaining wrappers after
checking editor integration and license compatibility with this GPL project.

## Shared engine contract

- Hosts own audio devices, MIDI ports, callback timing and transport. Plugin
  instances never open CPAL, JACK, ASIO or native MIDI ports. In REAPER the user
  chooses those drivers in REAPER. Standalone backends retain driver support.
- One instance owns one engine and independent loop/synth/FX/session state.
  No global PCM, mutable mappings or plugin-wide WebView profile directory.
- Start with stereo floating-point output, optional mono/stereo audio input and
  host MIDI/note input. Preserve headroom for the host mixer. Additional stems,
  buses and channel configurations are later additions, not silently enabled.
- Normalize host controls to sample offsets and typed actions. Stable automation
  IDs, gesture notifications, MIDI learn and original mapping behavior must agree
  across formats and the standalone UI.
- Keep a monotonic engine sample clock separate from host song position. Hosts
  can stop, seek, loop, preroll, omit transport and render offline. Implement and
  test explicit free-running versus host-synchronized behavior before exposing
  sync controls. Tempo/time-signature changes must not destroy recordings.
- Callbacks use bounded work and preallocated storage. File loading, stretching,
  state serialization and graph construction stay on workers/control threads.
  Plugin ABI code must contain panics and validate raw buffer/event layouts.
- Host save/restore must include versioned settings, mappings, arrangement and
  PCM assets. State handoff must preserve live audio ownership without locks or
  freeing buffers on the callback. `.alp` import/export remains supported.
- The original UI must be embedded in the host's supplied parent window. Extract
  assets/control messages from the current top-level Tao application. Host audio
  replaces Web Audio playback/capture; embedding the current compatibility engine
  would incorrectly create an independent audio clock and output path.

## Work order and acceptance

1. Shared device-free processor: implemented in `crates/looper-host`. It accepts
   borrowed planar float32 input/output, bounded ordered commands at sample
   offsets, master/monitor ramps and validated block-start transport. Processing
   has no added latency or hard clipping. Mono input duplicates to stereo;
   absent input is silent; monitoring defaults off. Malformed batches silence
   output without partly changing the engine. Zero-frame calls flush controls.
2. CLAP prototype: pinned ABI binding, discovery/descriptor, lifecycle, stereo
   audio ports, parameters and block-start transport translation are implemented.
   A Windows `.clap` bundle passes C-ABI record/play/isolation/reactivation checks
   and REAPER 7.43 discovery, two-instance loading and parameter metadata checks.
   Host MIDI/note ports, REAPER record/play/offline render and the original
   parented editor bridge remain pending. See `CLAP.md`; this prototype is not
   a feature-complete instrument release.
3. VST3 instrument: share processing/control/state with CLAP, implement component/
   processor/controller contracts and the appropriate instrument/audio-input bus
   declarations. Run the official validator and REAPER scan/play/render tests.
4. LV2: stable URI, bundle metadata, audio/control ports, Atom MIDI/time sequences,
   URID mapping, state/worker integration and an embeddable UI. Validate in REAPER
   on Linux and another LV2 host. See [LV2 Time](https://lv2plug.in/ns/ext/time)
   for transport units; normalize quarter-note values explicitly.
5. Complete shared native playback, sampler/synth/FX, scheduling, history and
   original UI parity. Verify host automation, undo, session reopen, missing
   assets, sample-rate changes, suspend/resume, bypass and transport policies.
6. Package platform bundles with licenses, installation instructions and host
   compatibility results. Measure physical latency, CPU deadlines and sustained
   sessions. Host-reported latency and tail must match the final DSP graph.

## What is verified now

Nine shared-host tests exercise stereo capture/play/retrigger offsets,
block-partition-independent ramps, whole-batch validation, zero-frame control
flush, finite headroom, transport discontinuities, lifecycle and instance
isolation. A measured allocator test covers 20 tracks and finds no allocations,
reallocations or frees during the measured processing/activation calls.

Current limits: 8–192 kHz, at most 65,536 frames per block and 1,024 control
events per block; exceeding these bounds returns an error. Track storage is
prepared before activation. Input/output are separate Rust borrows; ABI wrappers
must negotiate separate buffers or use preallocated input scratch for in-place
host buffers. Float64 buses are not implemented. Command state errors are reported
without aborting subsequent commands. Host transport is observed and seek jumps
are reported; host-synchronized loop positioning is not implemented yet.

The shared processor is now used by `looper-clap`; the standalone application
still retains its staged compatibility engine. A loadable Windows `.clap`
prototype is packaged. Host MIDI translation, complete synth/effects, plugin
full synth/FX state, editor embedding and further DAW validation remain pending.
Completed-take PCM/settings state is implemented; see `PLUGIN_STATE.md`.
VST3/LV2 loadable artifacts are not available yet.
