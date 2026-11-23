# D / Delay implementation checkpoint

Owner priority (21 November 2026): remaining loop features precede broad CPU
tuning. This is **internal native audio/control integration**, not an enabled product
control. Existing GUI/CLI/session IDs are unchanged; no new UI control is exposed.

## Contract and acceptance

Owner request, 21 November: support **both exact repeat counts and Feedback**.
Native settings now have additive `repeatMode` (`feedback`/`exact`) and integer
`repeatCount` (1–10). Original values, ranges and presets keep their Feedback
meaning. Applying an original preset returns to Feedback; manual mode/count
edits select Custom. GUI/CLI and session serialization are still pending, so
these controls are not yet available in the app.

Exact mode uses N feed-forward stereo taps: overlapping input receives its own
complete repeat train, without a silence detector or recirculating repeat
counter. First echo is unchanged; subsequent echoes are damped with a fixed
0.65 amplitude taper per repetition. Original Reps changes Feedback mode only.
Damping has a short filter tail; an echo is not a hard-gated impulse. Count
reductions fade unwanted taps rather than abruptly cutting them. This new sound
still needs audition; it is not a claim of original audio parity.

Exact-mode storage reserves up to four seconds per prepared tap. Increasing
beyond that prepared count, or changing mode, requires a worker-prepared owner;
`set` rejects atomically instead of allocating or reinterpreting old ring PCM.
`DelayStorage` provides 20 track owners / 40 fixed slots and a default 256 MiB
ring-storage admission budget. Active, pending, cancelled and retired slots stay
reserved until the worker actually frees them. It prepares at most one request
per service call; cancellations/clear and OOM leave an existing active graph
untouched and stale completions cannot resurrect cleared tracks. These ownership
components now use LoopBank's existing worker and editable D position. Product
message-thread commands/CLI and session serialization remain pending.

## Connected loop ownership

LoopBank validates clear epochs and settings revisions before admitting a
preparation and again before adoption. Only that matching ready owner commits
settings/request metadata. A newer accepted live edit cancels the older pending
preparation; DEL, panic and stopped reprepare invalidate old work. Failures retain
the previous audible owner. A prepared replacement crossfades old/new processors
for 20ms with both pinned; its old storage remains budgeted until worker
reclamation. New replacement admission is bounded by that transition, never an
audio-thread wait. `graph()` is only a control/inspection borrow; rendering must
use the storage's shared `tick()` to advance and retire transitions.

D runs at its actual user-selected chain position. Loop settings remain live
without changing PCM/history/source position. Persistent tails obey the existing
source STOP gate. If D is removed from the chain, only an outstanding bounded
owner transition advances silently; re-inserting D resets history with a fade-in.
This prevents hidden transitions from permanently reserving an old buffer.
Synced tempo updates both pinned owners. A just-prepared graph is initialized
at the latest BPM before rendering; resetting the timeline retargets and clears
D after resetting the clock. Unchanged-BPM configurations skip retargeting.

`DelayConnectedTests` records/finalizes real native clips, uses the real worker,
and compares audible LoopBank PCM to independently scheduled Delay/Drive DSP at
8/48 kHz and 64/128 frames, tolerance `2e-6`. It covers Feedback-to-exact mode
replacement, live edits, D→T / T→D order, in-flight/transition tempo changes and
timeline reset, epoch/revision rejection, superseded
worker requests, STOP and clear. GUI, persistence, hardware and full original
browser audio acceptance remain separate; Engine checks are described below.
It also removes D while a prepared mode replacement is adopted, verifies dry/T
audio without delayed leakage, drains the hidden transition and reinserts D with
fresh history/fade-in. Windows PCM maximum observed error is `4.47035e-8` at
8 kHz and `2.98023e-8` at 48 kHz for both 64/128 cases (limit remains `2e-6`).

The unchanged original `effects.js` defines six sliders, a sync checkbox and nine
presets. `DelaySettingsReference.cjs` executes those definitions and checks them
against the independent `.reference/Ascii-Looper` checkout and native catalogue.
Values retain double precision, fractional repetitions, original ranges and
one-based preset identities. Invalid edits preserve the complete previous state.

The original factory delays stereo input, feeds delayed audio through a damped
feedback path, and auto-pans only the wet output. Repetitions maps to `/11.1`;
sync scales time by `60/BPM`. This component retains that control contract.
Derived sync time is bounded to four seconds without modifying stored settings.
It does **not** claim original WebAudio filter/transition PCM parity.

The approved organic/natural model uses fractional linear reads, stable TPT
Butterworth feedback damping and a constant-sum two-head 20ms crossfade for time
changes. Heads start at the requested delay; no zero-delay startup leak. One
latest pending request is coalesced during a fade, bounding transition work and
time to the latest target. Mix, feedback, damping, depth and bypass are smoothed.
Wet pan retains the existing verified stereo panner primitive; no cross-feedback,
noise, random drift or claimed physical tape emulation is added. Named presets
are preserved, but their new model still needs audition before product promotion.

Design references: Faust's [delay documentation](https://faustlibraries.grame.fr/libs/delays/)
describes fractional reads and crossfaded non-transposing time changes, including
the initial-head issue. [AnalogTapeModel](https://github.com/jatinchowdhury18/AnalogTapeModel)
was reviewed as an example of the much larger scope of physical tape modelling.
No third-party source was copied; no dependency was introduced.

Only `prepare` and destruction allocate/free ring storage. A failed preparation
preserves the running state. The owner must prepare/retire the component outside
audio; it is neither copyable nor movable. Reset invalidates history in constant
time, preventing stale PCM without clearing the allocation. Fully disabled DSP
returns exact input and sleeps after the bypass fade; re-enabling begins fresh.
Normal time/control edits preserve history. Coefficients are cached, control
transcendentals only run when their source value changes, and the sine oscillator
uses a recursive rotation with bounded periodic renormalization. No unbounded
event queues or per-sample allocation, locks, serialization or logging.

Focused tests require analytic integer/fractional echo timing to `1e-7`, stereo
isolation, analytic time-transition PCM to `1e-6`, exact settled bypass, no stale
reset/re-enable tail, rejected OOM preserving PCM, maximum-feedback decay, finite
rapid-edit processing and zero tracked C++ heap operations in ticks/set/reset.
Rates: 8/22.05/32/44.1/48/96/192/384 kHz; sample ticks grouped in 64/128 chunks.
These are component scheduling tests, **not connected Engine/driver buffer tests**.

## Ordered Engine controls — 22 November 2026

`enqueueDelayEdit` atomically admits a small receipt descriptor and one command
in the existing performance FIFO. D is append-only in `LoopAction`; previous IDs
are unchanged. Live controls apply at their requested sample. Mode/capacity
changes start worker preparation at that FIFO point and become successful only
when LoopBank actually adopts their matching owner at a callback boundary.
Other performance commands can continue while preparation runs. Normal loop
acknowledgements are unaffected; D uses its own typed terminal result.

There is one outstanding edit per track, including unread results. The fixed
32-slot request/result queues are heap-allocated at construction; twenty
outstanding edits fit the 31-result capacity. Admission checks both queues
before publication and leaves outputs/state untouched on failure. Transactions
never reset/reuse on reprepare. Every result carries request ID, generation,
transaction, clear epoch, settings revision and actual commit/cancel frame.
Status is ready/cancelled/stale/busy/failed. A finished result is immutable even
when DEL or prepare occurs before the message owner reads it.

Cancellation uses lock-free per-track serials, independent of FIFO capacity or
lossy telemetry. Stale/cancelled future FIFO entries cannot block a fresh edit
or complete a newer request with a reused numeric ID. Engine prepare retains
old unread receipts and ownership bounds; a later consumer gets their original
generation. STOP ALL, clear and synthetic MIDI Stop invalidate pending work.
An already-committed success is not rolled back by a later cancellation.

While disabled, metadata may select another mode/count/time without allocating
or treating an old feedback ring as input history. The old runtime fades to
bypass; enabling a different mode/capacity later follows preparation admission.
Accepted disabled state and audible bypass are tested independently.

`DelayEngineTests` covers invalid/full FIFO admission, exact sample offsets,
twenty unread receipts under telemetry saturation, reused IDs, zero-block/full-
queue cancellation, unread success across clear/reprepare, worker OOM,
cancel/panic/DEL/MIDI Stop during a paused real allocation, wrong-generation
rejection and transition-busy preservation. `DelayEngineAudioTests` captures
actual two-channel Engine PCM at 8/48 kHz and 64/128, renders continuously
during preparation, and compares live `+17` edits, Feedback→exact replacement
and disabled-mode bypass to independently scheduled Delay DSP (`2e-6` limit).
No callback allocation/deallocation, UI access or locks are added. Results are
still internal APIs: no Processor/GUI/CLI/session feature is advertised as ready.
The Engine audio fixture uses paired mono input in two-channel buffers, centre
loop pan and zero wet-pan speed. It does not establish full browser stereo/wet-
pan routing parity or all Delay parameter combinations. Preserve that distinction
when completing product audio acceptance.

## Reproducible checks

### Internal production Processor attachment (23 November 2026)

Snapshot `f8e79b6` attaches the owner to the existing serialized message service.
No-editor Timer/request service consumes D results, preserving exact confirmed
shadows. Stopped prepare drains unread ready facts/clears first. A changed-rate
reset restores D through its existing bank, with failure explicit and no automatic
allocation retry loop. This is a D-only lifecycle operation, not a coordinated
project-state load. No Delay RPC, GUI, CLI or serialization schema is exposed.

Ordinary loop admission and clear intent are atomic to prepare under the existing
non-audio mutex. Cancellation follows successful validation/admission, including
queue-independent cancellation of a future D FIFO head. Rejected barriers leave
pending work intact. DEL accepted before a prepare resets its shadow even when
the ordinary audio command has not run. Clear after a failed lifecycle restore
also discards retained intent. Release/destruction and validated state load cancel
obsolete D commands; no second receipt consumer or callback lock was introduced.

`aion_delay_processor_tests` injects commands through a native-only friend; it
invokes the production Timer callback with no editor, rather than mocking DSP or
creating a product test RPC. At 64/128 it checks two instances, exact decimals,
native targets, DEL/panic/future-head cancellation, queued DEL/prepare, unread
ready settings, 8/48/96 kHz prepare, release, invalid/full-FIFO barriers, malformed/
validated host-state cancellation and failed ring allocation without retries or
clear resurrection. Actual `processBlock` calls retain heap-operation guards.
This fixture does not compare new PCM or prove actual Timer scheduling/listening.
Existing Engine/component PCM and previous sanitizer evidence retain their scope.

Windows Release: Delay + existing Arp/effects-restore checks passed 6/6 (19.59s).
Final test-only full-FIFO/host-state additions passed Delay 2/2 (0.46s); unchanged
passing Arp/effects cases were reused after those additions. Rebuilt Standalone
passed actual `NativeUiSmoke.cjs` with master zero, no product AudioContext and
unported SAVE disabled. No new UI/bridge code or preserved asset changed.
Native WSL Linux Release rebuilt Standalone and the three affected Processor
targets; all six strict native tests pass (6.46s), including the final test
additions. No JUCE assertion/leak failure was reported. No new Processor sanitizer,
actual Linux editor interaction, listening/device or wrapper test was run. Prior
core sanitizers remain evidence for those components, not this new attachment.
No package, push, version/dependency upgrade or CPU profiling was performed.

Fixture corrections retained every assertion: the initial fixed ten-block wait
was shorter than D's 20ms crossfade at 48 kHz/64 samples, so waits now render the
actual rate-derived transition duration. MSVC's pinned xmemory adds 32-byte
alignment bookkeeping to a large vector allocation; OOM injection now targets
that exact operator-new size (including its Debug variant) rather than the user
payload. Neither correction changes DSP timing, acceptance limits or product code.

Build/check against the existing native caches (core-only caches cannot build
these JUCE targets):

```powershell
& $cmake --build build/windows --config Release --target aion_delay_processor_tests aion_arp_processor_tests aion_effects_restore_processor_tests AionLooper_Standalone --parallel 4
& $ctest --test-dir build/windows -C Release -R '^native_(delay|arp|effects_restore)_processor_(64|128)$' --output-on-failure --no-tests=error
```

```sh
cd /home/jorge/projects/Aion-Looper
cmake --preset linux -DAION_TEST_NATIVE_PROCESSOR=ON
cmake --build build/linux --target aion_delay_processor_tests aion_arp_processor_tests aion_effects_restore_processor_tests AionLooper_Standalone --parallel 2
ctest --test-dir build/linux -R '^native_(delay|arp|effects_restore)_processor_(64|128)$' --output-on-failure --no-tests=error
```

Enable `-DAION_TEST_NATIVE_PROCESSOR=ON` in the Windows cache if absent. These
tests fail on JUCE assertions/leaks and do not build/install deferred wrappers.
The next step is coordinated D + existing Arp/effects-bank preparation/adoption
with all-or-nothing failure/cancellation. Separate successful banks do not prove
that contract. Shared GUI/CLI/state/MIDI and product sound acceptance follow it.

### Exact message-owned settings (23 November 2026)

`DelaySettingsOwner` (`c4c986e`) is the sole non-audio D producer/receipt consumer
for an Engine. At that earlier adapter-only checkpoint it was not attached to
the Processor; the newer section above supersedes that limitation. It supports control/preset/
enable/repeat candidates and D-only banks, with committed settings kept distinct
from pending intent. Only exact terminal ownership receipts commit settings;
global loop acknowledgement IDs and stale snapshots are not success evidence.

Clear after unread ready resets only that track; other bank members keep their
confirmed values. A late cancellation does not erase a completed edit. Old ready
values survive reprepare as historical facts while binding is invalidated until
known settings are verified against current audio. Failures/queue rejection keep
prior originals; unexplained external revisions detach rather than guess. No
JSON, parsing, allocation or new DSP path is added to processing.

`delay_settings_owner_contract` uses the real worker/Engine at 64/128 for decimal
precision, legacy-preset/Feedback semantics, exact mode replacement, cancellation/
request-ID reuse, full-FIFO rollback, OOM and recovery, twenty-track bank exclusion,
clear/unread receipts and same-/changed-rate binding. Audio callbacks retain heap
guards. The build commands below include `aion_delay_owner_tests`; the existing D
CTest regex selects it. Windows D/reference 10/10 passed (18.07s), native WSL
Linux Release 10/10 (2.76s), Linux Debug ASan/UBSan/leak 10/10 (24.64s), with no
reported sanitizer errors or suppressions. These durations are not benchmarks.

The initial fixture failed by waiting for an ordinary enable command using only
zero-sample callbacks. Those callbacks intentionally do not drain ordinary UI
commands. Dispatch now uses one nonzero callback before zero-frame receipt polling;
all original failure/cancellation assertions remain. This is a fixture correction,
not a production transport change. Existing PCM comparisons remain at their
unchanged limits; owner tests do not add listening/browser/device evidence.

Next: attach under Processor lifetime/intent ownership and coordinate with its
existing Arp-backed effects restore. GUI/CLI/MIDI/persistence and product/editor
acceptance remain pending. No routine wrapper build, package or remote push.

### Atomic D-only bank acceptance (22 November 2026)

`enqueueDelayBank` admits one bank plus a small ordered FIFO marker, without
putting twenty settings into every ordinary loop command. Individual D edits and
banks exclude each other until their receipts are consumed. Storage validates all
members and reserves aggregate old/new ring capacity before publishing work.
Enabled processors prepare on the existing worker; no member adopts early.
All twenty metadata revisions and owners commit at one callback boundary after
the requested FIFO deadline. All-off banks need no allocation but still commit
at that boundary, fading previous owners to dry over 20ms. Retired bytes count
against the budget until actual worker reclamation.

The bank has independent cancellation and immutable generation/transaction/
epoch/revision receipts. Partial preparation, worker failure and stale completions
cannot publish a partial target. Clear/panic/reset retain their intended behavior
while cancelling the pending whole bank. An unread receipt survives reprepare;
a fresh bank cannot steal it. This protocol covers Delay only: coordinating it
with existing Q/C/A/T/F/O/chain/PCM session restore remains Processor work.

`delay_atomic_bank_contract` checks all twenty members, partial-ready PCM rollback,
aggregate budgets, all-off retirement, full FIFO rejection, stale identities,
unread/queued reprepare and cancellation during a paused second allocation.
`delay_atomic_bank_audio_contract` captures actual loops 0 and 19 and compares
continuous Engine PCM against independent Delay owners. It checks atomic old/new
snapshots, Feedback/exact replacement, off transitions and unchanged clip identity,
length, cursor and Undo state at 8/48 kHz with 64/128 callbacks. Maximum Windows
error was 7.45058e-9 against the unchanged 2e-6 limit; both tracks use paired mono,
centre pan and zero wet-pan speed. This does not establish browser, full stereo,
product UI/CLI, persistence, listening or physical-device acceptance.

Add `aion_delay_bank_tests aion_delay_bank_audio_tests` to each build command
below; the existing Delay CTest regex includes both new cases. Source snapshot:
`401ce58`. No callback heap operations were observed by the test guards; ASan is
not a race detector. No distributable or remote push was produced.

Executed against that snapshot: native WSL Linux Release selected regressions
14/14 (7.59s), focused core `--loop-edits` and `--midi` both passed. Linux Debug
ASan/UBSan with leak detection passed Delay 9/9 (25.57s), with no reported errors
or suppressions. These are functional test durations, not runtime benchmarks.
Windows Release selected regressions passed 14/14 (38.33s). No original cases or
tolerances were removed or relaxed. Reference and preserved UI assets are unchanged.
Windows focused core `--loop-edits` and `--midi` also passed. The earlier manually
interrupted exhaustive core runs remain incomplete; they were not repeated here.

Windows, existing core cache (PowerShell):

```powershell
$cmake = '.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = '.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
& $cmake -S . -B build/windows-core
& $cmake --build build/windows-core --config Release --target aion_delay_tests aion_delay_repeat_tests aion_delay_storage_tests aion_delay_connected_tests aion_delay_engine_tests aion_delay_engine_audio_tests aion_delay_bank_tests aion_delay_bank_audio_tests aion_delay_owner_tests --parallel 4
& $ctest --test-dir build/windows-core -C Release -R '^(delay_.*_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
```

Linux, after committing the Windows snapshot and using `SetupWslWorkspace.sh`:

```sh
cd /home/jorge/projects/Aion-Looper
cmake -S . -B build/linux
cmake --build build/linux --target aion_delay_tests aion_delay_repeat_tests aion_delay_storage_tests aion_delay_connected_tests aion_delay_engine_tests aion_delay_engine_audio_tests aion_delay_bank_tests aion_delay_bank_audio_tests aion_delay_owner_tests --parallel 4
ctest --test-dir build/linux -R '^(delay_.*_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
# Existing ASan/UBSan Debug cache, plugin builds disabled:
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_delay_tests aion_delay_repeat_tests aion_delay_storage_tests aion_delay_connected_tests aion_delay_engine_tests aion_delay_engine_audio_tests aion_delay_bank_tests aion_delay_bank_audio_tests aion_delay_owner_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(delay_.*_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
```

To create a fresh sanitizer cache rather than reuse the existing one:

```sh
cmake -S . -B build/drive-sanitized -DAION_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
```

The affected existing-owner regression command uses the same cache and targets:
`aion_arp_preparation_tests aion_arp_owner_tests aion_arp_owner_bank_tests
aion_drive_connected_tests aion_signal_chain_tests aion_core_tests`. After
building those, extend the CTest regex to
`^(delay_.*_contract|original_delay_settings_contract|arp_preparation_contract|arp_owner_contract|arp_owner_bank_contract|drive_connected_contract|signal_chain_connected_contract)$`.
Run the built core executable with `--loop-edits` and `--midi` separately.
Windows uses `build/windows-core/Release/aion_core_tests.exe`; Linux uses
`./build/linux/aion_core_tests`. Do not replace these focused commands with an
unqualified core invocation during iteration.

## Remaining before D is a working feature

Engine-stage evidence, 22 November 2026: code snapshot `359ffd4`, strengthened
busy/generation regression snapshot `12f1453`:

- Windows Release selected regressions: 12/12 (30.95s). Incremental strengthened
  Engine transaction fixture: 1/1 (2.24s).
- Native WSL Linux Release selected regressions: 12/12 (7.00s). Incremental
  strengthened Engine transaction fixture: 1/1 (0.19s).
- Final Linux ASan/UBSan Debug/leak Delay checks: 7/7 (19.90s), no reported errors
  or suppressions. The cache flags were independently confirmed as Debug,
  plugin builds off, address+undefined instrumentation and frame pointers.
- Final Windows and Linux `--loop-edits` and `--midi` pass. These protect existing
  capture/reverse/history/Multiply/Start/Merge plus MIDI routing/timing/mute/
  held-gesture recovery. No assertion/tolerance/old case was removed or loosened.
- Actual Engine Delay audio maximum error: `1.49012e-8` at 8 kHz and `7.45058e-9`
  at 48 kHz, for 64/128. Limit remains `2e-6`; nonzero audible effect and exact
  settled disabled bypass are asserted. All new callback fixtures retain zero
  tracked C++ heap operations, including synthetic MIDI Stop during preparation.
- Only the changed transaction fixture was repeated after its test-only busy/
  generation addition; unchanged passing normal suites were reused. The full
  seven-case sanitizer set includes the strengthened fixture.
- No Standalone/GUI/CLI, physical device, panned-stereo/browser, listening or
  plugin-wrapper acceptance was executed. No package, push or CPU profiling.
  Prior interrupted exhaustive core runs remain incomplete; this stage did not
  rerun or claim them as passing.

Latest connected evidence, source/test snapshot `bd2e34d`, 21 November 2026:

- Final Windows Release Delay CTest: 5/5, 6.10s.
- Final native WSL Linux Release Delay CTest: 5/5, 1.18s.
- Linux ASan/UBSan Debug with leak detection: 5/5, 12.71s; no reported
  sanitizer errors or suppressions.
- Final Windows and Linux `aion_core_tests --loop-edits` pass capture/edit,
  reverse, history, Multiply, Start and Merge assertions, including their
  allocation/ownership guards. These existing cases were not removed or relaxed.
- Existing drive, Arp and signal-chain connected regressions pass in the broader
  selected runs. Those runs were **not 9/9 passes**: the exhaustive
  `core_realtime_contract` processes were manually stopped at 796.17s on Windows
  (pre-final tempo/bypass version) and 342.69s on Linux. CTest records the stopped
  cases as failed/terminated. No assertion failure was observed before stopping;
  no cause or CPU-speed conclusion is inferred. Full core-suite completion
  remains an integration acceptance gap; use focused checks during this slice.
- No product Standalone/GUI/CLI/listening/hardware test or wrapper build was
  executed for this internal integration; no package or remote push.

The storage fixture now renders its initial 20ms fade (160 zero frames at 8 kHz)
before requesting another owner. This reflects the new bounded admission rule,
not a removed failure case: all original OOM/PCM/cancel/exhaustion checks remain.
Added independent every-sample dry/new and old/new crossfade oracles, clear during
fade, and proof that retired bytes remain reserved until worker reclamation.
No tolerances were loosened. No ownership/destruction work moved onto audio.

For the next edit, use the five Delay cases above plus affected existing cases
and `aion_core_tests --loop-edits` when source/history paths change. Do not make
another exhaustive suite/profile run the default prerequisite to command wiring.

Latest executed checks, source snapshot `5707e01`, 21 November 2026:

- Windows Release: four focused checks passed, 0.88s.
- Native WSL Linux Release: four focused checks passed, 0.88s.
- Linux ASan/UBSan Debug with leak detection: four focused checks passed, 5.65s;
  no reported sanitizer errors or suppressions. An initial Makefile multi-target
  invocation did not see newly generated targets; explicit CMake regeneration
  followed by the documented build fixed the invocation, then all checks ran.
- Exact-count tests cover 1/2/3/10 at 8/32/48 kHz with 64/128 tick grouping,
  independently overlapping input trains, no extra recirculated echo, legacy
  feedback, capacity rejection, OOM rollback and zero tracked audio heap work.
- Ownership tests cover 20 tracks, 40-slot exhaustion/recovery, ring-budget
  boundaries, active PCM preservation, stale completions, clear during a real
  worker allocation, rate reset and worker-only retirement. ASan is not a data
  race detector; this is not a claim of ThreadSanitizer or full Engine acceptance.

No UI/CLI, session persistence, connected Engine, listening or hardware checks
were run for these internal changes. No package or remote push.

Earlier component evidence, source snapshot `e75c210`:

- Windows Release: focused CTest 2/2, 0.48s.
- Native WSL Linux Release: focused CTest 2/2, 0.65s.
- Linux ASan/UBSan Debug with leak detection: focused CTest 2/2, 3.78s;
  no suppressions or reported sanitizer errors.
- DSP fixture counts 1,126,144 rate/segmentation/stress samples, plus analytic
  transition, wrapped-history, bypass and feedback-decay cases; tracked
  callback C++ heap operations: zero. No runtime performance gain is claimed.
- Diff whitespace check passes; original reference/presentation files unchanged.

No product UI, CLI, connected Engine, device, host or listening test was run for
this internal component. No distributable produced or remote push performed.

Complete Processor command/result ownership and coordinated session-bank restore, then
shared GUI/CLI commands, MIDI Learn, versioned persistence and editor lifecycle.
Retain the connected Engine 64/128 PCM gates, then run actual GUI/CLI interaction
and listening acceptance. The Processor must cancel D via its independent path
before priority purge/delete and fence returned metadata by generation/epoch/
revision plus its existing document lifetime/correlation guards. Keep D unavailable until
these pass; do not start another effect or package this component as complete.
Broad full-workspace CPU tuning remains deferred, not silently accepted.
