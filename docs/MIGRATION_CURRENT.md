# Current migration checkpoint

## Current target policy — 30 October 2026

The updated owner prompt makes Windows and Linux **Standalone only** for routine
work. VST3/REAPER, LV2/Ardour and CLAP remain deferred/opt-in, preserving code,
IDs and historical tests. Current CMake defaults and desktop presets disable
those wrappers. Use explicit `-DAION_BUILD_VST3=ON`, `-DAION_BUILD_LV2=ON` or
`-DAION_BUILD_CLAP=ON` for a relevant wrapper task. Earlier format/build evidence
below is historical, not a requirement to rebuild wrappers on each engine fix.

## Active slice — queued Multiply before source construction, in verification

Continuing `9c4717b`. Acceptance: original queued Loop.multiply/multiplyInstant
exactly doubles stored PCM and adds one history entry without moving PLAY receipt,
callback deadline or grid; repeated doubling and Undo/Redo work through existing
GUI/keyboard/CLI commands. Source birth uses the latest committed length, direction
and effective rate. While the worker runs, one bounded conditional graph request
waits outside the command FIFO; no old-buffer birth, false acceptance or audio-thread
waiting. Completion revalidates receipt/generation; cancellation, cleanup, stream
loss, prepare, full-pool and OOM must not revive audio or corrupt PCM/history.

The 192-case unchanged-method reference/native source gate passes on Windows
(maximum PCM error 4.4128859011394184e-8 at unchanged 1e-6). All 108 focused native
lifecycle cases (including eight stream-loss cases), frontend eligibility and
production processor suite pass. Actual Windows UI passes at 64; 128 is running.
Linux and final regression/sanitizer checks remain pending; do not claim final
acceptance yet.
Baseline rejects queued MULT (`build/queued-multiply-negative-windows.log`).
Preserved resources/reference, public IDs/schema/capacity/dependencies/logo remain
unchanged. After-birth queued MULT/capture remains separate and disabled.

## Latest verified slice — queued history after source creation, 5 November 2026

Implementation `e1e0f5c`, original-method characterization `5bc745e`, continuing
`dc54833`. Shared GUI/keyboard/running-Standalone CLI Undo/Redo now accepts a
queued loop after source construction. Playing history also uses the original
5ms source replacement, correcting the previous immediate restart. Storage and
history change immediately; each existing source retains its own PCM, length,
rate and prior stop. The original grid continues to gate audible output even
when a replacement runs earlier. Stopped history does not start playback;
pending-stop queued history remains unavailable. Existing public IDs/schema,
parameters, capacity, dependency pins, ASCII logo and preserved assets are unchanged.

Forward-source/event leases complement existing reverse snapshots. Each source
renders its captured length. Private logical buffer versions distinguish original
JS history clones even when native immutable history shares physical PCM; NORM
and in-place MERGE redirect only matching forward identities/versions. History
reserves bounded event/overlap capacity before mutation; rejection preserves PCM,
history and sources. Releases use the existing worker retirement path, including
its retention-until-prepare fallback on retirement-queue overflow. No new callback
allocation, deletion, locks or I/O. Final diff review covered these ownership,
reservation, cancellation and event-ordering paths.

| Acceptance | Executed evidence / limit |
| --- | --- |
| Original source timeline, lengths and 5ms deadlines | 960 unchanged-JS/native cases on **each OS**: mono/stereo, 8/48/96kHz, lengths 1/17/512/4097, forward/reverse, early/near-grid, Apply chains, RTRG, NORM, repeated history and teardown |
| Source PCM and original grid gate | 2,784 sources; 19,680 observations; 23,625,040 predicted frames, including 20,565,520 pre-grid silent frames, 214,016 overlap samples and 2,726,544 distinct-buffer samples. Maximum absolute error **8.313785782032568e-8** on each OS; unchanged **1e-6** tolerance |
| Native timing/ownership/lifecycle | 124 post-birth cases at tracks 0/19, forward/reverse, 64/128; exact paired stereo, 5ms/grid silence, analyser/data retention, rapid history/NORM, stop/panic/clear/same/new-rate prepare and full event/overlap atomic rejection |
| Existing regressions | Both OS: 64 before-birth history cases, `--loop-edits` and full `--meter-queued`, including existing NORM, queue/FIFO, overlap, cancellation and OOM coverage |
| Production GUI/CLI path | Full processor suite on both OS, including 16 local/global before/after-birth early/late RPC cases at 64/128, existing external Python CLI routing, state/MIDI/timer checks and tracked zero processBlock new/delete. New timing cases use the shared production RPC, not a separate external-process source oracle |
| Actual original controls | Eight Windows WebView2 forward/reverse before/after-birth cases across 64/128; trusted MULT/UNDO/REDO pointers, acknowledgement, restored lengths, original grid, silence and nonzero playback. Before-birth uses automatic callback; after-birth uses the owned test-host early-source hook |
| Sanitizers | Native WSL Debug ASan/UBSan with leak detection: 124 post-birth +64 history +96 pre-birth NORM +108 queued NORM +80 pending NORM =472 cases, plus full `--loop-edits`; no diagnostics |
| Builds and selected CTests | Windows/Linux Release Standalone, core/probes, processor; Windows mouse host. **28/28 PASS** per OS, parallel 2: Windows 1040.91s total (960-case source gate 596.25s), Linux 1741.26s (source gate 1105.37s). Concurrent verification workload timings, not comparative build/runtime benchmarks |
| Still unverified | Browser renderer/envelope/resampling sound, listening, physical audio/MIDI devices, Linux WebKit interaction and long sessions. Deferred wrappers/DAWs and Apple were not built/tested; this is scoped development acceptance, not release readiness |

The first native corpus (768 cases) passed, but an added rapid Undo/Redo/Undo/NORM
near-grid case exposed pointer-only aliasing (0.1747151 PCM error). Logical buffer
versions repair it; the final complete 960-case corpus above includes both new
rapid-history/NORM and Redo/NORM groups. Baseline `dc54833` rejection and incorrect
zero-deadline negatives, plus the alias failure, remain in
`build/queued-history-sources-*-negative*.log`. No cases were deleted or tolerances
loosened. Four existing value Engine test fixtures moved to construction-time
heap storage after metadata growth exhausted the Windows 1MiB test stack; all
assertions and callback allocation guards remain, with no stack-limit increase.
The final rebuilt native suite reports 124, superseding interim 92-case output.

Callback observations ran after verification jobs stopped, sequentially Windows
then Linux: 48kHz stereo, one 512-frame loop doubled while stopped, 512 warm
callbacks, explicit source birth, Undo/Redo/Undo, playback through the original
grid and clear. Only Engine.process is timed; setup, worker preparation and
telemetry drain are outside measurement.

| Platform | Maximum measured history-command callback | Maximum any measured callback, 64 /128 samples |
| --- | ---: | ---: |
| Windows Release | 76.1us | 85.6us /131.8us |
| Native WSL Linux Release | 67.075us | 126.038us /146.525us |

Block deadlines are 1333.33us /2666.67us. These observations do not establish
worst-case/full-capacity/long-session continuity, command latency, driver/round-trip
latency or OS rankings. Ignored harness: `build/postbirth-history-bench-src`;
executed binaries: `build/postbirth-history-bench-windows/Release/queued_bench.exe`
and `build/postbirth-history-bench-linux/queued_bench`; raw per-OS JSON:
`build/postbirth-history-bench-{windows,linux}.jsonl`.

Reproduce with existing caches; resolve Windows VS CMake/CTest as documented below.
Do not run another CTest in the same cache until its LastTest.log is preserved.

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe --parallel 2
foreach ($selection in '--postbirth-history','--queued-history','--loop-edits','--meter-queued') { & ./build/windows-core/Release/aion_core_tests.exe $selection; if ($LASTEXITCODE) { throw $selection } }
$historyGate = '^(original_ui_bytes|native_frontend_commands|original_meter_source_lifecycle|original_queued_history_sources|js_native_(history|queued_history|queued_history_sources|merge|multiply|sync|stutter|meter_graph|normalization|reverse|start_phase|queued_(late_phase|early_edits|source_norm|source_norm_phase|prebirth_norm|prebirth_late_norm|apply_chain_overlap|apply_chain_prefix|apply_triple_overlap|apply_triple_prefix)|apply_norm_.*))$'
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R $historyGate
cmake --build build/windows --config Release --target AionLooper_Standalone aion_midi_processor_tests aion_mouse_host --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -QueuedHistoryOnly
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
for selection in --postbirth-history --queued-history --loop-edits --meter-queued; do ./build/linux/aion_core_tests "$selection" || exit; done
./build/linux/aion_midi_processor_tests
history_gate='^(original_ui_bytes|native_frontend_commands|original_meter_source_lifecycle|original_queued_history_sources|js_native_(history|queued_history|queued_history_sources|merge|multiply|sync|stutter|meter_graph|normalization|reverse|start_phase|queued_(late_phase|early_edits|source_norm|source_norm_phase|prebirth_norm|prebirth_late_norm|apply_chain_overlap|apply_chain_prefix|apply_triple_overlap|apply_triple_prefix)|apply_norm_.*))$'
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R "$history_gate"
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
for selection in --postbirth-history --queued-history --prebirth-norm --queued-norm --norm-pending --loop-edits; do env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests "$selection" || exit; done
```

Saved per-OS evidence: `build/queued-history-sources--{postbirth-history,queued-history,loop-edits,meter-queued}-{windows,linux}.log`,
`build/queued-history-sources-production-{windows,linux}.log`,
`build/queued-history-sources-ctest-final-{windows,linux}.log` and raw oracle
`build/queued-history-sources-source-results-{windows,linux}.log`.
Windows UI: `build/queued-history-sources-ui-windows.log`;
Linux sanitizer: `build/queued-history-sources-asan-linux.log`.
No package or push; Windows remains the publishing owner and WSL receives committed snapshots.

Next smallest slice: characterize **queued Multiply before source construction**.
Original Loop.multiply falls back to multiplyInstant while queued, whereas the
native action/UI currently rejects it. Establish buffer doubling/history,
original receipt/grid, callback during worker completion and cancellation/OOM
acceptance first; preserve the existing after-birth source contract. Queued
Multiply after source construction/capture is a separate follow-up, not accepted here.

## Previous verified slice — queued Undo/Redo before source creation, 5 November 2026

Implementation `28e4b1a` plus final unchanged-retrigger-guard correction `7f7a0c2`,
continuing baseline `15af02a`. Shared GUI/keyboard/CLI local and global Undo/Redo
now accept a queued loop before its independent source callback has constructed
playback. Restoring changes stored PCM, length and history depth, retaining the
original immutable PLAY receipt, callback time and grid. Even a passed grid stays
silent until callback delivery; birth takes the restored buffer/current direction.

The existing O(1) immutable history swap publishes an explicit pending-queue
continuation through the existing buffer receipt path. It neither allocates nor
creates/resets an analyser or source. Capacity rejection precedes all mutation.
Normal buffer notices cannot inherit queue authority. No new storage, public
IDs, schema, parameters, dependencies, preserved UI/reference assets or logo.
Queued history after source construction remains rejected/disabled for a separate
slice; playing/stopped history retains its existing behavior. GUI additionally
keeps scheduled-stop queues disabled. The unrelated RTRG guard remains unchanged.

| Acceptance | Executed evidence / limit |
| --- | --- |
| Original queued Undo/Redo, repeated shrinking/growing buffers | 96 unchanged-method/source cases: mono/stereo, 8/48/96kHz, lengths 1/17/512/4097, forward/reverse, early/late callbacks |
| Original PCM/grid/silence | 4,241,824 compared channel samples and 2,112,488 silent frames; maximum error 6.3772108371829e-8 on each OS; unchanged 1e-6 tolerance |
| Local/global history and ownership | 64 native Engine cases, tracks 0/19, 64/128, exact paired stereo, full-width continuation, repeated restore and original complete callback-time identity |
| Cancellation/capacity/FIFO | STOP NOW, panic, clear, same/new-rate prepare, atomic growing-restore rejection, saturated FIFO retry, callback requested before message consumes installed PCM |
| Existing edits and NORM | Existing native `--loop-edits` and 96-case `--prebirth-norm` suites on both OS; unchanged JS history/NORM/phase checks |
| Shared production commands | Eight new local/global early/late production RPC cases at 64/128; full processor suite including external Python CLI routing, state, MIDI and tracked callback allocations on both OS |
| Actual original UI | Four Windows forward/reverse cases at 64/128: trusted MULT/UNDO/REDO pointers, native acknowledgement, exact restored lengths, unchanged grid, silence, automatic original callback and audible PCM |
| Sanitizers | Linux ASan/UBSan with leak detection: all 64 new queued-history cases PASS |
| Outstanding release coverage | Linux WebKit interaction, physical audio/MIDI devices, listening, browser envelopes/resampling and long sessions remain unverified; no wrapper/DAW/Apple builds or package |

The initial unchanged-source test failed at native `undo` before implementation
(`build/queued-history-negative.log`). No existing case was deleted or tolerance
loosened. New focused CTest: `js_native_queued_history`; direct native selector:
`--queued-history` (also included in `--meter-queued`). Existing receipt-identity,
NORM cancellation/OOM and source-boundary checks remain intact. New feature CLI
commands use the same existing native actions; see [CLI.md](CLI.md#queued-history).
The external CLI transport suite passed; the new queued cases exercise its shared
production RPC path rather than a separate external-process queued timing oracle.

| Executed check | Windows Release | Native WSL Linux Release |
| --- | --- | --- |
| Standalone, core/probes, processor and Windows mouse host build | PASS | PASS (mouse host Windows-only) |
| `--queued-history`, `--loop-edits`, `--prebirth-norm` | PASS | PASS |
| Full production processor suite | PASS | PASS |
| 12 selected reference/frontend/asset CTests | PASS, 179.70s, parallel 2 | PASS, 288.30s, parallel 2 |
| Actual focused WebView pointer gate | PASS, final UI rerun at 64/128 | Not run |
| Focused ASan/UBSan/leaks | Not run | PASS, existing Debug sanitizer cache |

Callback-cost observations after builds/acceptance jobs stopped: same ignored
harness, Windows then Linux, 48kHz stereo, one 512-frame loop doubled while stopped,
512 warm playing callbacks, queued Undo/Redo/Undo then birth/playback/clear. Timing
covers Engine.process only; setup/worker preparation and telemetry drain excluded.

| Platform | Maximum measured history-command callback | Maximum any measured callback, 64 /128 samples |
| --- | ---: | ---: |
| Windows | 85.8us | 86.7us /150.1us |
| Linux | 48.552us | 123.837us /109.98us |

Deadlines are 1333.33us /2666.67us. These scoped samples do not establish worst-case
continuity, edit/round-trip latency, full-capacity/long-session CPU or OS rankings.
Ignored harness/raw results: `build/queued-history-bench-src`,
`build/queued-history-bench-{windows,linux}.jsonl` on the corresponding OS.

Reproduce with existing caches (Windows VS CMake/CTest resolution retained below):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --queued-history
./build/windows-core/Release/aion_core_tests.exe --loop-edits
./build/windows-core/Release/aion_core_tests.exe --prebirth-norm
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|native_frontend_commands|original_meter_source_lifecycle|js_native_(history|queued_history|normalization|reverse|start_phase|queued_(late_phase|early_edits|prebirth_norm|prebirth_late_norm)))$'
cmake --build build/windows --config Release --target AionLooper_Standalone aion_midi_processor_tests aion_mouse_host --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -QueuedHistoryOnly
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --queued-history
./build/linux/aion_core_tests --loop-edits
./build/linux/aion_core_tests --prebirth-norm
./build/linux/aion_midi_processor_tests
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|native_frontend_commands|original_meter_source_lifecycle|js_native_(history|queued_history|normalization|reverse|start_phase|queued_(late_phase|early_edits|prebirth_norm|prebirth_late_norm)))$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --queued-history
```

Logs per OS: `build/queued-history-{core,edits,norm,production,ctest}-{windows,linux}.log`;
Windows `build/queued-history-ui-windows.log`; Linux `build/queued-history-asan-linux.log`.
Raw oracle output preserved in `build/queued-history-source-results-{windows,linux}.log`.
No package or push. Windows remains publishing owner; WSL receives committed snapshots.

Historical next step (completed by the slice above): characterize queued Undo/Redo after source construction against unchanged
Loop.undo/redo/restart and AudioGraph, including different buffer lengths, original
future grid, forward/reverse source snapshots, pending Apply/RTRG and stop/cleanup.
Define replacement timing/PCM/ownership acceptance before changing eligibility;
do not simply remove the current queued-source guard.

## Previous verified slice — pre-birth queued NORM, 5 November 2026

Implementation `30db3bd`, continuing verified baseline `a19064c`. NORM now works
before and after queued graph creation through the existing shared GUI/CLI
`loop normalize TRACK` command. It preserves the play receipt, original callback
deadline and grid; successful PCM installation alone never creates a source.

The audio owner keeps an immutable queued PLAY buffer ID separately from the
current PCM waveform ID. A successful NORM `bufferInstalled` fact explicitly
continues that pending queue via its queue ID. The message owner retains the
original clock/receipt and pending transaction; normal buffer changes and bare
booleans cannot inherit that authority. Callback requests/results continue using
the original receipt, while waveform/export/stop operations use current PCM IDs.
No event/action IDs, persistence schema, capacities, dependencies, original UI/
reference assets or logo changed. Added storage is a fixed 20-entry uint64 array
and a message-owned flag; all NORM buffer processing/allocation stays on the worker.

| Acceptance | Evidence / remaining limit |
| --- | --- |
| Original committed NORM before early/late graph creation | 120 unchanged-JS cases, both directions, mono/stereo, 8/48/96kHz, lengths 1/17/512/4097/20003 |
| Queue/source/clock ownership | 96 new Engine cases at tracks 0/19, 64/128; exact original deadline, no graph from NORM or grid expiry |
| Existing queued/playing NORM and source lifecycle | Full selected core queue suite and 18 reference/frontend/asset checks on each OS |
| Shared GUI/CLI and actual interaction | Production processor/external CLI suites on both OS; eight trusted Windows WebView cases before/after graph birth at 64/128 |
| Memory/RT ownership | Linux ASan/UBSan/leak checks for three NORM selections; zero tracked callback new/delete in scoped core/processor tests |
| Full browser sound/device/release acceptance | Pending: renderer envelopes/resampling, Linux WebKit interaction, listening, hardware and long sessions |

Existing asynchronous commit semantics remain. A source constructed while the
worker is pending captures the PCM committed at birth. A forward source receives
later normalized storage; a reversed source retains its independent snapshot.
Reference gates commit NORM before source creation without advancing sample time.
Delayed-worker tests protect source/grid/ownership behavior; synchronous JS
button-to-sound latency equivalence is not established.

The new baseline oracle first rejected NORM (`build/prebirth-norm-negative.log`).
Early/late modes compare 1,243,592 /1,313,512 frames, with respective maximum errors
8.283281738297177e-8 /7.022869774608864e-8 on both OS, unchanged 1e-6 tolerance.
The late mode also retains tempo, speed, sync-off and Start metadata changes.
Native 96-case coverage: 32 commit/request/worker/late-callback orderings, 40
stop/panic/clear/same/new-rate prepare cases, eight exact OOM rollback cases,
four FIFO saturation/retry/stale-timer cases, four bare-boolean authority guards,
and eight old-analyser/data/cleanup/fresh-callback cases. All preserve history.
The former four pre-birth rejection assertions are retained as stronger acceptance,
original-grid, graphless-state and silence assertions; no case/tolerance was removed.

| Executed checks | Windows | Native WSL Linux |
| --- | --- | --- |
| Standalone, current start/reference/meter-graph probes and processor builds | PASS | PASS |
| 18 selected CTests | PASS, 259.79s, parallel 2 | PASS, 385.95s, parallel 2 |
| Full `--meter-queued` Release suite | PASS | PASS |
| Production GUI/CLI processor/timer/state suite | PASS | PASS |
| Focused NORM ASan/UBSan/leaks | Not run | PASS: `--prebirth-norm --queued-norm --norm-pending` |
| Actual focused WebView pointer test | PASS, 64/128 | Not run |

The broad sanitizer `--meter-queued` run was stopped unfinished during unchanged
speed/overlap matrices; no full-suite sanitizer pass is claimed. The focused
selections retain the new 96, existing 108 queued and 80 playing NORM cases,
including cancellation, no-op, OOM and retained-source exhaustion safety checks.
No implementation failure was bypassed or regression deleted. Historical
unchanged acceptance remains in the previous checkpoints; wrappers were not built.

Callback-cost observations after acceptance jobs stopped: same ignored harness
on Windows then Linux, 48kHz stereo, one 512-frame loop, forward/reverse, 512 warm
playing callbacks. Timed Engine.process for NORM admission/commit, subsequent
birth using the original receipt, queued-to-playing playback and clear:

| Platform | 64 samples /1333.33us deadline | 128 samples /2666.67us deadline |
| --- | ---: | ---: |
| Maximum observed Windows callback | 117.3us | 230.4us |
| Maximum observed Linux callback | 113.758us | 115.458us |

Worker sleeps/preparation are outside timing with sample time frozen; all four
cases/platform installed the result in one commit-attempt callback. These are
scoped callback-cost samples, not edit/round-trip latency, worst-case continuity,
full-capacity/long-session performance or OS rankings. Local ignored harness/raw
output: `build/prebirth-norm-bench-src`, `build/prebirth-norm-bench-{windows,linux}.jsonl`.

Reproduce from existing caches (installed CMake/CTest; VS tools on Windows):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --meter-queued
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|native_frontend_commands|original_meter_source_lifecycle|js_native_(meter_graph|normalization|reverse|start_phase|queued_(late_phase|stop_phase|early_edits|apply_chain_overlap|source_norm|source_norm_phase|prebirth_norm|prebirth_late_norm)|apply_norm_.*))$'
cmake --build build/windows --config Release --target AionLooper_Standalone aion_midi_processor_tests aion_mouse_host --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -QueuedNormOnly
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --meter-queued
./build/linux/aion_midi_processor_tests
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|native_frontend_commands|original_meter_source_lifecycle|js_native_(meter_graph|normalization|reverse|start_phase|queued_(late_phase|stop_phase|early_edits|apply_chain_overlap|source_norm|source_norm_phase|prebirth_norm|prebirth_late_norm)|apply_norm_.*))$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
for selection in --prebirth-norm --queued-norm --norm-pending; do
  env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests "$selection" || exit "$?"
done
```

Windows executable discovery is unchanged: see the VS `vswhere` CMake/CTest
resolution immediately below. Fresh probes were built before each reference run.
Logs: Windows `build/prebirth-norm-{core,queued-windows,production-windows,ctest-windows,ui-windows}.log`;
native WSL `build/prebirth-norm-{queued-linux,production-linux,ctest-linux,asan-focused}.log`.
Both copies retain `build/prebirth-norm-source-results-{windows,linux}.log` with
raw reference output. Exact source and commands above reuse the existing caches;
there was no dependency upgrade, package or push.

Next at that checkpoint (now completed above): queued Undo/Redo before graph creation. The unchanged Loop.undo/redo methods
replace stored buffers and leave queued play pending, while native restore still
requires playing/stopped. Establish its length/history/source expectations before
adding native eligibility; do not automatically apply NORM's continuation policy
to unrelated buffer replacement. After-graph history edits are a separate slice.

## Previous verified slice — constructed queued-source NORM, 5 November 2026

Implementation: `c79025b`; strengthened fixtures: `43ce195`.
NORM now accepts a queued loop after its independent graph callback has
constructed the future source. GUI and running-Standalone CLI share the existing
`loop normalize TRACK` command. The original grid, source identity and accepted
Apply/RTRG deadlines survive worker finalization. Forward sources receive the
normalized stored PCM; reversed sources retain the PCM snapshot leased at graph
construction. Grid/phase activation continues while the worker is pending.
Clear, stop and prepare release the leases without resurrecting playback.

| Parity item | Status / evidence |
| --- | --- |
| Constructed queued-source NORM | Verified for this slice; reference PCM, worker-delay and actual production command/UI checks |
| Pending Apply/RTRG and original grid silence | Preserved, including worker completion after activation |
| No-op, OOM, stop/clear/prepare and history | 108 focused Engine cases, tracks 0/19, forward/reverse, 64/128; no callback new/delete |
| NORM before graph creation | Still rejected/disabled; next slice needs explicit buffer/queue-receipt handling |
| Full renderer, listening, hardware and release acceptance | Pending; no new certification |

No parameter/action IDs, schema, capacity, dependencies, logo or preserved
`resources/ui` / reference assets changed. Only the native NORM readiness guard
changes presentation logic. The transient queue-finalization flag is audio owned;
storage/queues stay bounded and NORM buffer processing/allocation remain on the
existing worker.

| Executed verification | Windows Release | Native WSL Linux |
| --- | --- | --- |
| Standalone, current probes and processor builds | PASS | PASS |
| 26 selected reference/frontend/asset checks | PASS | PASS, 10 retained +16 resumed after chat interruption |
| Current normalization/reverse probe checks | PASS, rebuilt and rerun 2/2 | PASS, rebuilt and rerun 2/2 |
| Queued core/lifecycle and shared GUI/CLI processor tests | PASS | PASS |
| Strengthened `--queued-norm` 108 cases | PASS | PASS |
| Focused ASan/UBSan/leak checks | Not run | PASS |
| Actual WebView trusted NORM pointer, forward/reverse, 64/128 | PASS | Not run |

The baseline first rejected NORM (`build/queued-norm-negative.log`).
Each new source mode now covers 60 fresh-buffer cases at mono/stereo, 8/48/96kHz,
lengths 1/17/512/4097/20003 and both directions: plain queued NORM and two future
Applies followed by overtaking RTRG. Compared frames: 1,243,592 /1,246,832.
Windows maximum errors: 6.498684168576574e-8 /6.022638282532355e-8, unchanged
1e-6 tolerance. Linux has the same maxima. Browser envelopes/resampling are excluded.
Fresh buffers prevent the reverse case accidentally becoming a no-op after
forward NORM. The delayed-worker cases explicitly assert pre-grid silence.

Engine coverage: 48 changed/no-op/Apply/RTRG worker-delay cases, 48 immediate/
scheduled stop, panic, clear and same/new-rate prepare cases, eight exact OOM
rollback comparisons and four explicit pre-birth rejection cases. The 128-sample
fixture now accounts for RTRG firing inside the NORM admission block, retaining
its exact deadline. Existing playing-NORM/full-pool/overlap/cancellation guards
pass. Linux sanitizer selectors were `--queued-norm --norm-pending --meter-overlap`;
the strengthened queued selector was rebuilt/rerun after the test-only follow-up.
Production processor checks include external CLI, native outcomes, state and
lifecycle guards; the new queued commands use controlled offline commits.

Actual Windows UI checks use `tools/TestMouseWorkflow.ps1 -QueuedNormOnly`.
This is the focused NORM path, not the whole mouse/export/MIDI suite. An initial
sandbox renderer timeout occurred before feature execution; the verified owned
fixture was closed/relaunched outside the sandbox. The focused fixture's skipped
master-gain setup and reverse-checkbox selector were corrected before both block
sizes passed. No production behavior or tolerance was weakened. Review also
caught a stale historical reference executable: both OS probes were rebuilt and
the dependent normalization/reverse checks rerun. Retain those refreshed results.

Callback-cost observations, 48kHz stereo, one 512-frame loop, forward/reverse,
64/128, 512 warm playing blocks. Timed only Engine.process during early source
birth, NORM admission/commit, queued-to-playing playback and clear:

| Platform | 64 samples /1333.33us deadline | 128 samples /2666.67us deadline |
| --- | ---: | ---: |
| Windows maximum observed callback | 97.8us | 131.7us |
| Linux maximum observed callback | 193.236us | 130.227us |

Acceptance jobs had stopped before sequential Windows/Linux measurements.
Worker preparation sleeps are outside timing with sample time frozen; one
Windows 128 reverse case needed two commit attempts. This measures callback
cost, not edit latency, physical round-trip latency, worst-case continuity,
20-track/full-pool/long-session behavior or an OS comparison. Ignored harness:
`build/queued-norm-bench-src`; raw JSONL: `build/queued-norm-bench-{windows,linux}.jsonl`.

Commands (existing caches; Windows uses installed VS CMake/CTest):

```powershell
$vsRoot=& 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe' -latest -products '*' -property installationPath
$cmake=Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
& $cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --meter-queued
& $ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|native_frontend_commands|js_native_(normalization|reverse|start_phase|queued_.*|apply_norm_.*))$'
& $cmake --build build/windows --config Release --target AionLooper_Standalone aion_midi_processor_tests aion_mouse_host --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -QueuedNormOnly
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --meter-queued
./build/linux/aion_midi_processor_tests
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|native_frontend_commands|js_native_(normalization|reverse|start_phase|queued_.*|apply_norm_.*))$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --queued-norm
```

Logs: Windows `build/queued-norm-{core,queued-regression,playing-regression,production-windows,ctest-windows,reference-refresh-windows,ui-windows}.log`;
native WSL `build/queued-norm-{core-linux,production-linux,followup-core,ctest-linux,ctest-linux-resume,reference-refresh-linux,asan,asan-followup}.log`.
The interrupted run's ten passing names are retained in
`build/queued-norm-completed-linux.txt`; its unfinished cases were selected
with `ctest -E` rather than repeating those passes. Windows 26-test invocation:
518.18s; refreshed probe pair: 12.16s. Linux resumed 16: 473.83s; refreshed probe
pair: 18.47s. The interrupted Linux invocation has no completed-run total.

Next: NORM before graph creation. Preserve the queued intent and original
command-origin deadline across successful PCM installation, including an already
in-flight graph transaction. Do not create a graph merely because NORM ran or
silently broaden the guard without receipt/PCM/cancellation evidence. Other
buffer edits, Linux WebKit interaction, renderer envelopes/resampling, listening/
devices, long sessions and release checks remain open. No deferred wrappers,
package or push.

## Previous verified slice — 4 November 2026

Playing-loop NORM now preserves pending Apply/RTRG deadlines and the original
forward-buffer versus reversed-snapshot behavior. Implementation `09eac61`.
Windows/Linux all 15 selected source/asset CTests PASS, including 180 new NORM
sequences at unchanged tolerances. The 80 new Engine worker-delay/no-op/cancel/
OOM/full-pool cases, previous edit/overlap suites, shared GUI/CLI processor tests
and focused Linux ASan/UBSan/leak checks PASS at the documented 64/128 scope.
Both Standalone builds PASS. Full-pool callback timing observations remain within
the tested budgets, without worst-case/physical-latency claims. UI assets, IDs,
persistence schema, capacity and pinned dependencies are unchanged. No package
or push created; deferred wrappers were not built.
Next: NORM on still-queued native tracks (currently rejected), preserving queue/
graph/source identity and deadlines. Other live buffer edits, renderer envelopes/
resampling, listening/devices and release acceptance remain open. See final
acceptance evidence below.

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

## Early source Start/Reverse edits — 29 October 2026

Implementation `1542e01`; baseline `502bf68` / cleanup code `82c3a53` above.
The sibling reference remains absent; use unchanged `.reference/Ascii-Looper`
and the preserved `resources/ui` assets, plus the independent native WSL mirror.

Acceptance chosen before editing: Start-slider metadata/Reverse edits after a valid early source
callback must preserve that source's scheduled phase/direction and pre-grid
silence, update visible metadata, and apply on the next play. Controls changed
between queue intent and actual source birth must belong to that source.
Following-grid STOP and existing cancellation/cleanup/overflow guards must hold.

Reference: `UIManager.setLoopStartDelay` edits metadata only; Reverse explicitly
applies on next play. `AudioGraph.build` selects buffer/startOffset at source
creation. Native code previously selected them again at the audible grid.
The new oracle failed on the unchanged baseline (mono, 8kHz, 17 frames: native
0.0207093153 versus reference 0.0455965891; tolerance 1e-6).

| Behavior | Native result | Evidence | Still pending |
| --- | --- | --- | --- |
| Start/Reverse edits after early birth | Existing source retains captured phase/direction | Unchanged-JS source oracle and independent paired Engine PCM | Speed/tempo edits in this interval |
| Metadata changes before birth and on later play | Current controls belong to the newly created source | 48 Engine cases; 120 reference-comparison starts including replay | Capture/edit graph origins |
| Future start and following-grid STOP | Silent before grid; exact normal/stopping source PCM | Both blocks, tracks 0/19, both directions, separate/combined edits | Broader overlap and browser envelopes/resampling |
| Cleanup/cancel/prepare/overflow | Existing safety tests retained and passing | Focused Engine and actual JUCE processor/timer suites | DAW/hardware/integration release gates |

Only two transient values per track are added: prepared source phase/direction.
The callback captures them; the grid consumes them only for an admitted queued
source in production callback mode. Legacy sample-grid component mode and fresh
immediate/late sources retain their existing paths. Stop/clear/prepare invalidate
source ownership through the existing ticket/generation rules. No new allocation,
queue, parameter/action ID, saved-state field, dependency or product command.
Existing GUI/CLI Start/Reverse commands share the same DSP implementation.
This result applies to metadata edits, not explicit Apply/Q/nudge/RTRG restarts.
Those actions need a separate early-interval comparison: the native queued-state
restart guard differs from the reference, which is already marked playing after
graph birth. Do not treat the new metadata tests as their acceptance.

Executed verification:
- Windows and Linux `aion_core_tests --meter-queued` passed: 48 new paired stereo
  cases, plus unchanged late-source, retained-cleanup, cancellation, full FIFO,
  raw-reset, stale-result and prepare guards; zero tracked callback new/delete.
- Windows four source CTests passed in 111.46s. New oracle: 30 buffer cases,
  120 starts (mono/stereo, forward/reverse, 8/48/96kHz, lengths 1/17/512/4097/20003),
  2,572,556 compared frames, 2,561,396 silent future-source frames; maximum source
  error 5.669010411102349e-8 at unchanged tolerance 1e-6. IO-node stubs exclude
  browser envelopes/resampling; unchanged reference methods choose source/offset.
- Linux five focused CTests passed in 326.39s, including all four source
  comparisons and exact original UI bytes.
- Actual Windows and Linux `aion_midi_processor_tests` passed, including real JUCE
  timers, production processors, external CLI/isolation, MIDI/state and zero
  tracked processBlock new/delete at 64/128. Active Windows Standalone/VST3 and
  Linux Standalone/LV2 rebuilt successfully.
- No frontend/bridge code changed. Previous 82c3a53 pointer/keyboard/PCM evidence
  remains the UI baseline; no new actual pointer, DAW listening or hardware test
  is claimed for this binary. No sanitizer/full-suite/Apple/offline MIDI-render
  acceptance, distributor or push. Existing compiler warnings remain separate.

Reproduce in the existing configured Windows tree (use the VS bundled CMake if
not on PATH, as documented in DEVELOPMENT_WORKFLOW.md):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
build/windows-core/Release/aion_core_tests.exe --meter-queued
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(js_native_queued_early_edits|js_native_queued_cleanup|js_native_queued_late_phase|js_native_queued_stop_phase)$'
cmake --build --preset windows --target aion_midi_processor_tests AionLooper_Standalone AionLooper_VST3 --parallel 2
build/windows/Release/aion_midi_processor_tests.exe
# Commit a coherent snapshot before using the non-destructive WSL sync helper.
wsl -d Ubuntu --exec bash /mnt/d/Jorge/Documents/Codex/Aion-Looper/tools/SetupWslWorkspace.sh /mnt/d/Jorge/Documents/Codex/Aion-Looper
```

In `/home/jorge/projects/Aion-Looper`:

```sh
cmake --build build/core --target aion_core_tests aion_start_probe --parallel 2
build/core/aion_core_tests --meter-queued
ctest --test-dir build/core --no-tests=error --output-on-failure -R '^(js_native_queued_early_edits|js_native_queued_cleanup|js_native_queued_late_phase|js_native_queued_stop_phase|original_ui_bytes)$'
cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone AionLooper_LV2 --parallel 2
build/linux/aion_midi_processor_tests
```

The ignored `build/early-bench-src` harness imports the existing Release core:
512 idle warm blocks, a single early admission observation, Start/Reverse edits,
a single grid-start callback observation, then 128 playing warm blocks and 2,048
measured callbacks. One stereo 512-frame loop at 48kHz; input/view/timing storage
outside `Engine::process`. No new permanent build target/dependency.

| Release core | Block | Admission us | Grid start us | Mean us | p99 us | Max observed us | Budget us |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Windows | 64 | 30.2 | 30.4 | 30.93 | 47.6 | 80.8 | 1333.33 |
| Windows | 128 | 74.3 | 86.0 | 60.93 | 79.4 | 109.6 | 2666.67 |
| Linux/WSL | 64 | 19.07 | 20.08 | 19.95 | 35.48 | 85.8 | 1333.33 |
| Linux/WSL | 128 | 66.46 | 36.12 | 39.29 | 65.36 | 114.31 | 2666.67 |

Data: local ignored `build/early-bench-{windows,linux}.jsonl`. Rebuild/re-run the
corresponding `build/early-bench-windows` (Release config) or native
`build/early-bench-linux` harness; executable is `queued_bench`. These are small
synthetic main-thread observations, not maximum-load/host scheduling guarantees,
round-trip latency, or a cross-OS speed comparison. Do not compare their warm-up
setup directly with the older late-admission harness.

Next concrete slice: compare explicit Apply/Q/nudge after early graph birth but
before its grid, including the reference restart before the original future
source time. Then compare speed/tempo/clock-source edits, distinguishing source
offset from display anchors and AudioParam updates. Keep remaining sound,
DAW/device and asset-rights acceptance explicit; keep visible meters disabled.

## Early Apply/Q/nudge source and gain gate — 29 October 2026

Implementation: `eeb0383`, refined by `4f0f8b9`. Baseline: `b6751e0` / source
metadata code `1542e01`; reuse the earlier build/reference evidence above.
No dependency upgrade, renderer rewrite, ID/capacity/persistence change or edits
to `.reference/Ascii-Looper` / preserved `resources/ui`.

Acceptance: Apply/Q/nudge after a validated early callback schedules a replacement
50ms from the accepting audio frame, on either side of the original grid. It must
retain controls captured by the action, preserve the original graph's silence
until its volume start, survive a later scheduled STOP, and never reactivate the
old source at its original grid. Immediate cancellation, prepare/clear/cleanup,
unadmitted/stopping guards and full phase-FIFO rollback must remain safe.

| Reference behavior | Native result | Evidence | Remaining limit |
| --- | --- | --- | --- |
| Healthy graph restart can precede original source start | Existing bounded phase events admit the replacement at its own 50ms deadline | Unchanged `Loop.restart`/`AudioGraph.retrigger`, Engine and actual processor RPC tests | Early 5ms RTRG still separate |
| Post-effect graph volume remains zero until original grid | Source phase advances behind a retained volume gate; native smoothing opens it | Exact pre-grid zero PCM, unchanged volume scheduling facts, post-grid source comparison | Full WebAudio base/post fade shapes and resampling |
| Scheduled STOP acts on the already-created replacement | Pending replacement remains valid until actual stop; new Apply while stopping stays guarded | Following-grid Q/replacement PCM and existing normal Start/STOP regressions | Broader musical/host listening acceptance |
| Cancel and teardown cannot leak a silent advancing source | Stop/clear/panic/retirement close the gate and invalidate pending work | Strict zero tails through old grid, full FIFO/prepare/ownership regressions | Capture/edit graph origins and full release stress |

The baseline oracle reproduced zero native PCM versus reference 0.0176493642
(mono, 8kHz, 17-frame clip). Initial source-only evidence did not model the retained
volume node. Reading that unchanged node revealed its zero-volume start at the
original grid. The final oracle keeps all cases and tolerance, adds the volume
scheduling assertion, advances replacement phase while checking gated silence,
and compares post-grid PCM plus following-grid STOP. It still excludes complete
fade curves/resampling; it does not claim a finished browser-envelope renderer.

Implementation: a private graph-proof guard admits existing phase events while
a source is queued; grid activation preserves their epoch, and an earlier event
retires the old activation deadline. The scheduled volume deadline is retained
separately, using the existing native smoothing length when it opens. Source tails
use the gated value. Scheduled STOP preserves already-issued replacements; actual
stop/cancel/cleanup still invalidates later work. New Apply while stopping and raw
or unadmitted callbacks cannot bypass the proof. One transient frame field, no
new allocation, locks, queue, public command, parameter or session field.

GUI bindings in `resources/native/looper.js` already send `applyStart`,
`quantizeStart` and `nudgeStart` unconditionally. The generic `tools/aion.py loop`
client sends those same actions; human tracks remain 1..20. Actual processor RPC
cases exercise the shared operation/payload without a second audio engine.

Verification on final source:
- Windows `aion_core_tests --meter-queued` passed: 48 before/after-grid paired
  restart cases, 32 cancel/prepare/cleanup/unadmitted/stopping/FIFO cases, and
  16 cancellations after a source begins advancing behind a closed volume gate.
  Post-transition stereo PCM is exact against the existing playing-source path;
  reference ticks include strict gate silence and the first post-grid source
  sample, with full fade shapes excluded. Original
  cleanup, metadata, overflow and retired-result cases remain unchanged.
- Windows seven source/asset CTests passed in 117.77s (`--parallel 2`). New oracle:
  30 mono/stereo buffer configurations, 120 restart cases, both directions,
  8/48/96kHz, lengths 1/17/512/4097/20003; 2,918,302 compared frames and 2,563,772
  silent frames. Maximum error 6.3772108371829e-8 at unchanged tolerance 1e-6.
  The ordinary playing-source Start oracle and all earlier queued modes pass.
- Final Windows actual processor/CLI/MIDI/state/timer suite passed, including
  12 production RPC Apply/Q/nudge cases at 64/128 with real nonzero PCM, retained
  gate, exact deadline and clip length. Zero tracked callback new/delete.
  The new fixture drains bounded views outside audio every four callbacks; it
  never substitutes an old queued snapshot for current processing state.
- Final Linux Engine safety/PCM suite and actual processor/CLI/MIDI/state/timer
  suite passed at 64/128, including the new production RPC source/gate assertions.
  All seven final source/asset CTests passed in 298.84s with two independent jobs.
- Final Windows `tools/TestMouseWorkflow.ps1 -SkipMidi` passed at 64/128:
  actual pointer/keyboard Start/Q/nudge, queued STOP, recording, LIVE controls,
  STUT, exact stereo WAV export and MERGE/Undo/Redo. The synthetic host opens no
  hardware audio/MIDI device; physical MIDI was skipped.
- Windows Standalone/VST3 and Linux Standalone/LV2 builds passed on final code.
  Existing compiler warnings are retained separately; no dependency/format change.

Reproduce from existing configured trees (VS bundled CMake path is documented in
DEVELOPMENT_WORKFLOW.md when not on PATH):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
build/windows-core/Release/aion_core_tests.exe --meter-queued
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(js_native_queued_early_restart|js_native_queued_early_edits|js_native_queued_cleanup|js_native_queued_late_phase|js_native_queued_stop_phase|js_native_start_phase|original_ui_bytes)$'
cmake --build --preset windows --target aion_midi_processor_tests AionLooper_Standalone AionLooper_VST3 aion_mouse_host --parallel 2
build/windows/Release/aion_midi_processor_tests.exe
tools/TestMouseWorkflow.ps1 -SkipMidi
# Commit coherent Windows code, then sync; never reset dirty Linux work.
wsl -d Ubuntu --exec bash /mnt/d/Jorge/Documents/Codex/Aion-Looper/tools/SetupWslWorkspace.sh /mnt/d/Jorge/Documents/Codex/Aion-Looper
```

Inside `/home/jorge/projects/Aion-Looper`:

```sh
cmake --build build/core --target aion_core_tests aion_start_probe --parallel 2
build/core/aion_core_tests --meter-queued
ctest --test-dir build/core --parallel 2 --no-tests=error --output-on-failure -R '^(js_native_queued_early_restart|js_native_queued_early_edits|js_native_queued_cleanup|js_native_queued_late_phase|js_native_queued_stop_phase|js_native_start_phase|original_ui_bytes)$'
cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone AionLooper_LV2 --parallel 2
build/linux/aion_midi_processor_tests
```

Final warmed measurement harness: ignored `build/early-restart-bench-src` imports
the existing Release core. One stereo 512-frame loop at 48kHz; 512 idle warm
blocks, one early admission, controls/Apply on either side of the grid, single
schedule/replacement/nominal-grid callback observations, 128 playing warm blocks,
then 2,048 steady callbacks. Input generation, view draining and timing storage
are outside `Engine::process`. Runs were taken after the other tests finished.

| Platform | Block | Replacement | Schedule us | Replacement us | Mean us | p99 us | Observed max us | Budget us |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Windows | 64 | Before grid | 41.8 | 30.4 | 29.88 | 61.0 | 76.1 | 1333.33 |
| Windows | 64 | After grid | 31.4 | 30.2 | 29.50 | 60.2 | 79.8 | 1333.33 |
| Windows | 128 | Before grid | 52.2 | 53.5 | 58.16 | 98.7 | 145.8 | 2666.67 |
| Windows | 128 | After grid | 52.8 | 52.8 | 58.62 | 103.0 | 139.9 | 2666.67 |
| Linux/WSL | 64 | Before grid | 22.6 | 22.1 | 22.66 | 52.4 | 73.8 | 1333.33 |
| Linux/WSL | 64 | After grid | 20.2 | 21.5 | 22.21 | 47.2 | 75.8 | 1333.33 |
| Linux/WSL | 128 | Before grid | 41.2 | 56.3 | 43.65 | 79.4 | 111.0 | 2666.67 |
| Linux/WSL | 128 | After grid | 41.0 | 90.7 | 44.25 | 82.2 | 145.8 | 2666.67 |

Full data: local ignored `build/early-restart-bench-{windows,linux}.jsonl`, including
admission and nominal-grid observations. Rebuild/re-run with:

```powershell
cmake --build build/early-restart-bench-windows --config Release --parallel 2
build/early-restart-bench-windows/Release/queued_bench.exe
```

```sh
cmake --build build/early-restart-bench-linux --parallel 2
build/early-restart-bench-linux/queued_bench
```

These main-thread synthetic observations are not worst-case/maximum-load/host
scheduling guarantees, physical round-trip latency or cross-OS speed comparisons.
The graph's retained silence is musical scheduling, not a measured device buffer
latency. No permanent benchmark target/dependency or distributable was added.

No new REAPER/Ardour listening, physical audio/controller, sanitizer/full-suite,
Apple or offline MIDI-render acceptance is claimed. No package or push. Exact
browser gain curves, resampling, source overlap and wall/audio alignment remain
release/sound gates. Next: unchanged-JS early 5ms RTRG comparisons, then playback
speed/tempo/clock-source edits during the early interval. Keep meters disabled.

## Early 5ms RTRG — 29 October 2026

Acceptance: a validated early queued graph accepts the existing GUI/CLI RTRG
command, captures Start reset/quantization and direction at the tap, schedules
its replacement at +5ms before or after the grid, preserves pre-grid silence,
and does not resurrect the original source. Unadmitted/stopping sources remain
guarded; STOP, panic, clear, prepare, retained cleanup and full-queue metadata
rollback retain their existing ownership guarantees.

Implementation extends the RTRG guard and playing-branch selection;
`hasPreparedQueuedSource` excludes unvalidated/stop-pending sources. No new audio
storage, queue, persistence field or GUI/CLI command is introduced. Transient
`queuedSourceReady` snapshot telemetry enables the existing RTRG button only after
validated source admission, keeping unadmitted/stopping sources disabled. The existing
bounded phase-event execution and original volume gate own the replacement.

Tests extend the prior slice rather than replace its cases: 64 paired Engine
positive cases, 64 cancellation/overflow cases and 32 silent advancing-source
tail cancellations at tracks 0/19 and 64/128. Production RPC adds four RTRG
cases. The unchanged-JS source oracle adds 120 early RTRG starts across mono/
stereo, 8/48/96kHz and five clip lengths. PCM tolerance stays 1e-6; metadata
stays 1e-14. A hardware-free pointer case uses a test-host file marker to call
the existing message-thread queued-graph adapter; production gains no test RPC.
It checks disabled/unadmitted versus enabled/validated RTRG, exactly one trusted
button command, pre-grid zero PCM and subsequent audible playback. Oracle setup now preserves origin on an unchanged tempo instead
of accumulating redundant floating-point conversion drift. The processor test
allows initial grid activation inside the host block while keeping the exact
pending 5ms deadline; PCM silence and subsequent source assertions are retained.

Windows commands/evidence so far:
- Build `build/windows-core` Release targets `aion_core_tests aion_start_probe`.
- Build preset `windows` targets `aion_midi_processor_tests AionLooper_Standalone AionLooper_VST3`.
- `node tests/StartPhaseParity.cjs build/windows-core/Release/aion_start_probe.exe --queued-early-rtrg`: PASS, 120 starts, 2,885,470 compared frames, 2,596,604 silent frames; max absolute error 6.183482287314135e-8.
- `build/windows/Release/aion_midi_processor_tests.exe`: PASS, including actual shared GUI/CLI RPC and zero tracked processBlock new/delete.
- At the intermediate source snapshot these final gates were pending; their completed results are recorded below.

No REAPER/Ardour listening, physical controller or full WebAudio envelope/
resampling acceptance is claimed. No package or push requested for this slice.

Intermediate verification before GUI readiness follow-up:
- Windows `aion_core_tests --meter-queued`: PASS, all 160 early restart/control-safety cases plus retained queued ownership regressions, zero tracked callback new/delete.
- Windows eight affected CTests: PASS (150.69s, parallel 2), including original UI bytes, normal Start, queued STOP/late/cleanup/metadata/Apply and early RTRG.
- Linux core and production Standalone/LV2 builds: PASS; Engine queued suite and production processor executable: PASS at 64/128. These precede the additive readiness telemetry; rerun affected compiled tests on the final snapshot.
- `node tests/FrontendTests.cjs`: PASS after GUI readiness wiring.

Final-snapshot checklist (`c249f2e`, readiness wiring included):

| Reference behavior | Native status | Evidence | Remaining limit |
| --- | --- | --- | --- |
| RTRG on a healthy future graph replaces source at +5ms | Shared GUI/CLI action accepts only validated queued source | Strict unchanged-JS 120-start oracle; exact deadlines through actual processor RPC | Full WebAudio envelope/resampler comparison |
| Start reset/quantization and Reverse belong to replacement | Existing phase scheduler captures controls at action time | Mono/stereo, 8/48/96kHz and five lengths; 1e-14 metadata / 1e-6 source PCM tolerances unchanged | Speed/tempo changes during this interval are next |
| Retained graph volume stays silent until original grid | Existing gain gate retained through RTRG | Exact pre-grid zero PCM; real Windows early-source button test | Physical device/DAW listening |
| Stop/cleanup must retire pending and hidden advancing sources | Existing bounded cancellation and ownership paths reused | Paired Engine 64/128, tracks 0/19, full FIFO rollback, real JUCE lifecycle; zero tracked callback new/delete | Full integration sanitizers and long-session/max-load gates |
| Existing RTRG control reaches shared command after graph birth | Additive transient readiness telemetry enables button | Actual shared RPC and trusted Windows pointer test; no public test/debug operation | Linux WebKit pointer pass not executed this session |

Exact focused commands (existing configured caches; no dependencies upgraded):

```powershell
# Windows; resolve bundled CMake/CTest as in DEVELOPMENT_WORKFLOW.md.
& $cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
& build/windows-core/Release/aion_core_tests.exe --meter-queued
& $ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_start_phase|js_native_queued_stop_phase|js_native_queued_late_phase|js_native_queued_cleanup|js_native_queued_early_edits|js_native_queued_early_restart|js_native_queued_early_rtrg)$'
& $cmake --build --preset windows --target aion_midi_processor_tests AionLooper_Standalone AionLooper_VST3 aion_mouse_host --parallel 2
& build/windows/Release/aion_midi_processor_tests.exe
node tests/FrontendTests.cjs
& tools/TestMouseWorkflow.ps1 -SkipMidi
# WSL; after local commit and SetupWslWorkspace.sh sync, native storage only.
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/core --target aion_core_tests aion_start_probe --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/core/aion_core_tests --meter-queued
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec ctest --test-dir build/core --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_start_phase|js_native_queued_stop_phase|js_native_queued_late_phase|js_native_queued_cleanup|js_native_queued_early_edits|js_native_queued_early_restart|js_native_queued_early_rtrg)$'
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone AionLooper_LV2 --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/linux/aion_midi_processor_tests
```

The readiness follow-up rebuilds affected compiled targets and reruns Engine,
processor and early-RTRG PCM tests; unchanged passing broader source tests are
reused. No host routing/state/transport behavior changed, so REAPER/Ardour listening
and selected-format full validators remain integration/release work, not claimed
here. No physical MIDI/device tests, Linux WebKit pointer tests, sanitizers or
long-session/max-load tests were executed for this slice. Full browser fade curves,
resampling and complete feature parity remain pending. Build warnings include
existing alignment padding and test misleading-indentation warnings; they are
tracked separately from passing functional checks, without unrelated refactoring.

Final acceptance results (`c249f2e` code; documentation-only handoff follows):

- Windows and Linux final `aion_core_tests --meter-queued`: PASS. All 160 early restart/cancel/overflow/tail cases and retained queued ownership tests pass, with zero tracked callback new/delete.
- Both final production `aion_midi_processor_tests` executables: PASS, including 16 shared GUI/CLI early-start RPC cases (four new RTRG cases), readiness telemetry, MIDI/state, real JUCE message-loop lifecycle and zero tracked processBlock new/delete.
- Eight affected reference CTests: PASS Windows (150.69s) and Linux (446.41s), including preserved UI bytes. Linux `build/core` is cached Debug; these are test wall times under concurrent validation, not comparable build/runtime performance claims.
- Final rebuilt Windows/Linux probes, `StartPhaseParity.cjs ... --queued-early-rtrg`: PASS, identical 120 starts / 2,885,470 compared frames / 2,596,604 silent frames; maximum absolute PCM error 6.183482287314135e-8 versus unchanged 1e-6 tolerance.
- `FrontendTests.cjs`: PASS. Final `TestMouseWorkflow.ps1 -SkipMidi`: PASS at 64 and 128, including the new actual early callback/readiness/button/zero-before-grid/audible-after-grid case. Existing pointer/keyboard, Start/Q/nudge, RTRG repeat, STUT, LIVE, export, MERGE/Undo/Redo PCM checks also pass. No audio/MIDI device opened by the synthetic host.
- Final Windows Standalone/VST3/test host and Linux Standalone/LV2 builds: PASS. Existing warnings noted above remain; no new dependency or format added.
- `git diff --check`: PASS. Reference checkout, preserved resources/ui and logo remain untouched; parameter/action IDs, expanded capacity and persisted state format unchanged.

Warmed callback observations (isolated ignored `build/early-rtrg-bench-src`
harness, same source on both OSes): one stereo 512-frame clip, 48kHz, 512 idle
warm-up blocks, early validated source, one RTRG before/after the original grid,
128 playing warm-up blocks then 2,048 measured Engine.process calls. Input
creation, view draining and timing storage are outside the measured callback.
Windows uses the Release core in `build/windows-core/Release/aion_core.lib`;
Linux uses the production Release core in `build/linux/libaion_core.a`. The
initial Linux Debug-core observations are retained separately and not compared
to these Release results. Runs were sequential after test processes completed.

| OS / block / replacement | Schedule us | Replacement us | Steady mean us | p99 us | Observed max us | Callback budget us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Windows / 64 / before grid | 39.300 | 27.500 | 28.905 | 61.100 | 84.800 | 1333.333 |
| Windows / 64 / after grid | 29.200 | 27.400 | 28.600 | 48.400 | 81.900 | 1333.333 |
| Windows / 128 / before grid | 55.300 | 53.900 | 56.599 | 92.100 | 137.400 | 2666.667 |
| Windows / 128 / after grid | 55.400 | 54.100 | 55.752 | 93.000 | 117.800 | 2666.667 |
| Linux / 64 / before grid | 22.406 | 21.606 | 22.989 | 53.915 | 119.432 | 1333.333 |
| Linux / 64 / after grid | 20.806 | 18.405 | 21.731 | 53.214 | 134.535 | 1333.333 |
| Linux / 128 / before grid | 74.920 | 40.110 | 41.300 | 85.922 | 139.537 | 2666.667 |
| Linux / 128 / after grid | 42.611 | 40.610 | 41.444 | 83.222 | 126.734 | 2666.667 |

Reproduce the local harness with the CMakeLists/bench.cpp in the named ignored
source folder, passing `AION_SOURCE` and `AION_CORE_LIBRARY` above, then build
Release and run `queued_bench` after other tests stop. Outputs:
`build/early-rtrg-bench-windows.jsonl` and
`build/early-rtrg-bench-linux-release.jsonl`; Debug-core output is explicitly
`build/early-rtrg-bench-linux-debug-core.jsonl`. These are synthetic one-loop
callback observations, not worst-case/max-capacity scheduling, hardware round-trip
latency or an OS speed comparison. Full load/long-session measurements remain
release work. No tolerance was changed, failing case removed or sanitizer/DAW/
device pass inferred from these results.

Next concrete slice: compare speed/tempo/clock-source edits after early source
birth and before the original grid against unchanged JavaScript. Keep full browser
fade/resampling and physical device/selected-DAW release gates explicitly open.

## Early-source direct speed edits — 29 October 2026

Acceptance: after explicit early graph admission, original user speed changes
at 80ms or 1ms before the scheduled grid update the existing source's AudioParam,
not its buffer or actual source.start offset. Keep exact pre-grid silence and
match source PCM from the first representable deadline sample, including reverse
and fractional grids. No source-replacement phase event is created. Continue
using existing shared GUI/CLI speed commands and original slider/1x controls;
preserve stored clips/history, cancellation and callback allocation guards.

Scope is direct user speed edits on a validated, unstarted source without an
already-scheduled replacement. Tempo/clock-source changes and interleaved speed /
Apply / RTRG source replacements remain separate validation; no full browser
rate-envelope/resampling or device/DAW pass is inferred. Audible-source smoothing
is preserved. Reference files and preserved resources/ui remain unchanged.

Baseline failure: the new `--queued-early-speed` oracle rejected native PCM at
8000Hz / 17-frame clip, frame 3500 (native 0.045596588402986526 versus expected
0.04464251619354992, tolerance 1e-6). The oracle executes unchanged
`UIManager.setLoopSpeed`, asserts the same source/buffer/start offset and checks
the AudioParam event, with ordinary float conversion at the WebAudio boundary.
No PCM tolerance or old regression is relaxed.

Implementation retains the sub-sample difference between original grid time
and its native deadline. Direct speed edits on an admitted future source adjust
only that fractional advance and set the rate before start; ordinary playing
sources retain existing smoothing. This uses fixed per-track scalar storage,
no new queue/ownership/persistence model or command/parameter IDs.

Initial evidence (before final narrowing/RPC/UI/platform gates):
- Windows `StartPhaseParity.cjs ... --queued-early-speed`: PASS, 360 starts, 7,928,418 compared frames, 7,894,938 silent frames, max absolute error 5.568652272636143e-8 versus unchanged 1e-6 tolerance.
- Windows `aion_core_tests --meter-queued`: PASS. New 24 early speed cases compare exact stereo output for 80ms versus one-block-before-grid edits at tracks 0/19, both directions and 64/128. New 20 cancellation/prepare/cleanup cases and all prior queued source tests pass with zero tracked callback new/delete.
- Final build, actual production RPC, trusted Windows interaction and Linux results remain pending at this intermediate checkpoint.

Final-code checklist (implementation `eaf0eca`; regression follow-up `8329704`):

| Reference behavior | Native status | Verification | Remaining limit |
| --- | --- | --- | --- |
| UI speed setter changes the same future source's AudioParam | Direct speed action snaps only an admitted, unstarted source to its current target | 360 unchanged-JS starts; 80ms/1ms edits, .25/.5/2x, reverse, mono/stereo, 8/48/96kHz | Browser resampling and complete rate-envelope comparisons |
| Original source start offset remains captured, including fractional grid timing | Fixed per-track sub-sample advance correction; no restart event | Source/buffer/start-offset identity assertions plus first-sample PCM <=1e-6 | Speed edits interleaved with already-created Apply/RTRG sources remain separate |
| Grid gate and existing STOP/teardown still own audibility | Preserve deadline/silence, stopPending and source cancellation guards | 24 paired 64/128 stereo equality cases (8 also exercise retained normal STOP), 20 cancel/clear/panic/prepare/cleanup cases; zero tracked callback new/delete | Physical device and selected-DAW listening |
| Existing GUI and CLI speed commands share DSP/state | No new ID/action/parameter; existing speed slider and 1x button retained | Four added actual shared-RPC speed cases; trusted Windows range-key edit and reset at 64/128, stored clip/history/deadline unchanged | Linux WebKit pointer test not executed here; project-audio persistence remains separately unported |

Completed evidence before final Linux reference/timing gate:
- Final Windows and Linux Engine queued suites: PASS, including new speed/normal-STOP/cancellation cases plus retained ownership, overflow and queued-source regressions. Logs `build/early-speed-final-windows-engine.log` / `early-speed-final-linux-engine.log`.
- Both actual production processor suites: PASS, including 20 early-start shared GUI/CLI RPC cases (four speed cases), real JUCE lifecycle, MIDI/state and zero tracked processBlock new/delete. Logs `build/early-speed-windows-processor.log` / `early-speed-linux-processor.log`.
- Windows nine affected reference CTests: PASS, 244.69s with parallel 2. Original UI bytes and normal Start / queued STOP / late / cleanup / metadata / Apply / RTRG / speed gates retained. Linux equivalent still in progress at this intermediate note.
- `tools/TestMouseWorkflow.ps1 -SkipMidi`: PASS 64/128 after correcting only the new test's reset selector from the reference title to the existing native `1x` title. Early speed trusted key, unchanged clip/deadline, exact pre-grid silence and audible playback pass; earlier RTRG/STUT/LIVE, export and MERGE/Undo/Redo checks also pass. No audio/MIDI device opened.
- Windows Standalone/VST3/test host and Linux Standalone/LV2/processor builds: PASS. Existing alignment padding and test indentation warnings remain; no unrelated cleanup or dependency change.

Exact focused commands (existing caches, plus automatic CMake regeneration for
one new CTest registration):

```powershell
& $cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
& build/windows-core/Release/aion_core_tests.exe --meter-queued
& $ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_start_phase|js_native_queued_stop_phase|js_native_queued_late_phase|js_native_queued_cleanup|js_native_queued_early_edits|js_native_queued_early_restart|js_native_queued_early_rtrg|js_native_queued_early_speed)$'
& $cmake --build --preset windows --target aion_midi_processor_tests AionLooper_Standalone AionLooper_VST3 aion_mouse_host --parallel 2
& build/windows/Release/aion_midi_processor_tests.exe
& tools/TestMouseWorkflow.ps1 -SkipMidi
# Existing running Standalone, human track 20; add --instance when needed.
python tools/aion.py loop speed 20 0.5
# Commit locally, then sync with SetupWslWorkspace.sh as documented before WSL tests.
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/core --target aion_core_tests aion_start_probe --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/core/aion_core_tests --meter-queued
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec ctest --test-dir build/core --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_start_phase|js_native_queued_stop_phase|js_native_queued_late_phase|js_native_queued_cleanup|js_native_queued_early_edits|js_native_queued_early_restart|js_native_queued_early_rtrg|js_native_queued_early_speed)$'
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone AionLooper_LV2 --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/linux/aion_midi_processor_tests
```

The CLI example documents the existing shared command; this session's added
speed coverage calls the real production RPC used by both clients. Actual
external CLI lifecycle checks remain part of the passing processor executable.
No physical controller or REAPER/Ardour listening, Linux WebKit pointer tests,
Apple/CLAP, full format validators, sanitizers or long-session/max-load gates
were executed for this slice. No host routing/state/transport logic changed.
No package/push requested or created. Original reference and preserved resources/ui,
logo, capacity, IDs and persisted format remain unchanged.

Final acceptance — 29 October 2026:

- All nine affected reference CTests PASS on both Windows (244.69s) and Linux (745.00s), parallel 2. Final CTest logs on both OSes report identical speed-mode results: 30 source configurations, 360 starts, 7,928,418 compared frames, 7,894,938 silent frames and maximum absolute error 5.568652272636143e-8 against unchanged 1e-6 PCM tolerance. Existing metadata tolerances and old cases were retained. The Linux core cache is Debug; these are test wall times under concurrent validation, not build/runtime speed comparisons.
- Final Engine suites with the retained-STOP follow-up PASS on both OSes. There are 24 new paired speed cases (eight also test later normal STOP) plus 20 cancellation/prepare/cleanup cases, alongside all earlier queued-source ownership/overflow regressions; exact paired stereo output and zero tracked callback new/delete at 64/128 remain required.
- Actual Windows/Linux processor executables PASS, including the four added early-speed shared GUI/CLI RPC cases and all prior real message-loop, external CLI, MIDI/state and zero processBlock allocation regressions.
- Corrected full Windows trusted pointer/keyboard workflow PASS at 64/128: quarter-speed edit on the existing admitted future source, unchanged clip/history/grid, zero pre-grid output, audible playback at original grid and the existing 1x reset. Original RTRG/STUT/LIVE, export and MERGE/Undo/Redo regressions also PASS. The first attempt's failure was solely the new reset-button selector, corrected without application changes or weaker assertions.
- Final active Standalone/VST3 and Standalone/LV2 builds PASS. No production source changed after `eaf0eca`; `8329704` adds test coverage and corrects the selector, so unrelated passing wrappers/RPC/reference gates were not repeated for that test-only follow-up.
- Diff review and `git diff --check` PASS. The fixed scalar correction is bounded and allocation-free; source storage/lifetimes and the existing command queue remain unchanged. No reference or preserved UI/logo edits, ID/capacity/state-format changes, dependency upgrades, package or push.

Isolated warmed Release callback observations:

The ignored `build/early-speed-bench-src` harness uses one stereo 512-frame clip
at 48kHz. After 512 idle warm-up blocks, it admits a source and sends one .5x
speed action about 80ms before grid or during the last block before grid. Input
generation, snapshots/view draining and timing storage sit outside the measured
Engine.process call. After source start, 128 playing warm-up blocks precede
2,048 measured steady callbacks. Measurements ran sequentially after tests ended.
Windows links `build/windows-core/Release/aion_core.lib`; Linux links the actual
production Release `build/linux/libaion_core.a`, not the Debug reference-test core.

| OS / block / edit | Edit callback us | Source-start callback us | Steady mean us | p99 us | Observed max us | Callback budget us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Windows / 64 / ~80ms early | 44.000 | 27.100 | 29.011 | 61.000 | 94.900 | 1333.333 |
| Windows / 64 / last block | 31.800 | 31.800 | 31.134 | 75.300 | 99.900 | 1333.333 |
| Windows / 128 / ~80ms early | 52.400 | 52.200 | 57.021 | 92.500 | 129.500 | 2666.667 |
| Windows / 128 / last block | 60.400 | 60.400 | 57.522 | 91.600 | 166.300 | 2666.667 |
| Linux / 64 / ~80ms early | 21.515 | 21.913 | 22.426 | 53.389 | 93.131 | 1333.333 |
| Linux / 64 / last block | 23.407 | 23.407 | 22.971 | 51.197 | 79.286 | 1333.333 |
| Linux / 128 / ~80ms early | 42.033 | 41.536 | 44.577 | 83.469 | 151.798 | 2666.667 |
| Linux / 128 / last block | 58.169 | 58.169 | 45.987 | 96.517 | 162.556 | 2666.667 |

Local measurement commands: build the named ignored harness folder with CMake,
setting `AION_SOURCE` to the corresponding checkout and `AION_CORE_LIBRARY` to
the Release library above; build Release, then run
`build/early-speed-bench-windows/Release/queued_bench.exe` and, separately in WSL,
`build/early-speed-bench-linux/queued_bench`. Raw outputs are
`build/early-speed-bench-windows.jsonl` and
`build/early-speed-bench-linux-release.jsonl`. The last-block edit and source
start can share a callback, intentionally reporting the same duration.
These one-loop synthetic observations are not worst-case/max-capacity scheduling,
hardware round-trip latency, measured device latency or an OS speed comparison.
Long-session/max-load and full release renderer/device/DAW acceptance remain open.

Next concrete slice: unchanged-JS comparison for speed changes after Apply/RTRG
has already scheduled a replacement. Do not infer that multiple future sources
receive identical rate changes. Then validate early-source tempo/clock changes;
this patch deliberately leaves their updateRate path unchanged. Keep normal
playing-source smoothing and the accepted queued cancellation/ownership coverage.

## Scheduled replacement then speed — 29 October 2026

Acceptance: Apply (50ms) / RTRG (5ms) creates a new source immediately with its
initial rate event at future actualT. A speed edit before actualT targets that
newest node only: the old source continues at its rate and the future rate event
wins at source start. Control metadata nevertheless takes the new value for a
later source. Explicitly writing the same displayed speed after start must update
the live AudioParam rather than no-op. Preserve original grid silence, captured
source phase/direction, queue deadlines, stop/cleanup/prepare/overflow safety,
normal audible smoothing and fixed storage. GUI/CLI keep the existing commands.

Baseline reproduced: new unchanged-JS oracle failed at mono / 8000Hz / 17-frame
clip, native frame 3500: 0.0872587338089943 versus 0.04236931274822469 (unchanged
1e-6 tolerance). Reference AudioGraph.retrigger schedules the initial source
rate at actualT; UIManager.setLoopSpeed writes only nodes.source at now. The
model now evaluates those actual AudioParam events chronologically, with the
same existing sample-deadline normalization used for source starts. This fixes
test model timestamp consistency at near-integer floating-point boundaries,
without changing any PCM/metadata tolerance or reference code.

Implementation: add one float to each fixed phase-event record for its source
initial rate. Source activation snaps to that captured rate; control metadata
(phaseRate) and current source rate/target are intentionally separate. Unchanged
clock polls no longer replay an old control edit over the future rate event.
Direct same-value speed writes remain real writes. STOP/fresh source creation
restore current control rate for later playback. Effective-rate telemetry still
reports the original control calculation, as the JS property does. The timestamp
tie path scans at most the existing 128-entry phase queue, respecting a sub-sample
future initial event that rounds to the current native frame. No dynamic queue,
source allocation, ownership transfer, parameter/action ID or state-version change.

Initial focused evidence:
- `StartPhaseParity.cjs ... --queued-replacement-speed`: PASS, 120 starts (48 RTRG), 2,901,886 compared frames / 2,580,188 silent frames; max absolute error 6.3772108371829e-8 versus unchanged 1e-6 tolerance. Mono/stereo, 8/48/96kHz, five clip lengths and both source directions, replacements before/after original grid.
- Windows paired Engine tests: initial 32 Apply/RTRG/speed cases PASS at tracks 0/19 and 64/128, exact old/replacement stereo PCM, same-value live write and fresh play, zero tracked callback new/delete. Added 20 pending cancellation/clear/panic/prepare/retained-cleanup cases are under the final gate.
- Existing UI/CLI bindings, native frontend and preserved resources/ui are unchanged. Reuse the prior `ff78df8` real Windows pointer/keyboard evidence for Start/Apply, RTRG and speed controls. This slice adds eight actual mixed shared-RPC cases to the processor fixture; it does not claim an unexecuted new pointer timing test or physical-device check.
- Final relevant source/regression, active-format builds and production RPC tests are in progress before WSL acceptance.

Timestamp boundary follow-up: an added independent `--queued-replacement-speed-tie`
mode retains all previous cases and tests speed writes at the replacement deadline
for 17/512-frame clips (RTRG/Apply), reverse/mono/stereo and 8/48/96kHz. It caught a
real native boundary defect: converting original double seconds to frame units
could erase an ULP and misorder the user's write versus a future initial-rate
event. The fixed phase event now retains sourceStartSeconds instead of the
already-multiplied sourceStartFrame scalar; activation derives frame metadata,
while rate-event ordering compares the original seconds. No additional storage
or tolerance change beyond the captured float rate. Final tie/platform gates are
pending this snapshot; reuse the prior passing ten Windows source gates and
32 + 20 Engine cases only for the behavior they actually covered.

Final-code parity checklist (source-rate implementation `80bf816`, timestamp
precision follow-up `e222753`):

| Reference behavior | Native status | Evidence | Remaining limit |
| --- | --- | --- | --- |
| A source's future initial rate event wins over an earlier speed write | Capture rate in each bounded phase record; activate from that rate independently of displayed control | 120 mixed starts, Apply/RTRG, before/after grid; original node rate-event assertions and <=1e-6 source PCM | Full rendered WebAudio automation quantum/envelope/resampler comparison |
| Original timestamps order two writes even if they round to one native frame | Retain original seconds; later same-time writes win, sub-sample future events stay later | Independent 24-start deadline-tie oracle; no earlier case removed | Multiple pending replacements with rate edits need expanded sequence coverage |
| Older source is not the target of the newest node's speed edit | Current audio rate is separate from metadata; unchanged clock polls do not replay metadata | 32 exact paired Engine cases at tracks 0/19, reverse, 64/128, plus real processor callbacks | Early-source tempo/clock changes remain next |
| Same displayed speed after start still writes the live AudioParam; fresh plays use current controls | Direct writes are not discarded solely for equal metadata; STOP/fresh source reset uses current rate | Exact paired live-rate edit and fresh-play PCM in all 32 Engine cases | Full project/audio persistence remains separately unported |
| Pending source state must retire on stop/clear/panic/prepare/cleanup | Keep existing epoch, source-ticket and fixed-queue ownership; no new heap or lease | 20 added cancellation cases plus all prior overflow/cleanup guards; zero tracked callback new/delete | Long-session/max-load/sanitizer and device/selected-DAW release gates |
| GUI and CLI keep one action/state path | Existing commands/bindings and presentation unchanged | Eight added actual mixed shared-RPC cases, 28 early-start cases total, Windows/Linux processor suites | No new real-pointer timing pass this session; reuse `ff78df8` unchanged-control evidence |

Executed commands (existing caches, automatic regeneration only for added tests):

```powershell
& $cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
& build/windows-core/Release/aion_core_tests.exe --meter-queued
& $ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_start_phase|js_native_queued_stop_phase|js_native_queued_late_phase|js_native_queued_cleanup|js_native_queued_early_edits|js_native_queued_early_restart|js_native_queued_early_rtrg|js_native_queued_early_speed|js_native_queued_replacement_speed)$'
# Final precision follow-up: rerun affected mixed/tie cases and Engine/RPC;
# reuse unchanged passing wider source cases from the source-rate snapshot.
& $ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(js_native_queued_replacement_speed|js_native_queued_replacement_speed_tie)$'
& $cmake --build --preset windows --target aion_midi_processor_tests AionLooper_Standalone AionLooper_VST3 --parallel 2
& build/windows/Release/aion_midi_processor_tests.exe
# Commit/sync via SetupWslWorkspace.sh before WSL tests, as in DEVELOPMENT_WORKFLOW.md.
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/core --target aion_core_tests aion_start_probe --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/core/aion_core_tests --meter-queued
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec ctest --test-dir build/core --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_start_phase|js_native_queued_stop_phase|js_native_queued_late_phase|js_native_queued_cleanup|js_native_queued_early_edits|js_native_queued_early_restart|js_native_queued_early_rtrg|js_native_queued_early_speed|js_native_queued_replacement_speed|js_native_queued_replacement_speed_tie)$'
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone AionLooper_LV2 --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/linux/aion_midi_processor_tests
```

Windows source snapshot's ten CTests PASS (224.98s, parallel 2); final mixed/tie
checks PASS (55.16s). Final Windows and Linux Engine/RPC tests and active-format
builds PASS. Linux final eleven-source gate was still finishing at this note;
record its final result below. Existing test indentation and alignment warnings
are unchanged. No production UI/bridge or CLI code changed; actual shared RPC
coverage is new, prior pointer evidence is reused rather than claimed rerun.
No REAPER/Ardour listening, physical controllers, Linux WebKit pointer tests,
Apple/CLAP validation, full validators, sanitizers or long-session/max-load tests
were performed. No host routing/state/transport precedence was changed.
Original reference, preserved resources/ui and logo, parameter/action IDs, expanded
capacity, saved-state format and pinned dependencies remain unchanged.

Final acceptance — 29 October 2026:

- Linux all eleven relevant source CTests PASS (752.38s, parallel 2). Windows source-rate snapshot's ten gates PASS (224.98s), followed by final affected mixed/tie gates PASS (55.16s). The precision follow-up only retains seconds instead of the pre-multiplied frame scalar and fixes the tie ordering; unchanged passing broader Windows comparisons are reused, not claimed rerun. Linux core is cached Debug: these are test wall times, not comparable runtime/build performance measurements.
- Final mixed-source oracle on both OSes: 120 starts, 48 RTRG, 2,901,886 compared / 2,580,188 silent frames; max absolute error 6.3772108371829e-8, tolerance unchanged at 1e-6. Independent tie oracle on both OSes: 24 starts, 12 RTRG, 530,390 compared / 526,862 silent frames; max error 3.664825914961867e-8. Original script files and prior cases/metadata tolerances remain unchanged.
- Final Engine queued suites PASS Windows/Linux: 32 new exact paired stereo mixed-rate cases, explicit same-value live writes and fresh plays, plus 20 pending cancellation/clear/panic/prepare/retained-cleanup cases at tracks 0/19, reverse and 64/128. Existing direct-speed, Apply/Q/nudge/RTRG, STOP, source-retirement, overflow/loss/generation and raw-not-birth cases remain enabled and PASS; zero tracked callback new/delete.
- Final production processor executables PASS Windows/Linux: 28 early-start shared GUI/CLI RPC cases including eight new mixed cases, real JUCE message-loop lifecycle, external CLI, MIDI/state and zero tracked processBlock new/delete. Actual UI/bridge bindings are unchanged, so prior trusted pointer coverage at `ff78df8` is reused; no new actual pointer timing or Linux WebKit run is claimed.
- Final Windows Standalone/VST3 and Linux Standalone/LV2 builds PASS, with existing alignment-padding/test-indentation warnings retained separately. No additional format/dependency/version introduced. `git diff --check` PASS.

These source tests execute unchanged JS scheduling/DSP with IO node stubs and a
sample-deadline AudioParam-event model. They do not establish full browser-rendered
rate automation quantum, resampling or envelope parity. Physical devices, selected
DAW listening, full validators, sanitizers and long-session/max-load gates remain
open. No release-ready package or push was requested or created.

Isolated warmed Release callback observations:

Ignored harness `build/replacement-speed-bench-src` uses one stereo 512-frame clip
at 48kHz, 512 idle warm-up blocks, explicit queued-source admission, an Apply
replacement created roughly 80ms or one block before the original grid, then a
.5x speed write in the next callback before its 50ms replacement. It measures
speed-edit and replacement callbacks separately. After replacement, 128 playing
warm-up blocks precede 2,048 steady Engine.process measurements. Input generation,
view draining and timing storage are outside the measured callback. Runs were
sequential after all acceptance processes ended. Windows links
`build/windows-core/Release/aion_core.lib`; Linux links the production Release
`build/linux/libaion_core.a`, not the cached Debug test core.

| OS / block / Apply creation | Speed callback us | Replacement callback us | Steady mean us | p99 us | Observed max us | Callback budget us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Windows / 64 / ~80ms early | 47.100 | 32.100 | 31.691 | 62.400 | 68.900 | 1333.333 |
| Windows / 64 / last block | 30.400 | 30.900 | 30.249 | 61.300 | 84.000 | 1333.333 |
| Windows / 128 / ~80ms early | 54.200 | 54.500 | 60.058 | 96.200 | 126.600 | 2666.667 |
| Windows / 128 / last block | 55.200 | 54.900 | 59.935 | 95.400 | 124.500 | 2666.667 |
| Linux / 64 / ~80ms early | 21.749 | 22.042 | 23.877 | 53.685 | 99.337 | 1333.333 |
| Linux / 64 / last block | 22.140 | 21.650 | 24.206 | 54.762 | 95.908 | 1333.333 |
| Linux / 128 / ~80ms early | 42.713 | 43.987 | 47.042 | 90.128 | 115.599 | 2666.667 |
| Linux / 128 / last block | 43.888 | 44.378 | 47.262 | 88.267 | 152.336 | 2666.667 |

Local harness commands: configure the named ignored source folder with CMake,
`AION_SOURCE` set to the corresponding checkout and `AION_CORE_LIBRARY` to the
Release library above; build Release, then run
`build/replacement-speed-bench-windows/Release/queued_bench.exe` and, separately
in native WSL storage, `build/replacement-speed-bench-linux/queued_bench`.
Raw outputs: `build/replacement-speed-bench-windows.jsonl` and
`build/replacement-speed-bench-linux-release.jsonl`. Only speed-edit, actual
replacement and steady measurements are reported; the auxiliary grid marker is
not used for performance conclusions. These one-loop observations are not
worst-case/full-queue/max-capacity deadlines, hardware round-trip latency or an
OS speed comparison. The exact timestamp-tie queue scan remains bounded by the
existing 128 entries; full-load/long-session performance remains release work.

Next concrete slice: multiple pending Apply/RTRG sources interleaved with rate
writes, using the now-captured per-source rate and original timestamp. Then
validate early-source tempo/clock changes against their actual unchanged JS
handlers. Keep the accepted direct-speed, same-value write, fractional-tie,
cancellation/overflow and source-lifetime regressions intact. Do not infer full
browser/device/DAW acceptance from the passing source-event and RPC tests.


## Two pending same-action replacements / speed — 29 October 2026

Acceptance defined before extending the tests: schedule two Apply sources or two
RTRG sources before either starts; write .5x between creations and .25x after
source two exists. Earlier nodes must retain their own AudioParam events; source
two must capture .5x at birth despite the later .25x control metadata. Both source
starts must remain ordered, with the original grid volume gate, reverse PCM and
fresh-play/cancellation behavior intact. This slice adds evidence, not a new
control or a DSP rewrite. Different-action deadline overtaking remains separate.

| Reference behavior | Native status / evidence | Remaining limit |
| --- | --- | --- |
| Speed targets graph.nodes.source, the newest scheduled node | Existing implementation; new unchanged-JS two-source PCM oracle | Full browser AudioParam quantum/envelopes/resampling excluded |
| First and second future sources keep their own initial rates | Two Apply or two RTRG events, before/after grid, mono/stereo, 8/48/96kHz, five clip lengths and both directions | Mixed Apply/RTRG overtaking and tempo changes are not claimed |
| Original volume gate survives early replacements | Oracle checks silence through original grid and phase after both starts | Existing native fade shape is retained |
| Explicit later speed / fresh PLAY and cancellation remain safe | Paired Engine extension at tracks 0/19 and 64/128, StopNow/panic/clear/prepare/retirement fence | Full-load, device, DAW and renderer release acceptance stays open |

New CTest: `js_native_queued_multiple_replacement_speed` invokes
`node tests/StartPhaseParity.cjs <aion_start_probe> --queued-multiple-replacement-speed`.
Existing source cases and their 1e-6 PCM / 1e-14 metadata tolerances are retained.
The new mode executes unchanged original handlers, asserts source stop/start and
rate-event ownership, and evaluates each source's captured rate independently.
Engine tests retain all 32 single-source and 20 single-source cancellation cases,
adding 32 two-source and 20 two-source cancellation cases. Paired stereo checks
and callback allocation/deallocation tracking remain enabled.

Windows new oracle PASS: 30 configurations, 120 queued sequences (96 RTRG calls),
2,901,886 compared frames including 2,580,188 silent frames; maximum error
6.3772108371829e-8 at unchanged tolerance 1e-6. Further results recorded below.
No production DSP/parameters/commands/CLI/MIDI/bridge/UI/state or dependencies
changed. Prior source/RPC/active-format evidence at e222753 remains applicable;
unchanged suites and wrappers are not rebuilt solely for regression additions.


Reproduce this regression slice (existing configured caches):

```powershell
# Resolve $cmake / $ctest from Visual Studio as documented above.
& $cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
& build/windows-core/Release/aion_core_tests.exe --meter-queued
& $ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_queued_multiple_replacement_speed)$'
# Commit named source files locally, then sync only the committed snapshot.
wsl -d Ubuntu --exec bash /mnt/d/Jorge/Documents/Codex/Aion-Looper/tools/SetupWslWorkspace.sh /mnt/d/Jorge/Documents/Codex/Aion-Looper
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/core --target aion_core_tests aion_start_probe --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/core/aion_core_tests --meter-queued
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec ctest --test-dir build/core --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_queued_multiple_replacement_speed)$'
```

Windows registered tests PASS (2/2, 56.79s including the 54.36s source test).
These are test wall times, not callback performance or cross-OS comparisons.
The initial Windows Engine run covered the 32 added playback cases; the final
expanded cancellation executable is separately rebuilt and rerun. No production
code changed, so existing measured callback, processor RPC, UI interaction and
active-format build evidence is reused within its recorded scope. New physical
hardware, REAPER/Ardour listening, browser-renderer, sanitizer or long-session
acceptance is not claimed. No package or push created.

Next: mixed Apply/RTRG pending-source deadline overtaking with rate writes,
then early-source tempo/clock changes using the unchanged original handlers.


Final Windows and Linux Engine `--meter-queued` runs PASS, including all 32 new
two-source playback and 20 new two-source cancellation cases, retained single-source
cases and all pre-existing queued lifetime/overflow regressions. Both rebuilt
executables reported zero tracked callback new/delete. Windows final output is
retained locally at `build/multiple-replacement-windows-engine.log`.


Final Linux registered gate PASS: 2/2 tests, 155.68s including 155.06s for the new
source oracle. Linux matched Windows: 30 configurations / 120 sequences / 96 RTRG
calls, 2,901,886 compared frames including 2,580,188 silent frames, max error
6.3772108371829e-8 at unchanged 1e-6 tolerance. Linux uses the existing Debug core;
wall times are not a performance comparison. `git diff --check` PASS. Review found
no production callback/lifetime/state changes; original reference and embedded
assets remain unchanged. The scoped two-same-action slice has no known blocking
failure in the executed checks. Cross-action overtaking is the next slice, not
implicitly accepted by these results.


## Mixed RTRG then Apply / speed — 29 October 2026

Smallest mixed-order slice: RTRG creates a source at +5ms; Apply is invoked before
that source starts and creates another at +50ms. A .5x write between creations
must affect the first future node only; source two captures .5x, and a subsequent
.25x write must not override its later initial-rate event. Both starts, old-source
PCM, reverse direction, original grid silence, current control metadata, later
speed writes/fresh PLAY and cancellation must remain correct. The before/after-grid
matrix is retained. Acceptance uses unchanged original handlers and the existing
sample-deadline source AudioParam model, not a browser-renderer claim.

| Behavior | Added verification | Pending |
| --- | --- | --- |
| RTRG then Apply with speed writes | New `js_native_queued_rtrg_apply_speed`, 30 mono/stereo, 8/48/96kHz, five-length configurations with both directions and both grid sides | Final Windows/Linux results below |
| Both future nodes actually start | Oracle horizon extends through the second deadline +147 samples; Engine horizon through the second deadline +2048 samples | Browser envelopes/resampling excluded |
| Source/control rate ownership | 16 additional exact paired stereo Engine playback cases at tracks 0/19, 64/128 | No production code change required so far |
| Cancellation ownership | 20 additional StopNow/panic/clear/prepare/retirement-fence mixed-sequence cases; prior cases retained | Full release load/devices/DAWs remain open |

The horizon extension strengthens the prior two-same-action oracle as well;
rerun that affected test. All previous cases and PCM/metadata tolerances remain.
GUI/CLI commands, UI assets, DSP, state schema, IDs, expanded capacity and pinned
dependencies are unchanged. Reuse prior production RPC/UI/active-wrapper evidence
for unchanged code; this test-only slice does not require rebuilding all wrappers.

Important remaining difference, established by source inspection (not a claimed
rendered comparison): Apply then RTRG is not simply the reverse chronological
case. `AudioGraph.retrigger` stops only its current `this.source`. An Apply at
+50ms stops the older source at +50ms; a later RTRG at +5ms stops that future
Apply source before it starts, while the older source keeps its +50ms stop.
The older source can therefore overlap the RTRG source. Native `schedulePhase`
currently removes pending non-RTRG events and has one current source renderer;
its passing chronological source tests do not prove that overlap. Keep this as
the next explicit parity gap, requiring a multi-source stop/lifetime/audio oracle
and bounded native ownership if confirmed by that oracle. Do not switch the
expectation to only the newest source or count this overlap as accepted.

Reproduce: use the preceding core build and `--meter-queued` commands on Windows
and the committed native WSL mirror. Run CTest on each with:
`-R '^(original_ui_bytes|js_native_queued_multiple_replacement_speed|js_native_queued_rtrg_apply_speed)$'`
(and `-C Release` on Windows), `--parallel 2 --no-tests=error --output-on-failure`.
No package or push requested. Results and final checkpoint follow.


Windows final results for `3b3e45e`:

- Affected core/probe build PASS; expanded `aion_core_tests --meter-queued` PASS,
  including 16 new mixed-order playback and 20 new mixed-order cancellation
  cases. All previous queued tests remain enabled; zero tracked callback
  allocation/deallocation. Output: `build/rtrg-apply-windows-engine.log` (local).
- Three focused CTests PASS, 57.84s with parallel 2: original UI bytes, strengthened
  two-same-action oracle (54.00s) and new RTRG-to-Apply oracle (56.27s).
- New mixed-order oracle: 30 configurations, 120 queued sequences / 120 RTRG,
  2,940,238 compared frames including 2,541,836 silent frames; maximum absolute
  error 5.696890131856591e-8, unchanged 1e-6 tolerance.
- Strengthened previous oracle: 30 configurations / 120 sequences / 96 RTRG,
  2,901,934 compared frames including 2,580,140 silent frames; maximum absolute
  error 6.3772108371829e-8, unchanged tolerance. The slightly changed totals follow
  the longer horizon and subsequent fixtures' grid alignment, not removed cases.
- `git diff --check` PASS. No reference/preserved-asset, production callback,
  GUI/CLI/MIDI/bridge or state-model changes. No new host/device/UI acceptance
  claimed; prior evidence remains scoped to its original tests.

The initial exploratory Windows run used the shorter horizon and is superseded
by the final three CTests above. The final snapshot changes the printed scope
label only after that run to also explicitly exclude envelopes/resampling;
test logic and tolerances are the same. Linux uses that committed final snapshot.


Final Linux results: affected core/probe build PASS; `--meter-queued` PASS with
all 16 new chronological mixed-order playback and 20 cancellation cases, previous
queued cases and zero tracked callback new/delete. Three focused CTests PASS
(156.14s, parallel 2): original UI bytes, strengthened previous oracle (149.54s),
new RTRG-to-Apply oracle (155.28s). Both Linux oracle metrics exactly match the
Windows final metrics above. Cached Linux core is Debug; test wall times are
not comparable runtime/build performance. No production code changed, so no
new callback performance benchmark, wrapper rebuild, UI/bridge test or DAW/device
run is claimed. No validators/sanitizers/long-session release gate was rerun.

Diff review and `git diff --check` PASS. Prior cases and tolerances are preserved;
new horizons add post-second-start observations. Original reference, embedded
UI/logo, parameter/action IDs, capacities, schema and dependencies are unchanged.
The next slice must model all actually live sources and their independent stop
times for Apply then RTRG (including original-grid silence and cancellation),
then compare actual summed PCM and establish bounded ownership/overflow. The
source-inspection concern is pending that explicit oracle, not a fabricated
claim of measured mismatch. Keep tempo/clock work behind this overlap gate.


## Apply overtaken by RTRG: retained old-source overlap — 30 October 2026

Acceptance: execute unchanged Apply (+50ms), write .5x, then RTRG (+5ms) before
Apply starts, followed by .25x. Apply's source is stopped before birth; its older
source keeps its original +50ms stop and can overlap the new RTRG source after
the original grid. Compare the sum of all actually live source PCM, not only the
newest source. Preserve source-specific rates, reverse direction, original grid
silence, later explicit writes/fresh PLAY and cancellation. Test tracks 0/19,
64/128, bounded saturation, release and reuse. This is the smallest single-Apply
overtaking slice; arbitrary multiple pending Apply chains are not inferred.

The new `--queued-apply-rtrg-overlap` oracle retains the original BufferSource
start/stop times and each captured source rate. A source stopped before its
start contributes no audio. Actual old/new source samples are summed before
native loop pan/gain. PCM tolerance remains 1e-6; previous tests are retained.
The unchanged reference and preserved UI files are not edited. Browser gain
curves, AudioParam quantum and resampling remain outside this IO-stub model.

Baseline negative: original native probe fails at mono/8kHz/17-frame clip,
frame 6996: native 0.0242755226790905 versus source sum 0.06986710473102437.
Trace shows original old-source stop at Apply's deadline, canceled future Apply,
and the earlier new RTRG birth. Local negative: `build/apply-rtrg-negative.log`.
Do not weaken that expected sum to the newest source's output.

Implementation uses a fixed 128-slot retained-source pool in LoopBank. Reserve
before mutating/canceling phase events; when full, RTRG rejects and the accepted
Apply remains pending. At overtaking activation pin the older immutable clip,
phase/direction/rate/ramp/stutter state and retain its own stop deadline. Sum it
before shared volume/pan and the original queued gain gate. Stop/clear/epoch
changes/prepare release active and reserved leases. No audio-side new/delete;
clip releases go to the worker queue, now 512 entries for overlap + existing
current/held/history/job leases. Empty tracks bypass the overlap scan.

Regression additions: 120 unchanged-JS configurations/sequences (mono/stereo,
8/48/96kHz, five lengths, both directions, both grid sides); 16 paired Engine
playback +20 cancellation combinations, plus 128-slot saturation/recovery at
tracks 0/19 and 64/128. Existing shared production RPC test gains four overlap
cases using real GUI/CLI command paths and callback allocation tracking.

A new saturation fixture initially misread EngineView.rejected as a cumulative
counter. It is the latest user command's rejection flag (Engine.cpp); the reuse
check now requires false AND a newly scheduled source event. Full-pool rejection
still requires true with the accepted Apply deadline intact. No old regression
was removed or tolerance changed. Intermediate builds overlapped a struct edit;
those results are discarded. One clean affected-core build establishes a coherent
header/library/test baseline before final acceptance.

Exact commands/results and remaining limits follow after final verification.
No UI/bridge bindings, commands/parameter IDs, persistence schema, capacity,
logo/assets or dependencies changed. Existing GUI/CLI access invokes the repaired
single DSP path. No package/push requested.


Verification at implementation `bbe5ff2` (30 October):

- Clean Windows core build and `aion_core_tests --meter-queued` PASS. Final log:
  `build/overlap-clean-engine-windows.log`. Linux core build and the same queued
  suite PASS. Both include 16 new overlap playback cases, 20 overlap cancellation
  cases, 128-slot saturation/rejection/stop/clear/reuse at tracks 0/19 and 64/128,
  all prior queued tests and zero tracked callback new/delete.
- Windows all 14 relevant source CTests PASS (317.68s, parallel 2). New summed
  overlap oracle: 30 configurations / 120 queued sequences /120 RTRG,
  2,661,728 compared frames including 2,552,888 silent frames, max error
  7.978123062191145e-8, unchanged 1e-6 tolerance. Prior source modes remain intact.
- Windows Standalone/VST3 and Linux Standalone/LV2 builds PASS. Corresponding
  `aion_midi_processor_tests` executables PASS on both OSes, including four new
  overtaking GUI/CLI RPC sequences (32 total early-start cases), exact deadlines,
  current metadata/audibility, retained clip, real message-loop cleanup, external
  CLI instance routing, MIDI/state and zero tracked processBlock new/delete.
- No host routing/state/transport precedence or UI/bridge binding changed.
  Prior pointer interaction evidence is reused; no new renderer pointer test,
  physical device/controller, REAPER/Ardour listening, Apple/CLAP, full validator
  or long-session/max-capacity sound acceptance is claimed. Existing compiler
  alignment/float-equality/test-indentation warnings remain separate from passing
  core -Werror and acceptance gates. No dependency upgrade/package/push.

Reproduce (existing caches, see DEVELOPMENT_WORKFLOW.md for tool paths):

```powershell
& $cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
& build/windows-core/Release/aion_core_tests.exe --meter-queued
& $ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_start_phase|js_native_queued_stop_phase|js_native_queued_late_phase|js_native_queued_cleanup|js_native_queued_early_edits|js_native_queued_early_restart|js_native_queued_early_rtrg|js_native_queued_early_speed|js_native_queued_replacement_speed|js_native_queued_replacement_speed_tie|js_native_queued_multiple_replacement_speed|js_native_queued_rtrg_apply_speed|js_native_queued_apply_rtrg_overlap)$'
& $cmake --build --preset windows --target aion_midi_processor_tests AionLooper_Standalone AionLooper_VST3 --parallel 2
& build/windows/Release/aion_midi_processor_tests.exe
# Commit locally and sync with SetupWslWorkspace.sh before Linux source tests.
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/core --target aion_core_tests aion_start_probe --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/core/aion_core_tests --meter-queued
# Same 14-test regex above, ctest --test-dir build/core --parallel 2 (no -C).
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone AionLooper_LV2 --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec build/linux/aion_midi_processor_tests
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake -S . -B build/overlap-asan -DAION_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec cmake --build build/overlap-asan --target aion_core_tests --parallel 2
wsl -d Ubuntu --cd /home/jorge/projects/Aion-Looper --exec env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/overlap-asan/aion_core_tests --meter-overlap
```

Linux source/sanitizer final results and isolated callback observations follow.
Next slice: more than one pending Apply before an overtaking RTRG. Reference can
keep an intermediate Apply alive between its own start and the following Apply's
stop; current pruning of non-RTRG events is not proven equivalent for that chain.
Retain this as an explicit parity gap rather than claiming arbitrary burst sound
acceptance from bounded saturation tests. Tempo/clock work stays behind it.


Focused sanitizer acceptance (`9a47cf3` test selector; production implementation
unchanged from `bbe5ff2`): `--meter-overlap` PASS in Windows Release and Linux
ASan/UBSan Debug with leak detection and halt-on-error settings above. It runs the
16 new overlap playback, 20 cancellation cases and the 128-slot saturation/reuse
matrix at tracks 0/19 and 64/128. No sanitizer diagnostic/leak was reported; zero
tracked callback new/delete. Existing `--meter-queued` retains every prior case
and already passed Windows/Linux before this selector-only change. Do not claim
the full historical suite was rerun under sanitizers: that broad instrumented run
was stopped while computing, with no final result, and superseded by this scoped
ownership run. No implementation or tolerance was changed to speed it up.


Final Linux source gate: all 14 CTests PASS (949.99s, parallel 2); overlap test
147.35s. Its 120-sequence / 2,661,728 compared / 2,552,888 silent-frame counts
and 7.978123062191145e-8 maximum error exactly match Windows. Linux core is
cached Debug; test wall times are not an OS runtime/build speed comparison.
Final diff review and `git diff --check` PASS. Original source/UI/logo and stable
IDs/schema/dependencies are unchanged. No known blocker in the executed
single-Apply overtaking acceptance; remaining burst/renderer/DAW limits above
are not silently treated as passed.

Warmed Release callback observations, run sequentially after all acceptance and
build processes had finished (48kHz, one stereo 512-frame clip): ignored
`build/overlap-bench-src` imports the actual core Release libraries from
`build/windows-core/Release/aion_core.lib` and `build/linux/libaion_core.a`.
The queued harness warms 512 idle blocks, admits a queued source, creates Apply
~80ms or ~20ms before grid, writes .5x, creates RTRG, then writes .25x. It measures
the scheduling and actual 5ms birth callbacks, and the audible overlap window.
After the older stop, 128 warm-up blocks precede 2,048 steady measurements.
Input generation, view drains and timing storage stay outside timed Engine.process.
The corrected marker includes a birth that occurs in the .25x callback at block
128; preliminary zero birth markers were missing observations, not zero-cost
callbacks, and are superseded below. The auxiliary nominal-grid marker is not
used. Before-grid cases have no audible older-source overlap interval.

| OS / block / creation lead | RTRG schedule us | Birth us | Audible overlap max us | Steady mean us | p99 us | Steady observed max us | Budget us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Windows / 64 / ~80ms | 38.8 | 40.9 | n/a | 46.733 | 85.1 | 131.4 | 1333.333 |
| Windows / 64 / ~20ms | 44.4 | 43.6 | 46.2 | 36.104 | 67.5 | 86.8 | 1333.333 |
| Windows / 128 / ~80ms | 75.6 | 75.0 | n/a | 66.636 | 103.6 | 144.4 | 2666.667 |
| Windows / 128 / ~20ms | 75.3 | 75.1 | 117.1 | 67.682 | 107.2 | 168.5 | 2666.667 |
| Linux / 64 / ~80ms | 32.0 | 31.1 | n/a | 25.890 | 53.7 | 85.6 | 1333.333 |
| Linux / 64 / ~20ms | 32.3 | 31.3 | 46.4 | 25.055 | 42.3 | 70.0 | 1333.333 |
| Linux / 128 / ~80ms | 62.4 | 62.1 | n/a | 49.605 | 90.9 | 130.8 | 2666.667 |
| Linux / 128 / ~20ms | 61.5 | 61.8 | 62.4 | 51.197 | 95.2 | 448.6 | 2666.667 |

A second ignored harness warms 512 playing blocks, then submits 80 Apply/RTRG
pairs, processes five callbacks, submits another 48 pairs and processes five
callbacks. It measures the bounded burst/activation, all-128-retained-source
window before the first 50ms stop (27 blocks at 64, eight at 128), and 64 callbacks
covering lease release. No command rejected. This is synthetic capacity timing
and safety evidence, not an unchanged-JS arbitrary-burst sound comparison.

| OS / block | Burst/activation max us | Full overlap max us | Release-window max us | Budget us |
| --- | ---: | ---: | ---: | ---: |
| Windows / 64 | 106.7 | 163.6 | 98.3 | 1333.333 |
| Windows / 128 | 236.8 | 228.1 | 206.4 | 2666.667 |
| Linux / 64 | 97.8 | 119.0 | 109.2 | 1333.333 |
| Linux / 128 | 268.8 | 238.0 | 188.3 | 2666.667 |

Build ignored harness with CMake using `AION_SOURCE` and `AION_CORE_LIBRARY` set
to the corresponding checkout and Release library above; Linux additionally uses
`-DCMAKE_BUILD_TYPE=Release`, Windows `--config Release`. Run `queued_bench` then
`overlap_capacity_bench` from `build/overlap-bench-windows/Release` or
`build/overlap-bench-linux`, separately after acceptance jobs finish. Raw output:
`build/overlap-bench-windows.jsonl`, `build/overlap-capacity-windows.jsonl`, and the
Linux mirror's `build/overlap-bench-linux.jsonl`, `build/overlap-capacity-linux.jsonl`.
Observed maxima are limited samples on this system. No worst-case/long-session,
20-track simultaneous-load, driver/round-trip latency or OS performance ranking
is inferred. No repeated timing run was selected merely to lower an observed max.


## Multiple pending Apply sources overtaken by RTRG — 30 October 2026

Acceptance: first Apply captures its own direction/rate/start at +50ms; a second
Apply, created ~2ms later, stops it at the second Apply's start. RTRG at +5ms
then cancels only the newest future Apply. Sum the original, intermediate Apply
and RTRG source PCM using each source's captured rate and own start/stop, including
original-grid silence. Change .5x then 2x then .25x controls and reverse/Start
metadata between births to prove source independence. Preserve later writes,
fresh PLAY, stop/panic/clear/prepare/cleanup and bounded ownership at 64/128.

Baseline new unchanged-JS oracle fails the previous implementation: mono/8kHz,
17-frame clip, frame 7236: native -0.12318186461925507 versus source sum
-0.1388125105686761. Local log `build/apply-chain-negative.log`. Reference assets
and the 1e-6 PCM / 1e-14 metadata tolerances are unchanged.

Implementation: before pruning future Apply events, calculate every required
retained-source slot and reserve them all or reject without any mutation. Pin
intermediate clips with their captured phase, direction and initial AudioParam
rate; start/stop them independently without replacing the current graph source.
The newest Apply is canceled before birth. Existing earlier events due before
RTRG remain ordered instead of being pruned. No allocation, lock, I/O or growing
queue is added; reuse the fixed 128-slot source pool and worker retirement path.

New oracle `--queued-apply-chain-overlap` / CTest
`js_native_queued_apply_chain_overlap`: 30 mono/stereo, 8/48/96kHz, five-length
configurations, 120 two-Apply sequences across both directions/grid sides. Initial
Windows comparison PASS: 2,664,832 compared / 2,549,784 silent frames, max error
1.0434593022967675e-7. Browser envelopes/resampling/AudioParam quantum excluded.

Engine additions: 16 chain playback and 20 cancellation cases (paired exact
stereo, tracks 0/19, 64/128), plus atomic two-slot rejection at 127 occupied slots.
The latter requires that a different track can still acquire the remaining slot,
then verifies panic/clear release and chain reuse. Sample-timed batches stay
within 96 commands per block to respect the existing 128-command drain bound.
Shared GUI/CLI processor tests add four real-RPC chain cases; no command/parameter
IDs or UI bindings change. `--meter-overlap` retains its previous cases and adds
the chain cases; the full `--meter-queued` suite still contains all prior tests.

Current scope keeps clips and tempo unchanged between these scheduled births.
Chains involving live buffer edits and renderer fade/resampling require their
own evidence. The additional deadline-straddling prefix evidence follows below.
Results, exact commands and the next step follow after final verification.


Additional prefix variant: delay the second Apply/RTRG sequence ~47ms after
first Apply creation. The first Apply then starts before RTRG while the second
remains future. New `js_native_queued_apply_chain_prefix` executes all 120 cases
separately; the original early-chain test keeps its original sequence order and
32MiB probe-output bound. No prior case is removed. A preliminary combined
240-case run PASS (max 9.088834995729655e-8); final separate gates supersede it.
Engine playback adds 16 corresponding delayed-prefix cases at 64/128, keeping
all early-chain and cancellation cases. Remaining live-buffer-edit/tempo and
browser-rendered-envelope combinations stay explicitly outside this slice.


Windows results so far: original affected eight CTests PASS (200.57s, parallel 2),
then final separate early-chain and delayed-prefix gates PASS (50.25s /53.67s;
combined 53.70s). Early chain keeps the original 120-case order: 2,664,832 compared
/2,549,784 silent frames, max 1.0434593022967675e-7. New late-prefix gate: 120 cases,
2,733,228 compared /2,429,562 silent frames, max 7.88686600472488e-8. Tolerance 1e-6
unchanged; preserved source assets and prior cases remain intact.

The complete queued Engine suite PASS Windows/Linux, including all prior cases
and the initial early-chain additions. Final extended `--meter-overlap` PASS
Windows Release and Linux ASan/UBSan (leak detection, halt-on-error), adding the delayed
64/128 playback cases as well. It covers 16 single +32 early/late chain playback,
20 single +20 chain cancellation, existing 128-slot saturation and the new
127-occupied-slot atomic two-slot/cross-track admission test. Zero tracked callback
new/delete and no sanitizer diagnostic/leak. Source implementation is unchanged
between these runs; no broader instrumented historical-suite pass is claimed.

Windows Standalone build and production `aion_midi_processor_tests` PASS,
including four additional two-Apply GUI/CLI RPC cases (36 early-start cases),
metadata/deadline/audibility/clip lifetime, prior lifecycle/external CLI/MIDI/state
and zero tracked processBlock new/delete. Native code, APVTS/action IDs, schema,
track capacity and pinned versions stay unchanged except the scoped scheduler fix.

Standalone-only configure initially exposed cached pluginval paths requiring
disabled formats. Guard validation registration by AION_BUILD_VST3/LV2, matching
the existing CLAP behavior. Retain paths and validate normally when explicitly
enabled. Windows preset configure now PASS with VST3/LV2/CLAP OFF, and CTest -N
registers zero deferred-format validator gates. No wrapper was built this session.

Linux verification reuses existing `build/linux` Release core/probe/test targets,
with its independent native sources/dependencies. This avoids repeating the
slower cached Debug source-oracle run; configurations differ, so wall times are
not claimed as runtime/build improvement or compared directly with prior Debug.
The first Standalone-only reconfigure changes JUCE target compile definitions and
requires dependent module rebuilds; do not call that a warmed incremental build.


Final native Linux results: all nine affected source CTests PASS (394.48s,
parallel 2), including both final chain variants. Final full `--meter-queued`
PASS includes delayed-prefix cases. Standalone and production shared GUI/CLI
processor build/tests PASS, with zero tracked processBlock new/delete. Both
production caches have VST3/LV2/CLAP OFF and register zero deferred validator
gates. Cached validator paths remain intact for explicitly enabled formats.
Windows full queued tests passed before the delayed-selector extension; final
focused overlap tests include that extension. Production source did not change
between those checks. No full historical sanitizer suite was run this session.

Release timing observations, measured sequentially after acceptance work at
48kHz stereo (microseconds; no OS ranking or worst-case claim):

| Platform / block | Highest sampled one-chain callback | Highest sampled full-pool callback | Callback budget |
| --- | ---: | ---: | ---: |
| Windows / 64 | 119.2 | 96.4 | 1333.33 |
| Windows / 128 | 206.1 | 185.8 | 2666.67 |
| Linux / 64 | 110.6 | 82.9 | 1333.33 |
| Linux / 128 | 245.7 | 142.8 | 2666.67 |

One-chain cases cover early/delayed, before/after-grid scheduling, source birth,
intermediate start/stop, audible overlap and warmed steady playback. Pool cases
reserve 128 slots with 64 two-slot chains, in bounded 96-command batches, then
measure activation and lease expiry with zero rejections. This is not 128
simultaneously audible sources or a 20-track load/long-session test. Ignored
harness/raw JSONL files live under each checkout's `build/chain-*`; these are
local measurement evidence, not shipped benchmarks or round-trip latency.

Reproduce relevant checks after building the named targets (Windows CMake/CTest
from the installed VS Build Tools; native WSL working directory for Linux):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --meter-overlap
cmake --build --preset windows-dev --target aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --meter-queued
./build/linux/aion_midi_processor_tests
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_start_phase|js_native_queued_early_restart|js_native_queued_early_rtrg|js_native_queued_multiple_replacement_speed|js_native_queued_rtrg_apply_speed|js_native_queued_apply_rtrg_overlap|js_native_queued_apply_chain_overlap|js_native_queued_apply_chain_prefix)$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --meter-overlap
```

The isolated sanitizer cache uses Debug, plugin OFF, tests ON, with
`-fsanitize=address,undefined -fno-omit-frame-pointer` compiler flags and
`-fsanitize=address,undefined` linker flags. Windows source gates use the same
CTest regex with `--test-dir build/windows-core -C Release` (eight prior affected
gates followed by the final two chain gates were actually executed this turn).
No new renderer/pointer, listening, physical MIDI/audio, DAW, wrapper-validator,
full historical sanitizer or long-session pass is claimed. Existing compiler
warnings remain separate from this slice. No package or push was created.


## Three-Apply chain acceptance — 4 November 2026

Acceptance defined before editing: three Applies followed by overtaking RTRG,
both all-future and two-already-started prefixes; independent Start/reverse/rate
and own stop, grid silence, 64/128 PCM, cancellation and atomic three-slot failure
with two free slots. Regression snapshot `fc580c0` passes with the existing generic
production scheduler unchanged; no new UI binding, command, parameter, state
schema, runtime capacity, dependency or preserved-asset change. GUI/CLI reuse the
same existing command implementation. Previous single/two-Apply cases remain.

| Executed verification | Windows Release | Native WSL Linux |
| --- | --- | --- |
| Core/probe + Standalone/shared-processor builds | PASS | PASS (Release) |
| Five source CTests: original assets, both two-Apply and both three-Apply modes | PASS, 114.14s, parallel 2 | PASS, 202.79s, parallel 2 |
| Final `--meter-overlap` | PASS | PASS (Release) |
| Production `aion_midi_processor_tests` | PASS | PASS (Release) |
| Focused ASan/UBSan/leak `--meter-overlap` | Not run | PASS (Debug), no diagnostic |

New oracle modes: `--queued-apply-triple-overlap` and
`--queued-apply-triple-prefix`. Each runs 30 mono/stereo, 8/48/96kHz, five-length
configurations /120 sequences with unchanged 1e-6 PCM and 1e-14 metadata tolerance.
Windows early: 2,667,898 compared /2,546,706 silent frames, max error
7.663715295702644e-8; prefix: 2,736,302 compared /2,420,372 silent frames, max error
7.879732011328855e-8. Initial fixture .75x was incorrectly sent raw to the native
probe while the sync UI snapped it to 1x; use the supported 1x ratio in both paths.
No tolerance/source change or production failure is implied by that fixture fix.

Focused Engine selection now covers 80 paired stereo playback and 80 cancellation
cases (32 new three-Apply playback, 40 new pending/active cancellation), tracks
0/19, directions and 64/128; explicit same-value speed writes and fresh PLAY.
Retain all prior capacity cases; four new fixtures reserve 126 slots, reject a
three-slot request without canceling accepted Applies, then admit a two-slot
chain on another track, panic/clear and recover. Batches stay within 96 commands;
64-sample first batches span two callbacks. Cancellation covers StopNow/panic/
clear/prepare/cleanup after intermediate PCM begins as well as before birth.
Zero tracked callback new/delete. Production RPC adds eight three-Apply early/
prefix cases (44 early-source cases total), alongside existing external CLI,
timer/MIDI/state/lifecycle checks and zero tracked processBlock new/delete.

Logs: Windows `build/triple-production-windows.log`; native Linux
`build/triple-core-linux.log`, `triple-production-linux.log`, `triple-asan.log`
and build logs. Existing float-comparison/alignment warnings remain unrelated.
No full historical suite, wrapper build, validator, pointer/renderer, physical
MIDI/audio, listening or long-session acceptance was run/claimed this session.

Release timing observations at 48kHz stereo, Windows then Linux after acceptance
jobs stopped: four early/prefix/grid-side cases per block, 512 idle warm blocks,
full admission/transition scenario and 2048 steady callbacks per case. Highest
scenario or steady callback observed (microseconds):

| Platform | 64 samples / 1333.33us budget | 128 samples / 2666.67us budget |
| --- | ---: | ---: |
| Windows | 89.0 | 162.5 |
| Linux | 121.92 | 155.607 |

These are one-clip three-Apply scenarios, not three simultaneously audible voices,
a worst-case/physical-latency bound, OS ranking or 20-track load test. Previous
full-pool timings remain historical. Ignored local harness/raw JSONL lives under
`build/triple-bench-src`, `build/triple-bench-windows.jsonl` and native WSL
`build/triple-bench-linux.jsonl`. Harnesses link unchanged Release core and time
only Engine.process; input generation/view drain/timing storage are outside.
A raw nominal_grid_us zero means the grid already passed, not a zero-cost call.

Exact focused commands, from the matching checkout with installed CMake/CTest
(Windows uses VS Build Tools):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --meter-overlap
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_queued_apply_chain_overlap|js_native_queued_apply_chain_prefix|js_native_queued_apply_triple_overlap|js_native_queued_apply_triple_prefix)$'
cmake --build --preset windows-dev --target aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --meter-overlap
./build/linux/aion_midi_processor_tests
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_queued_apply_chain_overlap|js_native_queued_apply_chain_prefix|js_native_queued_apply_triple_overlap|js_native_queued_apply_triple_prefix)$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --meter-overlap
```

Sanitizer configuration remains the prior Debug/plugin-OFF/tests-ON cache with
address/undefined sanitizer and frame-pointer flags. CMake regenerated because
two CTest registrations were added; no configure/build speed claim. Linux used
the independent committed mirror via SetupWslWorkspace.sh without overwriting
local work. No package/push. Next: NORM/live PCM replacement between pending Apply
creation and RTRG, including original-buffer ownership and cancellation. Constant-
buffer acceptance does not establish that case, arbitrary longer chains, tempo
changes or full browser-rendered envelopes/resampling.


## Pending Apply/RTRG with NORM — 4 November 2026

Acceptance defined before production edits: preserve source starts/stops, phase/
rate and history through NORM before RTRG, after its birth and while an older
Apply is retained. Forward sources share normalized stored PCM; already-created
reversed sources keep independent old PCM. New reversed sources use current PCM.
Original assets and 1e-6 PCM /1e-14 metadata tolerances remain unchanged.
Baseline mono/8kHz/17 frames, frame 3541: native -0.2611359655857086 versus source
sum -0.17801891043622273 (`build/norm-pending-negative.log`).

Fix `09eac61`: NORM no longer invalidates phase events; valid source events still
activate while its worker is pending. Each scheduled reversed source owns one
raw-PCM lease, transferred/released on activation, retention, cancellation or
reset. Retained forward sources rebind to normalized immutable output; reversed
copies remain unchanged. Work/storage stays bounded at 127 phase events and 128
retained sources. No callback allocation/deallocation, blocking or full-PCM edit
was added. Existing 512-entry retirement queue and its worker-owned-until-stopped-
prepare overflow fallback remain unchanged. No parameter/action ID, preset/state
schema, capacity, UI binding or dependency change; GUI/CLI share existing NORM.

| Reference behavior | Native status | Evidence / remaining gap |
| --- | --- | --- |
| Playing NORM preserves pending and active source timing/PCM | Fixed | Three new reference positions; delayed-worker Engine cases |
| Forward alias and reversed snapshot ownership | Fixed for NORM | Source PCM at both directions, mono/stereo and 8/48/96kHz |
| No-op, failure and cancellation preserve musical state | Verified scoped cases | No-op/OOM/history/prepare/pool checks below |
| NORM before native queued-track activation | Still rejected | Next slice; current new oracles normalize after the grid |
| Complete browser renderer and physical performance | Pending | Envelopes/resampling/listening/devices/long sessions not certified |

| Executed checks | Windows Release | Native WSL Linux |
| --- | --- | --- |
| Core/probes + Standalone/shared-processor build | PASS | PASS (Release) |
| 15 selected source/asset CTests | PASS, 309.51s, parallel 2 | PASS, 432.57s, parallel 2 |
| `--norm-pending`, `--loop-edits`, `--meter-overlap` | PASS | PASS (Release) |
| `aion_midi_processor_tests` | PASS | PASS (Release) |
| ASan/UBSan/leak checks: those three core selections | Not run | PASS (Debug), no diagnostic |

New source modes each cover 60 cases/sequences (mono/stereo, 8/48/96kHz, five
lengths, both directions), 1,344,236 compared /1,238,012 silent frames. Windows
max errors: before/after RTRG 1.450341824416057e-7, active retained Apply
8.03442081753758e-8. Source tests commit the worker result without advancing audio
samples; they do not measure real-time edit latency. The separate Engine tests
pause only worker allocations outside processing while audio deadlines advance.

New Engine cases: 48 changed/no-op worker-delay, 20 stop/panic/clear/same/new-rate
prepare, eight exact paired OOM rollback and four full-pool checks, tracks 0/19
and 64/128; zero tracked callback new/delete. Preserve all previous regressions.
The new prepare assertion was corrected to the established same-rate stop/retain
contract, adding a rate-change clear case. Full-pool setup now drains the separate
127-event queue between bounded batches while keeping all 128 leases live; the
exhaustion assertion remains and each batch admission is checked. Existing
reverse/full-FIFO cancellation checks also pass in the edit suite.

Production RPC adds six NORM cases (50 early-source cases total), including
preserved Apply deadlines and audible playback, alongside prior external CLI,
timer/MIDI/state tests; zero tracked processBlock new/delete. New processor NORM
cases use controlled offline commits. No new pointer/renderer test is claimed;
those implementations/bindings are unchanged. Logs: `build/norm-production-windows.log`, `norm-loop-edits-windows.log`, `norm-overlap-windows.log`; native WSL
`build/norm-core-linux.log`, `norm-loop-edits-linux.log`, `norm-overlap-linux.log`,
`norm-production-linux.log`, `norm-asan.log`. Existing compiler warnings remain
separate; no full historical or deferred-format validation pass is claimed.

Release Engine.process timing observations at 48kHz stereo, sequential Windows
then Linux after acceptance jobs stopped. Reused ignored full-pool harness:
512 playing warm blocks, 64 two-slot chains /128 reservations, forward/reverse,
natural expiry versus clear; eight cases/platform. Maximum callback among burst,
NORM admission/commit attempts, full-pool playback and clear/expiry (microseconds):

| Platform | 64 samples /1333.33us budget | 128 samples /2666.67us budget |
| --- | ---: | ---: |
| Windows | 126.1 | 299.0 |
| Linux | 189.896 | 182.096 |

Worker preparation sleeps occur outside timing with sample time frozen; one
Windows case needed two commit-attempt callbacks. These are callback cost samples,
not hardware continuity/edit latency, a worst-case bound, OS ranking, 128 audible
voices, 20-track/long-session or complete JUCE/device callback measurements.
Ignored local harness/raw output: `build/norm-bench-src`, Windows
`build/norm-bench-windows.jsonl`, native WSL `build/norm-bench-linux.jsonl`.

Exact commands from each checkout, with installed CMake/CTest (Windows uses VS
Build Tools). Core cache builds/probes and selectors:

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --norm-pending
./build/windows-core/Release/aion_core_tests.exe --loop-edits
./build/windows-core/Release/aion_core_tests.exe --meter-overlap
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_(normalization|reverse|history|multiply|merge|start_phase|queued_apply_rtrg_overlap|queued_apply_chain_overlap|queued_apply_chain_prefix|queued_apply_triple_overlap|queued_apply_triple_prefix|apply_norm_before_rtrg|apply_norm_after_rtrg|apply_norm_retained))$'
cmake --build --preset windows-dev --target aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --norm-pending
./build/linux/aion_core_tests --loop-edits
./build/linux/aion_core_tests --meter-overlap
./build/linux/aion_midi_processor_tests
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|js_native_(normalization|reverse|history|multiply|merge|start_phase|queued_apply_rtrg_overlap|queued_apply_chain_overlap|queued_apply_chain_prefix|queued_apply_triple_overlap|queued_apply_triple_prefix|apply_norm_before_rtrg|apply_norm_after_rtrg|apply_norm_retained))$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
for selection in --norm-pending --loop-edits --meter-overlap; do
  env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests "$selection" || exit "$?"
done
```

Sanitizer flags/cache are unchanged from the earlier checkpoint. Windows snapshot
was committed before SetupWslWorkspace.sh synced the independent native mirror;
no Linux source work was overwritten. No package/push. Next: allow NORM on still-
queued native tracks with explicit queue/graph/state preservation and unchanged-
JS evidence; do not generalize this playing-source slice to that workflow or
other buffer replacement operations.
