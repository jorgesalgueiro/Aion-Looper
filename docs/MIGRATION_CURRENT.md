# Current migration checkpoint

Updated 2026-10-28. Current repair baseline: `02c4625` (clean main, three local
commits ahead of origin). Prior STOP acceptance remains applicable.
Continue the existing implementation; this is not a restart or release checklist.

## Reference and baseline

The requested `D:\Jorge\Documents\Codex\Aion-Looper.reference` path is absent.
The available original is `.reference/Ascii-Looper` inside this repository.
`tests/AssetTests.py` verifies embedded original UI bytes against that copy.
Neither reference nor original UI files are modified by this repair.

The baseline evidence is `build/unsynced-stop-evidence.json`: Windows/Ubuntu
portable 28/28 each, native 2/2 each, core sanitizer 6/6, 96 queue-delay cases,
126 committed-PCM cases. A fresh focused baseline passed before the new normal
start-guard regression was added; its reproduced failure is recorded below. Historical
STOP ALL and STOP NOW failures/evidence remain in VALIDATION.md and `build/`.

## Compact parity checklist

| Reference behavior | Native status / evidence | Remaining gap |
| --- | --- | --- |
| Capture and accepted loop edits preserve committed PCM | Existing scoped audit: 126 capture/OD/SUB cases at 1e-6; other loop-edit gates retained | Full browser envelopes/resampling and physical input timing are separate |
| Queue callback uses max(0, grid delta minus 100ms) | Accepted unrounded metadata; 96 actual Engine/original grid+dispatcher cases retained | Actual message callback, graph transaction and wall alignment remain unwired |
| STOP ALL cancels a still-queued intent even after its grid time | This repair routes queuedCancel by explicit pending ID; core, production processor, 20-route and real-timer regressions | This does not fix audible playback occurring before a late message callback |
| STOP NOW clears pending queue before immediate graph cleanup, including after grid | Explicit queue-ID routing; graphless/attached, production requests and real 25ms timer checks | Normal synchronized STOP and old-teardown state boundary remain separate |
| Normal STOP takes immediate path when sync is disabled | Pending-ID routing now also covers sync disabled after grid, with the same graphless/25ms rules | Scheduled STOP with sync still enabled remains outside this repair |
| Queued cancellation retains older analyser/data/history and cleanup | Existing owner/adapter policy retained, including original pre-grid test plus added post-grid frozen-clock test | If old cleanup already changed message state out of queued, existing rejection remains intentional pending full mapping |
| Audible grid does not prove message graph birth | Graph origin remains quarantined; no new graph, reset or 65ms cleanup inferred | Visible meters, remaining capture/edit graph origins, overlap and continuity remain unfinished |
| Pending intent requires its conditional callback transaction | Generic start now rejects real queued IDs before/after grid and after old teardown | Explicit callback transaction and binding/rejection recovery still need implementation |
| Full instruments/effects/controller/persistence/host parity | No new acceptance from this session; see PARITY.md and feature-specific validation notes | Do not infer suite completion from these looper/meter gates |

## Current slice and acceptance

Close the generic graph-start bypass before wiring a real queued callback.
`LoopMeterLifecycle::start(frame)` could previously clear an accepted queue ID,
create a graph and enqueue an unconditional raw reset. Reject while a real queue
ID is pending, independently of native/message state. Component starts without
a real queue ID retain their existing contract. No audio, UI, parameter, state,
dependency or platform configuration changes are needed.

Acceptance: immediate/future generic starts reject before/after actual audible
grid, retaining the full graph, binding, queue ID/delay, cleanup and history and
emitting no raw reset. Include graphless/attached tracks 0/19 at 64/128, the
old-teardown state boundary, all twenty prepared routes, real JUCE old timers
with frozen audio clocks, exact paired stereo PCM and zero tracked callback
new/delete. Retain all existing source lifecycle, conditional-window, STOP and
component graph-start regressions. No new interface or disconnected control.

The focused baseline passed, then the new test reproduced
`Generic graph start must reject a real pending queued intent` before the product
edit (`build/queued-start-guard-before.txt`). The original source remains unchanged:
its queued handler schedules an explicit callback that later invokes play(grid),
rather than treating an audible state transition as graph creation. Native raw
start receipts still do not prove that callback occurred.

Previous STOP ALL acceptance remains in force: pending-ID cancellation before/
after grid, stale/duplicate rejection, ordered delayed receipts, twenty bounded
7/13 routes and original cleanup/history. See VALIDATION.md and
`build/post-grid-panic-evidence.json`; those regressions remain enabled.

## Reproduce

Windows (PowerShell, repository root; use the bundled CMake/CTest path if not on PATH):

```powershell
cmake --build build/windows-core --config Release --parallel 2
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
build/core/aion_core_tests --meter-queued
ctest --test-dir build/core -V
cmake --build build/linux --target aion_midi_processor_tests AionLooper_Standalone -j2
ctest --test-dir build/linux -V -R '^(native_midi_processor|standalone_clock_sdk_generation)$'
cmake --build build/core-asan -j2
ctest --test-dir build/core-asan -V -R '^(core_realtime_contract|js_native_playback_clock|js_native_meter_graph|original_meter_source_lifecycle|js_native_meter_window|js_native_meter)$'
```

Retain BUILDING.md's configure/dependency/platform instructions. No dependency
upgrade, new format/package or UI activation is needed for this slice. Never edit
compiling sources or overlap CTests in a build tree.

## Next concrete step / validation still needed

This repair passed Windows/Ubuntu portable 28/28 each, native 2/2 each, core
sanitizer 6/6, the unchanged-tolerance 126-case PCM audit and original UI byte/
embedding/tamper checks. No unrelated failing checks were found in these runs.
Current generic-start guard evidence is `build/queued-start-guard-evidence.json`:
Windows portable 377.05s, Ubuntu portable 599.31s, native 11.42s/23.13s,
sanitizer 280.84s. `build/unsynced-stop-evidence.json` and
`build/post-grid-stop-now-evidence.json` retain the preceding STOP acceptance;
their original cases remain enabled.
Both current Standalone binaries rebuilt. See VALIDATION.md for scoped results;
`build/post-grid-panic-evidence.json` retains the preceding STOP ALL evidence.

Normal synchronized STOP after audible grid and the old-teardown
state boundary remain unaccepted. Original `scheduleStop()` requires message
state playing and a graph; native audible playing with an undelivered callback
does not prove those conditions. This generic-start guard does not cover
that scheduled policy and must not change audible timing to guess it.
The generic start guard is only a safety boundary, not that callback implementation.
Implement the queued message callback as an explicit graph transaction with
conditional raw-reset acceptance/rejection recovery. Establish a valid wall/audio
clock relationship before wiring its 100ms timer; retain independent old cleanup
and late-callback behavior. Do not infer birth from a raw receipt or audio state.
Do not wire generic start to that timer: the callback must carry queue/clip IDs,
handle accepted/rejected/lost reset work, and prevent old cleanup/cancel/clear
from resurrecting a graph while binding is pending. Validate subsequent STOP/
STOP ALL routing after callback acceptance before calling that lifecycle complete.
Only then integrate meter observation and the renderer.

Full transition sound/late-source alignment is still unaccepted. No REAPER/Ardour
or physical-controller/device tests were added here. Rebuild the relevant current
plugin formats before host validation; previously built other-format binaries
remain their old source snapshots. Apple builds/device validation remain deferred;
this lifecycle change stays in shared portable code and retains platform build
configuration for future macOS/iOS work. Existing preview archives remain labeled with their
original commits; this session creates no package or push.
