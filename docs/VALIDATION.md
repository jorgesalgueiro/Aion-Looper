# Foundation validation — 2026-10-18

Environment: Windows host, Ubuntu 24.04 under WSL; GCC 13.3.0, CMake 3.28.3,
Ninja 1.11.1, Python 3.12.3. Windows validation also used Microsoft Build Tools
2022 17.14.42 (MSVC toolset 14.44.35207), Windows SDK 10.0.26100.0,
CMake 3.31.6-msvc6 and bundled Python 3.12.14. JUCE 9.0.4 and CLAP extension revisions are pinned
in `cmake/Dependencies.cmake`. The first sections describe the 0.1.0 foundation; the 0.2.0 results follow below. Full feature parity is not yet achieved.

## Completed

- JUCE-independent Debug configure/build succeeded.
- CTest: `core_realtime_contract` and `original_ui_bytes` passed (2/2).
- Core test: queue capacity/overflow/FIFO, 100,000 concurrent SPSC messages,
  exact sample-32 gain command, 20 ms ramp reaching target, matching 64/128-frame
  output, finite output after infinite input, sample counter and no observed
  C++ new/delete while processing. C malloc/aligned allocation and JUCE internals
  are outside this instrumentation; no hardware latency claim is made.
- All 16 preserved reference files match SHA256 and the available reference
  checkout byte-for-byte. ZIP generation is deterministic; modified/unknown
  files are rejected by the packer.
- Linux JUCE configure and initial Debug build succeeded for Standalone, VST3,
  CLAP and native LV2, including VST3 moduleinfo and LV2 manifest generation.
- Unsupported macOS AU selection on Linux fails with the intended diagnostic.
- Linux compiler accepted the processor/APVTS and browser resource-provider code.
- Microsoft C++ Build Tools installation completed. Its final result requested a
  Windows restart; no automatic restart was performed. CMake works and a small
  Windows API program compiled, linked against WinMM/COM/User32, and ran successfully.
- Windows Debug core configure/build succeeded with MSVC; both CTest tests passed (2/2).
- Full Windows JUCE configure succeeded for Standalone, VST3 and CLAP with ASIO ON
  and WebView2 SDK 1.0.3485.44. This is not a Windows plugin binary/hardware test.

## Historical 0.1.0 package and remaining validation

Final Linux Debug build and CTest passed (2/2) after the packaging fix. The ZIP
`dist/Aion-Looper-0.1.0-Linux.zip` is 122,535,458 bytes and contains Standalone,
VST3, CLAP and LV2 artifacts, including VST3/LV2 metadata. Checks passed for
`GetPluginFactory`, `clap_entry`, `lv2_descriptor`, linked library resolution,
ZIP integrity, and exclusion of JUCE development SDK files from the runtime ZIP.
The local validation cache uses GNU thin intermediate archives to reduce writes
on the Windows-mounted drive (`CMAKE_CXX_ARCHIVE_CREATE=<CMAKE_AR> qcT <TARGET>
<LINK_FLAGS> <OBJECTS>`); the packaged products are normal ELF binaries.
At the 0.1.0 milestone, Windows binaries and browser/IPC execution had not yet
been tested and no Windows package was produced. The 0.2.0 results below supersede
those Windows limitations. Physical ASIO/Topping input, ALSA/JACK/CoreAudio, Apple
builds, DAW host loading, validators, signing and notarization remain untested.

The 0.1.0 engine was smoothed stereo pass-through, muted at startup. UI controls
were disabled, pending the native view-model adapter. Loop recording/playback and
its view adapter are implemented in 0.2.0 below. See `PARITY.md` for current scope.

## Native loop preview 0.2.0 — 2026-10-18

- `dist/Aion-Looper-0.2.0-win64.zip` contains the Release Standalone, VST3 bundle
  (including module metadata), CLAP, documentation and primary license notices.
  ZIP CRC/required-artifact checks pass; development libraries, object files and
  debug symbols are excluded. PE export checks find `GetPluginFactory` and
  `clap_entry`. These checks do not substitute for loading in a DAW/validator.

- Windows x64 Release Standalone, VST3 and CLAP compile successfully with ASIO ON
  using the bundled interface headers. Windows CTest passes 2/2 with the expanded
  native loop suite; Ubuntu/GCC Debug CTest also passes 2/2.
- Stereo record/readback, stop/restart, overdub feedback at 80%, substitution,
  20-track summing/mute, fixed-pool exhaustion/reuse, boundary fades, nonfinite
  capture, exact command sample boundaries and matching 64/128 output pass.
- Tests detect no callback C++ new/delete in capture/edit/play and an 8,192-frame
  20-track exercise. Queue overflow rejects, work is capped at 128 commands/block,
  and excess commands are retained. Same-rate reprepare preserves PCM; changed
  rates report cleared PCM; stale generations reject.
- Node frontend tests pass: all four reference presentation methods match (line
  endings normalized), 20 cards render, state actions match and fields are defined.
  The 16 original assets still pass byte/SHA verification.
- A dedicated Windows Standalone instance passed native WebView2/IPC tests at
  48 kHz: 20 original cards, valid snapshots, invalid parameter rejection, expired
  record-request rejection, audio-owner invalid-state acknowledgement, a harmless
  UI Stop All round trip, no legacy inline handlers/audio context, no startup
  overlay, readable status text, visible/unobstructed record control and disabled
  unported controls. Screenshots were inspected. Output stayed at zero; no live
  microphone capture was started. The test instance was closed afterward.
- The default browser startup overlay, logo sizing and sticky-scroll offset were
  adapted in the native presentation code. Reference files were not edited.

Physical ASIO/Topping duplex/underrun/latency testing, DAW plugin-host/validator
runs and Apple builds remain pending. The Linux core is tested for this milestone;
Linux native artifacts from 0.1.0 do not validate the new browser/processor code.
This preview is manual-timing and session-only: PCM is not serialized into DAW
state/projects and is cleared on sample-rate changes. Undo/export, complete device
routing, synths, tracker, FX, MIDI learn and full clock/host sync remain pending.

## Reference interface restoration 0.3.0 — 2026-10-18

- Removed the extra preview toolbar; session actions use original PROJECT MENU
  buttons, output volume uses MASTER OUTPUT and native input uses INPUT MIXER.
  Preserved original workspace/manual tabs, logo, stylesheet and loop-card markup.
  All 16 reference runtime files still pass byte/SHA identity verification.
- Added native exclusive SOLO with explicit mute preservation, manual-timing RTRG,
  DEL ALL and APVTS Auto-play after Rec. Volume/feedback update continuously with
  last-value coalescing while commands await the audio owner; feedback double-click
  resets to 80%. Original mode keys `+`/`-`, digits/Shift+digits, Space, `/`, 600 ms
  holds and blur/repeat/form-focus guards have automated coverage.
- Windows MSVC Release and Ubuntu/GCC Debug core CTest pass 2/2. New regression
  fixtures cover solo toggling/switching/mute restoration, clearing other tracks,
  retrigger sample phase, auto-play off, pool exhaustion and simultaneous recording
  shortage, clear-all pool recycling and no observed core callback C++ new/delete.
- Windows Release Standalone, VST3 and CLAP build with ASIO ON. Native WebView2 tests
  at 44.1 kHz pass layout placement, original tabs/mode keys, auto-play parameter,
  final coalesced slider value and SOLO on/off audio-thread acknowledgements,
  alongside previous parameter/expiry/20-card tests. Output remains muted and no
  microphone recording is started. The dedicated test app closes afterward.
- Windows ZIP `dist/Aion-Looper-0.3.0-win64.zip` includes these three formats,
  documentation and primary notices. ZIP CRC and required artifact checks pass.

This remains a partial migration. Sync is displayed OFF to match current manual
scheduling. New Linux native wrappers, Apple builds, DAW validation and physical
ASIO/Topping tests remain pending. Recorded PCM is still session-only; do not use
this preview as project storage. Full original functionality and production
readiness are not claimed.

## Original pan and varispeed controls 0.4.0 — 2026-10-18

- Original UI bytes remain unchanged. Existing Pan/Speed sliders, 1/2 and 1x
  buttons, Speed double-click reset and H shortcut now route to C++ state. The
  loop-card adapter takes pan/rate from native snapshots and displays effective
  duration. Original varispeed overdub/substitute restriction is retained.
- Native output uses the reference mono-fold/equal-power pan law, including center
  attenuation. Stereo capture remains unmodified. Clear now resets loop controls
  to reference volume=1, pan=5, speed=1 and feedback=0.8 defaults.
- MSVC Release and Ubuntu/GCC Debug CTest pass 2/2. Core fixtures cover fractional
  half-speed/wrap and double-speed reads, phase preservation, 20 ms monotonic pan
  and rate integration, pan endpoints/center, bounds, edit restrictions and clear
  defaults. A fractional 0.37x/2x fixture with pan/rate ramps produces identical
  output at 64/128 block sizes and no observed core callback C++ new/delete.
- Node tests pass original renderer identity, native pan/rate presentation and H
  dispatch. Windows native WebView2 tests pass pan 10/5 and speed 0.5/1 changes
  through original sliders to audio-owned snapshots, plus the preceding original
  layout/control tests. Test audio stays muted, tracks empty and no microphone
  recording starts. The dedicated test app closes after the checks.
- Windows Release Standalone/VST3/CLAP build successfully with ASIO ON. Local
  `dist/Aion-Looper-0.4.0-win64.zip` CRC/required artifacts/notices/docs checks pass.

Limitations: the fractional reader currently uses linear interpolation and can
alias when speeding up; it is not a commercial-quality antialiasing resampler.
Direct browser/offline audio waveform comparison and listening/hardware tests are
still pending. Pan intentionally follows the reference mono-fold stage, changing
previous previews' stereo playback routing. PCM/project save, export, undo,
synths, FX, MIDI learn and clock/host sync remain incomplete. New native Linux
wrappers and Apple targets are not validated by this core-only Ubuntu run.

## Direct reference audit 0.4.1 — 2026-10-18

- New `js_native_state_parity` CTest runs actual unchanged JS methods against a
  C++ probe: 408 commands / 8,160 loop snapshots. Windows MSVC Release and Ubuntu
  GCC Debug CTest pass 3/3, including asset identity and realtime contracts.
- The reference test first reproduced mute loss on clear and now passes after
  correction. Fractional pan truncation, exclusive capture entry, zero-frame finish
  and keyboard/stop/clear UI handling were corrected; original assets stay intact.
- Windows Release Standalone/VST3/CLAP compile with ASIO ON. Native WebView2 smoke
  tests pass original controls plus DEL ALL cancellation without sending a clear
  command. The dedicated app remains muted, never records microphone input and
  closes after testing.
- **Sound parity FAILS:** `node tests/AudioParityAudit.cjs <probe>` exits 1, with
  102/512 mismatched samples and maximum error 0.0424264117 in an isolated native
  overdub/reference circular-mix fixture. This is not hidden by the three passing
  CTest tests. See `REFERENCE_AUDIT.md` for fixture scope and unported features.
- The local Windows 0.4.1 ZIP includes the audit document. Runtime artifacts and
  ZIP integrity are checked; tests/probe/development files are not runtime payloads.

This audit does not establish full functionality or acoustic equivalence. The
next DSP acceptance work requires preallocated capture/finalization transactions,
reference DC/boundary processing, tapered/soft-clipped overdub, and broader
browser-rendered audio fixtures. Apple/DAW/hardware testing remains pending.

## Current capture/overdub repair components — 2026-10-18

Windows Release and Ubuntu Debug CTest pass 4/4 with the new unchanged-JS offline
DSP kernel comparison: 471 fixtures / 395,772 samples, maximum observed Windows
error 4.994e-9 (tolerance 1e-6). The initial DC-oracle extraction selected an earlier
buffer-copy loop; it was corrected to select the actual Remove DC Offset block,
then the comparison was rerun successfully. Original reference bytes are unchanged.

The separate native looper audio diagnostic was rerun unchanged and still FAILS
with 102/512 samples different. Offline kernels are not wired into capture yet.
A dedicated CI acceptance job retains this failure and feature additions are
frozen. No native GUI or runtime packaging is needed for these unused component
kernels; no new application behavior is claimed by the component test results.
