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
queue cancellation, unread success across clear/reprepare, worker OOM and
cancel/panic/DEL/MIDI Stop during a paused real allocation. `DelayEngineAudioTests`
also has explicit wrong-generation rejection and transition-busy preservation
coverage in the transaction fixture. The audio fixture
captures actual stereo Engine PCM at 8/48 kHz and 64/128, renders continuously
during preparation, and compares live `+17` edits, Feedback→exact replacement
and disabled-mode bypass to independently scheduled Delay DSP (`2e-6` limit).
No callback allocation/deallocation, UI access or locks are added. Results are
still internal APIs: no Processor/GUI/CLI/session feature is advertised as ready.

## Reproducible checks

Windows, existing core cache (PowerShell):

```powershell
$cmake = '.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = '.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
& $cmake -S . -B build/windows-core
& $cmake --build build/windows-core --config Release --target aion_delay_tests aion_delay_repeat_tests aion_delay_storage_tests aion_delay_connected_tests aion_delay_engine_tests aion_delay_engine_audio_tests --parallel 4
& $ctest --test-dir build/windows-core -C Release -R '^(delay_.*_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
```

Linux, after committing the Windows snapshot and using `SetupWslWorkspace.sh`:

```sh
cd /home/jorge/projects/Aion-Looper
cmake -S . -B build/linux
cmake --build build/linux --target aion_delay_tests aion_delay_repeat_tests aion_delay_storage_tests aion_delay_connected_tests aion_delay_engine_tests aion_delay_engine_audio_tests --parallel 2
ctest --test-dir build/linux -R '^(delay_.*_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
# Existing ASan/UBSan Debug cache, plugin builds disabled:
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_delay_tests aion_delay_repeat_tests aion_delay_storage_tests aion_delay_connected_tests aion_delay_engine_tests aion_delay_engine_audio_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(delay_.*_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
```

## Remaining before D is a working feature

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

Add all-or-nothing bank restore and Processor command/result ownership, then
shared GUI/CLI commands, MIDI Learn, versioned persistence and editor lifecycle.
Retain the connected Engine 64/128 PCM gates, then run actual GUI/CLI interaction
and listening acceptance. The Processor must cancel D via its independent path
before priority purge/delete and fence returned metadata by generation/epoch/
revision plus its existing document lifetime/correlation guards. Keep D unavailable until
these pass; do not start another effect or package this component as complete.
Broad full-workspace CPU tuning remains deferred, not silently accepted.
