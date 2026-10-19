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
