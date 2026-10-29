# Current migration checkpoint

## Latest implementation � 29 October 2026

Code snapshot `82c3a53` adds replacement-source retirement to the late-callback
admission work in `27063ed`. A retained old cleanup now cuts the replacement
queued source at the next audio callback, without a residual STOP tail. Pending
callbacks cannot resurrect a retired source; callbacks issued after cleanup and
fresh playback survive. Full command queues cannot lose cleanup. Reference assets,
parameter/action IDs, capacity and session format are unchanged.
See the latest source-cleanup evidence below. Next: early graph-birth control
edits before the future grid, then remaining overlap/envelope sound gates. Full
looper sound/renderer, DAW/device and offline MIDI-render acceptance remain open.

## Owner-approved faster development workflow — 28 October 2026

Active delivery is Windows Standalone/VST3 and Linux Standalone/LV2. CLAP is
preserved but opt-in/deferred; Apple delivery remains future work. Read the new
AGENTS.md policy first, plus DEVELOPMENT_WORKFLOW.md and DEVELOPMENT_PROMPT.md.
Use windows-dev/linux-dev for incremental Standalone builds; run affected checks
while iterating, relevant Windows/Linux acceptance at feature completion, and
applicable full selected-format gates at integration/release. Observable sound,
timing, controls/state and real-time ownership stay strict; identical internal
browser graph machinery is not a product requirement. Do not weaken regressions.

Native WSL mirror is at `/home/jorge/projects/Aion-Looper`, with independent JUCE,
reference and build directories. Windows remains the editing/publishing owner.
SetupWslWorkspace.sh mirrors committed main, handles CRLF-normalized caches and
refuses dirty/unrelated destinations; guard probes passed without changing HEAD
or protected files. Uncommitted Windows edits are not copied. Missing historical
Git blob 7542c2300d1c793a942ee91a0c2eb093e1910347 (an older CMakeLists.txt) was
restored by `git fetch --refetch origin main`; connectivity fsck now passes. No
working files or branch history were rewritten for that repair.

Verification so far: Windows preset configuration + Standalone-only build passed;
CLAP is absent from the regenerated solution despite an existing cached validator
path. Four focused Windows infrastructure tests passed in 7.98s; a subsequent
Standalone incremental build took 5.05s. Native WSL core build and three focused
infrastructure tests passed in 10.12s, including exact original UI bytes. This is
not a new full audio/DAW/hardware acceptance run.

Matched configure comparison at source f0c1264 / pinned JUCE 9.0.4, Release,
Standalone/LV2 ON, CLAP/VST3 OFF, BUILD_TESTING OFF: fresh setup 288.21s mounted
versus 46.22s native; warm configure 176.15s versus 1.87s (~94x in this one run).
Evidence/commands: `build/wsl-configure-benchmark.json`, with each tree's
`build/workflow-benchmark` logs. Configure timing is not build/runtime timing.
Native Linux preset configuration passed in 48.71s; its first Standalone-only
build passed in 280.47s and LV2 in 12.39s. Windows VST3 also rebuilt successfully.
Native target inventory contains Standalone/LV2 and no CLAP/VST3. Detailed native
phase commands/results are in `build/wsl-native-build.json` and native
`build/workflow-{configure,standalone,lv2}.log`. These builds are not new DAW,
hardware or full audio acceptance. No DSP/parameter/UI implementation changed
and no packages were generated. Native Standalone incremental no-op took 0.75s.
Next: sync committed main with the helper, then
resume the current migration slice under the new focused workflow.

## Documentation, target and license decision — 2026-10-28

- README now describes the default 10 loop / 10 drone / 10 sampler workspace.
  Native 20-slot capacity, IDs and compatibility are unchanged; drone/sampler
  ports remain pending. README source-build commands are Linux-only.
- Linux VST3 generation and its optional CTest are removed, including cached
  VST3=ON configurations. Windows VST3 remains. Active build instructions reflect
  this; historical validation and the shared LV2 validator helper are retained.
- Owner selected AGPL-3.0-only after discussing GPL-2.0/JUCE incompatibility.
  Root LICENSE uses the standard AGPLv3 text from the pinned JUCE checkout and
  package rules include it. Original UI/license files stay unchanged. Original
  GPL-2.0-only asset permissions and dependency inventory remain release checks.
- Chrome is not a drop-in JUCE browser backend. Windows still uses WebView2
  Runtime, independent of the Edge browser. CEF would be a separate integration;
  no browser/dependency switch was made.
- README links/fences and whitespace checks pass. Linux configuration completed
  with an explicit VST3=ON request reset to OFF; Windows configuration completed
  and retains its VST3 project. Direct Ninja target inventory confirms Linux
  Standalone/CLAP/LV2 remain and VST3 is absent; the negative VST3 build/query
  rejects the unknown target as expected. No DSP code changed, audio suites were
  not rerun, and no packages were generated.

Updated 2026-10-28. Current CLI source baseline: `21f9a32`. The session began at
`24df7e9`, clean main twelve commits ahead of cached origin; the verified queued
edge-case slice was committed separately before adding the running-app CLI.
Configured cleanup deadlines retain the accepted command-counter integration.
Continue the existing implementation; this is not a restart or release checklist.

## Reference and baseline

The requested `D:\Jorge\Documents\Codex\Aion-Looper.reference` path is absent.
The available original is `.reference/Ascii-Looper` inside this repository.
`tests/AssetTests.py` verifies embedded original UI bytes against that copy.
Neither reference nor original UI files are modified by this repair.

The baseline evidence is `build/stop-clock-evidence.json`: Windows/Ubuntu
portable 28/28 each, native 2/2 each, core sanitizer 6/6, 96 queue-delay cases,
288 counter cases / 864 reads, and 126 committed-PCM cases. A fresh Windows
native baseline passed before the timer integration. Historical
STOP ALL and STOP NOW failures/evidence remain in VALIDATION.md and `build/`.

## Compact parity checklist

| Reference behavior | Native status / evidence | Remaining gap |
| --- | --- | --- |
| Capture and accepted loop edits preserve committed PCM | Existing scoped audit: 126 capture/OD/SUB cases at 1e-6; other loop-edit gates retained | Full browser envelopes/resampling and physical input timing are separate |
| Queue callback uses max(0, grid delta minus 100ms) | Exact accepting callback retained; immutable command-counter deadline; 96 original delay cases plus 288 offset cases / 864 deadline reads at 1e-12 | Delivery is now automatic on the existing 25ms service tick; punctual event-loop timing and full source sound remain unaccepted |
| Stop cleanup uses 25ms / unrounded lead+50ms / 65ms | Configured live JUCE timers now retain command-origin deadlines; original 144 stop cases / 432 reads retained | Event-loop jitter, UI/physical timing and full teardown sound remain unaccepted; explicit compatibility paths stay receipt-based |
| STOP ALL cancels a still-queued intent even after its grid time | This repair routes queuedCancel by explicit pending ID; core, production processor, 20-route and real-timer regressions | Late-source admission now prevents pre-callback audio; old-cleanup source lifetime sound remains separate |
| STOP NOW clears pending queue before immediate graph cleanup, including after grid | Explicit queue-ID routing; graphless/attached, production requests and real 25ms timer checks | Normal synchronized STOP and old-teardown state boundary remain separate |
| Normal STOP takes immediate path when sync is disabled | Pending-ID routing covers sync disabled after grid; accepted graph origins support immediate and synchronized stops before/after grid | Actual automatic callback delivery and full transition sound remain separate |
| Queued cancellation retains older analyser/data/history and cleanup | Existing owner/adapter policy retained, including original pre-grid test plus added post-grid frozen-clock test | If old cleanup already changed message state out of queued, existing rejection remains intentional pending full mapping |
| Grid deadline does not prove source/graph birth | Production keeps queued state and silence until validated callback; raw resets cannot admit audio | Visible meters, remaining capture/edit graph origins, overlap and continuity remain unfinished |
| Pending intent requires its conditional callback transaction | Generic start rejects real queued IDs; explicit graph request/result now binds a fresh graph with rejection/loss/cleanup recovery | Default timer now requests due real-ID callbacks; full source lifetime remains unaccepted |
| Conditional raw reset reports its outcome without graph creation | Explicit request/result handshake, reserved tickets, typed outcomes and production routing; old graph/binding/cleanup retained | This is raw-window readiness only; graph acceptance, source timing and renderer remain separate |
| Explicit queued callback creates a graph independently of old cleanup | Unchanged-source/component comparison: 72 scenarios, 1,278 additional observations; fresh analyser/data, retained old deadlines, cleanup before/after callback, repeated immediate/scheduled/panic stops | This oracle supplies explicit notifications; the real-ID transaction is checked separately, and late source admission now has PCM/Engine/Processor coverage; wall/audio alignment and full cleanup sound remain open |
| Explicit graph callback owns subsequent stop routing | Real queue IDs, 120 core lifetime/stop combinations, prepared processor hook and twenty-track result/stop routing pass | Automatic service delivery is covered below; late source phase is validated below; overlap/envelopes and wall/audio alignment remain unaccepted |
| Due callback retains its original intent and fires once | Guarded local-owner delivery checks deadline and stable receipt, preserves cleanup ordering, rejects stale replacement and retries FIFO failure | Existing processor timer supplies autonomous delivery below; zero-delay/backlog interactions and full source sound remain separate |
| Full instruments/effects/controller/persistence/host parity | No new acceptance from this session; see PARITY.md and feature-specific validation notes | Do not infer suite completion from these looper/meter gates |

## Previous slice: running-Standalone development CLI

The user chose control of the existing Standalone session/devices, not a second
headless engine. `tools/aion.py` exposes the current native JSON handler through
named status/parameter/loop/MIDI/name/metronome/export operations and raw `request`.
See CLI.md for exact commands and the distinction between accepted actions and
completed recording/finalization/export. Unported engines and remaining session/
device setup operations remain unavailable; this is not full feature parity or
a bundled release CLI. Every newly ported feature must extend command coverage.

Only the desktop Standalone holder's own processor creates the local server,
independently of editor lifetime. Private per-instance directories and tokens,
kernel-held client locks, atomic request claim, expiry, explicit selection,
bounded bytes/depth and one request per message tick keep ownership bounded.
The immediately previous result can answer an immediate duplicate-ID reply retry;
older results retire, and the client never blindly resubmits. No file, JSON,
serialization, locks or CLI work enters processBlock. No TCP port is opened.

Named loop commands wait for their exact native outcome. A later controller's
result cannot establish success/failure of an earlier command. The GUI now uses
the same exact-ID rule; a superseded outcome reports uncertainty and retains
authoritative snapshot state. The regression first failed against the previous
`>=` policy (`build/cli-gui-outcome-negative.txt`), then the actual production
block passed all 24 older/exact/newer/generation/full-width cases. Original HTML,
CSS, reference files, visible controls and ASCII logo bytes remain unchanged.

External Python processes exercise actual native processors and synthetic PCM at
64/128, including instance isolation, rejection after enqueue, names/Unicode,
MIDI configuration, authentication, version/expiry/size/depth/UTF-8 guards, busy
locks, timeout, duplicate reply and owned-file shutdown. Existing timer tests
retain their own 30s watchdog after a separate 60s external CLI stage. Initial
Windows reply-sharing and aggregate-watchdog failures are preserved in build/.
Final native results: Windows 2/2 (33.39s), Ubuntu 2/2 (49.55s), both development
Standalone builds. Actual Windows CLI registration/shared WebView roundtrip,
NativeUiSmoke, browser audio-reference checks and full synthetic 64/128 mouse/
export/merge/MIDI workflows pass. Windows frontend policy passes 24 new cases;
Ubuntu affected frontend CTest passes 1/1 (8.90s). Original assets pass again.
The earlier full portable 28/28 each and unchanged 126-case PCM audit are retained
for unchanged core/oracles; affected frontend/native/UI checks are rerun after
the CLI/acknowledgment change. No final skipped check, JUCE assertion or leaked
object; existing compiler warnings remain. Evidence: `build/cli-evidence.json`.
No new sanitizer, DAW/plugin-format, physical controller/device, listening or
Apple validation. No package or push. See VALIDATION.md for commands and scope.

## Previous slice: zero-delay and full-batch queued callbacks

The real production message-loop runner now covers fourteen cases across tracks
0/19 and 64/128. A 999 BPM, one-beat grid produces the original clipped delay of
exactly zero: RPC/readout leaves it pending, the autonomous message tick enqueues
it once, and only typed acceptance creates graphs. Another case generates 34
actual ordered facts in one audio block, with track 0's already-due cancellation
last. A full 32-fact tick defers; the trailing cancellation is consumed before any
graph request. An internal message-only `queuedDeferredPasses` counter verifies
that the production deferral path was exercised. No DSP, UI, parameter or state
schema changes. Existing positive-delay, old-cleanup, loss and lifetime tests stay.

Verification: Windows/Ubuntu portable 28/28 (416.03s/668.92s), native 2/2
(15.05s/28.73s), development Standalone builds and original assets pass.
The unchanged 126-case / 965,826-sample PCM audit remains within 1e-6 (maximum
7.394757495227111e-8). Logs: `build/queued-edge-*`. No core/DSP changes,
sanitizer rerun, plugin-format/DAW/hardware test, package or push in this slice.

CLI decision: the user chose command-line control of the running Standalone app,
sharing its existing session/devices and validated command handler. The current
slice above implements its development transport and existing handler access.

## Previous slice: autonomous queued graph callback delivery

Only the existing processor Timer tick enables automatic delivery. RPC, inspection
and manual diagnostic service remain synchronous fact-draining operations without
automatic callback delivery. Each tick drains at most 32 raw facts first. A full
budget defers delivery until another tick reaches the end of the ordered stream,
so a queued cancellation cannot be overtaken behind an unfinished receipt batch.
Healthy prepared owners then read their retained original command fact and request
its exact due graph through the existing guarded one-shot transaction. Work is
bounded to twenty owners; failed FIFO enqueue retries on a later tick, without
spinning or changing the deadline. Result acceptance still owns graph creation.

There is no second fact consumer, timer allocation, audio callback change, new
parameter, JS operation or persistence schema. Missing/offline clock observations
remain explicit-only. Old independent cleanup may detach before callback delivery;
it must not erase the original still-pending queued fact. Release, generation and
loss guards remain in force. This uses the existing 25ms message-service cadence;
it does not promise a punctual OS timer or implement audible source-phase changes.

Acceptance: actual editor-free production timer, tracks 0/19 at 64/128, frozen
counter/early and overdue delivery, one-shot typed result, cancellation, release,
offline exclusion and old cleanup before callback; retain existing FIFO, stale
receipt, generation/loss and cleanup-after-enqueue guards and unchanged-JS gates.
Initial Windows run rejected an invalid test setup attempting simultaneous loop
recordings. LoopBank permits one capture; record each fixture clip separately.
Final verification: Windows/Ubuntu portable CTest 28/28 each (409.83s/664.45s),
native 2/2 each (13.89s/27.71s), both development Standalone builds, original
UI/embedding/tamper checks, and the unchanged 126-case PCM audit pass. PCM remains
965,826 samples, maximum error 7.394757495227111e-8 at 1e-6. No final skipped test,
JUCE assertion or leaked-object report; existing compiler warnings remain. Core
sanitizers are not rerun because core/audio sources are unchanged. Logs use
`build/automatic-queued-*`; the checked local manifest is
`build/automatic-queued-evidence.json`. See VALIDATION.md for exact commands and
scope. No new plugin-format, DAW/hardware, listening or Apple validation; no package
or push.

## Previous slice: configured-counter cleanup timers

Production adapters receive the same immutable counter reader as processBlock.
Valid live stop facts use their captured entry + sample offset + original delay.
The first receipt counter is the regression floor; it does not postpone the due
time and may precede a logical sub-block command. Overdue cleanup is scheduled
asynchronously at the next eligible JUCE tick, preserving ordered dispatch.
Synchronisation validates/cancels without rearming. Prepared generation, owner
lifetime, old-cleanup retention and already-armed loss behavior stay unchanged.

Manual stops, canonical missing entry, offline facts and unconfigured component
adapters retain their existing receipt-origin behavior. Invalid present stamps
or counter values reject before mutating the owner. These paths are explicit;
no inferred host/DAC correlation or latest-view fallback is introduced.

Acceptance: pure immutable absolute expiry, overdue/no-rearm, receipt before
logical command, duplicate/invalid-input guards; actual JUCE message-loop delivery
with a controlled configured counter, all four overdue stop policies at 64/128,
future offset and early wake, missing/offline compatibility and malformed rejection,
frozen audio/data retention, exact paired PCM and zero tracked callback new/delete.
Existing production, cancellation/replacement, release/prepare/destruction and
all-track routing tests remain enabled. No automatic queued callback is activated.

## Previous slice: stop command / cleanup-request correlation

ImmediateStop, scheduledStop, stopAll and queuedStop facts now copy the existing
accepting callback stamp. Queued cancellation remains distinct and cannot imply
cleanup. A pure validated policy maps processor-entry plus callback-local frame
offset to command-counter time, then adds the original delay: 25ms immediate or
queued STOP; unrounded lead + 50ms scheduled; 65ms panic. Late reads reduce the
remainder without moving the deadline. Host/DAC time, output latency and later
views cannot replace that captured edge. Known zero is valid; missing, offline,
foreign, stale, malformed or precision-losing correlation rejects unchanged.

The message owner retains each actual stop fact with its exact cleanup request.
The internal `readMeterCleanupCallbackTimeMessageThread(track)` uses the configured
processor counter and validates the prepared generation. Delivered/cancelled or
replaced requests cannot reuse old correlation. A manual stop lacking a captured
command cannot borrow one either. A still-pending old cleanup retains its own time
when another queued source is accepted. Graphless stops do not invent a request.
This adds fixed copies to existing preallocated storage; no audio clock queries,
allocation, locks, serialization or timer scheduling change.

Acceptance: original four stop policies, exact sub-block capture at tracks 0/19,
64/128; full-width frames and 8/44.1/48/96k pure-clock cases; zero/variable blocks,
invalid/offline/foreign inputs with unchanged outputs; late/missing-view retention,
request replacement/delivery/manual-stop/prepare rejection, exact paired PCM and
zero tracked callback new/delete; native reader and original JS delay comparison.
The copied-policy negative must reject receipt-plus-delay without modifying any
production source/object. Actual timer activation and full source sound remain
unaccepted. This command-counter model is not a physical latency measurement.

## Previous slice: guarded due callback delivery

Acceptance: a retained callback receipt may request its graph at or after its
original deadline, once successfully enqueued. Recompute timing from the owner's
original fact and validate generation, track, full-width queue/clip, event/callback
serials, clock origin, command time and deadline. The receipt's remaining time is
an earlier observation, not authority to fire. Early/foreign/nonfinite clocks or
altered stable receipt fields reject without request/graph/cleanup mutation. An
old receipt must never retarget a newer queue on the same clip and track. FIFO failure permits retry;
successful enqueue consumes the callback even if later cleanup retires its result.
Preserve cleanup before delivery versus cleanup after enqueue, typed acceptance,
prepared lifetime guards and zero tracked callback allocations at tracks 0/19,
64/128. No timer, public JS operation or persisted field is added in this phase.

The initial focused run reproduced duplicate enqueue after cleanup retired a
pending result (`build/queued-due-lifetime-failure-{windows,linux}.txt`). Keep this
regression; fixing it requires a delivered-event high-water mark independent of
pending transaction status, not weakening cleanup retirement.

`requestQueuedGraphDue` implements that gate on the existing message owner. The
prepared cleanup adapter and `requestQueuedMeterGraphDueMessageThread(receipt)`
expose it internally using the processor's configured counter. Successful enqueue
records the delivered event serial independently of pending-result cleanup; FIFO
failure does not consume it. Existing manual raw/graph hooks remain unchanged.
Receipt values use the local Engine namespace: future timers must remain bound
to their owner/processor lifetime, not transfer these readouts between instances.
No new timer, JS/IPC operation, audio processing or persisted state is activated.

## Previous slice: queued command clock correlation

Each actual queuedPlay fact now carries the callback stamp captured before MIDI
and command dispatch. The message owner retains that exact fact independently of
later or missing processor views. The existing fixed heap-owned SPSC queue still
has 255 usable entries and the same fail-closed generation-loss policy; callback
work adds fixed copies, without new clocks, allocation, locks or IO.

For a validated shared monotonic counter origin:

```text
commandSeconds = acceptingEntrySeconds + (commandFrame - beginFrame) / sampleRate
deadlineSeconds = commandSeconds + unroundedQueueDelay
remainingSeconds = max(0, deadlineSeconds - currentCounterSeconds)
```

Frame subtraction occurs as full-width integers before conversion. This is the
processor-entry command-counter model, not physical output timing. Host nanoseconds,
device/DAC delivery, output latency, later snapshots and receipt-plus-delay cannot
substitute for that correlation. Late receipts reduce remaining time to zero and
never move the deadline. A known zero entry/deadline is valid, including a real
zero-frame MIDI callback. Missing entry, offline callbacks, foreign origin, invalid
generation/range/nonfinite values and lost precision reject without changing output.

`readQueuedMeterCallbackTimeMessageThread(track)` is an internal native readout
using the same configured reader as processBlock. It services the existing bounded
route and validates the prepared generation. It adds no JS operation, timer,
parameter or serialized-state field. Old cleanup can leave a still-live intent and
its original deadline; cancellation, clear, replacement, prepare and stream loss
make it unavailable. Loss recovery drains old notices before prepare, preserving
the existing queue ownership rules.

Acceptance covers pure clock/range/precision failures, actual sub-block commands
at tracks 0/19 and 64/128, later/missing-view independence, zero-frame MIDI,
old cleanup, cancellation/clear/prepare/loss recovery, exact paired PCM and zero
tracked callback allocations. Original grid/dispatcher delays are compared at
three offsets and three receipt lags: 288 cases and 864 deadline reads at 1e-12.
The intentional receipt-time-rearm negative must fail before restored-object gates.

Automatic scheduling remains a separate slice: timer fire must validate its stored
generation, queue, clip and event identity, not retarget a newer queue via bare track.
Cleanup clock origins and source lifetime/sound still need independent alignment.

## Previous slice: explicit queued graph transaction

The new core-only `meterQueuedGraphRequest` shares the bounded command FIFO,
ticket reservation and full-width queue/clip validation. It is a distinct request
from the existing raw-only reset. The audio owner resets the window once and
publishes distinct graph accepted/rejected results. A successful explicit callback
also establishes the audio-side origin for subsequent immediate/scheduled/panic
stop facts; a raw reset alone still cannot do so. Enum values are appended.
The callback must request graph acceptance directly. The retained raw-only hook
is not a first step to promote afterward: one queue can reset its window once,
so consuming that reset through the raw-only primitive makes a later graph
request for the same intent reject safely. Its existing diagnostic contract stays.

The message owner creates and binds a fresh graph only for its exact live graph
request and matching accepted result. Enqueue/raw started/raw typed acceptance
cannot create it. A newly requested callback may begin from stopped after older
cleanup, provided its real queue/clip intent remains alive. Cleanup delivered
after enqueue instead retires that pending result; it cannot resurrect a graph.
Detached stop/new-queue routing validates the current audio token, and graph
creation retains an existing independent cleanup deadline. No new timer is armed.

For a synchronized stop after accepted graph creation but before source start,
the audio owner keeps its queued gate and records the stop deadline separately.
It transitions to stopping only at the original start deadline, then stops at the
scheduled end. Setting stopping immediately would render audio early. Repeated
normal STOP during this pending stop takes the immediate path.

`requestQueuedMeterGraphMessageThread(track)` is an internal production hook.
The existing sole prepared dispatcher and cleanup adapter route its result and
later stops. It is not a new JS operation, parameter, device setting or automatic
100ms timer. The existing `windows/windowApplied/windowIgnored/pendingWindows`
diagnostics now include both transaction types; `readyWindows` remains raw-only.

Acceptance: tracks 0/19 and 64/128, before/after source grid, graphless/attached,
cleanup before request/before result/after result, fresh binding and real window
observation, original cleanup retention, all three stop policies and repeated
STOP/panic; typed rejection, cancellation/clear/prepare, zero callbacks, FIFO
saturation, bounded outcome loss and fresh-generation recovery. Keep previous
raw-only tests and the 72-case unchanged-source oracle without tolerance changes.
The intentional pre-grid gate regression must fail before restored-source gates.

Full source envelopes, old/new audible overlap and late-source phase remain
unaccepted. These component windows do not establish browser sound parity.
Establish valid wall/audio alignment before wiring automatic delivery; then
validate actual callback delivery, cleanup/source sound and rendering end to end.

## Previous slice: queued graph callback acceptance oracle

Before changing production graph acceptance, make its original lifetime contract
executable. `tests/MeterGraphParity.cjs` now invokes the actual original
`handleAction` queue callback, `Loop.play`, `scheduleStop` and `stopAll`. Its 72
scenarios cover tracks 0/19, early/late/frozen audio-clock delivery, graphless
starts and old cleanup before/pending/after callback, with three stop policies.
All original 377 observations remain; the complete trace now has 1,655.

The reference proves two different events must remain distinct:

- A still-pending original queue callback survives `_teardown`. If cleanup ran
  first, that later explicit callback invokes `play(grid)` from stopped and
  creates fresh analyser/data identities. Rejecting every callback whose current
  state is not queued would lose this original behavior.
- An older cleanup still pending when the callback creates a graph is retained.
  If delivered next, it tears down the new graph too and retains its data/history.
  A subsequent stop must not rearm a detached owner. Live repeated immediate and
  panic stops replace deadlines; `scheduleStop` refuses an already-stopping graph.

These are component acceptance facts, not new production origins. The retained
graph/stopped precondition is supplied explicitly; no end-to-end UI path to it is
claimed. Native graph/lifecycle probes receive explicit graph notifications and
compare identities/state/delays plus the existing graph follower readings. They
do not use real queued IDs, commit the raw-window transaction, or measure source
PCM/device timing. The supplied source start time/offset is checked against the
unchanged reference, not a native audible-source implementation.

Keep the completed transaction's stale-result rule: cleanup before a pending raw
result retires that result, which cannot resurrect a graph. A newly delivered
explicit callback after cleanup needs its own provenance/acceptance path; do not
turn an obsolete result into that callback or remove current queue guards.

Acceptance: unchanged 1e-12 comparison tolerance, all old observations retained,
fresh identities and exact old deadline retention, original callback delivery
after old cleanup, repeated/detached stop rules, and an isolated negative control
that rejects a queued-only callback guard. Logs use `build/queued-callback-oracle-*`.
No application source, parameters/state, dependencies, original assets, or
format/package configuration changed.

## Previous slice: conditional raw-window transaction

Implement the conditional raw-window request/result stage. The message owner
reserves a monotonically increasing ticket and sends full-width queue/clip IDs
through the existing command FIFO. The audio owner revalidates against live
queue/clip/state, performs the existing raw reset once, and publishes a typed
accepted or rejected outcome. Raw started notices alone cannot complete it.
The old low-level reset contract remains unchanged; new internal enum values
are appended. No public parameter/state/IPC IDs or assets change.

Acceptance: exact-ID typed results complete once; malformed/raw/stale results
cannot complete a request. Enqueue is not acceptance. Keep one pending request
per owner, reserve tickets across rejection/clear, and expose ready only while
its queue/clip/state/raw token are still current. Retain graph/analyser/binding,
data/history and old cleanup. An explicitly proven raw rebase permits queued
cancel/STOP facts with quarantined ticket zero against an older attached graph;
it never restores playback origin or permits old-binding PCM observations.
Test graphless/attached tracks 0/19, before/after grid, zero/64/128 callbacks,
FIFO rejection, bounded outcome loss, fresh-generation recovery, twenty prepared
routes with 7/32/1 raw drains, production processor requests and real JUCE timers.

The original source remains unchanged: its queued handler schedules an explicit
callback which invokes play(grid). This stage deliberately stops before that graph
creation. Existing original-source comparisons remain enabled; no new browser
sound acceptance is inferred from native protocol tests. The baseline focused
binary passed (`build/queued-window-transaction-baseline.txt`). Suppressing typed
negative results caused `Typed rejection must terminate an invalid queued window
request` (`build/queued-window-transaction-negative.txt`). The Engine object was
rebuilt after restoration and focused positive tests passed again.

`requestQueuedMeterWindowMessageThread(track)` is an internal message-thread hook,
not a new JS/IPC operation or automatic timer. The existing sole dispatcher handles
outcomes; windowApplied counts handled results, including negative outcomes, while
readyWindows counts current raw readiness. Retired/unsolicited results are ignored
separately from user-action errors. Cancellation, clear, a new intent and old cleanup
retire pending work. Lost streams fail closed until prepare. Old cleanup delivered
before audio acceptance cannot resurrect a graph or timer.

Previous STOP ALL acceptance remains in force: pending-ID cancellation before/
after grid, stale/duplicate rejection, ordered delayed receipts, twenty bounded
7/13 routes and original cleanup/history. See VALIDATION.md and
`build/post-grid-panic-evidence.json`; those regressions remain enabled.

## Reproduce

Windows (PowerShell, repository root; use the bundled CMake/CTest path if not on PATH):

```powershell
cmake --build build/windows-core --config Release --parallel 2
build/windows-core/Release/aion_core_tests.exe --callback-clock
build/windows-core/Release/aion_core_tests.exe --meter-queued
ctest --test-dir build/windows-core -C Release -V
cmake --build build/windows --config Release --target aion_midi_processor_tests AionLooper_Standalone --parallel 2
ctest --test-dir build/windows -C Release -V -R '^(native_midi_processor|standalone_clock_sdk_generation)$'
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
python tests/AssetTests.py
```

Ubuntu, in `/mnt/d/Jorge/Documents/Codex/Aion-Looper`:

```sh
cmake --build build/core -j2
build/core/aion_core_tests --callback-clock
build/core/aion_core_tests --meter-queued
ctest --test-dir build/core -V
cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone -j2
DISPLAY=:0 ctest --test-dir build/linux -V -R '^(native_midi_processor|standalone_clock_sdk_generation)$'
cmake --build build/core-asan -j2
ctest --test-dir build/core-asan -V -R '^(core_realtime_contract|js_native_playback_clock|js_native_meter_graph|original_meter_source_lifecycle|js_native_meter_window|js_native_meter)$'
```

Retain BUILDING.md's configure/dependency/platform instructions. No dependency
upgrade, new format/package or UI activation is needed for this slice. Never edit
compiling sources or overlap CTests in a build tree.

## Next concrete step / validation still needed

Current integration evidence: `build/correlated-cleanup-evidence.json`.
Each platform passes a complete direct core audit plus 27/27 remaining CTest
checks (Windows 278.35s, Ubuntu 583.70s for those 27). Native gates pass 2/2 each
(12.24s/25.21s), core sanitizer selection 6/6 (457.77s). Real configured-clock
JUCE cleanup cases, original queue/stop/graph comparisons, PCM and asset checks
pass. PCM remains 126 cases / 965,826 samples, max error 7.394757495227111e-8
at unchanged 1e-6. No final skip, sanitizer error, JUCE assertion or leaked-object
report; existing compiler warnings remain. Isolated production policy exits 0
and copied receipt-rearm negative exits 1. Both development Standalone binaries
rebuilt. No package or push; no new plugin-format or DAW/hardware validation.

Previous stop-clock evidence: `build/stop-clock-evidence.json`.
Windows/Ubuntu portable 28/28 (417.43s/671.34s), native 2/2 (11.61s/23.91s),
core sanitizer 6/6 (467.25s), fresh focused policy/actual-event tests, original
144 stop cases / 432 deadline reads, prior queue/graph oracles, 126-case PCM audit
and original asset checks pass. PCM remains 965,826 samples with maximum error
7.394757495227111e-8 at unchanged 1e-6. No final skip, sanitizer error, JUCE
assertion or leaked-object report; existing compiler warnings remain.
The isolated copied-policy positive exits 0 and receipt-rearm negative exits 1.
See VALIDATION.md for fixture/harness corrections and actual verification scope.

Previous guarded-delivery evidence: `build/queued-due-evidence.json`.
Windows/Ubuntu portable 28/28 (405.64s/671.27s), native 2/2 (11.68s/24.08s),
core sanitizer 6/6 (453.38s), rebuilt focused queued regressions, unchanged 126-case PCM
audit (965,826 samples, max error 7.394757495227111e-8 at 1e-6), and original UI
byte/embedding/tamper checks pass. No unrelated failing gate, skipped check,
sanitizer error, JUCE assertion or leaked-object report was found. Existing compiler
warnings remain. The unchanged 72-case graph oracle retains all 1,655 observations
at 1e-12; 96 original queue timings and the new 288 counter cases / 864 reads pass.

The initial duplicate-after-cleanup failure is retained in
`build/queued-due-lifetime-failure-{windows,linux}.txt`. The delivered-event guard
fixed it; affected objects were rebuilt and focused positives passed before broader
gates. Prior correlation/graph/STOP evidence remains in VALIDATION.md and build/.

Both Standalone development binaries and native test binaries rebuilt. Prior raw
transaction/STOP evidence remains in VALIDATION.md and `build/`; its regression
cases are retained. Application parameters, serialized state, original assets,
dependency versions and target configuration remain unchanged.

Zero-delay and full-budget cancellation interaction checks now pass. After the
running-Standalone CLI, compare late-source phase,
outgoing/incoming overlap and cleanup sound against the original before enabling
the renderer. Preserve the original stopped-after-old-teardown state boundary:
original STOP ALL cancels queueTimeout only while message state is queued. Do not
silently broaden cancellation or weaken pending-result retirement. The raw-only
hook and generic start must not become shortcuts to graph creation. A native
audible source still does not prove a message callback or graph result occurred.

For the previous cleanup slice's portable gates, run `aion_core_tests` without arguments, then
`ctest --test-dir build/windows-core -C Release -V -E '^core_realtime_contract$'`
or Ubuntu `ctest --test-dir build/core -V -E '^core_realtime_contract$'`.
This is one complete direct core audit plus 27 remaining CTest checks per platform.
The earlier `--meter-graph` command was unrecognized and executed that same full
audit; its final PASS and new correlated-deadline marker are required as evidence.
Native two-test, sanitizer six-test, PCM and asset commands above remain unchanged.

Full transition sound/late-source alignment is still unaccepted. No REAPER/Ardour
or physical-controller/device tests were added here. Rebuild the relevant current
plugin formats before host validation; previously built other-format binaries
remain their old source snapshots. Apple builds/device validation remain deferred;
this lifecycle change stays in shared portable code and retains platform build
configuration for future macOS/iOS work. Existing preview archives remain labeled with their
original commits; this session creates no package or push.

## 29 October 2026 — queued source-phase regression

Acceptance for this characterization slice: with the original scheduled `Loop.play`
callback delivered before the grid, the native source must remain silent before
its sample deadline and match the original graph's source PCM/phase from that
sample onward. Preserve the existing 1e-6 tolerance and all prior phase cases.
This covers 30 mono/stereo, 8/48/96 kHz, 1/17/512/4097/20003-frame clip cases;
each adds fixed/reverse and dynamic/forward queued starts with a nonzero
Start offset, tempo-preserved origin and fractional grid deadlines.

`tests/StartPhaseParity.cjs` now executes unchanged `SyncManager.getNextGridTime`
and `Loop.play`/`AudioGraph.build`, then compares the conditioned buffer using the
existing linear interpolation oracle. The probe exposes the existing core PLAY
action. No DSP, UI, parameters, bridge, CLI product commands or state format changed.
This is core source-phase coverage, not a timer, browser resampler/envelope,
64/128 callback, interaction or DAW acceptance claim.

Windows Release: built `aion_start_probe`, then ran:
`node tests/StartPhaseParity.cjs build/windows-core/Release/aion_start_probe.exe`.
PASS: 30 cases, 60 queued starts, 201740 silent pre-grid frames,
2077240 total compared frames, maximum absolute error 6.250360756743234e-8
(tolerance 1e-6); prior 300 retriggers, 540 nudges and 288 tiny-wrap checks retained.
Linux reproduction follows after committing this coherent test snapshot.

Next: characterize late callback source birth/phase and then outgoing/incoming
cleanup overlap against the original audio. These remain unaccepted; keep the
renderer disabled and do not package this characterization as a completed feature.

Linux completion for the snapshot above (`ac02f40`):
`cmake --build build/core --target aion_start_probe --parallel 2`, followed by
`ctest --test-dir build/core --output-on-failure --no-tests=error -R '^(js_native_start_phase|original_ui_bytes)$'`
in `/home/jorge/projects/Aion-Looper` passed 2/2 (118.15s total;
PCM comparison 117.68s, asset check 0.46s). Windows `AssetTests.py` also passed
original UI byte identity, deterministic embedding and tamper rejection.
No source-phase defect was found in this scheduled-before-grid subset. Runtime
code did not change, so application/plugin rebuilds, DAW/device tests and packages
were not run for this test-only change. Existing late-source, overlap/envelope,
64/128 interaction and renderer acceptance gaps remain open.

## 29 October 2026 — queued source with a following-grid STOP

While continuing source/transition sound parity, found a smaller concrete defect:
an early queued graph callback exists, then normal STOP arrives within 30ms of
the source's upcoming grid. The original `scheduleStop()` minimum lead chooses
the following grid, so the incoming source must play until then. Native
`schedules()` correctly kept the start gate, but changed state to `stopping`
before `restartSource()`, whose playing-only guard skipped both Start phase and
reverse initialization. Fixed that guard to initialize either audible start state.
No IDs, serialized state, UI assets, CLI commands, queue policy or timer changed.
Existing PLAY/STOP through the shared command layer receive the correction.

Acceptance: silence before source start; original source phase/PCM after start
for forward and reverse mono/stereo clips at 8/48/96 kHz and fractional grid
positions; identical Engine stereo PCM to a normally started source stopped at
the same later grid, including completion/tail at tracks 0/19 and 64/128 samples;
no tracked callback allocation/deallocation. Keep existing source, callback,
identity, cancellation, overflow and ownership regression tolerances unchanged.

Before the C++ fix, the new unchanged-JS comparison failed at mono/8000Hz/17
frames, audio frame 3500: native 0.0458209291100502 versus original
0.045596589134934966 (tolerance 1e-6). After the fix, the new
`js_native_queued_stop_phase` comparison passed 30 cases / 60 queued starts,
1238502 silent pre-grid frames and 1244082 total compared frames; maximum error
3.683564875345269e-8 at the unchanged 1e-6 tolerance. This oracle invokes original
`SyncManager.getNextGridTime`, `Loop.play`, `AudioGraph.build`, `scheduleStop`
and `stop`; browser resampling/gain envelopes remain excluded.

Windows Release built `aion_start_probe` and `aion_core_tests`.
`aion_core_tests --meter-queued` and `--meter-stops` passed, including the new
8-case production Engine differential (2 block sizes x 2 tracks x 2 directions),
exact stereo PCM through the following-grid STOP/tail and zero tracked callback
new/delete. The Engine test uses the real integer queue/clip/ticket callback path;
the normal-source comparison receives STOP after start, choosing the same end.
One initial test assertion mistakenly expected the combined scheduled-frame
readout to expose the stop while its start was still pending; corrected it to
check the start gate first and the stop deadline after start. DSP/reference
expectations and tolerances were retained.

| Behavior | Current evidence/status |
| --- | --- |
| Early queued source start | Prior/current source-phase regression; preserved |
| Queued source starts while a later STOP is armed | C++ guard fixed; original source PCM + Engine 64/128 passed on Windows |
| Late message callback / source creation | Still unaccepted; no policy change in this fix |
| Old/new source overlap, cleanup sound and envelopes | Still unaccepted; renderer stays disabled |

Linux and active-format rebuild evidence follows after committing this coherent
snapshot. No package is produced. Next after this fix: late callback source birth
and phase, then independent old-cleanup overlap; preserve all cancellation and
retired-result guards while addressing those remaining gates.

Completion evidence for code snapshot `bbcc7da`:

- Windows Release: existing `js_native_start_phase` + `original_ui_bytes` passed
  2/2 in 42.93s; the new queued-stop oracle passed directly as recorded above.
  `cmake --build --preset windows-dev --parallel 2` and
  `cmake --build --preset windows --target AionLooper_VST3 --parallel 2` passed.
- Native WSL core Debug: built `aion_start_probe` and `aion_core_tests`; direct
  `build/core/aion_core_tests --meter-queued` and `--meter-stops` both passed,
  including the same new 8-case Engine stereo comparison and allocation guard.
  `ctest --test-dir build/core --output-on-failure --no-tests=error -R '^(js_native_start_phase|js_native_queued_stop_phase|original_ui_bytes)$'`
  passed 3/3 in 179.75s (existing phase 112.03s; new queued-stop phase 67.31s;
  original assets 0.41s). Original asset bytes and test tolerances are unchanged.
- Native WSL Release: `cmake --build --preset linux-dev --parallel 2`, then
  `cmake --build build/linux --target AionLooper_LV2 --parallel 2` both passed.
  The LV2 build emitted the existing WSL EGL/DRI3 acceleration warning; this is
  build/link evidence, not GUI or device validation.
- Reviewed the source diff and all `restartSource` call sites. The only newly
  initialized state is the queued-to-stopping start; other callers initialize
  playing sources as before. No allocation, lock, ownership field, unbounded
  work or state/parameter migration was introduced. `git diff --check` passed.

This narrow defect meets its source-PCM and Engine acceptance criteria. No new
host-routing/state/host-transport, frontend interaction or bridge behavior was
introduced. No new REAPER/Ardour, physical-controller, listening, real UI,
sanitizer/full-suite or Apple checks are claimed. Do not treat the rebuilt
formats as release acceptance: late source birth/phase, independent old/new
cleanup sound and browser envelopes remain open. The renderer stays disabled;
no distributable package or push was performed. Next is the late-callback sound
slice, retaining the existing graph-result/cancellation and overflow guards.

## 29 October 2026 — callback-controlled queued source admission

Scope/acceptance: a fresh queued PLAY must stay silent and queued when the grid
passes before its message callback. A valid queued graph request then admits one
source on the audio owner, using actual current-time phase as the original
`Loop.play(grid)` / `AudioGraph.build()` does when late. Early callbacks still
retain the future audio grid. Reject wrong, raw-only, cancelled, cleared and
prepare-stale requests without source birth. Preserve FIFO/identity/ownership
coverage, exact 64/128 stereo behavior and zero tracked callback new/delete.

Added an explicit immutable `QueuedStartMode`: direct DSP clients keep the existing
sample-grid scheduler; the production Processor selects message-callback starts.
This is independent of whether raw meter windows are observed. One audio-owned
pending bit defers the grid transition; only the already validated integer
queue/clip/ticket graph callback clears it. A late acceptance initializes phase,
reverse and source timing at the current audio frame; stop/clear/prepare retire
it. No new queue, allocation, lock, UI/file access, user command, ID, parameter or
serialized state is introduced. Existing GUI, running-app CLI and MIDI PLAY/STOP
all use the same Processor engine and service, including with the editor closed.
No visible-meter renderer is enabled.

Reproduced the defect before wiring the gate: at mono/8000Hz/17-frame clip, audio
frame 3500, native emitted 0.045596588402986526 while the original source was not
yet created (expected zero). New `js_native_queued_late_phase` then passed 30
mono/stereo 8/48/96kHz clip cases / 60 forward/reverse starts: 1273462 silent
pre-birth frames, 1279042 compared frames, maximum error 3.177606203164096e-8
(tolerance unchanged at 1e-6). The reference invokes unchanged scheduled `play`
late and checks its actual start offset. Browser gain envelopes/resampling and
wall-clock delivery error are not claimed by this source-PCM oracle.

Windows core `--meter-queued` and `--meter-stops` passed. New Engine tests cover
48 late-source cases at tracks 0/19, 64/128: fresh-source stereo comparison,
wrong clip, raw reset, STOP NOW, panic, clear and prepare. The following-grid STOP
regression now covers both scheduling modes (16 cases). One new fixture assertion
was corrected to expect prepare's existing pre-enqueue generation rejection.
Production post-grid tests now require silence and queued state rather than the
old premature playing state, while retaining their original queue/cleanup counts,
cancellation routing, stale result and orphan-panic assertions. They also require
real nonzero PCM after an explicit late acceptance. The actual JUCE Processor
suite passed 1/1 in 32.88s, including autonomous timer, MIDI, running CLI and
allocation checks. Active Windows Standalone/VST3 builds passed; remaining focused
reference/UI and Linux evidence follows after this coherent snapshot is committed.

Remaining scope: old/new source overlap and independent cleanup sound; exact
browser volume envelopes/resampling; parameter edits between early graph birth
and a future grid; complete wall/audio delivery alignment and DAW/device listening
acceptance. Offline autonomous callbacks retain their existing exclusion and
require explicit callback delivery in these deterministic offline fixtures; this
is not offline MIDI-render acceptance. No package or release-readiness claim.

Late-source edge follow-up (same slice): extended the oracle with sync-off,
tempo, speed and Start edits before late construction. The fade-free source
oracle passes 60 starts / 1272982 silent frames / 1278562 compared frames,
maximum error 4.034987166612325e-8 (same 1e-6 tolerance). The actual smoothed Engine
then reproduced a distinct mismatch when speed changed just before admission:
its new source continued an old source's rate ramp. Admission now initializes
rate from current target and clears that stale ramp, as a fresh original source
sets its playbackRate immediately. This happens only while the source is still
inaudible; subsequent running-source control smoothing is unchanged. Strict
64/128 stereo comparison now passes that edit for both directions/tracks, along
with all `--meter-queued` identity/cancel/overflow gates. Final affected Processor,
source-oracle and Linux checks follow for this small refinement.

The preceding `f3a5fa1` snapshot completed Linux `--meter-queued`, `--meter-stops`,
the production Processor suite, Standalone/LV2 builds and all six focused CTests
(262.18s). Windows five focused CTests passed (70.85s); its native Processor test
passed (32.88s), and `tools/TestMouseWorkflow.ps1 -SkipMidi` passed the actual
64/128 pointer/keyboard/PCM, queued playback/STOP, WAV export and MERGE workflows
using the synthetic test host (no audio device opened). These are not DAW tests.

## Final evidence and next step — code 27063ed

Final Windows Release: `aion_core_tests --meter-queued` passed after the rate
refinement; direct `StartPhaseParity.cjs ... --queued-late` passed the extended
60-start oracle above. Rebuilt `aion_midi_processor_tests`, `AionLooper_Standalone`
and `AionLooper_VST3`; `ctest --test-dir build/windows -C Release --output-on-failure --no-tests=error -R '^native_midi_processor$'`
passed 1/1 in 34.02s. The source-rate refinement does not alter running-source
smoothing or the preceding unchanged UI/bridge/asset code. Full Windows 64/128
pointer/export/MERGE workflow evidence is from admission snapshot f3a5fa1; it
was not redundantly rerun for the two-field fresh-rate initialization refinement.

Final native WSL: built `aion_core_tests` / `aion_start_probe`, then passed
`build/core/aion_core_tests --meter-queued` and
`ctest --test-dir build/core --output-on-failure --no-tests=error -R '^js_native_queued_late_phase$'`
(1/1, 69.14s; same 4.034987166612325e-8 max error). Rebuilt
`aion_midi_processor_tests AionLooper_Standalone AionLooper_LV2` in build/linux,
then ran `build/linux/aion_midi_processor_tests` directly: PASS, including actual
Processor late source silence/admission, autonomous timers, CLI, MIDI, state and
zero tracked audio-thread new/delete. Build logs retain the WSL EGL/DRI3 warning;
no hardware acceleration or physical audio-device validation is inferred.

Measured the linked Release core with a local ignored harness in
`build/queued-bench-src` (CMake targets under queued-bench-windows / queued-bench-linux).
One stereo loop, 48kHz, warm caches; 2048 steady callbacks per block size;
input generation/view consumption/timing storage outside the measured process call.
Late admission is a separate single observation, not a worst-case statistic.

| Release core | Block | Admission us | Mean us | p99 us | Observed max us | Block budget us |
| --- | --- | --- | --- | --- | --- | --- |
| Windows | 64 | 27.3 | 27.90 | 48.5 | 81.2 | 1333.33 |
| Windows | 128 | 53.9 | 59.18 | 90.9 | 130.5 | 2666.67 |
| Linux/WSL | 64 | 20.0 | 21.75 | 40.8 | 100.7 | 1333.33 |
| Linux/WSL | 128 | 58.8 | 46.18 | 89.2 | 125.5 | 2666.67 |

Local benchmark data: `build/queued-bench-windows.jsonl` and
`build/queued-bench-linux.jsonl`. The generated harness uses the existing core
library; no project dependency or permanent target was added. These are synthetic
main-thread processing observations, not device round-trip latency, maximum
track/FX load, host scheduling guarantees or a cross-OS speed comparison.

Reviewed final source/command/state diff; `git diff --check` passed. No reference
or preserved resource changed, no parameter/action/session format changed, and
no distributor/package/push was produced. No new REAPER/Ardour listening,
physical controller/device, sanitizer/full-suite, Apple or offline MIDI-render
acceptance is claimed. Next concrete slice: map independent old/new cleanup to
actual source lifetime/overlap sound, retaining retired-result/cancel/overflow
safety. Early graph-birth control edits, exact browser envelopes/resampling and
wall/audio alignment remain separate sound gates; keep visible meters disabled.
## Replacement queued source cleanup — 29 October 2026

Implementation snapshot: `82c3a53` (built/tested on both Windows and native WSL).
Baseline reused: the `27063ed` late-admission evidence above; no dependency,
framework, parameter, action, persistence or preserved UI/reference change.

Acceptance established before editing: a retained old `_teardown` must stop its
replacement source; cleanup must cover an already-issued pending callback and
an accepted future source, reject stale generation/timer identities, preserve
clip/history, and allow later fresh playback. No callback allocation or FIFO-loss
path is permitted. The original `Loop._teardown` calls `AudioGraph.cleanup`,
which immediately stops and disconnects the current source. The old native path
only detached message-side metadata, leaving replacement audio alive.

| Reference behavior | Native status | Evidence | Remaining gap |
| --- | --- | --- | --- |
| Cleanup stops/disconnects the replacement graph/source | Source cut at next callback, with no extra native STOP tail | Unchanged-JS cleanup oracle; exact zero post-cleanup PCM | Browser resampling/envelopes and wall/audio correlation |
| Old cleanup before a callback leaves that later callback alive | Higher source ticket survives prior retirement | Engine identity/order cases; actual JUCE timer fresh-play check | Broader retired-source overlap sound |
| Cleanup after callback issuance must not resurrect its source | Cumulative generation/ticket retirement also covers requests still in FIFO | Pending/future/raw-reset cases at tracks 0/19, 64/128 | Capture/edit graph origins remain separate |
| Clip and history survive teardown | Existing stopped state/clip/history retained | Exact length/undo preservation, fresh PLAY and prepare recovery | Full project/DAW release acceptance |

Implementation: one fixed lock-free 64-bit retirement mailbox per track, tagged
with prepare generation and a monotonic source ticket. Message owner publishes
without the ordinary command FIFO. Audio polls a fixed twenty entries per
callback and retains a per-generation ticket high-water mark, preventing a queued
request from reviving audio after cleanup. Raw meter resets cannot replace source
identity. Normal user STOP fades are unchanged; graph disconnect cuts its tail.
The default sample-grid component mode retains its historical meter-only contract;
production uses callback-controlled source lifetime. Existing GUI/CLI PLAY/STOP
routes share the fix; there is no new product command or second DSP engine.

Executed verification:
- Windows and Linux: `aion_core_tests --meter-queued` and `--meter-stops` passed.
  New cleanup cases cover two blocks, two tracks and seven orderings (28 cases),
  including a full ordinary command FIFO and old-generation mailbox recovery.
  Existing cancellation/overflow/retired-result checks are retained. Zero tracked
  callback new/delete. Prepare recovery explicitly drains the existing prepare
  stop tail at reset master gain before isolating the new source.
- Windows and Linux: actual `aion_midi_processor_tests` passed, including the new
  real JUCE retained-timer test with nonzero PCM, exact next-callback cutoff and
  later playback survival at 64/128. Existing production processor, external CLI,
  isolation, MIDI/state/lifecycle tests passed; no physical device IO is asserted.
- Windows: three source CTests passed in 71.97s; separate UI asset/frontend/bridge
  CTests passed in 5.77s. New cleanup oracle covers 60 starts (mono/stereo,
  forward/reverse, 8/48/96kHz, five lengths including 1 frame), including 8,820
  explicit post-cleanup silent frames. Maximum source PCM error was
  4.034987166612325e-8 against unchanged tolerance 1e-6; browser gain envelopes and
  resampling remain excluded. Reference files and preserved UI bytes are unchanged.
- Linux: six focused source/UI-contract CTests passed in 201.18s, including the
  new unchanged-JS cleanup oracle.
- Windows: rebuilt actual embedded-UI pointer/keyboard workflow passed with
  `tools/TestMouseWorkflow.ps1 -SkipMidi` at 64/128, including queued STOP,
  recording, Start/STUT, LIVE controls, exact stereo WAV export and MERGE/Undo/Redo.
  The synthetic host opens no audio/MIDI device; MIDI hardware was skipped.
- Active wrappers built: Windows Standalone/VST3 and Linux Standalone/LV2.

Reproduce from the existing configured trees (Windows uses the VS bundled CMake
where `cmake`/`ctest` are not on PATH; see DEVELOPMENT_WORKFLOW.md):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
build/windows-core/Release/aion_core_tests.exe --meter-queued
build/windows-core/Release/aion_core_tests.exe --meter-stops
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(js_native_queued_cleanup|js_native_queued_late_phase|js_native_queued_stop_phase|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle)$'
cmake --build --preset windows --target aion_midi_processor_tests AionLooper_Standalone AionLooper_VST3 --parallel 2
build/windows/Release/aion_midi_processor_tests.exe
cmake --build --preset windows --target aion_mouse_host --parallel 2
tools/TestMouseWorkflow.ps1 -SkipMidi
# Commit the coherent snapshot before sync; never reset a dirty mirror.
wsl -d Ubuntu --exec bash /mnt/d/Jorge/Documents/Codex/Aion-Looper/tools/SetupWslWorkspace.sh /mnt/d/Jorge/Documents/Codex/Aion-Looper
```

Inside the native `/home/jorge/projects/Aion-Looper` mirror:

```sh
cmake --build build/core --target aion_core_tests aion_start_probe --parallel 2
build/core/aion_core_tests --meter-queued
build/core/aion_core_tests --meter-stops
ctest --test-dir build/core --no-tests=error --output-on-failure -R '^(js_native_queued_cleanup|js_native_queued_late_phase|js_native_queued_stop_phase|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle)$'
cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone AionLooper_LV2 --parallel 2
build/linux/aion_midi_processor_tests
```

Warmed callback measurements reuse the previous generated single-stereo-loop,
48kHz Release harness, 512 pending warm blocks, one admission observation,
128 playing warm blocks and 2,048 measured blocks. Fixed input/view work remains
outside the measured `Engine::process` call.

| Release core | Block | Admission us | Mean us | p99 us | Observed max us | Block budget us |
| --- | --- | --- | --- | --- | --- | --- |
| Windows | 64 | 31.1 | 29.68 | 61.8 | 115.6 | 1333.33 |
| Windows | 128 | 55.3 | 58.54 | 92.7 | 134.5 | 2666.67 |
| Linux/WSL | 64 | 20.81 | 20.05 | 36.03 | 70.77 | 1333.33 |
| Linux/WSL | 128 | 39.88 | 39.07 | 63.8 | 104.23 | 2666.67 |

Data: local ignored `build/cleanup-bench-{windows,linux}.jsonl`; harness rebuilds
in `build/queued-bench-windows` / native `build/queued-bench-linux`. These small
synthetic main-thread observations are not worst-case scheduling guarantees,
maximum track/FX loads, cross-OS speed comparisons or round-trip device latency.

No new REAPER/Ardour listening, physical controller/device, sanitizer/full-suite,
Apple or offline MIDI-render acceptance is claimed. No distributable or push.
Next concrete slice: compare controls edited after an early source callback but
before its grid against the unchanged JS source; then remaining overlap/envelope
sound gates. Keep visible meters disabled until their separate acceptance.
