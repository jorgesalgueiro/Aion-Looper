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

### Panic after MIDI learn capture, before acknowledgement (2026-10-21)

A new production regression exposed a stale mapping commit: capture a Program
Change for learning, then receive MIDI Stop/CC120/CC123 or an overflow before
message-thread acceptance. Audio cleared learn/provisional state, but the queued
result still matched the message owner's token and generation. The first audio
snapshot hid the commit; immediately saved state contained the cancelled binding.
The failing assertion is retained (`build/midi-learn-panic-repro.log`).

Captured results now carry an internal audio-published lock-free cancellation
serial. Newly received and pending results must still match it before acceptance.
Cancelled results use the existing native learn-cancellation path. The snapshot
queue is not a validity oracle; previously accepted mappings remain intact.
Saved-state version, frontend protocol, original layout and DSP are unchanged.

Acceptance evidence:

- Windows focused core/MIDI/frontend/assets and unchanged-JS DSP/state/sync:
  7/7, 59.86s (`build/midi-learn-panic-windows.log`).
- Ubuntu focused checks: 4/4, 14.30s (`build/midi-learn-panic-ubuntu.log`);
  ASan/UBSan plus leak detection: 4/4, 21.24s
  (`build/midi-learn-panic-asan.log`). Callback new/delete remains zero; earlier
  exact stereo capture/expression, held release and four-second gates remain.
- Final production JUCE MidiBuffer cases pass at 64/128 with both ordinary and
  saturated/stale telemetry. MIDI Stop, CC120, CC123 and a subsequent 300-message
  overflow cannot persist a cancelled mapping or revive it on the next callback.
  Fresh learn recovers and consumes its packet
  (`build/midi-learn-panic-processor.log`). Core cases also invalidate an already
  popped result after device/global or queue-independent learn cancellation.
- Actual original mapping clicks and synthetic same-block PC/panic messages pass
  at 64/128, with no later mapping revival and successful fresh-learn recovery
  (`build/midi-learn-panic-mouse.log`). All previous MIDI mouse cases remain.
- Connected unchanged-JS capture/overdub/substitute audio audit passes 126 cases,
  965826 samples, maximum absolute error 7.394757495227111e-8 at unchanged 1e-6
  (`build/midi-learn-panic-audio.log`). Browser envelopes/resampling, input latency
  compensation and sync remain outside this committed-PCM comparison.

- Final unchanged 20-card native UI/keyboard smoke and actual WebAudio count-in,
  nudge, analyser and STUT comparisons pass (`build/midi-learn-panic-ui.log`).

No new feature or package was introduced. Version stays 0.4.3; existing Windows
binaries were incrementally rebuilt. Original assets and the user's local logo
edits are preserved. Physical expression/listening, broader driver/DAW and Apple
platform acceptance remain separate work.

### Physical expression exposed focused-slider display failure (2026-10-21)

Jorge reported learn waiting, then successful learn with a stationary slider.
Live AudioBox USB diagnostics first observed no packets; after a numbered
footswitch press they observed real channel-1 CC27 movement. Loop 1 learned CC27
(binding 110), and native volume reached 0.015748031437397 for CC value 1 with
MASTER zero. This establishes real CC arrival/learning/parameter routing. It does
not establish calibration or audible/driver latency quality.

The focused range was excluded from snapshot rendering indefinitely. Prior GUI
checks asserted native metadata, missing the visible defect. The strengthened
focused-DOM test failed on the previous binary
(`build/midi-slider-focus-repro.log`). Learned volume/pan now follow native state
while idle even if focused; explicit pointer/range-key edit ownership protects
ongoing local edits and clears on release/cancel/lost capture/blur. Unassigned
focused controls retain their existing behavior. Original markup is unchanged.

Acceptance:

- Windows original assets/MIDI/frontend 3/3, 6.65s
  (`build/midi-slider-focus-windows.log`); Ubuntu 3/3, 8.14s
  (`build/midi-slider-focus-ubuntu.log`). Frontend cases cover focus, right-click,
  drag ownership, unrelated release, key repeat, cancellation and blur.
- Real native focused volume/pan DOM positions follow injected MIDI at 64/128;
  an actual mapped mouse drag remains locally editable while native CC moves,
  then visibly resumes hardware state on release
  (`build/midi-slider-focus-mouse.log`). Prior MIDI cases remain intact.
- Original native 20-card UI/keyboard and actual WebAudio count-in, nudge,
  analyser and STUT references pass (`build/midi-slider-focus-ui.log`).
- Core/processor/DSP sources were unchanged; their prior exact stereo PCM and
  sanitizer acceptance is retained, not claimed as rerun for this UI-only repair.

Only existing Standalone/test-host binaries were incrementally rebuilt; no
package or version bump. Original UI assets and the user's separate logo edits
are preserved. The fixed app reopened with the user's real CC27 mapping retained;
Jorge confirmed that the visible slider now follows the real pedal while focused.
The post-fix native/rendered values are recorded in
`build/midi-slider-focus-hardware.log`. This closes the physical expression
response/display gate for this AudioBox USB/CC27 setup; endpoint calibration,
audible performance and other controllers/platforms remain separate.

### MIDI mapping focus/blur and Tab repair (2026-10-21)

The native click-only mapping handler left learning armed after clicking away,
unlike unchanged KeyMapManager.handleInputFocus/handleInputBlur. The actual GUI
failure is retained (`build/midi-field-focus-repro.log`). A focused field now
learns through mouse or Tab, cancels on blur and releases focus after accepted
completion. A re-click before the next render restarts learning. Original table
markup stays intact; existing EN/ES/PT MIDI instructions describe the behavior.

Newly received replies/snapshots are fenced to the active field. Native
cancelLearn optionally scopes cancellation to a validated target; delayed blur
cannot cancel a different field/slider. Generic Escape/editor cancellation is
unchanged, including capacity-independent fallback. Unsupported drone fields
remain hidden or disabled. No audio-thread or DSP implementation changed.

Acceptance evidence:

- Final Windows original-assets/MIDI/frontend 3/3, 5.36s
  (`build/midi-field-focus-windows.log`); Ubuntu 3/3, 7.97s
  (`build/midi-field-focus-ubuntu.log`). Original focus/blur methods execute
  unchanged. Frontend tests retain stale reply/snapshot protection, completion
  blur without extra cancellation, failed-learn retry and slider handoff.
- Production JUCE MidiBuffer checks pass at 64/128
  (`build/midi-field-focus-processor.log`), including target mismatch while
  callbacks are paused, immediate matching-target cancellation, invalid scopes,
  full 31-entry queue for both scoped blur and generic Escape, no stale assignment
  after resume and fresh-learn recovery. Earlier persistence/legacy/panic/overflow
  and zero processBlock new/delete checks remain.
- Actual MIDI mouse/Tab/blur workflows at 64/128 are retained in
  `build/midi-field-focus-mouse.log`: leaving the field prevents a later PC from
  learning, Tab arms the next field, accepted learn releases focus, rapid re-click
  restarts, reload/save works and previous focused expression/drag cases remain.
- Windows unchanged-JS DSP/state/sync 3/3, 22.24s
  (`build/midi-field-focus-parity.log`). Connected original/native capture,
  overdub and substitute audit passes 126 cases, 965826 samples, maximum error
  7.394757495227111e-8 at unchanged 1e-6 (`build/midi-field-focus-audio.log`).
  Browser envelopes/resampling, sync and input latency compensation are outside
  that committed-PCM audit. Prior sanitizer evidence is retained, not rerun.

- Final original 20-card UI/keyboard smoke and actual WebAudio count-in, nudge,
  analyser and STUT comparisons pass (`build/midi-field-focus-ui.log`).

Only existing Windows Standalone/test targets were rebuilt, with no installer,
ZIP, new format package or version bump. Original assets and local logo edits
are preserved. The physical CC27 response/display confirmation remains valid for
its tested setup; this change does not establish other hardware/platform parity.

### Original lost-NoteOff recovery (2026-10-21)

The retained core failure (`build/midi-note-recovery-repro.log`) showed native
loop NoteOn remained debounced forever after a lost release. Unchanged original
App.handleInputPress discards a stale press only when a new press arrives more
than ten seconds later. The native loop-note path now follows that rule using
sample time. Exactly ten seconds remains debounced. Ignored repeats do not reset
the age; long holds are never automatically finished. CC positive-edge/rearm,
PC bursts and global mode-note handling are unchanged. No alternative behavior
or new interface was introduced.

Acceptance:

- Windows focused core/MIDI/frontend/assets and unchanged-JS DSP/state/sync:
  7/7, 62.44s (`build/midi-note-recovery-windows.log`).
- Ubuntu focused checks: 4/4, 13.53s (`build/midi-note-recovery-ubuntu.log`);
  ASan/UBSan with leak detection: 4/4, 27.05s
  (`build/midi-note-recovery-asan.log`). Zero callback new/delete remains enforced.
- The actual original MIDI/press/release methods run unchanged for before,
  exactly-at and strictly-after-ten-second cases. Native core tests retain
  ordinary tap recovery, uninterrupted long recording, and CC still requiring
  zero even after a long positive hold.
- Nonzero stereo capture at samples 17..80017, finished by a recovered NoteOn
  at 80018, yields byte-identical WAV to direct sample-scheduled commands at
  both 64/128 blocks. Existing normal/held/expression PCM comparisons remain.
- Production juce::MidiBuffer verifies the repeated events at offsets 17/18
  in the callback beginning at sample 80000: only the latter recovers and ends
  an 80001-frame take. MIDI passthrough and zero processBlock new/delete pass
  (`build/midi-note-recovery-processor.log`).

- Connected original/native capture, overdub and substitute audit: 126 cases,
  965826 samples, maximum absolute error 7.394757495227111e-8 against the unchanged
  1e-6 tolerance (`build/midi-note-recovery-audio.log`).
- Full production editor/processor MIDI pointer/key workflows pass at 64/128
  (`build/midi-note-recovery-mouse-final.log`), including the actual long-hold /
  lost-release case and earlier learn, focused-slider, panic, reload/save cases.
  The expanded test exposed an intermittent background click after page reload:
  captured pointer events had no control ID and no native save request was sent
  (`build/midi-save-trace.log`). DOM readiness alone did not establish settled
  scroll/layout. The fixture now requires a fresh initialized document and stable,
  hit-tested pointer coordinates before dispatching real pointer events. Save and
  cancellation assertions are unchanged; no product save behavior was altered.

- Existing Standalone original 20-card UI/keyboard/IPC smoke and real WebAudio
  count-in, nudge, live analyser and STUT comparisons pass
  (`build/midi-note-recovery-ui.log`). No product AudioContext is created.

Only existing Windows app/test targets were incrementally rebuilt; version stays
0.4.3, without a new package. Original assets and the user's logo edits are kept.
The previously confirmed physical PC0 and CC27 checks remain valid for their
specific setup; other hardware/driver/platform and listening acceptance remain
separate work.

### Original repeated global MIDI mode notes (2026-10-21)

Unchanged App.handleMIDIMessage dispatches OD/SUB/SUS directly on every positive
NoteOn, bypassing the loop handleInputPress map. Native code incorrectly retained
these notes as held presses and ignored later NoteOns until release. The retained
before-fix run fails both core and original-JS comparison
(`build/midi-mode-repro.log`). Native global NoteOns now remain pulses, with no
held slot; NoteOff and velocity-zero never toggle. Loop-note debounce/600ms cut/
4s delete/>10s lost-release recovery are unchanged. Native global CC mappings
still require zero rearm; global PC mappings retain the 20ms burst window.
This restores original behavior rather than introducing an alternative.

Acceptance:

- Windows core/MIDI/frontend/original assets plus unchanged-JS DSP/sync/state:
  7/7, 58.82s (`build/midi-mode-windows-final.log`). The original handler AND all
  three original toggle methods execute unchanged across 16 channels, repeated
  positives without release, NoteOff/velocity-zero and OD/SUB mutual exclusion.
  VM arrays are copied into the test realm before strict value comparison;
  tolerances and expected values are unchanged.
- Ubuntu 4/4, 13.12s (`build/midi-mode-ubuntu-final.log`); ASan/UBSan with leak
  detection 4/4, 27.56s (`build/midi-mode-asan.log`). Core checks retain ordinary
  loop debounce and native CC zero rearm / PC burst behavior at 64/128.
- Actual production juce::MidiBuffer/no-editor tests pass at 64/128, with offsets
  17/18/19/20, host MIDI passthrough, eighty same-block mode pulses, no overflow
  and zero processBlock new/delete (`build/midi-mode-processor.log`).
- Production processor/editor GUI MIDI checks pass at 64/128
  (`build/midi-mode-mouse.log`), including the original visible OD/SUB/SUS
  indicators after each repeated NoteOn and all prior learning, slider drag,
  PC burst, lost-release, panic, timeout, cancellation and reload/save cases.
- Original 20-card Standalone UI/keyboard/IPC smoke and actual WebAudio count-in,
  nudge, analyser and STUT comparisons pass (`build/midi-mode-ui.log`).
- Connected unchanged-JS/native capture, overdub and substitute PCM audit passes
  126 cases / 965826 samples, maximum absolute error 7.394757495227111e-8 at the
  unchanged 1e-6 tolerance (`build/midi-mode-audio.log`).

No original markup or user logo edit was changed. Only existing Standalone/test
targets were rebuilt. The requested distribution is one Standalone-only Windows
ZIP, generated from the already-tested executable by tools/PackageStandalone.ps1,
with current docs, dependency notices and source/local-override/executable-hash
manifest. Version stays 0.4.3. No plugin/installer package or production-ready
claim follows. Physical drivers, listening, other platforms and DAW hosts remain
separate acceptance work.

### Held loop note reassigned to OD/SUB/SUS (2026-10-21)

The core regression first failed at "Original global NoteOn bypasses a cancelled
former loop press" (`build/midi-reassigned-mode-repro.log`). Suppressed old loop
presses intentionally remain until release after a transport-binding change, but
native code consulted that entry before resolving a key's new global assignment.
The original handler directly dispatches modes and never consults the loop press
map for them. Native routing now resolves current bindings first and bypasses
old entries for global NoteOns. Ordinary loop debounce/recovery and global CC/PC
edge/burst handling remain unchanged. The old suppressed release still cannot
finish a former recording. No alternative behavior or new interface was added.

Acceptance:

- Windows focused core/MIDI/frontend/assets and unchanged-JS DSP/sync/state:
  7/7, 61.77s (`build/midi-reassigned-mode-windows.log`). Unchanged original MIDI,
  press/release and mode methods compare reassignment to each of OD/SUB/SUS.
- Ubuntu 4/4, 14.06s (`build/midi-reassigned-mode-ubuntu.log`); ASan/UBSan with
  leak detection 4/4, 29.15s (`build/midi-reassigned-mode-asan.log`). Earlier
  Note/CC/PC learning, focused sliders, hold/delete and lost-release gates remain.
- Nonzero distinct stereo capture from sample 17 through 899 (883 frames) stays
  byte-identical to direct capture at 64/128 when a held loop key becomes OD,
  SUB or SUS, receives repeated NoteOns and is finally released. This verifies
  preserved audio, in addition to mode metadata. Zero callback new/delete passes.
- Production juce::MidiBuffer tests pass at 64/128
  (`build/midi-reassigned-mode-processor.log`): record with held Note 36, clear its
  binding, learn it as global OD, then repeat/release at offsets 19/20/21. New
  modes work immediately while the former recording continues. All earlier
  persistence/overflow/learn and host MIDI passthrough/allocation checks remain.
- Connected original/native capture/overdub/substitute audit passes 126 cases,
  965826 samples, maximum error 7.394757495227111e-8 at unchanged 1e-6 tolerance
  (`build/midi-reassigned-mode-audio.log`). Full browser envelopes/resampling,
  physical latency and external sync remain outside that committed-PCM scope.

- Full production MIDI mouse/key/reload/save/slider gates pass at 64/128
  (`build/midi-reassigned-mode-mouse.log`). The added case uses actual pointer
  clear/learn gestures while Note 36 is held, verifies its new global OD action,
  repeats/releases without ending capture, then restores original mappings.
- Existing Standalone original 20-card UI/keyboard/IPC smoke and actual WebAudio
  count-in, nudge, analyser and STUT gates pass
  (`build/midi-reassigned-mode-ui.log`). No product AudioContext is created.

Only existing Windows Standalone/test targets were incrementally rebuilt. No
new package, installer or version bump. Original markup and local logo edits
remain intact. No broader hardware/DAW/platform or production-readiness claim.

### Architecture and complete portable baseline audit (2026-10-21)

`MIGRATION_BLUEPRINT.md` inventories actual owners and current CMake targets,
separately from planned synth/tracker/FX/transport/project modules. Original
reference/assets and all audio/MIDI/control sources are unchanged by this audit.
README's stale claim that all export and MIDI learn are pending is corrected:
raw WAV and loop/volume/pan MIDI learn exist; broader paths remain incomplete.
CMake now installs the audited blueprint alongside architecture/build docs.

- Existing Windows JUCE configure/generation succeeds with the cached pinned
  dependencies and Standalone/VST3/CLAP/ASIO enabled
  (`build/architecture-audit-cmake.log`); generated cmake_install.cmake includes
  MIGRATION_BLUEPRINT.md. Preset listing succeeds. No SDK or wrapper upgrade.
- All portable/reference targets incrementally rebuilt; full MSVC suite 18/18,
  256.80s (`build/architecture-audit-windows.log`).
- All portable/reference targets incrementally rebuilt; full Ubuntu suite 18/18,
  446.04s (`build/architecture-audit-ubuntu.log`).
- Original asset identity/tamper rejection and all retained MIDI, core realtime,
  frontend, WAV/merge, waveform/STUT/Start/count-in/metronome, DSP/sync, multiply,
  history/reverse/NORM/state comparisons pass. No tolerance or case was removed.

Prior native processor, actual WebView2 GUI/browser-audio and sanitizer results
for e66a86c are retained, not rerun for documentation/install-list changes.
No new package, installer, DSP feature or interface redesign. The user's local
logo override remains untouched/unstaged. Available CMake targets are not proof
of current plugin host, physical driver, Apple device or commercial acceptance;
full feature parity and arbitrary C malloc/host allocation certification remain
unestablished.

### IPC document correlation across reload (2026-10-21)

A failing-then-fixed VM lifecycle regression reproduces numeric request ID reuse:
close an old document with request 1 pending, open a new bridge with request 1,
then deliver the obsolete response to the new document. Previously it resolved
the new promise and removed its deadline (`build/bridge-session-repro.log`).
The bridge now attaches a per-document `_aionSession`; native message-thread
success/error replies echo a validated optional token. The frontend requires a
matching token AND pending ID, then strips the private field before resolving.
Direct legacy native clients without a token retain their response shape.
No audio/core/HTML/CSS/control behavior changes; no interface redesign.

Acceptance:

- Windows focused original-assets/core/MIDI/frontend/bridge and unchanged-JS
  DSP/sync/state 8/8, 61.23s (`build/bridge-session-windows.log`).
- Ubuntu original-assets/core/MIDI/frontend/bridge 5/5, 13.33s
  (`build/bridge-session-ubuntu.log`). Expanded Windows and Ubuntu unit runs
  additionally cover missing/malformed envelopes, frozen caller payloads,
  caller-token override protection, duplicate/unknown IDs, reordered success/
  error, unchanged 128-pending/5s deadline bounds, timeout-slot recovery, pagehide
  rejection and crypto UUID/getRandomValues/older-environment fallback
  (`build/bridge-session-unit.log`, `build/bridge-session-ubuntu-unit.log`).
- Production processor/no-editor checks pass at 64/128
  (`build/bridge-session-processor.log`): native success/error echo, invalid and
  oversized tokens, legacy direct clients and all prior MIDI persistence/
  cancellation/passthrough/zero processBlock new/delete checks remain.
- Actual production WebView2/editor MIDI workflows pass at 64/128
  (`build/bridge-session-mouse.log`). At each reload a harmless hello is held
  locally; the test delivers an old-token envelope with its current numeric ID
  through JUCE's real inbound dispatcher, asserts it stays pending, then forwards
  the held request to C++ and accepts only the genuine current response. This
  injection exists in CDP test code, not a product RPC or hardware-MIDI claim.
- Existing Standalone 20-card UI/keyboard/IPC and actual browser count-in, nudge,
  analyser and STUT audio gates pass (`build/bridge-session-ui.log`).
- Production main/export/merge pointer workflows pass at 64/128
  (`build/bridge-session-main-export-merge.log`): arm/cancel/record/play/stop,
  actual PCM, STATIC/DYNAMIC sync, rapid and held-button cases, Start/Q/nudge/
  STUT, WAV byte/cancel/replacement/overwrite/DEL guards and merge/confirmation/
  Undo/Redo/source ownership. `-SkipMidi` avoids repeating the already-passed
  focused MIDI run; default coverage is unchanged. Contradictory MidiOnly /
  SkipMidi is rejected. No timeout, hold boundary or audio tolerance was loosened.
- Connected original/native capture/overdub/substitute PCM audit: 126 cases,
  965826 samples, maximum absolute error 7.394757495227111e-8 at unchanged 1e-6
  (`build/bridge-session-audio.log`). The explicit unit watchdog prevents a
  never-resolved mocked promise from silently exiting the test successfully.

The token correlates replies only. It does not authenticate callers or cancel
already accepted native/audio commands; existing generation/expiry, native
navigation and picker lifetime/sequence guards remain required. No APVTS state
version change, arbitrary C malloc certification or new hardware/platform claim.
Previous sanitizer results are retained, not rerun for this message-thread and
frontend-only change. Existing app/test targets only; no package/version bump.
The user's local logo edits remain untouched/unstaged. The original keyboard
`*` SUS shortcut/custom keyboard mapping gap found while choosing the next
increment is explicitly recorded in PARITY, not represented as implemented.

### Bridge synchronous-send failure recovery (2026-10-22)

The retained regression (`build/bridge-send-failure-repro.log`) showed a failed
native send rejected its promise but left a timer/pending entry until five seconds
later. Enough failures therefore exhausted the 128-request budget. The bridge
now removes that ID and timer immediately when payload serialization or transport
throws, then rejects with the original error. Successful request/reply shape,
session correlation, deadline, queue bound, pagehide and native commands are
unchanged. No HTML/CSS, audio/C++ DSP, APVTS or device behavior change.

Acceptance:

- Expanded BridgeTests exercises transport failure, circular JSON and a throwing
  payload getter, then more than 256 immediate failures and recovery using all
  128 pending slots without waiting for a timeout. Earlier stale-session/ID,
  malformed-envelope, crypto fallback, queue/deadline and close checks remain.
  A real watchdog still prevents an unresolved mock promise from silently exiting.
- Windows focused original-assets/core/MIDI/frontend/bridge and unchanged-JS
  DSP/sync/state 8/8, 61.23s (`build/bridge-send-failure-windows.log`);
  Ubuntu focused checks 5/5, 14.09s (`build/bridge-send-failure-ubuntu.log`).
- Actual production WebView2 MIDI/reload workflows pass at 64/128
  (`build/bridge-send-failure-midi.log`). Each reload injects 256 synchronous
  send failures for a harmless hello at the real JUCE outbound boundary, verifies
  the original error each time, restores transport and accepts a genuine native
  hello immediately. The prior obsolete-completion/reused-ID and all MIDI cases
  continue afterward. Injection exists only in CDP tests, not a product RPC.
- Production processor/no-editor checks retain native echo/validation, original
  MIDI persistence/panic/held gestures and zero processBlock new/delete at 64/128
  (`build/bridge-send-failure-processor.log`).
- Existing Standalone 20-card UI/keyboard/IPC and real browser count-in, nudge,
  analyser and STUT audio checks pass (`build/bridge-send-failure-ui.log`).
- Connected unchanged-JS/native capture/overdub/substitute PCM audit passes
  126 cases / 965826 samples, maximum error 7.394757495227111e-8 at unchanged
  1e-6 (`build/bridge-send-failure-audio.log`). Browser playback envelopes,
  resampling, physical latency and external sync remain outside that scope.

Previous full main/export/merge 64/128 and sanitizer evidence is retained, not
rerun for this failed-send cleanup. No threshold/case was relaxed. Only existing
Windows Standalone/test targets rebuilt; no new package or version bump. Original
assets and local logo edits are preserved. No next feature was enabled and no
broader device/host/platform or production-ready claim is made.


## 22 October 2026: native navigation releases held STUT

The new production-editor regression first failed at 64 samples: after a real
pointer STUT hold, lost blur/pagehide release RPCs and navigation to `about:blank`,
the actual audio callback still reported `stuttering:true`, `playing:true` and
nonzero output. See `build/native-navigation-repro.log`. The test does not consult
browser snapshots after leaving the editor or rely on the next page releasing it.

`Browser::pageAboutToLoad` now invokes the existing queue-independent native STUT
release alongside export/MIDI cleanup. The looper adapter skips a document without
the original input mixer, so native user-script injection into an allowed empty
page cannot dereference missing markup. The original interface, stored PCM,
transport and JS feature semantics are unchanged.

The hardware-free host records bounded audio-owned state/clip identity and actual
output-buffer peaks through an SPSC queue only while this regression requests it.
JSON/file work remains on the message thread; the fixture is excluded from normal
builds and is never packaged. Acceptance requires a newer audio block with STUT
released, continuing playing/nonzero output, unchanged clip identity/length, and
restoration of all 20 original cards. Runtime exception collection spans the empty
document and restored editor without clearing errors. Existing ordinary
release/outside/blur/reload/deferred-start cases remain.


Fresh verification for this change:

- Windows portable suite: 19/19, 265.57s (`build/native-navigation-windows.log`).
- Ubuntu portable suite: 19/19, 447.45s (`build/native-navigation-ubuntu.log`).
- Production processor/MidiBuffer, persistence/cancellation and zero tracked
  processBlock new/delete at 64/128 (`build/native-navigation-processor.log`).
- Full actual WebView2 mouse/main, new lost-release navigation, export, merge and
  MIDI workflows at 64/128 (`build/native-navigation-gui.log`). No device is
  opened by the synthetic host; no physical MIDI observation is inferred.
- Muted Standalone native UI/keyboard/IPC checks and actual browser count-in,
  nudge, analyser and STUT sound oracles (`build/native-navigation-ui.log`). STUT
  compares 12 cases / 86016 samples, maximum error 1.4901161193847656e-8 at the
  unchanged 1e-6 tolerance; playback envelopes/resampling remain outside its scope.
- Connected original-JS/native capture/overdub/substitute PCM: 126 cases / 965826
  samples, maximum error 7.394757495227111e-8 at unchanged 1e-6
  (`build/native-navigation-audio.log`). This is committed-PCM acceptance, not
  physical latency or complete browser audio parity.

Only existing Windows Standalone/test targets rebuilt. No ZIP, version bump,
format rollout or new feature. Original assets and user-local logo edits remain
preserved; no DSP core source changed. Prior sanitizer evidence is retained rather
than rerun. Apple/Linux GUI, physical drivers and host sync remain separate work.


## 22 October 2026: restore original X / DEL ALL shortcut

The unchanged `App.handleKeyboardPress` oracle reproduced a missing native
feature: an ordinary `KeyX` invokes DEL ALL in JS but performed no action in the
native adapter (`build/clear-shortcut-repro.log`). `AionPerformance` now routes
that fallback to the same native handler as the original DEL ALL button. Physical
key, Shift/case, text/control fields, Ctrl/Alt/Meta and original repeat behavior
are compared against the unmodified method across 72 combinations. The existing
native content-editable guard remains. Custom keyboard remapping, master recording
and full project persistence are not implemented by this increment.

The shared handler retains the exact original confirmation. Cancel sends no audio
clear command; accepted confirmation cancels held STUT/loop gestures and older
frontend work before using the existing C++ bank clear. No new audio algorithm,
queue producer, callback storage, browser audio context or product UI was added.
Research and primary sources are recorded in ARCHITECTURE.md; the original JS,
not another application's behavior, remains the interaction oracle.

Fresh checks:

- Windows original-assets/core/MIDI/frontend/bridge/DSP/sync/state 8/8, 61.64s
  (`build/clear-shortcut-windows.log`).
- Ubuntu same focused suite 8/8, 65.81s (`build/clear-shortcut-ubuntu.log`).
- Production processor MidiBuffer/cancellation/persistence and zero tracked
  processBlock new/delete at 64/128 (`build/clear-shortcut-processor.log`).
- Connected original-JS/native committed capture/overdub/substitute PCM audit:
  126 cases / 965826 samples, maximum absolute error 7.394757495227111e-8 at
  unchanged 1e-6 (`build/clear-shortcut-audio.log`). Browser envelopes/resampling,
  physical latency and external sync remain outside this comparison.

- Full production-editor mouse/main, export and merge checks pass at 64/128
  (`build/clear-shortcut-gui.log`). The new case drives actual KeyX and original
  DEL ALL pointer input through real WebView2 confirmation dialogs, with exact
  prompt text and both accept/cancel responses. It records two native loops,
  checks preserved identity/length/playing PCM on cancel, then holds STUT and
  pauses dispatch behind held loop keys before confirming. Only one clear command
  survives; every track becomes empty, actual output is zero, and no old gesture
  fires after four seconds. Text-focus protection and the shared button path pass.
  Existing navigation, hold, source, export and merge gates remain unchanged.

- Muted Standalone original 20-card UI/keyboard/IPC and actual browser count-in,
  nudge, analyser and STUT sound oracles pass (`build/clear-shortcut-ui.log`).
  No product AudioContext is present.

Only existing Standalone/test targets rebuilt; no package/version bump. Original
assets and the user's unstaged logo hunk are preserved. No new C++ DSP or RT source
changed. The preceding 19/19 Windows/Ubuntu full suites and sanitizer/MIDI GUI
results remain retained baseline evidence rather than fresh reruns here. No new
hardware, DAW, Apple-platform or production-ready claim follows from these checks.


## 22 October 2026: coherent native host transport observation

Baseline `40da70f` read BPM/PPQ/play only and published none of that host input.
The new fake-head processor test first failed with “Host transport snapshot must
be published” (`build/host-transport-repro.log`). The native processor now reads
JUCE PositionInfo once per callback, validates its optional fields and transfers
one block-stamped POD observation through the existing bounded EngineView SPSC.
IPC exposes available play/record/loop flags, tempo, PPQ, signed sample/seconds
position, meter, last-bar and cycle boundaries; unavailable optional fields are
null. This does not implement host-follow scheduling or MIDI clock. Original
UI/assets, internal sync controls and audio algorithms are unchanged.

Portable tests cover nonfinite/nonpositive values, meter/range validation, signed
preroll, absent host reset/prepare and snapshot saturation/recovery at 64/128.
Seven observations fit the original queue; dropped telemetry cannot mix a host
position with another audio block. Storage remains construction-time heap owned.
Native fake-head tests exercise real AudioPlayHead/PositionInfo, missing/empty
positions, negative/fractional timelines, malformed fields, changing meter,
backward seek/cycle wrap, host stop/detach and zero-sample callbacks. State save
and IPC never call the playhead. The extended production allocation watch remains
active around processing. Paired host/no-host processors compare nonzero monitored
stereo PCM bit-for-bit while host tempo/meter/seek/play/cycle state changes.

Implementation research and primary JUCE, VST3 and CLAP sources are recorded in
ARCHITECTURE.md. JUCE's optional-field/callback-start model is the boundary; this
increment does not assume all wrappers/hosts supply identical information or
handle sample-offset tempo ramps. No actual DAW or hardware evidence is inferred
from the fake host. Arbitrary C malloc/driver internals remain outside new/delete
instrumentation.

Fresh checks completed:

- Windows full portable/reference suite: 19/19, 260.02s
  (`build/host-transport-windows.log`).
- Production host/PCM and existing MIDI, cancellation, persistence, IPC and zero
  processBlock new/delete at 64/128 (`build/host-transport-processor.log`).
- Connected original-JS/native committed capture/overdub/substitute PCM audit:
  126 cases / 965826 samples, maximum absolute error 7.394757495227111e-8 at
  unchanged 1e-6 (`build/host-transport-audio.log`). Browser envelopes/resampling,
  physical latency and external sync remain outside this scope.

- Ubuntu ASan/UBSan focused core/frontend/bridge/STUT/sync: 5/5, 171.22s
  (`build/host-transport-asan.log`, leak detection enabled).

- Ubuntu full portable/reference suite: 19/19, 447.33s
  (`build/host-transport-ubuntu.log`).
- Full actual WebView2 mouse/main, WAV export, merge and MIDI workflows at 64/128
  (`build/host-transport-gui.log`), retaining X/DEL ALL, held-STUT navigation,
  mapped-slider, stale-session and original pointer/key/PCM regressions.
- Muted Standalone original UI/keyboard/IPC and actual browser count-in, nudge,
  analyser and STUT audio oracles (`build/host-transport-ui.log`). Real IPC checks
  musical fields are null while JUCE Standalone's supplied device sample/seconds
  counters remain numeric and consistent. Its `available:true` means partial
  PositionInfo, not DAW tempo or playing state. Missing-playhead behavior remains
  independently tested in the fake-head processor. No product AudioContext exists.

Only existing Windows Standalone/test targets rebuilt; no new package or version
bump. The user-local logo changes remain unstaged and original runtime assets are
unchanged. These gates establish the observation boundary and retained looper
behavior, not actual REAPER/Ardour sync, wrapper/platform certification, physical
ASIO latency or a completed migration. No next scheduling feature was enabled.


## 22 October 2026: original keyboard missed-release recovery

An unchanged App.handleKeyboardPress/handleInputPress oracle first reproduced the
native duplicate guard ignoring a fresh key at 10001ms (`build/keyboard-recovery-repro.log`).
A missed key-up could therefore strand a take indefinitely. The native adapter
now keeps <=10000ms debounced, then cancels that key's old timer/queued cut-delete
ownership before executing a fresh current-state main action. Ignored repeats,
modifiers and text focus do not recover or refresh age; elapsed time alone never
finishes an intentional held recording. Existing physical key identity, global
shortcut routing, per-track cancellation and four-second STOP deletion remain.

The oracle exercises every digit/Shift+Digit loop path and 9999/10000/10001ms,
new-press age, repeat/modifier/text guards. Global mappings are unmapped in that
fixture to isolate all 20 original loop paths; this is not default keyboard SUS
or custom-map acceptance. Native-only ownership checks retain monotonic origin
zero, stale deferred callbacks and queued-cut predicates, and an unrelated held
key remaining valid. The original source is unchanged. Research/clock choices are
recorded in ARCHITECTURE.md; the existing monotonic hold clock is reused.

Fresh checks:

- Windows focused original-assets/core/MIDI/frontend/bridge/DSP/sync/state 8/8,
  60.86s (`build/keyboard-recovery-windows.log`).
- Ubuntu same focused suite 8/8, 66.17s (`build/keyboard-recovery-ubuntu.log`).
- Extended final frontend ownership oracle rerun directly
  (`build/keyboard-recovery-frontend.log`).
- Production processor host/PCM and existing MIDI/persistence/cancellation/IPC,
  with zero tracked processBlock new/delete at 64/128
  (`build/keyboard-recovery-processor.log`).
- Connected original-JS/native committed capture/overdub/substitute PCM audit:
  126 cases / 965826 samples, maximum absolute error 7.394757495227111e-8 at
  unchanged 1e-6 (`build/keyboard-recovery-audio.log`). Browser playback envelopes,
  resampling and physical latency remain outside this scope.

- The stronger final per-key ownership assertion also passes directly on Ubuntu
  (`build/keyboard-recovery-ubuntu-frontend.log`).
- Full real WebView2 mouse/main, WAV export and merge at 64/128
  (`build/keyboard-recovery-gui.log`). The new real-key case omits key-up, holds
  an actual RECORD beyond ten seconds without finish/cut/delete, injects an OS
  repeat (ignored), then a fresh key (one FINISH). The committed clip retains
  >=10 seconds of input and plays nonzero native PCM with unchanged identity;
  released/old timers issue no later commands. Existing holds, X/DEL ALL, STUT
  navigation, sync, source, export and merge cases remain.

- Muted Standalone original 20-card UI/keyboard/IPC, partial-playhead transport
  fields and actual browser count-in/nudge/analyser/STUT audio oracles pass
  (`build/keyboard-recovery-ui.log`). No product AudioContext exists.

Only existing Standalone/test targets rebuilt; no package or version bump.
Original assets and user-local logo edits are preserved. No C++ DSP/RT source
changed; preceding full 19/19 Windows/Ubuntu, sanitizer 5/5 and MIDI GUI evidence
from the host-observation increment is retained rather than rerun for this
frontend-only recovery fix. No hardware, host-sync or production-ready claim is
made, and no next feature was enabled while this gate was under verification.


## 22 October 2026: reject malformed CC before panic cancellation

The new native core regression first reproduced “Malformed CC120/123 must not
finish a held SUS recording” (`build/midi-panic-validation-repro.log`). That panic
fast path ran before the existing seven-bit data check. Moving it after validation
rejects corrupt framed host CC values before performance/learn cancellation. No
new MIDI action, UI, binding, queue, allocation or lock is introduced. Valid
seven-bit cancellation behavior, including the existing acceptance of value 127,
is retained. This is not stricter channel-mode semantics or a wire-stream parser.
Primary MIDI Association format guidance and the boundary are in ARCHITECTURE.md.

New core tests inject values 128/255 for CC120/123 on all 16 channels at 64/128.
A SUS take and transport Learn stay active until a real NoteOff; the recorded
nonzero stereo WAV must be byte-identical to the same capture without malformed
packets. Valid panic controls on every channel still restore a held SUS mute and
preserve PCM, with releases rearming normally. Allocation instrumentation remains
active. Real raw juce::MidiBuffer tests additionally preserve a slider Learn and
capture through 64 malformed packets, then verify normal release and valid panic.
The original reference source/cases/tolerances remain unchanged.

Fresh checks completed:

- Extended production processor malformed/valid panic, MIDI, persistence,
  cancellation/IPC and host-observation PCM checks at 64/128, with zero tracked
  processBlock new/delete (`build/midi-panic-validation-processor.log`).
- Real WebView2 MIDI workflows at 64/128 (`build/midi-panic-validation-gui.log`):
  held recording and actual right-click volume Learn survive every-channel
  malformed CC; Escape still cancels learning, real NoteOff commits audio and
  the take plays nonzero PCM. Valid panic releases a held mute without changing
  length. Earlier controller, slider, reload and bridge regressions remain.
- Ubuntu ASan/UBSan core/MIDI/frontend/bridge: 4/4, 26.15s
  (`build/midi-panic-validation-asan.log`, leak detection enabled).
- Connected unchanged-JS/native committed capture/overdub/substitute PCM audit:
  126 cases / 965826 samples, maximum absolute error 7.394757495227111e-8 at
  unchanged 1e-6 (`build/midi-panic-validation-audio.log`). Browser playback
  envelopes/resampling and physical latency remain outside that scope.
- Muted Standalone original 20-card UI/keyboard/IPC/partial-host fields and
  actual browser count-in, nudge, analyser and STUT sound oracles pass
  (`build/midi-panic-validation-ui.log`); no product AudioContext exists.
- Windows full unchanged-JS/native suite: 19/19, 266.46s
  (`build/midi-panic-validation-windows.log`).

- Ubuntu full unchanged-JS/native suite: 19/19, 460.02s
  (`build/midi-panic-validation-ubuntu.log`).

Only existing test targets and the Standalone were rebuilt. No version bump or
package was created; original UI assets and the user's separate logo edit were
preserved. The MIDI GUI workflows were rerun for this fix; the full mouse
export/merge workflows retain their previous baseline. These results do not
establish physical-controller, DAW-host or all-platform acceptance, or complete
application parity. No new import, synth, clock or transport-follow feature was
enabled in this increment.


## Numeric keypad routing repair (22 October 2026)

The new unchanged-original handleKeyboardPress oracle first reproduced keypad
Numpad0/Insert incorrectly issuing Loop 10's main action
(`build/keypad-routing-repro.log`). The native combined code-based Digit/Numpad
branch also added the upper-bank Shift offset for keypad digits, unlike the
original character mapping. The fix requires a keypad digit key value and uses
it for loops 1-10; main-row Digit Shift behavior is preserved. Release identity
still follows physical code. No C++ DSP, storage, queue, MIDI or UI markup changes.

The frontend matrix retains the original default numeric and global mappings
while checking all keypad digits, navigation names, empty/Unidentified values,
Shift variants and reported-character/physical-code combinations. It also checks
repeat/modifier/text guards, physical release after NumLock/Shift changes and
that navigation is not swallowed. Original assets, dispatcher methods and prior
cases/tolerances are unchanged. Keyboard SUS/custom maps remain outside scope.

The connected original-JS/native captured/overdub/substitute PCM audit passed:
126 cases / 965826 samples, maximum absolute error 7.394757495227111e-8 at the
unchanged 1e-6 tolerance (`build/keypad-routing-audio.log`). This does not cover
browser playback resampling/envelopes or physical latency.


Fresh Windows full unchanged-JS/native suite: 19/19, 268.80s
(`build/keypad-routing-windows.log`). The new frontend matrix is included in
native_frontend_commands; all original byte-identity, DSP, state, waveform,
MIDI, sync, editing/export and bridge checks remain in the full suite.


Fresh production processor/editor WebView2 workflows passed at both 64 and 128
samples (`build/keypad-routing-gui.log`). Actual keypad events record, finish,
play nonzero native PCM and stop on loops 1/10 despite Shift. All ten navigation
keys with/without Shift issue no transport commands; NumLock-changing releases
leave no cut/delete after four seconds and preserve clip identity/length and
nonzero playback. The browser event log confirms keypad digit/Shift/navigation
values. Existing mouse/header, static/dynamic sync, four-second deletion, Start/
Q/nudge/RTRG/STUT, >10s lost-release, real export/merge/WAV bytes, MIDI learn/
expression/PC/CC, reload and IPC tests also pass. No physical device is used.
The picker fixture substitutes destination selection only; no new OS-picker
interaction or physical keyboard/driver evidence is claimed.


Fresh Ubuntu full unchanged-JS/native suite: 19/19, 453.97s
(`build/keypad-routing-ubuntu.log`). Final muted Standalone UI/keyboard/IPC and
actual browser count-in/nudge/analyser/STUT oracles passed
(`build/keypad-routing-ui.log`), retaining 20 original cards, parameter and
command validation, host-observation checks and no product AudioContext.
The portable callback allocation checks remain in both full suites. No C++ DSP
was changed, and no fresh sanitizer claim is made for this frontend-only fix.

Only the existing Standalone and synthetic host were rebuilt; no ZIP, installer
or version bump was created. The separate user logo hunk remains unstaged and
original reference assets remain byte-identical. This increment closes the
reproduced keypad routing fault only; full application parity, custom keyboard
mapping/SUS, physical OS/layout/device and DAW/platform acceptance remain pending.


## Zero-sample MIDI callback repair (22 October 2026)

The retained core regression first failed at “Explicit zero-sample MIDI Stop
still stops and preserves stored PCM” (`build/midi-zero-repro.log`). Offset zero
was rejected as overflow before dispatch in both the JUCE adapter and core. A
zero callback now processes only offset-zero MIDI at the current frame, without
rendering or advancing clocks; empty zero callbacks and ordinary pending UI loop/
gain work keep their prior behavior. Invalid offsets and event limits still
cancel through the existing bounded path. Reference algorithms/assets/UI and
MIDI assignments remain unchanged. Sources/native contract are in ARCHITECTURE.md.

New core regressions compare a held SUS take with/without zero-sample control
callbacks between 64/128 blocks. Harmless and malformed packets preserve capture
and slider Learn; NoteOff finishes at the same exact sample span and the exported
nonzero stereo WAV is byte-identical. Ordered press/release, explicit MIDI Stop,
positive invalid offsets, 257-event rejection and stationary clocks are checked.
Production juce::MidiBuffer tests additionally use both zero-channel and stereo
zero-length buffers, exercise frame-zero recording, CC zero Learn/continuous
updates, PC0 learn consumption/main dispatch, MIDI Stop, pass-through and bounded
rejection. Every callback remains under the existing new/delete instrumentation.


Fresh completed checks:

- Windows full unchanged-JS/native suite: 19/19, 255.15s
  (`build/midi-zero-windows.log`).
- Ubuntu full unchanged-JS/native suite: 19/19, 439.05s
  (`build/midi-zero-ubuntu.log`).
- Production JUCE processor zero-sample cases at 64/128, both 0/2 channels:
  recording/learn/release/PC0/Stop/pass-through/frame stability and zero tracked
  processBlock new/delete. Negative zero-block offsets and ordinary end-exclusive
  block offsets remain rejected (`build/midi-zero-processor.log`).
- Ubuntu ASan/UBSan core/MIDI/frontend/bridge: 4/4, 26.68s, leak detection enabled
  (`build/midi-zero-asan.log`).
- Connected unchanged-JS/native committed capture/overdub/substitute PCM:
  126 cases / 965826 samples, maximum absolute error 7.394757495227111e-8 at the
  unchanged 1e-6 tolerance (`build/midi-zero-audio.log`).
- Actual WebView2 MIDI/learn/expression, raw packets, PC0, malformed panic,
  reload/bridge and native sound workflows: 64/128 passed
  (`build/midi-zero-gui.log`). These ordinary GUI callbacks do not simulate
  zero-sample buffers; those are covered by the production processor test above.
- Muted Standalone original UI/keyboard/IPC/partial-host smoke and actual browser
  count-in/nudge/analyser/STUT oracles passed (`build/midi-zero-ui.log`), with
  20 original cards and no product AudioContext.

The existing Windows VST3 and CLAP targets also compile against the changed shared
processor (`build/midi-zero-plugin-build.log`); no plugins were installed. The
pinned JUCE VST3 wrapper forwards zero-sample buffers with nonzero bus counts,
but skips processAudio when sample and input/output bus counts are all zero
(parameter-only flush). This source inspection is not actual DAW or format-load
validation, and the zero-channel direct processor fixture is a robustness check.

Only existing Standalone/plugin/test targets were rebuilt; no version bump or package.
The user logo edit is preserved separately. Full mouse/export/merge workflows
retain the previous keypad-fix baseline; the MIDI workflows and Standalone smoke
were freshly rerun. Original full browser playback, physical hardware, DAW/wrapper
and other-platform acceptance remain separate. No MIDI clock, host-follow,
keyboard SUS, synth or new import feature was enabled in this repair.


## Actual Windows VST3 host-interface gate (22 October 2026)

Source baseline: 46e9c78, with the user's existing local logo hunk retained. No
product C++/DSP/UI/state or SDK pin changed for this increment. The built VST3
module tested has SHA256
5B16EF91E131305678E0CC76FBBCC41975E426A7CB179DCEAFA2A253F3DC3778.
This is actual dynamic VST3 scan/load/process/editor/state validation through
Tracktion pluginval, rather than a direct AionProcessor fixture or a compile-only
claim. Tool provenance and pinned hashes are in SDKS.md.

The official tool passed level 5, then level 9 with seed 1337:
build/pluginval/initial and build/pluginval/strict9-initial retain vendor logs and
stdout/stderr. One plugin was found, VST3-Aion-Looper-e8f091b8-93ca4d0c, v0.4.3.
The matrix covers 44.1/48/96 kHz and blocks 64/128/256/512/1024. Audio processing,
32-sample automation subdivisions, cold/warm load, editor/open-whilst-processing,
editor automation, bus/layout operations, state restoration, non-releasing
prepare/process, background state, parameter thread safety and fuzz tests pass.
This exercises the default plugin state; no recorded loops or learned-controller
session was loaded, and actual HTML rendering/content is covered separately by
the existing native UI tests, not by a successful editor-open check alone.

The repeatable tools/TestVst3Pluginval.ps1 harness verifies pinned tool bytes,
requires exact completed-test and matrix markers, retains GUI tests, and rejects
a binary modified during the run. Its per-run result.json records binary/tool/
harness hashes, source revision/dirty tracked paths, command, seed, matrix,
coverage gaps, exit code and timing. Direct execution with a space-containing
output path passed in 20.33s. The opt-in CMake/CTest entry passed 1/1 in 20.38s
(build/vst3-validator-ctest.log). CMake configuration/generation passed with the
existing local dependencies (build/vst3-validator-configure.log).

The setup/inherited-options check also passed in 20.08s. It explicitly requested
pinned tool extraction, inherited SKIP_GUI_TESTS=1, SAMPLE_RATES=8000,
BLOCK_SIZES=1 and RANDOMISE=1, then confirmed all GUI/default matrix markers,
zero missing tests and restoration of the caller environment. Evidence is in
build/vst3-validator-inherited.log and build/pluginval inherited options/*/result.json.
The harness provides no test-disable or GUI-skip option; CTest is serial and
opt-in, while default portable builds remain independent of pluginval.

Important tool limits: the pinned official CMake source does not include
Source/tests/ExtremeTests.cpp. Therefore allocationTestExecuted=false, even
though level 9 succeeds. The external Steinberg validator was not configured
and its skip is retained: externalSteinbergValidatorExecuted=false. These passes
are not arbitrary malloc/new certification, Steinberg certification, MIDI/clock
or recorded-project parity, actual REAPER/Ardour/device/ASIO testing, Apple/Linux
GUI or CLAP/LV2/AU/AUv3 validation, or commercial readiness. Separate native core/
processor allocation and original-JS/audio gates remain required.


The portable Windows build regenerated without requiring pluginval and passed
19/19 unchanged-JS/native tests in 249.89s (build/vst3-gate-windows.log).
The connected capture/overdub/substitute audit passed 126 cases / 965826 samples,
maximum absolute error 7.394757495227111e-8 at the unchanged 1e-6 tolerance
(build/vst3-gate-audio.log). It retains the documented exclusions for browser
playback envelopes/resampling, latency compensation and sync. Existing native
64/128 GUI/MIDI/zero-block and sanitizer evidence remains at the unchanged
46e9c78 product baseline; it is not claimed freshly rerun for a CMake/test-harness
addition. No app/plugin binary, version or package was rebuilt for this increment.


Ubuntu's portable build also regenerated without a validator dependency and
passed 19/19 in 470.12s (build/vst3-gate-ubuntu.log). Final execution of the exact-
marker harness through the documented CTest command passed 1/1 in 20.21s
(build/vst3-validator-ctest-final.log), with no missing required tests and matching
before/after module hashes. The core regression counts remain 19 on both systems;
only the explicitly enabled Windows native build gains the separate wrapper test.
All application/migration scope limits above remain in effect.


## Windows CLAP state-cache repair and format gate (22 October 2026)

Baseline: 134b0a4, with the user's pre-existing 4-addition/2-deletion logo hunk
preserved in resources/native/looper.js. No UI/DSP behavior, parameter encoding,
dependency version, app version or package was changed. Existing Windows
Standalone/VST3/CLAP and synthetic mouse host were incrementally rebuilt.

The official free-audio/clap-validator 0.4.1 Windows release was downloaded into
the ignored project-local toolchain, with the archive checked against GitHub's
published SHA256 and executable pinned (SDKS.md). The first actual CLAP module
run failed state-reproducibility-basic, -binary and -buffered: restored APVTS
values changed without a host rescan. See build/clap-validator/initial/stdout.json
and stderr.log. A direct AudioProcessorListener regression independently failed
before the repair (build/clap-state-repro.log).

After accepted MIDI configuration and APVTS restore, AionProcessor now calls
updateHostDisplay with parameterInfoChanged, outside its configuration mutex.
The pinned CLAP wrapper maps that flag to VALUES/TEXT/INFO rescan. It does not
mark non-parameter state dirty. Malformed/empty/queue-rejected restores do not
notify. Direct processor tests check restored gain, exact state bytes, malformed
rollback, bounded full-queue rejection and unchanged current gain on rejection.
The complete processor MIDI/host/zero-block suite passed at 64/128 with zero
tracked callback new/delete (build/clap-state-processor.log). This instrumentation
is not arbitrary malloc or format-wrapper allocation certification.

Complete fixed vendor run: 44 tests, 33 success, 10 skipped, one warning, exit 0
(build/clap-validator/fixed/stdout.json and stderr.log). All three original
state failures now succeed; no test was disabled or threshold changed. Skips:
Unix RTLD_NOW, float64 processing, CLAP-native wildcard notes, preset discovery,
and optional audio-port activation/configuration extensions. The retained
state-invalid-random warning reflects the wrapper's documented assumption of
success because JUCE setStateInformation returns void; the actual processor
rejects those bytes without mutation. This is not an all-tests-success claim.

The new TestClapValidator.ps1 verifies pinned tool bytes, all 44 exact test IDs,
module paths/plugin ID, approved unsupported-test skips and the exact known
warning, plus before/after module hashes. It refuses ancestor TOML test filters;
a temporary configuration disabling state-reproducibility-basic was rejected.
A direct run with a space-containing output path and explicit local setup passed
(build/clap validator script/20261022T193205352Z-68027a4e/result.json).
Opt-in CMake/CTest clap_validator passed 1/1 in 7.30s
(build/clap-state-ctest.log; build/clap-validator/20261022T193331421Z-69af0c12/result.json).
The normal portable build has no validator/download dependency. Rebuilt VST3
also retained its pluginval level-9 pass, 1/1 in 20.18s
(build/clap-state-vst3.log), including editor, automation and state/thread checks.

Validated CLAP module SHA256:
718E54F02AC185A57E6C605E30652343D93DEF82BEB27486598BC4BF7FC061D4
Validated VST3 module SHA256:
96BD757AE05AA87582AD18EADA95688299FF8A1F4BBC2D2FCE11AC97ACEAA2B3
The binaries include the preserved local logo; that user edit is not committed.

Fresh Windows portable unchanged-JS/core suite: 19/19, 252.84s
(build/clap-state-windows.log). Connected capture/OD/SUB PCM audit: 126 cases,
965826 samples, max error 7.394757495227111e-8 at unchanged 1e-6 tolerance
(build/clap-state-audio.log), with its existing playback-envelope/resampling/
latency/sync exclusions. No core DSP changed and no fresh sanitizer run is
claimed. Actual DAWs/physical drivers/controllers, CLAP GUI validation, recorded
project recall, other OS/format wrappers, complete feature parity and commercial
readiness remain separate acceptance work. The vendor CLAP suite is hardware-
free and does not replace original UI/connected-PCM tests.


Fresh Ubuntu portable suite completed 19/19 in 471.25s
(build/clap-state-ubuntu.log). Full production processor/editor synthetic workflows
passed at both 64/128 samples: mouse/header arm/cancel/record/finish/play/stop,
static/dynamic sync, long-hold deletion/cancellation, Start/Q/nudge/RTRG/STUT,
keyboard/keypad/lost-release, waveform, native WAV export/late-picker safety,
MERGE/confirmation/history and raw MIDI learn/PC/CC/SUS/release checks
(build/clap-state-gui.log). No physical audio device is opened by that test host.
The rebuilt muted Standalone also passed NativeUiSmoke and original browser-audio
count-in/nudge/live-input/STUT oracles (build/clap-state-ui.log): 20 original loop
cards, working native IPC, MASTER zero and no product AudioContext. No physical
expression/footswitch test was repeated or newly inferred. All test processes
closed normally. The user's logo whitespace hunk remains unchanged and unstaged.


## Original slider/number wheel protection (22 October 2026)

Baseline 8997232. Disabling legacy main.js also omitted its global wheel guard:
focused slider/number fields were not blurred/protected on non-Shift scrolling.
The unchanged original listener fixture failed before the repair: the native
listener was absent (build/wheel-repro.log). The adapter now installs the original
window capture/non-passive listener in controls.js, preserving the precise
preventDefault, stopPropagation, target.blur sequence for non-Shift range/number
inputs. Shift and all other targets pass through; no wheel increment algorithm,
layout, UI control, C++ DSP, parameter encoding or package/version changed.
The user's separate 4-addition/2-deletion logo hunk remains untouched.

Original listener oracle passes 630 cases (build/wheel-frontend.log), including
both Shift states, range/number/text/checkbox/radio/button/file, multiple target
tags, wheel directions and delta modes, listener options and blur receiver.
Trusted Input.dispatchMouseEvent mouseWheel tests exercise MASTER, loop Volume,
BPM and loop-name targets, verify Shift bypass and text focus, preventDefault and
focus loss, and prove blocked events preserve DOM/native masterGain/syncBpm/loop
volume plus lastRequestId. These are actual native WebView2 events, not synthetic
DOM dispatchEvent proofs. Shift default editing, if the platform performs it, is
restored through existing control events before other workflows continue.

The connected capture/OD/SUB audit passed 126 cases / 965826 samples with max error
7.394757495227111e-8 against unchanged tolerance 1e-6 (build/wheel-audio.log), with
its retained scope exclusions. Native binaries/test host rebuilt incrementally
(build/wheel-plugin-build.log). Actual CLAP and VST3 validation initially passed
2/2 (build/wheel-formats.log): CLAP retains its documented 33 passes/10 skips/one
JUCE void state-load warning; VST3 retains level-9 GUI/automation/state coverage.
These are scoped tests, not full Ascii-Looper parity or real DAW/device/ASIO,
Apple/Linux GUI or allocation certification. No fresh sanitizer run is claimed
for a frontend-only change; existing C++ processor/core evidence is unchanged.


Fresh Windows portable suite passed 19/19 in 267.43s
(build/wheel-windows.log). Trusted wheel tests passed on both 64/128 hosts;
the full 64-sample workflow also passed mouse/capture/playback, sync, keyboard,
hold/delete, export, merge, and MIDI checks (build/wheel-gui.log).


Ubuntu completed 19/19 in 458.91s (build/wheel-ubuntu.log), including the new
unchanged-JS frontend comparison. Both complete 64/128 production mouse host
workflows passed (build/wheel-gui.log): original capture/playback, static/dynamic
sync/count-in, Start/Q/nudge/RTRG/STUT, 4s hold deletion/cancellation, keyboard/
keypad/missed-release, WAV export, MERGE/confirmation/history and MIDI control/
learn/continuous/panic/reload checks. No physical controller evidence was inferred.


Final format checks were rerun serially after the synthetic GUI host closed:
CLAP + VST3 2/2 in 29.17s (build/wheel-formats-final.log), followed by the muted
Standalone NativeUiSmoke and actual browser audio-reference comparisons
(build/wheel-ui.log). They pass with all 20 original cards, working IPC, MASTER
zero, no legacy product AudioContext, and no runtime gesture exceptions. The
original 16 asset SHA gate remains passed; all test hosts closed normally.
No new package, platform/driver certification or overall bug-free claim is made.


## Linux native build/processor/CLAP acceptance (22 October 2026)

Source baseline 9e75ec7, retaining the user's separate local logo hunk. No DSP,
frontend, parameter defaults/encoding, JUCE/CLAP pin, version or package changed.
The final increment also scopes OS MIDI watching to an actual Standalone holder;
that processor routing fix and its separate regression evidence are below. The existing native Linux build directory contained
0.1.0-era artifacts (LV2 TTL exposed only masterGain); it must not be treated as
matching the current Windows source acceptance.

The actual old Linux CLAP was tested before rebuilding. The pinned official
Ubuntu validator reported 31 successes, nine unsupported skips, one known
JUCE void state-load warning and three failures: state-reproducibility-basic,
-binary, -buffered. Each failed because restored masterGain changed without a
rescan, reproducing the already-fixed Windows defect in the stale ELF artifact.
All 44 tests were accounted for and Unix RTLD_NOW passed. Evidence:
build/linux-clap-initial.log and
build/linux clap initial/20261022T202015Z-01b7da24/{stdout.json,stderr.log,result.json}.
No failure was filtered, reclassified or removed to proceed.

CMake now exposes the existing production aion_midi_processor_tests on Linux as
well as Windows, without adding it to the default build. AION_TEST_NATIVE_PROCESSOR
registers its explicit serial CTest, with JUCE debug assertion/leak text treated
as failure even if a process returns zero. Windows target build passed; the final
opt-in CTest passed 1/1 in 1.53s (build/linux-gate-windows-processor-final.log),
covering the same 64/128 real MidiBuffer, held capture, CC/PC learn/continuous,
zero-block/overflow/panic, malformed data, fake-host/PCM isolation and state
restore/rollback checks. Only tracked C++ callback new/delete is instrumented;
this is not arbitrary malloc or wrapper allocation certification.

TestLinuxClap.py pins the official Ubuntu archive/executable hashes, refuses
ancestor TOML filters, accounts for every expected test/module path/ID and
requires Unix symbol loading to execute. The explicit setup reads only the
regular binary from the official zip's nested tarball. A TOML fixture disabling
state-reproducibility-basic was rejected (build/linux-clap-filter.log). Logs and
result.json retain source/dirty paths, platform, UTC timing, tool/harness/module
hashes, statuses, skips and warnings. Timeout kills only the harness-owned process
group. Neither CMake nor default builds download a validator. BUILDING.md gives
exact optional commands; SDKS.md records official asset hashes/layout.

Fresh portable suites passed Windows 19/19 in 260.78s and Ubuntu 19/19 in 435.07s
(build/linux-gate-windows.log, build/linux-gate-ubuntu.log). The connected original
capture/OD/SUB PCM audit passed 126 cases / 965826 samples, max absolute error
7.394757495227111e-8 at unchanged 1e-6 tolerance (build/linux-gate-audio.log), with
its retained envelope/resampling/latency/sync exclusions. The initial CMake/test-harness stage retained Windows GUI evidence at 9e75ec7.
After the routing defect below was found, affected native binaries were rebuilt
and GUI/format gates rerun separately. No new sanitizer run is claimed.


All four requested Linux targets linked successfully: Standalone, VST3, CLAP and
LV2 (build/linux-refresh-configure.log, build/linux-refresh-build.log). Existing
Debug build was retained to avoid a second configuration's redundant binaries.
Environment: Ubuntu 24.04.3 LTS x86_64 under WSLg, GCC 13.3.0, CMake 3.28.3;
installed pkg-config ALSA 1.2.11, JACK 1.9.21, WebKitGTK 2.52.6 and GTK 3.24.41.
The unused optional libcurl lookup remains absent with JUCE_USE_CURL=0; no system
package installation was needed. Generated LV2 dsp.ttl and VST3 moduleinfo.json
now identify 0.4.3; LV2 metadata includes current inputMonitor and syncBpm.
The LV2/VST3 metadata helpers loaded their modules during build, but that is not
full host/MIDI/audio/UI validation for those formats.

The refreshed ELF CLAP passed the complete gate: 34 success, nine unsupported
skips, one retained warning, exit 0, all 44 tests accounted for, in 6.13s.
All three stale-binary state failures now succeed, and Unix RTLD_NOW passes.
Evidence: build/linux-clap-refreshed.log and
build/linux clap refreshed/20261022T203807Z-dd7c0ea9/result.json. Module SHA256
b3bca4e1add2b71ed674c794c2262471f73e2a08f8371cee24163d243c1b46d4
was unchanged before/after validation. The same unsupported optional extensions
as Windows remain explicit, excluding Windows's Unix-only skip. The known
state-invalid-random warning still reflects JUCE's void restore API, not an
accepted processor-state mutation. GUI/allocation/hardware flags remain false.


The first stricter Linux native CTest exposed a genuine routing problem despite
all functional checks printing PASS: JUCE asserted in juce_Midi_linux.cpp:539
because /dev/snd/seq is absent. Initial gates were 1/2 (native processor failed,
CLAP passed), retained in build/linux-native-gates.log. The processor constructor
unconditionally subscribed to OS MIDI device changes, even in host-routed/synthetic
instances. This was previously hidden by Release assertions and by not building
the JUCE processor test on Linux. The new FAIL_REGULAR_EXPRESSION caught it;
no assertion rule was removed and no ALSA/driver installation was used to hide it.

The processor now subscribes only when JucePlugin_Build_Standalone is enabled and
an actual StandalonePluginHolder exists. The pinned JUCE holder sets currentInstance
before creating its processor. Real Standalone retains the identical connection
and engine.cancelMidi callback; host/synthetic instances no longer probe/watch
hardware, so unrelated OS device changes cannot cancel their host-routed gestures.
Existing Standalone polling/selection cancellation and explicit MIDI Stop/panic/
overflow paths are unchanged. MIDI input remains host-provided in plugins.
midiDevices adds the diagnostic deviceWatchRequested (registration request,
not proof of OS hardware availability). Native tests require false with no holder;
actual Windows Standalone UI smoke requires true. No new user-facing control was
added and no MIDI assignment was changed by this fix. Affected Windows and Linux
native targets were rebuilt before final acceptance; logs are recorded below.


Final acceptance after the routing repair:

- All affected Linux targets rebuilt (build/linux-routing-fix-build.log), including
  the production processor executable and all four formats. Existing nonfatal
  compiler diagnostics and the metadata helper's libEGL/DRI3 warning are retained;
  accelerated GTK rendering is not claimed tested.
- Linux native_midi_processor passes 1/1 in 4.62s with the unchanged assertion/leak
  failure rules (build/linux-routing-fixed-processor.log). No JUCE assertion,
  leak diagnostic or ALSA sequencer-open message remains. All 64/128 functional,
  zero-block/offset, state/learn/panic/continuous, host-field/PCM-isolation and
  tracked zero callback new/delete checks pass. /dev/snd/seq remains absent
  (build/linux-routing-environment.log); no driver/device was installed to pass.
- Final registered Linux CLAP CTest passes 1/1 in 11.50s: 34 successes, nine
  unsupported skips, one retained void-load warning, all 44 tests accounted for
  and Unix RTLD_NOW successful (build/linux-routing-fixed-clap.log). Evidence:
  build/linux-clap-validator/20261022T205856Z-88373fc3/result.json.
- Windows affected app/plugin/test targets rebuilt
  (build/linux-routing-fix-windows-build.log). Native processor CTest passes 1/1
  in 1.66s (build/linux-routing-fix-windows-processor.log). Actual Windows VST3
  level-9 pluginval and CLAP gates pass 2/2 in 25.50s
  (build/linux-routing-fix-windows-formats.log), retaining known coverage limits.
- Actual muted Standalone UI smoke confirms deviceWatchRequested=true and
  Standalone routing, all 20 original cards, native IPC, MASTER zero and no
  product AudioContext. Browser count-in/nudge/live-input/STUT oracles pass
  (build/linux-routing-fix-windows-ui.log). The synthetic host requires no OS
  watcher; complete mouse/export/merge/MIDI workflows pass at both 64 and 128
  samples (build/linux-routing-fix-windows-gui.log). No physical footswitch,
  expression or hotplug result was inferred. All launched test hosts closed.

Final repaired ELF CLAP module SHA256: 3553e6bb97f9504fc50f73f30cb0e48201c754feb5f65942ada8a73c20cd62ed.
It was identical before/after the final run. The baseline failed logs and the
intermediate pre-repair CLAP result above remain distinct. Core/DSP code did not
change after the portable 19/19 and 126-case PCM gates recorded above, so those
results remain applicable. Linux GTK/WebKit interaction, real JACK/ALSA audio/
MIDI/controllers/DAWs, Linux LV2/VST3 host workflows, Release latency/CPU, Apple
platforms and remaining original features stay unverified/unported as tracked
in PARITY.md. No package/version bump or full commercial-readiness claim.


## Linux VST3 wrapper acceptance (22 October 2026)

This increment closes the missing Linux VST3 public-host gate. It does not add a
looper feature or change product DSP/UI/state, original reference assets or the
user's local logo edit. The existing 0.4.3 Debug bundle from the MIDI-routing
repair was used; no new app/plugin package or version bump was needed.

The official Tracktion pluginval 1.0.4 Linux x86_64 binary was obtained directly
from its tagged release, with archive/executable SHA256 pins in SDKS.md and
TestLinuxVst3.py. The historical release API exposes no publisher digest, so
these are pins of the fetched official asset, not a claimed publisher signature.
The binary reports its own JUCE 8.0.3 runtime; product JUCE remains pinned 9.0.4.
The machine is the existing Ubuntu 24.04.3 WSL environment with WSLg DISPLAY :0.
No system installation, physical audio/MIDI device or user's live session was
used. The optional helper creates only its own temporary validator editor hosts.

Acceptance recorded:

- Direct Linux VST3 validation passed at level 9 / seed 1337 in 21.90s:
  build/linux-vst3-initial.log and
  build/linux-pluginval/20261022T222527Z-24d09bdf/result.json.
- New registered linux_vst3_pluginval CTest passed 1/1 in 24.40s (24.52s total),
  build/linux-vst3-registered.log and
  build/linux-pluginval/20261022T223006Z-e0b7b3fe/result.json. All required editor,
  editor-while-processing, audio, automation, state/restoration, background/thread
  safety and fuzz tests completed, with all 15 combinations of 44100/48000/96000
  Hz and 64/128/256/512/1024 samples present. There were no JUCE assertion/leak
  messages and only one plugin was discovered.
- The registered run deliberately inherited SKIP_GUI_TESTS=1,
  DISABLED_TESTS=/missing/aion-disabled-tests.txt and BLOCK_SIZES=4096. The helper
  removed these only in the child environment: the full required GUI/rate/block
  coverage still passed and the caller's three values remained identical.
- Negative checks rejected no DISPLAY (build/linux-vst3-no-display.log) and an
  unpinned validator (build/linux-vst3-wrong-tool.log) before launching it. Python
  compilation succeeded. No test was disabled or tolerance relaxed.
- Connected committed-PCM audit passed 126 cases / 965826 samples, maximum error
  7.394757495227111e-8 against the unchanged 1e-6 threshold
  (build/linux-vst3-audio.log). Its existing capture/OD/SUB scope excludes full
  browser envelopes/resampling, hardware latency and sync scheduling.

The VST3 .so SHA256 was identical before/after both runs:
840e8eba642e446b1b77b19f9d4e155e7145f5f30b1596d2a0195df5be600155.
result.json records source/local changes, harness/tool/module identities, command,
timing, required completions, matrix and limitations. CMake registration is
opt-in and serial; the helper's 240-second timeout kills only its process group.

Expected WebKit CSP messages explicitly reject the original inline scripts and
legacy WebAudio modules. These do not imply that the native adapter failed, nor
do editor-lifecycle tests prove that the rendered HTML controls work. WSLg also
prints the existing libEGL/DRI3 acceleration warning. Linux HTML interaction,
accelerated rendering and actual DAW/recorded-loop/controller sessions remain
separate. The official pluginval binary omits allocation tests and no external
Steinberg validator is configured; both result flags are false. This pass is not
arbitrary-malloc, JACK/ALSA driver, Release latency/CPU, Apple-platform or complete
feature-parity/commercial-readiness certification. Earlier pending Linux VST3
host-workflow statements above are historical; this closes the vendor wrapper
subset only. Broader DAW/hardware and LV2-host workflows remain pending.


Additional unchanged-behavior regressions for this gate addition:

- Windows portable/reference CTest: 19/19, 262.32s
  (build/linux-vst3-windows-core.log), including original asset bytes and all
  existing frontend/MIDI/DSP/sync/edit/export comparisons.
- Existing Linux native processor and CLAP CTests: 2/2, 13.73s total
  (build/linux-vst3-native-regressions.log). The processor passes at 64/128 with
  strict assertion/leak checks and tracked zero callback new/delete. CLAP retains
  its complete 44-test accounting and documented skips/void-load warning.
- Ubuntu portable/reference CTest: 19/19, 435.81s
  (build/linux-vst3-ubuntu-core.log), retaining the same original-JS/audio gates.

All sessions launched for this increment completed. Product DSP and UI were not
changed, so no native binary rebuild or repeat Windows GUI workflow was needed.
The existing VST3 module was verified by before/after hashes; current coverage
limits above are retained. No package or version change was created.


## Linux LV2 host and nonzero stereo acceptance (22 October 2026)

This increment validates the existing 0.4.3 Debug LV2 bundle through its public
host interface. No product DSP/UI/parameter/state code, original reference bytes,
MIDI assignments or user's local logo edit changed. No rebuild/package/version
bump was required: the existing compiled bundle was used and its identity was
checked. Both the vendor tool and ABI probe ran in the existing Ubuntu/WSLg
environment without opening a physical audio or MIDI device.

The official pluginval 1.0.4 release explicitly adds LV2 support. TestLinuxLv2.py
reuses the existing pinned official Linux runner in TestLinuxVst3.py; the VST3
entry point/defaults remain compatible. The shared runner now records format,
entry-point and shared-harness hashes, requires the correct format/name scan,
and checks module/RDF hashes before/after LV2 validation. No supported test was
filtered or assertion/leak rule weakened. Limits remain explicit: the vendor
binary omits allocation testing and no external Steinberg validator is configured.

Initial observations:

- Raw official LV2 level-9 run passed in 22.67s; logs are retained in
  build/linux-lv2-initial/{stdout,stderr}.log.
- Full direct pinned LV2 harness passed in 23.73s, including all required editor,
  editor-while-processing, automation, state/restoration, background/thread-safety
  and fuzz completions, plus all 15 rate/buffer combinations. Evidence:
  build/linux-lv2-direct.log and
  build/linux-lv2-pluginval/20261022T224315Z-8b1ff632/result.json.
- Negative preflight checks reject no DISPLAY, a VST3 supplied as LV2 and missing
  LV2 UI metadata before validator execution (build/linux-lv2-negative.log).
  Both runner entry points compile successfully under Ubuntu Python.
- Actual LV2 descriptor/ports/URID/options/Atom patch:Set audio probe passes all
  six 44100/48000/96000 Hz x 64/128 cases (build/linux-lv2-audio-abi.json and
  build/linux-lv2-audio-abi.stderr.log). It compares 24192 settled stereo samples,
  including 13824 nonzero samples; maximum error is 0.0 against 1e-7. Default
  mute, unity/half gain, input-monitor off, master zero and unmute recovery pass.
  Output is first filled with NaNs, then every sample must be overwritten with
  a finite value. Reported latency remains zero; this is the declared processing
  latency, not a measurement of physical driver or round-trip latency.
- The probe supplies public host structures and block-start patch:Set messages;
  it does not include product headers or call private processor methods. It waits
  beyond the existing 20ms master ramp before comparing PCM, so it does not claim
  ramp-waveform or sample-offset parameter scheduling acceptance. It verifies the
  descriptor URI, generated port metadata and module/RDF hashes, and records
  source/harness identities. Its Python host callbacks/buffers are not realtime
  allocation instrumentation. No editor is opened by the audio probe.

LV2 module SHA256:
faa2721ec29d77a768895f4e079780618d7626854b12755fe476d897df96a530.
The module and all three generated RDF files are identical before/after the
recorded runs. Their individual hashes are included in the JSON evidence.
Expected WebKit CSP refusals of legacy browser-audio scripts and WSLg libEGL/DRI3
warnings remain visible in vendor stderr; no policy or rendering workaround was
introduced to pass the gate. Editor lifecycle still does not prove HTML controls.

This closes only the tested LV2 wrapper/parameter/stereo subset. Actual DAW
recorded-loop/project/MIDI/controller workflows, hardware JACK/ALSA timing,
Linux HTML interaction, Release performance, other platforms and all remaining
original synth/effect/project features remain separate. Historical pending-LV2
host statements above should be read with this narrower acceptance addition;
there is no complete feature-parity or commercial-readiness claim.


Final registered acceptance (build/linux-lv2-final-gates.log): 5/5 in 71.58s.

- Native production MIDI/audio processor: 4.80s; strict assertion/leak and
  existing 64/128/tracked zero callback new/delete checks retained.
- LV2 pluginval: 27.24s; final evidence
  build/linux-lv2-pluginval/20261022T225111Z-ce18f0be/result.json.
- LV2 public ABI stereo audio: 4.22s; registered JSON output matches the six-case
  zero-error direct probe and before/after module/RDF identities above.
- VST3 pluginval regression through the shared runner: 26.13s; evidence
  build/linux-pluginval/20261022T225144Z-53b2ae47/result.json.
- CLAP regression: 9.00s; 34 successes, nine documented unsupported skips and one
  retained JUCE void-load warning, all 44 tests accounted for. Evidence:
  build/linux-clap-validator/20261022T225207Z-72a8e608/result.json.

The combined run inherited SKIP_GUI_TESTS=1, DISABLED_TESTS pointing to a missing
file, BLOCK_SIZES=4096 and REPEAT=9. Both pluginval gates still completed the
required GUI/tests and full intended matrix with repeat one. The shared runner
sanitized only child options; all caller values were verified unchanged afterward.
Final runner/entry-point/probe and module/RDF hashes match the evidence. No
additional download, package, native UI change or dependency upgrade was needed.

Unchanged original behavior regressions also passed:

- Windows portable/reference CTest: 19/19 in 262.98s
  (build/linux-lv2-windows-core.log).
- Ubuntu portable/reference CTest: 19/19 in 429.73s
  (build/linux-lv2-ubuntu-core.log).
- Connected committed capture/OD/SUB PCM audit: 126 cases / 965826 samples,
  maximum error 7.394757495227111e-8, unchanged tolerance 1e-6
  (build/linux-lv2-audio.log). Its existing scope excludes full browser envelopes,
  resampling, hardware latency and sync scheduling.

All launched test processes completed. The user's running app/session was not
replaced or closed. Since product code did not change, the existing modules were
verified by hashes rather than producing new builds/packages. Remaining feature
and physical-platform limits in PARITY.md are unchanged.


## Focused loop slider reset parity (23 October 2026)

A real WebView2 regression exposed a display mismatch: double-clicking the Speed
range reset native playbackRate to 1, but its focused DOM value stayed at 1.15.
Feedback had the same path: native reset 0.8 while the focused slider value was
excluded from snapshot refresh. The unchanged original resetLoopSpeed and
resetLoopFeedback explicitly set their range values regardless of focus.
The reproduced failed gate is retained in build/slider-reset-before.log; the
new oracle failed without an adapter (build/slider-reset-oracle-before.log).
The failed synthetic host closed through its normal owned-process cleanup.

controls.js now installs a bounded capture-phase double-click adapter for enabled
Speed/Feedback ranges with canonical track IDs 0..19. It moves only the DOM value
to 1/0.8. Existing target listeners still own the native command; no duplicate
input/change event, blur, propagation suppression, style/UI redesign or C++ DSP/
parameter/state change is introduced. The user's separate looper.js logo edit
was not modified. All original reference assets remain byte-identical.

The unchanged original setter/reset methods are executed across 560 combinations
(all 20 loops, both sliders, seven starting values, sync on/off), comparing the
focused range result. Guards exclude unrelated/disabled controls and malformed/
out-of-range IDs. Existing 630-case wheel and all gesture regressions remain.
After the adapter, the complete frontend test passes
(build/slider-reset-oracle-after.log).

The actual mouse workflow now sends trusted two-click browser input, waits for
native acknowledgement and checks the focused DOM thumb and native value. It
also requires playing state, stored clip length/identity and Undo depth to stay
unchanged, and verifies nonzero recorded audio continues after each reset. This
covers the accepted reset path, not exact browser-vs-native transition envelopes
or hardware latency. No package/version bump was created. Existing native
binaries are refreshed only to embed the corrected adapter.


Final acceptance:

- Windows portable/reference CTest: 19/19, 258.76s
  (build/slider-reset-windows-core.log); Ubuntu: 19/19, 409.05s
  (build/slider-reset-ubuntu-core.log), including original asset identity and the
  560 reset/630 wheel cases. No tolerance or original method was changed.
- Connected capture/OD/SUB PCM audit: 126 cases / 965826 samples, maximum error
  7.394757495227111e-8 against unchanged 1e-6 (build/slider-reset-audio.log).
- Full real WebView2 mouse/export/merge/MIDI workflow passes at both 64/128,
  including trusted focused Speed/Feedback resets, stored nonzero PCM and history
  preservation (build/slider-reset-windows-gui-final.log).
- Existing Windows Standalone/VST3/CLAP and test binaries refreshed
  (build/slider-reset-windows-build.log); Linux Debug Standalone/VST3/CLAP/LV2 and
  processor refreshed (build/slider-reset-linux-build.log). Existing nonfatal GCC
  shadow/float and WSLg libEGL warnings are retained. Exact current controls.js
  bytes were found embedded in the Windows app/plugins and all four Linux formats;
  source/module hashes are recorded in build/slider-reset-embedded.json.
- Muted actual Standalone passes count-in/nudge/live-input/STUT original WebAudio
  oracles and NativeUiSmoke, including 20 original cards, native IPC and no legacy
  production AudioContext (build/slider-reset-windows-ui-final.log).
- Windows native processor + VST3 pluginval + CLAP CTests: 3/3, 27.87s
  (build/slider-reset-windows-formats.log). Existing vendor skip/allocation limits
  remain unchanged; actual DAW/controller sessions are not certified by this.
- Linux production processor/LV2 nonzero audio/CLAP: 3/3, 20.68s
  (build/slider-reset-linux-native.log). Linux LV2/VST3 pluginval editor/host gates:
  2/2, 54.39s (build/slider-reset-linux-formats.log). This retains strict assertion/
  leak checks, native 64/128 tracked zero new/delete, actual LV2 stereo PCM and
  all CLAP 44-test accounting. Final editor-host evidence:
  build/linux-lv2-pluginval/20261022T232334Z-c6ba1a28/result.json and
  build/linux-pluginval/20261022T232403Z-ef453aba/result.json.

Two earlier interrupted attempts remain visible instead of being counted passed:

1. The first repaired 64/128 GUI run passed both focused resets, but its 128-sample
   DYNAMIC queued-finish fixture clicked near the end of the bar. The final trace
   had a valid playing take with no rejection; its capture end was 1024 samples
   (21.33ms at 48kHz) before the bar boundary. Both original stopRecording and the
   native engine intentionally finish immediately inside 50ms. The test now waits
   for a fresh early phase after screenshot/mode setup, then still strictly
   requires stopPending and subsequent committed playback. No DSP timing or
   tolerance changed. Existing SyncTests.cpp <=50ms immediate-finish coverage is
   retained and passed. Failed attempt: build/slider-reset-windows-gui.log;
   complete final rerun: build/slider-reset-windows-gui-final.log.
2. The first Standalone oracle run lost its CDP page between CountIn and Nudge
   (build/slider-reset-windows-ui.log). That attempt had no process-exit evidence,
   so its cause is unconfirmed; no product crash fix is claimed. The runner now
   records its owned PID and process status before cleanup. The full instrumented
   run passed every oracle/UI assertion and explicitly recorded the app still
   running before normal owned cleanup (build/slider-reset-windows-ui-final.log).
   No assertion was removed or timeout/tolerance relaxed.

All owned test sessions completed/closed. User mappings and the separate logo
edit were not modified. No package/version bump was made. These checks establish
this reset interaction and existing synthetic/format regression scope, not
physical ASIO/JACK/CoreAudio, full browser envelopes, Apple/Linux physical pointer
interaction or complete synth/effect/project parity/commercial readiness.

## Real REAPER and Ardour acceptance, 23 October 2026

This increment adds repeatable DAW checks against the existing c95ab50 product
binaries. UI assets/adapters, C++ DSP, mappings, pinned JUCE and CMake targets
are unchanged. JUCE latest stable was checked as 9.0.4 against the official
[release](https://github.com/juce-framework/JUCE/releases/tag/9.0.4). No package
or version bump was made. Ubuntu's Ardour 8.4 package and required dependencies
were installed without upgrading/removing existing packages; no user session or
profile was imported. Exact reproduction commands are in BUILDING.md.

Windows REAPER 7.82/x64, tools/TestReaper.py + tests/ReaperDawProbe.lua:

- Final evidence: build/reaper-daw/20261023T080417-4c8861a4/result.json.
- Both actual Release VST3 and CLAP were observed at their expected local module
  paths inside the owned REAPER processes. Before/after module hashes match;
  host executable, runner and script hashes are recorded. Both children exited
  successfully. The preexisting user REAPER PID 8316 remained running.
- Seven target renders per format: default mute, unity, half gain, monitor off,
  master zero, recovery and actual RPP reopen. Both stereo channels are checked
  at 48kHz after 100ms, excluding the separately tested gain-ramp envelope.
  All 1,209,600 compared samples have measured maximum absolute error 0.
- Each of the six parameter states was saved as a track chunk, deliberately
  mutated, restored and processed, with strict host-value checks at each stage.
  Parameter changes are actually delivered through offline processing before
  serialization and comparison; no device callback runs while stopped/closed.
  Final recovery is saved and reopened as an RPP before another verified render.
- Profiles are unique and guarded against reuse of a nonempty/user project.
  Test master hardware outputs are removed; input is a generated stereo WAV.
  No global plugin installation, user settings, MIDI assignments or license copy.
  A missing-host negative check failed before launch as intended.

Ubuntu Ardour 8.4.0~ds1, tools/TestArdour.py + tests/ArdourDawProbe.lua:

- Final evidence: build/ardour-daw/20261023T080619-655d0b97/result.json; summary
  build/daw-ardour-final.log. Both runner/script hashes match the final files.
- Actual ardour8-lua/libardour loads the existing Debug LV2 through its normal
  plugin/session APIs, using isolated HOME/XDG paths and a Dummy sine backend.
  This is the real session engine without GTK, not the prior ctypes LV2 host.
- Both 64 and 128 samples at 48kHz passed seven stereo stages: default mute,
  unity, half gain, monitor off, master zero, recovery and saved-session reopen.
  The backend rate/buffer are checked before and after reopen. Unity/half peaks
  match the synthetic source level; half is -6.0206dB relative to unity within
  0.001dB, recovery within 0.001dB, and mute is exactly negative-infinite peak.
- All twelve APVTS numeric property controls are discoverable through Ardour's
  Automatable API; the exercised controls are Master gain and Input monitor.
  Plugin.parameter_count counts LV2 ports and is not the numeric-property API.
  MIDI meters precede audio meters, so the probe derives their offset and
  independently requires two audio channels. No JUCE wrapper fork was needed.
- Module and all three RDF files remain unchanged. Strict PASS plus Lua-error/
  JUCE assertion/leak checks are required even if luasession exits zero. The
  missing-display preflight fails before session creation. Host debug messages
  about MIDI-clock buffers, direct dispatch and the absent audition synth remain
  in the log; they are not evidence of implemented plugin MIDI clock.

Retained native regressions passed against the same product binaries:

- Windows native processor, VST3 pluginval and CLAP validator: 3/3, 32.22s,
  build/daw-windows-regressions.log.
- Ubuntu native processor, exact LV2 ABI stereo PCM and CLAP validator: 3/3,
  18.25s, build/daw-linux-regressions.log. Existing allocation/assertion checks,
  reference tolerances and all CLAP test accounting remain intact.

Earlier harness failures are retained, not counted as product acceptance:

- Windows path separators initially tripped the profile guard; both sides are
  now normalized. Immediate state reads while the device was closed saw queued
  host changes; actual renders now precede set/mutation/restore checks, whose
  numeric assertions were retained. An invalid cleanup API and a missing RPP
  save-as-current flag were corrected using the official REAPER API. Those
  failed/owned-terminated runs retain passed:false reports under build/reaper-daw.
- Ardour discovery initially inspected only port parameters, not property
  automation. The first meter probe also read MIDI meter 0 as left audio; the
  final probe derives the offset rather than ignoring a silent audio channel.
  A parser assumed tab-delimited print output; whitespace tokenization now
  retains exact backend/rate/buffer requirements. Diagnostic attempts remain
  under build/ardour-discovery and build/ardour-daw.

Scope limits: REAPER renders are offline, not 64/128 physical-driver latency
certification. Ardour compares stereo peaks/ratios, not sample-by-sample PCM or
GTK/embedded-HTML interaction. Ardour 8.4 does not support CLAP; a real compatible
Linux CLAP DAW is still needed. Both checks cover parameter/session recall, not
unported recorded-loop project storage, host-follow scheduling, MIDI clock,
physical ASIO/JACK/CoreAudio, arbitrary callback malloc, Apple builds or complete
synth/effect/functionality parity. No new feature was enabled by these tests.

Fresh Windows portable/original-JS regressions passed 19/19 in 254.68s
(build/daw-windows-core.log). The independent connected PCM audit passed all
126 cases / 965,826 samples, maximum error 7.394757495227111e-8 against the
unchanged 1e-6 tolerance (build/daw-reference-audio.log). No source asset,
audio tolerance or retained core/GUI test was changed by this increment.

Fresh Ubuntu portable/original-JS regressions also passed 19/19 in 442.20s
(build/daw-ubuntu-core.log). All owned test hosts completed; no new GUI workflow,
physical controller, sanitizer or Apple acceptance is claimed for this increment.
The user's preexisting resources/native/looper.js logo edit remains unstaged.

## Original synchronized Speed restored, 23 October 2026

Baseline: a75514d. Original UIManager.setLoopSpeed in preserved loopTracks.js
quantizes synchronized edits at <0.38/.75/1.5 to .25/.5/1/2 and explicitly moves
the focused range when sync is enabled. The native adapter previously forwarded
raw values and its ordinary snapshot refresh skipped focused inputs. The prior
build reproduced 0.370000004768372 instead of .25 in a real trusted range edit.
OFF intentionally suppresses focused-thumb writes in the original; that behavior
is preserved rather than introducing an unsolicited UI change.

The message-thread-only AionLoopSpeed/speedCommand adapter now uses those exact
thresholds for STATIC/DYNAMIC and the existing slider/reset/half command paths.
Queue ordering/coalescing and native command ownership stay unchanged. No C++ DSP,
APVTS schema, original HTML/CSS/JS asset or user logo change is included.

Reference and live acceptance:

- tests/FrontendTests.cjs executes the unchanged original setter: 1,560 cases
  covering all 20 tracks, OFF/STATIC/DYNAMIC, focused/unfocused and both sides
  of every threshold. It compares command values and the original DOM rule.
  Existing 630 wheel and 560 double-click-reset cases remain. Before/after logs:
  build/sync-speed-before.log and build/sync-speed-after.log.
- The unchanged Windows binary failed the trusted 64-sample range-key check
  (build/sync-speed-gui-before.log). After rebuilding, complete 64/128 mouse,
  export, merge and MIDI workflows passed (build/sync-speed-gui-final.log).
  The new check covers six boundary edits in both sync modes, checks native rate
  and focused thumb, retains state/length/waveform/history, and requires nonzero
  recorded PCM. No failing state/audio assertion or tolerance was relaxed.
- Actual muted Standalone UI and original browser count-in/nudge/live-input/STUT
  audio regressions passed (build/sync-speed-standalone-ui.log). PID 26992 was
  still running before normal owned cleanup; native DOM had no legacy AudioContext.
- Full portable/original-JS suites passed Windows 19/19 in 256.23s and Ubuntu
  19/19 in 392.67s (build/sync-speed-windows-core.log and
  build/sync-speed-ubuntu-core.log).
- After those suites, the retained SyncParity.cjs matrix was extended from
  half/unity/double to include quarter speed. Fresh runs of that final version
  passed on BOTH OSes: 144 effective-rate checks, 144 unchanged capture cases,
  388,772 PCM frames, maximum capture error 6.093089943703944e-8 / tolerance 1e-6.
  Evidence: build/sync-speed-windows-rate.log and build/sync-speed-ubuntu-rate.log.
  Effective-rate metadata comparisons are separate from capture PCM comparisons.
- Independent connected capture/OD/SUB audit passed 126 cases / 965,826 samples,
  maximum error 7.394757495227111e-8 / unchanged tolerance 1e-6
  (build/sync-speed-audio.log).

Rebuilt current native artifacts, retaining format/assertion/allocation gates:

- Windows Release Standalone/VST3/CLAP, synthetic mouse host and processor test
  built successfully (build/sync-speed-windows-build.log). Native processor,
  VST3 pluginval and CLAP validator passed 3/3 in 27.12s
  (build/sync-speed-windows-formats.log).
- Ubuntu Debug Standalone/VST3/CLAP/LV2 and processor test rebuilt successfully
  (build/sync-speed-linux-build.log). Native processor, actual LV2 stereo ABI,
  VST3/LV2 pluginval and CLAP validator passed 5/5 in 74.20s
  (build/sync-speed-linux-formats.log). Compiler/EGL/DRI3 warnings are retained.
- Exact complete controls.js and looper.js bytes are embedded in all seven
  Windows/Linux app/plugin modules; hashes/matches are recorded in
  build/sync-speed-embedded.json. This verifies current embedded adapters rather
  than relying on a source-only or stale executable test.
- Real REAPER 7.82 VST3/CLAP passed all seven target stereo renders per format,
  track-state mutation/restore and actual RPP reopen. All 1,209,600 settled
  compared samples have maximum error 0, exact loaded modules were observed and
  hashes remained stable. Final evidence:
  build/reaper-daw/20261023T083712-2ef21ff6/result.json and build/sync-speed-reaper.log.

No ZIP, installer or version bump was created. Native binaries are for the
existing acceptance workflow. The user's separate logo whitespace edit remains
outside the commit. These checks establish original synchronized control/rate
semantics and retained audio/host regression scope, not browser resampling or
transition-envelope equality, physical latency, MIDI clock/host-follow, complete
synth/FX/project parity, Apple validation or commercial readiness.

Final real Ardour 8.4 LV2 gain/mute/session recall also passed at 64/128 using
the rebuilt bundle (build/sync-speed-ardour.log;
build/ardour-daw/20261023T084112-37cea025/result.json). Bundle/RDF hashes remain
stable; the synthetic Dummy-backend stereo peak checks retain their documented
limits. All owned test hosts completed and closed normally.


## Original loop status text repair — 23 October 2026

The native refresh displayed STOPPING with `[S]`, the original stopped symbol.
The unchanged original updater uses `[.]` until stopping completes. Mute/duration
spacing also differed. `AionLoopStatus` now supplies original state symbols,
uppercase labels and exact mute/duration formatting; native worker finalization
retains `[..]` / SAVING. No DSP, transport command, APVTS, CMake dependency or
preserved reference asset changed.

Reproduction and reference gate:

- `build/loop-status-before.log` retains the failing original text comparison;
  `build/loop-status-baseline-symbol.json` records the old production symbol
  against the unchanged original updater and baseline commit.
- `tests/FrontendTests.cjs` executes the complete unchanged
  `UIManager.updateLoopDisplays` method and `getLoopAction` with status-only DOM
  fixtures (action/canvas nodes absent, precomputed peak placeholder). All 6,400
  cases pass: 20 loops, ten reference states, mute on/off, zero/one/short/long
  stored length and .25/.5/1/2 speed. Native finalizing is checked separately.
  This compares display outputs, not canvas pixels, playback envelopes or DSP.
  `build/loop-status-after.log` retains the final reference run.
- Windows portable/core regressions passed 19/19 in 269.39s
  (`build/loop-status-windows-core.log`). The independent connected audio audit
  passed 126 cases / 965,826 samples, maximum error
  7.394757495227111e-8 / unchanged tolerance 1e-6
  (`build/loop-status-audio.log`).

Architecture remains the existing message-thread browser adapter and native
processor/audio ownership graph in MIGRATION_BLUEPRINT.md. JUCE's official
[WebBrowserComponent API](https://docs.juce.com/master/classjuce_1_1WebBrowserComponent.html)
was consulted; this repair adds no browser/native callback or audio-thread work.
No package or version bump is needed. Physical devices, Apple targets, complete
JS parity, MIDI clock/host-follow and recorded-loop persistence remain separate.
The user's existing logo whitespace edit is retained outside this commit.


Completed rebuilt-artifact acceptance for this repair:

- Ubuntu portable/core tests passed 19/19 in 386.03s
  (`build/loop-status-ubuntu-core.log`).
- Complete actual WebView2 mouse/export/merge/raw-MIDI workflows passed at both
  64 and 128 samples (`build/loop-status-gui.log`). The new test requires
  stopping `[.]` while captured PCM is nonzero, then stopped `[S]` after second
  STOP, exact original duration text and unchanged clip identity/length/history.
  Existing arm/record/play, hold-delete, sync, speed, waveform, keyboard and
  cancellation coverage remains in place. This is synthetic audio, not a device.
- Actual muted Standalone UI plus original browser count-in/nudge/live-input/STUT
  comparisons passed (`build/loop-status-standalone-ui.log`): twenty original
  loop cards, IPC/audio acknowledgements and no legacy AudioContext. Owned
  hosts closed normally; no user hardware session or mappings were changed.
- Windows Release app/VST3/CLAP and mouse/processor hosts rebuilt
  (`build/loop-status-windows-build.log`); processor plus pinned VST3 pluginval
  and CLAP validation passed 3/3 in 30.46s
  (`build/loop-status-windows-formats.log`).
- Linux Debug app/VST3/CLAP/LV2 and processor host rebuilt
  (`build/loop-status-linux-build.log`); processor/LV2 stereo ABI and pinned
  VST3/LV2 pluginval/CLAP gates passed 5/5 in 72.56s
  (`build/loop-status-linux-formats.log`). Existing compiler/DRI3 warnings and
  documented vendor skips/limitations are retained.
- Full current controls.js and looper.js bytes match all seven rebuilt modules;
  paths, module hashes and match results: `build/loop-status-embedded.json`.
  Local binaries include the user's retained logo whitespace; that separate edit
  is intentionally outside the commit.
- Real REAPER 7.82 VST3 and CLAP each passed seven target stereo renders, deliberate
  parameter mutation/restore and RPP reopen. Evidence:
  `build/loop-status-reaper.log` and
  `build/reaper-daw/20261023T103831-301ec44e/result.json`.
- Real Ardour 8.4/libardour LV2 passed stereo gain/mute and saved-session reload
  at 48kHz/64/128 using its Dummy backend. Evidence:
  `build/loop-status-ardour.log` and
  `build/ardour-daw/20261023T104302-6a8315b3/result.json`.
  These host gates preserve their earlier settled-PCM/peak and synthetic-device
  scope; neither establishes physical latency, editor interaction, recorded-loop
  storage, MIDI clock or host-follow scheduling.


## Original global performance-status repair — 23 October 2026

The original `UIManager.updateStatus` independently paints the active recording/
armed/overdub/substitute/multiply state and applies SUB > SUS > OD precedence to
the master-header/global-mode classes. Native refresh lacked active OD/SUB
classes, SUS precedence and header text, and substituted RECORDING/READY for the
original YES (Loop N)/NO label/colours. `AionGlobalStatus` now restores those
presentation rules from native capture ownership, preserving unrelated classes
and the initial master label. Finalizing is not an active recording. The native
master recorder, keyboard SUS, synths and other unported engines remain disabled/
pending; this does not implement any of them. DSP, MIDI dispatch, APVTS, persisted
state formats, original assets and CMake dependencies are unchanged.

- `build/global-status-before.log` retains the failing old-production comparison
  (READY versus original NO, missing original label colour and stale classes).
- The complete unchanged `UIManager.updateStatus` and its called live updater
  execute against a focused DOM fixture; the live panel/action buttons are absent.
  The 7,920 comparisons cover all 20 capture owners, eleven native states, three
  OD/SUB modes, SUS on/off, queued on/off and three default-label strings. Native
  inputs and unrelated body classes must remain unchanged. This checks global
  status only; it does not certify live-panel rendering or application language
  selection. Final source run: `build/global-status-after.log`.
- Windows portable/core tests passed 19/19 in 260.49s
  (`build/global-status-windows-core.log`). Independent connected stereo audio
  audit passed 126 cases / 965,826 samples, max error
  7.394757495227111e-8 / unchanged 1e-6 tolerance
  (`build/global-status-audio.log`).

Initial native workflow attempts are retained, not counted as passes:

1. A sandboxed host timed out at CDP Runtime.enable and failed normal shutdown
   (`build/global-status-gui-startup-failure.log`).
2. The first cleanup command failed; the next attempt briefly overlapped the
   still-running owned host. It failed the existing mandatory pre-navigation
   stutter evidence check with length zero
   (`build/global-status-gui-overlap-failure.log`). Both owned processes were
   subsequently closed. Exact causation is unconfirmed; shared browser/evidence
   resources made that attempt unsuitable for acceptance.
3. The runner now refuses any existing test host and uses a unique WebView2
   profile/run ID. The test-only message-thread navigation report includes that
   ID; the reader rejects evidence from another run. No callback allocation or
   product RPC was added, and the original PCM/held-fragment/navigation assertions
   remain mandatory. Later results are recorded separately below.

This repair uses the existing browser/message-thread presentation layer; the
[Juce WebBrowserComponent native-integration API](https://docs.juce.com/master/classjuce_1_1WebBrowserComponent_1_1Options.html)
was consulted. It introduces no callback/audio-thread work and no new UI controls.
No package or version bump is needed. The separate user logo whitespace edit is
preserved outside the commit. Hardware/Apple/complete-engine parity, MIDI clock,
host-follow scheduling and recorded-loop persistence retain their earlier limits.


The first isolated run passed the existing mouse/navigation/audio/export/merge
checks, then the new MIDI computed-colour assertion sampled the original
`index.html` 0.3s background transition one channel step before its endpoint
(rgb(26,0,41) versus rgb(26,0,42)). The fixture now waits up to two seconds for
the exact original settled value; the final strict assertion is unchanged. No
CSS, comparison tolerance or product behavior was altered. Evidence:
`build/global-status-gui-transition-failure.log`.


Final acceptance:

- Ubuntu portable/core regressions passed 19/19 in 392.54s
  (`build/global-status-ubuntu-core.log`).
- The complete rebuilt native mouse/export/merge/raw-MIDI workflow passed at
  BOTH 64/128 (`build/global-status-gui.log`), including the new MIDI SUB > SUS >
  OD header precedence, exact settled original purple after mode-off during active
  overdub, YES (Loop 1)/NO label, unchanged playing-clip identity before commit,
  nonzero committed waveform, unchanged loop length and one retained Undo entry.
  The owned pre/post-navigation audio evidence tags match their current runs;
  `build/global-status-navigation.json` retains both nonzero-PCM reports.
- Windows Release Standalone/VST3/CLAP and processor/mouse host rebuilt
  (`build/global-status-windows-build.log` and
  `build/global-status-mouse-host-build.log`). Processor, official CLAP validator
  and VST3 pluginval passed 3/3 in 27.39s
  (`build/global-status-windows-formats.log`).
- Actual muted Standalone UI plus browser count-in/nudge/live-input/STUT gates
  passed (`build/global-status-standalone-ui.log`): twenty loop cards, native IPC/
  audio acknowledgements, no legacy AudioContext. Its owned window closed normally.
- Ubuntu Debug app/VST3/CLAP/LV2 and processor host rebuilt successfully
  (`build/global-status-linux-build.log`). Complete current controls.js/looper.js
  bytes match all seven Windows/Linux app/plugin modules;
  `build/global-status-embedded.json` records their module hashes/matches.
- `build/global-status-source.json` records hashes of all modified tracked files
  including index changes at the pre-commit test point. Existing DAW runners'
  dirtyTracked field only lists unstaged paths; module identity/hash evidence and
  this companion record identify the tested working tree. Local binaries include
  the retained user logo whitespace, which remains outside the source commit.

These are synthetic audio/UI tests; no physical controller/driver, browser
resampling/envelope, master-recorder or Apple acceptance follows from them.


Real REAPER 7.82 passed both current Windows VST3 and CLAP modules: seven target
stereo renders per format, parameter mutation/restore and actual RPP reopen.
Evidence: `build/global-status-reaper.log` and
`build/reaper-daw/20261023T121620-95770252/result.json`. This preserves the earlier
settled-PCM/isolated-profile/module-identity scope, not physical driver latency,
plugin editor interaction, MIDI clock or loop-audio project persistence.


Final Linux native processor/LV2 stereo ABI/official VST3 and LV2 pluginval/CLAP
validation passed 5/5 in 72.96s (`build/global-status-linux-formats.log`). Existing
compiler/DRI3 warnings, supported/unsupported vendor-test accounting and known
state-load warning remain documented; no checks were filtered or weakened.
Real Ardour 8.4/libardour LV2 passed stereo gain/mute and saved-session recall at
48kHz/64/128 using its isolated Dummy backend. Evidence:
`build/global-status-ardour.log` and
`build/ardour-daw/20261023T121936-4ed18cde/result.json`. These remain stereo peak/
ratio checks, not per-sample equality, GTK interaction or physical JACK tests.
All final owned test hosts closed normally. No installer/ZIP or version bump was
created; the separate user logo edit remains uncommitted.


## Original LIVE performance loop section — 23 October 2026

The LIVE PERFORMANCE tab previously contained no native loop cards. It now shows
the twenty original cards from unchanged `generateLiveLoopHTML`, with the original
`updateLivePerformanceState` priority. Header short actions use the existing
native mainAction/FIFO; Rvers shares the native reverse command. State/name/reverse
are coherent with the main loop view, including while the other view is hidden.
Cards are rebuilt on LIVE entry like the reference, then updated in place during
polling so user interaction is not lost to DOM replacement. The original updater's
quote/name values and colours are retained; native asynchronous finalization shows
SAVING. Unported effects stay disabled. Live master mixing, drones and samplers
remain unported: this increment restores only the loop section.

No C++ engine/DSP/audio-thread, MIDI dispatcher, APVTS or persisted-state format
changed. The reference assets and CMake/dependency pins are unchanged. The existing
WebBrowser/message-thread bridge remains the presentation/control route, as in
[JUCE's native-integration API](https://docs.juce.com/master/classjuce_1_1WebBrowserComponent_1_1Options.html).
Loop names are applied as the original updater's text value before painting,
without parsing session data as markup; static card structure retains the exact
original renderer. This follows the original updater and the documented
[textContent behavior](https://developer.mozilla.org/en-US/docs/Web/API/Node/textContent).

Reference gates:

- `build/live-loops-before.log`: missing native LIVE renderer reproduces the
  absent feature. Source-copy identity uses canonical LF, matching the existing
  adapter checks; an initial CRLF/LF-only comparison was corrected without
  changing original source or relaxing semantic/markup comparisons.
- `build/live-loops-after.log`: 400 exact original card-markup comparisons
  (20 tracks x ten states x reverse on/off), 1,600 unchanged `updateLiveLoop`
  comparisons (including empty, Unicode, quotes/markup and 4096-character names),
  and 121 original two-loop panel-priority combinations. Native finalization is
  checked separately. These compare presentation, not sound.
- Windows complete core/reference suite passed 19/19 in 267.97s
  (`build/live-loops-windows-core.log`). Independent connected audio audit passed
  126 cases / 965,826 samples, max error 7.394757495227111e-8 / unchanged tolerance
  1e-6 (`build/live-loops-audio.log`).

The separate user logo whitespace edit is retained outside the commit. No package,
installer or version bump is needed for this restoration. Physical controllers/
drivers, complete effects/synth/clock/project parity and Apple validation retain
their earlier limits; recorded-loop persistence remains unported.


Completed native acceptance for this increment:

- Ubuntu complete core/reference suite passed 19/19 in 392.05s
  (`build/live-loops-ubuntu-core.log`).
- Full production processor/editor mouse/export/merge/raw-MIDI workflows passed
  at BOTH 64/128 (`build/live-loops-gui.log`). New trusted LIVE tests record and
  finish through the original header, require audible nonzero synthetic PCM,
  stop and keyboard-play from LIVE, check shared reverse/name state in the main
  view, literal name/updater semantics, disabled effects, nested-label isolation,
  tab changes without audio commands, and unchanged clip length/identity/history.
  Original main-loop hold/delete/sync/navigation/keyboard/MIDI gates remain intact.
- `build/native-live-loops-64.png` was visually inspected: original three-column
  layout, loop cards/state colours and controls remain coherent. Both buffer-size
  screenshots are retained. This is not a browser pixel-difference certification.
- Windows Release app/VST3/CLAP, mouse host and processor test rebuilt
  (`build/live-loops-windows-build.log`). Native processor, official VST3 pluginval
  and CLAP validator passed 3/3 in 27.70s
  (`build/live-loops-windows-formats.log`).
- Actual muted Standalone and unchanged-browser count-in/nudge/live-input/STUT
  gates passed (`build/live-loops-standalone-ui.log`), including IPC acknowledgements,
  twenty main loop cards and no legacy AudioContext. The owned host closed normally.
- Ubuntu Debug app/VST3/CLAP/LV2 and processor test rebuilt
  (`build/live-loops-linux-build.log`). Exact complete controls.js, loop-view.js
  and looper.js bytes match all seven app/plugin modules;
  `build/live-loops-embedded.json` records module hashes/matches.
- `build/live-loops-source.json` records pre-commit hashes of all modified tracked
  files, including staged changes. Local binaries include the retained user logo,
  which remains outside the source commit. Two staged whitespace diagnostics are
  exact original renderer spaces, verified separately in
  `build/live-loops-reference-whitespace.json`; other staged lines are clean.
- Real REAPER 7.82 VST3 and CLAP each passed seven target stereo renders,
  parameter mutation/restore and RPP reopen. Evidence:
  `build/live-loops-reaper.log` and
  `build/reaper-daw/20261023T145533-3f9bade3/result.json`.

The real DAW regression remains settled monitor PCM/state recall, not LIVE-card
interaction inside a DAW or recorded-loop project storage. The new LIVE interaction
gate uses the production processor/editor in the synthetic no-device host. No
physical-driver latency or full browser resampling/envelope claim follows.


Final Linux native processor/LV2 stereo ABI/VST3 and LV2 pluginval/CLAP gates
passed 5/5 in 72.46s (`build/live-loops-linux-formats.log`). Real Ardour 8.4 LV2
stereo gain/mute and saved-session recall passed at 48kHz/64/128 with its isolated
Dummy backend (`build/live-loops-ardour.log` and
`build/ardour-daw/20261023T150042-72d17ee7/result.json`). Existing vendor skips,
known state-load warning and compiler/DRI3 diagnostics are retained. These are
host-interface/monitor regression gates, not physical JACK, GTK interaction or
per-sample Ardour PCM equality. All final owned test hosts closed normally.
No package or version bump was created.


## LIVE loop mixer controls — 23 October 2026

Only the original twenty loop volume/M/S strips are enabled in the existing LIVE
mixer. No DSP/APVTS/schema/original-asset change or package is part of this gate.
Master/input/drone/sampler strips, RMS/held-peak meters and live-slider MIDI learn
remain pending. Main-loop MIDI mappings and the user's logo edit are retained.

`build/live-mixer-frontend.log` passes 1,200 complete unchanged renderLive markup
cases across tracks, volumes, mute/solo ownership and empty/recorded loops; copied
method identity is required. The production wheel handler passes 48 boundary/
disabled cases against the unchanged original inline handler. Original whitespace
is retained (`build/live-mixer-reference-whitespace.json`). The first upgraded
wheel-oracle fixture incorrectly ran repeated inline handlers in a shared lexical
scope; its const redeclaration failure is retained in
`build/live-mixer-oracle-fixture-failure.log`. Giving each invocation the original
handler's function scope fixed the fixture without changing product or reference.

Windows portable regressions passed 19/19 in 267.96s and Ubuntu 19/19 in 398.02s
(`build/live-mixer-windows-core.log`, `build/live-mixer-ubuntu-core.log`). The
independent committed-audio audit passes 126 cases / 965,826 samples, maximum
absolute error 7.394757495227111e-8 versus 1e-6 tolerance
(`build/live-mixer-audio.log`). Its existing exclusions, including resampling,
playback envelopes and physical latency, remain unchanged.

The complete Windows WebView2 Mouse/Export/Merge/MIDI workflow passed at 64/128
with synthetic audio, including trusted focused LIVE gain Home/End, gain-zero/mute/solo silence, nonzero recovery,
solo transfer/release, main fader mirroring, stable fader identity across tab entry
and unchanged recorded clip/history (`build/live-mixer-gui.log`). All owned mouse
hosts closed normally. The 64 screenshot was visually inspected: original
vertical strip layout/colours, no pointer overlay. This is not pixel equality.
Screenshots are `build/native-live-mixer-64.png` and `-128.png`.

Initial real pointer tests failed because scrollIntoView's default inline axis
left vertical-writing-mode faders behind the original sticky header. Block-end
alignment alone also failed. Both logs are retained as
`build/live-mixer-gui-scroll-failure.log` and
`build/live-mixer-gui-end-scroll-failure.log`. Separate read-only real DOM geometry
checks (`build/live-mixer-geometry.log`, `build/live-mixer-geometry-both.log`)
confirmed that centring both logical axes reveals the actual input. The final
fixture keeps strict elementFromPoint hit-testing and trusted pointer/key input;
no original CSS/layout or audio assertion was changed to obtain the pass.

All three working-tree adapters match the embedded bytes in all seven Windows/
Linux app/plugin modules (`build/live-mixer-embedded.json`). Those binaries include
the user's separate uncommitted logo whitespace; it is excluded from the commit.

Windows frontend/native processor/VST3 pluginval/CLAP acceptance passed 4/4 in
31.74s (`build/live-mixer-windows-formats.log`). The actual muted Standalone plus
original browser count-in/nudge/live-input/STUT comparisons and NativeUiSmoke
passed (`build/live-mixer-standalone-ui.log`). REAPER 7.82 real VST3 and CLAP
seven-case settled stereo rendering, track-state and saved-project recall passed
(`build/live-mixer-reaper.log`,
`build/reaper-daw/20261023T154030-4c9dfd63/result.json`). Existing user REAPER PID
8316 was preserved. The runner's dirtyTracked field lists unstaged paths only;
companion source/module hashes identify the full tested working tree. These host
checks do not cover recorded-loop persistence, new LIVE editor interaction inside
REAPER, MIDI clock, host-follow scheduling or physical driver latency.

Final Linux frontend/native processor/LV2 stereo ABI/LV2 and VST3 pluginval/CLAP
acceptance passed 6/6 in 79.54s (`build/live-mixer-linux-formats.log`). Ardour 8.4
actual LV2 gain/mute and saved-session recall passed with isolated Dummy stereo
inputs at 48kHz/64/128 (`build/live-mixer-ardour.log`,
`build/ardour-daw/20261023T154417-8e1536e5/result.json`). Existing vendor skips,
known state-load warning and DRI3/compiler diagnostics remain visible. This is
libardour session-engine validation, not GTK pointer testing, physical JACK or
per-sample Ardour PCM comparison. No new physical hardware evidence is claimed.
All final owned hosts closed normally; no version bump or package was created.


## LIVE loop-volume MIDI Learn — 23 October 2026

The native explicit ID resolver omitted the original LIVE volume faders. The
pre-fix unchanged-original setupTouchSafety regression failed with -1 instead of
33 (`build/live-midi-before.log`). The repair aliases all twenty LIVE volume IDs
to existing 33..52 targets, with shared learn outline/CC description. No DSP,
APVTS/schema, target table, original asset, CMake dependency or automatic user
assignment change is introduced. Native manual additions describe the shared
binding in English, Spanish and Portuguese.

`build/live-midi-frontend.log` passes 20 unchanged original context-menu gestures,
existing main volume/pan target guards and 720 actual production status-renderer
cases across target/CC/channel/learning/bound states. Rendering leaves mapping
state unchanged. Existing original markup/status/wheel/gesture oracles remain.

The focused MIDI workflow passed at 64 and 128 in separate owned Windows WebView2
synthetic hosts (`build/live-midi-focused-gui.log`). Real pointer right-click
learns a zero-valued CC; raw MidiBuffer packets on channels 16/1 change the shared
native gain and both DOM aliases while LIVE retains focus. Actual output becomes
silent at zero and returns nonzero at positive values. A held real pointer edit
retains display ownership; MIDI display resumes on release. Shared learn outline,
Escape/ten-second timeout, Shift-right-click clear, Save Mappings/editor reload
and cleared-CC isolation pass. Learning/CC movement never sends transport
commands; recorded waveform ID/length/undo history are retained. Other fixture
loops are excluded using the existing solo control before output checks.

Windows portable regressions passed 19/19 in 260.98s and Ubuntu 19/19 in 399.41s
(`build/live-midi-windows-core.log`, `build/live-midi-ubuntu-core.log`). The
independent committed-stereo-audio audit passes 126 cases / 965,826 samples with
maximum absolute error 7.394757495227111e-8 against 1e-6 tolerance
(`build/live-midi-audio.log`), retaining its original envelope/resampling/physical
latency exclusions. This increment introduces no new physical hardware evidence.
Master/input/drone/sampler mixer routes, RMS/held-peak meters, other MIDI targets,
clock/host-follow scheduling, recorded-loop persistence and Apple acceptance
remain pending. Existing archives remain explicitly named f5789c1289cd snapshots;
no new package or version bump is required for this adapter repair.

The remaining Mouse/Export/Merge regressions passed at 64/128 in separate fresh
hosts via -SkipMidi (`build/live-midi-regression-gui.log`); MIDI had independently
passed via -MidiOnly. All final owned mouse hosts closed normally. Windows
frontend/native processor/VST3 pluginval/CLAP passed 4/4 in 32.37s
(`build/live-midi-windows-formats.log`). Actual muted Standalone, original browser
count-in/nudge/live-input/STUT and NativeUiSmoke passed
(`build/live-midi-standalone-ui.log`). Real REAPER 7.82 VST3/CLAP seven-case
settled stereo rendering and parameter/project recall passed
(`build/live-midi-reaper.log`,
`build/reaper-daw/20261023T171301-b3986847/result.json`).

Linux frontend/native processor/LV2 stereo ABI/VST3 and LV2 pluginval/CLAP passed
6/6 in 79.81s (`build/live-midi-linux-formats.log`). Actual Ardour 8.4 LV2
gain/mute and saved-session recall passed with isolated Dummy inputs at
48kHz/64/128 (`build/live-midi-ardour.log`,
`build/ardour-daw/20261023T171733-52c83b91/result.json`). Existing vendor skips,
known state-load warning and compiler/DRI3 diagnostics remain. These are scoped
host-interface/monitor checks, not GTK pointer interaction, per-sample Ardour PCM,
physical JACK/ASIO, MIDI clock or recorded-loop project persistence acceptance.

All four native adapters, including midi.js, match embedded bytes in all seven
Windows/Linux app/plugin modules (`build/live-midi-embedded.json`). Companion
`build/live-midi-source.json` records the complete modified tree including staged
files and the user's logo whitespace. Tested binaries include that local logo
override, which is excluded from the source commit.


## Direct loop-action MIDI Learn — 23 October 2026

This is the explicitly requested native right-click transport-learning extension.
Original main action button markup/CSS and original mapping fields remain intact.
AionMidiLearnTarget aliases the twenty primary actions to existing 0..19 bindings,
retaining main/LIVE continuous aliases. There is no DSP, native callback, target
table, APVTS/schema, SDK/dependency or original-asset change. Manual instructions
are added to the existing native MIDI help in English/Spanish/Portuguese. Keyboard
SUS was audited and remains pending its full held-action/release/cancellation
implementation; no status-only keyboard toggle was enabled.

The pre-fix regression fails because direct action routing is absent
(`build/action-midi-before.log`). `build/action-midi-frontend.log` passes all twenty
action aliases and sixty retained slider aliases, 320 actual production context-
menu target/modifier/disabled cases, and 640 actual mapping/action status cases
across target/kind/number/channel/learning. Mapping state is immutable during
rendering. Existing unchanged-JS MIDI/gesture/audio oracles are retained; the
new direct gesture itself has no unchanged-JS context-menu oracle.

The focused production processor/editor MIDI gate passes at 64/128 with raw
MidiBuffer injection (`build/action-midi-focused-gui.log`). Real pointer actions
cover same-target mapping-field handoff, Note/CC/PC consume-on-learn, PC triple-
burst debounce, CC zero/rearm/positive-repeat, actual stopped silence and playing
nonzero PCM, a 4.2s held right-click without cut/delete, Escape, Save Mappings,
editor reload/cancellation, Shift-right-click clearing and former-controller
isolation. Original field/button learning status stays aligned; clip identity,
length and undo depth are preserved. These are synthetic audio/MIDI observations,
not new physical controller or driver evidence.

The final disabled-expression fixture is repaired: it restores an assigned CC
at a nonmaximum volume before disabling MIDI and sending a later maximum value.
The preceding LIVE fixture had cleared that binding and left maximum volume,
making the prior final assertion ineffective. Product disable behavior and all
tolerances are unchanged.

Windows portable regressions pass 19/19 in 261.61s and Ubuntu 19/19 in 401.77s
(`build/action-midi-windows-core.log`, `build/action-midi-ubuntu-core.log`). The
independent committed-audio audit passes 126 cases / 965,826 samples with maximum
absolute error 7.394757495227111e-8 against 1e-6 tolerance
(`build/action-midi-audio.log`), retaining its original resampling/envelope/latency
exclusions. No package/version bump is needed for this increment; prior archives
remain their explicitly named source snapshots.

The remaining Mouse/Export/Merge workflows passed at 64/128 via -SkipMidi
(`build/action-midi-regression-gui.log`), with MIDI independently checked via
-MidiOnly. All owned mouse hosts closed normally. Windows frontend/native
processor/VST3 pluginval/CLAP passed 4/4 in 32.75s
(`build/action-midi-windows-formats.log`). Actual muted Standalone, original
browser count-in/nudge/live-input/STUT and NativeUiSmoke passed
(`build/action-midi-standalone-ui.log`). Real REAPER 7.82 VST3/CLAP seven-case
settled stereo rendering and parameter/project recall passed
(`build/action-midi-reaper.log`,
`build/reaper-daw/20261023T182308-ce46c863/result.json`).

Linux frontend/native processor/LV2 stereo ABI/VST3 and LV2 pluginval/CLAP passed
6/6 in 81.33s (`build/action-midi-linux-formats.log`). Actual Ardour 8.4 LV2
gain/mute and saved-session recall passed at 48kHz/64/128 using isolated Dummy
inputs (`build/action-midi-ardour.log`,
`build/ardour-daw/20261023T182855-2ae921cb/result.json`). Existing vendor skips,
known state-load warning and compiler/DRI3 diagnostics remain. These host checks
do not establish GTK pointer behavior, per-sample Ardour PCM, physical JACK/ASIO,
MIDI clock, recorded-loop persistence or Apple acceptance.

All four native adapters match embedded bytes in seven Windows/Linux app/plugin
modules (`build/action-midi-embedded.json`). Companion
`build/action-midi-source.json` identifies the complete modified tested tree,
including the separate user logo whitespace. Tested binaries contain that local
logo override; the source commit excludes it. No new physical hardware evidence
is claimed. Other action targets, keyboard SUS and broader feature parity remain
pending; this completed direct-learning increment does not complete the migration.


## LIVE header direct MIDI Learn — 23 October 2026

This bounded increment extends the requested native direct-learning gesture to
the original twenty LIVE loop headers and their child elements. It shares the
main action/mapping-field binding and outline, including recreated cards; FX,
card backgrounds and disabled main actions are excluded. No original assets,
DSP/C++ callback, APVTS/state schema, dependency or build configuration changed.
Manual instructions are updated in English/Spanish/Portuguese.

The initial regression fails on missing LIVE element routing
(`build/live-action-midi-before.log`); the repaired frontend suite passes 560
production context-menu hit/modifier/disabled cases and the retained 320 main
context-menu / 640 immutable mapping/action status cases with added LIVE outline
checks (`build/live-action-midi-frontend.log`). These are native extension tests;
unchanged original JS markup/gesture/audio oracles remain separate gates.

The production processor/editor MIDI workflow passes at both 64 and 128 samples
(`build/live-action-midi-focused-gui.log`). Real CDP pointer actions on LIVE
headers and nested header content learn Note/CC/PC, consume the learn event,
share all three outlines, and control actual stopped silence/playing nonzero
PCM. PC triple-burst filtering, CC zero/rearm/repeated-high handling, a 4.2s
right-hold without transport/deletion, Escape, tab rebuild, save/editor reload,
Shift-right-click clear and former-CC isolation pass. Clip identity, length and
undo depth remain unchanged. All preceding main-action, field, expression and
MIDI regression checks are retained. No new physical hardware evidence is claimed.

Windows portable regressions pass 19/19 in 262.82s
(`build/live-action-midi-windows-core.log`). The first invocation omitted the
required Release configuration, so no tests ran; its incomplete log is retained
as `build/live-action-midi-windows-core-invocation.log` and is not a feature pass.
The independent committed-audio audit passes 126 cases / 965,826 samples with
maximum absolute error 7.394757495227111e-8 against 1e-6 tolerance
(`build/live-action-midi-audio.log`); resampling, envelopes and physical latency
retain their documented exclusions.

Ubuntu portable regressions pass 19/19 in 401.53s
(`build/live-action-midi-ubuntu-core.log`). This includes the unchanged original
UI byte-identity, MIDI/gesture/frontend and committed DSP/sync/edit/history gates.
The Windows and Ubuntu results establish their tested scope, not full parity,
physical audio latency or Apple-platform readiness.

The remaining mouse/export/merge workflows pass at 64/128 in separate owned
hosts via -SkipMidi (`build/live-action-midi-regression-gui.log`); MIDI had passed
independently via -MidiOnly. All final owned mouse hosts close normally. Windows
frontend/native processor/VST3 pluginval/CLAP passes 4/4 in 31.56s
(`build/live-action-midi-windows-formats.log`). Actual muted Standalone, original
browser count-in/nudge/live-input/STUT and NativeUiSmoke pass
(`build/live-action-midi-standalone-ui.log`). Real REAPER 7.82 VST3/CLAP seven-case
settled stereo rendering and parameter/project recall pass
(`build/live-action-midi-reaper.log`,
`build/reaper-daw/20261023T190557-ceacc8da/result.json`). These host checks retain
their synthetic-audio/settled-monitor/parameter-state scope; they do not establish
recorded-loop project persistence or physical drivers.

Linux frontend/native processor/LV2 stereo ABI/VST3 and LV2 pluginval/CLAP passes
6/6 in 80.16s (`build/live-action-midi-linux-formats.log`). Actual Ardour 8.4 LV2
gain/mute and saved-session recall passes with isolated Dummy inputs at
48kHz/64/128 (`build/live-action-midi-ardour.log`,
`build/ardour-daw/20261023T190931-83011cad/result.json`). Existing vendor skips,
known JUCE state-load warning and compiler/DRI3 diagnostics remain. The Ardour
scope excludes GTK pointer interaction, per-sample PCM, physical JACK/ASIO,
recorded-loop persistence and MIDI clock/host-follow scheduling.

All four native adapters match embedded bytes in seven Windows/Linux app/plugin
modules (`build/live-action-midi-embedded.json`); recorded module hashes remain
unchanged after validation. Companion `build/live-action-midi-source.json`
identifies the complete modified tested tree, including the separate user logo
whitespace. Tested binaries include that local logo override, which is excluded
from the source commit. Physical LIVE direct-learning, broader device/driver
behavior, keyboard SUS, other controls/engines and Apple acceptance remain
pending. No new package or version bump is required; existing archives remain
explicitly named snapshots of their earlier source commit.


## Main loop-header/state MIDI Learn — 23 October 2026

This increment extends requested direct learning to the existing main header,
number and state surface, sharing primary-button/LIVE/mapping-field bindings.
Nested controls, waveform, name editing, FX/control areas and card backgrounds
stay excluded; disabled primary actions cannot start header learning. Original
markup/CSS, DSP/C++ callback, APVTS/state schema and build/dependencies are unchanged.
Existing native MIDI help is updated in English/Spanish/Portuguese.

The initial regression reproduces rejected main-header learning
(`build/main-header-midi-before.log`). The fixed frontend suite passes 1,520
actual production context-menu main-header/state/modifier/disabled cases and
invalid-owner checks (`build/main-header-midi-frontend.log`); existing 320/560
main-button/LIVE routing and 640 immutable shared-status cases remain, with
main-header outlines added. The native extension gesture is tested separately
from preserved original markup, performance and audio comparisons.

The focused production processor/editor workflow passes at 64/128 samples
(`build/main-header-midi-focused-gui.log`). Trusted pointer right-clicks on the
main number/state/symbol/summary learn Note/CC/PC and share primary/main-header/
LIVE/mapping-field outlines. Learning consumes the first packet and sends no
performance command. Subsequent Note, PC triple-burst and CC zero/rearm/repeat
checks observe actual stopped silence/playing nonzero PCM. Holding right-click
on STOP for 4.2s does not delete or cut. Escape, Save Mappings, reload cancellation,
Shift-right-click clearing and former-CC isolation pass with unchanged clip
identity, length and undo depth. All prior main/LIVE/continuous/held-MIDI gates
remain. These synthetic-audio/MIDI observations add no physical hardware evidence.

Windows portable regressions pass 19/19 in 258.75s
(`build/main-header-midi-windows-core.log`). The independent committed-audio
audit passes 126 cases / 965,826 samples with maximum absolute error
7.394757495227111e-8 against 1e-6 tolerance (`build/main-header-midi-audio.log`);
resampling, transition envelopes and physical latency retain their exclusions.

Ubuntu portable regressions pass 19/19 in 407.31s
(`build/main-header-midi-ubuntu-core.log`), retaining unchanged original asset,
MIDI/frontend/gesture and DSP/sync/edit/history comparisons. Both portable suites
establish their recorded scope, not full functionality, browser/driver timing or
Apple-platform acceptance. No package/version bump is needed for this adapter.

The remaining mouse/export/merge workflows pass at 64/128 via -SkipMidi in
separate owned hosts (`build/main-header-midi-regression-gui.log`), with MIDI
independently checked via -MidiOnly. All final owned hosts close normally.
Windows frontend/native processor/VST3 pluginval/CLAP passes 4/4 in 32.66s
(`build/main-header-midi-windows-formats.log`). Actual muted Standalone, original
browser count-in/nudge/live-input/STUT and NativeUiSmoke pass
(`build/main-header-midi-standalone-ui.log`). Real REAPER 7.82 VST3/CLAP seven-case
settled stereo rendering and parameter/project recall pass
(`build/main-header-midi-reaper.log`,
`build/reaper-daw/20261023T193215-7424ab3a/result.json`). These retain their
synthetic-audio/settled-monitor/parameter-state scope, excluding recorded-loop
project persistence and physical driver/latency acceptance.

Linux frontend/native processor/LV2 stereo ABI/VST3 and LV2 pluginval/CLAP passes
6/6 in 80.29s (`build/main-header-midi-linux-formats.log`). Actual Ardour 8.4 LV2
gain/mute and saved-session recall passes with isolated Dummy inputs at
48kHz/64/128 (`build/main-header-midi-ardour.log`,
`build/ardour-daw/20261023T193538-7454e500/result.json`). Existing vendor skips,
known JUCE state-load warning and compiler/DRI3 diagnostics remain. This excludes
GTK pointer interaction, per-sample Ardour PCM, physical JACK/ASIO, recorded-loop
persistence and MIDI clock/host-follow scheduling.

All four native adapters match embedded bytes in seven Windows/Linux app/plugin
modules (`build/main-header-midi-embedded.json`); their hashes remain unchanged
after validation. Companion `build/main-header-midi-source.json` identifies the
complete modified tested tree, including the separate user logo whitespace.
Tested binaries include that local logo override, which is excluded from this
source commit. Physical main-header learning, other controls/engines, keyboard SUS
and Apple acceptance remain pending. Existing archives retain their explicitly
named earlier source snapshots; no new package/version bump is introduced.


## Immediate MIDI Note recovery after cancellation — 23 October 2026

Original stopAll clears held input entries; native cancellation instead left Note
entries active/suppressed, blocking a fresh press until NoteOff or ten seconds.
The failing core test is retained in `build/midi-panic-before.log`; the actual
unchanged-JS StopAll/NoteOn comparison fails in `build/midi-panic-oracle-before.log`.
The first test build had a mixed-width auto declaration; it was corrected before
recording those behavioral failures (`build/midi-panic-red-build.log`,
`build/midi-panic-red-build-fixed.log`). No production behavior was changed to
compensate for that test compilation error.

A preliminary same-key-only repair passed core/processor/editor checks but the
new full 64-Note-pool/different-key case failed
(`build/midi-panic-capacity-before.log`). It was replaced by immediate retirement
of cancelled Note entries after momentary restoration. CC entries retain their
zero-before-positive ownership; active Note repeats/hold deadlines and PC burst
rules remain. Final acceptance below uses `midi-panic-final-*` logs; preliminary
results are not acceptance for the final implementation. Original UI/assets,
MIDI bindings, APVTS/state schema, SDKs and CMake remain unchanged.

The final focused core and unchanged-JS oracle pass
(`build/midi-panic-final-core-focused.log`, `build/midi-panic-final-oracle.log`).
Coverage includes 224 play/stop and 28 exact-mute recovery cases at 64/128 across
STOP ALL, MIDI Stop, CC120/123, device cancellation, overflow and disable/enable,
using nonzero captured stereo input and byte-identical stored WAV before/after.
Full 64-Note-pool/different-key recovery, late old releases, explicit CC zero rearm,
ordinary repeat debounce and no stale cut/delete/clip/history mutation also pass.

The actual JUCE MidiBuffer processor gate passes both block sizes and channels
1/16 with panic at offset 18, fresh NoteOn at 19, repeated NoteOn at 20 and NoteOff
at 21; it verifies one action, actual stopped silence/nonzero playing PCM, retained
clip/history and fresh SUS release (`build/midi-panic-final-processor-focused.log`).
Tracked processBlock new/delete stays zero. This is not an arbitrary malloc or
physical driver/latency certificate.

The final owned WebView2 editor workflow passes at 64/128
(`build/midi-panic-final-focused-gui.log`): real mouse STOP ALL during held SUS
play, immediate same-key replay without old NoteOff, ordinary repeat and new release,
plus raw same-block MIDI Stop/CC120/123 on channels 1/16. Actual output is silent or
nonzero as appropriate, and clip identity/length/undo depth remains. All previous
MIDI Learn, expression, field/button/main/LIVE alias and held-MIDI gates remain.

Windows portable regressions pass 19/19 in 284.92s
(`build/midi-panic-final-windows-core.log`). ASan/UBSan with leak detection passes
core_realtime_contract/js_native_midi 2/2 in 73.35s
(`build/midi-panic-final-asan.log`). The independent committed-audio audit passes
126 cases / 965,826 samples at maximum absolute error 7.394757495227111e-8 against
1e-6 tolerance (`build/midi-panic-final-audio.log`), retaining resampling/envelope/
physical latency exclusions. No new physical controller evidence is claimed.

Ubuntu final portable regressions pass 19/19 in 418.49s
(`build/midi-panic-final-ubuntu-core.log`). All original asset bytes and unchanged
JS MIDI/gesture/DSP/sync/edit/history gates remain. Sanitizer, portable and synthetic
processor/editor results do not establish physical controller/driver or Apple
acceptance, MIDI clock or full feature parity.

The remaining mouse/export/merge workflows pass at 64/128 via -SkipMidi in
separate owned hosts (`build/midi-panic-final-regression-gui.log`); MIDI had passed
independently via -MidiOnly. All final owned mouse hosts close normally. Windows
frontend/native processor/VST3 pluginval/CLAP passes 4/4 in 31.06s
(`build/midi-panic-final-windows-formats.log`). Actual muted Standalone, original
browser count-in/nudge/live-input/STUT and NativeUiSmoke pass
(`build/midi-panic-final-standalone-ui.log`). Real REAPER 7.82 VST3/CLAP seven-case
settled stereo rendering and parameter/project recall pass
(`build/midi-panic-final-reaper.log`,
`build/reaper-daw/20261023T203029-8724911b/result.json`). Host checks retain their
synthetic/settled-monitor/parameter-state scope; full recorded-loop project save,
physical latency/driver behavior and full DAW MIDI sessions remain separate.

Final Linux frontend/native processor/LV2 stereo ABI/VST3 and LV2 pluginval/CLAP
passes 6/6 in 80.54s (`build/midi-panic-final-linux-formats.log`). Actual Ardour 8.4
LV2 gain/mute and saved-session recall passes at 48kHz/64/128 with isolated Dummy
inputs (`build/midi-panic-final-ardour.log`,
`build/ardour-daw/20261023T203420-5fcbb5fd/result.json`). Existing vendor skips,
known JUCE state-load warning and compiler/DRI3 diagnostics remain. This does not
establish GTK pointer interaction, per-sample Ardour PCM, physical JACK/ASIO,
recorded-loop persistence or MIDI clock/host-follow scheduling.

All four native adapters match embedded bytes in seven rebuilt Windows/Linux
app/plugin modules (`build/midi-panic-final-embedded.json`); module hashes remain
unchanged after host validation. Companion `build/midi-panic-final-source.json`
records the complete modified tested tree, including the user's separate logo
whitespace. Tested binaries include that local logo override, excluded from the
source commit. No new package or version bump is introduced; older archives remain
their explicitly named source snapshots. This closes the tested cancellation
repair, not the wider migration or physical/Apple acceptance gates.

## Space STOP ALL repeat repair — 23 October 2026

Continued from 69bf493. The unchanged App.handleKeyboardPress comparison failed
because native Space handling followed the generic repeat guard
(`build/space-repeat-before.log`). The actual 64-sample WebView2 test also failed:
a new MIDI-triggered clip kept producing audio after repeated Space keydown
(`build/space-repeat-gui-before.log`). Move that existing STOP ALL fallback before
the guard, retaining Ctrl/Meta/Alt, text/editable-content protection and normal
loop-key repeat debounce. No C++ DSP, UI markup/CSS, bindings or state schema change.

The repaired source passes 130 unchanged-dispatcher focus/modifier/Shift/repeat
cases (`build/space-repeat-frontend.log`). Windows portable CTest passes 19/19 in
288.32s and Ubuntu portable CTest passes 19/19 in 453.14s
(`build/space-repeat-windows-core.log`, `build/space-repeat-ubuntu-core.log`).
These retain original asset hashes, MIDI/hold/cancellation, worker, sync and audio
oracles and callback new/delete checks. The independent committed-audio audit
passes 126 cases / 965,826 samples, maximum absolute error 7.394757495227111e-8
against the unchanged 1e-6 tolerance (`build/space-repeat-audio.log`); browser
resampling/envelopes, physical latency and other pending engines remain excluded.

The complete native synthetic MIDI/editor workflow passes at 64/128
(`build/space-repeat-midi-gui.log`). The new trusted repeat case focuses the
original volume range and STOP ALL button, starts real nonzero MIDI playback
between three repeated keydowns per focus target, and requires each to settle to
stopped/silent. Clip identity, length and Undo history remain unchanged. Existing
MIDI Learn, panic, PC burst/CC edges, SUS release and reload tests remain enabled.

The broader mouse regression initially failed its first trusted wheel probe at
MASTER: no browser wheel event arrived. Logs are retained in
`build/space-repeat-mouse-gui.log`, `build/space-repeat-mouse-gui-retest.log` and
`build/space-repeat-mouse-diagnostic.log`. Diagnostics record the correct DOM hit,
visible/focused document and successful real wheel delivery on the owned test
page (`build/space-repeat-wheel-probe.log`). Explicitly activate that page with
standard CDP Page.bringToFront before compositor-routed wheel input; retain the
exact one-event, trust, target, focus, preventDefault and audio/value assertions.
The test now includes selector/coordinate evidence on failure. This is fixture
preparation, not a change to product wheel behavior or a relaxed acceptance gate.
The full mouse/keyboard/export/merge workflow then passes at both 64 and 128
samples (`build/space-repeat-mouse-active-page.log`), including original queued
STOP presentation, held STOP deletion/cancellation, static/dynamic sync, focused
controls, LIVE cards/mixer, exact exported PCM and merge/history isolation. All
owned acceptance hosts close normally; the separate diagnostic host was also
closed normally after its page activation probe.

Actual muted Standalone NativeUiSmoke plus original browser count-in/nudge/live
input/STUT comparisons pass (`build/space-repeat-standalone-ui.log`). Windows
frontend/native MIDI processor/CLAP validator/VST3 pluginval passes 4/4 in 32.60s
(`build/space-repeat-windows-formats.log`), retaining existing vendor skips and
the known JUCE state-load warning. No new real DAW or hardware evidence is implied.

Windows Standalone/VST3/CLAP and the synthetic host rebuilt successfully
(`build/space-repeat-windows-build.log`). Current native adapters byte-match all
four binaries (`build/space-repeat-embedded.json`), including the user's separate
logo whitespace override, which remains excluded from this commit. No package or
version bump is introduced. This increment adds no new physical controller,
ASIO/JACK/CoreAudio, real DAW session, Linux GTK keyboard or Apple acceptance;
the earlier REAPER/Ardour results above retain their stated scope. Keyboard SUS,
custom mappings, MIDI clock/host-follow, projects, synths and effects remain pending.
Space delivery to a plugin editor versus the DAW's own transport remains a
separate host shortcut/focus acceptance check.

## Independent MUTE MIDI Learn acceptance — 24 October 2026

Source base eee8bcffe954656e83ca2586d5d2ff9015adc3cf plus this increment. New
MUTE targets 73..92 retain original audio/UI behavior and do not advance other
migration features. Physical new MUTE controller response remains unverified.

Final evidence under build/midi-mute-final-*:
- Windows core: 19/19, 328.77s (windows-core.log).
- Ubuntu core: 19/19, 413.37s (ubuntu-core.log).
- ASan/UBSan core/MIDI: 2/2, 120.85s, leak/error halt enabled (asan.log).
- Windows frontend/native processor/CLAP/VST3: 4/4, 33.55s
  (windows-formats.log); production processor also passed its focused run.
- Linux frontend/native processor/LV2 audio/CLAP/LV2+VST3 pluginval:
  6/6, 82.70s (linux-formats.log). Existing vendor skips/warnings retained.
- Actual WebView2 MIDI pointer/Learn/save/muted-editor-reload and full remaining
  mouse/keyboard/export/merge checks passed at both 64/128
  (midi-gui.log, mouse-gui.log). Muted Standalone NativeUiSmoke and original
  browser count-in/nudge/live-input/STUT checks passed (standalone-ui.log).
- Independent audio audit: 126 cases, 965826 samples, maximum absolute error
  7.394757495227111e-8 against tolerance 1e-6 (audio.log). Scope is committed
  capture/OD/SUB through center pan, not every envelope/resampling/hardware path.

Real REAPER 7.82 VST3/CLAP checks passed seven settled stereo cases per format,
parameter/track state and project recall: build/reaper-daw/20261024T110919-bb34cf0d/
result.json (reaper.log). Real Ardour 8.4 libardour/Lua LV2 checks passed Dummy
48kHz 64/128 stereo gain/mute/peak and session recall:
build/ardour-daw/20261024T112250-8f7ecebe/result.json (ardour.log). These DAW
checks do not certify recorded-loop persistence, MUTE MIDI sessions or GTK
pointer interaction. Ardour CLAP hosting is not covered; the CLAP validator is
separate. No new physical ASIO/JACK/CoreAudio or Apple acceptance is claimed.

New focused coverage includes 240 real-PCM/WAV cases, exact capture preservation,
sample-offset mute/unmute, held main SUS ownership during Learn/ack/remap/clear,
CC zero rearming after reassignment, state versions 1/2/3 and future rejection,
288 unchanged-reference action packets, 400 context-menu and 2880 status cases.
Callback allocation checks remain enabled. Reproduced failures and original
reference boundaries are recorded in REFERENCE_AUDIT.md.

Windows and Linux development binaries rebuilt; embedded assets and hashes are
recorded in build/midi-mute-final-embedded.json and source hashes in
build/midi-mute-final-source.json. The local test binaries include the separate
user logo whitespace override; that override is excluded from the feature
commit. No package/version bump. Other MIDI controls, clock/host-follow,
projects, synths/effects and physical hardware/platform gates remain pending.

## Strict actual REAPER MIDI gate — 24 October 2026

This increment adds validation, not a new product feature or a VST3 repair.
Baseline source is 2a89d5b358c6b6bc0edf2c5fe351b369c7efc183 with the local logo
override preserved. No production C++/DSP/frontend asset changed. No package,
installer, dependency patch or version bump was produced. The previously passing
synthetic MUTE tests did not establish actual VST3 Program Change delivery.

Final CLAP result: build/reaper-daw/20261024T123303-2920e9ce/result.json is passed,
with the existing seven monitor/state/recall PCM cases plus four MIDI cases:
Note, CC, PC and project-reopened PC assignment. Main Note36 records/finishes a
fresh loop from synthetic stereo; MUTE channel-16 Note0/CC0/PC0 sends repeated
packets, releases/rearms and unmutes. Capture remains monitor-silent, playback is
nonzero in both channels, the mute window is exactly zero and recovery is nonzero.
For all four cases, the recovered 150ms-loop phase window matches four cycles
earlier with maximum error 0 at tolerance 2.1920928955078125e-7. Original center-pan
mono projection and finite output are checked. Reopen verifies all 93 mapping
slots, version 3, enabled state and PC0 target 73, then records fresh audio from
the saved item; this is not recorded-loop project persistence. Evidence log:
build/reaper-midi-clap-acceptance.log. A prior independent paced CLAP run also
passed in build/reaper-daw/20261024T123002-89484d10/result.json.

Final VST3 result: build/reaper-daw/20261024T123137-19147c0f/result.json is FAILED;
tools/TestReaper.py --format VST3 --midi exits 1. It retains the seven old monitor
cases and two successful MIDI Note/CC cases, both with recovered phase error 0.
PC0 does not mute: both channel peaks are 0.09677410125732422 in the required
zero window. The same failure was reproduced before pacing in
build/reaper-daw/20261024T122344-fffea83d/result.json. Final log:
build/reaper-midi-vst3-paced.log. Keep this as a failed acceptance gate, not an
expected-failure skip or evidence that MUTE works in every format. This blocks
advancing the cross-format MIDI feature. The wrapper/program-list repair remains
pending; architecture and primary-source investigation are in ARCHITECTURE.md.

Both final host runs verify the exact compiled module path in the owned process,
before/after SHA256 equality, raw item byte/timing identity and saved projects.
Runner, both Lua script and fixture-test hashes plus source revision are retained.
No user REAPER process/profile/MIDI assignment was reused. Host script PASS only
means render completion; the independent Python PCM assertion owns acceptance.

Fixture failures were preserved rather than hidden:
- build/reaper-midi-first.log: incremental InsertEvt normalized overlapping Notes
  into separately released taps. The saved source proves the extra NoteOffs.
- build/reaper-midi-raw.log and reaper-midi-raw-diagnostic.log: the host replaced
  an empty terminator. Expected/actual binary events are retained in their run
  directories. Publish the complete stream atomically and supply explicit CC123;
  require exact readback before rendering, retaining the repeated-Note case.
- build/reaper-midi-complete-events.log: CountEvts groups paired notes; it is not
  a count of raw repeated NoteOns. Raw equality remains the strict packet check.
- build/reaper-midi-clap-final.log: full-speed reopened recording outran the
  background finalization worker; only 1440 nonzero samples/channel appeared in
  the 3360-frame playing window. This remains an unpaced-offline limitation.
  MIDI renders now require project 1x speed and measured duration >=0.9s for one
  second of audio. Final CLAP measured roughly 1.6s/render including UI/settling;
  no PCM/window/density/phase tolerance was relaxed. No sleep/wait was added to
  product audio. Full-speed offline recording/finalization is NOT certified.

Portable reaper_midi_fixture has three test methods with positive/negative
subcases: preserve opaque wrapper bytes and exact state size; reject seven bad
states; reject silent playback, monitor leakage, changed phase/pan and lost
recalled mappings. It is registered with CTest. The final Windows and Ubuntu
subset original_ui_bytes/reaper_midi_fixture/js_native_midi/native_frontend_commands
passes 4/4 on each platform (build/reaper-midi-portable-*-final.log). This is a
focused subset, not a fresh full 20-test/sanitizer/native GUI/Ardour run. The
previous increment's 64/128, sanitizer and Ardour evidence retains its original
scope; no product code changed here. Physical pedals/drivers, real-time driver
buffers, editor Learn through DAWs, full-speed offline recording, VST3 PC and
Ardour MIDI remain unverified or failing as specified above.

## VST3 per-channel program bridge acceptance — 24 October 2026

Baseline 1264c84b03e62adc2eeda70c717d9299e3350c94 plus this repair. This supersedes
the preceding missing-Program-Change result for the tested REAPER formats only.
The generated wrapper adds sixteen hidden MIDI routing parameters/channel units
and named PC command lists. Existing APVTS IDs/state version 3, one application
program, bindings and original UI/audio algorithms remain unchanged. No package,
version bump, SDK checkout modification or hardware assignment change was made.

Final real REAPER 7.82 evidence:
build/reaper-daw/20261024T135706-1f38cbd1/result.json (passed), with
build/vst3-pc-reaper-acceptance.log. BOTH actual VST3 and CLAP pass the original
seven monitor/track-state/project renders plus 20 MIDI renders each:
- Original Note/CC MUTE cases, with Note36 record/finish and the unchanged fixed
  7200-frame/four-cycle phase oracle.
- A separate Note-recorded PC0-only MUTE case: initial PC0 equals the parameter's
  default, then the next tap repeats PC0 with no intervening program value. Keep
  both equal-value pulses and the original fixed-period phase check.
- Sixteen channel-specific PC127 record/finish and PC0 MUTE cases, each with
  repeated bursts, PC0 on every other channel, an unmapped PC64, a wrong-channel
  PC127, exact silent mute, nonzero playback/recovery and unchanged center pan.
- Saved assignment/item reopen on channel 16, then a fresh recording controlled
  by the saved PC127/PC0 routes; this does not implement saved-loop persistence.

All 40 MIDI renders compare 6720 recovered stereo samples at the same source phase
with maximum difference 0, at the unchanged 2.1920928955078125e-7 PCM tolerance.
CLAP's raw-MIDI takes remain 7200 frames. REAPER VST3 PC-driven takes measured
7168 frames for the nominal 7200-frame interval; Note-driven takes remain 7200.
The bridge preserves the supplied sample offset, but this host-format PC timing
is not certified sample-accurate. Added PC captures uniquely identify the source
period from pre-mute audio alone, then compare four actual cycles later. Ambiguous
periods are rejected. No recovered audio determines the expected period, no PCM
error tolerance is enlarged, and the older Note/PC0-only fixed-period checks
remain. The earlier 19-case run is in
build/reaper-daw/20261024T134009-8fa6dc11/result.json, also passed. Its preceding
fixed-150ms assumption failure is retained in build/vst3-pc-reaper-matrix.log and
build/reaper-daw/20261024T133127-e9fbb85c/result.json. Rechecking those retained
samples against their independently identified 7168-frame pre-mute period yields
zero phase error; this was fixture timing, not a mute/source edit.

The final host report retains both module identities, before/after binary hashes,
raw MIDI byte/timing evidence and hashes, script/runner/test hashes, saved projects
and source base. Assigned channels and repeated PC0/PC127 actually affect the audio
through the real wrapper, not private IPC or an injected processor fixture.

Portable and processor evidence:
- Windows core 21/21, 314.41s: build/vst3-pc-windows-core.log.
- Ubuntu core 21/21, 422.88s: build/vst3-pc-ubuntu-core.log.
- ASan/UBSan 3/3, 146.76s: build/vst3-pc-asan.log; leak/error halt enabled,
  including new program routing, full core and unchanged-JS MIDI tests.
- New vst3_program_routing covers 4096 actual-PCM cases (16 channels x 128 programs
  x 64/128), wrong-channel isolation, exact decoded offsets, repeated identical
  programs, silence/recovery, unchanged clip/history and zero tracked callback
  new/delete. Invalid values/IDs/offsets and the shared 256-point bound are checked.
- Production native_midi_processor tests verify no reserved-ID/APVTS collisions,
  one application program and no preset mutation, atomic entire-MIDI-block overflow
  rejection without fabricated/raw-MIDI mutation, exact-offset recovery and zero
  tracked callback allocations at 64/128. Full existing processor tests remain.
- vst3_wrapper_generation verifies the exact current generated source, upstream
  bytes/license, idempotence and fail-closed drift under ordinary AND optimized
  Python. Normalized source SHA256 is
  91716a2225f034ed4c21e86a37edaaba71bdf3fa161de0938f4e754bb1e61874.
- The final portable fixture has positive state/PCM and negative malformed-state,
  silent-output, monitor-leakage, phase/pan/mapping and ambiguous-period controls.
  Channel-specific fixture XML stays exactly the same total length, with short
  equivalent numeric values and root whitespace preserving opaque wrapper sizes.

Windows frontend/fixture/routing/generation/processor/CLAP/VST3 group passes 7/7
in 44.67s (build/vst3-pc-windows-formats-final.log). The newest 20-case fixture and
optimized-Python guards additionally pass 2/2 in 10.29s
(build/vst3-pc-final-fixtures.log). An earlier group correctly failed because the
new native routing test executable had not been built; retain
build/vst3-pc-windows-formats.log as Not Run, not a passing gate. Building that
specific target repaired preparation without removing the test.

The FULL production-editor mouse/keyboard/MIDI/export/merge workflow passes at
both 64 and 128: build/vst3-pc-full-gui.log. This retains real pointer/key learning,
Save/reload/clear, focused expression sliders, repeated Space panic, held SUS and
Stop/delete safety, static/dynamic capture, LIVE, STUT, exact WAV and merge/history
coverage. No physical audio device was opened by the owned synthetic host.

Linux frontend/fixture/routing/generation/processor/LV2 audio/CLAP/LV2+VST3 pluginval
passes 9/9 in 96.49s (build/vst3-pc-linux-formats.log). Strict assertion/leak gates
and existing vendor skips/warnings remain. Real Ardour 8.4 libardour/Lua LV2
48kHz Dummy stereo gain/mute/peak/session recall passes at 64/128:
build/ardour-daw/20261024T140156-9afab4a3/result.json,
build/vst3-pc-ardour.log. This remains peak/property/session coverage, not GTK
pointer interaction, Ardour MIDI recording or physical JACK acceptance. GUI/DAW
gates ran sequentially across platforms; independent builds/core audits overlapped.

The independent unchanged-JS committed capture/OD/SUB audio audit passes 126 cases,
965826 samples, maximum absolute error 7.394757495227111e-8 at tolerance 1e-6
(build/vst3-pc-audio-audit.log). The initial invocation used the wrong DSP probe;
it failed command-row identity before comparison and is retained separately in
build/vst3-pc-audio-audit-wrong-probe.log. The accepted run uses the required
command-stream aion_reference_probe; the product/oracle/tolerance did not change.

Windows and Ubuntu existing binaries rebuilt. Source, generated wrappers,
embedded asset identities and module hashes are recorded in
build/vst3-pc-final-evidence.json. Test binaries contain the user's separate local
logo whitespace override, which remains excluded from the feature commit. New
routing is bounded and uses prepare-time storage; this is not certification of
all arbitrary oversized framework MIDI allocation paths. Physical foot controllers,
ASIO/JACK/CoreAudio latency, Apple/other-host builds, full-speed offline recording
finalization, DAW-follow/MIDI-clock scheduling, projects and remaining DSP migration
retain their earlier unverified/unimplemented limits. No next feature started.


## Offline recording finalization — 24 October 2026

The user approved waiting only on a host-declared offline render owner. Live
processing remains nonblocking. The worker still owns allocation, whole-clip DSP
and destruction; waiting holds virtual time instead of pacing audio at 1x.
AionProcessor forwards JUCE isNonRealtime on every callback. Core completion
boundaries include queued commands, individual MIDI FIFO tasks, zero-sample MIDI
flushes and scheduled/capacity-triggered finishes. The original UI, DSP kernels,
APVTS/MIDI schemas and physical assignments are unchanged.

Reproduction and repair evidence is retained in build/offline-finalization/:

- offline-before.log and build/reaper-daw/20261024T154121-624c8404/result.json:
  unpaced CLAP failed real nonzero playback (only 2976 nonzero samples/channel in
  a required 3360-frame window). This was not an accepted render.
- offline-midi-before.log: the initial offline wait still deferred the next
  track's same-sample MIDI record by one sample. The regression remains mandatory;
  the bounded MIDI FIFO now commits the previous worker job before inspecting its
  next task, without advancing the clock or increasing its task limit.
- offline-native-build.log: the first new processor test did not compile because
  of an ambiguous JUCE var/int comparison; an explicit test-side int conversion
  corrected it. No assertion or product behavior was changed for that error.
- offline-final-windows-core.log: final Windows core/reference suite 22/22,
  338.08 seconds. offline-final-linux-core.log: final Ubuntu 22/22, 422.55 seconds.
  Its independent unchanged-JS capture/OD/SUB center-pan audit also passed all 126
  cases / 965826 samples, maximum absolute error 7.394757495227111e-8 at the retained
  1e-6 tolerance. Browser resampling, envelopes and hardware latency stay excluded.
- offline-final-sanitizers.log: final ASan/UBSan with leak detection passed 4/4:
  offline PCM, real-time core, VST3 program routes and original-JS MIDI comparisons.
- offline-windows-native-tests.log and offline-production-processor.log: Windows
  offline core/generator/CLAP/VST3 validation 4/4 plus production MidiBuffer 1/1.
  The production test sets host offline mode after prepare, records/finishes at
  an exact offset, checks immediate committed/nonzero PCM, then switches live and
  back without changing the clip. Tracked callback new/delete remains zero.
- offline-linux-host-tests.log: final Linux offline/generator/production processor,
  public LV2 audio, CLAP validator and LV2/VST3 pluginval gates 7/7. Actual Ardour
  libardour/Lua Dummy stereo gain/mute/session recall also passed at 48kHz/64/128;
  this is not GTK pointer, physical JACK or Ardour offline-recording acceptance.
- offline-live-gui.log: the full synthetic-audio mouse/keyboard/MIDI/export/merge
  workflow passed at 64 samples; the first 128-sample attempt failed because no
  wheel event reached the test (evidence array empty). Preserve that failure.
  After other Windows host checks finished, the unchanged complete 128-sample
  workflow passed in offline-live-gui-128-isolated.log. No product/UI/input change,
  synthetic wheel substitute, assertion removal or tolerance change was made.

Final real REAPER 7.82 evidence:

- build/reaper-daw/20261024T155427-b5aa057c/result.json: full-speed, projrenderlimit=0.
- build/reaper-daw/20261024T155748-30f5e568/result.json: retained default 1x mode,
  with the original measured >=0.9s requirement for each one-second MIDI render.

Each run passed seven monitor/track-state/project renders and twenty MIDI renders
per VST3/CLAP format. Every recovered-loop phase error was zero at the original
PCM tolerance. Mode is checked before each MIDI render, including project reopen;
loaded-module identity and before/after hashes match the final rebuilt modules.
Full-speed is an explicit host render configuration, not a wall-time speed claim.

OfflineTests.cpp retains exact entire-render PCM equality across 1/64/128 samples
and mono/stereo, simultaneous command/MIDI capture handoffs, OD/SUB/NORM, reverse,
Undo/Redo/MULT/MERGE, static automatic completion at 8/48/96 kHz, pool exhaustion,
empty/short/near-capacity lengths, zero-sample MIDI finish and cancelled live-origin
worker ownership. Worker waits must never advance audio time. Tracked render-owner
new/delete stays zero; arbitrary system/framework malloc is not globally certified.

The prior full-speed failure is repaired for the tested REAPER VST3/CLAP workflow.
REAPER VST3 PC-driven capture still receives a 7168-frame rather than nominal
7200-frame period; the independent pre-mute period oracle and old Note-timed test
remain unchanged. Supplied-offset core acceptance is separate from host timestamp
accuracy. Physical ASIO/JACK/CoreAudio latency, Apple/other hosts, full host-follow,
MIDI clock and stored-loop project persistence remain pending. Hosts must correctly
declare offline mode; prefetch alone must never enable waiting. No package or
version bump is created, and the separate user logo edit remains outside the commit.


## Keyboard RTRG repeat parity — 24 October 2026

The unchanged original App.handleKeyboardPress dispatches an unmapped '/' or
NumpadDivide before its generic repeat guard. Native AionPerformance put it after
the guard, losing repeated retriggers. Move only this shortcut before loop-key
repeat suppression, retaining modifier/text/content-editable guards and ordinary
loop/mode-key debounce. The existing selected-loop/native RTRG route is unchanged;
no C++ DSP, APVTS/MIDI state, bridge protocol or original markup/assets change.

Observed evidence:

- build/retrigger-repeat-before.log reproduces the mismatch against the complete
  unchanged original dispatcher: native count zero versus expected one.
- build/retrigger-repeat-frontend.log passes all 1,300 new original-dispatcher
  cases: two valid selected-track endpoints, five key/code variants, repeat,
  Shift/modifier and thirteen target/input types. It separately retains native
  editable-content protection and requires no held cut/delete timer ownership.
  Every prior frontend case remains present, including loop/mode repeat guards.
- build/retrigger-repeat-windows.log: six applicable asset/frontend/bridge/
  original-JS state/DSP/MIDI gates passed in 9.51s; Ubuntu equivalent passed in
  14.20s (build/retrigger-repeat-ubuntu.log).
- build/retrigger-repeat-audio.log: unchanged-JS committed capture/OD/SUB through
  centre pan, 126 cases / 965826 samples, maximum absolute error
  7.394757495227111e-8 under the retained 1e-6 threshold. Scope still excludes
  browser resampling, transition envelopes, latency compensation and sync.
- build/retrigger-repeat-gui.log retains the initial failed 64-sample startup:
  no wheel event reached the existing strict window-capture test. No wheel
  assertion, event, selector or tolerance was changed. The unchanged test then
  passed in the isolated 64-sample rerun.
- build/retrigger-repeat-gui-64-retest.log and build/retrigger-repeat-gui-128.log
  pass the complete production processor/editor mouse/keyboard/MIDI/export/merge
  workflows. New checks use actual trusted CDP keydown events with repeat flags,
  not synthetic DOM KeyboardEvents. Before every playing-loop repeat, genuine
  range-key input moves Start above zero, so observing an old zero cannot pass.
  Each shortcut sends exactly one accepted command, resets Start/applies phase,
  renders nonzero PCM and preserves waveform identity/length/Undo/Redo. Repeated
  keydowns also start stopped PCM. Eight trusted events per buffer size include
  six repeats, split across Slash and keypad Divide. Existing hold/cancel/panic,
  MIDI learning, waveform/export/merge and immutable-audio checks are retained.
- build/retrigger-repeat-reaper.log: the rebuilt Windows VST3/CLAP each pass the
  seven real REAPER stereo monitor/state/project-recall renders. This is module
  load/audio/state validation, not DAW keyboard-focus or plugin HTML interaction.

Windows Standalone, VST3, CLAP and the synthetic test host were incrementally
rebuilt. Their embedded native adapters byte-match current sources, including
the separate user logo whitespace, which is excluded from this commit. C++
audio/MIDI/processor source hashes still match the accepted offline-finalization
baseline. No new allocation/sanitizer claim is inferred from this JS-only change.
Linux portable JavaScript/state/DSP gates pass; Linux native binaries and GTK/
WebKit keyboard input were not rebuilt/retested for this increment. macOS/iOS,
physical keyboard/OS layouts, DAW shortcut-focus routing, custom key mappings,
keyboard SUS and broader selected-FX targeting remain separate work.

MDN documents the browser's [repeat flag](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/repeat).
The unchanged application dispatcher defines the required repeat policy; browser
reference material does not justify changing other shortcut semantics. No package
or version bump is produced, and the original interface is preserved.


## Keyboard Multiply repeat parity — 24 October 2026

The unchanged original App.handleKeyboardPress dispatches an unmapped backslash
before generic key-repeat suppression. Native AionPerformance discarded repeated
keydown, so holding the shortcut could begin Multiply but could not finish its
interactive take. Move only the Multiply shortcut ahead of generic loop-key
repeat debounce, keeping modifier/text/editable guards. The existing processor
still chooses interactive input capture or instant fallback. No C++ DSP, worker,
MIDI/APVTS schema, bridge protocol, original assets or UI layout change.

Evidence:

- build/multiply-repeat-before.log: complete unchanged original dispatcher
  expected one MULT call for repeated Backslash; native delivered zero. The
  failing case is retained.
- build/multiply-repeat-frontend.log: 1,040 new complete-original-dispatcher
  comparisons pass across valid track endpoints, Backslash/IntlBackslash/key
  characters, repeat, Shift/modifier and thirteen target/input types. Unsupported
  shifted pipe and editing/modifier guards remain. No held cut/delete timers or
  loop-press ownership is created. All old frontend and RTRG comparisons remain.
- build/multiply-repeat-windows.log: seven applicable asset/frontend/bridge/
  original-JS state/DSP/MIDI/Multiply gates pass in 41.68s; matching Ubuntu gates
  pass in 71.10s (build/multiply-repeat-ubuntu.log). js_native_multiply retains
  the unchanged original capture/conditioning/history PCM oracle and tolerances.
- build/multiply-repeat-audio.log: unchanged-JS committed capture/OD/SUB through
  centre pan, 126 cases / 965826 samples, maximum absolute error
  7.394757495227111e-8 at the retained 1e-6 threshold. Browser resampling, transition
  envelopes, latency compensation and sync remain outside this audit.
- build/multiply-repeat-gui-64.log and build/multiply-repeat-gui-128.log: complete
  production processor/editor mouse/keyboard/MIDI/export/merge workflows pass.
  New checks use trusted CDP Backslash keydown/repeat input, not DOM keyboard
  dispatch. A normal key begins actual interactive Multiply; a repeat finishes
  it, committing nonzero appended input while preserving the complete original
  stereo WAV prefix byte-for-byte. Local Undo restores the original WAV; Redo
  restores the exact appended WAV. After Undo and STOP ALL, another held-key
  repeat doubles stopped PCM into two exact copies of the original; Undo again
  restores exact original bytes, then actual playback resumes. Three trusted
  events include two repeats per buffer size. The destination picker is the
  existing host-owned test substitute, not a physical OS save-dialog check.
  Existing wheel/hold/panic, RTRG, sync/phase/STUT, MIDI Learn and immutable
  export/merge gates are retained without assertion or tolerance changes.
- build/multiply-repeat-reaper.log: rebuilt Windows VST3 and CLAP each pass all
  seven real REAPER settled stereo/state/project-recall cases. Module identity
  and before/after hashes match the rebuilt local binaries. This is not DAW
  keyboard-focus or HTML keyboard interaction acceptance.

The existing Windows Standalone/VST3/CLAP and synthetic host are rebuilt, with
all six native adapters verified inside each binary. The separate user logo
whitespace is present in local binaries and remains outside this source commit.
C++ audio/MIDI/processor hashes still match the accepted offline-finalization
baseline; this JS-only repair adds no new callback allocation/sanitizer claim.
Ubuntu portable original-reference tests pass; Linux native binaries/WebKit UI
were not rebuilt/retested for this increment. Apple platforms, physical OS/layout
or DAW shortcut focus, custom keymaps, keyboard SUS and broader selected-FX routing
remain separate work. Existing finalization/varispeed behavior is retained; this
is repeat-dispatch parity rather than a redesign of Multiply scheduling.

Browser [repeat flags](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/repeat)
identify held-key events; the unchanged application defines which shortcuts may
repeat. No package or version bump is generated. Original interface preserved.


## Loop volume readout units — 24 October 2026

The native label dropped the original dB suffix at zero volume. The production
refresh now matches unchanged UIManager.setLoopVolume: -infdB at silence, 0.0dB
at unity and 6.0dB at gain two. No gain calculation, audio callback, parameter,
MIDI assignment, original HTML/CSS or saved-state schema changed.

- build/volume-label-before.log retains the initial failing original-setter
  comparison (-inf versus -infdB).
- build/volume-label-frontend.log: 4,060 complete unchanged-setter comparisons
  across all twenty tracks, 0..2, negative zero and a small positive value.
- build/volume-label-windows.log and build/volume-label-ubuntu.log: seven of
  seven original-asset, frontend, bridge, state, DSP, MIDI and Multiply gates
  pass on each system, retaining existing assertions and tolerances.
- build/volume-label-audio.log: 126 committed-PCM cases, 965,826 samples,
  maximum error 7.394757495227111e-8 against the unchanged 1e-6 threshold.
- build/volume-label-gui.log: complete production editor/processor workflows
  pass at 64/128 samples. Trusted Home/End input on the actual loop volume range
  checks visible -infdB/6.0dB labels, actual zero/nonzero output and unchanged
  waveform identity, length and Undo/Redo. Unity is restored and displays 0.0dB.
  All retained main/keyboard/hold/sync/MIDI/export/merge checks remain enabled.

The first full 64-sample run passed the new readout and complete main workflow,
then ExportWorkflow timed out at ordered NORM/export after reload with the
previous cancellation status. build/volume-label-gui-failed.log retains it.
The independent fresh-host --ExportOnly check passes in
build/volume-label-export-recheck.log, followed by the complete successful rerun
above. No export code, fixture assertion, timeout or tolerance was changed;
the initial failure's cause is not established, so this is not an export repair
or a claim of universally failure-free GUI automation.

The existing Windows Standalone, VST3, CLAP and synthetic host were rebuilt.
Linux portable comparisons pass; native Linux UI, physical drivers/controllers,
Apple platforms and DAW HTML interaction were not retested for this label repair.
This is presentation parity, not new sound/latency or full migration acceptance.
No package or version bump; the user's separate logo whitespace remains unstaged.
The logarithmic display retains the reference formula; [Math.log10 documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/log10)
was consulted without changing the original zero-volume convention.


## Delayed frontend export cleanup — 24 October 2026

Investigating the previous post-reload NORM/export timeout identified a separate,
reproducible lifecycle race: frontend bootstrap and pagehide sent unscoped
cancelExport even though native navigation/destruction already owns cancellation.
A delayed bootstrap packet could cancel a newer picker. Those two redundant
frontend requests are removed. Explicit Cancel, native lifetime/sequence guards,
selected background writes, original UI and DSP/state schemas are unchanged.

The old historical timeout did not capture its request ordering; this test proves
the cancellation mechanism, rather than claiming its sole cause was observed in
that earlier run. The fixture also awaits the actual reloaded enabled NORM button
before the edit/export gesture: cancellation telemetry is not UI readiness, and
[disabled click() controls do not fire](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/click).

- build/export-cleanup-debugger-before.log: actual old production controller
  paused at its DOMContentLoaded body via CDP, with only its startup cleanup IPC
  held. After real NORM finalization and a new chooser, releasing one packet
  reproduces WAV export cancelled / exportBusy false. Native audio/worker runs
  normally; no snapshot/PCM fixture or audio sleep is used.
- build/export-cleanup-after.log: targeted 64/128 workflows pass. The same hook
  sees zero startup packets; chooser filename/clip identity survives, normalized
  stereo WAV is saved, and existing empty-loop/rename/duplicate/cancel/replacement/
  late-callback/JS-disabled-navigation/failure/recovery/overwrite/DEL assertions
  remain intact. Destination selection alone uses the existing host picker.
- build/export-cleanup-full-gui.log: complete 64/128 production editor/processor
  mouse, keyboard, hold/delete, sync, Multiply/history, MIDI and merge workflows
  pass, including the delayed-cleanup export gate in each run.
- build/export-cleanup-windows.log and build/export-cleanup-ubuntu.log: seven
  original assets/frontend/bridge/state/DSP/MIDI/Multiply gates pass per system.
- build/export-cleanup-audio.log: 126 cases / 965,826 committed PCM samples,
  maximum error 7.394757495227111e-8 at the unchanged 1e-6 tolerance.

Early document-created diagnostic hooks did not execute in this WebView and were
inconclusive (export-cleanup-hook-inconclusive.log / export-cleanup-before.log).
They are not acceptance evidence; the actual-controller debugger hook above is.
Windows Standalone/VST3/CLAP and the synthetic host were rebuilt. All C++ hashes
retain the accepted baseline; no new real-time allocation/latency claim is made.
Ubuntu portable gates pass; Linux native UI, Apple platforms, physical drivers,
OS file-dialog interaction and DAW HTML export were not retested this increment.
No package/version bump. The user's separate logo edit remains unstaged.


## Portable native meter calculation — 24 October 2026

Added core/MeterFollower.h, a bounded single-owner C++ port of the complete
unchanged AudioEngine.readMeter method. It consumes supplied mono analyser
windows and stable identity tokens, retaining original RMS release, 0.8s sample
peak hold, 20dB/s decay, 1.2s clip indication, -60dB floor and 128-item timed
history. Source/context/data/running/backwards-clock resets, stalled clocks,
audible-clock jitter and disconnected tails follow the original. Oversized
windows (>32768) are a native input-contract rejection that preserves state;
they are not a new user-facing behavior or a browser-array parity claim.

- MeterParity.cjs executes the entire unchanged method with Float32 windows,
  not a reimplementation of its equations. The final 1,000 observations include
  0/1/64/128/256/2048/32768 lengths, threshold values, nonfinite samples/clocks,
  identity changes, stopped contexts, ring wraps and history older than audible
  time. Maximum absolute error is 2.4868995751603507e-14 at the fixed 1e-12
  relative-to-max(1, expected) tolerance on Windows, Ubuntu and ASan/UBSan.
- MeterTests.cpp checks independent exact hold/clip boundaries, finite handling,
  oversized rejection, wrap/decay and identical analyser observations assembled
  from 64/128 partitions. Construction/update/destruction has zero tracked
  new/delete. This is not an arbitrary-malloc or device-latency certificate.
- build/meter-kernel-windows.log: all nine selected core/reference/bridge gates
  pass after rebuilding the final probe. build/meter-kernel-ubuntu.log: nine
  pass on the initial 936-observation matrix; meter-kernel-ubuntu-expanded.log
  passes the rebuilt final 1,000-observation matrix. LastTest.log in each build
  retains the detailed comparison output.
- build/meter-kernel-sanitizers.log: strict -Wall/-Wextra/-Wpedantic/-Werror
  syntax checks plus core_realtime_contract and js_native_meter pass under
  AddressSanitizer/UndefinedBehaviorSanitizer with leak detection and halt-on-error.
- build/meter-kernel-audio.log: retained 126-case / 965,826-sample capture/OD/SUB
  audit passes; max error 7.394757495227111e-8 at unchanged 1e-6 tolerance.

The first Windows aggregate run used the old 4096-window probe after the test
matrix was expanded and rejected its larger input (exit 2). That inconsistent
run is retained in meter-kernel-windows-stale-probe.log and excluded from final
acceptance; the matching probe was rebuilt before the successful nine-gate rerun.
No assertion or tolerance was removed to obtain a pass.

This completes the isolated calculation component only. It is not wired into
production Engine/IPC/UI, collects no native PCM and establishes no browser
downmix/window, audible/device latency or original-renderer integration parity.
Existing audio routing and all original UI/JS assets remain unchanged, including
the separate user logo edit. SDK binaries retain the previous accepted hashes;
no app/DAW/GUI rebuild or new hardware/Apple/Linux UI claim is made. No package
or version bump; the visible meters remain a later integration gate.


Raw loop meter-window increment (24 October 2026), based on 5cd43ed:

- Windows and Ubuntu portable CTest: 24/24 each. Relevant ASan/UBSan gates:
  3/3, leak detection and halt-on-error, with strict warning syntax checks.
- Actual LoopBank/Engine: 36 loop gain/pan/block cases; zero prefix, wrap,
  antiphase, nonfinite mixed-sample protection, invalid ID, clear/prepare,
  zero-frame callbacks, MASTER/monitor/click isolation, exact 64/128 PCM and
  zero tracked callback new/delete. Tests feed collected PCM to MeterFollower.
- Raw JS/native window oracle: 36 cases/73,728 samples. Actual WebView2
  OfflineAudioContext: 40 cases/81,920 samples, maximum error 0 (tolerance 1e-6),
  mono/stereo/antiphase and varying/short signals at 8/44.1/48/96 kHz with
  64/128 native partitions. This is a data-window test, not audible latency.
- Retained original capture/OD/SUB audit: 126 cases/965,826 samples, maximum
  error 7.394757495227111e-08 at unchanged 1e-6 tolerance.
- Rebuilt native JUCE processor: 1/1 Windows and 1/1 Linux; full synthetic
  production editor/mouse/keyboard/hold/export/merge/MIDI workflow at 64/128.
- Rebuilt REAPER Windows VST3 and CLAP: seven render/state/recall cases and
  twenty MIDI cases per format, full-speed offline. Module hashes stable.
  Report: build/reaper-daw/20261024T225621-857f2998/result.json.
- Rebuilt Linux Standalone/VST3/CLAP/LV2; actual Ardour LV2 at 48 kHz, Dummy
  backend 64/128, gain/mute/session recall. Bundle hashes stable. Report:
  build/ardour-daw/20261024T230828-e88611d2/result.json.

Logs use build/meter-window-*; final source/binary/report hashes are recorded
in build/meter-window-final-evidence.json. The attempted xvfb-run wrapper was
unavailable and its failed log is retained; the Linux processor and Ardour
were successfully run using the existing WSLg display, without changing gates.
Ardour checks peaks/session recall through libardour, not GTK interaction or
sample-exact envelopes. REAPER offline tests do not certify physical MIDI or
device latency. Linux CLAP was built, not hosted by Ardour (which lacks CLAP).
No new Apple, physical device, host-follow or MIDI-clock acceptance is claimed.

The mutable PCM windows have one audio owner. No production follower call,
cross-thread payload, JSON/schema or visible meter has been added. Source
identity, timestamped bounded transfer and audible-time renderer integration
remain pending. The separate user logo edit and all original JS/UI behavior
are retained. No package or version bump; this completes only raw collection.


Raw meter transfer increment (25 October 2026), based on 4747bcb:

- Windows and Ubuntu full portable suites: 24/24 each, all linked probes rebuilt.
  Relevant ASan/UBSan core/window/follower gates: 3/3, leak detection and
  halt-on-error. Strict -Wall/-Wextra/-Wpedantic/-Werror syntax check also passes.
  After strengthening only the Engine fixture to use ordinary live callbacks
  following deterministic worker finish, affected core/window/follower checks
  were rebuilt and repeated: 3/3 Windows, 3/3 Ubuntu and 3/3 ASan/UBSan. Production
  code and original reference gates were unchanged during that fixture extension.
- Rebuilt Windows and Linux JUCE processor targets: native_midi_processor 1/1
  on each platform, retaining real MidiBuffer/host/PCM/allocation regressions.
- Transfer tests cover invalid track IDs, coalescing 1000 requests, exactly seven
  queue slots, at most two window copies per callback, 500 saturated callbacks,
  all twenty tracks recovering without overwritten PCM, prepare cancellation,
  bounded old-generation draining and untouched failure output.
- 4000 exchanges between two real owners validate every sample and identity of
  each packet. Tracked new/delete remains zero on both request/pop and publishing
  owners after construction; no ThreadSanitizer or arbitrary-malloc claim.
- Actual Engine tests at 8/48 kHz and 64/128 compare transferred nonzero PCM
  exactly to the audio-owned 2048-frame tap and coherent clip/clear metadata.
  Exclusive end-frame and sample-rate stamps, zero-frame callbacks, delayed clear
  packets, fresh cleared-tail PCM, eventual exact silence, changed-rate prepare
  rejection and fresh empty-window recovery pass. Identical stereo audio from
  a second Engine with no requests proves the transfer is passive in these cases.
- Retained original meter/window oracle gates pass. Connected capture/OD/SUB
  audit: 126 cases / 965,826 samples, maximum error 7.394757495227111e-08
  at unchanged 1e-6 tolerance.

The first Ubuntu and sanitizer runs rejected a fixture that expected immediate
zero PCM after rendering a cleared loop. Existing LoopBank clear intentionally
retains a short output fade. Failed evidence remains in
build/meter-transfer-ubuntu-clear-assumption.log and
build/meter-transfer-sanitizers-clear-assumption.log. The corrected fixture keeps
the delayed-clear case, requires exact equality to the real current tap (including
that tail), and adds eventual exact silence after the rolling window advances.
No DSP change or relaxed tolerance was used to obtain a pass. The initial sandbox
WSL service access denial was resolved by running WSL outside the sandbox.

Full-suite logs: build/meter-transfer-windows.log, meter-transfer-ubuntu-final.log;
final affected checks: build/meter-transfer-live-windows.log,
meter-transfer-live-ubuntu.log and meter-transfer-live-sanitizers.log. Retained
audit: build/meter-transfer-audio.log. Source hashes
and unchanged SDK binary hashes are in build/meter-transfer-final-evidence.json.
Original assets, meter collection/follower kernels and the user's separate logo
edit are preserved. Runnable app/plugin/test-host hashes equal the prior accepted
REAPER, Ardour and 64/128 GUI builds; those host/GUI tests were not rerun for this
unexposed Engine telemetry API. Only native processor test targets were rebuilt
in the SDK trees. No new DAW, physical driver/MIDI, Apple, device
latency or visible-meter acceptance is claimed. Source-lifecycle identities,
audible-time selection and original renderer integration are still pending.
No package, SDK update or version bump.


Native meter source revisions (25 October 2026), based on d1e622e:

- Windows and Ubuntu full portable suites: 25/25 each, all probes rebuilt.
  Relevant ASan/UBSan gates: 4/4 with leak detection/halt-on-error. Strict
  -Wall/-Wextra/-Wpedantic/-Werror syntax checking also passes.
- The 136-case original_meter_source_lifecycle gate runs unchanged Loop.play,
  restart, stop, _teardown and clear, AudioGraph.retrigger, and the exact main.js
  loop meter input block. It verifies analyser/data preservation through healthy
  retriggers, replacement after teardown/play, the original 25ms stop timer,
  destroyed graphs, and active/inactive state gating. Audio nodes/timers are IO
  stubs: this is object/state parity, not actual browser audio or timer latency.
- Six native fixtures cover tracks 0/19 at 8/48 kHz plus actual Engine packets
  at 64/128. Capture without playback creates no source; commit/autoplay does.
  Controls, reverse latch, STUT and NORM preserve the current native source.
  Scheduled RTRG retains it before the application sample and advances exactly
  there, while the raw 2048-frame history shifts without reset. STOP cancels old
  pending events without later resurrection. Repeated stopped STOP is stable;
  static/dynamic grid stop/play stamp only actual transitions. Clear/timeline
  reset invalidate prior stamps.
- Actual nonzero Engine recordings exercise same-generation/same-clip stale
  packet rejection after a scheduled replacement, fresh packet recovery and
  invalid track/zero generation/clear/clip guards. Source waits are bounded so a
  missing transition fails instead of hanging. Source metadata and validation
  have zero tracked callback new/delete. The retained 4000 concurrent transfer
  packets now verify source revision with the other metadata and every sample.
- Rebuilt native JUCE processor targets: 1/1 Windows and 1/1 Linux, including
  existing host/MidiBuffer/PCM/state/allocation checks.
- Retained connected capture/OD/SUB audit: 126 cases / 965,826 samples, max error
  7.394757495227111e-08 at unchanged 1e-6 tolerance. Original asset,
  phase/RTRG, sync, history, export, merge and meter calculation/window gates pass.

Evidence: build/meter-source-windows.log, meter-source-ubuntu.log,
meter-source-sanitizers.log, meter-source-processor-windows.log,
meter-source-processor-linux.log and meter-source-audio.log. Source, processor
test binary and unchanged runnable SDK hashes are recorded in
build/meter-source-final-evidence.json. The native revision is a source change
stamp, not an analyser identity, graph-availability flag or audible-time decision.
It is not yet published in UI JSON or used by a production MeterFollower call.
The original 25/50ms graph teardown and browser-source overlap are not certified
by native source stamps; original browser object allocation may precede native
applied-source time. Complete graph/analyser lifetime mapping, audible-time
selection and original renderer integration remain pending.

Runnable apps/plugins/test-host were not rebuilt; their hashes match the prior
accepted REAPER/Ardour/64/128 GUI binaries. Only the native processor test targets
were rebuilt in SDK trees. No new DAW/GUI, physical MIDI/device latency, Apple
or full-visible-meter acceptance is claimed. The separate user logo edit is
preserved; no package, SDK update or version bump.

Meter graph/read composition (25 October 2026): added the isolated
LoopMeterGraph component and a complete original-method numeric oracle. The
377-observation trace executes unchanged Loop/AudioGraph methods, the exact
main.js input gate and complete AudioEngine.readMeter using Float32 windows.
Maximum error is 2.842170943040401e-14 at retained 1e-12 relative tolerance.
Audio-node and timer IO are stubbed; timer callbacks are explicitly delivered.
It checks healthy retrigger identity retention, stopped/destroyed graph gating,
fresh play, attached/detached clear, suspended/replaced contexts, audible-clock
stalls/jitter, history wrap, stop timer requests and late delivery. A queued
state change before play initially lost an existing cleanup timer in the model;
build/meter-graph-timer-before.txt records the failure. Native code was corrected
to retain it, preserving original behavior. Neither reference code nor tolerance
was changed to obtain acceptance.

Native tests additionally cover atomic stale/short required-window rejection,
invalid stop clocks, exact supplied-window 64/128 behavior and zero tracked
new/delete through lifecycle/read operations. These are native input contracts,
not claims about original invalid arguments or arbitrary malloc interception.
The retained connected PCM audit passes 126 cases / 965,826 samples at unchanged
1e-6 tolerance (maximum error 7.394757495227111e-08).

The complete portable suites pass 26/26 on Windows and Ubuntu. After the timer
correction, the five focused Windows gates and the two changed Ubuntu gates
were rebuilt/rerun; ASan/UBSan runs the five focused gates on corrected sources.
See build/meter-graph-{windows,ubuntu}.txt,
build/meter-graph-{windows,ubuntu}-final-focused.txt,
build/meter-graph-sanitizers.txt and build/meter-graph-final-evidence.json for
checks and hashes; build/meter-graph-oracle.json records numeric comparison.

No production Engine/processor/IPC/UI caller uses this composition yet. SDK
app/plugin/test-host and processor test binary hashes match the preceding
source-stamp evidence; no SDK rebuild, new DAW/GUI/device/Apple acceptance,
package or version bump is claimed. The original UI and separate user logo edit
are preserved. Actual graph-event binding, graph-owned PCM notification/transfer,
audible-time selection and original renderer integration remain pending.

Audible-clock policy (25 October 2026): PlaybackClock ports the complete original
AudioEngine.playbackTime getter and composes it with LoopMeterGraph's guarded
observation. The complete unchanged getter + readMeter oracle compares 616
snapshots on Windows and Ubuntu, maximum error 1.4210854715202004e-14 at retained
1e-12 relative tolerance. It covers timestamp precedence and millisecond units,
startup/invalid/overflow/subnormal values, missing/null/throwing timestamp and
performance APIs, separate base/output latency fallback, context loss/replacement
and suspension, backwards/stalled clocks, clipping history and ring eviction.
Native tests exercise actual MeterWindow PCM assembled in 64/128 partitions:
the delayed clipping window becomes visible at its audible time even after a
new silent window arrives. Missing-context resets and stale-window atomic
rejection remain intact; clock selection/composition has zero tracked new/delete.

Windows passes the complete 27-gate suite. Ubuntu's first full run passes 26/27;
the clock probe's stod conversion rejects the smallest finite timestamp on
libstdc++. The corrected strtod probe keeps that case and the unchanged policy;
the corrected CTest gate passes 1/1, accepting all 27 unique gates. The initial
failure is retained in build/playback-clock-ubuntu.txt and
build/playback-clock-ubuntu-first-oracle.txt. ASan/UBSan passes the six focused
core/clock/graph/lifecycle/window/follower gates. Strict Ubuntu -Wall -Wextra
-Wpedantic -Werror syntax checks pass for the new tests/probe. The retained
capture/OD/SUB audit passes 126 cases / 965,826 samples at 1e-6 tolerance, maximum
error 7.394757495227111e-08.

Evidence: build/playback-clock-{windows,ubuntu}.txt,
build/playback-clock-ubuntu-corrected-clock.txt,
build/playback-clock-sanitizers.txt, build/playback-clock-{oracle,ubuntu-oracle}.json,
build/playback-clock-audio.json and build/playback-clock-final-evidence.json.
The manifest records source and unchanged SDK/processor-test/user-logo hashes.
Clock/analyser IO is stubbed; these results do not measure hardware latency.
Actual native device/host timestamp acquisition, validated clock transport,
graph-owned PCM binding, IPC and original renderer integration remain pending.
No production caller enables the clock, no audio/UI/reference assets change,
and no SDK rebuild, new GUI/DAW/hardware/Apple acceptance or package is claimed.

Raw native callback clock (25 October 2026): production processBlock captures
the optional JUCE PositionInfo hostTimeNs from its existing audio-only host
observation. Engine carries it as exact uint64 nanoseconds in CallbackClockStamp
with local begin/end frames, generation, nonzero serial, rate and offline flag.
The complete stamp is transferred with EngineView and requested PCM packets.
There is no conversion to an audible-output/performance clock or frontend JSON.

Acceptance covers 8/48kHz and 64/128/zero-frame callbacks, engaged zero versus
missing timestamps, high-bit uint64 values, DAW seek independence, host loss,
wrong-start-frame suppression, reset/generation reuse, saturated queues and
recovery. Same-frame/same-source/same-generation zero-size callbacks require
different serials; old PCM cannot pair with a newer callback snapshot. Exact
view/packet tuple comparison is identity only, not graph/audible readiness.
4000 concurrent packets verify every clock field, including changing optional
and offline flags, along with all metadata and PCM. Real monitored Engine output
is bit-identical with and without raw host timestamps; tracked callback new/delete
remains zero.

Portable CTest: Windows 27/27 and Ubuntu 27/27. The final strengthened core
executable was rebuilt/rerun directly on Windows, Ubuntu and ASan/UBSan. The six
focused sanitizer gates also pass. Strict -Wall -Wextra -Wpedantic -Werror syntax
checks pass for CallbackClockTests and MeterTransferTests. The complete unchanged
377-observation graph and 616-observation audible-clock oracles remain passing
alongside the original follower/window/loop/MIDI/export/sync gates. The connected
capture/OD/SUB PCM audit retains 126 cases / 965,826 samples, maximum error
7.394757495227111e-08 at unchanged 1e-6 tolerance.

Rebuilt only the native JUCE processor test target in each SDK tree: Windows
1/1 and Linux 1/1. Actual processor tests cover raw optional timestamp acquisition,
full uint64 precision, zero frames, unavailable/detached playheads, offline mode,
reset, and audio-owner-only host reads at 64/128. These use a controlled JUCE
playhead; they are not physical output timestamp or device-latency measurements.
Runnable Standalone/plugin/test-host hashes match the preceding accepted SDK
artifacts; no new REAPER/Ardour/GUI/hardware/Apple acceptance is claimed.

Logs: build/callback-clock-{windows,ubuntu,sanitizers}.txt,
build/callback-clock-{windows,ubuntu,sanitizer}-final-core.txt,
build/callback-clock-processor-{windows,linux}.txt and
build/callback-clock-audio.json. Source, rebuilt processor test and unchanged
runnable/logo hashes are recorded in build/callback-clock-final-evidence.json.
Device output-latency/origin correlation, production audible-time selection,
graph-owned PCM binding and original renderer/IPC integration remain pending.
No package, SDK update, version bump or UI/reference asset change is made.

Prepared device output-latency report (25 October 2026): the owning Standalone
processor captures device sample rate/output latency during preparation, with
ownership guards and an optional controlled native IO reader. Engine validates
and replaces the report on every prepare; callback views and requested PCM carry
availability and sample count together. Reported zero remains distinct from
missing, negative, throwing or nonfinite/rate-mismatched reports. No driver query
is added to processBlock, message snapshots or state serialization.

Portable CTest passes 27/27 on Windows and 27/27 on Ubuntu. The six focused
clock/meter gates pass under ASan/UBSan. Strict -Wall -Wextra -Wpedantic -Werror
syntax checks pass for OutputLatencyTests and MeterTransferTests. Tests cover
8/48kHz, 64/128/zero/offline callbacks, maximum int sample count, known zero,
missing/error/rate guards, prepare/reset generations, exact snapshot pairing,
and every-field 4000-packet concurrent coherence including output-latency flags.
Actual monitored Engine PCM remains bit-identical with/without latency reports;
callback validation/publication retains zero tracked new/delete.

Rebuilt only the native JUCE processor tests: Windows 1/1 and Linux 1/1.
Controlled-device tests execute the actual processor preparation and prove that
device/reader queries do not recur during processing. They cover report refresh,
sample-rate change/mismatch, negative/zero/throwing IO, missing owned device,
and byte-identical saved parameter/MIDI state. This verifies the lifecycle/IO
adapter, not physical device latency. The complete unchanged original clock,
graph, follower and PCM-window oracles remain passing. The connected capture/
OD/SUB audit passes 126 cases / 965,826 samples, maximum error
7.394757495227111e-08 at unchanged 1e-6 tolerance.

Evidence: build/output-latency-{windows,ubuntu,sanitizers}.txt,
build/output-latency-processor-{windows,linux}.txt,
build/output-latency-audio.json and build/output-latency-final-evidence.json.
The manifest records source, rebuilt processor test and unchanged runnable SDK/
user-logo hashes. No app/plugin/test-host runnable was rebuilt or packaged;
no new REAPER/Ardour/GUI/hardware/Apple acceptance, SDK update or version bump
is claimed. The report is not an acoustic measurement or correlated output
timestamp; render-end/callback-edge/block-lead alignment and native audible-time
conversion remain pending before graph/renderer/IPC integration can be accepted.

Callback-delivery audible-time model (25 October 2026): Windows 27/27 and
Ubuntu 27/27 portable gates pass; the six focused clock/meter gates pass under
ASan/UBSan. Strict portable compilation passes with warnings treated as errors.
The new core test reports 19,200 controlled FIFO-timeline observations across
8/44.1/48/96 kHz, 64/128 blocks and five latency reports, at 1e-12 tolerance.
It tests callback-start rather than render-end alignment, signed startup delay,
slow consumers, raw-host-clock independence and atomic rejection of invalid,
offline, empty or mismatched correlations. Zero tracked new/delete is retained.
These are explicit-correlation model tests, not observations of a physical DAC.

The complete original JS clock, graph, meter and other existing parity gates
remain passing. The actual capture/OD/SUB PCM audit passes 126 cases / 965,826
samples, maximum error 7.394757495227111e-08 at the unchanged 1e-6 tolerance.
Evidence: build/audible-alignment-{windows,ubuntu,sanitizers}.txt,
build/audible-alignment-audio.json and build/audible-alignment-final-evidence.json.
The manifest verifies source hashes, the separate unchanged user-logo edit and
unchanged runnable SDK/processor-test binaries. No Engine or native processor
behavior changed, so those SDK targets were not rebuilt. No package, new DAW/
GUI/hardware/Apple acceptance or version bump is claimed. Actual output-delivery
clock acquisition/correlation, drift/xrun handling and graph/IPC/renderer wiring
remain pending; this does not complete the visible-meter feature.

Processor-entry clock capture (25 October 2026): Windows and Ubuntu each accept
all 27 portable gates after correcting a new core fixture. The initial complete
runs were 26/27: the fixture's extra invalid-clock callbacks left old EngineView
entries queued before its reset assertion. The fix consumes each view using the
existing consumer path; production queue/reset behavior was not changed. The
corrected core gate then passes independently on Windows and Ubuntu. ASan/UBSan
initially passed the five other focused gates and reproduces the same fixture
failure; the corrected core gate passes there too (six accepted gates). No
unaffected gates were repeated after this test-only repair. The pre-fix source
and all initial/recheck logs are retained. An initial Windows relink also met the
still-running old test executable's lock; relinking succeeded after it exited.

Rebuilt only the actual native processor test targets: Windows 1/1, Linux 1/1.
They verify preparation warm-up, exactly one noexcept counter-reader call per
processBlock at 64/128, zero/offline callbacks, invalid/known-zero input,
reprepare, no observation-reader calls from state/snapshot code, exact saved
parameter/MIDI bytes, default JUCE-counter bracketing independent of host seeks,
and zero tracked callback new/delete. Core tests retain 4000-packet every-field
coherence and bit-identical actual monitored PCM. Strict portable compilation
passes. All unchanged original JS gates pass. The capture/OD/SUB audio audit
passes 126 cases / 965,826 samples, maximum error 7.394757495227111e-08 at 1e-6.

Evidence: build/processor-entry-{windows,ubuntu,sanitizers}.txt,
build/processor-entry-core-recheck-{windows,ubuntu,asan}.txt,
build/processor-entry-native-{windows,linux}.txt, build/processor-entry-audio.json,
build/processor-entry-callback-fixture-before.cpp and
build/processor-entry-final-evidence.json. The manifest records retained
failures, corrected gates, source/native-test hashes and unchanged runnable SDK,
clock-policy and separate user-logo hashes. No package, new runnable app/plugin,
DAW/GUI/hardware/Apple acceptance, SDK update or version bump is claimed.
Processor entry follows host/device wrapper work and is not the output-delivery
edge; no automatic audible-time correlation, graph binding, IPC or renderer is
enabled. Those remain necessary before visible-meter acceptance.

Standalone IO-dispatch scope (26 October 2026): initial full portable runs pass
Windows 28/28 and Ubuntu 28/28; seven focused ASan/UBSan gates pass. They cover
the new LF/CRLF/fail-closed generator fixture, thread-local scoped ownership,
one-use/frame/nesting/thread guards, known-zero and invalid time, 64/128/zero/
offline/reset publication and 4000-packet field/PCM coherence. Monitored Engine
PCM remains bit-identical and scope/publication retain zero tracked new/delete.
Strict portable compilation passes. The unchanged original JS clock/graph/meter
and all other existing gates pass. The audio audit passes 126 cases / 965,826
samples at unchanged 1e-6 tolerance, maximum error 7.394757495227111e-08.

Review then added an actual native-processor regression: reprepare inside an
unconsumed IO scope must reject that scope's old timestamp. It failed before
the lifecycle token (build/device-dispatch-epoch-before.txt). The fix invalidates
old bindings before prepare, establishes the new token after success and checks
both captured and current tokens before consumption. Braced hook initialization
loads the token before sampling time. Core fixtures also reject prepare between
the token snapshot and the clock read. The updated core/generator gates pass
2/2 on Windows, Ubuntu and ASan/UBSan. Unchanged JS/PCM gates were not repeated
after this scope/lifecycle-only correction. The failure and pre-fix sources are
retained, and the final manifest distinguishes initial full runs from rechecks.

Both actual Standalone wrappers and native processor test targets were rebuilt
on Windows and Linux. Final SDK-generation/processor gates pass 2/2 per platform,
including the reproduced lifecycle case, stop/reprepare, owner/frame/single-use,
unchanged serialized parameters/MIDI and callback allocation checks. Reversing
the scoped insertion, internal binding declaration and wrapper include restores
the exact pinned JUCE holder/application bytes. Original SDK sources, original
clock policies, other plugin-format/test-host binaries and the separate user-
logo edit retain their recorded hashes. No package, new plugin-format build,
GUI/DAW/controller/physical-driver or Apple acceptance is claimed.

Evidence: build/device-dispatch-{windows,ubuntu,sanitizers}.txt,
build/device-dispatch-epoch-core-{windows,ubuntu,asan}.txt,
build/device-dispatch-epoch-native-{windows,linux}.txt,
build/device-dispatch-epoch-before.txt, build/device-dispatch-audio.json and
build/device-dispatch-final-evidence.json. The manifest also hashes rebuilt
Standalone/processor artifacts and generated source files. This observes holder
dispatch after device-manager work; it is neither driver entry nor a calibrated
output timestamp or device-running signal. Physical/manager correlation,
drift/xruns and graph/IPC/renderer integration still gate visible meters.

Enclosing-buffer split timing (26 October 2026): a new independent FIFO fixture
reproduced an offset-sized lead when the old pure model received a timestamp for
an enclosing device buffer. The failing output is retained in build/split-before.txt.
The model now validates an explicit total/offset and computes from the enclosing
first frame. The original 19,200 whole-callback observations remain unchanged;
13,824 additional observations cover four rates, 64/128 splits, one-sample and
partial tails, startup/long latency, delayed consumers and invalid ranges at
the unchanged 1e-12 tolerance. Rejections preserve the previous output.

Initial full portable suites pass Windows 28/28 and Ubuntu 28/28. Six focused
ASan/UBSan gates pass. The unchanged audio audit passes 126 cases / 965,826
samples, max error 7.394757495227111e-08 at 1e-6. Native SDK/processor gates pass
2/2 on each platform, including 72 callbacks through an exact test-only copy of
JUCE's pinned CallbackMaxSizeEnforcer. Production processors compare nonzero
monitored PCM bit-for-bit and saved parameter/MIDI bytes. Tests retain optional
raw callback context, exact local block ranges, partial/empty buffers, two
start/stop cycles and JUCE's zero-maximum silence-without-processing guard.
All checked callback paths have zero tracked new/delete.

Final review changed a test loop to use a const reference (strict -Werror caught
the unnecessary copy) and added independent channel-pointer offset assertions
to the native test. Only the changed core gate was rebuilt/rechecked on Windows,
Ubuntu and ASan/UBSan, plus the two native gates on each platform. These final
rechecks pass, as does strict portable compilation. Production model code was
unchanged after the initial full suites; unaffected JS/PCM checks were retained.

Evidence: build/split-{windows,ubuntu,asan}.txt,
build/split-native-{windows,linux}.txt,
build/split-final-core-{windows,ubuntu,asan}.txt,
build/split-final-native-{windows,linux}.txt and their detail logs,
build/split-strict-final.txt, build/split-before.txt, build/split-audio.json and
build/split-final-evidence.json. The manifest hashes tested sources, extracted
headers and rebuilt native test executables, and verifies unchanged SDK,
Standalone/plugin/test-host binaries, original clock/graph policy and separate
user-logo hashes. No app/plugin build, package, version bump or GUI/DAW/hardware/
Apple timing acceptance is claimed. This is controlled splitter/model validation;
production enclosing-delivery origin/range capture, drift/xruns and meter
graph/IPC/renderer wiring remain required before visible-meter acceptance.

Enclosing IO acquisition (26 October 2026): full portable suites pass Windows
28/28 and Ubuntu 28/28; six focused ASan/UBSan gates pass. Strict scope/packet
compilation passes. The unchanged audio audit passes 126 cases / 965,826 samples,
max error 7.394757495227111e-08 at 1e-6. Existing original-JS meter/graph/clock
oracles and the independent FIFO models retain their previous tolerances.
The 4000-packet concurrent check now varies every enclosing-delivery field too.
Owner/range/thread/nesting, invalid/empty clocks, prepare/stop, epoch-before-clock
ordering, invalidation during holder wait and exact Engine view/PCM pairing pass
with zero tracked new/delete and bit-identical monitored Engine PCM.

Both native processors and real Standalone wrappers were rebuilt on Windows and
Linux, including the generated audio_devices module and holder. SDK/processor
gates pass 2/2 per platform for 144 original/instrumented splitter callbacks,
captured entry bracketing and exact total/offset/identity, partial/empty/zero-max
buffers, restart, unchanged raw context, nonzero PCM/state identity and zero
tracked callback new/delete. Generator tests cover LF/CRLF, duplicate/drift
rejection, exact manager/cpp/mm reversal and exact generated test-body copies.
The original SDK checkout and other plugin-format/test-host binaries remain
unchanged. No Apple compilation, new GUI/DAW/controller or physical timing
acceptance is inferred from these checks.

An additional linked AudioDeviceManager fixture exercises the actual production
module's manager-owner handoff with a registered synthetic device type, without
opening platform audio devices. Windows passes 18 processor blocks at 64/128,
nonzero PCM/state identity and zero tracked steady callback new/delete after the
existing JUCE scratch-buffer warm-up. This does not claim the entire existing
manager/player is allocation- or lock-free, especially its first scratch resize.

On WSL, constructing that real manager registers JUCE's MIDI observer, which
asserts when /dev/snd/seq is absent. The first extra-manager run fails the existing
CTest assertion rule; retain build/delivery-manager-linux.txt as evidence. The
fixture now checks that kernel-device prerequisite before constructing a real
manager and explicitly reports SKIP/unavailable if absent. The assertion rule
and JUCE MIDI code remain unchanged; the 144 original/instrumented splitter and
production-processor cases still run. Final native gates pass 2/2 on each host,
with the extra 18 real-manager blocks passed only on Windows. Actual Linux
manager acceptance remains unverified here and must run on a host providing the
sequencer. This capability limit is not a physical MIDI or audio pass.

Evidence: build/delivery-{windows,ubuntu,asan}.txt,
build/delivery-native-{windows,linux}.txt,
build/delivery-manager-{windows,linux}.txt,
build/delivery-final-native-{windows,linux}.txt and their detail/build logs,
build/delivery-strict.txt, build/delivery-audio.json and
build/delivery-final-evidence.json. The final manifest distinguishes the retained
Linux failure, explicit capability skip, Windows real-manager acceptance and
unchanged initial full suites from the final fixture-only native rechecks.
It hashes sources, generated copies, rebuilt Standalone/test artifacts and
unchanged SDK, original policies, other plugin formats and separate user logo.
No package/version bump or automatic audible clock, graph/IPC/renderer is enabled.
Raw acquisition now observes JUCE's enclosing AudioIODevice callback, not an OS
driver interrupt or DAC timestamp. Drift/xruns, audible selection, visible meters
and physical acceptance remain unfinished.

### WSL ALSA sequencer enabled (26 October 2026)

The earlier missing-sequencer limitation above is resolved on this machine.
Installed alsa-utils and built the matching official Microsoft WSL
linux-msft-wsl-6.18.54.1 kernel with SND_SEQUENCER, SND_SEQ_MIDI,
SND_SEQ_MIDI_EVENT and SND_SEQ_DEVICE as modules. The active release is
6.18.54.1-microsoft-standard-WSL2-aion-midi. Matching modules and headers were
staged using Microsoft's artifact VHDX generator; module vermagic, artifact
SHA256 hashes and the image filesystem check pass. Activation followed explicit
user approval for the global WSL restart, with configuration backup/rollback.

Normal user jorge now belongs to audio; /dev/snd/seq is a real character device
(root:audio, 0660), and unprivileged aconnect -l succeeds with the kernel System
Timer/Announce ports. A dedicated modules-load.d entry loads snd-seq at startup.

Without rebuilding or changing application/test sources, native SDK generation
and processor gates pass 2/2 in build/alsa-midi-native-linux.txt. The processor
test now actually runs and passes all 18 linked production AudioDeviceManager
blocks at 64/128, alongside the 144 original/instrumented splitter callbacks.
There is no missing-sequencer skip or JUCE assertion in this run. The old failed
and skipped logs remain intact. Evidence: build/alsa-midi-environment.json.

This establishes the synthetic-device manager fixture on Linux and real ALSA
sequencer access, not physical USB MIDI, expression-pedal response, hardware
audio timing, visible meters or new DAW/platform acceptance. No application
package, version bump or UI change was made.

### Captured device audible-time adapter (26 October 2026)

capturedDeviceAudibleTime now builds the existing FIFO-model correlation from
the actual captured enclosing AudioIODevice edge and its original total/offset.
The caller must explicitly establish a nonzero shared native counter origin and
provide the current generation. It requires nonempty live callbacks, known
prepared output latency and ordered delivery/holder/processor/wall observations.
Missing, malformed, backwards, mismatched or offline data rejects without
changing the caller's previous result. Host nanoseconds never establish the
native origin. No device query, allocation, lock or audio scheduling is added.

Retained the original 19,200 unsplit and 13,824 split independent FIFO cases and
added 12,288 captured-edge adapter observations at 8/44.1/48/96 kHz, 64/128,
known-zero/long latency, partial buffers, startup and delayed reads. Native tests
also call the adapter through the instrumented splitter and real linked manager
using the same JUCE high-resolution counter as acquisition, with an independent
FIFO expression for the known synthetic range/latency. Original splitter and
synthetic-anchor controls, exact nonzero PCM/state identity and zero tracked
callback new/delete remain intact. The full manager's existing first scratch
allocation and locks are still outside this steady allocation acceptance.

This adapter has controlled test callers only. It does not establish device
continuity across xruns/restarts, graph/PCM freshness or physical DAC timing.
Production audible selection, graph binding, IPC and the renderer remain
unfinished. Original JS/HTML/CSS, SDK sources and the separate logo edit are
preserved; no application package or Standalone wrapper rebuild is required.

Validation passes: 28/28 portable gates on Windows and Ubuntu (the full core gate
and remaining 27 run separately), 2/2 native SDK/processor gates on each platform,
and the full core gate under ASan/UBSan. Both platforms execute the extra 18 real
linked-manager blocks with no missing-sequencer skip or assertion. The unchanged
JS clock/meter/graph/window and other reference comparisons retain their original
tolerances. The PCM audit passes 126 cases/965,826 samples, maximum absolute
error 7.394757495227111e-08 at 1e-6, within its existing committed capture/OD/SUB
scope; it does not cover browser resampling, latency compensation or envelopes.

Evidence: build/captured-clock-core-{windows,linux}.txt and detail snapshots,
build/captured-clock-rest-{windows,linux}.txt,
build/captured-clock-native-{windows,linux}.txt, build/captured-clock-asan.txt
and its detail log, build/captured-clock-audio.json and
build/captured-clock-evidence.json. The manifest verifies source/log hashes and
unchanged SDK/generated hooks, JS policies, Standalone/other-format binaries and
the separate logo hash. No new GUI, DAW, physical controller, Apple or visible
meter acceptance is claimed from this conversion helper.

### Owned production message playback reader (26 October 2026)

AionProcessor::readDevicePlayback now supplies the native read boundary around
the captured-device model. It accepts no foreign callback argument, consumes at
most eight coherent Engine views into its own message cache, and matches the
current generation. A private atomic active session is cleared before release
or prepare counter warm-up and published only after successful owned preparation.
The reader rechecks session/generation after its native counter read and writes
the result only on success. No driver query, host timestamp interpretation or
audio-owned frame read is added. The processBlock body and all core DSP remain
unchanged. Only controlled native callers currently use this API; no UI/IPC
fields or automatic metering are enabled.

Native fixtures compare reads against external JUCE counter brackets and require
unbound owners, empty prepared generations, stop/reprepare with retained cache
and release inside the exact wall read to reject without mutating output. The
same controlled native counter seam then proves fresh 64/128 recovery. The
historical pure model is deliberately still accepted after release, showing why
its numbers alone cannot establish a live lifecycle. Original/instrumented
splitters, linked real manager, exact nonzero PCM/state identity and zero tracked
steady callback new/delete remain required.

This is the processor prepare/release lifecycle, not a physical device-running
flag. In the pinned JUCE SDK, xrun reports can be unavailable or combine input
and output events; their public contract does not certify an output FIFO clock.
No universal cross-thread driver polling or jitter threshold was added. Device
continuity/drift/xruns, graph/PCM freshness, renderer/IPC and physical acceptance
remain unfinished. See JUCE's AudioIODevice getXRunCount documentation:
[JUCE AudioIODevice documentation](https://docs.juce.com/master/classjuce_1_1AudioIODevice.html).

Validation: rebuilt existing native processor tests and real Standalone wrappers
on Windows and Ubuntu. Native SDK/processor gates pass 2/2 on each, including 18
linked-manager blocks without skips and the new lifecycle-read regressions.
The seven focused unchanged-JS meter/clock/frontend/bridge gates pass on each OS.
PCM audit: 126 cases, 965,826 samples, maximum absolute error
7.394757495227111e-08 at the unchanged 1e-6 tolerance, within its existing
committed capture/OD/SUB scope. The prior 28/28 portable and ASan/UBSan core
results are retained, not rerun or claimed as sanitizer coverage of this new
native method; all core sources and processBlock are unchanged.

Evidence: build/owned-clock-build-{windows,linux}.txt,
build/owned-clock-native-{windows,linux}.txt,
build/owned-clock-reference-{windows,linux}.txt, build/owned-clock-audio.json and
build/owned-clock-evidence.json. The manifest hashes changed sources, logs and
rebuilt artifacts; verifies unchanged core/processBlock, original policies,
SDK/generated hooks, other-format binaries and separate user logo; and records
prior full-suite evidence distinctly from this increment's targeted checks.
No package/version bump or new GUI/DAW/physical/Apple acceptance is claimed.

### Generation-bound meter graph requests (26 October 2026)

MeterTransfer now carries an optional nonzero graph ticket with its requested
Engine generation in one always-lock-free uint64 atomic word per track. The
existing request bit mask is only a wake hint. Audio claims a request before
filling PCM; a newer request during fill cannot re-tag that old event. Pending
queue-full requests may coalesce, but recovery recaptures current PCM rather
than relabelling an old scratch buffer. Reset cancels pending words without
changing queue ownership. Old-generation requests are dropped before fill;
legacy diagnostic replies explicitly retain graphTicket zero. The existing
seven usable slots, two-window copy limit and twenty-track fairness remain.

LoopMeterBinding/observeBoundLoopMeterWindow compose explicit current graph
identity and request provenance with existing source and exact callback guards
before invoking unchanged follower code. Native sourceRevision is not an
analyser identity. The decisive actual-Engine counterexample keeps the same
source and exact callback after explicit graph teardown/recreation: both old
native identity helpers still match, but the new graph ticket rejects the reply.
A fresh bound response recovers; healthy native RTRG changes sourceRevision while
retaining analyser/data identity and history. Twenty-three stale/malformed
graph/source/view/callback/time cases reject without changing the meter value.

Transfer tests cover coalescing, a request crossing fill, immutable full-queue
tickets, fresh recapture after saturation, stale generation/reset, all generation
and ticket bits, and 4000 real-owner exchanges alternating bound/unbound requests.
Actual paired Engines at 8/48 kHz and 64/128 carry exact tickets with identical
stereo PCM and current windows/metadata; both owners retain zero tracked
new/delete. Graph composition uses actual Engine packets with supplied clock
results, not a physical/native output timestamp. Existing original-JS graph,
window, meter and clock oracles and tolerances remain unchanged.

Tickets certify request provenance only. They do not initialise a new native
analyser window, derive graph transitions from snapshots, deliver the original
25/50ms cleanup timers, establish device continuity or enable a renderer.
Explicit graph notification/window-reset mapping and live source/view/clock
pairing remain required. The receipt issuer must not reuse a ticket for another
graph during one Engine generation. No UI/IPC metering or package is enabled.

Validation passes: full portable suites 28/28 on Windows and Ubuntu; ASan/UBSan
core/meter/clock gates 6/6; rebuilt native processor and Standalone targets on
both systems; native SDK/processor gates 2/2 per host, including all 18 linked
real-manager blocks without skips. PCM audit passes 126 cases/965,826 samples,
maximum absolute error 7.394757495227111e-08 at the unchanged 1e-6 tolerance,
within its existing committed capture/OD/SUB scope. No tolerance or preserved
source was changed to obtain these results.

Evidence: build/graph-ticket-full-{windows,linux}.txt,
build/graph-ticket-asan.txt, build/graph-ticket-native-{windows,linux}.txt,
their build logs, build/graph-ticket-audio.json and build/graph-ticket-evidence.json.
The manifest hashes changed sources, test logs and rebuilt artifacts, and checks
unchanged audio/DSP/processor/graph policies, SDK/generated hooks, original JS,
other-format binaries and separate user logo. No new physical controller, DAW,
GUI, native graph-notification or Apple acceptance is inferred.

### Explicit sample-timed graph-window starts (26 October 2026)

The audio owner accepts an internal ordered integer-ticket start notification,
resets its window in O(1) at the actual application frame and publishes separate
request/window tickets plus reset frame. Healthy retriggers retain history;
zero/repeated/decreasing tickets cannot reset it. Clear invalidates ownership but
retains the ticket high-water mark and existing raw diagnostic fade tail. Prepare
resets ownership under its generation barrier. Bound observations additionally
reject absent window ownership and future reset frames without follower edits.

Tests independently reconstruct every sample from a second Engine's actual
stereo output, resetting at sample 17 inside 64/128 callbacks, through multiple
wraps. They check full uint32 tickets, future requests, duplicates, clear/prepare,
36 stale-ring/reset/fill combinations and unchanged UI acknowledgements/rejection
flags. Final review reproduced MIDI Stop incorrectly acknowledging cancelled
future graph notifications; build/graph-reset-panic-before.txt retains that
failure. The corrected bounded cancellation drains pending and queued internal
notifications without acknowledging UI actions; the next ordinary UI command
applies immediately. The regression includes zero-sample MIDI Stop at 64/128.
Tracked callback new/delete remain zero; this is not malloc instrumentation.

Full portable regressions passed 28/28 on Windows and Ubuntu before the final
narrow panic-acknowledgement correction. Final sources then passed focused
core/meter/clock gates 6/6 per OS, including all core MIDI tests and the new
cancellation fixture; final ASan/UBSan passed 6/6. Processor and Standalone targets
were rebuilt after the correction. Native gates passed 2/2 per OS, including all
18 linked AudioDeviceManager blocks without skips/assertions. The final committed
capture/OD/SUB PCM audit passed 126 cases/965,826 samples, maximum error
7.394757495227111e-08 at unchanged 1e-6 tolerance. Playback envelopes, browser
resampling and latency compensation remain outside that PCM audit's scope.

Evidence: build/graph-reset-ctest-{windows,linux}.txt (full),
build/graph-reset-final-ctest-{windows,linux}.txt (final focused),
build/graph-reset-ctest-asan.txt,
build/graph-reset-native-ctest-{windows,linux}.txt, their build logs,
build/graph-reset-audio.json and build/graph-reset-evidence.json.
The checked writer build/WriteGraphResetEvidence.py records final source/log/
artifact hashes, full-run pre-correction source hashes and the retained failure
hash. It verifies unchanged original UI, DSP, processor, clock and SDK hooks,
untouched other-format binaries and the separate user logo. No preserved source
or comparison tolerance was changed to obtain these results.

This is still a primitive without production graph-notification/cleanup timer
mapping, device-continuity proof, coherent live renderer pairing or visible
meters. No new DAW, GUI, physical-controller/DAC or Apple acceptance is claimed.
No UI/IPC enable, package, version bump or push is included.

### Lifecycle owner and nonblocking future starts (26 October 2026)

LoopMeterLifecycle now composes explicit graph notifications, queue acceptance,
bindings and externally delivered stop-cleanup receipts. It is noncopyable and
must persist per Engine/track across editor reopen. Same-generation reset cannot
recycle tickets; changed-generation reset follows the prepare barrier. Queue
saturation leaves graph, reading, binding, cleanup request and ticket unchanged.
Buffer removal, stale-generation packets and invalid/cancelled/superseded timer
receipts reject without follower mutation. Original queued->play deliberately
retains a stop timer that can detach the replacement graph; stopping->play cancels
it. Teardown/attached-versus-detached clear and healthy RTRG history are preserved.

The unchanged original Loop/AudioGraph trace now compares 377 seven-field owner
state/identity/cleanup-delay observations in addition to its existing 14-field
policy/follower comparison. Audio/timer IO is stubbed in that oracle. Separately,
paired actual Engines at 64/128 check window receipt gating, sample-17 starts,
healthy retrigger ownership, queue failure/recovery and exact stereo PCM.

A further acceptance test reproduced future meter work at the performance FIFO
head delaying an ordinary volume command. The failure is retained at
build/lifecycle-owner-future-before.txt. The fix transfers future meter starts
into a construction-time, fixed 256-entry audio-owned min-heap ordered by sample
frame and arrival. Transfers and due applications share the existing 128-command
callback budget. Full storage never waits or grows and cannot block ordinary
controls behind a future timestamp. Its rejection does not acknowledge/reject a
UI action; only an actual matching window receipt permits observation. Clear
invalidates old clear epochs, including DEL followed by a newly committed clip;
UI/MIDI Stop All and prepare cancel deferred starts. Tests also cover full integer
precision, same-frame FIFO order, compaction and zero-sample MIDI Stop.

Final evidence: focused core/meter/clock gates 6/6 per OS, full portable suites
28/28 per OS, ASan/UBSan 6/6. Native processor and Standalone targets were rebuilt;
native gates passed 2/2 per OS with all 18 linked AudioDeviceManager blocks and
no skips/assertions. Callback fixtures retain zero tracked new/delete; they do
not instrument malloc or prove a worst-case physical device deadline. The actual
committed capture/OD/SUB PCM audit passed 126 cases/965,826 samples with maximum
error 7.394757495227111e-08 at unchanged 1e-6 tolerance; its existing envelope,
browser-resampling and latency exclusions remain. Asset byte/deterministic
embedding/tamper checks also passed against the local .reference/Ascii-Looper.

Logs: build/lifecycle-owner-ctest-{windows,linux,asan}.txt,
build/lifecycle-owner-full-ctest-{windows,linux}.txt,
build/lifecycle-owner-native-ctest-{windows,linux}.txt, their build logs,
build/lifecycle-owner-audio.json and build/lifecycle-owner-assets.txt.
The checked writer build/WriteLifecycleOwnerEvidence.py records gate/source/
artifact/failure hashes in build/lifecycle-owner-evidence.json and verifies
unchanged original UI/DSP/clock/SDK hooks, other-format binaries and separate logo.

This is still an internal owner and scheduler boundary. A production source of
validated lifecycle notifications, real timer scheduling, graph-specific PCM
ownership during transitions, coherent live clock/device continuity and renderer
integration remain unfinished. The coordinator's notification methods do not
replace DSP play/stop/clear actions. No new GUI/DAW/controller/DAC or Apple
acceptance is inferred. No UI/IPC activation, package, version bump or push.

### Raw meter-window mutation event producer — 26 October 2026

Base: local `fd74186`. Added the bounded runtime receipt producer directly at
accepted LoopBank meter-window resets and clear mutations. No playback graph
birth is inferred from sourceRevision or a snapshot. New tests retain exact
sample-17 execution at 64/128, duplicate/no-clip rejection, healthy RTRG retention,
DEL frame/epoch, ordered twenty-track Clear All, full-width integer fields,
latched overflow with untouched pop destination, performance command progress,
stale-generation discard and fresh prepare recovery. Paired actual Engines keep
bit-identical stereo PCM with zero tracked callback new/delete.

Final unchanged-source gates:

- Windows complete portable suite: **28/28**, 351.44s,
  `build/graph-events-full-ctest-windows.txt`.
- Ubuntu complete portable suite: **28/28**, 556.59s,
  `build/graph-events-full-ctest-linux.txt`.
- ASan/UBSan realtime/clock/meter gates: **6/6**, 181.08s,
  `build/graph-events-ctest-asan.txt`.
- Rebuilt production processor/Standalone checks: Windows **2/2**, Ubuntu **2/2**,
  `build/graph-events-native-ctest-{windows,linux}.txt`. Both include all eighteen
  linked AudioDeviceManager synthetic blocks; no skipped ALSA gate, assertion or
  physical IO claim. Only these existing native targets were rebuilt.
- Original-JS supplied-event owner comparison remains **377 observations** on each
  OS. This is separate from the native raw-window event producer.
- Committed PCM audit: **126 cases / 965,826 samples**, maximum absolute error
  **7.394757495227111e-8** against unchanged **1e-6** tolerance;
  `build/graph-events-audio.json`. Scope remains capture/overdub/substitute through
  center pan, excluding playback envelopes, browser resampling, latency and sync.
- Asset gate: original UI byte identity, deterministic embedding and tamper
  rejection passed in `build/graph-events-assets.txt`. User logo edit remains
  separate and unchanged. Existing SDK instrumentation, other format artifacts,
  processor/editor/IPC and original reference assets are unchanged.

Evidence: `build/WriteGraphEventsEvidence.py` verifies gate totals, new event-test
PASS labels, native manager blocks, source/binary/log hashes and retained SDK/
other-format/logo hashes, producing `build/graph-events-evidence.json`.

Limits: these receipts certify raw-window reset/clear facts only. The existing
lifecycle owner still has no production playback/buffer/stop event connection or
wall-clock cleanup scheduler. Old/new graph-specific PCM, device continuity,
coherent audible-clock/renderer pairing, visible-meter and actual DAW/hardware/
Apple acceptance remain incomplete. Any lost stream remains invalid until prepare;
retained receipts require both matching generation and current stream validity.
No UI enablement, dependency/version change, package or remote push.

### Explicit unavailable observations in the meter owner — 26 October 2026

Base: local `c603735`. LoopMeterLifecycle previously exposed only the active bound
PCM path. Added `observeUnavailable` for explicit main-loop observations that need
no analyser window. It delegates silence history to the unchanged MeterFollower;
no device-clock query, timer, allocation, UI/IPC change or production caller was
added. A running attached active graph (including stopping) still rejects the
path atomically; missing buffer alone cannot replace required PCM. Nonfinite
times and prepare-stale owners also reject without follower mutation.

The actual Engine fixture records loud PCM, obtains accepted graph/window tickets
and seeds clipping/held peaks through the coherent callback path at 64/128. It
then checks all seven reading fields against the existing unchanged-JS-tested
LoopMeterGraph across audible-delay silence history, RMS release, peak hold/decay,
clip expiry, all twelve attached states, suspension with frozen supplied time,
missing context, destroyed graphs and fresh prepare recovery. Explicit assertions
retain the audible tail after teardown and verify expiry. Paired Engine stereo
PCM is bit-identical, with zero tracked callback new/delete. The 377-observation
original-JS graph oracle and seven-field lifecycle-owner oracle are unchanged;
this is not a claim of 377 full-field owner observations.

Current final gates:

- Windows targeted realtime/clock/meter suite **6/6**, 106.98s:
  `build/unavailable-owner-ctest-windows.txt`.
- Ubuntu same suite **6/6**, 56.37s:
  `build/unavailable-owner-ctest-linux.txt`.
- ASan/UBSan same suite **6/6**, 178.51s:
  `build/unavailable-owner-ctest-asan.txt`.
- Committed PCM audit **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8**, unchanged **1e-6** tolerance:
  `build/unavailable-owner-audio.json`. Scope excludes playback envelopes,
  browser resampling, latency compensation and sync.
- Original UI byte identity, deterministic embedding and tamper rejection passed:
  `build/unavailable-owner-assets.txt`.

Only the owner header and its test changed under source/tests; no production
translation unit includes this owner yet. The complete Windows/Ubuntu **28/28**
and native **2/2** per OS remain the `c603735` baseline, not rerun here. Their logs
and unchanged native/SDK/other-format artifact hashes are checked by
`build/WriteUnavailableOwnerEvidence.py`; current source/log/PCM/asset hashes and
the separate unchanged logo hash are recorded in
`build/unavailable-owner-evidence.json`.

Limits: explicit context/running/time model inputs do not prove backend suspension
or device continuity. Never use this path merely because PCM is late/stale or a
callback timed out. Production play/buffer/stop notifications, timer scheduling,
graph-specific overlap PCM, coherent live clock/renderer pairing and actual
DAW/hardware/Apple acceptance remain unfinished. No package, push, dependency or
version change; user's logo edit remains unstaged and unchanged.

### Lost lifecycle notifications invalidate owner observations — 26 October 2026

Base: `5eec35c`. Reproduced a missing invalidation guard in the lifecycle owner:
retain an actual clipped PCM packet and its matching old EngineView/callback,
fill all 255 graph-event slots, then execute DEL on the observed loop. The event
stream correctly latched loss, but owner.observe still accepted that internally
coherent old tuple. The pre-fix executable failed with:
`Lost lifecycle events must reject a retained coherent PCM/view/clock without follower mutation`
in `build/lost-owner-before.txt`.

The fix gates start, PCM request, bound observation and explicit unavailable
observation on current generation plus valid event stream, with generation checks
around the stream query. It preserves explicit stop, clear, teardown and timer
delivery so telemetry failure cannot strand cleanup. Same-generation reset or
queue drain cannot restore validity. Fresh prepare plus owner reset recovers the
unavailable path. Original detached-clear follower retention is preserved.

Actual Engine fixtures at 64/128 retain clipped history, lose a real DEL event,
verify subsequent volume acknowledgement/progress, reject the old coherent tuple
without changes to all seven follower readings, reject start without ticket/timer
mutation and reject silence fallback through queue drain. Explicit cleanup and
prepare recovery remain usable. Paired Engine stereo PCM is bit-identical with
zero tracked callback new/delete.

Final current gates:

- Windows targeted realtime/clock/meter **6/6**, 105.59s:
  `build/lost-owner-ctest-windows.txt`.
- Ubuntu same suite **6/6**, 56.89s:
  `build/lost-owner-ctest-linux.txt`.
- ASan/UBSan same suite **6/6**, 180.92s:
  `build/lost-owner-ctest-asan.txt`.
- Committed PCM **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8**, unchanged **1e-6** tolerance:
  `build/lost-owner-audio.json`. Capture/OD/SUB scope through center pan excludes
  playback envelopes, browser resampling, latency compensation and sync.
- Original UI byte identity, deterministic embedding and tamper rejection:
  `build/lost-owner-assets.txt`.
- Original-JS graph oracle and seven-field owner lifecycle comparison remain
  unchanged, including the existing 377 supplied lifecycle observations.

Only the owner header and test changed under source/tests; no production
translation unit includes the owner. Full portable **28/28** and native **2/2** per
OS remain the preserved `c603735` baseline, not rerun here. Evidence writer
`build/WriteLostOwnerEvidence.py` verifies current gate totals/new PASS labels,
pre-fix failure, source/log/PCM/asset hashes, inherited logs and unchanged native/
SDK/other-format artifacts plus the separate logo edit, producing
`build/lost-owner-evidence.json`.

Limits: this specifically rejects evidence after latched event loss. It does not
supply a production notification consumer, normal clear/view freshness guarantee,
cleanup scheduler, per-graph overlap PCM, backend suspension/continuity evidence
or live renderer pairing. GUI/DAW/hardware/Apple acceptance remains incomplete.
No production audio/UI change, package, dependency/version change or remote push.

### Current raw-window identity rejects normal DEL immediately — 26 October 2026

Base: local `74a958d`. Reproduced the no-overflow counterpart of the lost-DEL bug:
retain an actual clipped PCM packet plus its coherent old EngineView/callback,
execute normal DEL, and observe before consuming its event. The stream remained
healthy, so the owner accepted the old tuple. The pre-fix executable failed with
`Normal DEL must reject retained coherent PCM before its event is consumed` in
`build/clear-owner-before.txt`.

MeterGraphEvents now publishes one always-lock-free uint64 generation/ticket word
per track at accepted window reset/clear. Clear sets ticket zero before enqueueing
its event; prepare clears the words. The owner requires this current accepted
window identity in addition to existing graph/source/view/callback checks and the
loss latch. No queue consumption or snapshot diff establishes that identity.
Requests may still precede actual reset acknowledgement. Other tracks retain
their identity, and normal DEL can recover in the same generation with the next
accepted graph ticket.

Current tests retain real clipped audio, reject the old coherent tuple before and
after clear-event consumption without changes to any of seven follower fields,
record a fresh same-generation clip/ticket and accept its actual reset receipt,
and preserve unrelated-track isolation at 64/128 with bit-identical paired stereo
PCM and zero tracked callback new/delete. Direct token tests preserve full-width
generation/ticket values, reject invalid/old identities and exercise 4000 explicit
start/clear events between producer and consumer threads without tracked new/delete.
Test-harness synchronization is not part of the audio implementation.

Final gates on unchanged final source:

- Windows complete portable suite **28/28**, 354.60s:
  `build/clear-owner-full-ctest-windows.txt`.
- Ubuntu complete portable suite **28/28**, 585.19s:
  `build/clear-owner-full-ctest-linux.txt`.
- ASan/UBSan realtime/clock/meter **6/6**, 191.15s:
  `build/clear-owner-ctest-asan.txt`.
- Rebuilt production processor/Standalone checks **2/2** each on Windows/Ubuntu:
  `build/clear-owner-native-ctest-{windows,linux}.txt`. Both contain all eighteen
  linked production AudioDeviceManager synthetic blocks, with no skipped ALSA
  gate or JUCE assertion. No physical IO evidence is claimed.
- Committed PCM audit **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8**, unchanged **1e-6** tolerance:
  `build/clear-owner-audio.json`. Capture/OD/SUB scope excludes playback envelopes,
  browser resampling, latency compensation and sync.
- Original UI byte identity, deterministic embedding and tamper rejection:
  `build/clear-owner-assets.txt`.
- Original-JS graph oracle and seven-field owner lifecycle comparison remain
  unchanged, including all 377 supplied lifecycle observations.

`build/WriteClearOwnerEvidence.py` verifies gate totals/new PASS labels, pre-fix
failure, source/log/binary/PCM/asset hashes, unchanged production processor/editor/
IPC, core implementation bodies, SDK instrumentation, other-format artifacts and
separate user-logo hash, producing `build/clear-owner-evidence.json`.

Limits: the atomic word proves current explicit raw-window identity only, not
source/analyser graph construction, current device delivery or future continuity.
Production playback/buffer/stop event consumption, cleanup timers, per-graph
overlap PCM, coherent live clock/renderer pairing and GUI/DAW/hardware/Apple
acceptance remain unfinished. No original UI or audio sample algorithm change,
package, dependency/version change or remote push. Only existing native processor
and Standalone targets were rebuilt; user's logo edit remains separate and unchanged.

### Transactional owner observations and suspended-after-DEL rejection — 26 October 2026

Base: local `918f909`. Reproduced another observation-path gap: after normal DEL,
with its event unconsumed and stream healthy, the active PCM path rejected old
ownership but a supplied suspended-context observation still changed the old
follower. The pre-fix executable failed with
`Normal DEL must reject suspended-context observation before consuming clear`
in `build/observation-commit-before.txt`.

While a receipt remains attached, explicit unavailable observations now also
require its current audio-owned window identity. Both observation paths calculate
into a fixed-size local LoopMeterGraph candidate and commit only after a second
evidence check. A rejected precheck, failed calculation or detected invalidation
at the postcheck leaves the original graph/follower untouched. Explicit detached
receipt-zero silence remains supported. Copying is message-owner work; no audio
callback copy, allocation, wait, device query or timer was introduced.

The new deterministic fixture seeds from actual recorded clipped PCM, calculates
a candidate under supplied policy clocks, then executes actual paired Engine DEL
at the second guard query. It checks rejection and compares subsequent hidden
history, peak/clip deadlines and decay with an untouched reference trajectory.
This places a specific invalidation interleaving; it is not a physical timing or
exhaustive concurrency claim. Additional cases reject before calculation, reject
failed calculation and accept healthy commit. All actual paired Engine stereo
PCM remains bit-identical at 64/128 with zero tracked callback new/delete.

Final current gates:

- Windows targeted realtime/clock/meter **6/6**, 102.21s:
  `build/observation-commit-ctest-windows.txt`.
- Ubuntu same suite **6/6**, 57.55s:
  `build/observation-commit-ctest-linux.txt`.
- ASan/UBSan same suite **6/6**, 184.82s:
  `build/observation-commit-ctest-asan.txt`.
- Committed PCM **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8**, unchanged **1e-6** tolerance:
  `build/observation-commit-audio.json`. Capture/OD/SUB scope excludes playback
  envelopes, browser resampling, latency compensation and sync.
- Original UI byte identity, deterministic embedding and tamper rejection:
  `build/observation-commit-assets.txt`.
- Original-JS graph oracle and seven-field owner lifecycle comparison remain
  unchanged, including 377 supplied lifecycle observations.

Only owner/observation headers and the test changed under source/tests; they still
have no production caller. The full portable **28/28** and native **2/2** per OS
are preserved `918f909` baseline gates, not rerun here. Native binaries, SDK hooks,
other-format artifacts and the separate logo edit remain byte-identical. Evidence
writer `build/WriteObservationCommitEvidence.py` verifies these hashes plus current
source/log/PCM/asset hashes and the pre-fix failure, producing
`build/observation-commit-evidence.json`.

Limits: the final guard is the observation's acceptance point and cannot prevent
later audio transitions. It does not dispatch events, implement cleanup timers,
prove current physical delivery/device continuity or establish coherent live
renderer pairing. Production playback/buffer/stop wiring, per-graph overlap PCM
and GUI/DAW/hardware/Apple acceptance remain unfinished. No original UI change,
package, version/dependency change or remote push.

### Meter cleanup deadline and real JUCE timer (26 October 2026)

Added a one-receipt wall-deadline policy and a message-thread-only JUCE Timer
adapter around explicit owner stop notifications. Stop captures the wall origin
before mutating the owner; later synchronisation cannot discover/restart a delay.
Delivery validates full serial/generation, checks the absolute deadline and only
then invokes owner teardown. Preserve 25ms immediate / scheduled delta +50ms,
repeated-stop replacement, stopping-play cancellation and queued-play retention.
The component is noncopyable and stopped/destroyed on its owning message thread.

Portable cases cover exact deadlines, duplicate arm without postponement,
replacement/cancellation, full-width identities, invalid/backwards/nonfinite/
unrepresentable clocks and failure-output preservation with zero tracked
new/delete. Native cases run the actual desktop MessageManager dispatch loop,
not an internal synchronous timer helper. They exercise delayed delivery after a
blocked message thread, frequent synchronisation without starvation, cancellation,
replacement, the queued-play quirk, prepare-stale delivery/recovery and destruction.
The owners are seeded with actual recorded Engine PCM at 64/128; paired stereo
output stays bit-identical and cleanup preserves data/all seven readings while
audio sample time is frozen. Only Engine callbacks are under the native allocation
watch; message-thread Timer scheduling may allocate/lock as part of JUCE.

Current portable gates: Windows 6/6 (106.89s), Ubuntu 6/6 (56.34s), ASan/UBSan
6/6 (180.06s). Committed PCM: 126 cases / 965,826 samples; maximum absolute error
7.394757495227111e-8 at the unchanged 1e-6 tolerance. Original UI byte identity,
deterministic embedding and tamper rejection pass. Detailed native results and
evidence hashes are recorded below after final delivery checks.

The full portable 28/28 suites are inherited 918f909 baseline evidence, not rerun
for this component. Existing production processor, original UI, SDK hooks,
Standalone/other-format binaries and user logo are hash-checked separately;
only native test executables are rebuilt. The adapter still has no production
playback/buffer/stop notification caller. Complete lifecycle dispatch, per-graph
overlap PCM ownership, backend continuity and coherent live renderer pairing,
plus GUI/DAW/hardware/Apple acceptance remain unfinished. No package, dependency/
version change or remote push. The original meter feature is not yet accepted.

Final native gates: Windows **2/2**, 5.25s; Ubuntu **2/2**, 11.33s. Both include
actual JUCE message-loop timer delivery and the retained **18 linked production
AudioDeviceManager blocks**, without skips, JUCE assertions or leak reports.
The final fixture also checks that a rejected nonfinite stop preserves the old
owner/deadline. Prepare-stale callback rejection waits for actual delivery rather
than assuming an upper callback latency; an independent watchdog detects failure
to deliver. Frequent synchronisation must not postpone pending cleanup.

Logs: `build/cleanup-timer-ctest-{windows,linux,asan}.txt`,
`build/cleanup-timer-native-{windows,linux}.txt`, `build/cleanup-timer-audio.json`
and `build/cleanup-timer-assets.txt`. `build/WriteCleanupTimerEvidence.py` verifies
current gate results, original-JS lifecycle coverage, inherited full-suite logs,
source/PCM/assets hashes, rebuilt native test executables and unchanged production/
SDK/other-format/logo hashes, producing `build/cleanup-timer-evidence.json`.

### Clear receipt to meter owner and JUCE timer (26 October 2026)

Added the actual removed-window identity to native raw clear publication, and an
explicit receipt handoff to LoopMeterLifecycle / LoopMeterCleanupTimer. Current
cleared-window ticket remains zero; retiredTicket is the preceding audio-published
window identity, captured before invalidation. A repeated clear without a new
start retires zero. The existing fixed SPSC is still constructed on the heap;
publication adds constant atomic/metadata work only at native mutations.

Owner receipt delivery requires the current generation/track and matching nonzero
retained ticket, a new serial and a healthy stream before/after candidate clear.
Attached history/data resets, while clear delivered after teardown preserves the
original detached holds/data. Accepted clear cancels cleanup metadata; the JUCE
adapter also cancels its native timer. Failed delivery preserves prior metadata
and all seven readings.

The core fixture uses actual recorded clipped PCM and actual DEL at sample 17 for
64/128 blocks. It checks foreign fields, duplicate delivery, old already-consumed
AND previously unconsumed clear after same-generation replacement, detached
history retention, retained receipt after event loss and prepare rejection, with
exact paired Engine stereo PCM and zero tracked new/delete. Stream checks retain
4000 concurrent start/clear exchanges, now checking removed identities, full-width
uint32 tickets and repeated-clear zero. The native fixture runs the real desktop
message loop, delivers actual Engine DEL while cleanup is pending, then records
and starts a new graph in the same generation and verifies no old callback detaches
it. The unchanged-JS 377-observation graph/owner oracle is retained.

This handoff assumes a sole consumer of actual Engine-pop receipts; it is not an
external message decoder or authenticity check. It does not itself drain the shared
stream, connect the production processor, infer play/buffer/stop notifications or
resolve zero-ticket pending/unaccepted graphs. Those mappings, per-graph overlap
PCM, backend continuity, renderer/GUI/DAW/hardware/Apple acceptance remain pending.
Original assets, DSP source, processor/build configuration and tolerances are
unchanged. Native tests and Standalone are rebuilt; other-format binaries retain
their prior hashes and are not new host acceptance. No package/version/dependency
change or remote push. Meter feature acceptance is still incomplete.

Review caught a further ordering case: report a newly recorded stopped buffer
after old DEL, before a new graph start. The retained window ticket still names
the old graph and cannot prove the new buffer belongs to that clear. A conservative
buffer-notice fence now rejects receipt delivery after any setBuffer notification
following graph start, until fresh graph ownership is established or the caller
uses explicitly ordered clear. The new actual DEL/record/buffer/no-graph regression
checks buffer, data and readings remain intact. The boolean API intentionally
cannot distinguish repeat notifications from new buffer identities, so ambiguous
cases remain unavailable pending complete buffer/lifecycle mapping.

Initial full suites passed Windows **28/28**, 357.40s, and Ubuntu **28/28**, 572.90s.
After the final message-owner-only fence, rerun the affected six realtime/clock/
meter gates, native processor/SDK gates and ASan/UBSan gates. The other **26 portable
executables** are hash-checked unchanged across this final guard rebuild; the full
28-test logs precede this final guard and are not described as a second full run.
Committed PCM passes **126 cases / 965,826 samples**, maximum absolute error
**7.394757495227111e-8**, unchanged **1e-6** tolerance; asset identity, deterministic
embedding and tamper checks pass. Capture/OD/SUB audit scope excludes playback
envelopes, browser resampling, latency compensation and sync.

Final checks after the buffer-notice fence:

- Windows affected realtime/clock/meter **6/6**, 110.07s.
- Ubuntu same affected suite **6/6**, 57.37s.
- ASan/UBSan same suite **6/6**, 177.21s.
- Windows native processor/SDK **2/2**, 4.55s.
- Ubuntu native processor/SDK **2/2**, 11.11s.

Both native runs retain actual pending-timer DEL/re-recorded recovery and **18
linked production AudioDeviceManager blocks**, with no skips/assertions/leaks.
Current logs are `build/clear-receipt-final-{windows,linux,asan}.txt` and
`build/clear-receipt-final-native-{windows,linux}.txt`; initial full logs are
`build/clear-receipt-full-{windows,linux}.txt`. The evidence writer
`build/WriteClearReceiptEvidence.py` verifies current results, initial full runs,
the 26 unaffected portable executables, original-JS lifecycle coverage, PCM/assets,
rebuilt native test/Standalone targets and unchanged SDK/other-format/logo hashes,
producing `build/clear-receipt-evidence.json`.

### Bounded shared clear dispatcher and generation-safe dequeue (27 October 2026)

The old MeterGraphEvents header is reproduced in build/dispatch-before with a
minimal diagnostic: publish generation 1, prepare/publish generation 2, call pop
with stale generation 1, then try the correct generation. Old code drains the
new receipt and fails. build/dispatch-stale-before.txt retains that failure.
The new conditional dequeue checks active ownership before advancing the head;
ordinary SpscQueue::pop remains byte-identical at the source-function level.
A budgeted event pop counts raw stale/lost dequeues and preserves output when no
valid event is returned. Core tests cover future receipt retention, partial stale
discard, zero work, lost-generation draining and fresh-generation recovery, plus
all previous concurrent field/PCM checks and zero tracked new/delete.

LoopMeterClearDispatcher accepts a complete fixed non-owning table for twenty
current same-Engine/track timers. Invalid tables consume no events. Drain clamps
to 255 raw dequeues, counts starts without graph-birth inference and applies clears
through existing owner/timer guards. Partial/rejected delivery is explicit; lost
streams can drain but cannot recover without prepare. Owners reset after the
prepare barrier before the dispatcher captures the new generation.

Native fixtures record/observe all twenty tracks and compare actual paired Engine
stereo at 64/128. All timers are pending during actual Clear All: seven deliveries
cancel/reset exactly seven owners while thirteen remain intact, then the remainder
is delivered. Missing/duplicate/foreign tables preserve all initial start facts.
Prepare/stale-owner/reset, raw stale budgets, unbound clears and event overflow /
loss are exercised. These controlled callbacks do not establish physical-device
continuity or full original overlapping-graph PCM. Message Timer cancellation uses
the actual JUCE API; existing actual message-loop timer tests remain enabled.

Production processor construction/service and complete play/buffer/stop mapping,
zero-ticket pending graphs, overlap PCM, backend continuity and renderer acceptance
remain unfinished. This dispatcher has a native acceptance caller, not a production
processor caller. No new GUI/DAW/hardware/Apple acceptance, visible meters, package,
version/dependency change or push. Original UI/native logo and DSP/processor source
remain unchanged; existing native test and Standalone targets are rebuilt.

Final current gates:

- Windows full portable **28/28**, 318.96s.
- Ubuntu full portable **28/28**, 560.08s.
- ASan/UBSan targeted realtime/clock/meter **6/6**, 172.78s.
- Windows native processor/SDK **2/2**, 6.34s.
- Ubuntu native processor/SDK **2/2**, 13.12s.
- Committed PCM **126 cases / 965,826 samples**, maximum absolute error
  **7.394757495227111e-8** at unchanged **1e-6** tolerance. Capture/OD/SUB scope
  excludes playback envelopes, browser resampling, latency compensation and sync.
- Original UI byte identity, deterministic embedding and tamper rejection pass.

Both native runs include the actual all-twenty-track dispatcher fixture, existing
real message-loop cleanup tests and **18 linked production AudioDeviceManager
blocks**, with no skips/assertions/leaks. Current logs are
build/dispatch-full-{windows,linux}.txt, build/dispatch-asan.txt,
build/dispatch-native-{windows,linux}.txt, build/dispatch-audio.json and
build/dispatch-assets.txt. build/WriteDispatchEvidence.py verifies current results,
the old-header failure and unchanged ordinary pop source, source/log hashes,
rebuilt native/Standalone targets and unchanged SDK/other-format/UI/logo evidence,
producing build/dispatch-evidence.json.

### Processor prepared-generation readiness (27 October 2026)

Reproduced against 05ed773: a successful callback followed by releaseResources
left snapshot audioReady=true because the Engine retains its generation. The
new production-processor regression fails with `Released processor must reject
its retained generation as ready` in build/readiness-before.txt; the preceding
processor source is retained in build/readiness-before-Processor.cpp.

The processor now invalidates a separate atomic prepared generation before any
prepare-time IO and on release, then publishes after completed prepare. Snapshots
require a current callback view from that completed generation. Device playback
reads also bracket this ownership. Tests at 64/128 cover unprepared/first-view
readiness, repeated release, reentrant snapshot during prepare-time latency IO,
direct reprepare and fresh callback recovery without device ownership, while
retaining parameter/MIDI serialization. processBlock and all core DSP are unchanged.

Current incremental gates:

- Windows native processor/SDK **2/2**, 5.87s; Ubuntu **2/2**, 13.36s.
- Selected realtime/clock/meter gates: Windows **6/6**, 87.07s; Ubuntu **6/6**,
  54.36s. Full portable and sanitizer gates were not rerun for this processor-only
  change; their preceding 05ed773 results remain above.
- Both native runs retain **18 linked production AudioDeviceManager blocks**,
  all-twenty-track dispatcher, real JUCE cleanup timer tests, actual paired PCM
  and zero tracked callback new/delete, with no skip/assertion/leak failures.
- Committed PCM audit **126 cases / 965,826 samples**, maximum absolute error
  **7.394757495227111e-8**, unchanged **1e-6** tolerance. Existing exclusions for
  playback envelopes/browser resampling/latency compensation/sync still apply.
- Original UI byte identity, deterministic embedding and tamper rejection pass;
  the committed native logo remains byte-identical.

Logs are build/readiness-{native,core}-{windows,linux}.txt,
build/readiness-audio.json and build/readiness-assets.txt. The evidence writer
build/WriteReadinessEvidence.py verifies these results, the old-code failure,
unchanged processBlock/core/assets/SDK/other-format artifacts and rebuilt native
test/Standalone hashes, producing build/readiness-evidence.json.

This fixes readiness reporting; it does not complete meter owner/timer runtime
notification mapping, overlap PCM ownership, backend continuity or rendering.
Visible meters remain disabled. No package, version/dependency change or push;
existing CLAP/VST3/LV2 binaries were not rebuilt or newly validated for this change.

### Production meter ownership and bounded clear service (27 October 2026)

The processor now owns twenty stable graph lifecycle objects in construction-time
heap storage. Bridge requests and its existing message timer share one raw-event
consumer with a 32-dequeue budget. Completed host prepare schedules the existing
Timer; owner generation changes and event consumption remain message-thread work.
Released processors skip meter work. No editor, enabled MIDI or export is needed
for autonomous prepared servicing. Start facts are counted without graph-birth
inference; unbound clears remain rejected.

Pinned reset rejects stale/zero/future prepare generations without changing graph,
history, bindings or cleanup. New-generation route capture can discard an already
lossy backlog, but neither capture nor draining repairs loss or applies lost facts.

A new native real-message-loop regression reproduced the idle-stop policy closing
the scheduler before prepare: build/meter-service-idle-before.txt ends with
`Unprepared idle service must not stop and race a host prepare wakeup`.
The pre-fix processor sources are retained in build/meter-service-before-idle-
Processor.{cpp,h}. The final shared timer stays scheduled once started until
processor destruction, removing the idle stop/start race and its extra flag.

At 64/128, native fixtures queue 240 actual Clear All receipts, drain exactly 32
through one request, then let the processor timer finish without an editor.
Host-thread release freezes owner counts with forty receipts pending; host-thread
reprepare resets through timer service alone and discards those old facts.
Overflow retains 255 raw receipts that drain without restoring health; fresh
prepare then restores all-twenty-track routing. The idle, never-prepared processor
keeps its scheduler but no meter generation/counts/health. Nonzero stereo PCM is
compared exactly against a separately driven processor and saved MIDI/parameters
remain unchanged. Existing actual Engine all-track and cleanup timer tests remain.

Full playback/buffer/stop notifications, cleanup mapping, pending graphs, overlap
PCM, backend continuity and visible rendering remain unfinished. Arbitrary
concurrent off-thread processor destruction is not established by these tests.
There is no package, push, version/dependency change or new plugin-format build.

Final gates for these sources:

- Windows native processor/SDK **2/2**, 5.44s; Ubuntu **2/2**, 14.08s.
- Selected realtime/clock/unchanged-JS meter gates: Windows **6/6**, 93.08s;
  Ubuntu **6/6**, 53.61s; ASan/UBSan **6/6**, 169.96s.
- Both native runs include the new production service fixture, existing actual
  all-twenty-track dispatcher/cleanup fixtures and **18 linked production
  AudioDeviceManager blocks**, without skips/assertion/leak failures.
- Committed PCM: **126 cases / 965,826 samples**, maximum absolute error
  **7.394757495227111e-8**, unchanged **1e-6** tolerance. Existing exclusions for
  envelopes, browser resampling, latency compensation and sync still apply.
- Original UI byte identity, deterministic embedding and tamper rejection pass;
  native logo bytes, SDK/generated wrappers and other-format artifacts remain
  unchanged. Existing native test and Standalone binaries were rebuilt only.

Logs: build/meter-service-{native,core}-{windows,linux}.txt,
build/meter-service-asan.txt, build/meter-service-audio.json and
build/meter-service-assets.txt. build/WriteMeterServiceEvidence.py checks the
results, reproduced idle-stop failure, unchanged processBlock/DSP/UI/logo/SDK,
source hashes and rebuilt native/Standalone hashes, producing
build/meter-service-evidence.json. Full 28-test portable suites were not rerun
for this bounded lifecycle integration; their earlier 05ed773 results remain
above. The final Windows core rebuild required elevated access to protected
MSBuild .tlog metadata; its retry and all final checks succeeded.

### Processor shutdown message ownership (27 October 2026)

The added native regression fails against 8543d6b with `Background processor
destruction must retain message ownership through member and base parameter
cleanup` in build/shutdown-before.txt. The unchanged pre-fix Processor.cpp/.h
are retained as build/shutdown-before-Processor.{cpp,h}. A gated dependency
destructor lets the negative fixture finish parameter cleanup while the message
test is blocked, avoiding a race merely to report the missing ownership.

The new first private base acquires ownership only during host-worker shutdown
and retains it through native members, APVTS and AudioProcessor base parameters.
Existing message-thread or pre-held ownership is reused. Both actual dependency
and base-parameter destructors verify ownership and the host-selected destruction
thread. At 64/128 the fixture covers prepared/released/unprepared worker teardown,
nested ownership and direct message destruction; prepared cases render nonzero
PCM and wait for real processor service callbacks before stopping audio calls.
processBlock, core DSP, runtime meter servicing, original UI and state format are
unchanged. Only the existing native test and Standalone targets were rebuilt.

Final current gates:

- Windows native processor/SDK **2/2**, 6.01s; Ubuntu **2/2**, 14.39s.
- Selected realtime/clock/unchanged-JS meter gates: Windows **6/6**, 90.80s;
  Ubuntu **6/6**, 53.75s. No core source changed; full portable/sanitizer suites
  were not rerun and no native sanitizer result is claimed.
- Each native run retains **18 linked production AudioDeviceManager blocks**,
  real JUCE meter cleanup and twenty-track processor servicing, with no skips,
  assertion/leak failures or tracked callback new/delete.
- Committed PCM **126 cases / 965,826 samples**, maximum absolute error
  **7.394757495227111e-8** at unchanged **1e-6** tolerance; existing envelope/
  browser-resampling/latency-compensation/sync exclusions remain.
- Original UI byte identity/deterministic embedding/tamper rejection pass.
  The committed logo, SDK/generated wrappers and other-format artifacts remain
  unchanged. No package, dependency/version change or push.

Logs: build/shutdown-{native,core}-{windows,linux}.txt,
build/shutdown-audio.json and build/shutdown-assets.txt. The evidence writer
build/WriteShutdownEvidence.py verifies the old-code failure, current gates,
source/binary hashes and unchanged processBlock/core/UI/logo/SDK, producing
build/shutdown-evidence.json.

The running dispatcher must remain available during worker shutdown; the message
thread cannot join a worker awaiting message ownership. Audio must already have
stopped. After global dispatch shutdown the caller must already have quiesced
callbacks; no new lock is requested then. Concurrent audio, reentrant or arbitrary
post-quit destruction and DAW/Apple/hardware acceptance are not established.
Full meter playback/buffer/stop notification mapping and rendering remain pending.

### Queued meter creation and pending audio resume (27 October 2026)

Expanded the existing original_meter_source_lifecycle gate without changing the
preserved JS, product UI, C++ engine, processor, SDK or plugin formats. The gate
retains its 136 original source/graph/input-gate checks and adds 14 actual
handleAction scheduling/cancellation cases and 22 actual AudioEngine.resume/play
cases across tracks 0 and 19. Context resume and timer IO are controlled stubs;
the original methods execute unchanged. This is reference characterization, not
acceptance of a native notification producer or complete meter feature.

The original creates an analyser on the queued timeout up to 100ms before the
grid, then starts its source at max(grid,currentTime). Tests also retain near-grid
zero-delay scheduling, late-start phase, queued short-action/stop/DEL/STOP ALL
cancellation, rejected resume, stale play/transport/slot invalidation, successful
resume and same-graph repeated play. Native audible starts must not move early
to imitate the separate graph creation point. See REFERENCE_AUDIT.md for the
mapping requirements this identifies.

- Windows selected realtime/meter/clock gates: **6/6**, **86.65s**.
- Ubuntu selected realtime/meter/clock gates: **6/6**, **57.45s**.
- Three deliberately broken isolated source copies fail: removing the 100ms lead,
  accepting a stale play request, and accepting a stale transport generation.
  Only build-directory copies were mutated; no preserved source was edited.
- Committed PCM: **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8** at unchanged **1e-6** tolerance. Existing envelope,
  browser resampling, latency compensation and sync exclusions remain.
- Original UI byte identity, deterministic embedding and tamper rejection pass.
  The committed logo SHA256 remains
  b3f528936976cd892863965e86204914e2c2c39193c7bb432c04ee4c8f35ef0c.

Logs: build/meter-scheduling-{windows,linux,assets}.txt,
build/meter-scheduling-audio.json and build/meter-scheduling-mutations.json.
build/CheckMeterSchedulingMutations.py retains each broken copy and assertion log;
build/WriteMeterSchedulingEvidence.py records the checks in
build/meter-scheduling-evidence.json. No binary rebuild/native host/sanitizer/full
portable-suite run was needed or claimed for this JS test/documentation change.
Existing native and platform evidence retains its prior scope. No package,
dependency/version change or push. Full meter producer/overlap/device continuity
and renderer acceptance remain unfinished.

### Processor-owned cleanup adapters and release guard (27 October 2026)

The processor now routes its sole bounded raw-clear consumer through twenty
retained LoopMeterCleanupTimer adapters, rather than directly to lifecycle
owners. Storage is allocated during construction; actual JUCE timer objects are
emplaced on the message thread after completed prepare. Host construction and
prepare do not construct them. The dispatcher is destroyed before adapters,
adapters before owners, with the existing last-destroyed message ownership guard
covering host worker destruction. Construction/operations remain message-only;
destruction also permits host-held MessageManager ownership.

The newly bound completed-generation guard cancels an armed callback after
release or generation mismatch without mutating the surviving owner/history.
New released stops reject, cancelled deadlines stay cancelled after readiness
recovery, and a fresh explicit stop recovers with the original 25ms delay.
The unguarded adapter was compiled and run against this expanded native test;
it fails with `Released generation must prevent armed cleanup callback from
changing owner`. That source snapshot and failure remain in
build/meter-adapters-unguarded-LoopMeterCleanupTimer.h and
build/meter-adapters-unguarded.txt. The final guarded code passes.

- Windows native **2/2**, **5.79s**; Ubuntu native **2/2**, **15.37s**.
- Selected realtime/unchanged-JS meter/clock gates: Windows **6/6**, **87.69s**;
  Ubuntu **6/6**, **54.01s**. All retained pre-grid/resume reference cases pass.
- Each native run retains **18 linked production AudioDeviceManager blocks**,
  the existing twenty-track dispatcher, autonomous service and supported
  prepared/released/unprepared/nested/direct host shutdown cases at 64/128.
  Added checks cover host construction, zero pre-service adapters, twenty
  persistent service routes, no invented pending stops, armed host-owned adapter
  destruction, release/mismatched-generation cancellation and fresh recovery.
  No skips, JUCE assertion/leak failures or tracked callback new/delete.
- Exact paired nonzero stereo PCM and serialized processor state remain checked.
  The committed PCM audit passes **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8** at unchanged **1e-6** tolerance; existing envelope,
  browser resampling, latency compensation and sync exclusions remain.
- Original UI byte identity/deterministic embedding/tamper rejection pass. The
  committed logo, core sources, SDK/generated wrappers and other-format binaries
  remain unchanged. Existing native test and Standalone targets were rebuilt on
  Windows and Ubuntu. No new format, dependency/version, package or push.

Logs: build/meter-adapters-native-{windows,linux}.txt,
build/meter-adapters-core-{windows,linux}.txt, build/meter-adapters-audio.json,
build/meter-adapters-assets.txt and final build logs. The evidence writer
build/WriteMeterAdapterEvidence.py verifies gates, the unguarded failure,
unchanged callback/prepare/release/bridge/core/UI/logo/SDK and rebuilt binary
hashes in build/meter-adapters-evidence.json. No full portable-suite or native
sanitizer run is claimed for these plugin/message-service changes.

Production adapters are constructed and routed but remain unarmed: complete
play/buffer/stop notifications are still missing. Active release/generation
checks exercise the same adapter with a controlled completed-generation atomic;
they do not claim production stop notification acceptance. Overlap PCM ownership,
device continuity, visible meter rendering and new DAW/hardware/Apple validation
remain unfinished. Supported worker destruction still requires stopped audio,
running message dispatch and no message-thread join while a worker awaits it;
arbitrary concurrent/reentrant/post-quit destruction remains outside this scope.

### Accepted committed-buffer handoff (27 October 2026)

Production now subscribes to accepted immutable buffer receipts in the existing
ordered meter stream. Audio-owner worker acceptance and successful Undo/Redo
restore publish the current clip ID; prepare explicitly hands off retained
nonempty clips. Clear captures the removed ID before retiring its lease. The
message owner can apply a matching clear without any raw graph ticket, while
rejecting old buffer/clear serials and preserving the legacy boolean-notice
fence. Buffer receipt delivery does not create an analyser or reset its window,
follower history or stop deadline. The same sole consumer/32-raw-work budget and
generation-wide loss latch remain in use. No new IPC levels or APVTS state.

New MeterBufferTests exercise capture, OD/SUB, MULT, clone MERGE, NORM and
Undo/Redo; failed and cancelled worker results; rejected empty play; full-width
IDs; wrong/stale/duplicate clear; typed clear with ticket zero; retained prepare
and loss/recovery. Each actual 64/128 stereo callback is compared exactly with
an Engine retaining the raw-window subscription. Two thousand concurrent buffer
exchanges check coherent metadata above 2^53 and preservation of window tokens.
Native processor tests independently enforce the production subscription with
actual bridge capture, zero-ticket clear and retained prepare handoff.

Removing the actual worker-acceptance publication makes the focused test fail
with `Actual audio owner emits the expected ordered buffer/window fact`.
The negative source/log are retained in
build/buffer-receipts-missing-producer-LoopBank.cpp and
build/buffer-receipts-missing-producer.txt. Final publication is restored.

- Full portable suite: Windows **28/28**, **317.06s**; Ubuntu **28/28**, **530.50s**.
- Native processor/SDK generation: Windows **2/2**, **5.86s**; Ubuntu **2/2**,
  **15.26s**. Both retain **18 linked production AudioDeviceManager blocks**,
  all twenty cleanup routes and the previous timer/readiness/shutdown checks.
  No skips, JUCE assertion/leak failures or tracked callback new/delete.
- Ubuntu core sanitizer selection: **6/6**, **173.22s**, including the new
  committed-buffer cases in core_realtime_contract. No native sanitizer claim.
- Committed PCM: **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8** at unchanged **1e-6** tolerance. Existing envelope,
  browser resampling, latency compensation and sync exclusions remain.
- Original UI byte identity, deterministic embedding and tamper rejection pass.
  Logo, SDK/generated wrappers, dependencies/version and other-format binaries
  remain unchanged. Existing native tests and Standalone were rebuilt on both
  platforms. No package or push.

Logs: build/buffer-receipts-full-{windows,linux}.txt,
build/buffer-receipts-native-{windows,linux}.txt, build/buffer-receipts-asan.txt,
build/buffer-receipts-audio.json and build/buffer-receipts-assets.txt.
build/WriteBufferReceiptEvidence.py verifies these gates, the missing-producer
failure, unchanged DSP/processor callback/UI/SDK and rebuilt binary hashes,
writing build/buffer-receipts-evidence.json.

Focused reproduction after building aion_core_tests:
`build/windows-core/Release/aion_core_tests.exe --meter-buffers` (Windows) or
`build/core/aion_core_tests --meter-buffers` (Ubuntu).

Acceptance covers committed-buffer ownership and clear routing. Raw-window
component tests retain their narrower stream; paired Engine and actual processor
tests enforce production behavior. No complete JS edit graph-lifetime parity,
accepted play/stop origins, pre-grid graph creation, overlapping graph PCM,
device continuity or visible renderer acceptance is claimed. These remain
unfinished before a meter feature package, alongside platform/DAW/hardware work.

### Meter adapter completed-generation entry guard (2026-10-27)

While tracing accepted play/stop delivery, a readiness regression was reproduced:
the adapter rejected a zero prepared generation, but a different nonzero
generation could still mutate the old owner's stop metadata and arm cleanup.
The added real-message-loop check fails against the old implementation with
`A mismatched completed generation rejects new stops without owner or deadline mutation`.
The failing CTest log and original adapter are retained in
build/meter-readiness-negative.txt and build/meter-readiness-before.h.

Bound adapters now require the completed generation to match their owner's
current Engine generation before stop, buffer/clear delivery or route validation.
The actual capture/clear fixture at 64/128 verifies both zero and mismatched
readiness, twenty bound dispatcher routes leaving the queued clear untouched,
rejected delivery retaining buffer/analyser/data/history and an already armed
deadline, and successful application of the retained receipt after readiness
matches again. Existing timer tests also exercise rejected mismatched stops
after callback cancellation, alongside release/reprepare and the earlier
shutdown, worker ownership and production processor regressions.

- Rebuilt native tests and existing Standalone: Windows **2/2**, **6.02s**;
  Ubuntu **2/2**, **15.17s**. Both retain the **18 linked production
  AudioDeviceManager blocks**, actual processor buffer/service/shutdown coverage,
  no skips/JUCE assertion/leak failures and zero tracked callback new/delete.
- Rebuilt portable meter selection: Windows **6/6**, **88.95s**; Ubuntu **6/6**,
  **52.97s**, including the unchanged original-JS lifecycle and meter comparisons.
- Committed PCM: **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8** at unchanged **1e-6** tolerance. Existing envelope,
  browser-resampling, latency-compensation and sync exclusions remain.
- Original UI byte identity, deterministic embedding and tamper rejection pass.
  The committed logo and other-format/SDK/generated-clock artifacts are unchanged.
  Full portable 28-test and sanitizer suites were not repeated for this repair;
  the previous results above retain their original scope.

Logs: build/meter-readiness-native-{windows,linux}.txt,
build/meter-readiness-core-{windows,linux}.txt, build/meter-readiness-audio.json
and build/meter-readiness-assets.txt. build/WriteMeterReadinessEvidence.py checks
the gates, old-code failure, unchanged DSP/UI/SDK/other-format artifacts and
rebuilt native binary hashes, producing build/meter-readiness-evidence.json.

DSP, processor callback, immutable buffer producers, serialized state and UI
are unchanged. The readiness precheck does not establish atomic concurrent
prepare/message dispatch, accepted graph creation/stop origins, pre-grid lead,
overlapping graph PCM, continuity or visible rendering. Meter acceptance remains
unfinished; no package, dependency/version update or push.

### Accepted immediate playback graph handoff (2026-10-27)

Production fixes its Engine subscription to acceptedPlayback, preserving the
raw-window and committed-buffer-only component contracts. Successful unsynced
PLAY and stopped RTRG publish an explicit immediatePlay fact with the actual
sample frame, immutable clip ID, generation and fresh raw-window ticket. A raw
started fact, buffer installation, capture auto-play, healthy RTRG, queued PLAY
or audible grid transition does not use this origin. Ticket exhaustion invalidates
telemetry instead of recycling ownership or rejecting audible playback.

The sole dispatcher applies these facts through prepared JUCE cleanup adapters.
The owner creates fresh analyser/data identities, retains meter history, cancels
obsolete cleanup and binds the audio-created window without enqueueing a second
reset. Increasing receipt serials and tickets reject duplicates/reordering;
clip/generation/stream health reject stale or foreign ownership. Ordered delayed
births can update lifetime metadata, while bound PCM validation independently
requires the current window ticket. Clear and buffer serial guards include the
last accepted immediate play, retaining the earlier typed-clear fence.

MeterBufferTests' focused --meter-playback mode exercises actual paired Engine
callbacks at 64/128 with bit-identical stereo output: accepted unsynced PLAY,
nonzero bound PCM, healthy RTRG identity retention, rejected repeated PLAY,
fresh replacement/analyser data with held history and old cleanup rejection,
wrong clip/duplicate/old buffer/stale generation rejection, delayed ordered
births, stopped synced RTRG bypassing the grid, queued/grid non-inference,
typed clear, retained prepare and full-width ticket exhaustion. Callback new/delete
tracking remains enabled. Removing the actual PLAY branch's producer makes the
focused case fail with `Actual audio owner emits the expected ordered buffer/window fact`;
the tested mutation and failure are retained in
build/immediate-play-missing-producer.cpp and build/immediate-play-negative.txt.
The producer is restored in the final implementation.

Native tests independently route actual accepted receipts through prepared
adapters, reject released/mismatched delivery, seed nonzero meter history and
cancel an armed real JUCE timer on accepted replacement. Actual AionProcessor
RPC -> audio acceptance -> existing message-service dispatcher -> owner attachment
is checked at both block sizes, including healthy RTRG retention, stopped RTRG
replacement and typed clear detachment. These use the fixed production
subscription, with no fabricated playback receipts or direct owner setup seam.
Native graph counters are diagnostics only and add no IPC levels or serialized
parameter state. Existing buffer-without-graph, twenty-route service/readiness,
shutdown and linked AudioDeviceManager regressions remain in the native gate.

Research retained the unchanged Loop.play/retrigger methods as the behavior
authority; tests execute the original JS lifecycle and scheduling methods.
The separate RT/non-RT queues in
[SooperLooper's engine](https://github.com/essej/sooperlooper/blob/master/src/engine.hpp)
provide an ownership-transfer comparison, not a substitute graph timing policy.
[JUCE Timer documentation](https://docs.juce.com/master/classjuce_1_1Timer.html)
also rules out treating message timer callbacks as sample-accurate source starts.
No third-party source was copied.

Focused reproduction after building the core tests:
`build/windows-core/Release/aion_core_tests.exe --meter-playback` or
`build/core/aion_core_tests --meter-playback` on Ubuntu.

- Full portable suites: Windows **28/28**, **320.98s**; Ubuntu **28/28**,
  **547.75s**, including the new accepted immediate playback cases.
- Rebuilt native tests and existing Standalone: Windows **2/2**, **6.69s**;
  Ubuntu **2/2**, **15.05s**. Both retain **18 linked production
  AudioDeviceManager blocks**, actual buffer/cleanup/readiness/shutdown coverage,
  no skips/JUCE assertion/leak failures and zero tracked callback new/delete.
- Ubuntu core sanitizer meter selection: **6/6**, **175.64s**. No native
  sanitizer acceptance is claimed.
- The additional first-window zero-prefix assertion passes in focused mode on
  Windows, Ubuntu and Ubuntu core sanitizers with the final rebuilt test binaries.
  It excludes old graph PCM while requiring nonzero fresh samples at 64/128.
- Committed PCM: **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8** at unchanged **1e-6** tolerance. Existing envelope,
  browser-resampling, latency-compensation and sync exclusions remain.
- Original UI byte identity, deterministic embedding and tamper rejection pass.
  Logo, SDK/generated clock wrappers, dependencies/version and other-format
  binaries remain unchanged. No new REAPER/Ardour, physical hardware or Apple
  build acceptance is claimed by these gates.

Logs: build/immediate-play-full-{windows,linux}.txt,
build/immediate-play-native-{windows,linux}.txt, build/immediate-play-asan.txt,
build/immediate-play-prefix-{windows,linux,asan}.txt,
build/immediate-play-audio.json and build/immediate-play-assets.txt.
build/WriteImmediatePlayEvidence.py verifies these gates, the missing producer
failure, unchanged DSP/processor callback/UI/SDK/other-format artifacts and
rebuilt binary hashes, writing build/immediate-play-evidence.json.

Acceptance is limited to the immediate playback handoff. Stop notification
origins, capture/edit auto-play graph lifecycle, queued 100ms lead, overlapping
graph PCM, envelope parity, device continuity and visible rendering remain
unfinished. No visible meter, package, dependency/version change or push.

### Accepted immediate-stop handoff (2026-10-27)

The acceptedPlayback stream now carries distinct immediateStop facts from
explicit stopImmediate and STOP's immediate branch for known accepted playback
origins. The facts retain the raw window token and name the actual clip, ticket,
frame and generation. Clear, prepare and raw component resets cancel the origin.
The private generic stop routine, scheduled STOP and STOP ALL never invent this
25ms origin. Native worker/finalizing or stopped state cannot decide analyser
attachment: the message owner applies the stop to its explicitly owned graph,
replaces a pending cleanup for a repeated tap, or accepts a detached ordered no-op.

The owner validates stream health, serial, generation, ticket and clip before
committing graph metadata. Duplicates/stale/wrong-owner receipts leave the
cleanup deadline and history unchanged. The sole dispatcher routes through the
prepared timer adapter, which captures wall time at receipt application and arms
the existing 25ms cleanup. A new accepted PLAY cancels old cleanup without
resetting meter history. Actual timer delivery detaches the analyser while
retaining data/history; a subsequent clear preserves the unchanged JS detached
history quirk. Stop receipt counters are native diagnostics, not IPC levels.

MeterBufferTests' --meter-stops mode drives actual paired Engine callbacks at
64/128 on tracks 0/19. It covers immediate/unsynced actions, actual frame/clip/
window identity, nonzero seeded history, wrong ticket/clip/duplicate rejection,
audio-clock advancement without teardown, repeated stops after audio has stopped,
explicit cleanup and detached no-op, typed clear history, same-callback stop/play
cancellation, NORM->STOP while the worker finalizes followed by the accepted
buffer, scheduled-stop override, scheduled/panic non-inference, raw reset and
lost/prepare rejection. Every callback compares exact stereo PCM with a control
Engine and tracks new/delete. Existing accepted-play tests retain their earlier
assertions while explicitly consuming the new ordered stop facts.

Native tests drive actual stop receipts through JUCE timers with the audio clock
frozen, checking the 25ms minimum, repeat/replacement cancellation, detached clear
history, attached and detached readiness rejection, retained-receipt recovery and
nonzero PCM/history. The test readiness atomic now outlives its bound fixture
adapters. Actual production RPC -> callback -> message dispatcher -> pending
cleanup -> accepted PLAY cancellation is checked at 64/128, retaining previous
buffer, twenty-route service, shutdown and linked device-manager coverage.

Removing the actual stopImmediate action's publication makes the focused gate
fail with `Actual audio owner emits the expected ordered buffer/window fact`.
The mutation and failure are retained in build/immediate-stop-missing-producer.cpp
and build/immediate-stop-negative.txt. The final producer is restored.

- Full portable suites: Windows **28/28**, **322.87s**; Ubuntu **28/28**,
  **568.36s**, including prior buffer/playback cases and the new stop handoff.
- Rebuilt native tests and existing Standalone: Windows **2/2**, **6.50s**;
  Ubuntu **2/2**, **16.83s**. Both retain **18 linked production
  AudioDeviceManager blocks**, actual processor/service/readiness/shutdown checks,
  no skips/JUCE assertion/leak failures and zero tracked callback new/delete.
- Ubuntu core sanitizer meter selection: **6/6**, **186.75s**, including the new
  accepted stop/worker-ordering cases. No native sanitizer claim.
- Committed PCM: **126 cases / 965,826 samples**, maximum error
  **7.394757495227111e-8** at unchanged **1e-6** tolerance. Existing envelope,
  browser-resampling, latency-compensation and sync exclusions remain.
- Original UI byte identity, deterministic embedding and tamper rejection pass.
  Logo, SDK/generated clock wrappers, dependencies/version and other-format
  binaries remain unchanged.

Logs: build/immediate-stop-full-{windows,linux}.txt,
build/immediate-stop-native-{windows,linux}.txt, build/immediate-stop-asan.txt,
build/immediate-stop-audio.json and build/immediate-stop-assets.txt.
build/WriteImmediateStopEvidence.py checks these gates, the missing-producer
failure, unchanged DSP/processor callback/UI/SDK/other-format artifacts and rebuilt
binary hashes, writing build/immediate-stop-evidence.json.
Focused reproduction: `build/windows-core/Release/aion_core_tests.exe --meter-stops`
or `build/core/aion_core_tests --meter-stops` on Ubuntu.

The unchanged reference Loop.stop/_teardown methods remain the behavior authority.
[JUCE Timer documentation](https://docs.juce.com/master/classjuce_1_1Timer.html)
confirms callbacks run on the message thread and can be delayed by its workload;
there is no upper callback-latency assumption in the tests. This gate measures
25ms from accepted receipt application on that thread. Absolute deadline alignment
across command/receipt delivery latency, scheduled STOP/STOP ALL, queued/capture/
edit graph origins, overlap/envelope/continuity and rendering remain unaccepted.
No full original end-to-end stop timing, physical IO, DAW or Apple acceptance is
claimed. Keep visible meters disabled; no package/version/dependency change or push.

### Accepted scheduled STOP receipts (2026-10-27)

Continues the unfinished meter feature from bffc355. The unchanged original
`Loop.stop(scheduledTime)` keeps its graph until the requested future audio time
plus 50ms, whereas the immediate volume-node branch uses 25ms. The native synced
playing STOP branch now publishes its own scheduledStop fact, restricted to a
known accepted immediate-play graph origin. It carries the unrounded grid delta
in seconds and retains the immutable clip, raw window ticket and ordered serial.
It does not infer graph creation from native state or the audible stop boundary.

The message owner rejects wrong kind/generation/ticket/clip/serial, stream loss
and invalid deltas before mutation. Its existing prepared cleanup adapter uses
delta plus 50ms; duplicate receipts cannot postpone cleanup. A second STOP takes
the existing immediate branch and replaces that request with 25ms. Actual PLAY
or attached clear cancels obsolete cleanup; teardown preserves detached data and
history. The sole 32-work dispatcher and native-only stop counters serve both
receipt types. No new bridge fields, UI controls or serialized state were added.

Coverage adds actual paired Engines on tracks 0/19, 64/128 buffers and
120/137.3/999 BPM, requiring exact callback stereo PCM against the unobserved
control and zero tracked callback new/delete. Tests seed nonzero bound meter PCM,
check the exact pre-rounding grid delta and identity, reject malformed/stale
facts, advance through actual audible scheduled completion without message
teardown, override with a second STOP, replace playback and clear attached graphs.
All previous immediate-stop/buffer/play cases remain. The native test drives the
production processor through its existing RPC and prepared dispatcher, then
actual JUCE timers with frozen audio clocks, readiness and duplicate rejection.

The missing-producer negative removes only the scheduled publication hook. The
focused test must fail with `Actual audio owner emits the expected ordered
buffer/window fact`; retained source/logs are
build/scheduled-stop-missing-producer.cpp and build/scheduled-stop-negative.txt.
The final source is restored before positive gates and binary builds.

Evidence files use build/scheduled-stop-full-{windows,linux}.txt,
build/scheduled-stop-native-{windows,linux}.txt, build/scheduled-stop-asan.txt,
build/scheduled-stop-audio.json and build/scheduled-stop-assets.txt.
build/WriteScheduledStopEvidence.py verifies test results, the negative,
unchanged processor/DSP/UI/SDK/other-format artifacts and rebuilt binary hashes,
writing build/scheduled-stop-evidence.json. Focused reproduction remains
`build/windows-core/Release/aion_core_tests.exe --meter-stops` or
`build/core/aion_core_tests --meter-stops` on Ubuntu.

Final restored-source results:

| Gate | Result |
| --- | --- |
| Windows portable suite | 28/28, 325.62s |
| Ubuntu portable suite | 28/28, 589.93s |
| Windows native processor / standalone SDK generation | 2/2, 6.94s |
| Ubuntu native processor / standalone SDK generation | 2/2, 17.09s |
| Ubuntu core AddressSanitizer selection | 6/6, 191.01s |
| Committed PCM audit | 126 cases, 965826 samples, max absolute error 7.394757495227111e-8 at unchanged 1e-6 tolerance |
| Original resources / embedding / tamper rejection | Pass |

Native runs retain all eighteen linked production AudioDeviceManager blocks;
there are no skips, JUCE assertions, leak reports or tracked callback new/delete.
The existing Standalone binaries were rebuilt on both platforms. Other plugin
formats, SDK and original resources (including the committed logo) are unchanged.

The timer deadline still begins at receipt application on the message thread.
This gate does not establish wall command/delivery alignment or full original
stop latency. Backdating without ordered queued clear/play handling is unsafe.
STOP ALL's distinct cleanup, queued 100ms graph lead, capture/edit graph origins,
overlap/envelopes/continuity and the visible renderer remain unfinished. JUCE's
[Timer contract](https://docs.juce.com/master/classjuce_1_1Timer.html) permits late
message callbacks; there is no asserted upper callback-latency bound. No new DAW,
physical IO, Apple or native-sanitizer acceptance, package, version/dependency
change or push is claimed.

### Accepted STOP ALL receipts for established graphs (2026-10-27)

Continues the unfinished meter lifecycle from 0ccaf9f. The unchanged
`LoopManager.stopAll` calls `loop.stop(now + 0.015)` for active/stopping/restarting
graphs, giving a nominal 65ms scheduled cleanup. Its queued branch cancels only
the queued action and retains any old stop timer; detached stopped loops do not
start another cleanup. Fourteen added reference cases execute these original
methods at tracks 0/19, preserving all previous 136 identity/state, 14 dispatcher
scheduling and 22 resume cases. These timer/node IO stubs do not establish browser
PCM parity or native queued graph creation.

Implementation review also checked [SooperLooper's engine ownership queues](https://github.com/essej/sooperlooper/blob/master/src/engine.hpp),
which keep distinct RT/main loop-management queues and instance collections. That
informed retaining this repository's explicit bounded ownership transfer; the
unchanged Ascii-Looper methods remain the timing authority, and no external code
was copied. [JUCE Timer documentation](https://docs.juce.com/master/classjuce_1_1Timer.html)
requires message-thread delivery and permits late callbacks, so tests assert a
lower deadline bound without treating timer callbacks as audio sample events.

The native global command now publishes stopAll facts before each generic audio
stop, but only for a known accepted immediate-play graph origin. It retains clip,
ticket, serial, frame and raw-window ownership. The owner uses 15ms plus 50ms,
retains analyser/data/history, replaces pending stopping cleanup despite native
audio already being stopped, and accepts detached no-ops. Wrong type/delta/clip/
ticket/generation, stale/duplicate facts and unhealthy streams reject before
mutation. Existing prepared adapters and the sole bounded dispatcher apply facts;
PLAY/clear cancel obsolete panic cleanup. Audible PCM and stop behavior are not
changed, and no new IPC, serialized state or visible controls are added.

An accepted queued PLAY explicitly makes panic graph ownership unknown until a
fresh accepted immediate PLAY/stopped RTRG. Neither the native queued snapshot
nor its audible grid transition restores that knowledge. The consumer also
refuses queued owners. This prevents the narrower handoff from falsely replacing
the original queued-cancel branch's retained timer. Full queued STOP ALL, its
100ms graph-creation lead, command/receipt wall alignment and other graph origins
still require implementation. This gate must not be presented as full panic
lifecycle or original end-to-end stop latency acceptance.

Actual paired Engine tests at 64/128 on tracks 0/19 seed nonzero meter PCM and
cover 65ms metadata, repeated/detached stops, same-callback replacement, attached
clear, prepare, raw-budget loss and origin quarantine/recovery. Actual OD/SUB/MULT
capture and same-callback NORM followed by panic retain cleanup through subsequent
worker-buffer acceptance. Live callbacks stay nonblocking; the separate existing
host-declared offline callback accepts finalization as previously authorized.
All observed callbacks require bit-identical stereo PCM against an unobserved
Engine and zero tracked callback new/delete.

Native tests cover actual twenty-track committed-buffer/play/panic batches,
7/13 partial drains, prepared/released/mismatched routing, retained pending
requests on rejection, repeated cleanup replacement and Clear All cancellation.
The production processor is exercised through its existing RPC and sole prepared
dispatcher. Actual JUCE timers require a 65ms lower wall-time bound with frozen
audio, repeated rearming, detached no-ops and accepted PLAY cancellation; no upper
callback-latency bound is assumed.

Two negative controls are retained: build/panic-missing-producer.cpp and
build/panic-negative.txt omit only panic publication and fail on a missing actual
receipt; build/panic-unguarded-queue.cpp and build/panic-queue-negative.txt remove
only queued-origin quarantine and fail because queued panic emits an inferred
active-graph cleanup. Final source is restored before positive builds.

Evidence files use build/panic-full-{windows,linux}.txt,
build/panic-native-{windows,linux}.txt, build/panic-asan.txt,
build/panic-audio.json and build/panic-assets.txt.
build/WritePanicEvidence.py verifies gates, both negative controls, unchanged DSP/
processor/UI/SDK/other-format artifacts and rebuilt binary hashes, writing
build/panic-evidence.json. Focused reproduction uses
`build/windows-core/Release/aion_core_tests.exe --meter-panic` or
`build/core/aion_core_tests --meter-panic` on Ubuntu.

Final restored-source results:

| Gate | Result |
| --- | --- |
| Windows portable suite | 28/28, 328.02s |
| Ubuntu portable suite | 28/28, 589.68s |
| Windows native processor / standalone SDK generation | 2/2, 7.91s |
| Ubuntu native processor / standalone SDK generation | 2/2, 19.89s |
| Ubuntu core AddressSanitizer selection | 6/6, 194.34s |
| Committed PCM audit | 126 cases, 965826 samples, max absolute error 7.394757495227111e-8 at unchanged 1e-6 tolerance |
| Original resources / embedding / tamper rejection | Pass |

Native runs retain all eighteen linked production AudioDeviceManager blocks;
there are no skips, JUCE assertions, leak reports or tracked callback new/delete.
The existing Standalone binaries were rebuilt on both platforms. Other plugin
formats, SDK and original resources (including the committed logo) are unchanged.

The broader meter feature remains unfinished: receipt-relative deadlines still
need command/delivery alignment, queued/capture/edit graph origins need full
mapping, and envelopes/overlap/continuity/visible rendering need acceptance.
No new DAW, physical IO, Apple or native-sanitizer acceptance, package, version/
dependency change or push is claimed.

### Accepted queued intent/cancel ownership (2026-10-27)

Continues the unfinished meter lifecycle from a171579. Actual synced PLAY now
publishes queuedPlay with an independent full-width queue ID, current committed
clip and existing raw-window ticket. It sets queued metadata without resetting
PCM, creating an analyser or arming a wall timer. Queue IDs are not graph tickets;
their high-water survives clear and resets only with a new prepared generation.

STOP ALL while the accepted audio intent remains queued publishes queuedCancel,
which changes queued metadata to stopped while retaining the old cleanup,
analyser/data and meter history. Explicit STOP/stopImmediate publishes queuedStop:
an attached volume graph receives a new 25ms request, while a graphless loop tears
down without a timer. Matching generation, serial, committed clip, raw identity
and exact queue ID are required before mutation. A zero raw ticket is allowed
only without an attached/pending binding; it must not equal a retired graph's
high-water. This fixes a reproduced queue-after-clear rejection without resetting
graph ticket history. Typed clear, replacement and prepare invalidate old intents.

The sole bounded dispatcher and twenty prepared adapters route these facts.
Native-only queued/queueApplied diagnostics do not add IPC meter levels, UI
controls or serialized state. Queued notices synchronise existing cleanup without
rearming it. Direct released/mismatched adapter calls and invalid route banks
reject before mutation/dequeue and can recover the exact retained receipt.

Original-method characterization retains all previous 136 graph/state cases,
14 dispatcher scheduling cases, 22 resume cases and 14 panic cases. Four new
queued STOP cases execute unchanged Loop.stop with and without an attached graph:
the former replaces an old timer with 25ms, while the latter has no timer.
The original 100ms scheduling callbacks remain characterized, not implemented by
these native intent facts. [JUCE Timer's message-thread contract](https://docs.juce.com/master/classjuce_1_1Timer.html)
permits late callbacks; native tests do not assume an upper callback-latency bound.

Actual paired Engine tests at 64/128 on tracks 0/19 cover first graphless queue,
full-width IDs, stale/wrong-identity delivery, retained versus replaced cleanup,
zero-ticket recovery after clear, old teardown, prepare and bounded loss drains.
Every observed callback requires exact stereo PCM against the unobserved control
and zero tracked new/delete. Prior immediate/scheduled STOP, panic, play and
buffer acceptance cases remain. Native tests exercise production RPC/dispatcher
queue counters, twenty-route 7/13 partial batches, prepared readiness/recovery,
queued panic retaining all old stop requests and Clear All cancellation. Actual
queued PLAY/cancel keeps a real old JUCE stop timer until frozen-clock teardown.

Negative evidence: build/queued-intent-missing-producer.cpp and
build/queued-intent-negative.txt omit only accepted queue publication and fail on
a missing actual receipt. build/queued-intent-before-zero-ticket.h and
build/queued-intent-zero-ticket-negative.txt retain the reproduced pre-fix
queue-after-clear rejection. Final source is restored before positive gates.

Evidence files use build/queued-intent-full-{windows,linux}.txt,
build/queued-intent-native-{windows,linux}.txt, build/queued-intent-asan.txt,
build/queued-intent-audio.json and build/queued-intent-assets.txt.
build/WriteQueuedIntentEvidence.py checks gates, negatives, unchanged DSP/callback/
UI/SDK/other-format artifacts and rebuilt binary hashes, writing
build/queued-intent-evidence.json. Focused reproduction uses
`build/windows-core/Release/aion_core_tests.exe --meter-queued` or
`build/core/aion_core_tests --meter-queued` on Ubuntu.

Final restored-source results:

| Gate | Result |
| --- | --- |
| Windows portable suite | 28/28, 329.63s |
| Ubuntu portable suite | 28/28, 589.15s |
| Windows native processor / standalone SDK generation | 2/2, 8.07s |
| Ubuntu native processor / standalone SDK generation | 2/2, 20.14s |
| Ubuntu core AddressSanitizer selection | 6/6, 196.75s |
| Committed PCM audit | 126 cases, 965826 samples, max absolute error 7.394757495227111e-8 at unchanged 1e-6 tolerance |
| Original resources / embedding / tamper rejection | Pass |

Native runs retain all eighteen linked production AudioDeviceManager blocks;
there are no skips, JUCE assertions, leak reports or tracked callback new/delete.
Existing Standalone binaries were rebuilt on both platforms. Other plugin
formats, SDK and original resources (including the committed logo) are unchanged.

This is the pre-graph intent/cancel stage. It does not implement the original
100ms lead timer, graph birth at that callback, late creation/source phase or
command/receipt wall deadline alignment. Audible queued-to-playing transitions
still do not prove analyser birth. An old teardown can change state out of queued
without cancelling the separate original queue timer: the intent ID is retained,
and later queuedCancel fails closed after that state change pending the remaining
mapping. Full queued panic/STOP lifecycle, capture/edit origins, overlaps,
envelopes/continuity and visible rendering remain unfinished. No new DAW, hardware,
Apple or native-sanitizer acceptance, package, version/dependency change or push.

## Conditional queued raw-window reset (2026-10-27)

This continues the unfinished meter migration with the audio-owner half of a
future queued callback. It does not connect a production queue timer or create a
message analyser. `meterQueuedWindowStart` carries separate integer graph ticket,
full-width queue ID and immutable clip ID through the existing bounded FIFO.
Only frame zero is accepted; enqueue success is distinct from actual acceptance.
The audio owner validates scope, generation, stream health, queue, clip, source
state and ticket before resetting raw history. Each queued intent can accept only
one reset. The raw `started` receipt includes the actual sample boundary, queue
and clip; it remains outside the immediate-play graph-birth route.

`aion_core_tests --meter-queued` retains all previous intent/cancel checks and adds
actual paired Engines at tracks 0/19 and 64/128 samples. It checks:

- Zero identities/ticket, foreign generation and nonzero guessed frame rejection.
- Queue and clip IDs differing only above bit 31, and foreign-track rejection.
- Exact accepted receipt identities/frame and fresh pre-grid zero raw window,
  with no early audible playback or performance acknowledgement changes.
- Duplicate callbacks with higher tickets before/after audible grid, retained
  live post-grid PCM, and bounded 255-entry duplicate saturation/drain.
- STOP, STOP NOW, STOP ALL and clear queued ahead of the stale callback in the
  actual command FIFO, before and after audible grid. The callback cannot reset
  a cancelled queue or consume a replacement intent's first ticket.
- Clear/re-record replacement clips, prepare discarding pending resets and
  component subscriptions refusing queued playback ownership.
- Exact paired stereo PCM on every fixture callback and zero tracked callback
  allocation/deallocation.

The initial fixture used a 1000-callback polling bound for a four-beat fractional
tempo grid. At 64 samples the grid exceeded that bound; it was extended to 4000.
No audio tolerance, reference method or product timing was changed.

The isolated negative replaces only the new conditional audio dispatch with the
old unconditional raw reset. It fails `Rejected conditional reset does not create
a window or alter performance acknowledgements`. Restored source and both
mutation snapshots are retained under build/queued-window-*. Focused tests pass
on Windows and Ubuntu. Final full-suite evidence is recorded below.

The first full Ubuntu run reused the deliberately broken negative-test object:
restoring the saved source also restored its older modification timestamp, so
incremental compilation skipped it. That run is preserved as
build/queued-window-stale-object-linux.txt. The object was explicitly removed;
the guarded source was rebuilt and the focused gate and full suite rerun. The
final acceptance report excludes the stale-object run.

Final restored-source results (build/queued-window-evidence.json):

| Gate | Result |
| --- | --- |
| Windows portable suite | 28/28, 334.61s |
| Ubuntu portable suite, rebuilt guarded object | 28/28, 623.71s |
| Windows native processor / standalone SDK generation | 2/2, 8.14s |
| Ubuntu native processor / standalone SDK generation | 2/2, 20.17s |
| Ubuntu core AddressSanitizer selection | 6/6, 237.11s |
| Committed PCM audit | 126 cases, 965826 samples, max error 7.394757495227111e-8 at unchanged 1e-6 tolerance |
| Original resources / embedding / tamper rejection | Pass |

Native gates retain all eighteen linked production AudioDeviceManager blocks.
Final logs contain no skips, JUCE assertions, leak reports or sanitizer errors.
Existing Standalone binaries were rebuilt on both platforms; SDK, other plugin
format binaries, original resources and committed logo remain unchanged. The
report records restored-source and rebuilt-binary hashes and the isolated negative.

Research consulted the [JUCE Timer documentation](https://docs.juce.com/master/classjuce_1_1Timer.html)
and [SooperLooper's Engine ownership queues](https://github.com/essej/sooperlooper/blob/master/src/engine.hpp).
The latter provides a reference for ownership transfer; the unchanged Ascii-Looper
remains the authority for graph lifetime and queue timing. No source was copied.

The 100ms message callback, graph transaction and acceptance recovery, independent
old cleanup, post-callback queue policy, late source phase, other graph origins,
overlaps/envelopes/continuity and visible meter rendering remain unfinished.
Existing application behavior and original UI are preserved. No package, version
or dependency update, push, new DAW/hardware/Apple or native sanitizer acceptance.

## Unrounded queued callback delay (2026-10-27)

The unfinished meter queue path now carries the original max(0, grid delta minus
100ms) delay in accepted queuedPlay receipts. Calculation uses the unrounded grid
time. Audible playback still uses the identical sample-rounded deadline; neither
the receipt nor its delay creates an analyser, starts audio early or arms a timer.
The message lifecycle rejects NaN/infinite/negative delays before mutation,
retains the exact accepted value and hides it after cancellation. Cancellation
and queued STOP receipts must carry zero delay. The existing cleanup remains
independent, including teardown retaining a still-pending queue intent.

The existing original_meter_source_lifecycle CTest now passes the native
aion_meter_graph_probe into MeterSourceReference.cjs. All prior 136 lifecycle,
14 scheduling, 22 resume, 14 panic and 4 queued STOP cases remain. A new
--queued-timing mode runs actual Engine capture/commit/stop/queue at tracks 0/19,
64/128 blocks, fixed/dynamic sync and 120/137.3/999 BPM. It transfers the actual
frame, origin, grid period, accepted queue delay and unchanged source deadline.
The probe also delivers the actual committed buffer and queue receipts to the
message owner, requiring an exact retained delay without an analyser/cleanup.

The Node oracle executes the unchanged getNextGridTime method from scales.js and
the unchanged handleAction dispatcher. Its controlled context/timer IO captures
the original timeout for each native case. It compares 96 cases at the existing
1e-12 millisecond near() tolerance: 48 zero-delay cases and 48 positive cases,
including 99.9/100.1ms grid leads. The source deadline remains the grid ceiling
within one sample; that sample test is separate from callback-delay precision.
This extends the test to the native producer; it does not establish real wall
delivery, browser PCM, source late-start phase or visible meter acceptance.

An isolated negative reconstructs the delay from t.startAt, the rounded sample
deadline. The unchanged original oracle rejects it: 0.11805555555541503ms becomes
0.125000000000014ms. Final and mutated source snapshots plus failure are retained
as build/queued-delay-final.cpp, queued-delay-rounded.cpp and
queued-delay-negative.txt. The restored source receives a fresh timestamp and
its Ubuntu object is explicitly removed before the final build and oracle run;
the final build log confirms recompilation. No reference or tolerance was changed.

The [JUCE Timer documentation](https://docs.juce.com/master/classjuce_1_1Timer.html)
confirms that delivery occurs on the message thread and can run late. This stage
therefore treats the receipt's audio-time delay as metadata, not proof of a wall
deadline or a callback. The actual 100ms callback/graph transaction, acceptance
recovery, post-callback cancellation, old-cleanup overlap, late source alignment,
other graph origins, envelopes/continuity and renderer remain unfinished.
Original UI, logo, audio DSP, processor/bridge state, SDK and other plugin-format
binaries remain unchanged. No package, dependency/version update, push or new
DAW/hardware/Apple/native-sanitizer acceptance.

Final restored-source results (build/queued-delay-evidence.json):

| Gate | Result |
| --- | --- |
| Windows portable suite | 28/28, 341.90s |
| Ubuntu portable suite | 28/28, 598.26s |
| Windows native processor / standalone SDK generation | 2/2, 8.17s |
| Ubuntu native processor / standalone SDK generation | 2/2, 20.15s |
| Ubuntu core AddressSanitizer selection | 6/6, 278.44s |
| Original grid/dispatcher vs actual Engine timing | 96 cases per portable/sanitizer run; 48 zero, 48 positive delays |
| Committed PCM audit | 126 cases, 965826 samples, max error 7.394757495227111e-8 at unchanged 1e-6 tolerance |
| Original resources / embedding / tamper rejection | Pass |

Native tests retain all eighteen linked production AudioDeviceManager blocks.
Final logs contain no skips, JUCE assertions, leak reports or sanitizer errors.
Both existing Standalone binaries were rebuilt. The evidence report records
source/binary hashes, the isolated rounding failure and unchanged SDK/other-format
artifacts and logo. This completes the delay-metadata handoff only; queued message
callback/graph integration and visible-meter acceptance remain unfinished.

## Pending queued STOP ALL cancellation after grid (2026-10-27)

Scope: repair an existing production cancellation handoff, without activating a
new timer, graph or control. Native audio can cross its grid while the independent
message owner still has an accepted queued intent. STOP ALL previously selected
queuedCancel only from the audio `queued` state, so it emitted no cancellation
after that transition. Select the explicit pending queue ID instead and retire it
once. The same generic audio stop still runs. Panic origin remains quarantined;
no audible state or raw receipt is reinterpreted as message graph creation.

The added core regression failed on the unchanged baseline with `Late STOP ALL
must publish cancellation for the still-pending queued intent after audible grid`.
Retained failure/build logs: build/post-grid-panic-before.txt and
post-grid-panic-before-build.txt. The original JS methods independently confirm
late panic cancellation in four added cases at tracks 0/19 with/without an attached
graph. The attached case is a controlled queued-state fixture, as in the existing
panic characterization; it is not proof of every workflow that creates that state.
All earlier 136/14/22/14/4 cases and the 96 actual native grid/delay cases remain.

The repaired core tests use actual paired Engine PCM at 64/128, exact pending IDs,
stale/duplicate/repeated panic rejection, retained analyser/data/history/cleanup
and ordered queue+cancel delivery when message service is delayed until after
panic. Native tests additionally exercise the production processor/bridge/sole
dispatcher, twenty pending post-grid cancellations with bounded 7/13 delivery and
partial owner isolation, and actual JUCE 25ms cleanup with frozen audio after late
cancellation. The original pre-grid real-timer test remains and the late test is
an additional stage. Existing readiness, loss, clear and prepare gates are retained.

The implementation diff is one receipt-selection predicate plus its explanatory
comment. Engine scheduling, generic audible stop, DSP, parameters/state, bridge,
original UI, reference and dependency/SDK sources are unchanged. There is no new
callback allocation, deallocation, lock or loop; the existing twenty-track bound
and stream overflow behavior remain. See docs/MIGRATION_CURRENT.md for the compact
parity checklist, acceptance criteria, baseline and exact commands.

Limit: cancellation applies only while the message owner is still queued. An old
teardown changing that state retains the existing rejection boundary; full queued
message callback/graph transaction, late-source sound alignment and wall timing
remain unaccepted. This repair does not enable visible meters or complete the
overall meter migration. No new REAPER/Ardour, physical hardware, Apple or native
sanitizer acceptance; no package, version/dependency update or push.
STOP/STOP NOW after audible grid also require separate pending-intent validation;
this STOP ALL repair must not be reported as acceptance of all queued controls.

Final results (build/post-grid-panic-evidence.json):

| Gate | Result |
| --- | --- |
| Windows portable | 28/28, 397.04s |
| Ubuntu portable | 28/28, 632.81s |
| Windows native processor / SDK generation | 2/2, 11.99s |
| Ubuntu native processor / SDK generation | 2/2, 23.92s |
| Core AddressSanitizer selection | 6/6, 289.48s |
| Committed PCM | 126 cases, 965826 samples, max error 7.394757495227111e-8, unchanged 1e-6 tolerance |
| Original UI / embedding / tamper | Pass |

Native runs retain all eighteen linked production AudioDeviceManager blocks.
Final logs contain no skips, JUCE assertions, leak reports or sanitizer errors.
Existing Standalone binaries were rebuilt on both hosts. SDK/other-format binaries
and committed logo remain unchanged. The report verifies the exact source diff,
retained pre-fix failure, scoped markers and source/binary/log hashes. Full
configure/build/platform instructions remain in BUILDING.md; the compact resume
note contains the focused and full commands actually used here.

### Pending STOP NOW after audible grid — 2026-10-28

Baseline `3ad30bc`. The new core regression first failed on the existing code:
`Late STOP NOW must emit queuedStop for the pending intent after audible grid`
(`build/post-grid-stop-now-before.txt`). The production change selects the existing
queuedStop route from the pending queue ID instead of native queued state, and
thereby retires the audio-side ID before a later STOP ALL. Generic audible stop,
scheduled STOP, parameters, persistence, frontend/reference and dependencies are
unchanged. This adds no allocation, loop, timer or lock to the audio callback.

Added graphless/attached cases at tracks 0/19, 64/128 with actual queued-to-playing
transitions and exact paired stereo PCM; wrong/duplicate queue receipt and obsolete
timer rejection; retained analyser/data/history; no orphan cancellation. Production
processor bridge requests and actual JUCE timers verify fresh 25ms cleanup and
frozen-clock history. Four unchanged-original stop() cases supplement all previous
136 lifecycle, 14 scheduling, 22 resume, 14 panic, 4 queued-stop, 4 late-panic and
96 native queue-delay comparisons. Attached reference fixtures explicitly supply
pending state; they do not establish queued graph-origin or browser sound parity.

| Executed gate | Result |
| --- | --- |
| Windows portable | 28/28, 371.82s |
| Ubuntu portable | 28/28, 619.28s |
| Windows native processor / SDK generation | 2/2, 11.16s |
| Ubuntu native processor / SDK generation | 2/2, 23.92s |
| Core sanitizer selection | 6/6, 290.04s |
| Committed PCM | 126 cases, 965826 samples, max error 7.394757495227111e-8, unchanged 1e-6 tolerance |
| Original UI / embedding / tamper | Pass |

No skips, JUCE assertions, leak reports or sanitizer errors occurred in these
runs; existing native test compiler warnings remain. Both current Standalone
development binaries rebuilt; plugin formats were not rebuilt or DAW-tested.
The actual callback checks retain zero tracked new/delete. Commands are in
MIGRATION_CURRENT.md, with configure/platform instructions in BUILDING.md.
Ignored report `build/post-grid-stop-now-evidence.json` verifies gate counts,
markers, exact production diff, original logo and source/binary hashes.

Next: normal synchronized STOP after grid, then the queued callback transaction
and acceptance/rejection recovery. Old teardown changing message state, late
source sound, remaining graph origins/continuity and renderer remain unaccepted.
No new hardware, REAPER or Ardour acceptance; macOS/iOS are explicitly deferred.
No package or push was performed for this repair.
