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

## Worker capture/finalization integration 0.4.2 — 2026-10-18

Latest results supersede the older direct-write sound failure above. The original
raw circular-mix regression remains in the kernel suite (now 472 cases / 396,284
samples). The connected gate adds reference capture conditioning and waits for
actual native worker commits; it passes 126 cases / 965,826 mono/stereo frames at
8/48/96 kHz. Maximum error 7.395e-8 against unchanged 1e-6 tolerance. Scope excludes
browser envelopes/resampling, latency correction, sync and hardware audio IO.

MSVC Release and GCC Debug CTest pass 4/4. No observed audio-thread C++ new/delete
in capture, job publication/commit, play/edit, clear/stop/cancellation and 20-slot
playback. The watcher is thread-local so legitimate worker allocation is excluded;
a forced worker bad_alloc checks rollback and error reporting. Stop during saving,
clear-before-worker pickup, 32 repeated canceled edits, full resident/take budgets,
rate reset and committed playback/pan/rate command invariance at 64/128 pass.

AddressSanitizer/UBSan CTest pass 4/4. TSan core tests pass using process-local
`setarch x86_64 -R`; default WSL launch reported unexpected memory mapping. The
worker uses pinned capture blocks, immutable clip leases and worker-only deletion.
These tools do not prove the absence of every race or instrument JUCE/drivers.

The final native WebView2 test passes original controls, a completion-message
SAVING fixture, cancellation confirmation and explicit Stop All audio acknowledgement.
Two earlier fixture attempts did not intercept editor requests; the test was fixed
without weakening the presentation assertion. The old Stop All enablement wait was
also replaced with request-ID acknowledgement because Stop remains enabled while
busy. The dedicated app never captures microphone audio, stays muted and closes.

Windows Release Standalone/VST3/CLAP build with ASIO ON. Local 0.4.2 preview package
CRC, format and notice/audit inclusion checks pass. No production/driver/listening,
DAW/Apple or complete-reference parity claim is made. No other feature was added.


## Command ordering regression (2026-10-18)

The editor previously returned early while a loop command awaited audio-owner
acknowledgement. Rapid keyboard gestures could disappear; Stop could also be
removed before a paused audio device resumed. The bounded message-thread command
queue now serializes gestures and resolves toggles against acknowledged snapshots.
Slider values coalesce; Stop removes unsent actions and takes priority, surviving
pauses within the same audio generation. Other queued actions expire after three
seconds; configuration resets and editor closure discard unsent work. An already
accepted audio command still follows the existing backend acknowledgement contract.

Frontend tests exercise burst ordering, coalescing, Stop, pause/resume, expiry,
generation reset, 128-entry overflow, close and rejection recovery. Native WebView2
smoke additionally clicks MUTE twice within one JavaScript turn and requires both
audio-owner acknowledgements and the original final mute state. It passes with
muted output and no recording. Windows core/state/DSP and committed PCM gates pass;
Ubuntu core/state/DSP, committed PCM and frontend gates also pass. Original UI assets remain untouched.


## 0.4.3: original NORM action (2026-10-18)

The current capture and command-ordering gates passed before this feature began.
NORM now uses the original button, shared stereo peak ratio and reference silence/
unity thresholds. Its full scan and copy run on the worker, with immutable source
leases and block-boundary commit. Live phase and control values are retained;
Stop prevents late playback, and clear/reset cancel the private result. This is
available on playing/stopped clips while the single capture/worker slot is free.
No audio persistence or undo was added.

- Windows Release and Ubuntu core CTest: 6/6. Existing capture/reference state
  regressions remain; frontend commands and connected normalization are now CTest
  entries. The latest phase and normalized 64/128 regressions also pass on Ubuntu.
- Kernel unchanged-JS comparison: 544 cases / 507,428 samples, max absolute error
  4.994e-9 at unchanged 1e-6 tolerance. This retains all earlier mixer cases and
  adds 72 linked-stereo normalization threshold fixtures.
- Connected normalization: 42 mono/stereo cases / 321,942 frames, lengths
  1-48,001, 8/48/96 kHz. Windows max error 6.135e-8 at tolerance 1e-6. The oracle
  executes unchanged reference capture conditioning followed by normalizeBuffer;
  the native probe records, finalizes, normalizes asynchronously and renders PCM.
- Lifecycle tests: idempotence, live phase, stopped intent, controls/mute, full
  resident budget, worker allocation-failure rollback, 32 clear cancellations,
  reset, empty/active-capture rejection and normalized 64/128 playback. Audio
  thread C++ new/delete watch remains zero; worker allocation is intentional.
- Ubuntu AddressSanitizer/UBSan: 6/6. ThreadSanitizer core tests pass using
  process-local `setarch x86_64 -R`; no race report or global ASLR change.
- Native Windows WebView2: the original NORM is disabled for empty/saving slots.
  A restored snapshot-only clip fixture enables it and verifies its IPC reaches
  the real audio owner, which correctly rejects the absent clip. Rapid double
  MUTE, existing controls, muted output and no legacy AudioContext checks pass.
  The smoke test does not capture a microphone or establish hardware audio parity.
- Windows Standalone, VST3 and CLAP targets build with ASIO enabled. Physical
  drivers/listening, host validators and Apple/mobile targets remain unverified.


The unchanged connected capture audit also passes after the NORM integration on
Windows and Ubuntu: 126 cases / 965,826 frames, max error 7.395e-8 at tolerance
1e-6. Package validation caught stale Windows 0.2.0 resource metadata. CMake now
adds JUCE's generated Info.txt dependency to its resource rule; incremental rebuild
regenerates it, and Standalone/VST3 file properties now report 0.4.3. The Windows
ZIP is regenerated from those binaries and checked for CRC integrity, required
formats, current documentation and exclusion of intermediate/test artifacts.


## NORM follow-up: unchanged audio must keep its envelope (2026-10-18)

A wider live-playback test found a bug beyond the committed-PCM comparison:
repeating NORM on an already unity-peaked loop restarted the transition blend,
although the reference normalizeBuffer returns false and leaves audio untouched.
The regression failed before the repair with "no-op normalization must leave live
PCM and playback envelope unchanged". New features were held while repairing it.

The worker now publishes the reference kernel's changed/no-change result alongside
its immutable PCM result through the existing release/acquire job-state handshake.
An unchanged result retains the current transition envelope as well as phase;
changed audio and capture edits retain their existing smoothing behavior. No scan,
allocation, mutex or extra syscall was added to the audio callback.

The regression compares exact stereo output with an untouched native control loop
at source phases 2 (attack), 21 (steady playback) and 511 (wrap). The fixture first
drains previous playback tails so asynchronous worker wait time cannot influence
the starting envelope. The corrected Windows core test passed three consecutive
runs. Existing reference thresholds, linked-stereo PCM, capture, cancellation,
allocation failure and 64/128 tests remain unchanged.

Only the incremental Windows standalone test binary is updated for this repair.
No new ZIP, plugin package or version bump is produced. Existing packaged 0.4.3
artifacts predate this fix; use the current source build when testing it.


Final repair gates: Windows Release and Ubuntu core CTest pass 6/6. Both connected
capture audits retain 126 cases / 965,826 frames with maximum error 7.395e-8 at
unchanged 1e-6 tolerance. The corrected fixture also passes Ubuntu ASan/UBSan and
ThreadSanitizer core tests (process-local setarch invocation as above). Hardware,
DAW and full browser playback equivalence remain outside these results.


## Original Rvers control (2026-10-18, development source follow-up)

Capture, NORM and the repeated-NORM envelope regression passed their gates before
this feature began. The original loop Rvers checkbox now drives a native boolean
command. It latches direction on the next play/retrigger, retaining the current
source while a toggle is pending. Stored PCM remains forward. Reversed reads use
source indices rather than an audio-thread copy or reverse allocation.

Reference interaction matters: getReversedBuffer makes an independent source
copy. NORM changes stored PCM while that playing reversed copy remains unchanged.
The native engine transfers the old clip lease into a single held source per slot
until restart/stop/clear/reset, with worker-only destruction. Successful edit
finalization restarts using the requested direction. Failure keeps previous PCM
and active source/phase. No extra UI or other DSP effect was enabled.

Validation:

- Windows Release and Ubuntu core CTest pass 7/7, including the new reverse gate.
  The original 408 state traces remain, with 100 reverse/clear traces appended:
  508 traces / 10,160 snapshots compare the actual unchanged Loop/UI methods.
- Reverse PCM: 30 mono/stereo cases / 201,918 frames at 8/48/96 kHz, lengths
  1/4/17/512/4097. Actual unchanged toggle and graph build/retrigger routines run
  with audio-node IO stubs; source samples come from getReversedBuffer. Maximum
  Windows absolute error 1.029e-7, tolerance unchanged at 1e-6. Cases cover pending
  toggles, forward/reverse restarts, wrap, NORM source identity, fractional rates,
  and wrapped overdub/substitution. Fractional reads use an explicit linear model;
  this is not a browser-resampler/envelope/scheduled-start integration test.
- Core lifecycle/allocation tests include capture autoplay, stop/play, retained
  reverse PCM after NORM, forced NORM/edit allocation failure, current source/phase
  preservation, Stop during finalization, 32 cancel/clear cycles, prepare reset,
  20 simultaneous held sources and full resident budget. No callback C++ new/delete
  is observed. Timestamped reverse toggles/retriggers also preserve exact output
  across 64/128 blocks, including normalized clips. Earlier regressions remain.
- The unchanged capture PCM audit passes on Windows and Ubuntu: 126 cases /
  965,826 frames, max error 7.395e-8, tolerance unchanged at 1e-6.
- Ubuntu ASan/UBSan core and ThreadSanitizer core tests pass. ThreadSanitizer uses
  the previously documented process-local setarch invocation; no global settings
  are changed. Both compile the final failure/Stop cases.
- Windows native WebView2 smoke passes Rvers on/off acknowledgements and checkbox
  reflection, strict boolean IPC validation, original controls and no legacy
  AudioContext. Test output stays muted and no microphone recording starts.
- Only the incremental Windows Standalone test app is rebuilt. No package,
  plugin-format build or version bump is produced. Existing ZIPs predate these
  follow-ups. Hardware/DAW/Apple validation and full application parity are pending.


## Undo/Redo (2026-10-18, incremental source build)

Reverse and the preceding feature gates passed before starting this feature.
Original per-loop/global buttons and Ctrl/Cmd+Z / Shift+Z redo now reach C++.
Successful edit finalization transfers an immutable old PCM lease into a five-deep
history; restore swaps handles without full-clip work or allocation on audio.
The 50-entry global order follows the original JS, including eviction and branch
rules. NORM is not undoable as a new step, matching reference behavior. Stable
playing/stopped tracks can restore; the same track's active edit is guarded.

- Windows Release and Ubuntu CTest pass 8/8. The new history oracle executes the
  unchanged JS history, cloning and DSP methods. Nineteen cases compare 828
  metadata snapshots across all 20 slots/global heads and 666,144 PCM frames at
  8/48/96 kHz, lengths 17/512/4097. Eight edits/five restorations test local eviction;
  120 edits over 20 tracks test global ordering/cap and local/global interaction.
  Substitution, NORM, branch replacement, clear and exhausted-history no-ops remain
  in the fixture. Maximum Windows error 1.249e-7, tolerance unchanged at 1e-6.
- Core tests verify exact stereo restoration, stopped state and preserved controls,
  failed/canceled edit atomicity and redo retention, Stop during finalization,
  unrelated-track capture, reset, 20 tracks at maximum history depth with retained
  reverse sources, and cancellation/clear of that ownership graph. The expanded
  fixed retirement queue holds the worst-case lease burst. The audio-thread C++
  new/delete watch remains zero; snapshots intentionally retain extra PCM memory.
- Timestamped Undo/Redo/global restores and reverse latching produce identical
  output and acknowledgements at 64/128 blocks. The first timing fixture generated
  edit inputs from absolute engine clocks, which vary while waiting for workers;
  fixing it to use identical relative input frames removed that invalid comparison.
- Unchanged capture audits pass on Windows/Ubuntu: 126 cases / 965,826 frames,
  maximum error 7.395e-8, tolerance unchanged at 1e-6. Reverse and NORM gates remain.
- Ubuntu ASan/UBSan and ThreadSanitizer core tests pass, including maximal history
  clear/cancel. ThreadSanitizer uses the documented process-local setarch launch.
- Native WebView2 checks original loop/global controls and labels plus all shortcut
  routes using restored snapshot-only history fixtures. The real audio owner
  rejects those absent clips; no microphone is captured and output stays muted.
  A new empty-shortcut regression failed before its fix (two commands sent where
  JS sends none). The corrected frontend resolves against acknowledged depth;
  both empty-history and repeated-Undo-to-empty tests now pass. Unsupported SAVE
  stays disabled; the old empty-UNDO-disabled check is retained explicitly.
- Only the incremental standalone test app is rebuilt. No version bump, ZIP or
  plugin package is made. History/audio are temporary; hardware/DAW, full browser
  rendering and long-session/mobile memory-pressure acceptance remain pending.


## Original MULT capture/doubling — 2026-10-18

The previous eight Windows gates plus the unchanged connected capture audit passed
before starting MULT. This increment retains the original MULT button, backslash
shortcut, original (M)/FINISH presentation and CSS. All 16 reference assets remain
byte/SHA identical. There is no new panel, version bump, ZIP or plugin package.

- Windows Release and Ubuntu CTest pass 9/9 (103.85s / 140.55s respectively), plus
  the unchanged connected capture audit: 126 cases, 965,826 frames, maximum error
  7.395e-8 at tolerance 1e-6. Existing NORM, reverse, history, state and kernel gates
  all remain intact. After adding the mixed global-redo regression and a pending
  growth assertion, the two affected gates passed again on both systems.
- Final actual-JS MULT comparison: 90 cases, 832 metadata snapshots, 1,546,108
  committed PCM frames, maximum absolute error 2.015e-8, unchanged tolerance 1e-6.
  This covers zero/1/127/128-frame discard/accept boundaries, mono/stereo,
  8/48/96 kHz, unequal append lengths and repeated stopped/no-input doubling,
  unequal-length Undo/Redo, and original local/global redo interaction.
- Core regressions pass worker OOM rollback with prior redo/PCM retained, active
  capture/edit guards, Stop without late autoplay, Clear/reset cancellation,
  held reverse-source cancellation, exact current-memory exhaustion and growth
  reservation against another track's history restore. Oversized doubles and
  longer Redo clips preserve state on rejection. After space is released they
  succeed. Observed callback C++ new/delete remains zero. Timestamped Multiply
  start/finish and committed rendering agree exactly at 64/128 blocks; asynchronous
  worker completion blocks are deliberately outside that deterministic comparison.
- Ubuntu ASan/UBSan core passes (3.26s); TSan core passes using the documented
  process-local setarch launch. No global ASLR/security settings changed.
- Native WebView2 checks MULT button/shortcut IPC against snapshot-only clips,
  original Multiply state/FINISH/speed guards, existing history/other controls,
  no inline handlers/legacy AudioContext, and muted output. The initial shortcut
  fixture failed with a JS SyntaxError caused by backslash escaping in the test
  harness; using String.fromCharCode(92) fixes the fixture and the complete smoke
  check passes. No microphone capture is started. Review also caught retrigger
  offered during capture although the audio owner guards it; the button now uses
  the same capture/finalization guard and the smoke assertion covers it.
- Only the existing incremental Windows Standalone app is updated. Physical
  TOPPING/ASIO/JACK/CoreAudio, DAW/Apple/mobile, long-session memory pressure,
  full browser-rendered audio and persistent projects/export remain unverified.
  A single worker lane guards concurrent capture/finalization interactions.
  Variable-length history can retain substantially more memory than the earlier
  fixed-length estimate; ARCHITECTURE documents the conservative bound.

## Main arm / record / play, static and dynamic — 2026-10-18

Baseline before this workflow: Windows 9/9 plus the unchanged connected capture
comparison passed (102.05s suite; 126 cases / 965,826 frames). All original UI
assets stay byte/SHA identical. No tolerance is widened and no previous oracle
case is removed. Only the incremental Standalone app/probes are built; no new
ZIP, installer, plugin-format package or version is produced.

- Windows Release and Ubuntu CTest pass 10/10 (116.15s / 177.49s). The unchanged
  capture audit also passes both platforms: 126 cases, 965,826 frames, maximum
  error 7.395e-8 at tolerance 1e-6. NORM, reverse, history, MULT and kernel/state
  gates remain intact.
- The final expanded unchanged-JS sync oracle passes on Windows and Ubuntu:
  112 cases, 224 metadata snapshots, 108 effective-rate checks, 388,772 committed
  PCM frames, maximum error 6.094e-8 at the unchanged 1e-6 audio tolerance.
  The 64 original capture cases and 32 grid/count-in/cancel cases are retained;
  16 extra loud-input cases cover clipping in the folded tail and dynamic trim.
  Three sample rates (8/48/96 kHz), mono/stereo, early/second/forced finish, uneven
  meters/default geometry, fractional BPM and both dynamic/non-dynamic rate rules
  are exercised. Pure rate telemetry is compared at its native float precision.
- Core tests pass exact frame-boundary arming/cancellation, fixed capture/extraction,
  post-commit phase, queued play/cancel, quantized short stop/second stop/panic,
  dynamic bar wait/second tap/50ms window/trim, Stop All, settings latched during a
  take, oversized reservation rejection, failed worker allocation and clear/reset.
  Static and dynamic capture plus committed playback agree exactly at 64/128 blocks.
  The audio-thread C++ new/delete watch stays zero. Ubuntu ASan/UBSan core passes
  (4.15s), and TSan core passes with the documented process-local setarch launch.
- The original-JS comparison first caught fractional-tempo static PCM disagreement
  at frame zero (native 0.210854292 versus JS 0.122038739). The original extractor
  floors its prefix offset, while deadline capture began on the ceiling sample.
  The two-sample POD input prefix repair preserves that sample without a new
  capture buffer. The same failing fractional case now passes; its tolerance and
  input stream are unchanged.
- The broader core gate then caught 2401 versus the intended 2400-sample queued
  deadline from floating-point integer noise. The repair removes only noise within
  four double epsilons before applying ceiling; an explicit fractional 2400.001
  deadline still becomes 2401. Old and new suites pass after the repair.
- Real WebView2 pointer/key tests exposed a genuine controller failure missed by
  the original permissive timer mocks: setTimeout/clearTimeout were invoked with
  the timer-object receiver, causing Illegal invocation, failed hold setup and
  failed release cleanup. Repeated presses were consequently ignored. Arrow
  wrappers now call the Window timer functions correctly. A strict receiver mock
  and actual browser traces guard that failure. A delayed real hold is recovered
  on release at most once; blur/cancel never invents one.
- Native smoke passes original sync radio/BPM/bars/meter fields through APVTS,
  timeline marker reuse and OFF-mode zero playhead, armed/queued CANCEL and STOP
  NOW captions, real mouse down/up/click exactly once, mouse hold, repeated number
  keys, held key/auto-repeat suppression, parameter bounds and missing-input errors.
  The real core rejects snapshot-only capture/history fixtures; no microphone is
  recorded and master output remains zero. Pointer traces require no browser
  exceptions and are restored/removed afterward. Test timings wait for audio-owner
  acknowledgements rather than fixed sleeps; an isolated WebView test profile and
  foreground-equivalent timer flags avoid stale shared debug sessions. One earlier
  debug-page discovery attempt failed; the isolated-profile run succeeds.
- Internal master-grid scheduling is the accepted scope. Loop-source/DAW/MIDI clock,
  audible metronome/count-in, pre-tap snap and latency calibration remain disabled
  or unported. Native capture settings are latched rather than reading mutable
  browser globals during worker finalization. Physical ASIO/TOPPING/JACK/CoreAudio,
  live listening, browser resampling/envelopes and Apple/mobile remain unverified.
  Audio/history are still temporary and are not saved into project/DAW state.


### Mouse arm / record / play follow-up (2026-10-18)

The unchanged reference `UIManager.createLoopElement` makes the header background
and state text a short-action target. That listener was missing from the native
adapter. It is restored with nested-control exclusion; the waveform retains its
own role and main-button bubbling never sends a duplicate action. No original
HTML, CSS or JS asset is edited.

`MouseWorkflowHost.cpp` links the production shared processor/editor rather than
mocking bridge replies. A separate test audio thread supplies stereo tones at
48 kHz; real WebView2 CDP mouse events operate original visible controls. No
hardware device is opened, and no production test hook or endpoint is introduced.
`MouseWorkflow.cjs` checks actual captured PCM length and nonzero output through
the processor with monitoring off. It covers header/button recording and replay,
release after dragging outside, nested control/waveform isolation, static arm
cancellation and automatic exact 96,000-frame capture, queued playback cancellation
across a grid boundary, quantized stop and second-stop cut, dynamic bar finish and
second-tap immediate finish, auto-play disabled, and rapid record/finish/play with
no delayed hold after release. These tests supplement the existing original-JS
PCM comparisons and the fixture-based Standalone smoke; they do not replace them.

The first harness runs found test setup issues (JUCE custom-app entry point,
restricted child-process WebView debugging, invisible radio inputs, and querying
before bridge injection). The harness now provides its own entry point, runs with
normal desktop child-process access, clicks visible radio labels, and waits for
the actual bridge/cards. Grid-phase preconditions make cancellation checks occur
before their deadlines; audio timing assertions still come from native snapshots.

This acceptance is synthetic input through the production processor in a test
host. Physical TOPPING/ASIO and other drivers, listening/round-trip latency and
DAW/Apple/mobile behavior remain separate pending checks. The updated existing
Standalone is the manual-test artifact; no ZIP/version/plugin package is created.

Follow-up results: the complete real-mouse/native-PCM suite passes at both 64
and 128 samples, including rapid record/finish/play and dynamic second finish.
Windows CTest passes 10/10 (125.20s); Ubuntu CTest passes 10/10 (187.28s).
The connected Windows and Ubuntu capture/overdub/substitute audits pass 126 cases / 965,826
compared samples with maximum absolute error 7.394757495227111e-8 (tolerance
unchanged at 1e-6). NativeUiSmoke also passes on the rebuilt Standalone at
44.1 kHz: 20 original cards, no legacy AudioContext/inline handlers, real
pointer/key holds/releases and audio-owner acknowledgements, master gain zero.
Logs are `build/mouse-final.log`, `build/mouse-regression-windows.log`,
`build/mouse-regression-ubuntu.log`, `build/mouse-audio-audit.log`, and
`build/mouse-native-smoke.log`; build logs are local, not committed artifacts.


### Held main-action audio-loss repair (2026-10-18)

The next real native-PCM regression reproduced data loss: with sync OFF, holding
STOP for 850 ms first stopped the playing loop, then the 600 ms hold timer saw
`stopped` and sent CLEAR. The test expected a stopped clip but received `empty`,
length zero, with the entire capture budget returned. This is recorded in
`build/hold-before-fix.log`. Earlier presentation fixtures always reported
`playing`, so they could not detect that transition.

The controller now captures the state before the short action and passes it
through both the normal hold timer and delayed-release fallback. The queued hold
also checks the latest acknowledged state. A playing/stopping gesture can cut
playback but cannot become CLEAR after stopping. FINISH, arm/cancel, queued cancel
and a short fixed recording that auto-completes while still held cannot act on
their later playing/stopped state. Existing stopped-track clear intent and active
playback cut intent remain available under compatible starting/current states;
DEL remains the explicit delete action. No DSP, audio-thread or original asset
changes are involved.

The original `LoopManager.handleAction(..., 'long')` body runs unchanged in the
frontend gate for stopped/playing/stopping clear/cut intent. Its separate original
input timer also reads mutable current state; this repair intentionally prevents
that hazard instead of reproducing destructive behavior. The state-origin guard
has 121 start/current-state checks, plus timer and delayed-release origin tests.
Real native-PCM mouse/keyboard acceptance retains every prior case and adds held
STOP (mouse and actual CDP keyboard input), FINISH with auto-play, queued CANCEL,
and RECORD with a 100 ms STATIC take. Both 64- and 128-sample runs pass and check
that PCM length/playback survives, rather than merely checking command counts.

Only the existing Standalone and optional test host are incrementally rebuilt.
No new packages, version bump or additional migrated feature are included.
Physical TOPPING/ASIO, listening and DAW/Apple/mobile validation remain pending.

Held-gesture results: Windows CTest 10/10 (122.95s), Ubuntu CTest 10/10
(187.94s), and the final extended frontend/original-long-action oracle passes
on both platforms. Windows connected PCM audit passes 126 cases / 965,826
compared samples, maximum error 7.394757495227111e-8 with tolerance 1e-6.
The 64/128 real native-PCM suite passes (`build/hold-mouse.log`). The rebuilt
Standalone smoke passes at 44.1 kHz (`build/hold-native-smoke-final.log`), with
no browser exceptions/legacy audio context and master muted. The first smoke
attempt recorded lost pointer capture before the hold threshold while another
GUI test was active; its timer correctly cancelled and the two-command assertion
failed. The isolated rerun passes without loosening that assertion. Run GUI
input suites serially; independent headless regressions may run concurrently.

The final Ubuntu connected PCM audit also passes all 126 cases / 965,826 compared
samples with the same maximum error 7.394757495227111e-8 and unchanged 1e-6
tolerance (`build/hold-ubuntu.log`). All required gates are green for this
message-thread hold-safety repair; physical-device acceptance is still separate.


### User correction: hold-to-delete is intentional (2026-10-18)

The user clarified that holding STOP after it stops a playing loop is the intended
way to discard a bad take. This supersedes the previous interpretation that this
transition was a bug. Restore playback-origin/current-stopped CLEAR while keeping
initial-state guards on FINISH/CANCEL/auto-completed RECORD. The UI retains its
original layout; a native addendum appears at the start of each original manual
panel (English, Spanish, Portuguese). Reference source bytes remain untouched.

The manual explicitly covers 600 ms, mouse main button and assigned keyboard key,
short tap versus hold, Undo/Redo history loss, sync waiting/cut versus already
stopped/delete, DEL, and header/state text being short-action only. The original
JS long-action oracle and 121 state pairs are updated to accept intentional
playback-to-stopped deletion. Real 64/128 tests now require actual PCM deletion
from both held mouse STOP and held keyboard STOP while retaining FINISH/CANCEL/
RECORD preservation cases. Native smoke verifies all three manual panels contain
the addendum. This is a gesture/manual correction; no DSP code or new feature is
introduced, and no package/version/plugin build is needed.


### Four-second STOP deletion (2026-10-19; supersedes 600ms deletion)

The user explicitly requests audio deletion only after holding STOP for 4 seconds.
Keep the 600ms playback cut as a separate action that cannot clear PCM. A second
4000ms timer deletes only for STOP/STOP NOW starting states; the current-state
resolver also rejects unrelated later states. Release/blur/pointer cancellation/
STOP ALL clear both timers. Monotonic elapsed time guards early callbacks and
recovers deferred deletion at most once on a >=4000ms release. PLAY/FINISH/CANCEL/
RECORD remain excluded even if their resulting state contains a completed clip.

The original long-action handler remains the clear/cut intent oracle, with the
user-requested four-second timing as an explicit override. Unit tests exercise
599/600/850/3999/4000/4001ms releases, early 3999ms timer rescheduling, exactly-once
release after timer firing, and cancellation/blur after a delayed deadline.
A mock initially retained executed timer entries (unlike browser timers); the
mock now removes one-shot entries on invocation, preserving cleanup assertions.
Native PCM tests check a 3.8s STOP release retains the clip after the deadline,
4.2s mouse/key STOP holds delete, and >4s FINISH/CANCEL/RECORD holds retain PCM.
Existing EN/ES/PT manual addenda document 4 s deletion versus 600 ms cut, history
loss, all sync modes and cancellation. Original assets remain byte-identical.

Four-second gate results: Windows CTest 10/10 (125.66s), Ubuntu CTest 10/10
(190.66s). The final frontend suite also passes independently on both platforms
including STOP eligibility captured before a mode change: an OVERDUB/REPLACE
press cannot become a deletion by changing global mode during the hold. This
is a frontend gesture/manual correction with unchanged C++/DSP; the previously
passed connected PCM audit remains the DSP baseline. No new audio feature is
advanced and no ZIP/version/plugin package is created. Final GUI acceptance is
recorded separately below after the rebuilt controller is exercised serially.

Final GUI acceptance passes on the rebuilt controller: real mouse/keyboard native
PCM tests at both 64 and 128 samples (`build/four-second-final-mouse.log`), then
Standalone WebView2 pointer/key/manual smoke at 44.1 kHz
(`build/four-second-native-smoke.log`). The latter verifies the 4 s / 600 ms
instructions in all three original manual panels, 20 original cards, no legacy
AudioContext/inline handlers, and master muted. Tests run serially and all owned
GUI hosts close. The existing Standalone is updated; no package is generated.


### Queued hold cancellation audit (2026-10-19)

Both a frontend queue regression and the previous native binary reproduce a
failure: after a four-second STOP queues deletion, blur cleared only active timers;
the pending resolver later deleted PCM anyway. The real test pauses only frontend
queue dispatch, captures real synthetic-input PCM, holds STOP for 4.2s, blurs and
resumes dispatch. The old binary reports empty/length zero instead of the expected
stopped clip (`build/queued-hold-before-build.log`). No snapshot/PCM fixtures are
used. This is distinct from the intentional four-second deletion feature.

Global and per-track validity epochs now survive normal release and are checked
by queued cut/delete resolvers at dispatch. Blur/reset/STOP ALL invalidates all
old holds, DEL invalidates that track, and audio-generation changes cancel active
and queued holds before new configuration state is used. Ordinary uncancelled
four-second release still authorizes a queued deletion. Already-dispatched audio
commands are not retractable; cancellation is guaranteed before dispatch.

Tests retain old four-second cases, and add paused queue + blur PCM preservation,
clear/re-record while the original key remains held beyond its deadline, queued
normal release/cancel/pointer/track/other-track cases and per-track timer cleanup.
Native smoke injects a generation notification and checks the real controller
clears active presses and invalidates a saved queued-action token; it does not
reset the physical audio driver or claim driver reset acceptance. Only frontend routing and
manual text change. Original assets, DSP tolerances and C++ processing are intact.

The first repaired-native run preserved PCM but exposed an incorrect test state
expectation: pausing dispatch for 4.2s also expires the short STOP under the
existing 3s queue limit, so playback should continue. That same paused-input case
is retained with an unchanged input stream and exact PCM/request-count checks;
no command should dispatch. A second case acknowledges STOP before pausing and
then requires a stopped, replayable clip with no later deletion. Neither case
removes the original failure or changes an audio tolerance.

The next full native run passed the new cancellation cases but failed the retained
rapid record/finish/play gate: the third request was resolved while the worker
reported finalizing and silently dropped. The stopped 5,952-frame clip remained,
but PLAY never reached the core. This timing race is now repaired in production:
an accepted main gesture waits in the bounded FIFO until finalization finishes,
retaining its original 3s expiry. Polling retries without spinning; priority panic
still removes waiting gestures. The failing rapid native case stays unchanged,
with deterministic wait/order/expiry/panic unit coverage added.

Final results: Windows CTest 10/10 (120.98s), Ubuntu CTest 10/10 (193.07s),
and final queue wait/order/expiry/panic + cancellation frontend checks pass on
both platforms. Connected native capture audit passes 126 cases / 965,826
compared samples, max error 7.394757495227111e-8 with unchanged 1e-6 tolerance
(`build/queued-hold-audio.log`).

The retained full real mouse/keyboard/PCM suite passes at 64 and 128 samples
(`build/queued-hold-final-mouse-v3.log`), including both queued blur variants,
clear/re-record protection, 4s deletion and rapid finalization taps. Standalone
smoke passes at 44.1 kHz (`build/queued-hold-native-smoke.log`), including the
injected generation notification, 20 original cards, all manual panels and
strict pointer/key checks. Earlier native failures are retained in the v1/v2
and before-build logs. No audio tolerance/case is removed to obtain these passes.
Only the existing Standalone and optional test host are rebuilt; no ZIP, version
bump or plugin package is produced. Physical input/driver/DAW acceptance remains
separate from these synthetic-input and bridge checks.


### Deferred DEL cancellation and priority (2026-10-19)

Audit of the finalization wait found DEL queued behind that wait. The unchanged
original DEL calls loop.clear directly. The native regression is reproduced both
in the frontend queue and in the prior binary: a controlled frontend wait holds
a real playing 18,176-frame clip; clicking DEL leaves it playing beyond the 1.2s
acceptance window (`build/deferred-clear-before-fix.log`). The test injects only
a queue wait and an unsent record intent, not fake snapshots/PCM or a DSP worker.

Local intents now carry track ownership. Explicit DEL removes that track's unsent
main/control/history/hold intents and dispatches before ordinary waits, while
retaining global STOP ALL priority and other tracks' relative order. In-flight
commands finish before CLEAR. Queue capacity (128), expiration (3s), validity
predicates and C++/DSP are unchanged. Unscoped/non-integer clear requests are rejected without
flushing global actions. The EN/ES/PT manual addendum documents DEL cancellation.

Unit tests cover a same-track wait, a different-track wait, retained local/global
actions, global panic precedence, invalid scope and in-flight acknowledgement.
Real 64/128 PCM tests retain every previous gesture case and add prompt DEL while
a scoped wait is present, exact request count, and no loop resurrection after the
old queue deadline. Existing DSP/state gates and original byte checks are retained.

Acceptance passes: Windows CTest 10/10 (123.89s), Ubuntu CTest 10/10 (194.07s),
connected PCM audit 126 cases / 965,826 samples, maximum absolute error
7.394757495227111e-8 with unchanged 1e-6 tolerance. Full native mouse/keyboard
PCM cases pass at both 64 and 128 samples (`build/deferred-clear-final-mouse.log`),
including prompt scoped DEL, no deferred re-record after 3s, and all retained
four-second deletion/cancellation/rapid-play cases. Standalone smoke passes at
44.1 kHz (`build/deferred-clear-native-smoke.log`): 20 original cards, manual
panels, pointer/key/generation checks, no legacy AudioContext/inline handlers,
master gain zero. Earlier failure remains in before-fix logs. The existing app
and optional host are rebuilt; no ZIP, version bump or plugin package is made.
These are synthetic-input/bridge checks, not physical driver/DAW acceptance.


### Held STOP deletion priority and stale holds (2026-10-19)

The four-second STOP deletion still used an ordinary FIFO command after DEL
received scoped priority. A production-binding regression and the previous
Windows binary both reproduce the difference: another track's finalization wait
keeps a real 19,200-frame loop stopped beyond the deletion acceptance window.
The native failure is retained in `build/held-clear-before-fix.log`. Only frontend
dispatch is fault-injected; snapshots, captured PCM and audio callbacks are real.

An eligible four-second deletion now supersedes unsent actions for its track and
bypasses ordinary waits using the existing scoped-clear queue path. At dispatch,
it rechecks cancellation and current state, then invalidates all older holds on
that track before sending CLEAR. This prevents a second held key from later
operating on a replacement take. Other-track actions, panic priority, in-flight
acknowledgements, queue bounds/expiry, the 600ms cut and 4000ms deletion threshold
are retained. An invalid/cancelled gesture cannot flush pending actions.

Tests execute the actual production erase binding, rather than a copied handler:
other-track waits, discarded local records, overlapping holds, released tokens,
blur before dispatch and ineligible origins/current states are covered. The real
native mouse/keyboard PCM workflow adds held STOP behind a different track's
wait, then checks that an unsent recording cannot resurrect the deleted loop.
All prior boundary, blur, DEL, rapid record/finish/play and static/dynamic cases
remain. No DSP, original UI asset, version or package changes are needed.

Acceptance: Windows CTest 10/10 (124.27s), Ubuntu CTest 10/10 (184.27s),
plus the expanded production-binding frontend test on Ubuntu. The connected PCM
audit passes 126 cases / 965,826 samples, maximum absolute error
7.394757495227111e-8 at unchanged 1e-6 tolerance. All native mouse/keyboard PCM
cases pass at 64 and 128 samples (`build/held-clear-mouse.log`); Standalone UI
smoke passes at 44.1 kHz (`build/held-clear-native-smoke.log`), with 20 original
cards, no legacy AudioContext/inline handlers and master gain zero. The original
asset byte gate passes. Only the existing Standalone and optional test host were
rebuilt; no ZIP or plugin package was produced. Physical driver, listening and
DAW validation remain outside this synthetic-input/bridge acceptance.


### Original loop-name fields (2026-10-19)

The original name inputs were rendered but disabled. They now edit C++
message-thread session labels for all 20 slots. Original markup/styles remain
byte-identical; Enter blurs the input, typing is isolated from transport keys,
and refresh does not overwrite an active edit. The view adapter now passes
native names through instead of substituting an empty string. A new rendering
regression caught that override before acceptance.

Labels survive editor/browser reload and ordinary stop/arm cancellation. DEL,
held STOP deletion and CLEAR ALL reset names when the audio owner acknowledges
the clear. A fixed-size per-track epoch distinguishes explicit deletion from
arm cancellation, includes deletion of an already-empty slot, and survives
skipped snapshots. Only these POD counters cross the audio snapshot queue;
string allocation/destruction stays on the message thread. Sample-rate resets
clear labels alongside audio. No audio callback locks or new/free are added.

Rename IPC validates integer track IDs and string types, accepts at most 4096
UTF-8 bytes, and rejects larger requests without modifying the old name. This
is a bounded native IPC limit; original JS has no explicit name-length limit.
Names remain session metadata: like PCM, they are not yet in project save/load
or the existing parameter-only host-state blob. No persistence parity is claimed.

New checks execute the unchanged original input handler and Loop.clear for
name assignment/deletion, preserve escaped name markup, and exercise empty,
Unicode, quote/markup and long names. Native UI tests cover actual typing,
number-key isolation, Enter, all 20 labels, rejected IDs/types, exact UTF-8 size
boundaries, browser reload and text-safe rendering. The synthetic native host
renames actual playing PCM and checks transport request count, clip length,
continued output and acknowledged name deletion at 64/128 samples. Existing
main/sync/hold regressions remain. The new arm/cancel unit setup initially used
a fixed take larger than its small pool; the engine correctly rejected it. The
fixture now uses a short supported take, retaining the capacity guard unchanged.

Review also reproduced a delayed-clear/name ordering failure in the first native
build: typing immediately after queued DEL saved a new label before the audio
owner cleared it, then the acknowledgement erased it (`build/names-order-before-fix.log`).
Name edits now share the ordered command queue with audio and coalesce by track.
A metadata item at the head may run without an active audio owner; it still waits
for preceding clears and their acknowledgement. Explicit clear cancels unsent
older names as it does other track work. Unit checks and the retained native
paused-dispatch/DEL/type/ack regression cover this repair.

Acceptance: Windows CTest 10/10 (126.19s), Ubuntu CTest 10/10 (188.27s),
including the corrected clear-epoch cancellation test and unchanged asset,
state, DSP, NORM, reverse, history, MULT and sync gates. After the metadata queue
repair, the expanded frontend suite passes again on both Windows and Ubuntu.
The final native UI gate passes (`build/names-ordered-native-smoke.log`), including
the previously failing DEL/name race, Unicode/size validation and reload. Real
synthetic-input mouse/keyboard workflows pass at both 64 and 128 samples
(`build/names-ordered-mouse.log`), preserving every prior hold and sync case.
The rebuilt native audio probe passes 126 committed-PCM cases / 965,826 samples,
maximum absolute error 7.394757495227111e-8 at unchanged 1e-6 tolerance
(`build/names-final-audio.log`). Captured original card layout was visually checked.
Only the existing Standalone and optional test host were rebuilt: no ZIP, version
bump or plugin package. Physical drivers, listening, DAW and Apple platform
acceptance remain separate; these tests do not establish complete feature parity.


### Naming during finalization and STOP ALL (2026-10-19)

The expanded naming audit reproduces another failure in the accepted binary:
a controlled wait on track 19 leaves an edit to track 18 unapplied beyond a
1.2s native acceptance window. Actual C++ snapshots still report the previous
name (`build/names-priority-before-fix.log`); only frontend dispatch is fault-
injected. The focused queue regression fails identically. Unsent name edits
were also discarded by STOP ALL, unlike the unchanged original method, which
is now executed explicitly in the name oracle.

Metadata edits now bypass ordinary audio/finalization waits, retaining the
relative order of audio work. They stay behind pending panic and deletion
commands, and continue to coalesce by track. STOP ALL cancels pending performance
commands/holds while retaining pending names. If a generic queue is filled
entirely with metadata, panic reserves a slot within the unchanged bound and
reports any cancelled edit; the actual UI coalesces at most 20 loop names into
the default 128 slots. In-flight acknowledgements and normal expiry are retained.

A second ordering regression caught during this change put names ahead of
DEL ALL. Global deletion now carries an explicit acknowledgement barrier, so a
name typed immediately afterwards is saved only after the clear completes.
DEL ALL still resets older queued work; per-track DEL still supersedes older
names on that track. Neither barrier is an audio-finalization wait, and the
existing four-second deletion thresholds and cancellation tokens are unchanged.

Unit regressions cover unrelated waits, retained audio order, in-flight work,
STOP ALL retention, metadata-only capacity pressure, and global-clear ordering.
Native tests retain delayed DEL/name coverage and add live metadata behind a
controlled different-track wait, STOP ALL acknowledgement/name retention and
the actual DEL ALL button followed immediately by typing. These checks inspect
real native snapshots/acknowledgements, not fixture names or fake PCM. The
original assets, layout, DSP and session-only naming/persistence scope are unchanged.

Final acceptance passes on Windows (10/10 CTest, 128.53s) and Ubuntu (10/10,
186.83s), with all original reference/state/DSP/sync and byte-identity gates.
The native UI gate passes with both destructive acknowledgement barriers and
the new wait/STOP ALL cases (`build/names-priority-final-native-smoke.log`).
All retained real mouse/keyboard PCM workflows pass at 64 and 128 samples
(`build/names-priority-final-mouse.log`). The connected native audio comparison
passes 126 cases / 965,826 samples with maximum error 7.394757495227111e-8 at
unchanged tolerance 1e-6 (`build/names-priority-audio.log`). Only the existing
Standalone and optional host were rebuilt; no package or version bump. This is
session naming/bridge acceptance with synthetic input, not driver, DAW, Apple
platform or complete application acceptance.


### Native continuous metronome (2026-10-19)

The existing Metro[N]ome button, VOLUME slider and N shortcut now control native
C++ DSP through APVTS. The original worklet's 1000 Hz beat / 2000 Hz bar accents,
11.3ms exponential decay, 0.001 envelope cutoff, phase wrap and grid-index start/
stop behavior are preserved. OFF/STATIC/DYNAMIC use the existing internal Master
BPM grid; enabling between beats waits for a boundary, and disabling resets the
envelope immediately. STOP ALL leaves the separately enabled metronome running,
as in the reference. Rapid toggles read the native boolean on the message thread
instead of toggling an old frontend snapshot. Text-entry and repeat guards apply
to N. Audio-owned 100ms flash telemetry drives the original button colours.

The unchanged MetronomeScheduler.init method is executed with node IO stubs to
check its direct-to-destination routing. The native click is likewise added only
to monitor output, after MASTER gain, and never fed to LoopBank capture. MASTER
metering excludes it; the actual OUTPUT meter includes it. This distinction was
added during routing review before acceptance. Output still uses the native
engine's existing [-1,1] protection. Original worklet raw output at VOLUME >1 is
compared separately from that protected Engine output. Existing EN/ES/PT manual
addenda explain the independent volume and recording exclusion.

The new js_native_metronome gate runs the complete unchanged injected worklet
with Float32 parameter/output arrays, without substituting a reimplementation.
It covers 8/44.1/48/96 kHz, 64/128 partitions, uneven meters, positive/negative/
future origins, long absolute frame positions, fractional/999 BPM, volume 0/2,
start/stop/restart, decay/phase wrap and per-sample BPM/origin arrays. Four
additional connected Engine cases compare actual stereo output with that same
worklet fed the Engine's master-clock telemetry, including tempo/meter changes,
all internal sync modes and final output protection. The clock itself retains
its separate unchanged-JS sync gate; this additional comparison verifies its
connection to metronome DSP, not browser UI scheduling or host/driver timing.

The realtime gate checks exact stereo and 64/128 output invariance, sample-grid
boundaries, accent frequency, mute/stop behavior, finite controls and callback
new/delete guards. A real Engine records 4096 silent input frames while its
metronome is audible with MASTER muted, then replays the committed take with the
metronome off and MASTER on: output must remain exactly zero. MASTER telemetry
must also remain zero while actual OUTPUT telemetry shows the click.

Native standalone UI checks use zero click volume to avoid physical sound, while
checking button/N, burst toggles, text/repeat guards, invalid parameters, flash
reset and slider round trips. The hardware-free production processor/editor host
checks audible PCM with MASTER muted, independent volume, MASTER display routing,
STOP ALL continuity and N stop at both 64/128, retaining prior looper/sync/naming
cases. All 16 original assets and copied markup methods stay unchanged.

This feature is the continuous internal metronome. The separate triangle-wave
SOUND count-in remains disabled/unported, as do DAW/MIDI/loop-source clock sync.
Project/PCM persistence and hardware/Apple/DAW acceptance are not implied.

Acceptance passes: Windows CTest 11/11 (140.10s), Ubuntu CTest 11/11 (201.96s).
Metronome comparison: 29 cases, including 4 connected Engine cases, 3,011,808
samples, maximum absolute error 4.995727609369283e-9 at unchanged tolerance 1e-6.
All prior committed recording/overdub comparisons pass (126 cases, 965,826
samples, maximum error 7.394757495227111e-8, same 1e-6 tolerance). Native UI smoke
passes at 44.1 kHz with no legacy AudioContext/inline handlers; the production
processor/editor synthetic mouse workflow passes at both 64 and 128 samples,
including metronome output/metering/N/STOP ALL and all retained main-loop,
static/dynamic, hold deletion and naming cases. Logs: `build/metronome-routing-windows.log`,
`build/metronome-routing-ubuntu.log`, `build/metronome-parity-connected.log`,
`build/metronome-routing-audio.log`, `build/metronome-native-smoke.log`, and
`build/metronome-mouse.log`. Only existing app/test targets were rebuilt. No
version bump, ZIP, installer or plugin package was generated.


## Static audible count-in — 2026-10-19

The existing SOUND checkbox now controls native static-recording cues. VISUAL
and SOUND independently request the same minimum lead; OFF/DYNAMIC still begin
immediately. The complete unchanged `LoopManager.scheduleCountIn` determines
cue time, 50ms scheduling guard, last-bar restriction, 440/880 Hz pitches, gain
0.5, 100ms audio duration and 150ms visual events. Cues latch at accepted arm time.
Native cancellation deliberately improves on abandoned browser timers: same-track
CANCEL/STOP/DEL, STOP ALL, DEL ALL and prepare/reset silence pending cues. No new
panel or original UI asset changes. The supplied ASCII logo remains intact.

The initial Nyquist-only triangle failed an actual OfflineAudioContext sound
comparison (8.39% relative RMS at 8 kHz). The repaired oscillator preserves
pitch-band blending and full-wave normalization, using an independently written
Fourier/table implementation; no browser DSP dependency was added to the product.
The model was checked against Chromium's published periodic-wave behavior:
[PeriodicWaveImpl](https://chromium.googlesource.com/chromium/src/third_party/+/c2ac657807607652f7a91204ded7f1c506bb98f5/blink/renderer/modules/webaudio/periodic_wave.cc).
Only two fixed-pitch wavetables are needed. Tables allocate/populate in prepare;
scheduling, mixing, metering, visual telemetry and cancellation allocate nothing.
A Windows stack overflow exposed by the core fixtures was repaired by keeping
these persistent tables off the stack; callback allocation guards remain zero.

`js_native_countin` retains 46 cases / 2,139,650 compared samples at 8, 44.1, 48,
96 and 384 kHz. It executes the complete unchanged scheduler, verifies original
destination routing/gain/type, compares native PCM to a separate harmonic model,
and exercises the actual Engine at 64/128. The 4e-5 absolute interpolation limit
was set before testing and retained. Final maximum error: 2.03997e-5. Connected
cases include fractional BPM, SOUND-only extra-bar lead, overlapping voices and
VISUAL-only silence; original arm/grid/cancel comparisons independently validate
the recording target. The existing `js_native_sync` coverage is retained and
expanded to every independent SOUND/VISUAL flag combination.

`tests/CountInWebAudio.cjs` renders the unchanged scheduler in an actual WebView2
OfflineAudioContext (test-only). Twelve sample-rate/tempo combinations cover
8/44.1/48/96 kHz, 120/137.3/999 BPM, fractional starts, both pitches and overlapping
voices. After repair the worst relative RMS error is 1.82165e-5 (0.001822%), and
maximum absolute error is 1.43946e-5. The initial coarse sound gate was tightened
to relative RMS <=1e-4 (0.01%) and absolute error <=4e-5; no failed case was removed.
This establishes numeric browser-rendered cue agreement within those tolerances,
not bit identity across browser engines or physical driver/listening acceptance.

Native core tests additionally verify exact stereo and 64/128 equality, a cue
that overlaps actual recording but is absent from committed PCM, MASTER/meter
bypass, all cancellation barriers, rejected/unrelated commands, latched controls,
OFF/DYNAMIC immediate capture, reset, finite controls and zero callback new/delete.
The native WebView gate toggles the original SOUND checkbox and rejects invalid
boolean values without opening capture. The hardware-free mouse gate hears real
SOUND PCM with MASTER at zero and checks actual CANCEL/STOP ALL for delayed cues.

Reproduce after building the existing app/probes/test host:

```powershell
ctest --test-dir build/windows -C Release --output-on-failure
node tests/AudioParityAudit.cjs build/windows/Release/aion_reference_probe.exe
./tools/TestCountInUi.ps1
./tools/TestMouseWorkflow.ps1
```

No version bump, ZIP, installer or plugin package is generated for this feature.

Final acceptance: Windows Release CTest 12/12 (143.95s), Ubuntu CTest 12/12
(212.69s), Ubuntu ASan/UBSan core 1/1 (5.81s), native WebView2 UI and the stricter
12-case actual-browser waveform gate, and complete hardware-free mouse workflows
at both 64/128 all pass. The connected capture/overdub/substitute audit retains
126 cases / 965,826 samples, maximum error 7.39476e-8 at the unchanged 1e-6 limit.
Original UI byte identity also passes. At very high BPM native flashes retain
the latest cue for 150ms rather than allowing a stale browser reset timer to
clear a newer flash; the unchanged scheduler's cue requests are preserved.

Logs: `build/countin-windows.log`, `build/countin-ubuntu-final.log`,
`build/countin-asan.log`, `build/countin-ui-final.log`, `build/countin-mouse.log`
and `build/countin-audio-audit.log`. Only the existing Windows standalone and
necessary test targets were rebuilt. Physical drivers, DAW/plugin lifecycle,
Apple/iOS, external MIDI sync and remaining application parity are not accepted
by these gates.


## Original Start slider — 2026-10-19

The original slider and millisecond display are restored, including input-only
metadata changes and the release-triggered +50ms source restart. OFF playback
uses the positive buffer offset; synced playback subtracts the chosen shift
from the master-relative source position. Offset 1 wraps to the loop beginning.
Start values and source-position math retain JS Number precision; existing audio
interpolation/control-rate storage keeps its float behavior. Reverse and phase
choices latch when the future source is scheduled. Stop/panic, DEL/reset and
source-ownership changes cancel old restarts; unrelated controls do not. Stopped
or empty releases are harmless. No PCM is edited and no history entry is added.
Q/nudge remain disabled and are separate work. Existing native declicking is
reused; bit identity of browser resampling and transition envelopes is not claimed.

A reproduced rapid-release regression lost the first accepted source when a
second release arrived within 50ms (`build/start-rapid-before.log`). It was
repaired with a fixed audio-owned FIFO (127 reserved future sources), cancellation
epochs and bounded work. Overflow rejects the newest request and preserves all
older accepted sources. Epoch invalidation prevents late restarts after deletion,
panic, capture/finalization or reprepare. No timer, allocation, lock or full-buffer
processing runs in the callback. The pending-event storage is fixed-size.

`js_native_start_phase` executes the unchanged UI setter, Loop.restart and full
AudioGraph build/retrigger methods with browser IO stubs. Conditioned source PCM
comes from unchanged reference fade/DC processing. Native committed playback is
compared using the same float linear interpolation model as the retained reverse
gate. All 24 mono/stereo, 8/48/96kHz and 1/17/512/4097-frame cases cover OFF/static/
dynamic clocks, positive/zero/full-cycle/tiny offsets, reversed sources, fractional
varispeed, dragging before release, overlapping future releases and the original
solo-muted restart exception (observed again after unmuting). Final scope:
1,198,724 compared samples, maximum absolute error 6.41354e-8, unchanged 1e-6 limit.
Native core tests cover the 50ms boundary, latched values despite later edits,
FIFO capacity and panic cancellation, invalid parameters, a full audio pool,
actual Engine stereo/64/128 equality and zero callback new/delete.

The native WebView gate exercises the original control on empty loops. The
hardware-free production processor/editor test drags it with real pointer events
while captured audio plays, verifies state/length/display and continued PCM,
and restores phase zero before the existing main-workflow regressions. Original
16 reference assets remain byte-identical. The user's local ASCII-logo whitespace
edits are retained separately; no package/version changes are needed.

The full post-FIFO regression suites pass Windows 13/13 (164.78s) and Ubuntu
13/13 (263.61s). The final solo-specific condition then passes all four affected
core/phase/frontend/original-asset gates again on Windows (44.55s) and Ubuntu
(58.69s), plus ASan/UBSan core and phase PCM 2/2 (92.68s). The connected capture
PCM audit retains 126 cases / 965,826 samples, max error 7.39476e-8 at 1e-6.
The rebuilt standalone also passes the original count-in WebAudio gate and native
WebView checks without creating a product AudioContext.

Final native acceptance also passes both 64/128 mouse workflows, including actual
Start dragging, unchanged clip length, phase display/PCM and the retained 4s STOP
deletion/cancellation cases. Logs: `build/start-final-windows.log`,
`build/start-final-ubuntu.log`, `build/start-acceptance-windows.log`,
`build/start-acceptance-ubuntu.log`, `build/start-acceptance-asan.log`,
`build/start-acceptance-ui.log`, `build/start-acceptance-mouse.log` and
`build/start-acceptance-audio.log`. No new package, version or platform readiness
claim. Physical drivers, DAW/plugin lifecycle, browser transition envelopes,
Apple/iOS and complete application parity remain separate acceptance work.


## ASCII marquee alignment — 2026-10-19

The header's inherited `text-align:center` centered each unequal-length ASCII
row independently. Fitting the overall bounding box did not detect those shifted
columns. The native adapter now left-aligns text inside the existing inline-block
rainbow span, retaining outer centering, font, colors, line spacing and the user's
hand-edited artwork. No audio code or original asset changes are needed.

The native WebView regression measures the x coordinate of the same character
column across actual DOM text rows at 800px and 1200px, asserts <1px spread, and
retains the viewport-fit checks. A visual preview confirms intact letter shapes.

Rebuilt Windows Standalone acceptance passed: frontend checks, 12 original
WebAudio count-in PCM comparisons and the native WebView regression, including
the new column-alignment checks at both widths. Visual inspection of the rebuilt
app screenshot confirms intact artwork. Logs: `build/logo-align-build.log` and
`build/logo-align-ui.log`; screenshot: `build/native-looper-top.png`. The existing
Standalone executable was updated; no new package was created.

## Original Start Q button — 2026-10-19

The existing Q control now dispatches one native audio-owner command. It uses
recorded duration and current BPM, rounds to the original sixteenth grid, and
reuses the accepted 50 ms future-source FIFO. It preserves short-clip no-ops,
endpoint 1, stopped/empty guards, reverse/sync offset latching and cancellation.
FIFO exhaustion rejects the edit without changing metadata or prior events.
The original artwork, loop card markup and 16 protected reference assets remain
unchanged. Existing EN/ES/PT manual panels explain Q; nudge remains unported.

The expanded Start oracle executes the unchanged `UIManager.quantizeLoopStart`
plus setter/restart/AudioGraph and conditioned PCM. It retains all old slider,
rapid-source and solo checks and adds Q no-op, rapid Q, endpoint, below-half and
exact-half cases across 30 mono/stereo, 8/48/96 kHz and 1/17/512/4097/20003-frame
fixtures, OFF/static/dynamic clocks, reverse and fractional rates. Final Windows
comparison: 1,578,100 samples, maximum absolute error 6.02428e-8 at the unchanged
1e-6 limit; phase metadata tolerance is 1e-14. This is committed PCM with the
retained linear interpolation model, not full browser transition/resampler parity.

A boundary fixture reproduces the initial `floor(x+0.5)` mismatch against actual
JS Math.round (`build/q-rounding-before.log`); subtracting the integer part before
checking the fractional half fixes it. The failed real-pointer gate also caught
the adapter replacing the original Q tooltip (`build/q-mouse.log`); its original
title is now retained. No failing case or tolerance was removed.

Windows and Ubuntu full core suites pass 13/13 (186.33s / 300.25s). After the final
rounding correction, affected core/Start gates pass again 2/2 on both systems
(59.64s / 82.66s). Core checks retain zero observed callback new/delete, exact
Engine stereo and 64/128 equality with and without Q, empty/short/stopped cases,
DEL/panic, invalid track and full-pool/FIFO rejection. Connected capture audio
audits also pass on both systems: 126 cases, 965,826 samples, max error 7.39476e-8
at 1e-6. Logs: `build/q-windows-tests.log`, `build/q-ubuntu-tests.log`,
`build/q-boundary-windows.log`, `build/q-boundary-ubuntu.log` and
`build/q-audio-windows.log` / `build/q-audio-ubuntu.log`.

Final ASan/UBSan core and expanded Start/Q PCM gates pass 2/2 (129.62s),
including the below-half rounding fix (`build/q-asan-final.log`). The final
frontend markup/syntax/command suite also passes (`build/q-frontend-final.log`).

The final corrected Standalone and production processor/editor test host rebuild
successfully. Real native mouse/PCM workflows pass at both 64 and 128 samples,
including original Q, Start dragging, arm/record/play, static/dynamic sync and
retained 4s STOP deletion/cancellation (`build/q-acceptance-mouse.log`). The
Standalone WebView2 smoke passes the new empty-loop Q action, command acknowledgement,
20 cards, logo alignment at both widths, original count-in WebAudio comparisons
and no product AudioContext (`build/q-acceptance-ui.log`). No new package/version.
Hardware ASIO/listening, DAW/plugin lifecycle, other browser engines, Apple/iOS,
full browser playback envelopes and complete application parity remain separate
acceptance work. This completes the documented Q scope only.

## Original Start nudges — 2026-10-19

The original < and > buttons now dispatch one native `nudgeStart` command with
exact direction -1/+1. The audio owner computes a 1/64-note step using current
BPM and recorded duration (independent of playback speed), then uses the accepted
50 ms source FIFO. Normal single-wrap arithmetic, endpoint 1, reverse/sync latches,
stopped/empty guards and failed-edit rollback are retained. No audio copies,
callback allocations or locks are added. Original markup/assets/logo are retained;
existing EN/ES/PT manual panels document the controls.

The unchanged JS handler only wraps once. On clips shorter than one nudge this can
leave negative or >1 metadata/source offsets. Native nudges deliberately repair
those cases into [0,1]. The PCM oracle distinguishes the repair: it reads the
unchanged handler's raw state, then auditions the intended wrapped phase through
the unchanged setter/restart/AudioGraph pipeline. This is normal-behavior parity
plus a documented tiny-clip repair, not a claim of identical invalid-offset browser
playback. Existing slider/Q and rapid/solo tests remain covered.

The expanded comparison passes 30 mono/stereo, 8/48/96 kHz, 1/17/512/4097/20003-frame
fixtures in OFF/static/dynamic modes with reverse/fractional rates: 540 nudges,
288 tiny-wrap repairs and 1,822,480 compared samples; maximum absolute error
6.21776e-8 at the unchanged 1e-6 limit (metadata 1e-14). Core checks add invalid
directions/tracks, full FIFO rollback, endpoint/wrap math, 100 rapid tiny nudges,
panic cancellation and Engine exact stereo/64/128 with and without Q/nudges.

The first actual-pointer run exposed IPC rejecting -1 before assigning a request
ID (`build/nudge-mouse.log`). The message-thread validator now accepts exactly -1
or +1 for nudgeStart while retaining other action bounds; malformed directions
are explicitly tested. A new stopped-loop core fixture initially used an incorrect
range for its configured 999 BPM; its expected value was replaced by the exact
reference formula, preserving tight tolerance. No audio/reference tolerance was
loosened or failing case removed.

Connected native capture/finalization/playback audits pass on Windows and Ubuntu:
126 cases / 965,826 samples, max error 7.39476e-8 at 1e-6
(`build/nudge-audio-windows.log`, `build/nudge-audio-ubuntu.log`).

Final realtime core regressions pass after the fixture correction on Windows
(25.45s) and Ubuntu (2.35s): `build/nudge-core-final-windows.log` and
`build/nudge-core-final-ubuntu.log`. ASan/UBSan core plus expanded Start/Q/nudge
PCM pass 2/2 (147.04s), retaining the exact same cases/tolerances
(`build/nudge-asan-tests.log`).

The full Windows/Ubuntu CTest runs pass the other 12/13 gates (178.06s / 310.78s);
the sole initial failure was the corrected core fixture described above. Its
separate final core reruns pass on both systems, so all 13 gates have passing
results for the final test coverage. Full logs: `build/nudge-windows-tests.log`,
`build/nudge-ubuntu-tests.log`; the retained failure is explained rather than hidden.

Final Windows Standalone/test host builds succeed. Actual original-button mouse
and real PCM workflows pass 64 and 128 samples, retaining Q/Start, main gestures,
static/dynamic sync and 4s STOP cases (`build/nudge-mouse-final.log`). Standalone
WebView2 passes signed/invalid nudge IPC, empty guards, original layout/logo and
no product AudioContext. Actual OfflineAudioContext reproduces the original raw
phase -13.488882352941175 throwing RangeError; wrapped phase 0.5111176470588248
renders finite PCM with peak 0.25. All 12 unchanged count-in WebAudio comparisons
also pass (`build/nudge-ui-final.log`); frontend checks pass separately
(`build/nudge-frontend-final.log`). No package/version changes. Hardware/DAW,
Apple platforms, full browser transition envelopes and complete application
parity remain separate acceptance work. Only the documented nudge scope is accepted.

## Recorded-loop waveform — 2026-10-20

The native solid progress fill is replaced by the original committed-waveform
rendering: 720 stereo-max bins, state colors, dim/bright centered bars, thin white
playhead and DPR-aware sizing. Peaks are computed on the finalization worker and
owned by immutable clips. A fixed eight-slot SPSC queue publishes at most two
720-float events per callback; identities in ordinary snapshots stay small. A
message-thread cache and editor known-ID acknowledgements avoid resending unchanged
arrays. Generation/identity matching excludes stale peaks; DEL/reset, history,
NORM, edits and Multiply update or restore the associated cache with the PCM.
Live input/capture oscilloscope and additional waveform interactions remain unported.

The unchanged generateWaveformPeaks oracle passes 42 mono/stereo, 8/48/96 kHz,
1/17/512/720/721/4097/20003-frame fixtures: 30,240 bins, maximum error zero at the
retained 1e-6 conditioning limit. This includes ceil partition and zero-padded tail.
The original committed drawing block is compared independently across 336 width /
state / DPR / progress combinations, including 1x/1.25x/2x/4x scaling, with exact
canvas-operation equality. Protected reference assets remain unchanged.

Initial Windows core tests exposed stack exhaustion after telemetry was embedded
in Engine; its fixed queue now allocates once in construction, with no callback
allocation/free. The history fixture also incorrectly expected NORM to create an
Undo checkpoint. It now asserts the unchanged no-history behavior and tests actual
overdub Undo/Redo instead. A concurrent old probe run briefly blocked a Windows
relink; the final build runs after the prior tests finish. These failures remain
in the initial logs; no reference method/case/tolerance was weakened.

Final Windows asset/core/frontend/waveform gates pass 4/4 (34.73s), Ubuntu core
passes (2.49s), and ASan/UBSan core/waveform passes 2/2 (18.74s). Tests cover NORM,
edit/history/reverse/Multiply, failed growth preserving the old waveform, DEL,
20-track slow-consumer recovery, reset, at-most-two events/block and zero observed
callback new/delete. Logs: `build/wave-acceptance-windows.log`,
`build/wave-core-final-ubuntu.log`, `build/wave-asan-final.log`.

Full Windows/Ubuntu suites pass their other 13/14 gates (156.49s / 297.41s);
the sole initial core failure is repaired and has passing final reruns above.
The final Windows full rebuild also succeeds after the transient relink conflict.
Connected audio audits pass on both systems: 126 cases / 965,826 samples, maximum
error 7.39476e-8 at 1e-6 (`build/wave-audio-windows.log`,
`build/wave-audio-ubuntu.log`). No audio-path tolerance changed.

Production processor/editor pointer/PCM workflows pass at 64/128 samples,
including new real captured-waveform pixels and known-ID payload omission, with
all existing Q/nudge/main/sync/4s STOP checks (`build/wave-mouse.log`). The real
captured screenshot `build/native-mouse-waveform.png` was visually inspected.
Standalone WebView, original layout/logo, count-in WebAudio and tiny-nudge browser
regressions also pass, without a product AudioContext (`build/wave-ui.log`).
Existing Windows Standalone rebuilt; no package/version change. Physical input,
DAW/plugin lifecycle, Apple/iOS and complete application parity remain separate
acceptance work. This accepts committed-loop waveform display only.

## DYNAMIC recording live trace — 2026-10-20

The missing initial-recording oscilloscope now follows the original DYNAMIC
recording path: 512-sample input window, mono speaker downmix, zero prefix after
start, original fill/trace and rolling four-bar markers/count labels. Audio-owned
history and fixed SPSC events carry capture-start sample and generation tags;
message-thread filtering stops stale windows after finish/panic/reset or on a new
take. MASTER/monitor/click output is excluded. Bounded queue overflow drops only
visual telemetry, with no callback allocation/free/lock or capture/output changes.
The original committed waveform takes precedence as before. Other recording modes
and visualizers retain their previously documented scope.

Exact original live-canvas operation comparisons cover 384 width/DPR/mode/state/
elapsed-time cases, plus the retained 336 committed-waveform cases. The unchanged
Loop.getProgress method is compared in 36 sample-rate/mode/elapsed cases at 1e-12.
The initial sync-off comparison reproduced a wrong grid-duration playhead; the
adapter now uses the original ten-second scale when sync is disabled mid-take
(`build/live-progress-before.log`, `build/live-frontend-final.log`). The strict
markup fixture also caught accidental whitespace formatting inside a retained
markup method; the original text was restored, preserving the fixture unchanged.

Core gates pass zero-prefixed/chronological 512-sample history, wrapped windows,
mono/stereo values, bounded overflow, new-start reset, invalid-input protection,
Engine sample deadlines and tags, click/MASTER exclusion, stop/reprepare, exact
64/128 windows and zero observed callback new/delete. Actual WebView2 WebAudio
AnalyserNode comparisons pass 12 mono/stereo/cancellation cases at 8/44.1/48/96 kHz:
6,144 samples, max error zero at 1e-6. Stable signed levels are the browser oracle
scope; varying-stream ordering is tested through Engine. Browser render-quantum
latency and physical input timing/listening are separate, not inferred from these
comparisons. No product AudioContext is created.

Windows core passes (24.88s) and the corrected final frontend gate passes (3.15s).
The initial full Windows run passes the other 13/14 gates (185.85s); its original
markup failure is repaired by the final frontend rerun. Ubuntu full suite passes
14/14 (306.94s). ASan/UBSan core/frontend/waveform gates pass 3/3 (24.77s), before
the later pure-JS playhead repair, which is independently retested. Connected audio
audits on both platforms pass 126 cases / 965,826 samples, max error 7.39476e-8 at
unchanged 1e-6. Logs: `build/live-windows-tests.log`,
`build/live-frontend-acceptance-windows.log`, `build/live-ubuntu-tests.log`,
`build/live-asan-tests.log`, `build/live-audio-windows.log` and
`build/live-audio-ubuntu.log`.

The final playhead/cadence build passes real production processor/editor mouse
workflows at 64/128 samples, including live red/yellow pixels, successive-take
ownership, sync-off ten-second progress and the retained main/Q/nudge/waveform/
4s STOP checks (`build/live-mouse-final.log`). The final captured screenshot
`build/native-live-input.png` was visually inspected. Standalone WebView, all
original count-in and tiny-nudge browser regressions, and the actual live analyser
oracle pass again (`build/live-ui-final.log`), without a product AudioContext.
Final frontend comparisons pass on both Windows (3.15s) and Ubuntu (5.87s),
including the later progress repair (`build/live-frontend-acceptance-ubuntu.log`).
No tolerance was loosened or failed case removed. Existing Windows Standalone
rebuilt; no package/version change. This accepts the documented initial DYNAMIC
recording trace only; full application parity, DAW/platform/physical driver and
render-quantum timing acceptance remain separate work.


## Original user RTRG phase/timing — 2026-10-20

Reproduced a native parity gap: RTRG only restarted the source and never applied
the unchanged `Loop.retrigger` Start reset/master-phase sixteenth snap. Native
RTRG now updates double-precision Start at the tap and uses the original playing
+5ms versus immediate stopped-play paths. Existing UI/keyboard wiring is retained.
The fixed phase FIFO now orders inter-track 5ms/50ms deadlines and preserves rapid
RTRGs. Successful RTRG intentionally cancels that track's stale slider/Q/nudge
releases, preventing later phase resurrection; this mixed-source safety repair
is distinct from exact overlapping WebAudio-envelope parity.

Initial full Windows/Ubuntu runs passed the other 13 gates, but the core suite
failed `old audio stays immutable while capturing edit`: its PCM fixture used
the old immediate graph primitive as a user RTRG. The primitive is now explicitly
`sourceRestart` (not exposed by IPC); all old immediate-source, reverse/history/
Multiply/rate/pan/Engine regressions retain their sample comparisons. Separate
new user-action tests execute the unchanged `Loop.retrigger` method, including
rapid taps, stopped play and solo exclusion. No case or tolerance was removed.
The first native mouse run also failed a test-only selector: the native binder
sets the button title to `RTRG`. Its corrected selector passes without a product
markup change.

Final acceptance:

- Windows: the initial 14-gate run passed 13 gates; repaired core + final Start
  comparison pass 2/2 (62.61s), and the later inter-track/ring-wrap core additions
  pass again (24.49s). Logs: `build/rtrg-tests-windows.log`,
  `build/rtrg-final-windows.log`, `build/rtrg-core-final-windows.log`.
- Ubuntu: the other 13 gates passed in the full run (311.92s); final core/Start
  retest passes 2/2 (98.21s). The expanded unchanged-JS oracle covers 300 RTRGs,
  540 nudges and 1,869,920 rendered samples over 30 cases, max error 5.98023e-8
  at unchanged 1e-6 tolerance; Start metadata stays within 1e-14. Logs:
  `build/rtrg-tests-ubuntu.log`, `build/rtrg-final-ubuntu.log`.
- ASan/UBSan + leak detection: core/frontend/Start pass 3/3 (148.72s); final
  inter-track/ring-wrap core additions pass again (6.64s). Logs:
  `build/rtrg-tests-asan.log`, `build/rtrg-core-final-asan.log`.
- Core tests retain zero callback new/delete, exact Engine stereo/64/128 PCM,
  empty/capture/finalization rejection, full-pool/FIFO rollback, panic/DEL,
  cross-track ordering and 300 ring-wrapped RTRGs.
- Real production processor/editor mouse/PCM workflows pass at 64 and 128,
  including playing/stopped RTRG and Start-display reset alongside all previous
  capture, sync, Q/nudge, waveform/live-trace and 4s STOP cases
  (`build/rtrg-mouse.log`). Standalone WebView2/bridge/keyboard smoke and retained
  actual WebAudio count-in, tiny-nudge and live-analyser oracles pass
  (`build/rtrg-ui.log`). Test-owned apps close afterwards.
- Connected capture/overdub/substitute audits pass on both platforms: 126 cases,
  965,826 samples, max error 7.39476e-8 at unchanged 1e-6. Logs:
  `build/rtrg-audio-windows.log`, `build/rtrg-audio-ubuntu.log`.

The existing Windows Standalone and optional mouse host were incrementally
rebuilt. Version stays 0.4.3; no installer/ZIP or additional format packages were
made. Original UI assets remain byte-identical and the user's local logo edits
remain unstaged. This accepts the documented RTRG state/source-PCM scope; exact
browser transition envelopes/resampling, physical drivers/latency, DAW/MIDI clock,
Apple platforms and full application parity remain separate work.


## Original STUT source and visual clock — 2026-10-20

Enabled the existing STUT button with native source-region playback. Hold selects
the unchanged graph-clock/effective-rate phase and BPM sixteenth bounds; release
restores the full loop at the actual fragment cursor. The white playhead retains
the original independent graph/master clock while held and after release. There
is no recording edit, Undo entry, browser audio engine, or new product panel.
Per-track atomic revisions release regions independently of command-queue capacity
and invalidate deferred starts. Frontend validity predicates also suppress unsent
released/deleted gestures. Blur, pointer cancellation, editor reload/destruction,
STOP ALL/DEL/reset and source replacement clean up the temporary region.

The first model/core gates passed, but actual WebAudio exposed a fractional
clip-tail neighbour bug at 48kHz. A deferred-wrap attempt then failed at 8kHz;
it was discarded. Original zero-clock cases were retained and nonzero graph
clocks added. The repair wraps the interpolation neighbour inside the fragment,
including tiny-region extrapolation; all failing browser cases now pass without
changing preserved JS or loosening PCM tolerance. The initial GUI run also hit a
sandbox localhost/process restriction; its identified test-owned process was
closed and subsequent GUI runs used the existing elevated localhost workflow.
A later mouse fixture lost its exception collector on reload. It now retains the
pre-reload assertion, reattaches the collector and monitors runtime exceptions
across navigation. Its original assertions remain in place.

A final reference review caught a visual parity gap: fragment cursor progress
would replace the original full-loop playhead. The native snapshot now publishes
an independent visual clock after STUT, reset by source replacement/stop. The same
unchanged-JS fixture compares Loop.getProgress during hold/release, frontend gates
verify routing, and actual native draw-stream checks require the playhead to leave
the held audio fragment. All final gates were rerun after that correction.

Final acceptance:

- Windows MSVC Release: 15/15 CTest gates, 199.77s
  (`build/stut-clock-tests-windows.log`). Original UI byte checks, all retained
  Start/RTRG/Q/nudge/capture/reverse/history/Multiply/count-in/metronome/waveform/
  live-trace regressions pass alongside the new STUT gate.
- Ubuntu GCC Debug: 15/15 gates, 354.15s
  (`build/stut-clock-tests-ubuntu.log`). The STUT comparison covers 30 mono-forward/
  stereo-reverse fixtures, 1,350 hold/release state/bounds/progress snapshots and
  321,192 source PCM frames, with maximum error 5.90515e-8 at unchanged 1e-6;
  region bounds stay within 1e-8 frames and visual progress within 1e-12 circular
  distance. Rates 8/48/96kHz, lengths 1/17/512/4097/20003, OFF/STATIC/DYNAMIC,
  fractional BPM and 1x/0.37x/2x, repeated holds and resumed cursors are retained.
- ASan/UBSan with leak detection: core/frontend/STUT/Start pass 4/4, 190.25s
  (`build/stut-clock-tests-asan.log`). Core checks retain zero callback new/delete,
  exact stereo/64/128 output, empty/recording guards, full-pool fragment PCM,
  repeated down, release cursor, tail wrap, panic/DEL/RTRG/reset, zero-block atomic
  release and invalidation of a deferred start with a full command queue.
- Actual WebView2 OfflineAudioContext executes unchanged STUT methods for ordinary
  and tail fragments at zero/nonzero source clocks: 12 cases / 86,016 samples,
  maximum error 1.49012e-8 at unchanged 1e-6. Retained count-in, tiny-nudge and live
  analyser browser comparisons and native bridge/keyboard/pointer smoke also pass
  (`build/stut-clock-ui.log`). No product AudioContext is created.
- Real production processor/editor mouse/PCM workflows pass at 64 and 128,
  including held STUT, original draw clock outside its repeated fragment, outside
  release, blur, editor reload, released deferred starts and all previous main/
  sync/Start/RTRG/Q/nudge/waveform/live-trace/4s STOP checks
  (`build/stut-clock-mouse.log`). Test-owned apps close afterwards.
- Final connected capture/overdub/substitute audits pass on both platforms:
  126 cases / 965,826 samples, maximum error 7.39476e-8 at unchanged 1e-6
  (`build/stut-clock-audio-windows.log`, `build/stut-clock-audio-ubuntu.log`).

Only the existing Windows Standalone and optional mouse test host were rebuilt;
version remains 0.4.3. No ZIP, installer or additional format package was created.
The 16 preserved UI assets remain unchanged, and the user's manual ASCII-logo
edits remain outside this commit. This accepts the documented STUT state/source/
visual-clock scope. Actual browser acceptance currently covers 1x, quantum-aligned
controls in WebView2; broader resampling/envelopes, overlapping future graphs,
physical ASIO/latency, DAW/MIDI, Apple/platform builds and full application parity
remain separate work.

## Waveform WAV export — 2026-10-20

Implemented the original waveform click with the original filename fallback,
UTF-16 sanitization and click-time timestamp. The PCM16 encoder matches unchanged
AudioEngine.bufferToWAV/encodeWAV. Exports read stored samples, independent of
playback volume/pan/reverse/rate/Start/STUT. No preserved UI asset was edited.

Audio pins one immutable clip through a bounded independent request/mailbox;
encoding and lease destruction run on the existing worker. The frontend gesture
FIFO waits for earlier edits/finalization and resolves the latest identity, while
keeping the name typed at the click. DEL cancels an unsent track-owned export.
The native picker supplies the destination; a background temporary-file writer
checks write/flush/replacement. Selected writes finish before picker resources
are destroyed. Empty, stale, failed and canceled exports keep audio/history.

Repairs and retained failure evidence:

- An unload/bootstrap-only cleanup failed the 128-sample reload test: a delayed
  picker callback saved after reload. Native pageAboutToLoad cancellation now
  invalidates it. The retained final test disables JavaScript across navigation
  and releases the old callback before reenabling it, checking the native boundary.
- Rapid NORM/export could bypass the queued edit and export the earlier clip.
  Gesture ordering/finalization waits now select the normalized committed take.
  Unit checks retain click-time naming and DEL cancellation; real saved PCM has
  the normalized peak, instead of the earlier <0.3 peak.
- A timed 400ms test picker raced slow CDP round trips. The fixture now waits for
  explicit completion markers and retains busy, cancel, failure, overwrite and
  late-callback cases. Repeated waveform clicks keep the current export status.
- Retained short-cue mouse tests sampled only gaps between real 100ms count-in
  sounds. A passive callback trace recorded outputPeak 0.494280159 while armed at
  frame 630080, before deadline 666016, with MASTER/flash zero. The integration gate
  now checks actual rendered PCM, the displayed CANCEL state, both mouse cancels
  and absence of later PCM. CountInTests retains the exact per-block outputPeak
  contract with an every-block consumer and checks passive inspection consistency.
  Product sound/timing and thresholds were not changed. CDP pointer events are
  queued before awaiting acknowledgements so response latency cannot extend taps
  into >=600ms holds. Both single-bar stop acknowledgements and late-duplicate
  checks remain, with verification after the timed pair. All 3800/4200ms holds and
  analytical 3999/4000ms boundaries remain.
- Early focused tests also needed explicit WebView startup/rendered-state waits
  and auto-play initialization. An intermediate test loaded newer assertions
  against an older binary; the final checks rebuilt both targets first. Failed
  diagnostic logs remain under build/. No reference method or tolerance was changed.

Final automated results:

- Windows core: 16/16, 203.37s (`build/wav-pcm-tests-windows.log`).
- Installed Ubuntu core: 16/16, 389.11s (`build/wav-pcm-tests-ubuntu.log`).
- Ubuntu ASan/UBSan with leak detection: core/frontend/WAV gates 3/3, 26.25s
  (`build/wav-pcm-tests-asan.log`).
- Exact WAV comparison: 86 cases / 767734 bytes, maximum byte difference 0.
  Includes original conditioned capture PCM at 8/48/96kHz, mono/stereo, tiny and
  longer clips; 30 repeated playback-control exports remain byte-identical.
  Encoder cases include empty buffers, clipping, nonfinite samples, fractional
  rate header conversion, NaN fallback and original filenames.
- Core ownership/OOM/stale identity/DEL/prepare/bound/zero-block/future-command
  cases and exact 64/128 capture bytes pass with zero callback new/delete.
- Full production processor/editor mouse workflows pass at 64 and 128, retaining
  arm/cancel/record/play, all sync/Start/Q/nudge/RTRG/STUT/waveform/live-trace/
  hold/cancellation cases (`build/wav-pcm-mouse.log`). Export cases cover actual
  saved stereo PCM, naming, repeat clicks, busy rejection, cancel/failure recovery,
  NORM ordering, overwrite, deletion and native-only navigation cancellation.
  Focused export/lifecycle checks pass again at both block sizes after the final
  writer/picker destruction-order adjustment (`build/wav-shutdown-export.log`).
- Actual WebView2 WebAudio count-in, nudge, analyser and STUT comparisons and
  NativeUiSmoke pass (`build/wav-pcm-ui.log`); EN/ES/PT export instructions are in
  the existing manual panels. No product AudioContext is created.
- Connected native/reference audio audits pass on both platforms: 126 cases /
  965826 samples, maximum error 7.394757495227111e-8 at unchanged 1e-6
  (`build/wav-pcm-audio-windows.log`, `build/wav-pcm-audio-ubuntu.log`).

Remaining acceptance gate: the real Windows save dialog could not be exposed by
the Windows automation helper (capture timeout / no returned dialog window).
The real picker path was invoked and canceled through the native API while the
synthetic loop stayed playing; that is not a verified OS Save/Cancel button test.
Manual visibility/save confirmation remains pending, and no subsequent feature
is started or full export acceptance claimed. Apple/Linux native picker behavior,
physical ASIO/latency, DAW/MIDI and full application parity remain separate work.

Only the existing Windows Standalone and excluded mouse host were incrementally
rebuilt. Version remains 0.4.3; no installer, distribution ZIP or additional format
package was created. The user's local ASCII-logo template edits remain uncommitted.

### WAV export Windows manual gate and lifecycle follow-up — 2026-10-20

The earlier pending Windows visibility/save gate is now resolved. The user
confirmed the real dialog was visible and saved `NativePicker_1792525456508.wav`
in Documents. NativePickerProbe reported `WAV saved`, exportBusy false, and the
same playing 24960-frame clip at 48000 Hz. Independently read
`D:\Jorge\Documents\NativePicker_1792525456508.wav`: valid RIFF size, 99884 bytes,
stereo PCM16, 48000 Hz, 24960 frames; both channels contain nonzero, distinct PCM.
Absolute integer peaks are 8194 and 8413. SHA-256:
`7abfd57b1e9e12956db2faa8512511cf167567cb4fd5b4311761cd3809751915`.
The Windows helper's capture timeout/no returned dialog window was an automation
limitation, not evidence that the actual dialog was invisible. The synthetic
host remains open for the user; no physical input or device was used.

Extended ExportWorkflow to cancel a pending destination, start another export,
then deliver the obsolete callback with a valid destination while the replacement
picker waits. It checks no obsolete file is written, the replacement keeps its
own busy state/filename/status, clip identity and transport command ID are
unchanged, and the replacement's entire WAV equals the earlier stored snapshot.
Existing production guards pass; no DSP or product UI change was required.

- Production processor/editor export workflows pass at 64 and 128 samples,
  including cancel/replacement/late-callback isolation
  (`build/wav-picker-lifecycle.log`).
- Windows exact-JS WAV, frontend command-ordering and waveform gates pass 3/3,
  15.70s. The broader Windows/Ubuntu/sanitizer results above remain the latest;
  they were not rerun for this test-only follow-up.
- No rebuild, version change, installer or distribution package was needed.
  The user's local logo edits remain untouched and unstaged.

This accepts the Windows stored-PCM waveform export increment within the tested
scope. Actual OS Cancel/overwrite button interaction, Apple/Linux native picker
behavior, physical ASIO/latency, DAW/MIDI and full application parity remain
separate validation work. No subsequent feature was implemented in this follow-up.

## Native MERGE — 2026-10-20

Restored the original MERGE source selector and GO button, including the existing
manuals' workflow and optional source-clear confirmation. No preserved asset was
edited. An empty destination receives independent source PCM, BPM/dynamic flags
and stays stopped; existing destinations retain length/playing phase and gain
one local/global Undo entry. The worker pins immutable base/source clips and
performs the original zero-offset, feedback-one circular mixer, without a second
capture-conditioning pass. Playback controls do not process stored samples.

Implementation and repair evidence:

- First Windows real-time test launch failed with `0xc00000fd` (stack overflow)
  after expanding telemetry. Moved the bounded EngineView SPSC queue to
  construction-time heap storage, retaining zero callback allocation/free and
  all original assertions. No stack-limit increase or tolerance change.
- Source and destination DEL, prepare, worker allocation failure, full recording
  pool, clone capacity, playing forward/reversed sources, dynamic BPM metadata,
  global/local Undo/Redo and exact 64/128 Engine PCM are covered in MergeTests.
- Delayed source clear is guarded by generation, completion, source identity and
  clear epoch. STOP ALL/reset invalidates pending completion, including a worker
  result committed afterwards. A new source capture is protected even when its
  old stored waveform still matches. This is checked at frontend and audio owner.
- Review caught that Undo/Redo could leave an older completion valid. History
  restoration and later successful destination PCM edits now invalidate it;
  frontend also checks destination waveform identity. Retained actual native
  stale-request rejection after Undo/Redo and during new source overdub. Failures
  do not publish success or modify prior PCM/history.
- Native merges serialize against capture/finalization, matching the existing
  single-worker safety boundary. These guards improve original asynchronous
  races; they do not claim identical browser envelope transitions.

Reference/audio acceptance:

- Unchanged user-level `LoopManager.mergeLoops`, `pushUndoState`, `sliceBuffer`
  and `mixBuffersCircular`, with original capture conditioning: 72 cases,
  1785720 exact stereo WAV bytes, maximum committed playback error
  7.907836940823643e-8 at unchanged 1e-6. Includes 8/48/96kHz, duplicated mono /
  distinct stereo, tiny and longer clips, clipping and multi-wrap sources up to
  48001 frames; source preservation and Undo/Redo remain exact.
- Full Windows 17/17, 256.73s (`build/merge-tests-windows.log`), and Ubuntu 17/17,
  429.79s (`build/merge-tests-ubuntu.log`). These broader runs precede the final
  source-clear guards; focused final checks are recorded below.
- Both full 64/128 production mouse/PCM workflows pass, retaining arm/cancel/
  record/play, sync, Start/Q/nudge, STUT/RTRG, held STOP/delete, waveform/live-trace
  and export checks (`build/merge-mouse-full.log`).
- Actual WebView2 count-in/nudge/analyser/STUT and native UI/bridge checks pass
  (`build/merge-ui.log`). Original 20 cards remain; unported controls are disabled,
  original assets are byte-identical and no product AudioContext is created.
- Connected capture/overdub/substitute audit passes on Windows and Ubuntu:
  126 cases, 965826 samples, maximum error 7.394757495227111e-8 at unchanged 1e-6
  (`build/merge-audio-windows.log`, `build/merge-audio-ubuntu.log`).
- Final production MERGE/GO tests pass at both 64/128, using real CDP mouse events
  and actual JavaScript confirmation accept/decline. Complete saved PCM verifies
  clone/source preservation and Undo/Redo; native panic, stale completion after
  Undo/Redo, and active-source-capture guards pass
  (`build/merge-acceptance-mouse.log`). Only file destination selection uses the
  existing test-host picker.
- Final Windows core/merge/frontend gates pass 3/3, 73.22s
  (`build/merge-acceptance-windows.log`).
- Final Ubuntu core/merge/frontend gates pass 3/3, 75.72s
  (`build/merge-acceptance-ubuntu.log`). ASan/UBSan with leak detection passes
  final core/frontend 2/2, 12.56s (`build/merge-acceptance-asan.log`); the preceding
  sanitized unchanged-JS merge gate also passed, 3/3 overall, 115.21s
  (`build/merge-final-asan.log`).

Only the existing Windows Standalone and excluded synthetic test host were
incrementally rebuilt. Version remains 0.4.3; no installer, ZIP or additional
plugin-format package was made. Local user logo edits remain untouched and
unstaged. Physical drivers/latency, DAW/MIDI, exact overlapping WebAudio source
transitions and Apple/Linux native UI remain separate acceptance work. This
increment does not establish full application parity or production readiness.

### MIDI looper control and learn (2026-10-20)

Scope: original note-driven loop main actions, OD/SUB/SUS, native learn for
Note/CC/PC, standalone MIDI routing and persisted bindings. Original ASCII UI
assets remain byte-identical. Continuous parameter/expression mappings, synth
MIDI, MIDI clock and HID/gamepad remain unported.

- Windows full core/reference suite: 18/18, 260.57s
  (`build/midi-tests-windows.log`); Ubuntu 18/18, 435.50s
  (`build/midi-tests-ubuntu.log`). Final additional core/MIDI cases pass 2/2,
  29.82s (`build/midi-final-core-windows.log`). Final ASan/UBSan with leak
  detection passes core/MIDI/frontend 3/3, 16.84s (`build/midi-final-asan.log`).
- Unchanged default MIDI handler/table comparisons cover 16384 messages.
  Core tests retain exact MIDI/direct-action stereo WAV bytes and sample offsets,
  600ms/4s boundaries, PC/CC pulse rules, SUS release/cancel, absent-input rejection,
  long RECORD-origin protection, MIDI Stop cancellation and zero callback new/delete.
- Actual production JUCE processor tests pass at 64/128, including learned PC 0
  restored and performed with no editor, invalid state rollback and zero
  processBlock new/delete (`build/midi-acceptance-processor.log`).
- Full main/export/merge/MIDI mouse/PCM workflows pass at 64/128
  (`build/midi-mouse-full.log`). Final focused MIDI mouse tests pass at both
  block sizes (`build/midi-acceptance-mouse.log`). These use the production
  processor/editor and synthetic PCM/MIDI, not a physical input device.
- Unchanged NativeUiSmoke exposed an OD/SUB regression: an old MIDI mode snapshot
  overwrote a newer keyboard choice (`build/midi-ui.log`). Fixed with revision
  acknowledgement fencing; retained the original failing assertion and added a
  deterministic out-of-order reply/snapshot regression. Final unchanged native
  UI/bridge and actual WebAudio count-in/nudge/analyser/STUT checks pass
  (`build/midi-acceptance-ui.log`). No product AudioContext is created.
- Connected capture/overdub/substitute audit passes on Windows and Ubuntu:
  126 cases, 965826 samples, maximum error 7.394757495227111e-8 at unchanged 1e-6
  (`build/midi-audio-windows.log`, `build/midi-audio-ubuntu.log`).
- Real Standalone: AudioBox USB MIDI input enabled, output muted. User reports
  Loop 1 learned `PC 0 ANY`; independent native telemetry confirms binding 3
  (packed PC 0/omni), learn target cleared, six messages, three actions and a
  stopped 176400-frame recording at 44100 Hz, with no overflow/rejected action.
  Subsequent real-device observation confirms queued-to-playing playback with
  the same recording intact (54 messages, 51 actions, no overflow/rejection).
  The user then reported a real failure: the second pedal tap queued playback
  instead of stopping. This kept physical acceptance open until the repair below.

Full suite runs precede the final UI mode fence and processor input-ready cache;
final processor, frontend/native UI and focused MIDI mouse tests exercise those
fixes. The final binary rebuild adds only manual text afterwards. Only the
existing Windows Standalone and excluded test tools were incrementally rebuilt;
version remains 0.4.3 with no installer/ZIP/additional format package. Original
local logo edits are preserved separately. Broader hardware, continuous MIDI
parameters, clock, Apple/Linux UI and commercial readiness remain separate work.

#### Physical pedal stop repair

Raw bounded MIDI telemetry reproduced the cause: one physical pedal tap sent
three identical channel-1 PC 0 packets, about 0-1ms apart. Treating each packet
as a tap stopped, immediately stopped again, then queued a restart. A fixed
20ms window, anchored to the first matching program/channel/target packet,
now merges that burst into one action. It does not extend with each duplicate;
different programs/channels and later taps remain independent. Notes/CC and
original preserved JS are unchanged. Diagnostic history is eight fixed entries;
serialization occurs on the message thread, with no callback allocation.

The user repeated the physical arm/record/play/stop workflow and confirmed it
now stops and stays stopped. Independent native telemetry: six incoming PC 0
messages, two accepted actions, a stopped 176400-frame clip at 44100 Hz, zero
rejected actions and zero overflow. This closes the observed physical pedal
control failure, not audible sound/latency, reconnect or broader hardware tests.

Retained regression cases exercise triple PC bursts both within and across
blocks, static/dynamic STOPPING-to-STOPPED, sync-off recording/stop, and the
159/160-sample boundary at 8kHz. GUI MIDI tests send triple packets through the
actual preallocated JUCE MidiBuffer at both 64/128. Existing single-message
coverage and all prior comparisons remain in place. Final result logs:
`build/midi-pc-burst-final-windows.log`, `build/midi-pc-burst-final-ubuntu.log`,
`build/midi-pc-burst-final-asan.log`, `build/midi-pc-burst-processor.log` and
`build/midi-pc-burst-mouse.log`. See those logs for final pass status/durations.
Final focused gates pass: Windows 3/3 (33.33s), Ubuntu 3/3 (10.09s),
ASan/UBSan with leak detection 3/3 (18.75s), actual JUCE processor tests and
triple-PC native mouse workflows at 64/128. No new package/version bump was made.

### Continuous MIDI loop volume/pan (2026-10-21)

Scope: original right-click CC learn on existing loop volume/pan sliders, all
20 loops, continuous zero/repeated values, native sample timing, persisted
bindings and legacy 33-slot state migration. Original resources/ui assets and
local logo edits remain untouched. Other MIDI/expression targets remain unported.

- Unchanged App.handleMIDIMessage and original UIManager setters: 4256 CC
  messages, all values/channels, zero/repeats/nonmonotonic movement; maximum
  float metadata error 5.913531686552176e-8 at 1e-7. Original pan parseInt steps
  are exact. Retains all prior 16384 default message comparisons.
- Connected Engine capture/playback uses distinct nonconstant stereo input,
  with a nonzero-output assertion. Sample-timed MIDI and directly scheduled
  slider controls render exactly equal PCM across 64/128, including zeros,
  both pan endpoints and rapid expression movement. This establishes routing
  through existing native smoothing, not bit identity of every browser
  AudioParam automation envelope.
- Actual production JUCE MidiBuffer tests pass at 64/128: CC-zero learn consumes
  its event, continuous CC changes controls without changing capture state,
  saved expression works without an editor, legacy 33-slot mappings survive,
  invalid duplicate XML preserves prior state and processBlock new/delete is
  zero (`build/midi-expression-processor.log`).
- Full Windows 18/18, 254.15s (`build/midi-expression-tests-windows.log`);
  full Ubuntu 18/18, 423.36s (`build/midi-expression-tests-ubuntu.log`).
- ASan/UBSan with leak detection: core/MIDI/frontend 3/3, 17.14s
  (`build/midi-expression-asan.log`). No tolerance was relaxed or test removed.
- Full native main/export/merge/MIDI mouse/PCM workflow passes at 64/128
  (`build/midi-expression-mouse-full-retry.log`). It exercises actual CDP
  right-click learning, CC zero/repeats, integer pan, conflicting button learn,
  10s native timeout, Escape, reload, shift-right-click clear and save/disable.
  All prior arm/record/play, sync, holds/delete, waveform, STUT/RTRG and PCM
  export/merge checks are retained.
- The first sandboxed GUI launch never initialized a CDP page, so it failed
  before interaction (`build/midi-expression-mouse-full.log`). Repeating with
  desktop access passed; the failed synthetic host was terminated by exact PID.
  No product assertion or timeout was weakened to compensate.
- Unchanged NativeUiSmoke and actual WebAudio count-in/nudge/analyser/STUT
  comparisons pass (`build/midi-expression-ui.log`); 20 original cards, validated
  IPC, disabled unported controls and no product AudioContext are retained.
- Windows and Ubuntu connected capture/overdub/substitute audits each pass
  126 cases and 965826 samples, maximum error 7.394757495227111e-8 at unchanged
  1e-6 (`build/midi-expression-audio-windows.log`,
  `build/midi-expression-audio-ubuntu.log`).

Physical expression check: the updated Standalone is open with output muted,
AudioBox USB enabled and unchanged PC pedal mappings. A native 10s learn timeout
was observed. No expression CC has yet arrived; no physical expression/listening
claim is made. The earlier user-confirmed physical PC record/play/stop repair
remains accepted. The optional check does not authorize inferring hardware success
from synthetic injection. Only existing binaries/test tools were incrementally
rebuilt; version remains 0.4.3, with no installer/ZIP/additional format package.

### MIDI learn burst repair (2026-10-21)

The user deferred physical expression testing until today after 19:45 Lisbon;
a one-time app reminder is scheduled. Work stayed within MIDI volume/pan rather
than starting another feature. Physical acceptance remains pending.

A retained new core regression failed before the fix: learn from CC 13 value 0
at sample 17, then send value 127 at sample 18 in the same block. The old native
code discarded movement until the message thread acknowledged the mapping
(`build/midi-learn-burst-repro.log`, failed core gate, 27.71s). The unchanged
JavaScript consumes the first packet and applies the next one immediately.

Fixed with one bounded audio-owned provisional target after a linear overlap
check; persisted settings remain message-owned. Mode-only configurations keep
that target without duplicating learn results. Cancellation/changed ownership
clears it; conflicting omni/channel assignments remain blocked. No callback
allocation, lock, unbounded queue or UI redesign was introduced.

Acceptance:

- Windows core/MIDI/frontend 3/3, 32.23s
  (`build/midi-learn-burst-windows.log`); Ubuntu 3/3, 10.29s
  (`build/midi-learn-burst-ubuntu.log`). These are focused repair gates; prior
  complete 18/18 regression evidence remains recorded above.
- ASan/UBSan with leak detection 3/3, 17.49s
  (`build/midi-learn-burst-asan.log`). Existing tests/tolerances are retained.
- Unchanged-JS MIDI oracle now also compares learn-packet consumption and
  immediate next CC, without native message-thread acknowledgement. Retains all
  prior 16384 default and 4256 assigned continuous message comparisons.
- Core same-block movement, mode-only update, single learn-result publication,
  cancellation and conflicting channel-specific assignment protection pass.
  Existing exact nonzero stereo PCM comparison retains direct/preassigned
  variants and adds provisionally learned volume, at both 64/128 block sizes.
- Actual production JUCE MidiBuffer applies a pan learn and immediate movement
  at samples 17/18 before the message owner runs; state migration/persistence,
  editor-closed performance, overflow and zero processBlock new/delete pass
  (`build/midi-learn-burst-processor.log`).
- Actual CDP slider right-click plus same-block learn/movement reaches the final
  volume without another input event, at 64/128; existing MIDI cases also pass
  (`build/midi-learn-burst-mouse.log`).
- Unchanged NativeUiSmoke and actual WebAudio count-in/nudge/analyser/STUT pass
  (`build/midi-learn-burst-ui.log`). No product AudioContext is created; original
  resources/ui remains byte-identical.
- Connected audio audits pass on Windows and Ubuntu: each 126 cases, 965826
  samples, maximum error 7.394757495227111e-8 at unchanged 1e-6
  (`build/midi-learn-burst-audio-windows.log`,
  `build/midi-learn-burst-audio-ubuntu.log`).

Only existing Windows Standalone/test tools and core probes were rebuilt.
The earlier muted test app was gracefully closed only after checking that all
20 tracks were empty and MASTER was zero; no recorded audio was discarded.
Version remains 0.4.3 with no installer/ZIP/additional format package. The updated
Windows executable is ready for the deferred physical check; tests do not prove
physical expression response/listening or general browser-envelope parity.

### Ordered MIDI enable/disable repair (2026-10-21)

Work stayed within the current MIDI feature while the physical expression test
remains deferred to today after 19:45 Europe/Lisbon. No next feature was started.

A deterministic test using the production frontend listener reproduced a native
race: two rapid clicks before another audio snapshot both requested enabled=true,
so one toggle disappeared (`build/midi-toggle-repro.log`, actual true/expected
false). The existing button now sends relative toggle intent. Processor resolves
each request against its latest accepted desired state under the existing
non-audio mutex and queues immutable settings. Explicit enable(bool) stays
idempotent; both operations cancel learn and its deadline. No DSP algorithm or
interface layout changed. Original browser requestMIDIAccess is not a toggle;
this repairs the already existing native enable/disable extension.

Acceptance:

- Frontend double/triple clicks from both initial states without snapshot updates
  pass (`build/midi-toggle-frontend.log`). The original failing case is retained.
- Actual native processor checks two queued toggles before another audio block,
  persisted desired state after each and final audio-owned enabled state, at
  64/128. Prior MIDI learn/expression/PC burst/legacy state/overflow/no-editor
  cases and zero processBlock new/delete pass (`build/midi-toggle-processor.log`).
- A new saved-state boundary test rejects binding decimal 18446744073709551616
  while preserving the complete previous state. It passed before the toggle fix
  (`build/midi-state-overflow-repro.log`); no numeric-parser failure/rewrite is
  claimed. This complements existing invalid duplicate-state rollback tests.
- Windows focused core/MIDI/frontend 3/3, 29.78s
  (`build/midi-toggle-windows.log`); Ubuntu 3/3, 10.15s
  (`build/midi-toggle-ubuntu.log`). ASan/UBSan core/MIDI/frontend with leak
  detection passes 3/3, 18.90s (`build/midi-toggle-asan.log`). Core/DSP sources
  are unchanged, so their existing binaries were reused for these checks.
  The modified production Processor is tested in the Windows native targets.
- Real CDP press/release/press/release preserves both MIDI revisions and final
  enable state at 64/128, retaining the existing full MIDI workflow cases
  (`build/midi-toggle-mouse.log`).
- Unchanged NativeUiSmoke and actual WebAudio count-in/nudge/analyser/STUT gates
  pass (`build/midi-toggle-ui.log`); original 20 cards, IPC validation, disabled
  unported controls and no product AudioContext remain intact.
- Connected audio audits pass on both Windows and Ubuntu: each 126 cases,
  965826 samples, maximum error 7.394757495227111e-8 at unchanged 1e-6
  (`build/midi-toggle-audio-windows.log`, `build/midi-toggle-audio-ubuntu.log`).

Only existing Windows Standalone and test binaries were incrementally rebuilt.
Version remains 0.4.3 with no installer, ZIP or additional format package. Original
UI assets and the user's separate logo edits are preserved. Physical expression,
listening, broader drivers/DAWs and platform parity remain separate acceptance.

### MIDI learn cancellation repair (2026-10-21)

No new feature was started while physical expression testing remains deferred to
the user's reminder for today after 19:45 Europe/Lisbon. This audit found and
fixed four cancellation failures within the existing MIDI feature:

- Native cancelLearn rejected when paused audio left the 31 usable configuration
  slots full (`build/midi-cancel-full-repro.log`).
- Escape before the first/updated MIDI snapshot did not request cancellation
  (`build/midi-cancel-stale-repro.log`).
- The learn indicator stayed active until another audio callback, despite
  successful cancellation (`build/midi-cancel-paused-view-repro.log`).
- Focused loop-name input stopped the event before the document listener
  (`build/midi-cancel-name-repro.log`).

All failing assertions are retained. Escape now reaches the native cancellation
fallback in capture phase; the message owner's accepted intent drives the learn
indicator. Unrelated keys, ordinary name-field isolation, audio-thread bounds
and persisted mappings remain intact. No DSP algorithm or interface layout changed.

Acceptance:

- Windows core/MIDI/frontend 3/3, 27.57s (`build/midi-cancel-windows.log`);
  Ubuntu 3/3, 10.04s (`build/midi-cancel-ubuntu.log`); sanitized core/MIDI/frontend
  with leak detection 3/3, 19.04s (`build/midi-cancel-asan.log`).
- Final additional focus/reference checks pass Windows 2/2, 3.27s
  (`build/midi-cancel-focus-windows.log`), Ubuntu 2/2, 6.05s
  (`build/midi-cancel-focus-ubuntu.log`), and ASan/UBSan MIDI probe/frontend 2/2,
  5.98s (`build/midi-cancel-focus-asan.log`). Core/DSP sources are unchanged;
  existing exact stereo PCM, timing and zero-callback-new/delete gates remain.
- Final actual production processor checks pass at 64/128, including a full
  configuration queue with no callbacks, cancellation/resume without stale
  assignment, fresh-learn recovery and cancelled indicator before another block
  (`build/midi-cancel-focus-processor.log`). All previous native MidiBuffer,
  state/legacy/overflow/editor-closed cases and zero processBlock new/delete pass.
- Real 64/128 right-click/immediate Escape and Escape while a loop-name field
  owns focus pass, along with the retained full MIDI workflow
  (`build/midi-cancel-focus-mouse.log`).
- Original keyboard Escape is executed unchanged for range/text input targets.
  All previous 16384 default and 4256 continuous message comparisons, immediate
  learned movement and original mapping markup comparisons are retained.
- The connected Ubuntu audio audit passes 126 cases, 965826 samples, maximum
  error 7.394757495227111e-8 at unchanged 1e-6
  (`build/midi-cancel-audio-ubuntu.log`). Windows connected-audio acceptance
  from the preceding increment is retained; no core/DSP processing code changed.

The earlier GUI/UI runs passed before the final focus refinement
(`build/midi-cancel-mouse.log`, `build/midi-cancel-ui.log`,
`build/midi-cancel-final-mouse.log`, `build/midi-cancel-final-ui.log`). Final
unchanged NativeUiSmoke and actual WebAudio count-in/nudge/analyser/STUT evidence
is in build/midi-cancel-focus-ui.log. These gates supplement the original 20-card,
IPC/no-product-AudioContext and unchanged asset checks; they do not prove physical
expression/listening or general browser-envelope/driver/DAW parity.

Only existing Windows Standalone and test tools were incrementally rebuilt;
version remains 0.4.3 with no installer/ZIP/additional format package. Original
UI assets and the user's separate logo edits remain preserved.

### MIDI learn during held performance (2026-10-21)

The retained failing regression (`build/midi-held-learn-repro.log`) showed that
starting an unrelated slider learn prematurely finished a held SUS recording.
The same global cancellation also restored a held momentary mute and suppressed
an uninterrupted STOP-origin delete deadline. Slider acknowledgement and the
full-queue cancel fallback shared this performance-cancellation path.

Learn/provisional ownership now clears independently. Held performance remains
owned until release; disable, panic, overflow, device changes and transport-binding
replacement retain their previous safety cancellation. No layout or DSP algorithm
changed and no new feature was started.

Acceptance evidence:

- Windows and Ubuntu core/MIDI/frontend/original-assets checks: 4/4 each
  (`build/midi-held-windows.log`, `build/midi-held-ubuntu.log`). The retained
  four-second boundary and disable/panic cases pass. Captured stereo WAV is
  byte-identical between direct commands, normal MIDI taps and held SUS across
  learn/CC movement/acknowledgement, at both 64/128 sample blocks.
- ASan/UBSan with leak detection: 4/4 (`build/midi-held-asan.log`), including
  zero callback new/delete and the final exact held-recording PCM regression.
- Production JUCE MidiBuffer tests pass at 64/128
  (`build/midi-held-processor.log`), including a full 31-entry configuration
  queue, cancelled learn with recording still held, then the owned NoteOff.
  The immediate-release fixture explicitly disables sync; dynamic sync may
  legitimately queue the recording finish until its grid boundary.
- Actual native mouse right-click/learn acknowledgement/Escape preserves a held
  synthetic recording/mute at 64/128 (`build/midi-held-mouse.log`). Existing PC
  burst, expression, timeout, mapping, reload and cancellation cases remain.
- Unchanged original context-menu, MIDI message and Escape methods preserve
  a note's press/release routing. This spy comparison establishes input ownership,
  not reference DSP; previous 16384 default and 4256 continuous cases remain.
- Windows unchanged-JS DSP/state/sync gates: 3/3
  (`build/midi-held-parity-windows.log`). Windows and Ubuntu connected capture/
  overdub/substitute audit: 126 cases, 965826 samples, maximum absolute error
  7.394757495227111e-8 at unchanged 1e-6
  (`build/midi-held-audio-windows.log`, `build/midi-held-audio-ubuntu.log`).
- Unchanged 20-card native UI/keyboard and actual WebAudio count-in, nudge,
  analyser and STUT comparisons pass (`build/midi-held-ui.log`).

Version remains 0.4.3; only existing binaries were incrementally rebuilt, with
no installer/ZIP/additional format package. Original assets and local logo edits
are preserved. Physical expression remains deferred to the requested hardware
check; synthetic tests do not establish listening/driver/DAW/platform parity.

### CC pedal release intercepted by slider learn (2026-10-21)

The retained failing case (`build/midi-cc-release-repro.log`) reproduced a held
CC SUS recording stranded by its zero/release being consumed as a slider-learn
candidate. A conflicting pending learn also swallowed subsequent owned releases.
The audio owner now delivers a CC zero to its existing exact kind/channel/key
owner before learn interception. The packet still undergoes normal learn and
atomic duplicate validation; it cannot steal the transport binding or cause a
new press. Ordinary continuous-parameter dispatch order is preserved.

Acceptance:

- Windows focused core/MIDI/frontend/assets plus unchanged-JS DSP/state/sync:
  7/7, 52.38s (`build/midi-cc-release-windows.log`). Ubuntu focused checks: 4/4,
  11.68s (`build/midi-cc-release-ubuntu.log`).
- ASan/UBSan with leak detection: 4/4, 20.95s
  (`build/midi-cc-release-asan.log`). Callback new/delete remains zero.
- Core cases retain SUS record/play/mute, different-channel isolation,
  duplicate/provisional rejection and next-positive-edge recovery. Captured
  stereo WAV is byte-identical to direct sample-scheduled commands at 64/128,
  including a CC release consumed by pending slider learn at sample 900.
  Previous normal-tap and held-note/learn PCM comparisons remain intact.
- Production JUCE MidiBuffer at 64/128 passes recording release, conflicting
  mapping preservation, commit to playback, rearmed press and mute release
  (`build/midi-cc-release-processor.log`). All earlier persistence/full-queue/
  legacy/overflow and processBlock allocation checks remain.
- Actual original mouse mapping/right-click slider learn with injected synthetic
  CC press/release passes at 64/128 (`build/midi-cc-release-mouse.log`), including
  duplicate rejection, subsequent press and momentary mute restoration.
- Connected unchanged-JS capture/overdub/substitute audio audit: 126 cases,
  965826 samples, maximum error 7.394757495227111e-8 at unchanged 1e-6
  (`build/midi-cc-release-audio.log`). Browser envelopes/resampling, latency
  compensation and sync are outside this committed-PCM comparison.

- Original native 20-card UI/keyboard smoke and actual WebAudio count-in, nudge,
  analyser and STUT references pass (`build/midi-cc-release-ui.log`).

CC transport mapping is a native extension, not a claim that original JS offered
CC button transport. The retained original-JS note and continuous-mixer gates
cover their own behavior. Physical expression/listening and driver/DAW/platform
acceptance remain separate. No new feature/layout/package or version bump;
existing Windows binaries were incrementally rebuilt and local logo edits kept.
