# Current migration checkpoint

Updated 2026-10-28. Current source baseline: `24df7e9` (clean main at session start,
twelve local commits ahead of cached origin). Current work verifies zero-delay
queued callbacks and cancellation behind a full message fact batch.
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
| STOP ALL cancels a still-queued intent even after its grid time | This repair routes queuedCancel by explicit pending ID; core, production processor, 20-route and real-timer regressions | This does not fix audible playback occurring before a late message callback |
| STOP NOW clears pending queue before immediate graph cleanup, including after grid | Explicit queue-ID routing; graphless/attached, production requests and real 25ms timer checks | Normal synchronized STOP and old-teardown state boundary remain separate |
| Normal STOP takes immediate path when sync is disabled | Pending-ID routing covers sync disabled after grid; accepted graph origins support immediate and synchronized stops before/after grid | Actual automatic callback delivery and full transition sound remain separate |
| Queued cancellation retains older analyser/data/history and cleanup | Existing owner/adapter policy retained, including original pre-grid test plus added post-grid frozen-clock test | If old cleanup already changed message state out of queued, existing rejection remains intentional pending full mapping |
| Audible grid does not prove message graph birth | Graph origin remains quarantined; no new graph, reset or 65ms cleanup inferred | Visible meters, remaining capture/edit graph origins, overlap and continuity remain unfinished |
| Pending intent requires its conditional callback transaction | Generic start rejects real queued IDs; explicit graph request/result now binds a fresh graph with rejection/loss/cleanup recovery | Default timer now requests due real-ID callbacks; full source lifetime remains unaccepted |
| Conditional raw reset reports its outcome without graph creation | Explicit request/result handshake, reserved tickets, typed outcomes and production routing; old graph/binding/cleanup retained | This is raw-window readiness only; graph acceptance, source timing and renderer remain separate |
| Explicit queued callback creates a graph independently of old cleanup | Unchanged-source/component comparison: 72 scenarios, 1,278 additional observations; fresh analyser/data, retained old deadlines, cleanup before/after callback, repeated immediate/scheduled/panic stops | This oracle supplies explicit notifications; the real-ID transaction is checked separately, and audio/wall scheduling remains unwired |
| Explicit graph callback owns subsequent stop routing | Real queue IDs, 120 core lifetime/stop combinations, prepared processor hook and twenty-track result/stop routing pass | Automatic service delivery is covered below; overlap/envelopes and source phase remain unaccepted |
| Due callback retains its original intent and fires once | Guarded local-owner delivery checks deadline and stable receipt, preserves cleanup ordering, rejects stale replacement and retries FIFO failure | Existing processor timer supplies autonomous delivery below; zero-delay/backlog interactions and full source sound remain separate |
| Full instruments/effects/controller/persistence/host parity | No new acceptance from this session; see PARITY.md and feature-specific validation notes | Do not infer suite completion from these looper/meter gates |

## Current slice: zero-delay and full-batch queued callbacks

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
sharing its existing session/devices and validated command handler, rather than
a second headless engine. All newly ported features must have CLI access. Product
CLI transport is not yet present; the existing console binaries are test probes.

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

Zero-delay and full-budget cancellation interaction checks now pass. Next add the
user-selected running-Standalone CLI, then compare late-source phase,
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
