# Arp Delay migration

## Ordered original preset gesture — 12 November 2026

Before message-owned state/preset integration, resolve the old compiler handoff's
unverified same-time preset trajectory. Acceptance defined before editing:
execute original `applyGenericPreset`/`update`/`updateLoopNode` unchanged; preserve
Object.entries order and repeated BPM writes; atomically admit and apply the
entire gesture without callback allocation or replacing delay/STAY/LFO storage.
Invalid/stale/full-queue cases must preserve all old automation, not just metadata.

`arpPresetWrites` produces the original eleven control/BPM pairs. The typed
`Engine::enqueueArpPreset` compiles the original double controls/catalog and uses
one existing performance FIFO entry. `LoopBank::setArp` validates epoch/revision,
seed, mask, enable state and all values before its fixed 22-write path. One
logical settings revision is committed afterward. A preset neither toggles A
nor changes its seed. Raw single-control commands retain their old behavior.
No new action/parameter IDs, default sound, session schema, preserved asset or
product GUI/CLI/MIDI surface changes. A remains disabled until full integration.

| Reference behavior | Native status | Evidence / remaining gap |
| --- | --- | --- |
| Eight original named presets, ordered eleven edits plus eleven BPM writes | One command preserves all 22 writes, including equal values | 56 unchanged-JS cases / 1232 exact ordered Float32 writes |
| Synchronous preset application | One FIFO entry and one dispatch budget slot | Whole preset applies at its sample deadline after 127 older commands; no cross-callback prefix |
| Persistent active effect | Controls update existing graph, no prepare/reset | Exact nonzero Engine stereo PCM and graph state versus eleven scheduled edits at 8/48 kHz, 64/128 |
| Repeated same-time BPM automation | Retain repeated retargets | Frame-14 / 48 kHz deferred-anchor regression distinguishes ordered writes from collapsed targets |
| Invalid/stale/full queue | Prevalidate before mutation; reject complete gesture | Epoch/revision/seed/enable/mask/BPM/catalog rejection, last-slot/full FIFO, prepare cleanup and disabled controls |
| Original sound | Actual original methods in offline WebView2 plus native batch graph | 50 preset transitions including repeated presets, five rates, 15 comparisons; peak 6.70552e-8 <=1e-6, relative RMS <=1e-5 |
| Message-owned intent, saved preset name, UI/CLI/MIDI and full state restore | Still pending | The helper reports admission only; exact acknowledgement, rollback and Q/C/chain restore coordination remain required |

Windows tests pass: focused **4/4 (7.15s)**, retained Arp ownership/connected/assets
**5/5 (39.16s)** and extended preset **1/1 (4.08s)**. Standalone builds. Linux and
sanitizer checks are pending this candidate snapshot. The broader pre-existing
core evidence at `6654944` is retained; no unrelated suite is repeated here.

The browser launcher originally used scalar-string splatting, which passed
characters instead of `--presets`. Its old 63-case graph run passed (peak
2.38419e-7); retain that log. A typed argument array plus strict JS option checking
fix the harness, then the intended 15-case preset audio matrix passes. The fixture
records hashes for original worklet/factory/smoothing AND preset/update methods.
No tolerances or original sources were changed. This is test-only OfflineAudioContext
sound evidence, not a product AudioContext, GUI interaction or physical-device test.

Reproduction in existing caches:

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_core
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_preset_tests aion_arp_settings_tests aion_arp_probe aion_arp_bank_tests aion_arp_preparation_tests aion_arp_connected_tests aion_arp_storage_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(arp_preset_contract|original_arp_preset_order|arp_settings_contract|original_arp_settings_contract|arp_bank_contract|arp_preparation_contract|arp_connected_contract|arp_delay_storage_contract|original_ui_bytes)$'
cmake --build build/windows --config Release --parallel 2 --target AionLooper_Standalone aion_effects_browser_host
./tools/TestArpDelayWebAudio.ps1 -Presets
```

Commit/sync with `SetupWslWorkspace.sh` before native WSL builds. Transfer only
the generated browser fixture into the Linux build directory for replay:

```sh
cd /home/jorge/projects/Aion-Looper
cmake --build build/linux --parallel 2 --target aion_core
cmake --build build/linux --parallel 2 --target aion_arp_preset_tests aion_arp_settings_tests aion_arp_probe aion_arp_bank_tests aion_arp_preparation_tests aion_arp_connected_tests aion_arp_storage_tests AionLooper_Standalone
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_preset_contract|original_arp_preset_order|arp_settings_contract|original_arp_settings_contract|arp_bank_contract|arp_preparation_contract|arp_connected_contract|arp_delay_storage_contract|original_ui_bytes)$'
cp /mnt/d/Jorge/Documents/Codex/Aion-Looper/build/arp-preset-browser-fixtures.json build/arp-preset-browser-fixtures.json
node tests/ArpGraphWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-preset-browser-fixtures.json
cmake --build build/overlap-asan --parallel 1 --target aion_core
cmake --build build/overlap-asan --parallel 1 --target aion_arp_preset_tests aion_arp_bank_tests aion_arp_connected_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^(arp_preset_contract|arp_bank_contract|arp_connected_contract)$'
cmake --build build/arp-storage-tsan --parallel 1 --target aion_core
cmake --build build/arp-storage-tsan --parallel 1 --target aion_arp_preset_tests aion_arp_bank_tests aion_arp_connected_tests
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir build/arp-storage-tsan --no-tests=error --output-on-failure -R '^(arp_preset_contract|arp_bank_contract|arp_connected_contract)$'
```

Logs: `build/arp-preset-*-windows.log` and matching native WSL logs. Next: validate
this snapshot on Linux; then original message-owned settings, exact request/track
acknowledgement, rollback and coordinated native restore. Keep product A disabled.
No package/push.

## Atomic Arp-only bank transaction — 12 November 2026

Runtime `6654944`, extended tests `9355190`, after `44ef94a`;
**product A remains disabled**. Acceptance
was defined before editing: reserve every required replacement before publishing
work; adopt all twenty graphs/settings together; preserve old audio, STAY/LFO/
automation and settings on late OOM, cancellation, stale revisions or exhaustion.
Keep preparation/reclamation on the existing worker and zero callback new/delete.
No reference/render math, GUI/CLI/MIDI, persisted schema, seed/default policy or
preserved UI asset changes are included.

| Behavior | Native status | Verification / remaining gap |
| --- | --- | --- |
| Twenty enabled replacements with twenty old owners | Fixed forty-slot preflight; one bank owns pending descriptors | Nineteen ready cannot install a prefix; twentieth commits at one frame |
| Failure after earlier members prepared | Any failed allocation/reset discards the candidate bank | Deterministic twentieth-allocation OOM, invalid adoption clock, old pointers/state and exact later PCM |
| Mixed/off bank | Disabled members retire at the same commit; all-off needs no new slot | Storage + actual Engine settings/owner checks at 64/128 |
| Cancellation and ownership | Generation, all clear epochs/settings revisions, independent atomic cancellation | DEL/DEL ALL/STOP ALL/armed CANCEL/live edit, in-flight/queued prepare, stale result and recovery checks |
| Bounded message protocol | One bank including unread terminal receipt; excludes per-track preparations | Exact ID/generation/status, closed consumers, full ordinary FIFO, concurrent producer/audio/worker checks |
| Editable effect order and independent settings | Arp-only commit changes no Q/C/chain state | Success/failure checks retain Q/C settings, `ACQ` order and `CQ` omission of A; existing connected audio ordering gate also passes |
| Original projects/presets/full FX restore | Pending message-owner adapter and validated state importer | Bank holds raw accepted Arp render settings only; does not restore Q/C/chain or parse legacy state |

The bank does not own PCM clips, change track IDs/capacity or reuse a graph
pointer across owners. Worker publication/retirement still uses release/acquire
slot phases. Graph borrows are audio-local. All twenty candidate resets succeed
before old owners are retired; metadata and revisions commit before render or
snapshots. Candidate cancellation never frees on audio. Cancelled/retired slots
remain occupied until the worker reclaims them; explicit `busy` is a valid
terminal result. Test-only capacity probes consume it before retrying; no product
retry loop or disconnected visible control has been added.

Commit occurs at `beginBlock`, before ordinary performance FIFO commands. A later
queued panic/clear may intentionally change an already committed bank; it does
not retroactively cancel its ready receipt. Full product gesture order must be
implemented on the message owner before exposing restore/preset commands. This
transaction is Arp-only, not atomic session/Q/C/chain restore. Unread old-generation
results retain their original status/ID through repeated stopped prepare, and
continue to own admission until consumed.

Windows focused selection passes **7/7 (39.25s)**. New bank tests include exact
nonzero stereo Engine PCM and independent graph state/PCM, actual 64/128 callbacks,
last-slot OOM, queued/in-flight cancellation and same/new-rate prepare. Retained
preparation, storage, connected and settings tests plus unchanged-JS settings and
original byte assets pass. Initial new-fixture failures assumed immediate slot
reclamation and ordinary zero-block command dispatch; tests now retain the
production capacity/dispatch contract. No old regression or tolerance changed.
Linux selection passes **8/8 (322.99s)**, including the retained core regression
(315.13s). Unchanged-JS Arp worklet sound and streaming gates pass **2/2** on both
OSes (Windows 31.13s, Linux 31.11s). ASan/UBSan with leak detection passes
**3/3 (17.57s)**; ThreadSanitizer passes **3/3 (27.52s)**. Both Standalones build.
The broader Windows core regression also passes **1/1 (848.29s)**. This accepts
the internal Arp-only bank slice, not product Arp or session restore acceptance.

The test-only extension `5262ff0` also verifies adoption at frame 37 and actual
Engine callbacks of 0/1/7/63/64/65/127/128/3 samples at 32/48 kHz, matching the
independent graph state/clock. Direct LoopBank panic is exercised with every old
Arp disabled, retaining the unconditional bank cancellation guard. Extended bank
passes: Windows **1/1 (18.65s)**, Linux **1/1 (2.36s)**, ASan/UBSan/leaks
**1/1 (15.31s)** and TSAN **1/1 (24.19s)**. Unchanged passing suites were not
repeated for this test-only addition. TSAN uses process-only `setarch x86_64 -R`;
no global ASLR change. No sanitizer diagnostics were reported.

Final test `9355190` retains Q/C settings and user chain order/omission across
successful/failed bank transactions. The full bank test passes again: Windows
**1/1 (18.53s)**, Linux **1/1 (2.47s)**, ASan/UBSan/leaks **1/1 (15.51s)** and
TSAN **1/1 (24.50s)**. Runtime is unchanged after `6654944`; the stronger bank
test was the only test rerun for these additions.

These durations are test wall times, not callback/latency benchmarks. No new
hardware, UI, DAW, physical MIDI, driver or release-readiness claim is made. The
product surface did not change, so prior actual UI evidence is retained rather
than represented as newly executed. Original sound math remains unchanged.

Reproduction in existing caches:

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_bank_tests aion_arp_preparation_tests aion_arp_storage_tests aion_arp_connected_tests aion_arp_settings_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(arp_bank_contract|arp_preparation_contract|arp_delay_storage_contract|arp_connected_contract|arp_settings_contract|original_arp_settings_contract|original_ui_bytes)$'
cmake --build build/windows-core --config Release --parallel 2 --target aion_core_tests aion_arp_probe aion_arp_stream_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(core_realtime_contract|js_native_arp_delay|arp_delay_streaming_contract)$'
cmake --build --preset windows-dev --parallel 2
```

Commit in Windows before using `SetupWslWorkspace.sh`; then in the independent
native WSL checkout (build `aion_core` first to refresh existing Makefile targets):

```sh
cmake --build build/linux --parallel 2 --target aion_core
cmake --build build/linux --parallel 2 --target aion_arp_bank_tests aion_arp_preparation_tests aion_arp_storage_tests aion_arp_connected_tests aion_arp_settings_tests
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_bank_contract|arp_preparation_contract|arp_delay_storage_contract|arp_connected_contract|arp_settings_contract|original_arp_settings_contract|original_ui_bytes)$'
cmake --build build/linux --parallel 2 --target aion_core_tests aion_arp_probe aion_arp_stream_tests AionLooper_Standalone
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(core_realtime_contract|js_native_arp_delay|arp_delay_streaming_contract)$'
cmake --build build/overlap-asan --parallel 1 --target aion_core
cmake --build build/overlap-asan --parallel 1 --target aion_arp_bank_tests aion_arp_preparation_tests aion_arp_storage_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^(arp_bank_contract|arp_preparation_contract|arp_delay_storage_contract)$'
cmake --build build/arp-storage-tsan --parallel 1 --target aion_core
cmake --build build/arp-storage-tsan --parallel 1 --target aion_arp_bank_tests aion_arp_preparation_tests aion_arp_storage_tests
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir build/arp-storage-tsan --no-tests=error --output-on-failure -R '^(arp_bank_contract|arp_preparation_contract|arp_delay_storage_contract)$'
```

For just the test-only extension, rebuild `aion_arp_bank_tests` in each existing
cache and select `^arp_bank_contract$` using the corresponding environment above.
Logs: `build/arp-bank-{ctest,core-regression,reference,standalone,edge,chain}-windows.log`;
native WSL `build/arp-bank-{ctest,reference,asan-test,tsan-test,edge-normal,edge-asan,edge-tsan,chain-normal,chain-asan,chain-tsan}-linux.log`.
Next: message-owned original settings and
ordered shared GUI/CLI restore/preset integration. No package/push.

## Original control/preset compiler — 12 November 2026

Next internal prerequisite after `f974d35`; **product A remains disabled**. This
slice defines the original eleven named controls, eight named presets and twenty
scale labels, and compiles them to the accepted twelve-target render model. It
does not enable GUI/CLI/MIDI, serialize a session, choose organic defaults/seeds,
or change the processor's restore/routing implementation. Future GUI/CLI commands
must share this converter and the existing processor/DSP owner.

Acceptance defined before implementation: compare original default/preset values,
widget limits, factory targets and live edit target selection without changing
original methods. Retain JS decimal precision until the DSP boundary; each
original parameter edit also retargets BPM. Invalid settings, masks, edits or BPM
leave the previous output intact. Exercise compiled presets/edits through the
existing Engine storage adoption at 64/128 with zero watched callback new/delete.

| Reference behavior | Native checkpoint | Verification / remaining gap |
| --- | --- | --- |
| Eleven decimal controls; repetitions / 11.1 | `ArpSettings` stores doubles, `compileArpSettings` converts once to Float32 | Exact factory/live targets; 9.9 catches premature Float32 storage |
| Eight preset names/values; manual edits clear selection | Stable 1..8 catalog, Custom/default 0; preset application retains enabled state | Unchanged `applyGenericPreset` final values/selection; full live batch trajectories pending |
| Widget bounds vs wider worklet domain | Time 2s / stored 4s, amplitude 1.5 / stored 2; canonical checkbox 0/1, fractional scale preserved | Original markup/factory comparisons; not a legacy-project importer |
| Every knob/toggle edit also writes BPM | Control-to-render mask includes BPM for each nonzero gesture; zero mask stays zero | All 2048 masks; original live handlers for every control |
| Per-loop accepted settings/storage ownership | Compiled settings enter existing typed preparation and revision/epoch-guarded edits | All eight presets and 176 edits at 64/128; prior sound/ownership gates retained |

`ArpSettings` is separate from `ArpRenderSettings`: BPM and a caller-provided seed
are external inputs, not additional stored controls. BPM is limited to the original
AudioParam nominal 30..300 target range; this does not change transport/stored BPM
or implement master-tempo events. Source master-tempo changes use immediate
`setValueAtTime`, while control edits retarget it; that adapter remains pending.
Original preset batches issue repeated same-time BPM gestures; the target mask is
not proof of their complete automation trajectory or cross-queue command order.

The settings validator deliberately covers the known native domain, including
the wider time/amplitude domain and finite decimals. Arbitrary legacy out-of-domain
or non-binary truthy checkbox data is not accepted by this helper. No existing
project/state parser uses it; investigate legacy imports before adding one and
never silently discard/clamp stored data. Organic new-instance defaults remain
separate from this faithful reference baseline. Original assets, IDs, presets and
session versions remain untouched.

Windows initial unit checks pass. The unchanged-JS oracle has 2157 cases: original
factory construction, live `update`/`updateLoopNode`, preset application, all masks,
nominal BPM bounds, wider stored endpoints and decimal repetition cases. The
initial oracle wrapper failed because a trailing source comment swallowed its
class terminator; only the wrapper newline was repaired. Keep
`build/arp-settings-reference-windows.log` as the failed harness observation.
Accepted model source `af07ce9`: final Windows converter/oracle **2/2 (2.62s)**,
plus the final direct oracle with explicit signed-zero input. The retained Windows
selection including original assets passed **3/3 (4.56s)** before that last
checkbox case. Native WSL final selection passes **3/3 (3.77s)**, including all
2157 exact target/mask cases and byte-identical assets. Linux ASan/UBSan/leaks
**1/1 (0.63s)** and TSAN **1/1 (0.95s)** pass with no diagnostics. TSAN uses the
existing process-only `setarch x86_64 -R` workaround. Both Standalones build.

No existing audio-rendering source or bridge/UI/state path changed. Retain prior
connected PCM/browser/ownership evidence; these new checks establish target and
model/Engine integration, not full live preset envelopes, UI interaction,
controller hardware, arbitrary legacy imports or release acceptance. No new
callback timing claim is made for this unused product adapter.

Reproduction in existing caches:

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_core
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_settings_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(arp_settings_contract|original_arp_settings_contract|original_ui_bytes)$'
cmake --build --preset windows-dev --parallel 2
```

After committing/syncing with `SetupWslWorkspace.sh`, in the native WSL checkout:

```sh
cmake --build build/linux --parallel 2 --target aion_core
cmake --build build/linux --parallel 2 --target aion_arp_settings_tests AionLooper_Standalone
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_settings_contract|original_arp_settings_contract|original_ui_bytes)$'
# Existing sanitizer caches/flags are documented in the storage section below.
cmake --build build/overlap-asan --parallel 1 --target aion_core
cmake --build build/overlap-asan --parallel 1 --target aion_arp_settings_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^arp_settings_contract$'
cmake --build build/arp-storage-tsan --parallel 1 --target aion_core
cmake --build build/arp-storage-tsan --parallel 1 --target aion_arp_settings_tests
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir build/arp-storage-tsan --no-tests=error --output-on-failure -R '^arp_settings_contract$'
```

Logs: Windows `build/arp-settings-{ctest,checkbox,truthiness-reference,final-standalone}-windows.log`;
native WSL `build/arp-settings-{ctest,asan-test,tsan-test,build}-linux.log`.
Next: message-owned Arp settings and staged all-bank restore/OOM rollback, then
the original panel through shared GUI/CLI commands, MIDI, BPM events and approved
organic new-instance voicing. Keep product A disabled until end-to-end acceptance.
No package or push.

## Exact settled-automation fast path — 12 November 2026

Active acceptance: preserve original automation/PCM and callback ownership while
reducing redundant work after a parameter reaches its exact target. A lane may
reuse its last rendered Float32 only after the existing timeline produces that
exact target; no convergence threshold changes. Reset and every new/repeated
gesture invalidate the flag, retaining boundary cancellation and deferred events.
Negative-zero targets retain the original recurrence: its first quantum lane is
negative zero, later lanes positive zero. A new 256-frame bit-sign regression
failed the initial shortcut and passes after this exclusion; keep
`build/arp-settled-zero-negative-windows.log`. No product controls, presets, seed
policy, dependencies or original assets change.

The completed connected checkpoint below passed its software gates at `731b9c3`.
Its first twenty-loop Q/C/A callback observations (48 kHz, 512 warm / 2048 measured)
show limited headroom and retain all overruns:

| OS / block | Mean / p99 / maximum, us | Deadline, us | Overruns |
| --- | --- | --- | --- |
| Windows / 64 | 1007.13 / 1203.7 / 1305 | 1333.33 | 0 |
| Windows / 128 | 1953.62 / 2312.3 / 2745.3 | 2666.67 | 1 |
| WSL / 64 | 900.388 / 1030.4 / 1888.7 | 1333.33 | 4 |
| WSL / 128 | 1803.64 / 2639 / 3402.5 | 2666.67 | 17 |

Windows was recorded on 10 November, Linux on 12 November. Both are wall-time observations, excluding
devices, UI and round-trip latency; they do not identify scheduler versus DSP cost
or establish reliable twenty-track deadlines. Logs are retained as
`build/arp-connect-benchmark-{windows,linux}.log` in their respective checkouts.
Fast-path source `1444050`: Windows 5/5 (37.24s) plus storage/preparation 2/2
(14.78s); Linux 7/7 (32.87s). Both OSes replay all 22 modulation fixtures / 66
comparisons and 21 combined factory fixtures / 63 comparisons, preserving the
original limits: max parameter error 0, audio peaks 2.27243e-7 / 2.38419e-7.
These replay recorded original WebView2 audio, not Linux WebKit or new UI checks.
Linux ASan/UBSan/leaks 5/5 (34.06s), TSAN 3/3 (24.24s), no diagnostics. Both
Standalones build. The negative-zero exclusion subsequently passes affected
Windows connected/kernel/stream tests 3/3 (5.99s), Linux 3/3 (2.72s), and Linux
ASan/UBSan/leaks 3/3 (30.76s). Both final Standalones build.

Settled-lane observation at `faddcf1`, same workload: Windows 64 mean/p99/max
891.641/1062.1/1213.2us (0 overruns); 128 1806.47/2128.2/2774.3us (1).
WSL 64 837.618/987.675/1104.83us (0); 128 1694.41/1874.22/3028.45us (1).
Checksums match the earlier workload. These separate wall observations are not a
controlled speedup claim or a reliable-deadline acceptance; keep
`build/arp-settled-benchmark-{windows,linux}.log`.

The next bounded optimization submits worklet controls only at their existing
128-frame latch boundary, or the first sample after prepare/reset, rather than
resubmitting identical controls on every Engine sample. It does not defer the
automation or post-pan LFO, which still advance per sample. Acceptance retains
exact variable-block PCM/state and original factory fixtures. A new audible
reset-at-frame-37 regression checks that a pending mix target takes effect on the
first sample, matching a fresh owner, including single-sample calls across later
boundaries. Runtime/test snapshot `2695768` passes Windows 5/5 (20.62s) plus the
new restart/stream executable, Linux 6/6 (4.55s), and TSAN 4/4 (45.65s). Windows
replays all 63 original factory comparisons at unchanged peak 2.38419e-7. Both
Standalone builds pass. Linux replays the same 63 factory comparisons at peak
2.38419e-7, and ASan/UBSan/leaks passes 5/5 (35.00s). No diagnostics.

Final quiet observations at `2695768`, same 20 playing Q/C/A loops / 48 kHz /
512 warm / 2048 measured callbacks, excluding preparation/reclamation:

| OS / block | Mean / p99 / maximum, us | Deadline, us | Overruns |
| --- | --- | --- | --- |
| Windows / 64 | 883.426 / 1060.3 / 1180.6 | 1333.33 | 0 |
| Windows / 128 | 1745.88 / 2013 / 2320.2 | 2666.67 | 0 |
| WSL / 64 | 807.894 / 964.6 / 1100.06 | 1333.33 | 0 |
| WSL / 128 | 1682.71 / 1860.07 / 2264.38 | 2666.67 | 0 |

The workload remains non-silent with matching checksums (132697 / 285809).
These final observations meet the sampled callback deadlines; they do not erase
the earlier misses or prove continuous reliability, hardware latency or release
readiness. The two exact-processing optimizations pass their scoped software
acceptance. Product A remains disabled: continue its original controls/presets,
shared GUI/CLI/MIDI/state and BPM/restore integration before enabling it. Full
device/listening/long-session acceptance remains separate. No package or push.

For this optimization, build `aion_arp_tests aion_arp_stream_tests
aion_arp_connected_tests aion_arp_preparation_tests aion_arp_storage_tests
aion_arp_probe` in the existing Windows core/Linux caches, and select
`^(arp_delay_realtime_contract|arp_delay_streaming_contract|arp_connected_contract|arp_preparation_contract|arp_delay_storage_contract|original_ui_bytes)$`
with CTest (`-C Release` on Windows). Replay the unchanged original audio fixtures:

```sh
node tests/ArpModulationWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-modulation-browser-fixtures.json
node tests/ArpGraphWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-graph-browser-fixtures.json
```

On Windows substitute `build/windows-core/Release/aion_arp_probe.exe`. Fixture
generation is documented in the existing modulation/factory sections below.
Use the existing sanitizer flags below and the same affected target/test lists
(TSAN includes streaming/connected/storage/preparation). Benchmark only after
other builds/tests have stopped, with `aion_arp_connected_tests benchmark`.
Logs are `build/arp-settled-*` and `build/arp-quantum-*` on their respective OSes;
retain the negative-zero failure and every timing overrun observation. No original
fixture, tolerance or ownership regression was removed or relaxed.

## Connected render settings and user-order routing — 10 November 2026

Internal integration in progress; **product A remains disabled**. `ArpRenderSettings`
contains twelve raw DSP targets, explicit fixture/caller seed and enabled state.
It is not the original UI/preset/persistence schema, organic new-instance voicing,
transport BPM adapter or product seed policy. Its factory baseline maps
repetitions 2 to Float32 feedback `2 / 11.1`, preserving the original distinction
from the raw worklet's .18 default. Existing presets/state/IDs/assets are unchanged.

Acceptance defined before edits: prepared storage and matching settings commit in
one callback before rendering/snapshots; OOM, stale revisions, cancellation and
capacity failures preserve prior settings/sound. Raw diagnostic preparations must
not replace enabled owners. Direct controls cannot enable an unprepared graph.
Render A only at its selected per-loop chain position; retain persistent targets,
STAY/delay/LFO across ordinary source restarts/history, support masked live edits,
and retain clear/bypass/panic/prepare ownership and zero callback new/delete.

`LoopBankArp` holds twenty audio-owned pending descriptors. Before poll it cancels
descriptors with obsolete clear/settings revisions; successful storage adoption
commits descriptor settings/request/revision immediately. The Engine protocol's
optional `activate`/expected revision remains bounded to twenty unread transactions.
The appended internal `LoopAction::arpDelay` performs validated, revision/epoch-
guarded masked controls or disable; no product RPC/UI/CLI is exposed by this slice.
Each selected bit is a gesture, including unchanged values, preserving unselected
automation. Disable retires storage on the worker. Re-enable requires preparation.

Storage publishes an audio-owned adoption serial. Readiness requires a new
installation, rather than an older active graph's existence. This is important
when armed-take cancellation preserves the current graph while cancelling its
replacement: it must return `cancelled`, never an obsolete success. The new case
pauses worker allocation, arms/cancels the take, verifies the retained owner and
the exact terminal receipt, then releases obsolete work.

The router handles A alongside Q/C in any stored order. Reordering included A
retains its owner; omission freezes it, and reinsertion resets it without allocation
at the current clock, matching a newly inserted reference effect. Ordinary STOP
runs zeros through the persistent owner but bounds audible output by the existing
source gate; PLAY/RTRG/history preserve state. Panic resets delay/STAY/LFO in
constant work while retaining prepared storage/settings. Armed CANCEL preserves
its current owner. Same-rate, unchanged-capacity stopped prepare resets its clock
and retains the valid owner; rate/capacity-changing core prepare clears storage
AND metadata. Product-level
rate restore still needs asynchronous preparation before it can enable A again.

Initial Windows checks: actual Engine PCM at 8/48/96 kHz, 64/128, eight chain
orders/omission/reinsertion, all twelve raw targets and repeated masked gestures,
OD/SUB/MULT/finalization, RTRG/history, STOP/PLAY and panic match independent stage
owners at peak <=2.98023e-8 (unchanged 1e-6 limit). Callback new/delete watchers
pass. Typed OOM/stale/mask/disable/DEL/DEL ALL/twenty-track installs, old-active
cancellation and stopped-rate lifecycle cases pass. Retained preparation, storage
and chain selection passed 4/4 (21.75s) before the final test-only extensions.
The extended audible OOM and explicit live OD/SUB/MULT/scheduled-STOP controls pass
on Windows/Linux, retaining the same PCM limit and unchanged STOP deadline.
Linux ASan/UBSan/leaks and TSAN pass the connected/preparation pair (12.44s/20.69s);
the subsequent live-controls test-only extension passes connected checks again
(10.68s/17.73s). TSAN uses the documented process-only `setarch x86_64 -R`.
Linux retained regressions pass **8/8 (312.66s)**, including the full core
real-time contract (306.74s). Windows retained regressions pass **4/4 (865.81s)**,
including the full core contract (854.46s); the separate focused selection passes
4/4 (21.75s). Windows and Linux Standalone build. Connected twenty-track callback
observations are recorded above. No new original-JS audio math, UI controls or dependency change;
prior kernel/factory/browser evidence is reused for the unchanged DSP.

Next after this internal gate: original control/preset conversion and shared
GUI/CLI/MIDI/versioned state, staged all-bank restore/OOM rollback, BPM semantics,
organic new-instance defaults and relevant UI/performance acceptance before A is
enabled. This protocol is not cross-queue GUI ordering or atomic FX-bank restore.
No package or push.

Reproduction in existing caches (CMake regenerates for the added target):

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_core
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_connected_tests aion_arp_preparation_tests aion_arp_storage_tests aion_signal_chain_tests aion_eq_connected_tests aion_compressor_connected_tests aion_core_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(arp_connected_contract|arp_preparation_contract|arp_delay_storage_contract|signal_chain_connected_contract|eq_connected_contract|compressor_connected_contract|core_realtime_contract|original_ui_bytes)$'
cmake --build --preset windows-dev --parallel 2
build/windows-core/Release/aion_arp_connected_tests.exe benchmark
```

After a local commit, use `SetupWslWorkspace.sh` as documented in
DEVELOPMENT_WORKFLOW.md. In the native WSL checkout:

```sh
cmake --build build/linux --parallel 2 --target aion_core
cmake --build build/linux --parallel 2 --target aion_arp_connected_tests aion_arp_preparation_tests aion_arp_storage_tests aion_signal_chain_tests aion_eq_connected_tests aion_compressor_connected_tests aion_core_tests AionLooper_Standalone
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_connected_contract|arp_preparation_contract|arp_delay_storage_contract|signal_chain_connected_contract|eq_connected_contract|compressor_connected_contract|core_realtime_contract|original_ui_bytes)$'
# Existing ASan/UBSan and TSAN cache flags are documented in the storage section.
cmake --build build/overlap-asan --parallel 1 --target aion_core
cmake --build build/overlap-asan --parallel 1 --target aion_arp_connected_tests aion_arp_preparation_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^(arp_connected_contract|arp_preparation_contract)$'
cmake --build build/arp-storage-tsan --parallel 1 --target aion_core
cmake --build build/arp-storage-tsan --parallel 1 --target aion_arp_connected_tests aion_arp_preparation_tests
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir build/arp-storage-tsan --no-tests=error --output-on-failure -R '^(arp_connected_contract|arp_preparation_contract)$'
build/linux/aion_arp_connected_tests benchmark
```

Logs: Windows `build/arp-connect-{focused,regression,regression-details,live,standalone}-windows.log`;
native WSL `build/arp-connect-{build,ctest,live,asan-test,tsan-test,live-asan-test,live-tsan-test}-linux.log`.
The broad regressions run at runtime source `70c575b`; `731b9c3` adds only stronger
live-controls tests. No UI/bridge edits: reuse accepted Q/C-chain UI evidence;
these internal tests do not validate product A interactions, Linux WebKit,
physical drivers, listening, hardware MIDI or release readiness.

Current DSP slice, 10 November 2026: native original worklet + automation/post-pan
with approved zero-latency causal streaming. Bounded track storage and internal
Engine preparation transactions follow below. The product A checkbox stays
disabled pending audio routing and shared product GUI/CLI/MIDI/state acceptance.
Baseline: clean `f56d20c`; compressor gates are retained in COMPRESSOR_MIGRATION.
Reference sibling is absent; `.reference/Ascii-Looper` and matching preserved
`resources/ui/Modules/effects.js` / `loopTracks.js` are read-only inputs.

Owner decisions: persistent per-loop delay/STAY/modulation state across playback
restarts, and live controls during OD/SUB/MULT/scheduled STOP. These are intentional
connected-stage extensions; EQ/compressor approvals alone were not reused.
The owner subsequently selected **zero added latency with causal wake-up**, rather
than buffering 128 samples to reproduce the original lookahead on small callbacks.
The owner also requests a more organic/natural sound and selected **organic
voicing by default for new instances**, preserving existing presets and saved
sounds. Keep the faithful reference baseline for compatibility; apply the new
default only when the connected layer creates a new instance. That voicing is
not yet implemented/enabled by this kernel checkpoint.

## Acceptance defined before implementation

- Complete worklet: stereo/mono/tail routing, four-second Float32 delay storage,
  two 4096-frame grain heads, Hann table, pitch glide/pluck/wander, 16-note STAY,
  twenty original interval tables, bounce/octave range, feedback HP/LP/FTZ and
  nearest-subdivision sync. Preserve all eight presets and original parameter data.
- Unchanged worklet oracle runs identical Float32 input/control and deterministic
  random streams (test injection only). Require <=1e-6 peak and <=1e-5 relative
  RMS; retain failures and thresholds. Compare note/sequence/clock metadata too.
- Explicit 64/128 direct-worklet calls and arbitrary short blocks, rates, all
  scales, mono/right-only/no input, silence/tails, STAY, sync/BPM/control changes,
  ring wrap and reset. A direct 64-frame worklet comparison does not establish
  equivalence to the browser's usual 128-frame render quantum or a connected
  64-sample device callback.
- No process/control/reset allocation, free, locks or unbounded work. Prepare
  storage off audio; failed prepare preserves the previous owner. Reset invalidates
  old samples with generation tags rather than clearing seconds of PCM on audio.
  Invalid controls preserve state; non-finite/overflowing audio is quarantined.
- Windows/Linux focused original/kernel/storage tests and Linux sanitizers before
  moving to the connected A slice. No package or dependency upgrade.

## Reference details that must not be guessed

The original has eleven user controls (nine ranges plus Sync/STAY), eight presets,
and a separate post-worklet StereoPanner driven by a sine oscillator/depth gain.
The worklet reads only the first value of each AudioParam array per invocation.
It consumes three random numbers per note even when Feel is zero. Test seeding
does not prescribe identical randomness for live tracks.

Scale labels differ from the actual interval tables at several indices. Preserve
both; correcting labels/tuning changes musical behavior. The idle gate scans
only left input, pauses internal state on silence, and decays its detector once
per worklet call. Oscillator normalization also depends on invocation boundaries.
These facts constrain the later native streaming adapter; do not hide a new
buffer delay or claim callback-size independence from a kernel-only comparison.

Connected GUI/CLI/MIDI/state, track lifetime/clear/tail ownership, storage budgeting
and the organic treatment remain pending. Automation/post-pan and streaming DSP
evidence follow below; they do not establish product controls or device acceptance.
The original A control stays disabled until those acceptance checks pass.

## Windows checkpoint

The complete worklet kernel passes **84 unchanged-JS cases / 5,527,296 channel
samples**, peak **3.725290298461914e-9**. Exact note/sequence/random ownership
and bounded state tolerances are checked alongside PCM. All twenty interval
tables and eight original presets are included, plus live direct-worklet controls,
STAY/range/scale/sync/BPM/feedback, ring wrap, reset, left-only idle and mono/tails.
Final Windows CTests pass **3/3 (31.24s)**, including original asset bytes.
The native allocation/OOM/reset tests cover rates 8..384 kHz, 64/128 invocations,
in-place stereo and arbitrary short mono blocks with real ring wrap.

Actual original AudioWorklet in WebView2 **151.0.4129.107** passes **15 cases /
1,003,904 stereo frames**, peak **3.725290298461914e-9**, without changing the
1e-6 peak / 1e-5 relative-RMS limits. A seeded PRNG prefix makes this test
repeatable; the original worklet body is unchanged. The ignored fixture
`build/arp-browser-fixtures.json` stores its SHA256 and browser identity.

Product-document navigation/CSP and an insecure blank document prevented the
first browser harness attempts from loading a worklet. A separate excluded
`aion_effects_browser_host` now serves a secure test-only document with explicit
Blob worklet permission. It owns no processor, audio device or product controller;
no product CSP, UI, resource or native audio behavior changed. A test-host entry
point and test-source string-encoding defect were fixed before the browser pass.
This fixture is never installed/packaged.

Manual warmed Release measurement, twenty isolated stereo worklet kernels at
48 kHz, 512 warm / 2048 measured invocations: 64-frame mean/p99/max
**317.154/423.2/503us**; 128-frame **605.986/752.9/849.9us**. Nonzero PCM and
zero observed overruns against 1333.33/2666.67us periods. These exclude automation,
post-pan, Engine/mixing/telemetry, devices and round-trip latency. They are not
connected callback results or a worst-case guarantee.

Commands (VS CMake/CTest paths follow DEVELOPMENT_WORKFLOW):

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_tests aion_arp_probe
ctest --test-dir build/windows-core -C Release --output-on-failure -R '^(arp_delay_realtime_contract|js_native_arp_delay|original_ui_bytes)$'
cmake --build build/windows --config Release --parallel 2 --target aion_effects_browser_host
tools/TestArpDelayWebAudio.ps1
build/windows-core/Release/aion_arp_tests.exe --benchmark
```

The browser processing model is documented in the official
[Web Audio specification](https://www.w3.org/TR/webaudio-1.0/#rendering-loop).
Matching a worklet's PCM does not prove the separate native streaming/automation/
post-pan implementation, musical organic voicing, UI, hardware or release scope.

## Linux and ownership checkpoint

On committed source **551d021**, native WSL Release build and focused CTests pass
**3/3 (27.32s)**, including the complete 84-case oracle and original asset bytes.
Replay of all **15 actual Windows WebView2 fixtures** against Linux C++ passes,
with the same peak **3.725290298461914e-9**. This is not Linux WebKit rendering.
ASan/UBSan/leaks pass **1/1 (2.18s)** without diagnostics. The subsequent header
guard deletes copy/move operations so a future track assignment cannot copy or
free this ring on audio; a compile-time regression protects that restriction.
Final ownership source **3ae8cd0** builds on Windows and Linux. Its focused Linux
Release test passes **1/1 (0.40s)** and ASan/UBSan/leaks **1/1 (2.25s)** without
diagnostics; Windows unit coverage also passes. This header-only ownership guard
does not change PCM calculations, so unchanged passing sound suites were reused.

Linux manual warmed Release timing, same twenty-instance configuration: 64-frame
mean/p99/max **254.777/336.325/426.066us**, 128-frame
**513.185/622.509/1098.53us**, zero observed overruns. All timing is isolated
worklet processing, excluding the connected chain/adapter and physical latency.

```sh
cmake --build build/linux --parallel 2 --target aion_arp_tests aion_arp_probe
ctest --test-dir build/linux --output-on-failure -R '^(arp_delay_realtime_contract|js_native_arp_delay|original_ui_bytes)$'
node tests/ArpDelayWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-browser-fixtures.json
build/linux/aion_arp_tests --benchmark
cmake -S . -B build/overlap-asan
cmake --build build/overlap-asan --parallel 2 --target aion_arp_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --output-on-failure -R '^arp_delay_realtime_contract$'
```

Use the retained Debug/plugin-OFF sanitizer cache with
`-fsanitize=address,undefined -fno-omit-frame-pointer`. Copy only the generated
ignored browser fixture to the native WSL build directory; source changes travel
through committed snapshots. Logs: `build/arp-{final-ctest-windows,ctest-linux}.log`,
`arp-browser-windows.log`, `arp-browser-replay-linux.log`, `arp-asan-test.log`,
`arp-benchmark-{windows,linux}.log`, `arp-owner-unit-{windows,linux}.log`,
`arp-owner-asan-test.log`.

Before any product enablement: use the approved causal combined streaming stage
verified below, then bounded storage ownership and shared
GUI/CLI/MIDI/state, preserving dry PCM and accepted persistent/live policies.
The worklet's feedback descriptor default **0.18** differs from the factory's
**Float32(2 / 11.1)**; connected defaults must use the factory mapping. The stronger
organic voicing must remain distinguishable from the faithful baseline and must
not rewrite saved sounds. No later effect has started; no package or push.

## Automation / post-pan slice — 10 November 2026

Acceptance: preserve original `smoothSetParam` (cancel, anchor current `.value`,
50ms exponential target), all ten a-rate worklet parameters plus pan rate/depth,
first-value worklet sampling on a global 128-frame clock, repeated gestures and
stereo crossfeed pan after the complete wet/dry output. Compare actual browser
AudioParams and nonzero stereo output at unchanged **1e-6 peak / 1e-5 relative
RMS** audio limits; normalized parameter error <=1e-6. Cover 64/128 and arbitrary
subdivision, static/running/stopped LFO, retarget/reset, malformed controls and
zero callback new/delete. This is a DSP stage, not enabled product controls.

`ArpModulation` adds fixed-storage automation and post-pan, reusing the verified
Compressor timeline arithmetic without changing that effect. A single read-only
sine table is constructed before processing, with rate-dependent strided views.
Chromium's Float32 phase increments, static four-lane phase and interpolation
matter: a plain analytic sine failed the retained 5 Hz browser case at peak
**1.564621925354004e-6**. The corrected oscillator passes that case without a
tolerance change. Initial failure remains in `build/arp-mod-initial-failure-windows.log`.
A later pan-rate target of zero preserves the phase already reached; it does
not force the pan back to center. Only a fresh/reset oscillator starts at center.

The actual browser harness uses a test-only parameter tap with the original
worklet descriptors, unchanged original `smoothSetParam`, and real Oscillator,
Gain and StereoPanner nodes. It tests the factory's always-present post-pan
routing, separately from the unchanged worklet PCM fixture. It does not yet prove
their connected chain or streaming idle behavior. Original resources/UI stay
unchanged. Sources used for the oscillator correction retain Chromium's BSD
notice in the new source file:
[oscillator](https://raw.githubusercontent.com/chromium/chromium/main/third_party/blink/renderer/modules/webaudio/oscillator_handler.cc),
[periodic wave](https://raw.githubusercontent.com/chromium/chromium/main/third_party/blink/renderer/modules/webaudio/periodic_wave_handler.cc),
[stereo pan specification](https://www.w3.org/TR/webaudio-1.1/#stereopanner-panning).

Run the owned hardware-free browser host with
`tools/TestArpDelayWebAudio.ps1 -Modulation`; replay its ignored, source-hash-checked
fixture with `node tests/ArpModulationWebAudio.cjs --probe PROBE --fixtures build/arp-modulation-browser-fixtures.json`.
The probe's `modprepare/modtarget/modrender` commands are test-only, not a second
audio engine or the product CLI. Product A stays disabled. The owner has now
selected zero-latency causal wake-up; the following streaming slice implements it.

Windows results: **22 actual WebView2 fixtures / 66 comparisons / 2,162,688 stereo
frames**, with 64/128/37 subdivisions and rates 8..384 kHz. Maximum audio peak
**2.2724270820617676e-7**, maximum normalized parameter error
**4.4373207837449527e-7**; all original thresholds retained. Focused CTests pass
**2/2 (2.39s)**, including original assets. Twenty automated modulation/post-pan
stages at 48 kHz (512 warm / 2048 measured) show mean/p99/max **146.065/190.7/224.7us**
at 64, **281.738/409.2/577.8us** at 128, zero observed overruns. This excludes
worklet processing, streaming, Engine/devices and round-trip latency; it is not
a combined callback deadline guarantee. Logs `build/arp-mod-{browser,ctest,benchmark}-windows.log`.

Committed source **f6e9e2f** builds in native WSL. Linux focused tests pass **2/2
(0.85s)**, and all **66** recorded Windows-browser comparisons replay against
Linux C++ with identical maximum errors above. This is not Linux WebKit audio/UI
validation. Linux ASan/UBSan/leaks pass **1/1 (3.19s)** with no diagnostics.
Twenty automated stages, same warmed configuration: 64-frame mean/p99/max
**119.752/187/255.109us**, 128-frame **243.422/331.008/369.416us**, zero observed
overruns. The checksum agrees with Windows. Runtime source remains unchanged
after this acceptance; Windows additionally reran ownership/allocation tests
after making sine-table initialization explicitly prepare-only.

```sh
cmake --build build/linux --parallel 2 --target aion_arp_tests aion_arp_probe
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_delay_realtime_contract|original_ui_bytes)$'
node tests/ArpModulationWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-modulation-browser-fixtures.json
cmake --build build/overlap-asan --parallel 2 --target aion_arp_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^arp_delay_realtime_contract$'
build/linux/aion_arp_tests --benchmark-modulation
```

Windows used the same focused CTest selection with `-C Release`,
`build/windows-core/Release/aion_arp_tests.exe --benchmark-modulation`, and the
browser command above. Linux logs: `build/arp-mod-{build,ctest,replay,benchmark}-linux.log`,
`arp-mod-asan-{build,test}.log`. Only the generated ignored fixture was copied;
source traveled through the safe committed-snapshot helper. No wrappers,
Standalone/editor changes, dependency upgrade, product voicing, package or push.

Next is bounded per-track storage admission/failure/lifecycle handling before
shared GUI/CLI/MIDI/versioned persistence. Do not enable A or advance to another
effect until those end-to-end gates pass. The original kernel's 84-case JS and
15-case actual-worklet evidence is reused unchanged; this stage does not replace it.

## Causal streaming slice — acceptance

The owner selected no added buffering latency. `ArpDelayGraph` combines the same
worklet math, first-a-rate parameter latch and stereo post-pan; controls and LFO
run on a continuous render timeline. The worklet detector decays once per **128
audio samples**, and internal rotations normalize once per active quantum, not
once per callback or event subdivision. Controls never restart delay/STAY/LFO.

When tail <1e-5, preserve the original left-only wake trigger, but inspect input
causally: freeze worklet history until the first left sample >=1e-5, emit zero
before it, and process that triggering sample immediately. Normalize at wake,
then keep processing the rest of that quantum; accumulate the detector peak
across all slices. The skipped prefix does not advance note/grain/delay state.
This intentionally differs from original lookahead during that wake quantum;
there is no promise of a small bound on later feedback/sequence divergence.
The separate original whole-invocation kernel/oracle remains available and its
regressions must still pass. Direct and streaming APIs require reset between modes.

Acceptance: exact native PCM/state under 1/37/64/128/257 partitions and arbitrary
sample-offset controls; separate explicit causal wake/no-added-latency assertions;
unchanged original factory/worklet/automation/pan browser PCM at existing 1e-6
peak / 1e-5 relative RMS for awake/tail cases; mono/stereo/no input, idle/right-only,
in-place stereo, rate changes, zero blocks, reset, bounded invalid requests, whole
graph prepare OOM rollback, zero callback new/delete, Windows/Linux and sanitizers.
Use fixed 128-sample pan scratch; reject >4096-frame calls before state mutation.
Standalone GUI/CLI/MIDI, storage admission/retirement, persisted voice settings,
editor/track integration and organic defaults remain later connected acceptance.

### Windows streaming evidence

Native exact PCM/state partition tests pass for rates 8..384 kHz, 0/1/2 channels,
1/37/64/128/257 samples, matching in-place stereo, non-quantized control gestures,
real idle/right-only/wake intervals, reset and complete graph OOM rollback.
The causal fixture drains the real detector below 1e-5, sends right-only input
while idle, then introduces a left impulse 17 samples into a 64-frame slice.
The impulse is output at sample 17; only the remaining 47 samples advance the
worklet clock/ring. No processing/control/reset new/delete is observed.

The unchanged original worklet **84-case** CTest passes after extracting the shared
render primitive; its limits were not changed. Full actual original factory,
worklet, smoothSetParam and auto-pan now pass **21 fixtures / 63 comparisons /
4,128,768 stereo frames**, peak **2.384185791015625e-7**, including eight presets,
mono, six rates and live controls at 22.05/44.1/48/96 kHz. Initial combined 96k
automation failed at **2.16066837310791e-6**: Float32 subtraction at event anchors
and deferred-event lane/tail handling must be preserved. The fix affects only
ArpModulation, not the accepted Compressor. A browser-derived `.35 -> 1` anchor
golden regression protects the one-ULP difference. Existing **66** isolated pan/
parameter fixtures still pass, with **zero parameter-sample error** now observed.
Reference arithmetic/clipping was checked against Chromium's
[AudioParamHandler](https://raw.githubusercontent.com/chromium/chromium/main/third_party/blink/renderer/modules/webaudio/audio_param_handler.cc).

The test-only wrapper records first/last worklet parameters for diagnosis; the
original process body and parameters/PCM are unchanged. All original versus
observed factory PCM was compared **bit-identically (0 differing samples)**,
with identical applied-event frames. The initial failing fixture/log is retained
in `build/arp-graph-original-fixtures.json` / `arp-graph-initial-failure-windows.log`.
No gate or tolerance was weakened or failed case removed.

Windows focused CTests passed **3/3 (3.84s)**; the subsequent final anchor/kernel/
streaming regression selection passed **2/2 (2.20s)**. Actual browser log:
`build/arp-graph-final-browser-windows.log`; fixture `arp-graph-browser-fixtures.json`.
Replay requires unchanged original-source hashes. Commands:

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_tests aion_arp_stream_tests aion_arp_probe
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(arp_delay_realtime_contract|arp_delay_streaming_contract|js_native_arp_delay|original_ui_bytes)$'
tools/TestArpDelayWebAudio.ps1 -Graph
node tests/ArpModulationWebAudio.cjs --fixtures build/arp-modulation-browser-fixtures.json
build/windows-core/Release/aion_arp_stream_tests.exe --benchmark
```

Nominal AudioParam clipping also matters: `.01 -> 4` can briefly cancel to below
the worklet's .01 minimum. The original clips arrays and their exposed last value
after timeline calculation. Native now does the same, preventing valid gestures
from failing midway through processing. Native and actual browser endpoint cases
retain this regression at 8/48k, including real ring wrap and full feedback.

Twenty combined DSP graphs at 48k, 512 warm / 2048 measured callbacks initially
showed **13 overruns** at 128 with LoopBank's single-sample call pattern
(mean/p99/max **1497.69/2173.3/3792.6us**). Coefficient caching now computes
tempo/rotation/exp constants only when the latched worklet values change, keeping
the same arithmetic. The retained 84-case unchanged worklet, kernel and stream
CTest selection passes **3/3 (31.80s)** after this optimization.

Final warmed timing: grouped 64 mean/p99/max **492.605/603.1/947.6us**, grouped 128
**957.985/1149.1/1519.9us**; single-sample dispatch **606.741/725.6/821.7us** at 64,
**1172.29/1336.8/1594.4us** at 128. Nonzero PCM, matching checksums, zero observed
overruns/allocations in all four optimized runs. This excludes Engine/EQ/compressor/
mixing/devices and round-trip latency; full-bank headroom still requires its own
gate. Failed timing log `build/arp-graph-precache-overruns-windows.log`, final
`arp-graph-cached-benchmark-windows.log`; do not discard the failed observation.

### Linux streaming evidence and remaining timing limit

Runtime **3f35d82** builds in native WSL. Focused original/kernel/stream/assets
CTest selection passes **4/4 (28.69s)**. All **63** combined browser fixtures and
the existing **66** modulation comparisons replay against Linux C++ with the
same errors above; parameter samples match exactly. These replay Windows
WebView2 PCM, not Linux WebKit rendering. ASan/UBSan/leaks pass **2/2 (20.71s)**
without diagnostics. First sanitizer build regenerated CMake while processing
multiple make targets and did not recognize the new second target; the next
invocation built it successfully. Its log is retained separately, not hidden as
a passed first build (`arp-stream-asan-first-build-failure.log`).

Initial optimized Linux warmed timing (same 20 graphs / 512 warm / 2048 measured):
grouped 64 mean/p99/max **421.771/539.6/598.2us**, no overruns; grouped 128
**846.29/1005.2/3232.1us**, **1** wall overrun. Single-sample 64
**527.474/638.5/4053.3us**, **1** overrun; 128 **1030.31/1321.5/4325.1us**, **1**.
This observation remains in `build/arp-graph-benchmark-linux.log`.

Rather than rerunning until a clean timing result, a test-only
`--benchmark-cpu` diagnostic was added (test source **38d9e6b**). It records
`CLOCK_THREAD_CPUTIME_ID` beside wall time, capturing the first 16 wall misses
without logging or allocating during DSP. All miss counts remain reported.
Grouped 64/128 maximum thread CPU **596.2/1431.7us**, single-sample
**874/1349.6us**; **zero thread-CPU overruns**. One 64-frame single-sample wall
miss measured **2220.3us wall versus 594.4us thread CPU**. That observed miss
contains off-CPU delay; this does not prove the cause of the earlier uninstrumented
outliers or reliable device deadlines. The other three diagnostic configurations
had no wall misses. Checksums match Windows and no callback new/delete is seen.
Log: `build/arp-graph-cpu-benchmark-linux.log`.

Software DSP correctness is verified; **Linux/WSL wall deadline qualification is
still open**. No audio driver/real-time scheduling, Linux WebKit interaction,
listening, full EQ+C+A Engine mix or total round-trip latency has been validated.
Do not mark A production/release-ready or discard these timing misses. Retain
the native sample-rate reuse regression: unchanged values must still recompute
cached coefficients, delay length and LFO rate when preparing at a new rate.

```sh
cmake --build build/linux --parallel 2 --target aion_arp_tests aion_arp_stream_tests aion_arp_probe
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_delay_realtime_contract|arp_delay_streaming_contract|js_native_arp_delay|original_ui_bytes)$'
node tests/ArpGraphWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-graph-browser-fixtures.json
node tests/ArpModulationWebAudio.cjs --probe build/linux/aion_arp_probe --fixtures build/arp-modulation-browser-fixtures.json
cmake --build build/overlap-asan --parallel 2 --target aion_arp_tests aion_arp_stream_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^(arp_delay_realtime_contract|arp_delay_streaming_contract)$'
build/linux/aion_arp_stream_tests --benchmark
build/linux/aion_arp_stream_tests --benchmark-cpu
```

Source synchronization uses committed snapshots; only generated ignored fixtures
were copied to WSL. Linux logs: `build/arp-stream-{build,ctest,mod-replay}-linux.log`,
`arp-graph-replay-linux.log`, `arp-stream-asan-{build,test}.log`. Current product
controls remain disabled; next is bounded track storage/retirement before
shared Engine/GUI/CLI/MIDI/state and organic defaults. No package or push.

Final sample-rate reuse source **d940280** passes Windows native regression,
Linux **1/1 (1.62s)** and ASan/UBSan/leaks **1/1 (18.26s)** without diagnostics.
It compares an existing 48k owner re-prepared at 96k with unchanged controls
against a fresh owner, through real note-clock advancement and stereo PCM.
Runtime DSP remains **3f35d82**; no unchanged passing suites were repeated for
the test-only CPU diagnostic or this added rate case beyond their affected tests.
Logs: `build/arp-stream-rate-{test-linux,asan-test}.log` and
`arp-stream-rate-unit-windows.log`.

Connected-stage reminders: preserve original `repetitions / 11.1` factory mapping,
all eight presets and same-value gesture retargets. The original `updateLoopNode`
also retargets BPM on every Arp edit. Keep original UI ranges and master tempo
metadata; descriptor clipping belongs to DSP. Test-seeded randomness is not a
product-wide shared seed policy. No new parameter/action IDs or saved state were
introduced in this DSP slice.

## Prepared track storage — 10 November 2026

The next internal slice reuses LoopBank's existing edit/export worker; it does
not start another thread or enable product A. `ArpDelayStorage` has forty
construction-time heap slots (twenty active plus twenty replacement capacity).
Only requested slots allocate delay rings. At 48k the worst-case forty rings
consume 92,160,000 bytes (87.89 MiB); at 384k, 737,280,000 bytes (703.13 MiB),
excluding fixed graph/allocator overhead. OOM rejects the replacement and keeps
the old running graph. There is no claim that all devices can admit the maximum.

Audio publishes immutable track/values/seed requests with a release store;
the sole worker prepares at most one per service pass and publishes ready/failed.
Audio adopts only the exact still-pending slot, resetting its fresh graph to the
current sample clock; a stale completion cannot match a newer request. Retired
slots are destroyed by the worker before it publishes them free. Fixed slots
also bound retirement, so a saturated retirement queue cannot leak an owner.
Cancelled queued requests skip allocation; cancellation during preparation may
finish allocating but its result is rejected and retired. No callback scans PCM,
allocates/frees, waits, or uses locks. The worker may be delayed by an existing
clip edit; preparation is asynchronous, not a promised audio deadline.

`LoopBank::requestArpStorage` checks the current clear epoch. DEL/DEL ALL and
timeline reset detach/cancel storage; stopped prepare/rate change and destruction
join the existing worker before releasing all slots. PLAY and ordinary playback
do not recreate storage. These are audio-owner internal methods, not product CLI
commands or a second DSP implementation. Graph pointers must not escape to the
message thread or survive a poll/clear/prepare. Product command acknowledgements,
settings masks, rendering/bypass/tails, GUI/CLI/MIDI/state and organic defaults
remain the connected-stage work. The source DSP and original assets are unchanged.

Acceptance defined before editing: all twenty tracks; exact direct-graph PCM
and clock/state after adoption; OOM preserves active sound; bounded overflow and
full recovery; no obsolete adoption after cancellation/clear/reset/rate change;
worker-only destruction; zero watched callback new/delete at 64/128. New
`arp_delay_storage_contract` includes a test-only allocation barrier: pause the
worker inside allocation, cancel or DEL and request a replacement, then deliver
the obsolete result while the new request is pending. Only the new result may
adopt. It also covers stopped rate changes with 96k PCM, forty-slot exhaustion,
pending shutdown, real LoopBank worker lifecycle and a concurrent handoff stress.
An additional real-worker regression runs twenty Arp preparations during loop
capture/finalization, then twenty replacements alongside NORM and a pinned old-PCM
export. Original and normalized WAV bytes and nonzero connected loop playback
match a reference bank with no Arp jobs exactly at 64/128; the prepared effects
remain disconnected. This protects existing worker/clip leases, not A sound parity.

Runtime storage source **cbf9ab0** and additional shared-worker regression
**ee3edcb**: Windows affected CTests pass **5/5 (706.95s)**, including full core
**693.56s**; the subsequently extended storage binary also passes. Linux CTests pass
**5/5 (240.37s)**, including full core **236.48s**. The extended shared-worker
test passes Linux Release **1/1 (0.79s)** and ASan/UBSan/leaks **1/1 (2.89s)**,
without diagnostics. Initial narrower storage TSAN passed **1/1 (3.49s)**.
After extending the test, its first TSAN launch failed before main with
`FATAL: ThreadSanitizer: unexpected memory mapping 0x5ae62db19000-0x5ae62db1c000`.
That failure is retained, not overwritten by a random retry. Following the
process-scoped workaround documented in the
[upstream sanitizer issue](https://github.com/google/sanitizers/issues/1716),
the same instrumented extended binary passes **1/1 (4.22s)** with
`setarch x86_64 -R`; no race report. This is an address-layout launch change,
not a source/tolerance/suppression change or a global ASLR modification.
Environment: GCC 13.3.0, PIE executable, WSL2 kernel
`6.18.54.1-microsoft-standard-WSL2-aion-midi`. The protected entropy sysctl was
unreadable, so no exact entropy or proven root-cause claim is made.
Reproduce with the existing caches (CMake regenerates for the new test target):

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_storage_tests aion_core_tests aion_arp_stream_tests aion_compressor_connected_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(arp_delay_storage_contract|arp_delay_streaming_contract|core_realtime_contract|compressor_connected_contract|original_ui_bytes)$'
```

```sh
cmake --build build/linux --parallel 2 --target aion_arp_storage_tests aion_core_tests aion_arp_stream_tests aion_compressor_connected_tests
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_delay_storage_contract|arp_delay_streaming_contract|core_realtime_contract|compressor_connected_contract|original_ui_bytes)$'
cmake --build build/overlap-asan --parallel 2 --target aion_arp_storage_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^arp_delay_storage_contract$'
cmake -S . -B build/arp-storage-tsan -DAION_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=thread'
cmake --build build/arp-storage-tsan --parallel 2 --target aion_arp_storage_tests
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir build/arp-storage-tsan --no-tests=error --output-on-failure -R '^arp_delay_storage_contract$'
```

Windows log: `build/arp-storage-ctest-windows.log` (saved full CTest log).
Linux logs: `build/arp-storage-{build,ctest,asan-build,asan-test}-linux.log`,
`arp-storage-tsan-{configure,build,test}-linux.log`,
`arp-storage-shared-{release,asan,tsan}-{build,test}-linux.log` and
`arp-storage-shared-tsan-fixed-layout-linux.log` (all under `build/`). The
`shared-tsan-test` log is the failed startup and must remain visible.

Connected-stage requirement: unlike fixed-storage compressor preparation, Arp
cannot report terminal success from the first enable enqueue. Extend the shared
Engine command/outcome and restore-bank paths so settings/receipts commit only
with matching prepared storage; rejection/OOM must retain old settings/audio.
Keep the graph's clear-epoch/slot guards and never use `prepare` as a render-time
fallback. Seed selection, chain placement/bypass/tails and control retargeting
remain product integration, not guarantees of this internal storage layer.

No UI/bridge/audio math changed, so the existing browser/original-JS sound evidence
is retained rather than rerun unchanged. This storage slice is not full A
acceptance, device/host qualification or a package. Existing WSL wall-timing
misses remain recorded above; no new runtime performance claim is made.

## Engine preparation transaction — 10 November 2026

`EngineArpPreparation.cpp` connects asynchronous storage preparation to typed
terminal receipts, leaving product A and its audio chain disabled. The internal
`ArpPreparationRequest` contains raw DSP targets, explicit seed, clear epoch,
track, request ID and Engine generation. This is not a new product parameter,
preset, persistence schema or user CLI. The component's raw default feedback .18
must not replace the original factory's `2 / 11.1` mapping in the product layer.
No DSP, assets, dependencies or existing IDs changed.

Acceptance: success only after actual adoption; exact correlation; old graph
preservation on OOM/busy/cancel/stale input; bounded closed-consumer behavior;
cancellation before adoption and no obsolete success after DEL/panic/prepare;
independent ordinary loop acknowledgements; zero watched callback new/delete
at 64/128. A single message owner enqueues and reads results. Audio owns pending
records; the existing LoopBank worker alone allocates/frees rings. Outstanding
message ownership lasts until a terminal receipt is read, limiting all accepted
unread transactions to twenty. Both 31-usable-entry SPSC queues exceed that
bound. Unexpected result-queue failure retains the completed receipt for a later
callback; it never spins or silently drops it.

The callback checks cancellation/epoch/generation before `LoopBank::beginBlock`
can adopt a ready result. Enqueue has no success receipt. Terminal statuses are
`ready`, `cancelled`, `stale`, `busy` and `failed`. Preparation failure keeps old
audio ownership. Cancellation after an already-committed ready result cannot undo
that commit, including while its receipt remains unread. A cancellation racing
the callback's pre-adoption revision check may finish ready or cancelled; the
terminal receipt identifies which operation won. Results carry their
original generation across prepare; callers must not treat an old ready result
as evidence of a current graph. Prepare never clears message-owned outstanding
flags or discards unread results. Zero-frame processing may service this internal
transaction without advancing audio, matching the existing export path; ordinary
loop commands retain their zero-frame exclusion.

DEL, DEL ALL and STOP ALL cancel unfinished work. Clear-merged-source checks
the actual source's changed epoch without cancelling an unrelated destination.
The protocol has bounded work: at most twenty pending owners and twenty queued
requests examined per service. There is no new thread, render-time prepare,
second audio engine, product retry or browser/JSON work on audio.

The new `arp_preparation_contract` checks correlation and stale/invalid values,
OOM rollback/recovery, ordinary acknowledgements, all twenty unread receipts with
saturated ordinary telemetry, queue-independent cancellation, zero-frame clocks
and 64/128. Allocation barriers hold obsolete worker preparation while cancellation,
DEL, DEL ALL or panic and a replacement arrive. Forty-slot exhaustion returns
explicit busy receipts without damaging the old graph, then recovers after worker
reclamation. Repeated prepare distinguishes unread already-ready receipts from
queued unfinished work. A separate message/audio/worker test exercises actual
concurrent SPSC traffic and cancellation linearization. These are internal
protocol/ownership tests, not rendered A audio or GUI/CLI acceptance.

Source **eebbbed**: final extended protocol CTest passes Windows **6.51s**,
Linux Release **0.43s**, ASan/UBSan/leaks **1/1 (1.01s)** and TSAN **1/1
(1.60s)**, with no sanitizer report. TSAN uses the already documented
process-only fixed-layout launch, retaining the earlier storage startup failure.
Storage, connected-compressor and original-assets regressions also pass per OS.
Linux's complete affected selection passes **5/5 (221.21s)**, core **218.58s**;
Windows passes **5/5 (751.16s)**, core **728.43s**. Runtime/tests remain
**eebbbed**; subsequent edits finalize evidence only. No package or push.

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_arp_preparation_tests aion_arp_storage_tests aion_compressor_connected_tests aion_core_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(arp_preparation_contract|arp_delay_storage_contract|core_realtime_contract|compressor_connected_contract|original_ui_bytes)$'
```

```sh
cmake --build build/linux --parallel 2 --target aion_arp_preparation_tests aion_arp_storage_tests aion_compressor_connected_tests aion_core_tests
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(arp_preparation_contract|arp_delay_storage_contract|core_realtime_contract|compressor_connected_contract|original_ui_bytes)$'
# Existing sanitizer caches from the storage section; first build regenerates Make rules.
cmake --build build/overlap-asan --parallel 2 --target aion_core
cmake --build build/overlap-asan --parallel 2 --target aion_arp_preparation_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^arp_preparation_contract$'
cmake --build build/arp-storage-tsan --parallel 2 --target aion_core
cmake --build build/arp-storage-tsan --parallel 2 --target aion_arp_preparation_tests
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir build/arp-storage-tsan --no-tests=error --output-on-failure -R '^arp_preparation_contract$'
```

Logs: `build/arp-preparation-{build,ctest}-windows.log`, and native WSL
`build/arp-preparation-{build,ctest,asan-build,asan-test,tsan-build,tsan-test}-linux.log`.
The test logs are saved complete CTest `LastTest.log` files, including captured
core output and individual acceptance results.
No original-JS/browser PCM rerun is required for this internal transaction-only
change; verified DSP/source math is unchanged. No user-facing UI/CLI was enabled,
so actual interaction, product persistence, chain order/bypass/tails, organic
defaults and driver/full-bank audio headroom remain connected-stage gates.
In particular, `cancelArmed` currently calls the raw bank clear helper and restores
EQ/C settings. Preserve future Arp product settings/runtime through that workflow
when connecting A; this raw preparation protocol alone does not port that policy.
Product integration must commit settings in the same audio callback as matching
graph adoption, before rendering or snapshots, rather than enqueue a later
settings write after a ready receipt. The raw protocol changes no product
settings. Its twenty independent receipts are not an atomic FX-bank restore;
restore still needs staged all-bank readiness/OOM rollback and existing GUI/CLI
gesture barriers before A can be enabled.

### Owner clarification: editable effect chain (10 November 2026)

Q -> C -> A describes the beginning of the original default `QCATFODBVKZG`,
not a fixed audio path. The unchanged reference's `Loop.setSignalChain` accepts
a per-loop order, uppercases valid effect codes, removes duplicates preserving
their first occurrence and falls back to the default for an empty valid result.
`AudioGraph.buildEffectsChain` follows that order and includes only enabled
effects present in it; rebuilding can reuse effect nodes and their state.

Before editable-chain candidate `6d731ac`, native rendering called EQ then
compressor directly and supplied default-only markup. That candidate now connects
manual GUI/CLI/state ordering for Q/C. Its live-state policy was owner-approved
and software acceptance closed in the `097c1cd` follow-up, recorded in
[SIGNAL_CHAIN_MIGRATION.md](SIGNAL_CHAIN_MIGRATION.md). When connecting A, use
the shared representation and place every ported
effect at its user-selected position. Retain the original effect codes without
presenting unported/custom effects as working. Preserve the approved persistent
EQ/compressor/Arp ownership through reorder and playback restarts; explicitly
verify omission/bypass and delay tails against the reference and approved policy.
Acceptance needs order-sensitive connected PCM at 64/128, live reorder, queue
rejection/cancellation, clear/reset, save/restore and editor reopen evidence.
The existing default-order DSP results do not establish editable-chain parity.
