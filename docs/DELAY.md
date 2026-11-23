# D / Delay implementation checkpoint

Owner priority (21 November 2026): remaining loop features precede broad CPU
tuning. D now has a **development GUI/CLI preview**; MIDI Learn, listening/device
and full release acceptance remain separate. Existing IDs and preserved UI assets
are unchanged. Older checkpoints below retain their original narrower scope.

## Contract and acceptance

### Shared Delay controls — 23 November 2026

Snapshots `921788e` / `4018039` connect `operation: delay` and the running-app
`delay` CLI command to `DelaySettingsOwner`. Original Time/Reps/Damp/Mix/Pan/Sync
controls, nine presets, bypass/toggle and reset share one native DSP/settings
implementation and versioned persistence. The original-style D panel and main/
LIVE row toggles are enabled. Added repeat selectors expose Feedback and exact
1–10; reset preserves enabled/bypass while restoring default controls and
Feedback/count 1. No product AudioContext or new DSP model is introduced.

Commands validate track, generation, bounded deadline, action, strict value types
and ranges before admission. One edit per track stays outstanding until the
exact receipt is consumed; snapshots report `delayRequestId`, `delayPending`,
`delayReady` and `delayBusy`. Accepted intent is saved immediately; rejection,
cancellation and preparation failure roll back to confirmed settings. GUI/CLI
never use a larger global acknowledgement as success or retry an admitted action.
Tagged WebView Delay fields use the existing 17-digit JSON transport technique
to retain original double precision, with document-token/lifecycle guards intact.

The extended actual UI test exposed rapid repeat-count edits racing the 20ms
owner crossfade. Unsent repeat edits now coalesce; audio-owned transition
readiness is included in the existing bounded track snapshot. GUI/CLI wait for
it before the next admission. A revision fence also handles a receipt arriving
before its corresponding snapshot. Raw mutations reject while that readiness
is stale/busy; core admission limits, crossfades and failure rollback are unchanged.
No callback timers, allocations or locks are added.

Public native command tests cover exact receipts, expanded-track isolation,
double precision/immediate state save, reset/bypass, invalid/expired commands,
and an external Python process using the actual mailbox and production Processor.
The UI fixture uses actual CDP pointer/key input with production synthetic PCM,
not a control mock. Original reference resources and the user's logo are untouched.
Commands and semantics are documented in [CLI.md](CLI.md).

Executed final snapshot checks:

- Windows Release: native commands/external CLI plus state/joint/lifecycle/Arp/
  FX restore passed 12/12 at 64/128 (47.47s); Standalone and hardware-free mouse
  host rebuilt. Direct Node frontend and extended bridge lifecycle tests passed.
- Native WSL Linux Release: the same twelve cases plus frontend/bridge passed
  14/14 (27.28s); Standalone rebuilt from `4018039` in native WSL storage.
- Actual Windows editor at both 64/128: `tools/TestDelayUi.ps1` passed all original
  Delay ranges, Sync, preset, Feedback/exact 1/2/3/10, reset, exact bridge precision,
  reload/bypass and continuing nonzero synthetic native PCM with unchanged clip
  identity. Runtime exception collection remains active through reload. This is
  synthetic production audio and real CDP input, not physical-driver/listening proof.
  Log: `build/windows/delay-transition-ui.log`.
- Actual rebuilt Windows Standalone `NativeUiSmoke.cjs` passed, master volume
  zero, original card/bridge/lifecycle guards intact and no legacy AudioContext.
  An earlier run alongside the editor test failed the existing 100ms metronome
  flash-colour assertion; the final isolated run passed the unmodified check.
  No metronome code or threshold was changed; do not claim the concurrent failure
  passed or a proven cause. Logs: `delay-command-smoke.log` (failure) and
  `delay-transition-smoke.log` (pass), both under `build/windows`.
- Tracked C++ callback allocations/deallocations remain zero. No new sanitizer,
  broad CPU profile, physical controller, Linux WebKit, Apple or wrapper run.
  Existing original-JS/connected PCM/storage/sanitizer evidence below is reused;
  this slice changes commands/readiness, not DSP math or tolerances.

Reproduce (pinned Windows CMake/CTest, configured native-test cache):

```powershell
cmake --build build/windows --config Release --target aion_delay_command_tests aion_delay_state_processor_tests aion_joint_effects_processor_tests aion_delay_processor_tests aion_arp_processor_tests aion_effects_restore_processor_tests AionLooper_Standalone aion_mouse_host --parallel 4
ctest --test-dir build/windows -C Release -R '^(native_delay_commands_(64|128)|native_(delay_state|joint_effects|delay|arp|effects_restore)_processor_(64|128))$' --output-on-failure --no-tests=error
node tests/FrontendTests.cjs
node tests/BridgeTests.cjs
./tools/TestDelayUi.ps1
```

After committing/syncing with SetupWslWorkspace.sh as documented in
DEVELOPMENT_WORKFLOW.md:

```sh
cd /home/jorge/projects/Aion-Looper
cmake --preset linux -DAION_TEST_NATIVE_PROCESSOR=ON
cmake --build build/linux --target aion_delay_command_tests aion_delay_state_processor_tests aion_joint_effects_processor_tests aion_delay_processor_tests aion_arp_processor_tests aion_effects_restore_processor_tests AionLooper_Standalone -j2
ctest --test-dir build/linux -R '^(native_delay_commands_(64|128)|native_(delay_state|joint_effects|delay|arp|effects_restore)_processor_(64|128)|native_frontend_commands|native_bridge_lifecycle)$' --output-on-failure --no-tests=error
```

Next: D MIDI Learn through the shared owner and listening acceptance, retaining
existing PCM/reference/ownership gates. No physical controller/driver, Linux
WebKit, Apple or deferred-wrapper claim; no routine package/push or broad CPU tuning.

### Versioned native persistence — 23 November 2026

Source snapshot `af7f7e1` adds a `LoopDelays` version-1 bank to JUCE binary state.
Each of twenty `Delay` rows has `track`, `enabled`, `preset`, `repeatMode`
(`feedback`/`exact`), `repeatCount` (1–10), and the seven original named controls.
Double values use `max_digits10` strings to preserve original precision through
XML. Preset indexes retain their existing meaning (0 Custom, 1–9 original).
This versions D independently, leaving APVTS IDs and every other bank schema
unchanged. The native format is not an importer for original browser projects.

Missing D banks in pre-D native sessions load default controls with D disabled,
matching original missing-effect defaults. Present banks require exactly one
supported version and twenty unique valid IDs/rows, all required fields, finite
in-range values, valid repeat modes/counts and consistent named presets. Nested
rows, duplicates, incomplete banks and unsupported versions are rejected. All
banks validate before MIDI admission, cancellation, settings or APVTS mutation.
The existing bounded 1 MiB state-size guard remains.

`saveDelays` drains the sole non-audio owner's edit receipts and preserves accepted
load/clear intent. A validated load publishes its D candidate into the existing
coordinated FX transaction, retiring obsolete D work through that same owner.
Older FIFO-clear acknowledgements advance ownership epochs without erasing a
newer load's candidate. A newer GUI clear already resets its own candidate.
Missing-bank restore deliberately resets D off rather than retaining an unrelated
previous session's internal D settings.

`DelayStateProcessorTests.cpp` uses public binary save/load (friend access only
observes completion and invokes existing message service). It verifies exact
original decimal/preset/mode/count roundtrips, immediate accepted-intent save,
66 whole-state rejection cases per block size, preservation of an admitted valid
load after a malformed load, independent instances, stopped load/prepare,
8-to-48k prepare, legacy defaults, queued older DEL/newer load, post-adoption DEL,
and supersession with unread/saturated telemetry. Real callbacks count zero
C++ heap operations. No new audio-thread code or DSP math is introduced.

Initial tests also reproduced the existing busy refusal when a new bank arrives
during a previous 20ms owner transition. Ordinary roundtrip cases now drain that
transition before beginning; a separate real zero-frame admission case keeps the
audio clock still to prove deterministic busy rejection, unchanged old settings
and explicit later recovery at both 64/128. This is a documented safety/admission
limit, not silently retried success. No production admission policy, assertion or
audio tolerance was weakened to pass the tests.

Executed on that snapshot:

- Windows Release: final new state cases 2/2 (2.27s), followed by the rebuilt
  joint/Delay/Arp/existing FX-restore cases 8/8 (21.09s). These were separate
  focused runs, not one invented ten-case run. Standalone rebuilt successfully.
- Native WSL Linux Release: all ten affected Processor cases passed 10/10
  (9.53s), with Standalone rebuilt from the committed snapshot.
- Actual rebuilt Windows Standalone `NativeUiSmoke.cjs` passed: original loop
  cards, IPC validation/acknowledgements, disabled unported controls and no
  legacy AudioContext, master volume zero. This is compatibility smoke, not
  interaction/audition acceptance for the still-disabled D controls. The exact
  WebView debug launch recipe is retained in the joint-restore checkpoint below;
  this run's output is `build/windows/delay-state-ui-smoke.log`.
- No new sanitizer run for this non-audio JUCE state-parser slice. Prior core
  ownership/DSP sanitizer and unchanged-JS PCM evidence is retained at its
  recorded scope; no fresh audio model, listening, physical device, Linux
  WebKit interaction, wrapper, Apple or CPU-tuning acceptance is claimed.

Focused commands (existing configured Windows cache with native tests enabled):

```powershell
cmake --build build/windows --config Release --target aion_delay_state_processor_tests aion_joint_effects_processor_tests aion_delay_processor_tests aion_arp_processor_tests aion_effects_restore_processor_tests AionLooper_Standalone --parallel 4
ctest --test-dir build/windows -C Release -R '^native_(delay_state|joint_effects|delay|arp|effects_restore)_processor_(64|128)$' --output-on-failure --no-tests=error
```

Use the pinned CMake/CTest binaries under `.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin`
where CMake is not on PATH. Commit the Windows snapshot and sync using
`tools/SetupWslWorkspace.sh` as documented in DEVELOPMENT_WORKFLOW.md, then:

```sh
cd /home/jorge/projects/Aion-Looper
cmake --preset linux -DAION_TEST_NATIVE_PROCESSOR=ON
cmake --build build/linux --target aion_delay_state_processor_tests aion_joint_effects_processor_tests aion_delay_processor_tests aion_arp_processor_tests aion_effects_restore_processor_tests AionLooper_Standalone -j2
ctest --test-dir build/linux -R '^native_(delay_state|joint_effects|delay|arp|effects_restore)_processor_(64|128)$' --output-on-failure --no-tests=error
```

Next: shared product GUI/CLI commands and original-style Delay controls, then
applicable MIDI/interaction and listening acceptance. The controls remain
disabled; full Delay feature acceptance, PCM/project-file persistence and
whole-project/APVTS/audio atomicity are not claimed by this native settings bank.
Keep prior core PCM/reference/ownership evidence at its documented scope.

### Processor coordinated restore — 23 November 2026

Source snapshot `0ea9da9` makes the existing Processor FX owner submit the joint
Arp/D/Q/C/T/F/O/chain bank, including device-lifecycle D restoration. Its retained
D candidate, before-epochs/revisions and exact Arp receipt identify one admitted
transaction. `DelaySettingsOwner::acceptEffectsRestore` validates all twenty
tracks before changing any record. Generation and callback-serial fencing reject
stale telemetry; matching knob values alone cannot bind a newer joint revision.
Later clears reset only their own tracks, and proven newer owned post-clear edits
remain intact. No second consumer drains the Arp receipt queue.

Superseding accepted loads keep their new shadows while obsolete receipts retire.
Failed ordinary loads restore confirmed settings without retrying allocation on
each Timer tick. A failed changed-rate lifecycle restore retains explicit D intent
for a later prepare. Ready-bank ordering before STOP/DEL is preserved; a still-
preparing joint bank cancels on audio. The first regression run exposed premature
message-side D cancellation disrupting this ordering; it was fixed before retesting.
The old host-load test's cancelled D status became a newer successful joint
receipt: its replacement checks unchanged saved/native values, bound state and
exact matching joint request ID. Cancellation-only cases remain. No numeric
tolerance or DSP assertion was weakened.

Executed verification:

- Windows Release: eight native Processor cases passed, 20.46s; extended core
  owner contract passed, 0.69s; Standalone rebuilt.
- Native WSL Linux Release: the owner contract plus eight native Processor cases
  passed 9/9, 8.02s; Standalone rebuilt from that committed Windows snapshot.
- Linux Debug ASan/UBSan, leak detection: extended owner contract passed 1/1,
  0.74s, with no reported sanitizer error or suppression. This is core-owner
  sanitizer coverage, not a sanitized JUCE Processor run.
- Actual Windows Standalone WebView2 `NativeUiSmoke.cjs` passed: existing ASCII
  cards, bridge, acknowledgement/validation and disabled unported controls,
  master volume zero, no product AudioContext. This is separate from the native
  injected race fixture and does not expose or audition D.

`JointEffectsProcessorTests.cpp` uses the production Processor and actual 64/128
callbacks: exact receipt matching, same-value revision protection, stale/fresh
snapshots, clear after adoption, paused real ring allocation and superseding
intent, real allocation failure/rollback/no retry, prepare recovery and actual
message-Timer service with no editor. Tracked callback new/delete counts are zero.
The extended core-owner regression also covers malformed last-track evidence,
all-or-none confirmation, post-clear edit ownership and generation rejection.
Existing unchanged-JS/core PCM and storage evidence below is reused because DSP
and the core bank protocol did not change. No fresh listening/device, Linux
WebKit interaction, wrapper, Apple or broad CPU test was performed.

Reproduction (PowerShell; use the pinned CMake/CTest executables on Windows):

```powershell
cmake --build build/windows-core --config Release --target aion_delay_owner_tests --parallel 4
ctest --test-dir build/windows-core -C Release -R '^delay_settings_owner_contract$' --output-on-failure --no-tests=error
cmake --build build/windows --config Release --target aion_joint_effects_processor_tests aion_delay_processor_tests aion_arp_processor_tests aion_effects_restore_processor_tests AionLooper_Standalone --parallel 4
ctest --test-dir build/windows -C Release -R '^native_(joint_effects|delay|arp|effects_restore)_processor_(64|128)$' --output-on-failure --no-tests=error
wsl -d Ubuntu --exec bash /mnt/d/Jorge/Documents/Codex/Aion-Looper/tools/SetupWslWorkspace.sh /mnt/d/Jorge/Documents/Codex/Aion-Looper
```

In the clean native WSL snapshot:

```sh
cd /home/jorge/projects/Aion-Looper
cmake --preset linux -DAION_TEST_NATIVE_PROCESSOR=ON
cmake --build build/linux --target aion_delay_owner_tests aion_joint_effects_processor_tests aion_delay_processor_tests aion_arp_processor_tests aion_effects_restore_processor_tests AionLooper_Standalone -j2
ctest --test-dir build/linux -R '^(delay_settings_owner_contract|native_(joint_effects|delay|arp|effects_restore)_processor_(64|128))$' --output-on-failure --no-tests=error
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_delay_owner_tests -j2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^delay_settings_owner_contract$' --output-on-failure --no-tests=error
```

The sanitizer command reuses the existing core-only Debug ASan/UBSan cache; it
does not enable plugins. For UI smoke, launch the rebuilt Windows Standalone
with `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS` set to
`--remote-debugging-address=127.0.0.1 --remote-debugging-port=9234 --disable-background-timer-throttling --disable-renderer-backgrounding`,
then run `node tests/NativeUiSmoke.cjs`. Reserve a free debug port and close only
the test-owned app afterwards. Build/UI logs are under `build/windows/joint-effects-*`;
Linux build logs use `build/linux/joint-owner-*`.

Next: versioned D persistence and shared GUI/CLI commands, followed by applicable
MIDI/interaction and listening acceptance. Existing host state preserves internal
D intent but does not yet serialize D fields. Existing stopped Q/C/drive prepare
initialization is unchanged; no full APVTS/PCM-project atomicity claim is made.
D stays disabled in product controls. No package, push, wrapper or dependency
change. Earlier checkpoints below retain historical scope and former next steps.

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

### Coordinated core Arp/Delay/effects commit (23 November 2026)

Snapshot `c5a77cb` extends the existing Arp bank with opt-in D settings/revisions.
The default flag is false for legacy Arp-only and existing session callers. The
audio owner holds both ready banks, validates/resets only their new graphs, then
installs both in the same callback. Q/C/T/F/O/chain metadata follows that same
commit. Any worker/validation failure cancels both staged banks without touching
active graphs; cancelled/retired rings stay charged until worker reclamation.
D's complete slot/budget preflight precedes Arp job publication. Fixed twenty-
track/forty-slot storage and the existing single worker are reused. No callback
allocation, full PCM scan, extra worker or new parameter/session ID was added.

The one terminal Arp-bank receipt includes `includesDelays`, both revision arrays,
clear epochs and its callback serial. D edits/banks are excluded while a joint
request or unread receipt exists; D cancellation invalidates the whole joint
request. External accepted FX/chain/D edits cancel pending joint restore.
This core API is not yet submitted by the product Processor restore owner; it
does not make current saved projects or GUI/CLI controls Delay-enabled.

The coordinated-bank admission and ten scenario tests cover twenty old + twenty new owners
in each pool, both readiness orders, both kinds of worker OOM, all-off commit,
aggregate budget rejection before any worker job, stale final-track revisions,
independent-D/unread-receipt exclusion, D/Arp cancellation, panic, DEL, prepare,
synthetic MIDI Stop and same-ID recovery. Paused real allocations prove that
already-ready sibling banks cannot change settings early. Audio heap guards stay
enabled. These are ownership tests, not ThreadSanitizer or hardware evidence.

`delay_atomic_bank_audio_contract` retains every previous D-only rate/block case
and adds independent Arp + D owners scheduled at the exact receipt boundary.
Actual two-loop Engine PCM is checked through AD and DA order, exact/Feedback,
off transitions and real OOM rollback while old audio runs continuously. One
case holds the last Arp allocation with D ready for twenty audible callbacks.
Waveform identity, lengths, cursors and history remain checked. Maximum error:
7.45058e-9 at 8 kHz / 1.49012e-8 at 48 kHz, both 64/128, limit unchanged 2e-6.
Input is paired mono and the original loop downmix/centre-pan path; this is not
full stereo, browser-engine, listening, physical-device or round-trip-latency proof.

The first new audio oracle incorrectly kept separate stereo outputs after loop
pan. It now retains the existing full-chain mono downmix before loop pan, as
already protected in ArpConnectedTests. No product DSP or tolerance changed;
the original four D-only audio cases are still executed unchanged in scope.

Windows Release affected regressions: 19/19 (77.57s); final test-only synthetic
MIDI Stop/paused-audible additions passed 2/2 (26.42s), reusing unchanged passing
normal suites. Durations are functional test times, not performance benchmarks.
Native WSL Linux Release affected checks passed 19/19 (18.04s). Rebuilt Windows
and Linux Standalone and existing Processor compatibility checks passed 6/6 each
(19.99s / 7.07s). Those Processor callers still use the legacy opt-out path; this
does not claim product submission/binding of the new joint transaction.

The first Linux Debug ASan/UBSan/leak run had 18 passes and one aggregate
coordinated-test timeout (90.11s; total run 252.49s). It was not a passing run.
The harness now registers admission plus scenarios 0..9 separately, each retaining
the same 90s limit and both block sizes. The no-argument executable still runs the
original complete matrix. Every case, callback count and assertion is retained;
only test-process boundaries and flushed progress output changed. This makes a
single stuck scenario identifiable without assuming a CPU cause or loosening an
audio threshold. Test-driver snapshot `f0d5083`: Windows scenario checks 11/11
(23.95s), native Linux Release 11/11 (5.49s), Linux Debug ASan/UBSan/leak 11/11
(196.33s total, each scenario within 90s), with no reported sanitizer errors or
suppressions. The other eighteen sanitizer checks passed in the first run and
were not repeated after this test-only partition. These results span runs;
there was no claimed single 29/29 sanitizer run. The original aggregate timeout
remains recorded above. No production CPU optimization or runtime gain is inferred.

Build targets for the affected core set:

```text
aion_effects_delay_bank_tests aion_delay_tests aion_delay_repeat_tests
aion_delay_storage_tests aion_delay_connected_tests aion_delay_engine_tests
aion_delay_engine_audio_tests aion_delay_bank_tests aion_delay_bank_audio_tests
aion_delay_owner_tests aion_arp_storage_tests aion_arp_bank_tests
aion_arp_preparation_tests aion_arp_owner_tests aion_arp_owner_bank_tests
aion_arp_connected_tests aion_drive_connected_tests aion_signal_chain_tests
```

Use these after `--target` in the cache-specific build commands below. Core CTest
selection (Windows adds `-C Release`):

```sh
ctest --test-dir build/linux -R '^(delay_.*_contract|original_delay_settings_contract|effects_delay_coordinated_bank(_[0-9]+)?_contract|arp_delay_storage_contract|arp_bank_contract|arp_preparation_contract|arp_owner_contract|arp_owner_bank_contract|arp_connected_contract|drive_connected_contract|signal_chain_connected_contract)$' --output-on-failure --no-tests=error
```

For Linux sanitizers, regenerate/build the existing `build/drive-sanitized` cache
with the same targets and regex, then run with `ASAN_OPTIONS=detect_leaks=1
UBSAN_OPTIONS=halt_on_error=1`. For production compatibility, build Standalone and
the three Processor targets and run the six-case command below. Do not run the
unqualified exhaustive core suite or enable deferred wrappers for this slice.

Next at that core checkpoint (binding/clear work now implemented above): versioned
project persistence, then the shared product command/UI/CLI/MIDI surface. Neither
separate successful banks nor this core-only joint gate establish full project
restore or complete Delay feature acceptance. No routine distributable or push.

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

The newer coordinated-restore checkpoint above supersedes this historical next step:
complete Processor command/result ownership and coordinated session-bank restore, then
shared GUI/CLI commands, MIDI Learn, versioned persistence and editor lifecycle.
Retain the connected Engine 64/128 PCM gates, then run actual GUI/CLI interaction
and listening acceptance. The Processor must cancel D via its independent path
before priority purge/delete and fence returned metadata by generation/epoch/
revision plus its existing document lifetime/correlation guards. Keep D unavailable until
these pass; do not start another effect or package this component as complete.
Broad full-workspace CPU tuning remains deferred, not silently accepted.
