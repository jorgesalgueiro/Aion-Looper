# Current migration checkpoint

Updated 2026-10-28. Current repair baseline: `3ad30bc` (clean main, one local
commit ahead of origin); previous STOP ALL evidence below remains applicable.
Continue the existing implementation; this is not a restart or release checklist.

## Reference and baseline

The requested `D:\Jorge\Documents\Codex\Aion-Looper.reference` path is absent.
The available original is `.reference/Ascii-Looper` inside this repository.
`tests/AssetTests.py` verifies embedded original UI bytes against that copy.
Neither reference nor original UI files are modified by this repair.

The baseline evidence is `build/queued-delay-evidence.json`: Windows/Ubuntu
portable 28/28 each, native 2/2 each, core sanitizer 6/6, 96 queue-delay cases,
126 committed-PCM cases. Fresh focused baseline panic tests passed on both
platforms. A new regression then failed on the old implementation:
`Late STOP ALL must publish cancellation for the still-pending queued intent after audible grid`.
The failure is retained in `build/post-grid-panic-before.txt`.

## Compact parity checklist

| Reference behavior | Native status / evidence | Remaining gap |
| --- | --- | --- |
| Capture and accepted loop edits preserve committed PCM | Existing scoped audit: 126 capture/OD/SUB cases at 1e-6; other loop-edit gates retained | Full browser envelopes/resampling and physical input timing are separate |
| Queue callback uses max(0, grid delta minus 100ms) | Accepted unrounded metadata; 96 actual Engine/original grid+dispatcher cases retained | Actual message callback, graph transaction and wall alignment remain unwired |
| STOP ALL cancels a still-queued intent even after its grid time | This repair routes queuedCancel by explicit pending ID; core, production processor, 20-route and real-timer regressions | This does not fix audible playback occurring before a late message callback |
| STOP NOW clears pending queue before immediate graph cleanup, including after grid | Explicit queue-ID routing; graphless/attached, production requests and real 25ms timer checks | Normal synchronized STOP and old-teardown state boundary remain separate |
| Queued cancellation retains older analyser/data/history and cleanup | Existing owner/adapter policy retained, including original pre-grid test plus added post-grid frozen-clock test | If old cleanup already changed message state out of queued, existing rejection remains intentional pending full mapping |
| Audible grid does not prove message graph birth | Graph origin remains quarantined; no new graph, reset or 65ms cleanup inferred | Visible meters, remaining capture/edit graph origins, overlap and continuity remain unfinished |
| Full instruments/effects/controller/persistence/host parity | No new acceptance from this session; see PARITY.md and feature-specific validation notes | Do not infer suite completion from these looper/meter gates |

## Current slice and acceptance

The next bounded repair is pending **STOP NOW** after audible grid. Its old
`stopImmediate` branch selected queuedStop only from native state `queued`;
native `playing` could leave a graphless message intent stranded, or publish an
immediateStop against an older graph while retaining the audio-side queue ID.
Select the explicit pending meter queue ID, just as STOP ALL now does. Generic
audible stop, scheduled STOP, graph births, UI, parameter IDs and saved state
are unchanged.

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

Existing focused baseline passed, then the new test reproduced
`Late STOP NOW must emit queuedStop for the pending intent after audible grid`
before the production edit (`build/post-grid-stop-now-before.txt`).

Previous slice:

Repair only STOP ALL's selection of a pending queued cancellation. The old
selection used audio state `queued`; after audible grid it emitted no cancellation
while the independent message owner was still queued. Select the existing pending
queue ID instead. Keep generic audio stop, raw windows, origin quarantine, bounded
FIFO/dispatcher, parameter IDs, bridge schema and serialized state unchanged.

Accept only if exact pending IDs cancel before/after grid while the message owner
is still queued, stale/duplicate/repeated
facts reject, delayed intent+cancel receipts remain ordered, all twenty routes
retain bounded 7/13 delivery, and the original cleanup/history remain intact.
Retain original JS cases and add four late-panic cases. Require exact paired
64/128 stereo PCM and zero tracked callback new/delete. Full build/test results
are recorded in VALIDATION.md and `build/post-grid-panic-evidence.json`.

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
Current STOP NOW evidence is `build/post-grid-stop-now-evidence.json`: Windows
portable 371.82s, Ubuntu portable 619.28s, native 11.16s/23.92s, sanitizer 290.04s.
Both current Standalone binaries rebuilt. See VALIDATION.md for scoped results;
`build/post-grid-panic-evidence.json` retains the preceding STOP ALL evidence.

Next validate normal synchronized STOP after audible grid and the old-teardown
state boundary against the reference; the STOP NOW repair does not cover them.
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
