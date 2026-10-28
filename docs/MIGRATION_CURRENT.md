# Current migration checkpoint

Updated 2026-10-28. Current repair baseline: `c6bce80` (clean main, two local
commits ahead of origin). STOP NOW and STOP ALL evidence remains applicable.
Continue the existing implementation; this is not a restart or release checklist.

## Reference and baseline

The requested `D:\Jorge\Documents\Codex\Aion-Looper.reference` path is absent.
The available original is `.reference/Ascii-Looper` inside this repository.
`tests/AssetTests.py` verifies embedded original UI bytes against that copy.
Neither reference nor original UI files are modified by this repair.

The baseline evidence is `build/post-grid-stop-now-evidence.json`: Windows/Ubuntu
portable 28/28 each, native 2/2 each, core sanitizer 6/6, 96 queue-delay cases,
126 committed-PCM cases. A fresh focused baseline passed before the new normal
STOP regression was added; its reproduced failure is recorded below. Historical
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
| Full instruments/effects/controller/persistence/host parity | No new acceptance from this session; see PARITY.md and feature-specific validation notes | Do not infer suite completion from these looper/meter gates |

## Current slice and acceptance

Repair normal **STOP after sync is disabled**, with a pending meter intent after
audible grid. The immediate fallback of `LoopAction::stop` still used native state
`queued`; it could strand a graphless message owner or publish an immediateStop
against an older graph while retaining the audio-side queue ID. Select the pending
ID, as STOP NOW already does. Keep the synchronized scheduled-stop branch,
generic audible stop, graph births, UI, parameters and state unchanged.

Acceptance: after an actual queued-to-playing audio transition with an independent
message owner still queued, deliver one exact-ID queuedStop; graphless owners
tear down without a timer, attached owners replace old cleanup with 25ms and
retain analyser/data/history. Reject wrong IDs, duplicates and obsolete cleanup;
a following STOP ALL must emit no orphan cancellation. Exercise tracks 0/19,
64/128, exact paired stereo PCM, production processor requests and a real JUCE
timer with a frozen audio clock. Characterize the unchanged original stop()
method in four additional late cases, retaining all earlier reference tests.
The attached original fixture supplies the pending state explicitly; it proves
stop behavior, not the still-unaccepted origin of a late queued graph.

The focused baseline passed, then the extended test reproduced
`Pending unsynced STOP must emit queuedStop after audible grid` before the
production edit (`build/unsynced-stop-before.txt`). The original stop() fixture
now also disables sync after the grid before the explicit immediate stop; four
new cases retain the previous four STOP NOW cases and all earlier counts.

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
Current sync-disabled normal STOP evidence is `build/unsynced-stop-evidence.json`:
Windows portable 377.14s, Ubuntu portable 597.64s, native 12.46s/22.81s,
sanitizer 277.79s. `build/post-grid-stop-now-evidence.json` retains STOP NOW's
preceding acceptance; its original cases remain enabled.
Both current Standalone binaries rebuilt. See VALIDATION.md for scoped results;
`build/post-grid-panic-evidence.json` retains the preceding STOP ALL evidence.

Next validate normal synchronized STOP after audible grid and the old-teardown
state boundary against the reference. Original `scheduleStop()` requires message
state playing and a graph; native audible playing with an undelivered callback
does not prove those conditions. This immediate-fallback repair does not cover
that scheduled policy and must not change audible timing to guess it.
Then implement the queued message callback as an explicit graph transaction with
conditional raw-reset acceptance/rejection recovery. Establish a valid wall/audio
clock relationship before wiring its 100ms timer; retain independent old cleanup
and late-callback behavior. Do not infer birth from a raw receipt or audio state.
Only then integrate meter observation and the renderer.

Full transition sound/late-source alignment is still unaccepted. No REAPER/Ardour
or physical-controller/device tests were added here. Rebuild the relevant current
plugin formats before host validation; previously built other-format binaries
remain their old source snapshots. macOS/iOS implementation and validation are
explicitly deferred by the current user request. Existing preview archives remain labeled with their
original commits; this session creates no package or push.
