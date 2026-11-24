# Current migration checkpoint

## Current target policy — 30 October 2026

The updated owner prompt makes Windows and Linux **Standalone only** for routine
work. VST3/REAPER, LV2/Ardour and CLAP remain deferred/opt-in, preserving code,
IDs and historical tests. Current CMake defaults and desktop presets disable
those wrappers. Use explicit `-DAION_BUILD_VST3=ON`, `-DAION_BUILD_LV2=ON` or
`-DAION_BUILD_CLAP=ON` for a relevant wrapper task. Earlier format/build evidence
below is historical, not a requirement to rebuild wrappers on each engine fix.

## Active work — core loop-track buttons, 9 November 2026

### B / prepared stereo wet processor — 24 November 2026

Internal `Reverb` now composes stereo distributed convolution, 0..250ms
fractional pre-delay with constant-sum 20ms read-head fades, wet-only Q=.5
Nyquist-safe TPT damping, linear dry/wet mix and post-mix volume. Gains/enable/
derived damping smooth over 10ms; coefficients cache when stationary. Controls
and route order credit Ascii-Looper; existing AudioFFT/Ooura notices unchanged.
No new third-party code, dependencies, IDs/schema, preserved UI or app controls.

Worker preparation atomically owns both channel kernels plus pre-delay storage;
all 40 tested allocation/cancel sites preserve complete old running stereo PCM.
Audio `set` rejects Duration/Decay shape changes (worker replacement required).
Reset invalidates pre-delay history without scanning storage, clears fixed filter/
kernel state and never allocates. Settled bypass is exact dry and clears old tails;
reenabling ramps fresh wet audio. Ordinary playback restarts must retain memory.

Windows focused stereo processor 1/1 passed (1.14s), including 72 rate/delay/block
combinations + in-place, all 17 presets, generated 8/48kHz IR audio, independent
direct-form/FIR oracle within unchanged `3e-6`, live sample-indexed edits, bypass
tail erasure, stereo isolation/swapped buffers, reset and sample-rate replacement.
Source `ccaa80c`: final Windows and native WSL Linux probes both report peak
`1.19209e-07`. Linux Release 1/1 passed (0.78s), core-only Debug ASan/UBSan/leak
1/1 passed (25.02s), no reported sanitizer errors/suppressions. Debug,
address+undefined/frame-pointer and plugin-OFF cache flags were confirmed intact.
These are test wall times, not callback deadline/latency measurements. Diff checks
passed and preserved UI/reference/upstream FFT sources unchanged. Credits index
deduplicated in documentation-only follow-up; no passing audio suites repeated
for that cosmetic edit. See REVERB.md for exact commands/ownership limits.

B stays disabled pending bounded whole-graph memory admission/worker adoption/
retirement and shared product GUI/CLI/state/MIDI integration. No Standalone/UI,
listening, device, CPU/deadline or release claim. No push or package. NEXT is
per-loop prepared graph ownership before exposing any B controls.

### B / distributed non-uniform tail and visible credits — 24 November 2026

Internal `NonUniformConvolver` now uses a 128-frame partitioned head for the first
1024 IR samples and a delayed 1024-frame tail. Tail latency matches the IR offset,
so the complete kernel remains zero-latency. Past-partition products are spread
over consumed samples; FFTs run once per completed tail partition. Fixed scratch,
transactional worker preparation and valid-history resets retain old state on
OOM/cancel/invalid input. It reuses the already pinned AudioFFT/Ooura sources;
no dependency or third-party source changes. No product B controls enabled.

Windows Release focused kernels 2/2 passed (1.48s). Expanded shared fixture keeps
the old uniform checks/tolerances, adds split boundaries, 19 non-uniform OOM
sites (uniform still nine), cancellation at every allocation with partial old
tail history, and bounded tail-work counts at 64/128. Independent peak PCM error:
non-uniform `5.93611e-08`, uniform `6.81991e-08`, both below unchanged `3e-6`.
Source `8555fb3`: native WSL Linux Release 2/2 passed (1.32s); core-only Debug
ASan/UBSan/leak 2/2 passed (29.12s), no reported sanitizer errors/suppressions.
Both old/new contracts ran with their expanded checks and unchanged tolerances.
Sanitizer Debug/address+undefined/frame-pointer/plugin-OFF cache flags confirmed
unchanged. Package helper PowerShell parser check passed; diff check clean and
preserved UI/reference/upstream AudioFFT files unchanged. No Standalone/UI or
device test was required/run for this unconnected core slice. See REVERB.md.

Owner's attribution emphasis is recorded in DEVELOPMENT_WORKFLOW.md. New root
CREDITS.md names authors/code use/source revisions/licenses/modifications and
separates inspiration from copied code, linked from README/licensing and included
by install/package rules. Full original notices remain. No package generated.

NEXT: complete the prepared stereo wet processor (pre-delay/damping/mix/gain),
then bounded per-loop graph ownership and shared GUI/CLI/state/MIDI integration.
Whole-graph memory admission, twenty-track deadlines, listening/devices and live
transition acceptance remain pending. No push/package/production claim.

### B / convolution kernel and community reuse — 24 November 2026

New internal zero-latency `PartitionedConvolver` reuses unmodified MIT AudioFFT
from FFTConvolver revision `f2cdeb04c42141d2caec19ca4f137398b2a76b85`, portable
Ooura path only. Vendored hashes, MIT/Ooura attribution and install/package
notice rules are retained. No JUCE upgrade or platform runtime dependency.
AudioFFT source audit found prepared FFT/ifft arithmetic only, with no callback
allocation/locks/I/O. Aion's history layer adds transactional worker preparation,
retained old running state on OOM/cancel/invalid IR and fixed-size reset with a
saturating valid-history count instead of freeing or scanning IR-size history.

Windows Release 3/3 focused convolution/preparation/original-settings tests passed
(0.89s). Convolution probe reports peak independent PCM error `5.61065e-08`
under the fixed `3e-6` ordinary-audio tolerance, nine OOM sites, eight rates,
stereo isolation, 64/128/variable/in-place blocks, long tail/ring wrap and reset.
The first extreme-input fixture demanded exact float FFT zeros next to a 1e6
impulse; it failed, then was replaced by bit-exact manually sanitised input plus
exact all-nonfinite silence/finite-output checks. No DSP or FIR tolerance changed.
Native WSL Linux Release 3/3 passed (1.09s); core-only Debug ASan/UBSan/leak
3/3 passed (14.72s), with no reported errors/suppressions. Review added an
isolated `1e-8` tail checked within `1e-13`, silent IR and successful replacement
checks. Final expanded kernel at `ca61094` passed Windows 1/1 (0.56s), Linux 1/1
(0.52s), sanitizer 1/1 (13.67s). Windows/Linux probe peaks match. Unchanged
preparation/settings suites were not repeated for the test-only expansion.
Pinned AudioFFT bytes/hashes also match the native WSL checkout; Git attributes
explicitly preserve them. See REVERB.md for exact commands and acceptance limits.

B remains disabled: no product controls, GUI/CLI/MIDI/state/routing change. This
uniform primitive is not twenty-track CPU acceptance; NEXT use a non-uniform tail
arrangement, then complete wet controls and bounded per-loop graph ownership
before exposing B. No community presets/IR assets imported: Dragonfly/Signalsmith
were researched, but their different algorithmic controls require explicit
mapping/listening/rights checks. Existing 17 presets retained. No package/push.

### B / reverb offline preparation foundation — 24 November 2026

Started the next loop effect. New native settings retain six original controls,
eight room mappings and 17 room-first presets. The unchanged original handlers
match 93 settings/routing cases against the independent reference. Offline IR
preparation provides deterministic stereo noise, a fully responsive Decay range,
smooth zero endpoints, weighted DC removal and unit channel energy. IR-key reuse
avoids new/free for unchanged shape or live-only controls. Replacement validation,
either-channel OOM and cancellation retain the complete old PCM/metadata.

This stage is internal: B stays disabled, with no new product GUI/CLI/MIDI/state
or callback processing. The test catalogue is not a Standalone command. Maximum
raw IR storage is bounded to 15,360,000 bytes (384kHz/5s), excluding future FFT/
owner storage and old+candidate overlap. Cancellation checks every 1024 samples
do not replace future generation/adoption fences. No dependency/version change.

Source checkpoint `bb5b6f6`: final Windows Release 2/2 passed (0.39s), native WSL
Linux Release 2/2 passed (0.57s), core-only Debug ASan/UBSan/leak 2/2 passed
(1.59s), no reported sanitizer errors/suppressions. The first Makefile sanitizer
build needed explicit cache regeneration to discover the new target; original
Debug/address+undefined/frame-pointer/plugin-OFF settings were preserved and
checked. These are preparation/property/settings tests, not actual convolution
or 64/128 callback acceptance. Read [REVERB.md](REVERB.md) for contract, commands, backend
research, scope and next step. NEXT: audited convolution kernel with independent
PCM/latency/reset/RT tests, then bounded per-loop ownership and shared product
GUI/CLI/persistence/MIDI integration. No placeholder B controls or new package.

### Requested Windows Standalone alpha — 23 November 2026

Owner requested commit/push and an alpha package. All preceding local work was
pushed to origin/main; packaging manifest checkpoint `a156e21` corrects the stale
claim that Delay GUI/CLI/repeats were unavailable. The alpha includes Standalone,
optional Python CLI, current docs/notices and an exact committed Git source ZIP.
Archive naming: `dist/Aion-Looper-0.4.3-<revision>-win64-Standalone-alpha.zip`,
with a sibling SHA-256 file. No installer, plugin or Linux package was requested.

The committed Windows Standalone was built and packaged, then extracted into a
new verification directory. Packaged executable and CLI hashes matched their
build/source files; the embedded source included the repeat-selector repair;
manifest revision/hash and absence of test hosts/plugin binaries were checked.
Executable SHA-256:
`53C217312E824147103E1CF8C587F41745B91F758EAE58CE2ACB56727B1BD85D`.

Two unchanged extracted-app smoke attempts failed the existing `Disabled flash
clears` assertion: native OFF was observed but the UI button still showed green
at the 100ms check. Logs `build/alpha-package-smoke.log` and
`build/alpha-package-smoke-retry.log` retain both failures. Native test RPCs and
the editor's independent snapshot delivery are different observations. The
fixture now separately observes actual button `aria-pressed` ON/OFF before the
original unchanged 100ms colour assertion. It retains every prior assertion and
adds UI-state checks; no app, audio, timer threshold or metronome DSP changed.

Final full smoke on the extracted executable passed:
`build/alpha-package-smoke-ready.log`. Scope: actual WebView2, 20 original cards,
IPC/parameter/expired-command checks, audio-owner acknowledgement, MIDI/control/
name/reload checks, disabled unported controls, no legacy AudioContext, MASTER
zero, 44.1kHz device counters. It does not establish physical capture, driver
latency, controller-pair, listening, plugin or release acceptance. The final
package is regenerated with this test/handoff source; verify its executable
hash against the same tested binary. Outstanding licensing/release checks remain
in the package's TEST-BUILD manifest and LICENSING.md.

### Delay repeat selector synchronization repair — 23 November 2026

Implementation checkpoint: `56299ee` (also records the owner's quality-first
Delay clarification in AGENTS.md and the development guidance).

Confirmed bug: with Exact count 3 focused, the shared native command accepted
count 2 but the DOM remained at 3. `build/delay-repeat-focus-before.log` retains
the actual 64-sample failure. Feedback/Exact had the same focus-only exclusion.

Native state now refreshes focused selectors. Only the latest queued/in-flight
local repeat gesture temporarily owns their values, preserving rapid selections
through coalescing/older acknowledgements. Payload identity fences completion;
the existing send path releases ownership in `finally` on success, rejected
RPC/receipt or disabled audio. Unsent intent expires on the existing three-second
queue deadline; selected-track/generation/clear changes and pagehide invalidate it.
Dependent Reps/count enablement follows the displayed mode. No new DSP, timers,
audio-thread work, parameters, commands or state schema; shared CLI is unchanged.

Windows Standalone/test host builds succeeded. Full actual editor workflow
passed at 64/128, including focused count/mode native changes, existing rapid
1/2/3/10 choices, presets/ranges/Sync, MIDI, reload, nonzero playback and immutable
clip identity. Four affected Windows frontend/bridge/native external CLI tests
passed (49.01s). Final frontend tests also execute the actual production send
function's early-return/rejection/receipt paths and pass. Linux Standalone and
command target builds succeeded from the committed native-storage snapshot;
the same four Linux frontend/bridge/CLI gates passed (28.20s), including both
block sizes. No DSP rerun or new sanitizer claim: audio implementation is unchanged
and prior scoped native/audio/ownership evidence is retained.
Original preserved resources, reference checkout and ASCII logo are unchanged.

NEXT: begin B / convolution reverb as the next unported loop-effect slice. Inspect
the original six controls (volume, mix, duration, decay, damp, predelay), room/
preset behavior and impulse preparation before defining native quality/storage
acceptance. B remains disabled until its native end-to-end slice is ready. Keep
it an independently reorderable chain stage, sharing GUI/CLI state and DSP.
The current Delay development scope has no known blocking failure in its executed
checks; discrete-selector MIDI, physical multi-controller/device, listening and
release coverage remain explicit separate work. Browser PCM identity is not a
Delay gate. No routine package/push or broad CPU profiling.

### Owner clarification: Delay quality over original sound — 23 November 2026

The owner explicitly prefers a better-quality native Delay over an identical
Ascii-Looper Delay. Exact original browser PCM/rounding/filter/transition sound
is no longer a D acceptance gate. Preserve interface/controls/IDs/state and both
Feedback/exact repeat workflows; assess sound quality, accurate timing, stereo,
stable feedback/damping, smooth edits and real-time ownership. The independently
verified native fractional read remains unchanged. The strict browser fractional
comparison below is a retained optional diagnostic, not a defect or blocker
under this clarified contract. Do not keep rerunning or trying to match it.
NEXT: finish practical native Delay quality/lifecycle acceptance and proceed in
roadmap order. Keep listening/device/release limits explicit; they do not make
browser identity a gate again. No DSP change or audio-suite rerun is needed for
this acceptance-policy correction.

### Delay stereo quality slice — 23 November 2026

Source/test snapshots `1d43d93` and `aa4c25a` add independent stereo audio
verification. They do not change product DSP/parameters/UI/assets or introduce
another effect.
Previous connected and Engine cases remain. Two isolated dry captures recover
conditioned left/right PCM before the original final mono merger. Independent
fractional-read/wet-pan/loop-pan math checks unequal channels, five loop-pan
positions, hard-left/right silence and immutable source/history. Actual Engine
callbacks cover 8/48 kHz, 64/128, offset-17 prepared admission and receipt-owned
oscillator age, Feedback=0 and exact1, maximum/intermediate wet speed/depth and
zero tracked callback heap. The existing `2e-6` error threshold is unchanged.

Windows connected gate passed (9.85s); final Engine gate passed (2.84s), maximum
new stereo Engine error `1.01877e-8`. Component/original JS settings passed 2/2
(0.45s). Initial native WSL Linux four selected gates passed (5.11s).
The first combined sanitizer run timed out the enlarged LoopBank matrix at
60.08s (3/4 overall); it is retained as a failed run. `aa4c25a` registers the
additional stereo cases separately per rate/block, retaining every case,
assertion and original 60s watchdog. Final split results: Windows Release
12/12 (12.60s), native WSL Linux Release 12/12 (5.23s), core-only Debug
ASan/UBSan/leak 12/12 (98.40s), no reported sanitizer errors or suppressions.
Longest new sanitized LoopBank case: 16.08s. The final Windows explicit-header
cleanup compiled successfully; it does not change tested behavior. This is
fixture batching, not a product CPU optimization. Test-only changes do not need
a product Standalone/GUI/CLI rebuild.

Actual WebView2 OfflineAudioContext, using byte-checked unchanged reference
factories: explicit integer-delay subset passed 60 stereo/pan cases, 983,040
stereo frames, maximum error `3.273100349573954e-7`. **The default strict
fractional browser diagnostic still fails** at 8 kHz/.05017 seconds, maximum
`5.906564183533192e-6` above unchanged `2e-6`. Measured browser fraction is about
.359375 versus requested native .36, consistent with Float32 browser read-index
rounding; native independently follows the requested high-precision timing.
No DSP/reference/assertion/tolerance was changed to manufacture a pass. The
fractional case, its failure and exact PCM remain; `-IntegerOnly` is an additional
isolating diagnostic, not a replacement or full browser-parity claim.

Exact reproduction, logs, primary-source explanation and scope are in
[DELAY.md](DELAY.md#independent-stereopan-acceptance--23-november-2026).
The owner's clarification above supersedes the former NEXT step to resolve the
fractional browser difference. It remains recorded, with no browser PCM identity
claim, but is not a native Delay acceptance blocker. Remaining D listening/
device/lifecycle acceptance and simultaneous physical-controller checks retain
their separate scope. No package/push, broad CPU tuning or deferred-wrapper
build was performed in this verification slice.

### Owner requirement: simultaneous MIDI controllers — 23 November 2026

Support multiple hardware MIDI inputs concurrently, explicitly including an
FCB1010 and M-Audio Oxygen 49 together. This applies throughout the remaining
looper/effect/synth migration. Standalone already enables inputs independently;
the current processor receives a merged stream and bindings identify message
kind/number/channel, without device identity. Independent device-aware Learn/
routing remain work; channel-scoped Learn below provides collision resolution
when controllers send on distinct channels. Neither physical unit was tested here.
Acceptance must cover concurrent performance/control messages, saved routing/
mappings and one device being disconnected without disabling the other input.

### Channel-scoped Learn — 23 November 2026

Small acceptance slice for concurrent controllers: keep independent input enabling,
add an exact-channel Learn option through the shared GUI/CLI request, consume only
the matching Learn packet, preserve other-channel performance and same-block
provisional CC movement, reject omni/channel overlaps and preserve saved bindings.
This extends the original any-channel Learn workflow; it does not change DSP.

The native MIDI panel now has **Next Learn: Any channel / Channel 1–16**. This is
a temporary editor preference affecting subsequent Learn gestures only. CLI:
`python tools/aion.py midi learn --data '{"target":150,"channel":16}'`.
An omitted/zero channel retains omni semantics; typed invalid filters reject
without changing accepted intent. Packed binding channels were already persisted,
so schema 8, 153 target IDs and all legacy state readers remain unchanged.
Audio adds one bounded channel check; it retains the existing fixed overlap scan,
queues and cancellation/held-action ownership. No new audio storage or locks.

Windows Standalone/test-host builds succeeded. `native_delay_midi_64/128` passed
(10.26/10.56s), `native_frontend_commands` passed (5.78s), retained
`native_midi_processor` passed (4.64s), and external `native_delay_commands_64/128`
passed (15.09/16.35s), including channel Learn and invalid-filter rollback through
the real Python client/mailbox. Actual WebView2 tests passed at 64/128 using real
key selection and right-click Learn, interleaved channel-1/16 packets and reload.
Core `--midi` passed, including added preassigned/provisional channel-1 volume
and channel-16 pan sharing CC11: exact nonzero stereo PCM against the retained
scheduled-control oracle, at both block sizes, with zero tracked callback heap.
Unchanged-JS MIDI comparison passed 16,384 default messages plus 4,256 mixer
messages (maximum error 5.914e-8), and retained Learn/hold/release/focus/PC gates.
The initial CTest JS invocation failed to spawn its absent `build/windows` probe;
the actual comparison above ran successfully with the rebuilt core-cache probe.
Committed source `6d5601c` was synced with the non-destructive WSL helper.
Linux Standalone and affected targets built successfully. Focused core `--midi`
and the unchanged-JS oracle passed with the same exact PCM/error limits above;
the seven selected CTest MIDI/CLI/frontend/reference cases passed (44.41s).
The optional `xvfb-run` invocation could not start because it is not installed;
the successful suite used the existing WSL environment directly. No Linux
WebKit interaction, physical devices, sanitizers or deferred wrappers were tested
in this slice; existing broader evidence is unchanged.

Executed Windows commands (from repository root; CMake/CTest use the pinned
`.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin` tools):

```powershell
cmake --build build/windows --config Release --target aion_delay_midi_processor_tests aion_midi_processor_tests aion_mouse_host AionLooper_Standalone -j 6
cmake --build build/windows --config Release --target aion_delay_command_tests -j 6
ctest --test-dir build/windows -C Release -R '^(native_delay_midi_(64|128)|native_frontend_commands|js_native_midi)$' --output-on-failure
ctest --test-dir build/windows -C Release -R '^native_midi_processor$' --output-on-failure
ctest --test-dir build/windows -C Release -R '^native_delay_commands_(64|128)$' --output-on-failure
cmake --build build/windows-core --config Release --target aion_core_tests aion_midi_probe -j 6
build/windows-core/Release/aion_core_tests.exe --midi
node tests/MidiParity.cjs build/windows-core/Release/aion_midi_probe.exe
tools/TestDelayUi.ps1 -MidiOnly -Blocks 64,128
```

Executed WSL commands (build/tests run in `/home/jorge/projects/Aion-Looper`):

```sh
bash /mnt/d/Jorge/Documents/Codex/Aion-Looper/tools/SetupWslWorkspace.sh /mnt/d/Jorge/Documents/Codex/Aion-Looper
cmake --build build/linux --target aion_core_tests aion_midi_probe aion_delay_midi_processor_tests aion_midi_processor_tests aion_delay_command_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --midi
node tests/MidiParity.cjs ./build/linux/aion_midi_probe
ctest --test-dir build/linux -R '^(native_delay_midi_(64|128)|native_delay_commands_(64|128)|native_midi_processor|native_frontend_commands|js_native_midi)$' --output-on-failure
```

Limits/next step: messages carry channel but no device identity. Set the FCB1010
and Oxygen to different channels for overlapping controls. Verify both physical
units together, saved device enabling and disconnect/reconnect; synthetic channel
streams cannot establish those results. Continue remaining loop features after
this scoped acceptance; retain device-aware routing as pending.

### Delay continuous MIDI Learn — 23 November 2026

Source `4c2680f`, fixture/compatibility follow-up `4f7bb09`, retained-intent fix
`f624b24`: six original D sliders append CC targets 147–152, leaving every older
ID unchanged. MIDI schema 8 writes 153 slots and reads versions 1–7; v7 retains
its PC0, mixer/drive bindings and channel filters, with new slots unassigned.
GUI right-click Learn, Shift-clear, Escape and existing CLI `midi` commands share
the same mapping engine and selected-loop tab. Sync/bypass/repeat selectors are
not mapped by this six-slider slice. Physical controllers remain untested here.

Audio applies controls at MIDI sample offsets with no allocation, preserving
prepared D mode/count/storage. A fixed 511-usable-receipt queue rejects before
mutation on saturation; the sole Delay owner consumes exact original values and
revisions, including immediate save and clear/reset epochs. Generation-bound
admission pauses D MIDI until owner binding and joint-restore confirmation.
Unresolved retained restore failures stay paused; a control on another track
cannot discard that retained intent or falsely report active audio readiness.

Unchanged-JS comparison passed 12,768 original values. Nonzero stereo Engine
comparison passes six offset-17 controls at 8/48 kHz and 64/128, unchanged 1e-6
limit, with clear/suspend recovery and zero tracked callback new/delete. Windows
native MIDI tests cover all CCs/channels, same-block Learn, bounded overflow,
channel filtering, generation changes and legacy state. Full results, fixture
failure diagnoses and exact commands are in [DELAY.md](DELAY.md).

Windows broad compatibility: 13/13 (104.70s); final affected retained-intent/
MIDI/state/joint/lifecycle cases: 8/8 (24.09s). Linux broad: 18/18 (61.91s);
final follow-up: 8/8 (22.73s). Core-only ASan/UBSan/leak: 3/3 (9.41s), no
reported sanitizer errors/suppressions; no sanitized JUCE Processor claim.
Both Standalone builds succeeded. Actual Windows editor full workflows passed
at 64 and separately at 128; final focused MIDI run passed both blocks together.
The fixture now waits for a fresh editor snapshot before asserting the focused
DOM, rather than mistaking native admission for display completion. Numeric
tolerance tightened to the 0.005 slider half-step. Early startup timeouts are
recorded; the helper reuses `--steady-start` and document readiness without
changing production visibility policy. No physical-device, Linux WebKit,
Apple/wrapper or new broad CPU validation was performed.

NEXT: remaining D lifecycle/quality and listening acceptance; document/verify
physical-controller behavior when hardware is available. Do not promote D as
release-complete or begin another effect with known acceptance failures. Broad
CPU tuning, deferred wrappers, Apple and routine packages/pushes remain out of scope.

### Shared Delay GUI/CLI preview — 23 November 2026

Snapshots `921788e` and `4018039` expose the existing D owner through shared
native commands and the running-app CLI. Original-style main/LIVE toggles and
the Delay panel now cover bypass, nine presets, all original ranges/Sync, reset,
Feedback and exact 1–10 repeats. Native settings persistence remains version 1.
Tagged WebView D settings use round-trip precision like Arp; command completion
requires the exact D receipt, not a global transport ID.

The extended UI test caught rapid repeat changes reaching a still-crossfading
owner. Unsent repeats now coalesce and GUI/CLI wait on audio-published readiness;
revision fencing prevents an unread snapshot from reporting false readiness.
Storage/crossfade limits and failure rollback are unchanged. No callback
allocation, lock, timer or DSP math was added. The earlier UI-only reload failure
was an off-screen hit target: the fixture now re-observes visibility before real
pointer input, retaining its disabled/hit assertions.

Final Windows native Processor/CLI tests: 12/12, 47.47s. Native WSL Linux:
14/14, 27.28s, including frontend/bridge plus the twelve native cases. Both
Standalone builds succeeded; tracked callback new/delete counts remain zero.
Windows frontend/bridge scripts also passed directly. Exact commands, UI results
and current limits are in [DELAY.md](DELAY.md); usage is in [CLI.md](CLI.md).
`tools/TestDelayUi.ps1` passed at both 64/128 with all original ranges/Sync,
presets, Feedback/exact 1/2/3/10, reset, round-trip precision, reload/bypass,
continuing nonzero synthetic PCM and unchanged clip identity; no runtime exceptions.
Actual Standalone `NativeUiSmoke.cjs` also passed in isolation, master zero/no
AudioContext. An earlier concurrent run failed its existing 100ms metronome
flash-colour assertion; the final unmodified check passed. No metronome code or
test threshold changed. Keep that first failure recorded, without inventing a cause.

NEXT: D MIDI Learn through this same owner, listening/quality acceptance and
remaining relevant lifecycle/physical-device checks. Do not mark all Delay
acceptance complete or begin the next effect yet. Linux WebKit interaction,
Apple/wrappers, new sanitizer and broad CPU tuning were not run for this slice.
No package/push requested. Earlier disabled-control checkpoints below are history.

### Versioned Delay state — 23 November 2026

Source snapshot `af7f7e1` adds `LoopDelays` version 1 to the existing native
binary state. It stores all twenty track IDs, original double-precision controls,
enabled/preset identity and additive Feedback/exact repeat mode/count. Missing
banks in older native sessions restore original defaults/off. Unknown versions,
incomplete/duplicate/malformed rows, invalid ranges and inconsistent presets
reject the whole load before APVTS/MIDI/FX mutation or cancellation.

Validated D intent enters the existing joint restore owner, never an independent
bank. Immediate saves retain the newest accepted load; old FIFO-clear
acknowledgements cannot erase it. A newer DEL after adoption still resets its
own track. D commands and product controls remain disabled.

The public binary-state fixture covers exact roundtrips, 66 malformed-load
rejections per block size (including a valid pending load), legacy defaults,
clear/load order, unread/saturated telemetry, stopped/changed-rate prepare and
two editor-free instances. Initial back-to-back test loads hit the existing
20ms transition busy rejection; ordinary roundtrips now explicitly drain that
transition, while a separate zero-frame case proves busy rollback and explicit
later recovery. No assertion or tolerance was loosened and production admission
behavior is unchanged. Commands, executed results and limits: [DELAY.md](DELAY.md).

Windows Release: final new state tests passed 2/2 (2.27s); rebuilt related
Processor regressions passed 8/8 (21.09s) separately. Native WSL Linux Release:
all ten affected cases passed 10/10 (9.53s). Both Standalone builds succeeded.
Actual rebuilt Windows WebView2 `NativeUiSmoke.cjs` passed (master zero, no
product AudioContext); this is compatibility smoke, not enabled D interaction.
Tracked callback new/delete counts remain zero. No new sanitizer, listening,
physical-device, Linux WebKit, wrapper or Apple check was run for this slice;
previous unchanged core DSP/reference/ownership evidence is reused.

NEXT: shared Delay GUI/CLI commands and existing-style control integration, then
applicable MIDI/interaction and listening acceptance. Do not mark the whole Delay
feature complete or start the next effect yet. No routine package/push or CPU tuning.

### Processor joint-effects ownership — 23 November 2026

Source snapshot `0ea9da9` connects the existing Processor FX restore to the
coordinated Arp/D bank. Delay confirms only the exact admitted receipt and a
current-generation snapshot at or after its callback serial, with matching
per-track epochs/revisions/settings. Equal values alone cannot confirm a newer
transaction. A later clear resets its own track without losing the other
nineteen originals. Superseding loads retain their newest intent; allocation
failure preserves the old bank without a Timer allocation retry loop.

Changed-rate lifecycle restore now uses this same enclosing owner, replacing
the independent D-only bank. A ready bank retains its ordering before subsequent
STOP/DEL; audio cancels a still-preparing bank. The stopped prepare path retires
unread joint facts before changing generation. Existing stopped Q/C/drive
initialization is unchanged; this is not whole-project/APVTS/PCM atomic restore.

Windows: eight native Processor cases passed at 64/128 (20.46s), the extended
Delay-owner contract passed (0.69s), Standalone built, and actual WebView2
`NativeUiSmoke.cjs` passed with master volume zero and no product AudioContext.
The new Processor fixture covers stale telemetry, same-value revision fencing,
clear after adoption, paused-allocation supersession, real allocation failure,
prepare recovery and actual message-Timer service with no editor. All tracked
callback new/delete counts remain zero. Cross-platform results and commands are
recorded in [DELAY.md](DELAY.md).

Linux Release: the owner contract and eight native Processor cases passed 9/9
(8.02s), with Standalone rebuilt. Linux Debug ASan/UBSan/leak: extended owner
contract passed 1/1 (0.74s), no reported sanitizer errors/suppressions. This is
core-owner sanitizer coverage, not a sanitized JUCE Processor run. Previous
unchanged-JS/connected PCM/storage gates below remain reused evidence; no DSP
math changed. Linux WebKit interaction, listening, devices and wrappers were
not tested in this slice.

NEXT: versioned D persistence, then shared GUI/CLI commands and interaction/MIDI
acceptance. D remains disabled in product controls. No package/push or broad CPU
tuning. The core checkpoint below is retained historical evidence; its former
Processor-integration next step is now implemented.

### Coordinated core Arp/Delay/effects bank — 23 November 2026

Snapshot `c5a77cb` adds opt-in `EffectsBank::restoreDelays` to the existing Arp
bank transaction. Arp and D storage now separate readiness/validation from owner
installation. Both banks validate successfully before either can commit; Q/C,
T/F/O, the user-ordered chain and all twenty A/D settings then change at one audio
boundary. D slot/ring-budget preflight runs before publishing Arp jobs. Worker
failure, cancellation or stale epochs/revisions preserve old owners except the
cancelling action's own reset/clear. Storage reclaim remains worker-only.

One immutable Arp-bank receipt now carries both revision arrays, the callback
serial and `includesDelays`. A joint request excludes D edits/banks through unread
receipt consumption; D cancellation also cancels its enclosing joint transaction.
Legacy callers default to `restoreDelays=false`, retaining existing behavior and
session/parameter IDs. At that checkpoint no Processor state loader or product
command enabled it; the newer Processor checkpoint above supersedes that limit.

Windows affected core/reference/ownership checks: 19/19 (77.57s). Final test-only
MIDI Stop and paused-audible additions: 2/2 (26.42s); unchanged passing suites were
reused. Connected two-loop PCM retains the original D-only cases and adds A+D,
AD/DA order changes, real OOM rollback and twenty old-owner callbacks while D is
ready but the final Arp allocation waits. At 8/48 kHz and 64/128, maximum errors
are 7.45058e-9 (8k) / 1.49012e-8 (48k), against unchanged 2e-6. Input is paired
mono and the existing loop downmix/centre-pan path, not full stereo/device proof.
See [DELAY.md](DELAY.md) for commands, cross-platform results and limits.

Linux Release affected checks passed 19/19 (18.04s); existing Processor tests
passed 6/6 on Windows (19.99s) and Linux (7.07s), with both Standalone builds.
The initial sanitizer aggregate timed out at 90.11s (18 other passes), not a pass.
Admission and all ten coordinated scenarios are now separate 90s tests while
the original no-argument executable matrix remains available. All assertions,
callback counts and cases remain. Test driver `f0d5083` passes 11/11 on Windows
(23.95s), Linux Release (5.49s) and Linux Debug ASan/UBSan/leak (196.33s total,
each under 90s), with no reported sanitizer errors/suppressions. The other eighteen
sanitizer cases passed in the original run and were reused; do not describe the
timed-out run as a pass or invent a single 29/29 run. No native Processor sanitizer,
new GUI/listening/device or wrapper check was performed. See DELAY.md for scope.

Next at that checkpoint (now implemented above): have the Processor restore owner submit the joint bank and reconcile its D
shadow/binding only from the matching receipt plus current generation/epochs and
callback serial. Clear must win on its own track without discarding the other
nineteen confirmed originals. Current D-only device lifecycle restore remains
separate. Then add versioned D persistence and shared GUI/CLI/MIDI/interaction
acceptance. D remains unavailable in product controls. No package/push or CPU tuning.

### Internal Delay Processor attachment — 23 November 2026

Source snapshot `f8e79b6` attaches `DelaySettingsOwner` to `AionProcessor` under
the existing non-audio intent mutex. Its existing Timer/request service drains
receipts with no editor. Confirmed shadows survive same-rate prepare; changed-
rate D-only lifecycle restoration uses the existing bounded bank and exact result.
Failure retains explicit prior intent without repeatedly allocating on Timer;
clear overrides that intent so a later prepare cannot resurrect it. Release,
destruction and validated host restore cancel pending D through its independent
path. Invalid or queue-rejected requests do not cancel accepted work.

Validated loop admission and clear metadata now share the non-audio mutex with
stopped prepare. Accepted DEL intent survives prepare even before its audio
command runs. No lock, owner parsing, allocation or timer work was added to
`processBlock`. D still has no product RPC/GUI/CLI/session field; a native-only
friend injects test commands without opening an unfinished user-facing API.

Windows production Delay/Arp/effects-restore checks pass 6/6 (19.59s), 64/128;
final added full-FIFO/host-state cases pass Delay 2/2 (0.46s). Actual Windows
Standalone `NativeUiSmoke.cjs` passes after rebuilding `windows-dev`'s target,
with master zero and background-test browser flags. Native WSL Linux Release
Standalone builds and all six selected Processor checks pass (6.46s), with strict
JUCE assertion/leak rejection. Native tests invoke the
production Timer callback directly with no editor; they do not measure JUCE
Timer delivery punctuality. Commands, Linux results and fixture corrections are
in [DELAY.md](DELAY.md). No new Processor sanitizer run was performed; prior
core/component sanitizer evidence is retained at its recorded scope.

The newer core coordinator above supersedes the next core transaction step.
Current D lifecycle restore remains independent; Processor binding and project
restore are not established by that core gate. Then add validated
shared GUI/CLI commands, versioned state/MIDI and actual Delay interaction/audio
acceptance. No package/push, wrapper/device/listening or broad CPU work here.

### Delay message-owner adapter — 23 November 2026

Source snapshot `c4c986e` adds `DelaySettingsOwner`, the non-audio adapter for
Processor integration. It owns all D request/result traffic for one Engine,
separates accepted pending intent from confirmed settings, and commits only an
exact request/generation/transaction/epoch/revision receipt. Original presets,
double values and additive repeat settings are retained without DSP duplication.
Failed/busy/cancelled edits keep confirmed settings. Single edits and twenty-track
banks share exclusion; an unread bank followed by DEL updates the other nineteen
confirmed tracks while only the cleared track resets. Old telemetry cannot roll
back confirmed revisions/clear epochs; reprepare requires an explicit verified
known-state bind. Old ready receipts remain facts, not claims of current audio.

Windows focused Delay/reference checks pass 10/10 (18.07s), native WSL Linux
Release 10/10 (2.76s), Linux ASan/UBSan/leak 10/10 (24.64s), with no reported
sanitizer errors/suppressions. Commands/limits are in [DELAY.md](DELAY.md).
The first new fixture timed
out because it used zero-sample callbacks to dispatch ordinary commands; the
fixture now dispatches in a normal callback, then tests zero-frame receipt polling.
No product code or acceptance assertions changed to hide that failure.

The newer Processor checkpoint above supersedes this adapter-only stage.
Coordinate D with the existing Arp-owned
Q/C/A/T/F/O/chain session restore before exposing GUI/CLI or serializing enabled
D settings. Independent successful D and Arp banks do not prove atomic whole-
session restore. No DSP, UI assets, reference, parameter IDs or dependencies
changed. No new package, push, hardware/listening or broad CPU work this session.

### Atomic D-only bank transactions — 22 November 2026

Source snapshot `401ce58` adds a bounded twenty-track Delay bank through storage,
LoopBank and the Engine performance FIFO. Admission validates the complete bank,
all epochs/revisions, free slots and aggregate ring budget before publishing any
worker job. The existing worker prepares enabled targets; adoption commits all
twenty settings together at a callback boundary. Disabled targets fade old owners
to dry without allocating dummy processors. Old rings remain charged until the
worker reclaims them. Failure/cancellation leaves accepted owners unchanged except
for the cancelling action's own clear/reset behavior.

One outstanding bank includes its unread receipt and excludes individual D edits.
Request IDs may be reused, transactions may not. Heap-owned fixed queues retain
generation/epoch/revision receipts across reprepare; cancellation is independent
of FIFO room and telemetry. Tests cover partial preparation, aggregate budget,
OOM PCM rollback, paused-allocation cancellation/clear/panic/MIDI Stop, stale
request reuse and old-generation receipts. Actual two-loop Engine PCM comparisons
cover 8/48 kHz and 64/128 samples, including all-off fades and unchanged clip,
history and playback ownership. This is D-only atomicity, not a coordinated
whole-session restore with other effects, routing or PCM.

Validation results are recorded in [DELAY.md](DELAY.md). Next: Processor shadow,
result and lifetime integration, including coordination with existing effect-bank
restore, then shared GUI/CLI controls and versioned persistence. D remains
unavailable in product controls. No wrapper, package, push or broad CPU tuning.

Executed: Windows Release selected regressions 14/14 (38.33s), native WSL Linux
Release 14/14 (7.59s), Linux ASan/UBSan/leak Delay 9/9 (25.57s), and focused core
`--loop-edits` / `--midi` on both OSes. No reported sanitizer errors/suppressions.
New two-loop Engine PCM maximum error: 7.45058e-9 (limit 2e-6), 8/48 kHz and
64/128; paired mono/centre pan/zero wet-pan speed only. No product UI/CLI,
listening, device or wrapper acceptance was run. Earlier interrupted exhaustive
core checks remain incomplete. Reference/UI resources and dependencies unchanged.

### Ordered Engine D controls/results — 22 November 2026

Added `enqueueDelayEdit`/`popDelayEdit`/`cancelDelayEdit` on the shared Engine.
One atomic admission registers a never-reused transaction and an ordered
performance-FIFO command. Live controls commit at sample offsets; mode/capacity
changes prepare on the existing worker and report ready only at audio adoption.
Results independently retain request/generation/transaction/epoch/revision/frame;
normal loop acknowledgements are untouched. One outstanding per track includes
unread receipts; fixed request/result queues and pending storage are allocated
at construction. Cancellation is independent of FIFO room and snapshot delivery.

Covered future-head cancellation/reused request IDs, full FIFO atomic rejection,
twenty unread receipts with saturated telemetry, unread success across clear/
prepare, worker OOM and paused-allocation cancel/panic/DEL/synthetic MIDI Stop.
Disabled metadata can change mode/count without allocating; the existing runtime
fades to bypass and a later enable prepares the requested mode if necessary.
Actual Engine capture/playback is compared against independently scheduled Delay
at 8/48 kHz, 64/128, including live edits at +17 and continuous prepared mode
replacement. Code snapshot `359ffd4`, strengthened test snapshot `12f1453`:
Windows selected regressions 12/12 (30.95s), native WSL Linux selected regressions
12/12 (7.00s), final Linux ASan/UBSan/leak Delay 7/7 (19.90s), no reported errors
or suppressions. Incremental busy/wrong-generation checks pass Windows 1/1
(2.24s), Linux 1/1 (0.19s); unchanged passing normal suites were reused after
that test-only addition. Final core --loop-edits AND --midi pass on both OSes.
Actual Engine PCM max error is 1.49012e-8 (8k) / 7.45058e-9 (48k), both 64/128,
against an unchanged 2e-6 limit. The fixture uses paired mono/centre pan/zero
wet-pan speed; do not infer full stereo/browser parameter coverage. See
[DELAY.md](DELAY.md) for exact commands, protocol and evidence limits.

No product Standalone/GUI/CLI, physical device/listening or wrapper check was
run. No package, push, dependency upgrade or CPU profiling. Earlier manually
interrupted exhaustive core gates remain incomplete and were not rerun here.

At this earlier checkpoint, atomic D bank restore was the next internal step;
the newer bank checkpoint above supersedes that step. Processor command/result/
lifetime handling, GUI/CLI, MIDI Learn and session serialization remain pending.
Do not expose D in the UI or mark it feature-complete yet. Reuse prior component
evidence; run focused Delay/affected owner checks and core --loop-edits/--midi,
not the exhaustive core suite or broad CPU profiling on each internal step.

### Connected LoopBank D / prepared-owner transitions — 21 November 2026

D now uses LoopBank's existing worker and editable signal-chain position. Added
clear-epoch/settings-revision admission/adoption checks, success-only settings
commit, superseding-live-edit cancellation, STOP/DEL/reprepare cleanup and a
20ms pinned old/new owner crossfade. Old ring bytes stay budgeted until actual
worker reclamation. Absent D owners skip processing; removing D cannot strand a
hidden owner transition. Synced tempo updates both transition owners, fresh
adoption initializes at the latest tempo, and timeline reset uses reset-clock BPM.

New connected tests capture/finalize actual native loop PCM, use the real worker,
and compare Feedback→exact mode replacement, live controls, D→T / T→D order,
BPM changes during preparation/transition, timeline reset, stale requests and
STOP/clear at 8/48 kHz and 64/128 frames against independently scheduled DSP.
Final snapshot `bd2e34d`: Windows Release Delay 5/5 (6.10s), native WSL Linux
Release Delay 5/5 (1.18s), Linux ASan/UBSan/leak Debug 5/5 (12.71s), with no
reported sanitizer errors or suppressions. Connected PCM tolerance remains
2e-6; zero tracked audio C++ heap operations. Final Windows/Linux core
`--loop-edits` both pass capture, reverse, history, Multiply, Start and Merge
checks. No feature-complete/release claim.
Additional Windows off-chain/reinserted D PCM checks pass at both rates/blocks,
maximum error 4.47035e-8, with unchanged 2e-6 tolerance. The earlier exhaustive
Windows `core_realtime_contract` run was manually stopped after 796.17s without
an assertion failure being observed; CTest correctly records it failed/incomplete,
not passed. It had progressed through many existing source/clock/meter/MULT cases.
The Linux exhaustive core run was likewise stopped at 342.69s; both broader
selected runs have eight other passes and one manually terminated core case,
not nine passes. Full core completion remains unverified. Focused final source
checks above passed; original cases/assertions/tolerances remain intact.
Do not treat this interrupted exhaustive run as a CPU finding or waive its later
integration gate; do not routinely rerun the full suite for every internal edit.
Exact commands/results and the storage-fixture admission change are recorded in
[DELAY.md](DELAY.md). No product GUI/CLI, Standalone binary, listening, device or
wrapper check was executed, and no package/push was made.

Still INTERNAL: no Engine/message-thread command or result queue, UI/CLI, MIDI
Learn or new session serialization. Next is generation/cancellation/result
acknowledgement wiring and atomic bank restore, then shared product controls.
Preserve existing IDs; keep D unavailable in the UI until those connected gates.
Earlier standalone-component descriptions below are historical checkpoints.

### Delay exact repeats + Feedback — 21 November 2026

Owner explicitly requests both: exact 1/2/3/multiple repeats and fading Feedback.
Added native mode/count metadata (1–10), separate finite stereo taps and preserved
legacy Feedback values/presets. Exact mode has no recirculation and handles
overlapping input independently; later copies use damping and a 0.65 level taper.
Original presets restore Feedback mode. New controls are NOT yet exposed in the
app, serialized or shared through GUI/CLI commands.

Added internal `DelayStorage`: 20 tracks / 40 slots, a default 256 MiB ring budget,
worker-only preparation/destruction, atomic overflow rejection, stale-result
identity guards and preservation of active audio after OOM/cancellation. It is
not yet connected to LoopBank. Source snapshot `5707e01`: Windows focused tests
4/4 (0.88s), native WSL Linux Release 4/4 (0.88s), Linux ASan/UBSan/leak Debug
4/4 (5.65s), no reported sanitizer errors or suppressions. The sanitizer Makefile
needed an explicit CMake regeneration before its new targets were available;
the corrected build/test completed. Counts 1/2/3/10, overlapping input, 64/128 tick grouping,
feedback regression, budget boundaries, cancellation during real allocation and
zero tracked audio heap operations are exercised. See [DELAY.md](DELAY.md).

Next: integrate preparation with LoopBank/Engine generation and acknowledgement
guards, click-free mode/count-capacity replacement and atomic bank restore, then
chain + shared GUI/CLI/MIDI/state. Keep D unavailable until connected acceptance.
Broad CPU work remains deferred by owner request.

### Owner priority resolved — 21 November 2026

Resume remaining loop features; broad CPU profiling/tuning moves to hardening
after feature implementation. New components still require bounded processing,
off-audio preparation, cached coefficients and focused real-time checks. Earlier
CPU measurements remain release evidence, not a blocker to the next effect.
Next feature is **D / Delay**, following accepted Q/C/A/T/F/O. Its first internal
component slice preserves original controls/presets while implementing the approved
organic/natural direction. D is NOT yet connected to GUI/CLI/state or complete.
See [DELAY.md](DELAY.md) for observable acceptance, commands and ownership limits.
The component preallocates a four-second stereo ring, hides cleared history in
O(1), caches coefficients and crossfades two read heads during time changes.
Source snapshot `e75c210`: Windows Release 2/2 (0.48s), native WSL Linux Release
2/2 (0.65s), Linux ASan/UBSan/leak Debug 2/2 (3.78s). These are component audio
and original settings/preset checks, not connected Engine/GUI/listening acceptance.
The fixture exercises eight rates and 64/128 tick groupings, OOM preservation,
sync limits, wrapped clear, time-edit coalescing, feedback decay and zero tracked
callback heap operations. No new product control, package, push or CPU claim.
Next: bounded worker-owned per-track preparation/retirement, then editable-chain
and shared GUI/CLI/MIDI/state integration; keep D disabled until connected gates.

### Latest follow-up: measured decimator scheduling — 21 November 2026

Completed the planned intra-drive attribution using EXISTING gmon samples, not
another instrumented audio render. Offline inline-aware addr2line resolution
finds up/downsampling in 39/72 and 149/296 identified drive samples. Decimation
is the largest resolved stage; roughly 35% of drive samples remain other work,
so this is not exact per-stage timing. The new bounded offline resolver checks
format/ranges, source/executable identity and retains unknown/unresolved groups;
malformed-input/inline classification regression is registered in CTest.

Based on that evidence, candidate `8e335b8` interleaves independent wet/dry
decimator recurrences, preserving coefficients/equations/state/sample order and
class size. Its component means improve in all nine cases on both OSes. Full
callback observations are mixed: GNU/Linux means improve about 1.2–1.4%, while
Windows no-MIDI is slower despite the component gain. **Final `ea07783` retains
paired decimation only for GNU/Linux x64; Windows/MSVC and unvalidated compilers/
platforms keep the accepted schedule.** No quality/latency/oversampling, controls,
IDs, layout, routing, source scheduling, new storage/allocation/lock/API changes.

Native AND forced-portable paths match 4,818,576 samples exactly. GNU/Linux
focused gates pass 10/10 (17.13s), ASan/UBSan/leak checks 4/4 (256.95s), no
suppressions. Final selector has byte-identical native/portable GNU token streams
to the validated candidate, so unchanged Linux suites were not repeated. Final
Windows fallback: core/reference/parser 6/6 (11.06s), processor/CLI/MIDI 64/128
2/2 (56.51s), benchmark fixtures 2/2 (0.91s). Candidate JS capture/OD/SUB audits
pass both OSes (126 cases / 965,826 samples, max error 7.394757495227111e-8).
Both normal Standalone/benchmark artifacts rebuilt; snapshot benchmarking and
callback profiling restored OFF. No new UI/device/listening/DAW/Apple acceptance.

Every candidate full-matrix fixture passes state/nonzero/finite PCM and zero
tracked callback C++ heap checks, with accepted checksums. Windows timing rows
are retained explicitly as the REJECTED scheduling experiment, not a final
Windows speedup. Full-workspace high-rate deadlines remain open. No packages,
push, dependency or format change. Full scope/data/commands:
[decimator validation](benchmarks/drive-decimator-2026-11-21.md).

**Priority resolved by the owner:** resume remaining loop features in roadmap
order; retain heavy-load CPU work for hardening after feature implementation.
Do not repeat these profiles as a prerequisite to implementing D / Delay.

### Latest follow-up: planned callback attribution — 21 November 2026

Completed the preceding handoff's **profile the remaining full callback costs**
step, without another speculative DSP rewrite or advancing unaccepted effects.
Snapshot `a512d8a` adds an opt-in, non-installed Linux/GNU diagnostic with a
separate instrumented core object library. The ordinary core, processor, DSP,
UI/assets, state/IDs and formats are unchanged. Normal builds default OFF.

Normal benchmark fixtures pass Windows 2/2 (0.97s), Linux 2/2 (0.55s) at 64/128.
Two profiled production workloads (10 loops/96k/64 and 20 loops/96k/128) retain
all six PCM/state/MIDI fixture checks and accepted normal PCM checksums. Symbol
and link-map inspection confirms normal core/benchmark have no profiler hooks,
and the diagnostic resolves core symbols from its explicit instrumented objects.
Profiling includes fixture and off-audio snapshot work; it is NOT native callback
latency/deadline evidence. Profiler runtime is not certified audio-thread safe.

Drive::tick dominates sampled executable self time (56.10% / 60.04%); LoopBank's
own dispatch work and compression follow (roughly 9–12%). Shared-library time,
instrumentation overhead, coarse samples and inlining limit interpretation.
Existing disassembly already vectorizes control smoothing; do not propose it
again as an unmeasured scalar bottleneck. Optional gprof line analysis fails with
an internal line-table error, so no intra-drive line-level claim is made. This
tool limitation does not become a new product feature gate.

Full tables, commands, raw-bundle provenance, the fixed diagnostic build failure,
limits and next bounded investigation:
[callback attribution](benchmarks/callback-profile-2026-11-21.md).
No product/audio-suite rebuild/repeat, package, push or dependency upgrade.
**Next:** distinguish drive wet/dry resampling from nonlinear/tone cost before a
production optimization; preserve exact sound/ownership and roadmap scope. Do
not restart rejected cache/type-specialization experiments or add new formats.
High-rate capacity and historical intermittent editor timeout remain open; no
new release acceptance is claimed. The committed Release guard configures
successfully; optional profiling is restored OFF (`build/callback-profile-restore-default.log`).

### Latest follow-up: unused drive midpoint elimination — 21 November 2026

Snapshot `49c1001` keeps accepted T/O/F formulas, smoothing, primitive history, 4x
oversampling, state/layout/IDs and controls. O/F now skip midpoint transfer and
safe-denominator selection only when NEITHER stereo lane uses that fallback.
Both-close/mixed cases retain their original behavior. No new storage, callback
allocation, approximation or product/UI changes. A pure `none` mask helper is
provided for SSE2 and portable frames; existing frame operations are unchanged.

Acceptance: preserve all prior scalar/reference/ownership regressions and their
tolerances; add exact PCM for independent silent/changing lanes and all four mask
combinations, including 64/128. Windows core/connected/reference: 5/5 (10.42s).
Native AND forced-portable paths each compare 4,818,576 samples, max error zero.
Windows production processor/CLI/MIDI + callback fixtures pass 4/4 (33.25s).
Linux focused core/reference/processor fixtures pass 9/9 (14.08s), including the
same 4,818,576-sample zero-error native AND portable comparisons. Linux ASan/UBSan
with leak detection/halt on UB passes 4/4 (259.05s), no suppressions. Both
Standalone builds and original-JS capture/OD/SUB audits pass (126 cases, 965,826
samples, max error 7.394757495227111e-8). Controls/UI/bridge code did not change;
reuse the prior 40 real document-reload checks rather than repeat unchanged UI.

Paired component observations improve O/F means in all six cases on both OSes
(about 1.7–5.2%). Distortion arithmetic is unchanged but some measured cases are
slower (Linux up to about 1.9%); preserve that tradeoff. All 24 full production
fixtures per OS pass state/PCM/zero tracked C++ callback heap checks. Controlled
ABBA means improve about 2.5% on Linux and under 1% on Windows; deadline misses
are NOT consistently better. Twenty Windows loops at 96k/64 with QCTFO miss all
observed deadlines. This kernel improvement does not close high-rate capacity.
Do not promote release readiness or advance to another effect on these timings.

The opt-in processor snapshot benchmark now accepts
`AION_DRIVE_SNAPSHOT_REVISION` (default still fcef1a9) so the new change can be
isolated against accepted `9b874ef`, which already contains bias reuse. Header
compatibility and immutable generated-source/link-ownership guards remain.
Both app caches restored to benchmark OFF/default revision after measurements.
All measurements, negative/tail cases, provenance and exact commands:
[midpoint validation](benchmarks/drive-midpoint-2026-11-21.md).
No package/push/dependency/format change. **Next:** profile the remaining full
callback costs and choose the next bounded optimization; retain the existing
sound/ownership gates. Avoid another cache/specialization rewrite without new
evidence; their rejected results remain documented. The historical intermittent
editor/CDP timeout remains undiagnosed, not reproduced/fixed by this DSP work.

### Latest follow-up: document-owned drive UI acceptance — 20 November 2026

Scope: repair a confirmed test-readiness gap while investigating the historical
intermittent CDP timeout. No product, DSP, bridge, UI assets, IDs or preset/state
changes. Baseline `307a534` actual Drive UI passes 64/128 this session without
reproducing the timeout (`build/drive-readiness-baseline.log`). That pass alone
does not diagnose the prior failure.

The previous test waited 100ms after Page.reload and checked enabled controls;
it could accept controls from the old document before navigation completed.
The test now waits for a different default-world context in the same main frame
and native origin, and pins evaluations to its system-unique CDP identity.
Numeric context IDs alone can be recycled across processes; see the official
[Runtime protocol](https://raw.githubusercontent.com/ChromeDevTools/devtools-protocol/master/pdl/js_protocol.pdl).
No automatic retry of failed evaluations, longer deadlines, suppressed runtime
exceptions, PCM tolerance changes or changes to original pointer/MIDI assertions.
This closes the false-readiness gap; it is NOT proof of the earlier timeout's
cause or a product/browser timeout fix. Existing failure logs remain preserved.

Acceptance: an old ready page cannot qualify for reload; isolated worlds,
subframes and blank-origin contexts cannot qualify; reused numeric IDs still
require a new unique identity; late old-world destruction cannot retire the new
context. The deterministic `drive_document_readiness_contract` covers these
ownership cases, including missing identity and ordinary context destruction.
Snapshot `3eadd8f`: Windows CTest passes 1/1 (0.14s); Linux CTest passes 1/1
(0.41s) after clean committed-snapshot sync into native WSL storage.

Actual WebView2 acceptance passes with **20 document replacements per block size,
40 total**, on the existing production editor/processor host, with no retries.
Every additional reload checks nonzero playback, unchanged clip identity/length,
playing state, all drive settings and MIDI bindings. All existing preset, slider,
send-failure, 14 CC learning, reload/bypass/DEL and runtime-exception assertions
also pass. Evidence: `build/drive-document-stress.log`.

Commands (existing configured caches; pinned Windows CMake/CTest tools):

```powershell
node tests/DriveDocumentTests.cjs
node --check tests/DriveUi.cjs
cmake -S . -B build/windows-core
ctest --test-dir build/windows-core -C Release -R '^drive_document_readiness_contract$' --output-on-failure --no-tests=error
./tools/TestDriveUi.ps1 -ReloadCycles 20
```

Linux (after `tools/SetupWslWorkspace.sh`, in `/home/jorge/projects/Aion-Looper`):

```sh
cmake -S . -B build/linux
ctest --test-dir build/linux -R '^drive_document_readiness_contract$' --output-on-failure --no-tests=error
```

No audio rebuild or audio-suite repeat is required for this test-only change;
reuse the exact-DSP, ownership, sanitizer and callback evidence below. Linux
WebKit/device/long-session coverage remains separate. No package/push/dependency
change. **Next:** continue the open high-rate callback-capacity investigation for
the current Q/C/T/F/O slice. Retain lifecycle diagnostics if the historical timeout
recurs; do not claim it fixed or restart repeated full-suite retries without new
evidence. Do not promote release capacity or start another effect prematurely.

### Latest follow-up: identical drive bias correction — 20 November 2026

Current candidate keeps the accepted shared T/O/F rendering path and reuses the
first bias correction for later subsamples ONLY when both input arguments are
bit-identical. Bit comparison preserves +0/-0 distinctions. No new processor
state, approximation, oversampling reduction, controls, IDs, presets, routing,
capacity or presentation changes. Drive.h is identical to fcef1a9.

The persistent control/tone cache (ca9ea81) and full-path type specialization
(0faa24b) are NOT the final implementation: controlled measurements found
tradeoffs or inconclusive application gains. Their original logs remain under
build/drive-settled-* and build/drive-kind-*. Keep negative evidence visible.

Extended unchanged-scalar comparisons retain every original case and add long
settling/retarget/reset/rate-change and signed-zero bias boundaries. Windows
optimized AND forced-portable paths each compare 4,671,120 samples at maximum
error zero. The original 1e-6 bound is unchanged; added long/boundary cases require
exact Float32 equality. Focused Windows core/reference/chain gates: 5/5, 9.97s.
Linux passes the same exact comparison and its nine focused core/reference/
processor/fixture tests (16.22s). Windows processor/fixtures pass 4/4 (31.32s).
Linux ASan+UBSan passes 4/4 (280.79s), leak detection enabled, no suppressions.
Both Standalone builds and both capture/OD/SUB audio audits pass (126 cases,
965,826 samples, maximum error 7.394757495227111e-8). Earlier drive-kind-* results
belong to the rejected specialization candidate, not this final code (9bbc11e).

Actual WebView2 tests pass at 64 and at 128 on diagnostic retry: presets, sliders,
failed-send recovery, 14 CC controls, reload, bypass, DEL and nonzero PCM. The
first 128 run timed out evaluating control readiness during startup or reload;
the cause remains unresolved. Preserve build/drive-bias-ui.log. The retry changes
only diagnostics, retaining deadlines/assertions; it does NOT prove that the
intermittent browser/CDP timeout is fixed. Future failures now distinguish initial
bootstrap/reload and retain bounded browser lifecycle evidence. Linux WebKit,
physical devices, listening and long-session interaction were not executed.

The paired component helper tools/BenchmarkDriveSnapshot.cjs uses an accepted
SSE2 snapshot and current DSP in one process, alternates execution order each
callback, checks PCM equality and verifies equal storage footprint. It uses only
existing Node/CMake/compiler tools. Generated sources are immutable/guarded;
unique result/metadata files retain every run and source hashes. Nine cases:
T/O/F at 96k/64 with 20 instances, 256 warm / 2048 measured callbacks; automation
0=none, 1=tone/mix, 2=bias (T/F) or drive amount (O). It is not full-app latency.

An additional optional AION_BUILD_DRIVE_SNAPSHOT_BENCHMARK builds a production
processor comparison executable using accepted Drive.cpp with the same current
processor/common code. Header compatibility is checked; the link map verifies
Drive::tick comes from the snapshot object. The current header matches exactly
and needs no signature adapter. The optional tool also supports the previous
private-template signature for reproducing that rejected experiment, without
changing its runtime-kind DSP equations. No snapshot code is installed or built
by default. Keep the option OFF for ordinary work and restore OFF after use.

Both processor benchmark binaries support --case RATE BLOCK TRACKS (48/96k,
64/128, 10/20). Default 24-case and quick fixture behavior remains. Same-framework
baseline/current/current/baseline runs at ten loops/96k/64 report zero callback
C++ new/delete and matching PCM checksums; mean savings are small and deadlines
still miss. Preserve all run rows, p99/maxima and overrun counts. This is not an
application capacity, physical device, editor scheduling or round-trip pass.

All 40 Engine and 24 production-processor fixtures per OS pass state/nonzero/
finite PCM checks, match accepted baseline fixture checksums and report zero
tracked callback C++ new/delete. Same-framework ABBA observations at ten loops,
96k/64 show small average savings, but deadline misses and worse tail cases remain.
Paired moving-bias T/F cases are slightly slower; retain these tradeoffs, not a
universal speedup claim. Neither the full matrices nor these synthetic callback
measurements establish hardware capacity or round-trip latency.

Exact commands, provenance, every matrix/paired/ABBA row and limitations:
[drive bias validation](benchmarks/drive-bias-2026-11-20.md). Optional snapshot
benchmark restored OFF in both app caches after use. No package, dependency
change, wrapper build or push.

**Next:** investigate the intermittent editor/CDP readiness timeout using the
added bounded lifecycle diagnostics; high-rate ten/twenty-loop deadline capacity
remains open. Do not promote release readiness or start another effect on the
strength of a successful retry or the modest average CPU savings.

### Latest follow-up: production callback load diagnostic — 20 November 2026

Snapshot `f0578de` adds the optional, development-only
`aion_processor_effects_benchmark`. It links the production `AionProcessor` and
measures `processBlock`, including APVTS reads, JUCE MidiBuffer validation/routing,
native callback stamps and Engine rendering. No production code, DSP, parameters,
state, GUI, reference or preserved assets changed. This is a test executable,
not a second product headless engine or a distributable target.

Acceptance: synthesize real stereo input through production RECORD/FINISH/PLAY
requests; verify committed 1024-frame clips, stable owners, playback/mixer/FX/chain
settings, nonzero finite PCM, actual MIDI event counts, no MIDI transport actions
or overflow, real-time processing mode, and zero watched callback C++ new/delete
(aligned forms included). Setup uses offline finalization **before** measurement;
timed callbacks use `setNonRealtime(false)`. No timing threshold is asserted.

Executed:

| Gate | Windows | Linux |
| --- | --- | --- |
| Optional benchmark build | Pass | Pass |
| Quick functional fixture, 64/128 | 2/2, 0.78s | 2/2, 0.50s |
| Full warmed matrix | 24/24 fixtures pass | 24/24 fixtures pass |

Each matrix has 10/20 playing loops, 48/96k, 64/128, and three profiles: bypassed
FX, QCTFO enabled, and QCTFO plus 32 unassigned CC events per callback. MIDI is
enabled, events span channels/sample offsets, and delivered counts are checked;
these are not mapped continuous edits or physical hardware. Warm 256 / measured
1024 callbacks per case. Every case reports zero watched callback C++ heap calls.
Full results, including slower cases/outliers:
`docs/benchmarks/processor-effects-2026-11-20-{windows,linux}.csv`.

Ten loops, 96k/64; nominal callback budget 666.667us:

| Platform / MIDI events per callback | Mean us | p99 us | Overruns /1024 |
| --- | --- | --- | --- |
| Windows / 0 | 599.697 | 726.6 | 93 |
| Windows / 32 | 602.823 | 718 | 90 |
| Linux / 0 | 526.499 | 684.726 | 19 |
| Linux / 32 | 531.217 | 679.592 | 12 |

The observed extra MIDI cost is small in these cases; absent-MIDI processing
already overruns. This does not establish causal/constant MIDI overhead, mapped
MIDI performance or a universal deadline guarantee. Twenty-loop high-rate cases
also remain overloaded. Continue profiling rendering cost before another FX.

Scope limits: unpaced synthetic playback without a device, editor/WebView,
automatic message dispatch/timers, Arp or instruments. Snapshot servicing every
16 callbacks runs outside the measured interval on the same caller thread and
is reported separately, including warm-up service. It is not concurrent message
thread/GUI load. Older Engine-only and new processor fixtures have different
setup/capture/control timelines: do not subtract their timings or compare their
PCM checksums to claim exact wrapper overhead. No driver/xrun, listening,
round-trip latency, actual GUI scheduling or release acceptance is established.
Previous DSP/browser/sanitizer evidence remains unchanged; those suites were not
rerun for this test-tool-only addition. No package or push.

Reproduce after configuring changed CMake targets:

```powershell
cmake --build build/windows --config Release --target aion_processor_effects_benchmark --parallel 2
ctest --test-dir build/windows -C Release -R '^native_effects_benchmark_fixture_(64|128)$' --output-on-failure --no-tests=error
./build/windows/Release/aion_processor_effects_benchmark.exe > build/processor-effects-benchmark-windows.jsonl
```

```sh
cmake --build build/linux --target aion_processor_effects_benchmark --parallel 2
ctest --test-dir build/linux -R '^native_effects_benchmark_fixture_(64|128)$' --output-on-failure --no-tests=error
./build/linux/aion_processor_effects_benchmark > build/processor-effects-benchmark-linux.jsonl
```

Run timing serially, after other builds/tests and owned UI hosts finish. Logs:
`build/processor-effects-benchmark-{build,fixture-tests}.log` per checkout, plus
the platform JSONL above. The quick invocations are `--quick 64` and `--quick 128`;
they validate the fixture, not CPU capacity. Keep target opt-in and uninstalled.

### Latest follow-up: compressor detector CPU cost — 20 November 2026

Code snapshot `76c8c68` evaluates the detector's release logarithm/power only
when the existing update actually uses that coefficient. The accepted Float32
update, rounding, envelope/automation clocks, lookahead, metering, presets,
IDs/state, persistent/live controls and editable chain remain unchanged. No new
allocation, lock, dependency or presentation change. GUI and CLI retain one DSP.

`tests/CompressorScalarReference.*` freezes the complete accepted `5908944`
compressor, retaining its BSD notices; only namespace/includes/provenance differ.
New regression: **6,920,848 channel samples and 28,160 gain-reduction readings
exactly equal**, all 11 presets, eight rates, variable blocks including 64/128,
rapid/repeated parameter edits, silence/attack/release, resets, missing channels
and malformed inputs. Existing allocation, numeric, in-place, subdivision,
connected PCM, live OD/SUB/MULT/STOP and ownership cases remain intact.

| Executed gate | Result |
| --- | --- |
| Windows compressor/scalar/connected, original reference, chain | 5/5, 10.06s |
| Windows production compressor CLI/MIDI/state/lifecycle | 2/2 at 64/128, 15.42s |
| Linux corresponding core/reference/processor set | 7/7, 8.73s |
| Linux ASan/UBSan/leaks, compressor/scalar/connected/chain | 4/4, 69.25s; no suppressions |
| Actual WebView2 native compressor UI | 64/128 pass, controls/MIDI/reload/bypass/DEL/nonzero PCM; no runtime exceptions |
| Actual original WebAudio compressor graph/presets/automation | 60 cases, 1,966,080 frames, peak 4.806921385380747e-7; unchanged 1e-6 peak / 1e-5 relative RMS limits |
| Windows and Linux connected capture/OD/SUB audit | Each 126 cases / 965,826 frames, peak 7.394757495227111e-8; unchanged 1e-6 limit |

Both Standalone builds succeeded. No preserved `resources/ui` or reference files
changed. Listening, physical drivers, Linux WebKit and actual device/UI-heavy
application load remain unverified; synthetic/native UI gates do not establish
them. No wrappers, package, dependency upgrade or push this session.

Before/after timing uses `5908944` / `76c8c68`, warmed identical benchmark
configurations with no concurrent builds/tests. Raw logs per checkout:
`build/compressor-hotpath-*`. Complete 40-case Engine data, including slower cases,
outliers and zero watched callback C++ new/delete (aligned included), is retained
in `docs/benchmarks/compressor-hotpath-2026-11-20-{windows,linux}.csv`;
`...-kernels.csv` retains all eight initial kernel comparisons. All reported
before/after PCM checksums match; the exact-sample oracle is the sound proof.
Windows 20-compressor 48k/64 steady mean **240.91 → 211.255us**. Ten QCTFO loops,
96k/64 steady mean **618.514 → 571.745us**, overruns **166 → 44/1024**, but p99
**758.6 → 776.3us**. Automated mean **624.206 → 569.559us**, overruns **212 → 42**.
Initial Linux timing was mixed, prompting a fresh paired comparison below.
These are processing costs, not device/round-trip latency or a deadline guarantee.

Fresh Linux comparison uses clean detached `5908944` at
`build/compressor-baseline-5908944`, with independent Release/plugin-OFF cache
`build/compressor-baseline-build`. Baseline then optimized binaries run serially,
same compiler/flags, without concurrent builds/tests. Preserve this worktree;
never reset/overwrite it. All 40 fixture checksums match and both binaries report
zero watched callback C++ new/delete. Retain full data in
`...-linux-fresh-workspace.csv` and `...-linux-fresh-kernel.csv`, including outliers.

| Fresh Linux workload | Mean us before → after | p99 us before → after | Overruns /1024 before → after |
| --- | --- | --- | --- |
| Ten QCTFO loops, 96k/64, steady | 538.752 → 546.516 | 698.426 → 688.369 | 25 → 26 |
| Same, automated | 551.62 → 539.578 | 716.046 → 677.979 | 57 → 12 |

Fresh 20-compressor kernel means: 48k/64 steady **173.176 → 156.493us**,
automated **186.662 → 171.585us**; 128 steady **326.382 → 317.558us**, automated
**353.769 → 345.114us**. The baseline 64 steady kernel has one 13.78ms outlier
(measured over 2048 callbacks), retained in CSV. Full-chain gains are not universal;
the steady mean and overrun count above are slightly worse. These single-run
comparisons do not establish causal improvements for every workload or eliminate
OS/VM scheduling/frequency variation. Do not repeat runs to select only good data.

To reproduce the paired baseline, verify the existing worktree's commit and clean
state first; use `git worktree add --detach build/compressor-baseline-5908944 5908944`
only if absent. Then, in the native Linux mirror:

```sh
cmake -G Ninja -S build/compressor-baseline-5908944 -B build/compressor-baseline-build -DAION_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/compressor-baseline-build --target aion_compressor_benchmark aion_effects_engine_benchmark --parallel 2
./build/compressor-baseline-build/aion_compressor_benchmark > build/compressor-hotpath-fresh-before-kernel-linux.jsonl
./build/linux/aion_compressor_benchmark > build/compressor-hotpath-fresh-after-kernel-linux.jsonl
./build/compressor-baseline-build/aion_effects_engine_benchmark --drives > build/compressor-hotpath-fresh-before-workspace-linux.jsonl
./build/linux/aion_effects_engine_benchmark --drives > build/compressor-hotpath-fresh-after-workspace-linux.jsonl
```

Reproduce affected Windows checks with the existing Release caches:

```powershell
cmake --build build/windows-core --config Release --target aion_compressor_tests aion_compressor_scalar_tests aion_compressor_connected_tests aion_compressor_probe aion_compressor_benchmark aion_effects_engine_benchmark aion_signal_chain_tests --parallel 2
ctest --test-dir build/windows-core -C Release -R '^(compressor_.*contract|original_compressor_contract|signal_chain_connected_contract)$' --output-on-failure --no-tests=error
cmake --build build/windows --config Release --target AionLooper_Standalone aion_compressor_processor_tests aion_mouse_host --parallel 2
ctest --test-dir build/windows -C Release -R '^native_compressor_processor_(64|128)$' --output-on-failure --no-tests=error
./tools/TestCompressorWebAudio.ps1
./tools/TestCompressorUi.ps1
./build/windows-core/Release/aion_compressor_benchmark.exe
./build/windows-core/Release/aion_effects_engine_benchmark.exe --drives
```

Linux uses the independent committed mirror, `build/linux`, corresponding
targets/expressions without `-C Release`, and the existing sanitizer cache
explicitly reconfigured after adding the scalar target. The capture audit uses
`node tests/AudioParityAudit.cjs BUILD/aion_reference_probe` (add `.exe` on Windows).
Keep ASan leak detection and UBSan halt-on-error enabled. No tolerance changed.

Full high-rate capacity remains **open**, including twenty-track overload and
isolated deadline misses. Continue profiling actual Standalone load and remaining
full-chain costs before another effect; do not present the average savings as
complete 96k/64 acceptance.

### Latest follow-up: stereo EQ CPU cost — 17 November 2026

Local code snapshot `4c9a3a2` pairs independent stereo EQ recurrences using the
existing fixed SSE2/portable helper, now shared in `StereoFrame.h`. No musical,
parameter/state, smoothing, routing or interface changes. The frozen `80433e4`
scalar EQ/automation oracle matches **1,271,264 samples exactly** on both optimized
and forced-portable paths, Windows and Linux. Preserved resources remain intact.

Windows focused core/reference gates: **11/11** plus **2/2** drive/chain ownership;
production EQ/drive GUI/CLI/MIDI/state **4/4** at 64/128. Linux corresponding set
**17/17**. Linux ASan/UBSan/leaks **6/6**. Both Standalone builds succeeded. Actual
WebView2 EQ UI passes 64/128; original worklet/smoothing **48 cases** passes at
peak **8.570541420310773e-8** (unchanged 1e-6). Both connected capture/OD/SUB audits
pass **126 cases / 965,826 frames**, peak **7.394757495227111e-8**. A first UI128
status-clear assertion failed during concurrent work; the unchanged isolated
retry passed. Keep that timing-sensitive observation and failed log visible.

Twenty all-band EQ kernels at 48k/64 now take about **91us vs 197us** on Windows;
fresh sequential Linux **93us vs 208us**. Full QCTFO improvements are smaller:
fresh Linux ten loops/96k/64 steady mean **609 → 578us**, overruns **136 → 85/1024**,
but p99 worsened. Full high-rate capacity remains **open**; do not infer a deadline
pass from average savings or benchmark exit zero. All 40 cases per OS preserve
reported PCM checksums and zero watched callback C++ new/delete. Full data,
commands, limitations and fresh baseline worktree details:
[EQ_PERFORMANCE.md](EQ_PERFORMANCE.md). Logs: `build/eq-stereo-*` per checkout.

Next: continue profiling the remaining full-chain CPU cost and actual Standalone
load before another effect. One candidate to investigate is compressor detector
release math currently evaluated even when the release coefficient is unused;
retain the accepted envelope/float rounding and prove equivalence before changing
it. Listening, physical drivers, Linux WebKit and full instrument/release checks
remain separate. No package, dependency upgrade or wrapper work this session.

### Active follow-up: loaded T/O/F workspace performance — 17 November 2026

The existing manual `aion_effects_engine_benchmark` now accepts `--drives`:
40 warmed cases, 10/20 actual playing stereo loops, 48/96 kHz, 64/128, bypass /
TFO / QCTFO, steady and bounded live edits. It verifies nonzero finite PCM,
accepted settings, clip identities/lengths and watched callback C++ new/delete
(including aligned forms). The callback is measured separately from setup,
command production and off-audio telemetry consumption. No device, WebView,
Arp, drone/sampler or round-trip latency claim. Default no-argument Q/C benchmark
coverage remains intact.

Windows scalar baseline shows sustained 96 kHz overload with ten T/O/F loops
at 64 samples: mean 727.12us versus 666.67us budget, 1024/1024 observed overruns.
This takes priority over another effect port. Initial baseline logs are
`build/drive-workspace-baseline.jsonl`; the final Linux comparison uses the
independently serialized snapshot run documented below.

The optimization pairs independent stereo lanes (baseline SSE2 on x64, portable
C++ fallback elsewhere), reuses exact previous antiderivatives and shared bias
terms, and stores only the live five/eight allpass state entries plus necessary
downsample delays. No oversampling reduction, changed transfer curve, smoothing,
dry/wet alignment, parameter/state ID or dependency change. Test-only
`DriveScalarReference.*` retains accepted processor equations from `4e2a9d4`;
do not edit that oracle to conceal a regression.

Final product snapshot `d90ec37` also skips prepared, stateless bypass dispatch.
It retains first preparation after clear/cancel and pending fades; it does not
skip muted tracks or active filters. Tests add cold post-clear starts, sleeping
control edits/wake-up, reset during a fade, and all six T/F/O orders. Optimized
AND forced-portable paths each compare **1,634,448** scalar/stereo samples across
eight rates, maximum finite-sample error **zero**. Existing numerical/spectral/
64/128/live-state limits remain; no tolerance was relaxed. Intermediate slower
layout experiments remain in ignored logs and are not promoted as gains.

Executed final gates:

| Gate | Windows | Linux |
| --- | --- | --- |
| Core/Drive/portable/Arp/chain + original settings/chain | 7/7, 11.84s | Included in 20/20 below |
| Unchanged-JS state/DSP/sync/MULT/history/reverse/NORM | 7/7, 132.22s | Included in 20/20 below |
| Linked processor/CLI/MIDI/state/chain/restore, 64/128 | 6/6, 43.11s | Combined core/reference/processor set: 20/20, 253.76s |
| Actual WebView2 pointer/key/MIDI/reload/bypass/DEL/PCM | 64/128 pass | Linux WebKit not executed |
| Unchanged-JS capture/OD/SUB audit | 126 cases / 965,826 frames, peak 7.394757495e-8 | Same result |
| ASan/UBSan/leaks: optimized/portable/connected/Arp/chain | Not executed | 5/5, 149.45s; no suppressions |

Logs: `drive-dispatch-{build,tests,reference-build,reference-tests,audio-audit,app-build,app-tests,ui}.log`
as applicable per checkout, and Linux `drive-stereo-sanitized-{configure,rebuild,tests}.log`.
An initial sanitizer build failed resolving the new portable target from an old
Makefile. Explicit CMake reconfiguration fixed the build prerequisite; the
original failure log remains `drive-stereo-sanitized-build.log`.

All 40 timing cases per OS validate playing clips, reported settings/routing,
finite nonzero PCM and zero watched callback C++ new/delete (aligned included).
Before/after reported checksums match in every case. Serial Linux timing uses
the clean detached baseline `build/drive-baseline-3572c72` with its own cache
`build/drive-baseline-build`; the primary mirror remains clean on main. Do not
reset that snapshot or copy toolchain caches between OSes. Earlier concurrent
Linux timing is retained but excluded from the committed comparison.

At ten T/F/O loops, 96 kHz/64, Engine mean time falls Windows **727.12 → 474.93us**
and Linux **722.06 → 394.51us**; observed overruns fall **1024 → 0** and **837 → 0**.
This is processing headroom, not reduced device/round-trip latency. The full
Q/C/T/F/O chain at 96 kHz/64 still overruns (Windows 222/1024, Linux 36/1024);
twenty-loop high-rate cases and some isolated lower-rate outliers also remain.
Benchmark exit zero is fixture acceptance, not a universal timing pass.

See [DRIVE_PERFORMANCE.md](DRIVE_PERFORMANCE.md) for environment, full CSV data,
commands and limits. Next: profile the remaining high-rate full-chain load and
measure actual Standalone/MIDI/UI/device workloads before advancing beyond that
capacity issue. Listening, physical drivers and Linux GUI remain unverified.
Both Standalone apps rebuild; no dependency/format/state-ID changes, package or
push. Preserved UI/reference files remain unchanged.

### T / Distortion, O / Overdrive, F / Fuzz — 17 November 2026

The owner requested all three effects. Native `Drive` implements separate
double-precision ADAA models with fixed-storage 4x polyphase oversampling, DC
blocking, smoothed controls/bypass and Nyquist-safe tone. Exact original defaults,
bounds and all 17 named presets are retained; sound intentionally follows the
owner's quality-first policy. See [DRIVE.md](DRIVE.md) for models/quality scope.

Windows: DSP/reference checks pass 2/2; connected Engine, existing chain and
unchanged original-chain checks pass 3/3 with <=1e-6 ordered PCM at 64/128.
New production processor/state/CLI tests pass 2/2 (30.57s), including all fourteen
actual MidiBuffer learn targets. Actual WebView2/CDP pointer/key tests pass at
64/128: original checkboxes/presets/sliders, send failure recovery, all fourteen
focused learned sliders, reload, bypass, DEL, nonzero PCM and no runtime errors.
Logs: `drive-{tests,connected-tests,processor-tests,ui-final}.log`.

T/O/F share one GUI/CLI/native owner, per-field audio merging, editable chain
order, persistent processors and versioned `LoopDrives` state. MIDI schema v7
appends targets 133..146; v1..v6 remain loadable. The existing schema test advances
its current-version assertion and ADDS v6 migration coverage; old cases remain.
Preserved UI/reference assets and existing APVTS/action IDs are unchanged.

The first UI failures were fixture readiness, incorrectly ordered synthetic
packet size/offset fields, and post-reload layout movement. The fixture now waits
for catalog/bootstrap, uses the existing host packet format and requires actual
hit-tested pointer targets after scrolling. No product bridge timeout, audio
assertion, hold deadline or DSP tolerance was loosened. The new panic fixture
also verifies the unchanged 240-sample/5ms source fade before asserting hard zero;
it no longer mistakes that original fade for source resurrection.

Linux Standalone integration snapshot `a83c8d8` builds; all eleven focused
DSP/reference/connected/processor/CLI/state checks pass (14.96s). Native core
ASan/UBSan/LSan passes 3/3 (45.41s), including ordered source/ownership cases,
with leak detection and halt-on-UB enabled, no suppressions. Exact commands:

```sh
cmake --preset linux -DAION_TEST_NATIVE_PROCESSOR=ON
cmake --build build/linux --target AionLooper_Standalone aion_drive_processor_tests aion_effects_restore_processor_tests aion_signal_chain_processor_tests aion_drive_tests aion_drive_connected_tests aion_signal_chain_tests --parallel 2
ctest --test-dir build/linux -R '^(drive_dsp_contract|drive_connected_contract|original_drive_settings_contract|signal_chain_connected_contract|original_signal_chain_contract|native_(drive|effects_restore|signal_chain)_processor_(64|128))$' --output-on-failure
cmake -S . -B build/drive-sanitized -DAION_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/drive-sanitized --target aion_drive_tests aion_drive_connected_tests aion_signal_chain_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/drive-sanitized -R '^(drive_dsp_contract|drive_connected_contract|signal_chain_connected_contract)$' --output-on-failure
```

Windows full core and affected/reference gates pass 6/6 (1081.64s), native
chain/effects-restore 4/4 (14.48s), frontend command regression 1/1 (5.65s).
Logs are `drive-regressions.log`, `drive-native-regressions.log`,
`drive-frontend-tests.log`; Linux logs `drive-linux-{configure,build,tests}.log`
and `drive-sanitized-{configure,build,tests}.log`.

The broad Windows MIDI executable twice reached CTest's 180s limit after entering
the separate cleanup matrix (previous pre-T/O/F pass: 172.28s). Preserve both
failure logs `drive-midi-processor-{tests,retry}.log`. CTest now registers the
unchanged MIDI/device, source/grid ownership and actual message-loop cleanup
matrices separately, using explicit modes of the same executable. A two-way
split still exceeded 180s before completing the real timer stage; phase logs
identify ~150s of synthetic source/history/grid rendering ahead of that stage.
All old cases remain, with default direct invocation still running the complete
matrix. Every allocation assertion, per-suite 180s limit and cleanup's internal
60s CLI / 30s timer watchdogs remains. See DEVELOPMENT_WORKFLOW.md for all three
CTest names; no formerly combined coverage may be silently omitted.

The new drive telemetry also reuses unchanged immutable message-owner objects
instead of rebuilding sixty nested effect trees on every poll. Regression checks
retain old replies across control changes and DEL, and validate newly published
values for every learned CC. No callback cache or audio-thread allocation/lock.
Actual cached-telemetry WebView2 UI checks pass again at 64/128
(`drive-cache-ui.log`), and both production Drive/CLI/MIDI/state tests pass.

Expanded connected acceptance adds live presets/parameter smoothing, bypass,
chain reorder during OD/SUB/MULT/scheduled STOP, then capture finish, Undo/Redo
and RTRG without resetting the independent persistent DSP oracle. Windows passes
at 64/128, maximum error 5.96046e-8 (`drive-live-final-tests.log`). The first MULT
fixture incorrectly selected doubling (value 0); it now selects capture (value 1),
matching the retained compressor fixture. No product/tolerance change.
Expanded Linux connected/processor/CLI/MIDI/state checks pass 5/5 (159.27s,
`drive-final-tests.log`), including the earlier combined cleanup matrix. Expanded
live-state ASan/UBSan/LSan passes 1/1 (13.57s,
`drive-live-sanitized-tests.log`), without suppressions. Both Standalone apps build;
the Windows binary is `build/windows/AionLooper_artefacts/Release/Standalone/Aion-Looper.exe`.

Final three-way Windows gates pass 3/3 (207.04s): MIDI 4.40s, source ownership
154.42s, real message-loop CLI/timers 48.18s (`drive-final-midi-tests.log`).
All original assertions remain, including actual paired nonzero PCM, callback
allocation tracking and real timer deadlines. Final Linux registration also
passes 3/3 (149.16s): MIDI 5.60s, source 127.52s, actual CLI/timers 16.03s
(`drive-final-suites-tests.log`); no watchdog/tolerance extension. Tested code
snapshot: `4e2a9d4` (subsequent edits are documentation only).
The new kernel-only local CPU measurement is recorded in DRIVE.md; do not infer
full 10/20-track workspace performance or physical latency.

Next: audition T/O/F in the working Standalone, check full-workspace CPU and
physical device workflows. Keep release/listening acceptance separate from this
software preview; do not advance to another FX to bypass a reproduced defect.
All three requested effects now have connected development GUI/CLI/MIDI/state
coverage; no known blocking failure remains in the executed gates. No wrapper,
physical-device or Linux WebKit UI test, package or push was performed. Preserved
`resources/ui` and reference paths remain unchanged. Reproduction:

```powershell
cmake --build build/windows --config Release --target AionLooper_Standalone aion_mouse_host aion_drive_processor_tests aion_midi_processor_tests --parallel 2
ctest --test-dir build/windows -C Release -R '^native_(drive_processor_(64|128)|(midi|meter_source|meter_cleanup)_processor)$' --output-on-failure
./tools/TestDriveUi.ps1
```

```sh
cmake --build build/linux --target AionLooper_Standalone aion_drive_processor_tests aion_midi_processor_tests aion_drive_connected_tests --parallel 2
ctest --test-dir build/linux -R '^(drive_connected_contract|native_(drive_processor_(64|128)|(midi|meter_source|meter_cleanup)_processor))$' --output-on-failure
```
Listening, physical devices, Linux WebKit interaction and full-workspace CPU
acceptance remain pending; no release-ready claim, package or push.

### Optional Faust backend — accepted foundation, 16–17 November 2026

**Current checkpoint — 17 November:** the supported interpreter safety slice
passes Windows and Linux focused gates, including the fully instrumented Faust
dependency. Repairs cover defined bitcasts/iterator initialization, typed waveform
preparation, safe instance destruction, typed C UI callbacks and rejected-source
lexer/visitor cleanup. Cached AND fresh worker compilation preserve exact running
stereo PCM; the original cached case and numerical-input cases remain covered.

| Executed gate | Result | Log (under the relevant checkout's build/) |
| --- | --- | --- |
| Windows Release instruction/backend | 2/2, 1.22s | `faust-cleanup-host-tests.log` |
| Windows expanded worker/backend | 1/1, 3.20s | `faust-worker-host-tests.log` |
| Linux Release instruction/backend | 2/2, 0.23s | `faust-cleanup-host-tests.log` |
| Linux expanded worker/backend | 1/1, 0.20s | `faust-worker-host-tests.log` |
| Linux dependency-wide ASan/UBSan/LSan | 2/2, 9.91s | `faust-sanitized-cleanup-host-tests.log` |
| Linux expanded worker/backend, same sanitizers | 1/1, 9.59s | `faust-sanitized-worker-host-tests.log` |

Leak detection and halt-on-UB remain enabled, with no suppressions. Processing
retains zero watched new/delete and Linux wrapped direct heap calls for its
documented scope. Preserve the two earlier sanitizer failure logs below. Final
tested code snapshot is `5ca979a`; both checkouts are clean at that snapshot.
Only affected internal targets were built for that backend checkpoint; no
WebView/device/DAW test, push or package was performed then. T was still unported
at that point; the T/O/F integration above supersedes that product status. Faust
arbitrary-patch admission, full program safety and audition remain pending.

Earlier development evidence for this slice follows.

**Follow-up slice: defined VM instruction execution.** Acceptance is exact
single-precision bit-representation round trips (signed zero, subnormals,
infinity/NaN payloads), original recursive FTZ2 normal/subnormal semantics at
64/128, both switch/computed-goto implementations, retained backend/heap gates,
and an instrumented dependency run before promoting a product effect.
`PatchVmInstructions.cmake` changes the two scalar VM dispatch implementations:
copy representations with memcpy and value-initialize iterator stacks. The VM
has a 32-bit integer stack; unsupported non-single bitcasts now fail explicitly
instead of reading eight bytes from a four-byte object. The public wrapper still
forces single precision. The dependency marker requires rebuilding this patch.

`FaustInstructionTests.cpp` deliberately replaces only compiler-generated scalar
cast opcodes in a structurally valid pinned FBC fixture, then checks exact bits.
It separately compiles the real recursive FTZ2 path (not a mock) and retains a
non-FTZ subnormal-tail control. No public bytecode loader or new musical model is
added. Windows Release **2/2 passes in 1.11s** (`faust-instructions-*` logs).
Linux and dependency-wide sanitizer checks are next. Product T remains pending.

Linux optimized instruction/backend gates subsequently pass **2/2 in 0.19s**.
The first dependency-wide ASan/UBSan run **fails both tests**: the upstream
`interpreter_dsp::operator delete` reads its ended object's factory field, and the
UI convenience API treats the collector as an unrelated `UIFloat`. Keep
`build/faust-sanitized-host-tests.log` as the initial failure evidence. Repairs:
capture the allocator before destruction, explicitly destroy then free the
instance, and use the official C UI callback ABI through a separate translation
unit (the upstream C/C++ headers conflict). Factory lifetime/mutex ordering is
retained. No sanitizer suppression or weakened assertion is used.

The private compiler's waveform preparation now copies typed vector elements
instead of type-punning whole vector objects; exact integer/fractional waveform
samples are included in the backend contract. Windows final ownership/waveform
checks pass **2/2 in 1.22s** (`faust-ownership-host-{build,tests}.log`). Rebuild the
Linux normal and fully instrumented dependency and rerun the same gates before
marking these repairs accepted. No product GUI/CLI/effect promotion yet.

The next full sanitizer run clears both undefined-behavior failures, but reports
**287 bytes / six allocations leaked** on rejected compilation; retain
`build/faust-sanitized-ownership-host-tests.log`. `PatchCompilerCleanup.cmake`
adds exception-safe lexer teardown and deletes unfinished visitor blocks, clearing
their owners only after a factory successfully takes them. This is compiler-side
cleanup, never callback work. The manager regression also checks exact PCM and
balanced configured-allocator allocations/releases using the production deleter.
Windows focused cleanup gates pass; final Linux/ASan/UBSan/LSan reruns are next.

**17 November follow-up:** the repaired full dependency-wide sanitizer run passes
**2/2 in 9.91s**, with ASan leak detection and UBSan halt-on-error enabled and no
suppressions (`faust-sanitized-cleanup-host-tests.log`). The initial two failure
logs above remain intact. Normal Linux cleanup gates also pass. A subsequent
test improvement retains cached-factory concurrency and additionally forces a
different program to compile while checking exact running stereo PCM. The old
test exercised a cache hit only; its original case is retained. Windows passes
the expanded backend gate in **3.20s** (`faust-worker-host-tests.log`). The initial
finite-PCM assertion saw an intentionally retained NaN from an earlier fixture;
the independent NaN/sanitization checks remain unchanged, and the later steady
PCM fixture now explicitly restores that input before comparing finite samples.
Verify this test-only increment on Linux normal/sanitized builds next.

The owner authorized a Faust interpreter for native effect implementations.
`source/faust/FaustInterpreter` is an opt-in, prepared portable VM backend using
pinned Faust 2.88.0; see [FAUST.md](FAUST.md) for exact build commands, hash,
licensing, real-time scope and remaining admission/ownership work. No LLVM/JIT
or product routing is added. Default builds, existing Q/C/A and preserved UI are
unchanged. This is an internal backend slice, not completed T/GUI/CLI integration.

Windows Release contract passes: actual compilation/import, independent
instances/factory lifetime, exact gain/delay/feedback, 64/128/irregular/in-place,
invalid-source/control rejection, finite-input sanitation, numerical-fault
silence/recovery and concurrent off-audio compilation. It also executes Faust's
imported stereo `aa.tanh1` implementation with exact 64/128 subdivision identity,
and prepares gain instances at 8/22.05/32/44.1/48/96/192 kHz. Instrumented processing
observes zero C++ new/delete, including first callbacks. The final patched build
passes with inherited `FAUST_INTERP_TRACE=7` and `FAUST_INTERP_OUTPUT=1`, **1/1 in
0.72s**. Retain `build/faust-{dependency,host}-{configure,build,tests}.log`.

Committed snapshots `800af5d`, `14e4559`, then `1b11360` were synced through the
non-destructive helper to the clean independent Linux mirror. Linux downloads
and verifies its own archive and builds its own static library in native storage.
Its focused run passes **2/2 in 0.28s**: interpreter contract (0.19s) plus original
WaveShaper streaming contract (0.08s). The Faust callback additionally observes
**zero wrapped malloc/calloc/realloc/free calls** from linked objects, with zero
new/delete; this does not instrument shared-library internals or every lock API.
Linux logs: `build/faust-linux-{source,dependency-configure,dependency-build,
host-configure,host-build,host-tests}.log` and `build/faust-tests/Testing/Temporary/
LastTest.log`. Exact commands are in FAUST.md. A fresh Windows selected-source
extraction and original-hash patch configure also pass (`faust-extraction-check.log`,
`faust-patch-check.log`). No package/push or product UI/device test was run.

Illustrative warmed stereo ADAA-probe means at 64/128 frames: Windows **19.67 /
38.54 us**, Linux WSL **12.49 / 24.02 us**. Observed maxima were **85.9 / 154.5 us**
and **90.29 / 92.14 us** respectively (3000 callbacks per case). These are separate
backend runs, not a cross-OS speedup, full-chain capacity or hardware-latency claim.
Faust's GCC build emits upstream warnings, including double-interpreter bitcast
array-bounds, float/double aliasing and iterator initialization. This wrapper
forces single precision, which does not resolve the aliasing warnings;
arbitrary patch/instruction safety is NOT established. Audit
any selected effect/intrinsics and run applicable sanitizers before promotion.
Runtime patch admission, bounded swapping/retirement, production integration,
state/GUI/CLI/MIDI controls and effect quality/listening remain pending.

The preceding original-wave-shaper FIR candidate passes its native streaming
contract, but **fails** the retained actual-browser legacy comparison: worst
peak error 2.63378024e-6 against the unchanged 1e-6 limit (high-drive ramps).
Do not promote/relabel it as identical. Its fixed wet delay is 192 base samples.
See `build/distortion-waveshaper-{unit,browser,all-errors}.log`; the owner's later
quality-first goal below supersedes bit matching for remaining effects, not the
honest failure record. No T sound/control has been exposed to users.

Next after backend acceptance: choose and measure a licensed organic nonlinear
model, then integrate T through persistent arbitrary-order DSP, original controls/
presets and shared GUI/CLI/state with explicit quality and lifecycle acceptance.

### Owner update: quality-first remaining effects — 16 November 2026

The owner explicitly requested the best available implementations for remaining
effects, with an organic/natural result, and is not seeking a particular original
sound. For newly ported remaining effects, prioritize researched, maintained DSP
approaches and measured quality over JS audio identity. Preserve interface,
controls, parameter/preset/session compatibility, editable order and shared
GUI/CLI ownership. Already accepted Q/C/A behavior is not being rewritten here.
The persistent/live lifecycle approval below still applies.

Keep original results/fixtures as comparisons and retained evidence. Do not
loosen old tolerances or relabel a failed legacy comparison as passing. The
original T curve compiler remains verified, but matching the browser's exact
Float32 resampling arithmetic is no longer the goal for the improved T sound.
Promotion needs explicit aliasing/frequency/DC/stability, smoothing, latency/CPU,
64/128/variable-block, ownership/cancellation and UI/CLI/state checks, plus honest
listening/audition limits. "Best" is a design goal, not a claim proved by tests.

### Owner-approved remaining-effects lifecycle — 16 November 2026

The owner selected persistent processors and live controls for **T and the
remaining loop effects**, extending Q/C/A's policy. Preserve effect memory across
ordinary PLAY/RTRG/history restarts; apply edits during OD/SUB/MULT/scheduled STOP
with suitable smoothing/crossfades. Explicit destructive/reset actions must clear
owned effect state so old tails cannot reappear. Chain order stays user-editable.
Restart transitions intentionally differ from original JS; steady-state sound,
controls, presets and IDs still require reference acceptance. Do not ask this
same lifecycle question again for each remaining effect.

### MIDI integration gate / T preparation contract — 16 November 2026

`tools/TestMouseWorkflow.ps1 -MidiOnly` passes the complete actual UI/MidiBuffer
audit at **64/128** (`midi-integration-audit.log`): malformed panic, Space focus/
repeat, Stop/CC120/123, SUS/OD/SUB, Note/CC/PC MUTE/action/header learning,
continuous CC editing/focus/drag, timeout/Escape/clear/save/reload and native bridge
send/reload recovery, with nonzero/silent PCM and clip/history checks. No product
fix was needed. These synthetic packets do not establish physical-device support.
Merge follow-up is committed locally as `c5d0eb4`; no push/package.

Next effect is **T / Distortion**. The first internal preparation slice adds
`DistortionSettings` with exact-double controls, six original presets, atomic
invalid-input rejection and the fixed 4097-float original curve builder.
Untouched defaults (Amount 40, Tone 4000) deliberately differ from the named
"default" preset (35, 5000). Equal manual edits clear the preset label; enable
survives preset/manual edits. Steps describe widgets, not quantization of values.
Curve preparation is OFF audio and requires exclusive output ownership.

Windows new targets build; **2/2 tests pass (5.05s)**. The unchanged original
factory, generic preset handler, registry and `makeDistortionCurve` are executed
by `DistortionSettingsReference.cjs`: **1273 curves / 5215481 samples, max 0 ULP
and absolute error 0**, against the predeclared 2-Float32-ULP limit. This includes
all integer Amount/cent-step Shape sweeps, fractional values, all six presets and
128 range positions per control; exact positive zero is separately required.
The initial oracle failure was a missing AudioGraph class binding in the harness;
the original method bodies/tolerance were unchanged when fixing that binding.
Keep `distortion-settings-tests.log` and `distortion-settings-fixed-tests.log`.

Snapshot **`5facd26`** is committed locally and synced through
`tools/SetupWslWorkspace.sh` to the clean independent Linux mirror. Linux builds
only `aion_distortion_settings_tests` and passes **2/2 (4.03s)**; its 1273 curves /
5215481 samples also have **max 0 ULP / absolute error 0**. Logs in Linux native
`build/`: `distortion-settings-linux-{configure,build,tests}.log`; Windows sync
log: `build/distortion-settings-wsl-sync.log`. No dependency or platform-format
changes were needed. The first Linux metrics read used unavailable `rg`; a
subsequent `grep` read confirmed the measurements and clean checkout.

**T remains disabled/unported in the product.** This is preparation, NOT a full
effect or sound-parity claim: WaveShaper interpolation/4x resampling, low-pass
tone, gain/dry-wet automation, graph lifetime, arbitrary chain position, shared
GUI/CLI/MIDI and versioned state/bank ownership still need integration. No
product command, APVTS/action/session ID or UI asset was changed. Original
partial-parameter import/factory fallbacks are not defined by this complete
settings representation; retain the original importer as a separate pending gate.

Next: implement/compare the original full distortion audio path before exposing
T, using the approved persistent/live policy above. This preparation slice is
independent of graph lifetime. Configure only because CMake gained targets:

```powershell
cmake -S . -B build/windows-core
cmake --build build/windows-core --config Release --target aion_distortion_settings_tests --parallel 2
ctest --test-dir build/windows-core -C Release -R '^(distortion_settings_contract|original_distortion_settings_contract)$' --output-on-failure --no-tests=error
```

```sh
# After committed-snapshot sync; no Windows caches copied.
cd /home/jorge/projects/Aion-Looper
cmake -S . -B build/linux
cmake --build build/linux --target aion_distortion_settings_tests --parallel 2
ctest --test-dir build/linux -R '^(distortion_settings_contract|original_distortion_settings_contract)$' --output-on-failure --no-tests=error
```

### MERGE phase preconditions / natural grid — 16 November 2026

Full 64-sample replay at `26ead2d` passes core mouse, export and MERGE
(`merge-context64-diagnostic.log`). The constructed-source assertion was observed
at frame **16764096**, just **2112 frames / 44ms** before its original deadline
16766208. GO advanced request 218 to 219 only. This exposes inadequate timing
headroom in the fixture; the earlier failing run had no deadline capture, so its
exact cause remains unproven. No native defect was reproduced in this replay.

The strict queued-source/destination cases now wait out the last tenth of their
existing 128-second grid. Scheduled-STOP cases, which export PCM before GO, wait
for the first half. Waits are bounded at 150 seconds and log admission phase/frame.
Clock settings and every existing queued/stopping/deadline/identity/history/WAV
assertion stay intact. A separate real-pointer case uses a two-second grid to
observe queued -> natural playing, then GO/decline: source stays playing with the
same identity, destination stays stopped, both exports equal the source stereo
WAV byte-for-byte. It does not permit playing in the strict pre-grid cases.

Expanded MERGE passes **64/128 samples** (`merge-phase-acceptance.log`), including
forward/reversed pending source/destination/both, confirmation/history, panic and
active capture. Actual phase waits cross .558458 -> .000406 and .518 -> .002469
without changing the clock. Preserve failed
`merge-grid-acceptance.log` (new audible case inherited MASTER=0; explicitly fixed)
and `merge-grid-acceptance-fixed.log` (later STOP case expired during exports).
The latter also lacked a deadline capture; subsequent runs log both frames.

Fresh Windows/Linux core builds pass **32 queued-source + 88 scheduled-STOP
cases each**, all with Engine 64/128, paired/exact PCM, history, cancellation/OOM
and zero callback new/delete (`merge-boundary-{windows,linux}-{source,stop}.log`).
Linux uses the clean independent `5efac6b` mirror: its C++ source is unchanged
relative to this Windows test-only edit, so no uncommitted files were copied.
The five selected unchanged-JS/reference comparisons pass on Windows **5/5
(156.29s)** and Linux **5/5 (258.09s)**, with `--no-tests=error`.
The final Linux log was confirmed after approval-reviewed commands became
available again; its earlier read was blocked by the review service usage limit,
not an unsafe-action finding. The independent Linux mirror remains clean.

Next: the remaining actual MIDI UI integration audit. Keep the native near-grid paired-PCM coverage; phase preconditions are
not a substitute for audio boundary tests. No product/DSP/assets/IDs changed;
no package/push or new physical-device/Linux WebKit validation.

Reproduction (existing configured caches; Windows uses the bundled CMake/CTest
under `.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin`):

```powershell
tools/TestMouseWorkflow.ps1 -Blocks 64 -SkipMidi
tools/TestMouseWorkflow.ps1 -MergeOnly
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
build/windows-core/Release/aion_core_tests.exe --queued-merge-source
build/windows-core/Release/aion_core_tests.exe --stopping-merge
ctest --test-dir build/windows-core -C Release -R '^(js_native_merge_queued_source|original_merge_queued_destination|js_native_merge_queued_destination|js_native_merge_stopping_source|js_native_merge_stopping_destination)$' --output-on-failure --no-tests=error
```

```sh
cd /home/jorge/projects/Aion-Looper
cmake --build build/linux --target aion_core_tests aion_start_probe --parallel 2
./build/linux/aion_core_tests --queued-merge-source
./build/linux/aion_core_tests --stopping-merge
ctest --test-dir build/linux -R '^(js_native_merge_queued_source|original_merge_queued_destination|js_native_merge_queued_destination|js_native_merge_stopping_source|js_native_merge_stopping_destination)$' --output-on-failure --no-tests=error
```

### Windows editor visibility repair — 16 November 2026

Reproduced the blank editor with a controlled native window hide/show: a loaded
root remained at about:blank after the first zero-ms cycle. CDP history showed
root + blank, current index 1, no script exceptions. The pinned Windows JUCE
backend attempts goBack on show before blank navigation necessarily commits.

Windows Browser now owns that unloading policy: hide explicitly navigates to
blank and immediately cancels native STUT/export/learn; show waits for blank
completion before explicitly restoring the remembered allowed document. JUCE's
history-based unload is disabled, **not** document unloading. The initial trial
that immediately restored could cancel blank navigation before unload; the final
implementation waits for completion and tests a fresh document on every cycle.
Intentional blank documents remain blank, without mixer bootstrap. Linux and
Apple behavior is unchanged behind JUCE_WINDOWS. No DSP/parameter/asset change.

Runtime snapshot `e0d371c`: Windows Standalone and owned mouse host build. New
`TestEditorVisibility.ps1` passes **13 native cycles each at 64/128**, requested
hide durations 0/1/10/100ms. It verifies actual document unload, continuing
nonzero PCM/unchanged clip, held STUT release, lost-keyup STOP cancellation beyond
four seconds, MIDI learn cancellation, stale valid export destination rejection,
intentional blank preservation and native audio evidence belonging to this host.
Actual Arp UI regressions pass both blocks using normal startup (not the fixture
steady-start option). Windows frontend/bridge scripts pass. Linux Standalone
builds and frontend/bridge CTest passes **2/2 (10.53s)**. An initial mistyped CTest
selection found no tests; only the corrected registered names count as evidence.
No physical device, OS dialog, Linux WebKit, Apple or wrapper pass is claimed.

Broader mouse diagnostics exposed two fixture assumptions, repaired without
changing product behavior, thresholds or post-press timing:

- The deferred four-second STOP case could start while the actual controller
  still displayed FINISH and captured recording-origin, despite a newer native
  RPC reporting auto-play. The isolated preceding DEL/re-record reproduction
  records initialState=recording/canDelete=false and correctly keeps PCM. The
  positive test now waits for visible STOP and asserts captured playing-origin/
  canDelete=true. Original 2800/1400ms waits, 1200ms delete check and no-resurrection
  assertion remain. Focused replay passes 64/128; FINISH-origin protections and
  existing negative/boundary regressions remain intact.
- The queued-stop audio check queried again after its scheduled stop completed.
  It now asserts the unchanged >0.02 PCM threshold on the already captured
  stopping snapshot, tying state/frame/audio together rather than requiring sound
  after the deadline. The full run passes that case and its symbols/history checks.

Export regression initially supplied a picker immediately after CDP reload ACK,
which merely schedules navigation. It now waits for main-document commit while
script execution remains disabled, then supplies the obsolete destination.
The original 600ms file-suppression assertion stays, with neither unload RPC nor
new-page bootstrap available. Complete ExportWorkflow passes **64/128**, including
exact stored stereo WAV, replacement/late callbacks, errors/recovery, overwrite,
DEL and unchanged transport; startup cleanup packet count remains zero. The
helper's generic "mouse workflow" footer does not mean the full mouse suite passed.

**Early queued-speed integration follow-up — 16 November:** the prior full run
failed with playing instead of queued. An isolated replay of the unchanged case
also missed source admission before sending the speed key. The general helpers
wait 100ms after each matching snapshot, plus 160ms and another snapshot for each
duplicate check; those observations consume the original two-second grid window.
The shared early-speed case now dispatches and observes without those artificial
post-match waits, checking the same individual request increments and delayed
duplicate suppression after the deadline-sensitive steps. Master tempo, clip
duration, queued/silence assertions, source identity/history/deadline checks and
the trusted Home key remain unchanged. No product DSP or UI code changed.

`tools/TestMouseWorkflow.ps1 -EarlySpeedOnly` passes **64/128** with actual
processor/editor PCM. Observed quarter-speed edit / original scheduled frame:
**741824 / 768000 (64)** and **647552 / 672000 (128)**, both queued and silent.
Both subsequently play nonzero PCM and reset speed through the original button.
The focused option and broad workflow call the same test body. Windows frontend
and bridge scripts pass. Logs: `early-speed-timely-retry.log`,
`early-speed-frontend.log`, `early-speed-bridge.log`. Initial sandbox launch failed
to expose CDP and left its owned test host; only that verified PID was terminated
before the successful permitted run. That attempt is not audio-test evidence.

The first broad replay (`early-speed-full-workflow.log`) then missed the transient
STOPPING state in a separate snapshot after the STOP helper completed. The shared
pending-stop case now observes native state/PCM and actual `[.]`/`STOPPING` DOM
together, sends the second STOP immediately, and checks duplication afterward.
The focused unchanged RTRG case also reproduced late observation of an already
constructed source. It now observes queued/unborn and ready-source states without
the same artificial waits; both original button-enabled assertions remain.

The expanded `-EarlySpeedOnly` option runs the shared STOP/display, speed and
RTRG cases, **all passing at 64/128** (`static-deadlines-focused.log`). RTRG
observed frame / original grid: **1133248 / 1152000 (64)** and
**1138176 / 1152000 (128)**, with exact zero pre-grid PCM, later nonzero playback
and unchanged one-command-per-gesture checks. No threshold, grid, product
parameter, DSP or preserved asset was changed. Initial focused failure retained
in `static-deadlines-rtrg-baseline.log`.

The broad replays pass the complete **64/128 core mouse workflow and export**
(`static-deadlines-full-workflow.log`, `static-deadlines-full128.log`): all three deadline cases, held/deferred
STOP, STUT/blur/reload, Start/Q/nudge, exact Multiply WAV/Undo/Redo, count-in,
DEL ALL confirmation/cancellation, keypad/lost-release recovery and LIVE strips.
The 64-sample run then fails the source-side queued MERGE assertion (playing versus queued,
`MergeWorkflow.cjs`). No source/audio-deadline evidence was captured for that
failure; do not assume a crossed deadline or a native defect. Added narrow
queued/observed-frame/deadline/configuration diagnostics without changing the
MERGE assertion or PCM tests. Isolated `-MergeOnly` passes the complete **64/128**
cases (`merge-queued-deadline-baseline.log`), including both unconstructed/
constructed source variants, which remain queued before frame 6144000, exact
saved stereo PCM, confirmation, Undo/Redo, panic and active-capture guards.
Pointer-target collection now starts before these variants (same bounded
64-event diagnostic) so a recurrence records the actual GO target as well.

Full **`-Blocks 128 -SkipMidi` passes** (`static-deadlines-full128.log`), including
the complete core loop, export and MERGE workflows. Its queued-source observations
are frame 16450560 before original deadline 16770560 (unconstructed), and
16953728 before 22914560 (constructed); each GO advances one request only.
This passing replay does not explain or erase the earlier 64-sample failure.

**Historical next step (follow-up above):** repeat the full 64-sample context
with the new MERGE diagnostics (`tools/TestMouseWorkflow.ps1 -Blocks 64 -SkipMidi`).
Compare queued/observed frames to the original deadline and actual pointer/
request evidence. If it crossed the deadline, establish the queued-case phase
precondition without changing the clock or assertions; otherwise inspect native
source ownership. Then complete the remaining actual MIDI UI integration audit.
Do not advance to a new effect or claim full Arp/release acceptance yet.

This follow-up changes Windows test fixtures and notes only, using the existing
production processor/editor binary; no native rebuild, Linux audio rerun,
dependency/version change, package or push. Prior Linux evidence remains scoped
as documented above. No physical-device, new OS-picker or release claim follows
from the synthetic-host checks. Node syntax and `git diff --check` pass.

Commands and evidence (all logs under Windows `build/`, or Linux native `build/`):

```powershell
cmake --build build/windows --config Release --target AionLooper_Standalone aion_mouse_host --parallel 2
tools/TestEditorVisibility.ps1
tools/TestEditorVisibility.ps1 -HeldStopOnly
tools/TestArpUi.ps1
tools/TestMouseWorkflow.ps1 -ExportOnly -SkipMidi
tools/TestMouseWorkflow.ps1 -EarlySpeedOnly
tools/TestMouseWorkflow.ps1 -SkipMidi
tools/TestMouseWorkflow.ps1 -MergeOnly
tools/TestMouseWorkflow.ps1 -Blocks 128 -SkipMidi
node tests/FrontendTests.cjs
node tests/BridgeTests.cjs
# Remaining failing integration gate, not counted as passing:
tools/TestMouseWorkflow.ps1 -SkipMidi
```

Passing logs: `editor-visibility-final.log`,
`editor-visibility-held-rendered-stop.log`, `editor-visibility-arp-regression.log`,
`editor-visibility-export-navigation.log`, `editor-visibility-frontend.log`,
`editor-visibility-bridge.log`. Keep failed `editor-visibility-baseline.log`,
`editor-visibility-held-origin2.log`, `editor-visibility-mouse-regression.log`,
`editor-visibility-mouse-diagnostic.log`, `editor-visibility-mouse-rendered-stop.log`,
`editor-visibility-mouse-callback-audio.log`, `editor-visibility-export-regression.log`.
The final visibility run includes the stronger host-identity evidence assertion.

```sh
cmake --build --preset linux-dev --parallel 2
ctest --test-dir build/linux -R '^(native_frontend_commands|native_bridge_lifecycle)$' --output-on-failure --no-tests=error
```

Linux logs: `editor-visibility-linux-build.log`,
`editor-visibility-linux-frontend.log`. No package, version/dependency bump or
preserved UI/reference/logo edits. The optional visibility mailbox and steady-start
flag live only in the EXCLUDE_FROM_ALL synthetic test host, with safe delayed-show
ownership; they are not production RPCs or CLI features.

### Organic Arp product integration — 15 November 2026

Fresh effects now use Loose on their first successful enable. Existing native
version-1/absent Arp banks retain Original, including load-before-prepare.
Version 2 persists mode/fresh classification and the existing exact seed and
controls. Original presets select Original; explicit mode choice clears preset
identity but keeps its values. Bypass, MIDI, restarts and save/reload preserve
mode/seed. DEL creates a fresh effect. Per-track deterministic seeds and exact
semantics/CLI examples are documented in ARP_ORGANIC_VOICING.md.

One bounded descriptor carries mode through worker preparation and bank restore;
live changes preserve delay/STAY/envelope/reference RNG and settle stereo to
centre smoothly. No new callback allocation, lock, I/O or DSP engine. Existing
IDs, original assets/logo, runtime track capacity and wrapper policy unchanged.
The native selector and `arp TRACK voice --voicing original|loose` share the
existing processor/owner command, validation and exact acknowledgements.

Windows core/connected/owner/bank/original-JS **5/5 (42.47s)**, then final
pan-settling/core-owner/original-JS **3/3 (36.15s)**. A stronger independent
worker-mode oracle subsequently passes both block sizes in the owner executable,
alongside exact nonzero stereo prepared/restored/live comparisons. Production
Arp state/CLI + full existing MIDI processor **3/3 (219.53s)**. Frontend checks
pass. Windows Standalone and hardware-free UI host build.
The final added load-before-prepare legacy regression is also rerun on Windows:
production Arp **2/2 (21.59s)**, `arp-voicing-cold-restore-tests.log`.

Actual WebView2 passes in isolated **64** and **128** runs: fresh Loose enable,
trusted keyboard mode selection, original preset selection, nonzero playback,
seed/clip preservation, STAY/controls/tempo, arbitrary A omission/reinsertion,
MIDI DOM learn/conflict/removal and reload. Earlier attempts timed out or reached
an empty about:blank before bootstrap during concurrent testing; their cause is
not established. Added startup diagnostics retain the same bounds/assertions;
no forced navigation, timing relaxation or product DSP change masks them.

Two reproduced issues were fixed: fresh metadata initially scheduled an
unnecessary startup restore, blocking first enable; prepare now separates that
metadata while retaining explicit legacy restore intent. A new owner fixture
incorrectly expected ordinary commands to dispatch at zero frames; it now waits
for the exact receipt in a real callback and checks the graph clock continues.
The old future-schema rejection moves from version 2 to 3, retaining rejection
and adding malformed-mode/fresh and actual version-1 rollback coverage.

Linux committed runtime snapshot `73273e9`: Standalone builds; selected organic/
connected/owner/bank/original-JS + production Arp/CLI tests **7/7 (43.64s)**.
Snapshot `5ad710b` adds the independent worker oracle and stronger real-editor
startup diagnostics: Linux follow-up owner oracle passes; ASan/UBSan/leaks
organic + owner + bank **3/3 (39.69s)** without diagnostics. Sanitizers cover
core ownership, not the JUCE processor. Final saved browser graph-fixture replay
passes **63 comparisons / 4,128,768 stereo frames**, max **2.3842e-7** against
the unchanged 1e-6 limit (`arp-voicing-original-browser-replay.log`); this reuses
previously captured original browser output, not a fresh browser sound capture.
Final Linux TSAN organic/owner/bank **3/3 (60.78s)** without diagnostics
(`arp-voicing-tsan-tests.log`, existing cache and `setarch x86_64 -R`).
No package or wrapper build. Current Arp is a working preview, not full release
acceptance. Physical/listening, Linux WebKit GUI, broader long-session, original
JS project import and remaining unported FX stay open. Next is the remaining
Arp lifecycle/integration audit, prioritizing the intermittent blank startup
before declaring completion or moving to another reference effect.

Narrow startup follow-up: the pinned JUCE WebView defaults to unloading on hide;
its Windows implementation navigates to about:blank and attempts goBack when
shown. The test host deliberately hides/shows at startup. This is a relevant
mechanism, **not established causation** for these failures. Do not blindly add
keep-page-loaded: retaining a hidden document must also retain blur/held-input,
STUT, export and MIDI-learn cancellation safety. Reproduce with visibility and
navigation evidence, then verify any fix against those lifecycle gates.

Commands/logs for this slice (existing configured caches; no dependency change):

```powershell
cmake --build build/windows --config Release --target AionLooper_Standalone aion_arp_processor_tests aion_mouse_host aion_midi_processor_tests --parallel 2
ctest --test-dir build/windows -C Release -R '^(native_arp_processor_(64|128)|native_midi_processor)$' --output-on-failure --no-tests=error
cmake --build build/windows-core --config Release --target aion_arp_organic_tests aion_arp_connected_tests aion_arp_owner_tests aion_arp_owner_bank_tests aion_arp_probe --parallel 2
ctest --test-dir build/windows-core -C Release -R '^(arp_organic_dsp_contract|arp_connected_contract|arp_owner_contract|arp_owner_bank_contract|js_native_arp_delay)$' --output-on-failure --no-tests=error
tools/TestArpUi.ps1 -Blocks 64
tools/TestArpUi.ps1 -Blocks 128
node tests/FrontendTests.cjs
node tests/ArpGraphWebAudio.cjs --fixtures build/arp-graph-browser-fixtures.json
```

Windows logs: `arp-voicing-core-final-tests.log`,
`arp-voicing-final-kernel-tests.log`, `arp-voicing-worker-oracle.log`,
`arp-voicing-midi-product-tests.log`, `arp-voicing-ui-isolated.log`,
`arp-voicing-ui128-isolated.log`, `arp-voicing-final-frontend.log` under `build/`.
Keep `arp-voicing-ui-tests.log`, `arp-voicing-ui-keyboard-tests.log` and
`arp-voicing-ui-startup-diagnostic.log` as failed-attempt evidence. Run editor
checks separately from stress/broad suites; their earlier startup failure is
not attributed conclusively to concurrent load.

```sh
cmake --build build/linux --target AionLooper_Standalone aion_arp_processor_tests aion_arp_organic_tests aion_arp_connected_tests aion_arp_owner_tests aion_arp_owner_bank_tests aion_arp_probe --parallel 2
ctest --test-dir build/linux -R '^(arp_organic_dsp_contract|arp_connected_contract|arp_owner_contract|arp_owner_bank_contract|js_native_arp_delay|native_arp_processor_(64|128))$' --output-on-failure --no-tests=error
cmake --build build/overlap-asan --target aion_arp_organic_tests aion_arp_owner_tests aion_arp_owner_bank_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan -R '^(arp_organic_dsp_contract|arp_owner_contract|arp_owner_bank_contract)$' --output-on-failure --no-tests=error
cmake --build build/arp-storage-tsan --target aion_arp_organic_tests aion_arp_owner_tests aion_arp_owner_bank_tests --parallel 2
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir build/arp-storage-tsan -R '^(arp_organic_dsp_contract|arp_owner_contract|arp_owner_bank_contract)$' --output-on-failure --no-tests=error
```

Linux native `build/` logs: `arp-voicing-linux-tests.log`,
`arp-voicing-worker-oracle.log`, `arp-voicing-asan-tests.log`.

### Organic Arp DSP preview — 14 November 2026

Optional `loose` preparation now adds bounded, grid-anchored note timing,
per-note envelope dynamics and smoothed stereo balance through the existing
Arp kernel/streaming graph. Separate fixed-state randomness preserves original
RNG behavior. Feel zero on a fresh graph gives exact original PCM. Product
defaults, native sessions/presets, GUI/CLI and preserved assets remain unchanged;
this is **DSP groundwork, not finished new-instance product integration**.

Windows focused organic/original kernel/stream/reference checks pass **4/4,
30.11s**; Linux **4/4, 30.69s**. New tests cover 100,000 anchored intervals,
actual kernel onsets, exact block 1/37/64/128 PCM/state at seven rates 8–192kHz,
live tempo/STAY/Feel/feedback changes, seed/reset, mono/stereo/tails/in-place,
idle and invalid-mode preservation, with zero observed callback new/delete.
An added actual four-second-to-minimum-delay regression also passes on both OSes
and under Linux sanitizers.
Linux ASan/UBSan/leaks organic+streaming **2/2, 19.13s** without diagnostics.

Saved unchanged-browser fixtures replay against the new Windows probe: **93
comparisons, 6,094,848 stereo frames**, maximum error **2.3842e-7**, retaining
the existing 1e-6 limit. These are fixture replays, not a fresh browser/GUI run.
Audition WAVs (dry/original/loose, same seed/input/controls, Feel 0.35) render
12 seconds at 48kHz without clipping. Windows warmed 20-Arp-only benchmark:
original/loose p99 **537.2/550.2us** at 64 and **1032.3/1011.4us** at 128;
no observed wall deadline overruns in 1,024 measured callbacks per case. Single
runs do not establish a speedup, full-application deadline or driver latency.
Linux loose p99 **484.308/958.910us**, also no observed wall overruns in that
same limited fixture. Final Windows/Linux Standalone development builds pass.

Next: versioned original/loose mode and seed ownership, fresh-instance/legacy/
preset classification, shared GUI/CLI access and actual product-state/UI
acceptance. Do not turn this preview on globally or infer mode from Float32
controls. See `ARP_ORGANIC_VOICING.md` for policy, commands, evidence and limits.
Listening preference, physical devices, Linux WebKit and broader integration
remain unverified. No package or wrapper build.

### Arp MIDI sliders / exact state / bounded routing — 13 November 2026

Nine original sliders append IDs 124..132. The existing selected-loop CC learn/
conflict/filter/cancel path now reaches native Arp at event sample offsets.
Exact double receipts preserve original controls/BPM/preset identity; a 511-entry
heap SPSC journal rejects before mutation on overflow. MIDI version 6 appends nine
bindings, retaining versions 1..5. Save/stopped prepare consume already accepted
MIDI originals, including with no editor. Sync/STAY/enable are not CC sliders.

Fixes retain DEL followed by fresh CC in the same callback (replay exact receipts
from DEL's zero revision), unread MIDI during save/prepare, and original two-decimal
labels. Tagged browser Arp fields use round-trip JSON to avoid JUCE's 15-digit
completion rounding; the reply envelope, document token fence and legacy direct
object replies remain intact. All snapshot writers share the existing non-audio
mutex with host save/prepare readers. No audio lock or product AudioContext.

Windows: unchanged-JS/control/core/frontend **6/6 (61.27s)** plus focused core
`aion_core_tests --midi` passes: 6,144 linear-router comparisons and all retained
MIDI PCM/WAV/hold/overflow cases. Arp ownership/DEL repair **2/2 (7.49s)**. Final
Arp/full processor MIDI **3/3 (164.13s)**; FX restore **2/2** also passed. Actual
Arp WebView2 passes 64/128 with MIDI/DOM/tab/conflict/removal, exact fractional
values, reopen, injected obsolete-document completion and native legacy reply.
The broad core gate was inadvertently started and cancelled without a result;
it is **not** counted as passing. The existing focused MIDI mode was then used.

Fixed CC routing uses a 4 KiB audio-owned lookup rebuilt once after a bounded
31-config batch. No event is coalesced/dropped by that optimization. Matched
Windows 20-loop Q/C/A fixture, 48kHz, 256 CC/callback: p99 **1649.9/3046.6us**
before, **1209.9/2283.5us** after (64/128); observed wall overruns **897/260 → 2/5**.
Baseline itself had **1/3** wall overruns; this is not a hard real-time or driver/
round-trip guarantee. See ARP_DELAY_MIGRATION.md for scope, means and commands.

CLI reply publication now retains one bounded response and retries the file write
off audio, never the accepted action. Three injected write failures formerly lost
the first reply; both block-size regressions now recover with exactly one preset
execution. This does not prove the cause of every historical timeout. Both full
64/128 external CLI workflows run concurrently under the unchanged shared 60s
watchdog with all assertions retained (one measured combined run: 31.84s).

Linux snapshot `baac71c`: focused core MIDI and **12/12 (171.92s)** pass, ASan/
UBSan/leaks focused MIDI plus ownership **3/3 (43.56s)**, TSAN ownership **3/3
(74.31s)** without diagnostics. Final `5b15745` Linux snapshot-lock/Arp/full MIDI
follow-up passes **3/3 (124.37s)**; final Windows editor rerun also passes 64/128.
Optimized Linux 256-CC fixture p99 is **1104.23/1883.24us**, with **1/0** observed
wall overruns. Both Standalones build. No package; assets/logo/IDs preserved.
Next after final acceptance: the approved loose new-instance organic voicing;
preserve original presets/saved sound. Physical MIDI/listening, Linux WebKit GUI,
driver/long-session and broader integration/release checks remain separate gates.

### Arp automatic Master timing — 13 November 2026

Master timing gestures now propagate BPM to existing active Arp graphs, using
the unchanged `scales.js updateSync` immediate BPM write rather than the knob's
50ms target smoothing. A bounded twenty-track transaction preserves delay/STAY/
LFO state, presets, seeds, inactive BPMs and user command receipts. GUI/CLI share
this path; rapid Arp edits wait for timing maintenance. Immediate save includes
accepted timing intent, including a first enable still preparing. Prepare and
newer session restore retire older maintenance without resurrecting old state.

Windows: core/reference **7/7 (55.84s)**, frontend/CLI admission **2/2 (6.83s)**,
Arp/FX production **4/4 (15.08s)**; stronger pending-enable/prepare/restore Arp
production rerun **2/2 (14.66s)**. Actual WebView2 controls/PCM/reload and same-turn
BPM/Arp edit pass **64/128**. Browser oracle: **15 comparisons, 983,040 stereo
frames**, five rates 8–96kHz, blocks 64/128/37, peak **1.3318e-7** (unchanged
1e-6 limit). See ARP_DELAY_MIGRATION.md for commands and logs. Standalone builds.
Snapshot `58a0519`: Linux Standalone builds and selected production/CLI/core/
reference/frontend tests pass **13/13 (50.03s)**. Linux replays the same browser
fixture with identical maximum error. Core ASan/UBSan/leaks pass **3/3 (32.76s)**;
TSAN passes **3/3 (49.05s)**, without diagnostics. Sanitizers cover the core
ownership tests, not JUCE processor code. Linux mirror stayed clean.

External APVTS timing changes are observed by the existing message service
(25ms Timer); this is not sample-accurate host automation. GUI/CLI submit timing
intent synchronously; admitted banks commit atomically at callback start. Busy
unadmitted timing gestures coalesce to latest intent. No product AudioContext,
callback allocation or worker graph rebuild is added. Original assets unchanged.
Next after this acceptance: Arp MIDI original-value ownership, then the approved
loose new-instance voicing. Arp remains a preview; prior device/Linux-WebKit and
intermittent MERGE validation limits remain as recorded below. No package.

### Arp product state / shared GUI / CLI preview — 13 November 2026

Original Arp controls/presets now have production state and shared command access,
including worker-prepared enable and the original visible A controls. One
`ArpSettingsOwner` consumes all Arp receipts; coordinated FX restore retains its
callback-serial fence. Versioned twenty-track native state preserves original
doubles/BPM/seed and rejects malformed banks atomically; old native sessions load
Arp defaults/off. Prepare restores active graphs through the worker. New tests
retain DEL barriers and zero callback new/delete. CLI confirmation uses exact Arp
receipts; its JSON formatting now retains double precision. The ordinary FIFO
fence is separate from Arp preparation/bank IDs (a reproduced restore deadlock).

| Reference behavior | Native status / evidence | Remaining |
| --- | --- | --- |
| Original controls, eight presets, sound | Accepted core DSP reused; Windows core/reference 11/11 | Full product transition/listening acceptance |
| Original A controls, editable chain | Real WebView2 pointer/key/PCM/reload passes 64/128 | Broader OD/SUB/MULT/STOP cases |
| Original controls/state identity | Versioned native bank + exact receipt owner, new production tests | Original JS project importer |
| Shared command access | GUI + `tools/aion.py arp` connected | Final cross-platform evidence below |
| Master BPM / MIDI / organic new-instance sound | Gesture BPM capture only | Automatic propagation, MIDI original-value ownership, organic audition/policy |

This is a working **preview**, not finished Arp or release acceptance. See the
new section of ARP_DELAY_MIGRATION.md and CLI.md. No dependency/parameter ID,
reference/resource UI, package or push change. Continue Arp rather than another FX.

Snapshot `9deb299`: Windows production/CLI/MIDI **11/11 (212.53s)**, Linux **11/11
(143.71s)**, core/reference Windows **11/11 (73.27s)** and Linux **11/11 (39.56s)**,
core ASan/UBSan/leaks **3/3 (33.27s)**. Final prepare correction adds same-rate
retained-graph binding and atomic stopped reset/intent; Windows Arp/FX state
**4/4 (14.25s)**. Its Linux and full mouse-workflow evidence follows. The mouse
fixture's old unborn-RTRG disabled assertion contradicted accepted 9 November
behavior; it now retains source-readiness coverage and checks enabled admission.
Do not revert product RTRG behavior to satisfy that obsolete expectation.
Owner requests organic voicing in this work and selected loose timing, wider
dynamics and natural stereo movement for new instances. Its implementation and
compatible persisted mode/seed policy remain pending; raising Feel alone does
not vary the existing note-onset clock. Core sanitizers do not cover JUCE
processor code; physical IO/listening/Linux WebKit GUI remain unverified.

Final lifecycle `56a98bc` Linux Arp/FX rerun **4/4 (3.77s)**; both Standalones
build. Full main mouse and export checks pass at **64/128**. The wrapper's MERGE
scheduled-STOP case failed once at 128 (`stopped` vs `stopping`); unchanged isolated
128 MERGE passes. Keep `arp-product-mouse3.log` and `arp-product-merge128-rerun.log`;
do not claim the whole wrapper passed or a proven timing root cause.
NativeUiSmoke requires the real Standalone, not the hardware-free host. That
attempt failed its expected holder/hotplug precondition; a real Standalone retry
did not expose the CDP page before timeout, so actual Standalone smoke is unverified.
Its owned app was closed and settings restored. Next: automatic Master BPM into
the exact original owner, then Arp MIDI and approved loose voicing. Do not infer
original MIDI control values from render floats or advance to another effect.

### Production inactive FX state restore — 13 November 2026

Existing Q/C/chain state loads now use the coordinated native bank instead of a
60-command batch. The message owner waits for older ordinary FIFO actions to
settle, then owns one exact bank receipt. Callback serial fencing rejects older
snapshots even at an unchanged zero-frame clock. Host threads publish validated
intent/cancellation only for this FX path; rapid newer loads supersede old intent
without old results overwriting it. The existing Timer drains bounded telemetry
while restoring, so closed editors do not strand later loads. Stopped prepare
retains existing pending-state behavior and invalidates obsolete completions.

Product A remains disabled: only inactive A render settings are carried unchanged,
without reconstructing original controls or adding an Arp saved-state schema.
All-off graph banks now commit in their first callback with no worker allocation,
preserving the existing one-callback inactive-load workflow. Windows production
state/legacy/shared CLI checks pass **8/8 (46.87s)**, core ownership **4/4 (32.00s)**,
and actual WebView2 chain interaction/PCM checks pass at **64/128**. Windows MIDI
passes **1/1 (135.18s)**; Linux production/CLI/MIDI **9/9 (125.87s)** and core
**4/4 (4.14s)** pass. Core ASan/UBSan/leaks **3/3 (28.55s)** and TSAN **3/3
(45.25s)** pass without diagnostics. Both Standalones build.
Runtime `0c179a8`, final cleanup fix `b71f203`: a reproduced pending-restore gate
rejected STOP ALL. It now permits validated STOP ALL/DEL/DEL ALL; focused cleanup
tests pass Windows **2/2 (0.91s)** and Linux **2/2 (0.61s)**, and final actual
WebView2 chain checks pass again at 64/128. No assertion was removed or weakened.
See the Arp migration notes for exact commands, logs and scope: core sanitizers
do not establish sanitizer coverage of the JUCE processor or physical drivers.
No package/push. Next: versioned original Arp state/owner binding, integrating
with this sole receipt owner rather than creating a competing consumer, then
shared GUI/CLI/MIDI/BPM/voicing acceptance before enabling A.

### Coordinated native FX bank — 13 November 2026

The accepted staged Arp transaction can now optionally carry all twenty EQ,
compressor and user-chain settings. Those controls stay unchanged until every
Arp graph is ready, then commit with A before the callback renders/publishes a
snapshot. OOM/cancellation preserves the old complete FX bank. A newer accepted
Q/C/chain edit cancels the pending full restore; a rejected edit does not.
Arp-only requests retain their original behavior and preserve independent FX.

Runtime `8efbcaa`, expanded PCM regression `80f0a48`: Windows affected selection
passes **15/15 (67.97s)** and Linux **15/15 (17.48s)**. Both Standalones build.
ASan/UBSan/leaks passes **3/3 (26.71s)** and TSAN **3/3 (39.92s)** without
sanitizer diagnostics. Final live successful replacement/reorder, OOM/cancellation
and exact nonzero stereo PCM at 64/128 pass the expanded contract separately:
Windows **4.70s**, Linux **0.95s**, ASan **13.71s**, TSAN **20.02s**. Retain
zero watched callback new/delete, full-bank validation and paused accepted/rejected
edit races. One worker test barrier needed an acknowledgement before reuse; no
acceptance assertion changed. Prior unchanged-JS Arp DSP evidence is reused;
connected EQ and original chain/settings regressions were rerun. This is an internal
coordinated audio transaction, not yet product state/GUI/CLI integration. A remains
disabled. Next: connect production state intent/serialization and shared commands
to this transaction, retaining old state on failed admission/preparation. No package/push.

### Atomic original Arp bank restore — 13 November 2026

The internal settings owner now stages a complete known original Arp bank through
the accepted Engine graph-bank transaction. It commits all original double values,
preset metadata, BPM and seeds only after validating all twenty epochs/revisions
in one immutable terminal receipt. OOM, busy, cancellation and queued prepare
preserve the old original bank. A clear after successful adoption wins only on
its own track; the other accepted originals remain intact. No Float32 inference.

Runtime `4c71603`, final fixtures `46cb342`: Windows affected selection passes
**10/10 (53.83s)** and Linux **10/10 (11.80s)**; both Standalones build. Exact
nonzero stereo PCM versus the accepted raw Engine bank path passes at 64/128,
along with all-twenty enable/disable, closed telemetry/unread results, concurrent
callback publication and zero watched callback new/delete. The final owner-bank
contract passes three consecutive runs per configuration: Windows **9.88s**,
Linux Release **1.65s**, ASan/UBSan/leaks **20.73s**, TSAN **32.27s**. The retained
three sanitizer contracts also passed; no sanitizer diagnostics were reported.
Initial sanitizer runs exposed stale-snapshot assumptions in the new fixture.
It now explicitly obtains fresh audio-owned acknowledgement/clear/generation
observations. Original stale-view coverage and all assertions remain; no product
code, DSP or tolerances changed for those repairs. Failed logs remain recorded in
[ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md). Reuse prior unchanged-JS PCM
evidence: the DSP/automation math and preserved assets did not change.
This is **Arp-only internal state ownership**, not a complete FX/session restore
or exposed GUI/CLI feature. Product A remains disabled. Next: production owner/
state integration and coordinated Q/C/chain/Arp restore, then shared GUI/CLI, MIDI,
BPM and approved new-instance voicing acceptance. No package or push.

### Original Arp settings owner — 12 November 2026

The internal `ArpSettingsOwner` now retains original double controls, preset,
BPM and seed on the message owner. Edits/presets commit only after an exact
per-track terminal receipt; unrelated global acknowledgements and stale telemetry
cannot confirm them. Admission is atomic across the existing bounded preparation
and performance FIFOs. Cancellation uses non-reused internal identities; future
cancelled entries cannot block or complete a replacement. No new product IDs.

Acceptance: exact stereo PCM against the accepted preset path at 64/128,
original decimal preservation, all twenty unread receipts, full-queue rejection,
OOM/enable rollback, clear/prepare lifetime, MIDI Stop purge and concurrent
message/audio publication, with zero watched callback new/delete. Runtime/tests
`8729d33`: Windows affected selection **10/10 (74.99s)** plus original assets
**1/1 (1.79s)**; Linux **11/11 (42.27s)**. ASan/UBSan/leaks **3/3 (20.78s)**
and TSAN **3/3 (33.13s)** pass without sanitizer diagnostics. Saved unchanged
WebView2 preset PCM replay passes all **15** comparisons per OS, maximum
**6.70552e-8**, retaining the original 1e-6 peak / 1e-5 relative RMS limits.
Windows and Linux Standalones build successfully.
This is an internal adapter, **not connected product GUI/CLI/state support**.
Product A remains disabled. Next: integrate the owner into
the production message/state path with shared GUI/CLI access and coordinated FX
restore. No package or push.

### Ordered original Arp preset checkpoint — 12 November 2026

Accepted internal runtime/tests `b914984`. The message-owner prerequisite audit
found that original `applyGenericPreset`
performs eleven control edits, each followed by BPM. Native presets now compile
from `ArpSettings` into **one** bounded FIFO command that executes those 22 writes
in order, with one logical settings revision. Complete validation precedes live
automation mutation; full queues reject the whole gesture. The preset cannot
toggle A or replace its seed/storage. Persistent delay/STAY/LFO and editable chain
semantics remain unchanged. Product A is still disabled; this is a shared Engine
entry point for future GUI/CLI integration, not a visible preset/state feature.

Windows converter/preset/original-JS checks pass **4/4 (7.15s)**, retained
connected/storage/preparation/bank/original assets **5/5 (39.16s)**, and the final
boundary extension **1/1 (4.08s)**. Actual unchanged original preset/update/factory/
worklet methods in WebView2 pass **15** sound comparisons at 8/22.05/44.1/48/96 kHz,
64/128/37 partitions: maximum **6.70552e-8**, unchanged 1e-6 peak / 1e-5 relative
RMS limits. Engine stereo PCM/state is exact against eleven original-style
scheduled controls at 64/128, including 127 older dispatch entries, stale guards,
queue exhaustion, disabled presets and zero watched callback new/delete. The
frame-14 modulation regression proves repeated BPM is not a single final retarget.
Linux focused selection passes **9/9 (12.03s)** and its replay of the 15 WebView2
preset sound comparisons matches the same maximum error. ASan/UBSan/leaks passes
**3/3 (88.26s)** and TSAN **3/3 (142.01s)**, with no sanitizer diagnostics. Both
Standalones build. This accepts the internal ordered-preset slice, not product A.

The browser launcher initially splatted a scalar string as characters and ran
the older graph matrix (63 comparisons passed). It now uses a typed argument array
and rejects unknown JS options; the intended preset matrix was then executed.
No original assets or old tolerances changed. Full prior core evidence is reused:
only the affected Arp paths/suites are rerun for this internal addition.
Next: message-owned original settings with
exact per-track acknowledgement/rollback and ordered native state integration.
Keep original doubles/preset metadata authoritative; do not reconstruct them from
Float32 render targets or infer success from a newer unrelated global request ID.
Do not enable A before shared GUI/CLI, MIDI, state, BPM and new-instance voicing
acceptance. No package/push. See [ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md).

### Atomic Arp-only bank checkpoint — 12 November 2026

The internal Engine can now stage all twenty Arp render settings/graphs and
commit them together at one callback boundary. The existing worker prepares
replacements in the fixed forty-slot storage; late OOM/cancellation preserves
the complete old bank. Mixed enabled/disabled and all-off banks are supported.
One outstanding transaction includes its unread correlated terminal receipt and
excludes individual preparations. Epoch/revision/generation checks, DEL/panic,
armed CANCEL, newer controls and stopped prepare invalidate obsolete work.

Runtime `6654944`, extended tests `9355190`: Windows affected acceptance
**7/7 (39.25s)** passes, including exact old graph
and connected stereo Engine PCM after failure/cancellation, actual 64/128 blocks,
concurrent ownership, zero watched callback new/delete, retained preparation/
storage/settings/connected tests and unchanged-JS target/original-asset checks.
Linux **8/8 (322.99s)** includes the retained core regression (315.13s).
Retained unchanged-JS Arp sound/streaming pass **2/2** on each OS (31.13s Windows,
31.11s Linux). ASan/UBSan/leaks **3/3 (17.57s)** and TSAN **3/3 (27.52s)** pass.
The extended bank test adds partial frame-37 adoption, variable 0/1/7/63/64/65/
127/128/3 blocks at 32/48 kHz and direct all-old-stages-disabled panic. It passes
Windows **1/1 (18.65s)**, Linux **1/1 (2.36s)**, ASan/UBSan/leaks **1/1 (15.31s)**
and TSAN **1/1 (24.19s)**. The final test also preserves independent Q/C settings
and user chain reordering/omission through successful/failed Arp transactions:
Windows **1/1 (18.53s)**, Linux **1/1 (2.47s)**, ASan/UBSan/leaks **1/1 (15.51s)**
and TSAN **1/1 (24.50s)**. Both Standalones build. The broader Windows core
regression passes **1/1 (848.29s)**. This accepts the internal Arp-only bank slice,
not product Arp/session restore or device/latency/release acceptance.
The new fixture initially assumed immediate reclaimed capacity and ordinary
zero-block dispatch; it now observes explicit busy receipts and real positive
blocks. Product behavior was not changed to satisfy those assumptions.

This is **not** a session importer, full FX-bank transaction or product restore
command: Q/C/chain state and command ordering still need message-owner integration.
A stays disabled; no GUI/CLI/MIDI, defaults, state schema or preserved asset changes.
Next: message-owned original Arp settings
and ordered restore/preset commands using this transaction. No package/push.
See [ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md) for contract and commands.

### Arp control/preset compiler checkpoint — 12 November 2026

Accepted internal model `af07ce9`: original eleven double-precision controls, eight preset
names/values and twenty scale labels -> accepted raw render settings. Preserve
repetitions / 11.1 before Float32 conversion and the original per-edit BPM target.
The 2157-case unchanged-JS target/mask oracle and actual Engine preparation/edits
at 64/128 are the focused gates. Product A stays disabled; no GUI/CLI/MIDI/state
surface is being presented as complete. Original assets/IDs/session formats and
approved organic-voicing scope are unchanged. Windows final converter/oracle 2/2
(2.62s) plus the explicit signed-zero-input oracle pass; original assets passed in
the retained 3/3 selection. Linux 3/3 (3.77s), ASan/UBSan/leaks 1/1 (0.63s) and
TSAN 1/1 (0.95s) pass; both Standalones build. Tests cover all presets and 176
compiled edits at 64/128 with no watched callback new/delete. No existing render
math changed; previous audio/timing evidence is reused, not rerun as new proof.

Next: message-owned settings and atomic bank restore before GUI/CLI/MIDI/preset
workflow enablement; preserve original preset gesture order/BPM semantics and
investigate legacy out-of-domain state separately. This converter is not a state
importer, native UI feature or full Arp acceptance. No package/push.
See [ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md)
for the compact parity checklist, accepted-domain/legacy limits and next step.

### Arp exact-processing optimizations accepted — 12 November 2026

Connected checkpoint runtime `70c575b`, stronger live-state tests `731b9c3`:
Windows retained regression 4/4 (865.81s), Linux 8/8 (312.66s), connected audio
peak <=2.98023e-8 (limit 1e-6); ASan/UBSan/leaks and TSAN pass. Both Standalones
build. Internal settings/storage adoption and user-ordered routing are verified;
the product A control remains disabled pending GUI/CLI/MIDI/presets/state/BPM.

Runtime `2695768` now skips redundant recurrence only for exactly settled lanes
and submits worklet controls at their original latch boundary, including fresh
partial-quantum starts. Every gesture invalidates the settled flag. Keep the
failing-then-fixed negative-zero sign regression and new audible frame-37 reset
case. No sound/default, parameter/action ID, reference or preserved UI asset changes.

Windows affected tests 5/5 (20.62s) plus the extended stream executable; Linux
6/6 (4.55s), ASan/UBSan/leaks 5/5 (35.00s), TSAN 4/4 (45.65s). Both Standalones
build. Recorded original WebView2 modulation/factory fixtures pass on both OSes:
66 modulation comparisons (parameter error 0) and 63 factory audio comparisons
(peak 2.38419e-7, unchanged 1e-6 limit). These are fixture replays, not Linux
WebKit/device/UI validation. The final quantum change repeats factory acceptance;
modulation math is unchanged after its signed-zero-safe check.

Test-only follow-up `f9d9059` first renders the old fully-wet graph before reset,
so the frame-37 regression cannot pass merely by retaining prepare's fresh flag.
Windows extended stream executable passes; Linux 1/1 (1.49s) and ASan/UBSan/leaks
1/1 (15.13s) pass. Logs: `build/arp-quantum-final-restart*` on the relevant OS.
Runtime remains `2695768`; no further runtime edits after the recorded checks.

Final quiet twenty-loop Q/C/A, 48 kHz, 512 warm / 2048 measured callbacks:
Windows 64/128 mean 883.426/1745.88us, max 1180.6/2320.2us; WSL mean
807.894/1682.71us, max 1100.06/2264.38us. Zero observed overruns in these runs;
matching nonzero checksums. Preserve the earlier Windows 0/1 and WSL 4/17
overruns and intermediate results. Wall observations exclude devices/UI/roundtrip
and do not establish continuous reliability or release readiness.

Next: original Arp controls/preset conversion and shared GUI/CLI/MIDI/versioned
state, staged all-bank restore with OOM rollback, BPM semantics and approved
organic defaults for new instances. Keep A disabled until end-to-end acceptance.
No package/push. Windows remains publishing owner; WSL receives committed snapshots. See
[ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md) for scope and exact commands.

### Arp connected render-settings checkpoint — 10 November 2026

Internal typed settings now commit with prepared storage adoption and A renders
at its user-selected chain position. The product A control stays disabled: this
does not expose GUI/CLI/MIDI, original presets/state, organic defaults or BPM
integration. Windows/Linux connected sound/control/ownership, retained core and
sanitizer checks pass for their recorded scope; callback timing remains a follow-up
above. Both Standalones build. No preserved assets, existing IDs or product parameters changed.
See [ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md) for acceptance, lifecycle,
the old-active-graph receipt repair and outstanding product integration.

### Editable chain prerequisite — 10 November 2026

Accepted Q/C software-preview per-loop order/omission, shared GUI/CLI and version-1 native persistence
are implemented for ported Q/C. Original codes stay preserved; A remains disabled.
Windows connected audio/original-JS, production state/CLI and actual 64/128 WebView2
checks pass. Visible toggles follow the chain without rebuilding the loop card.
Retained Windows/Linux core/frontend, connected/reference, six processor/CLI
cases per OS, Windows main/LIVE UI at 64/128 and Linux sanitizers pass after the
documented fixture/adapter fixes. Runtime source **6d731ac**, Node 18/LIVE test
extension **963722b**. Isolated warmed twenty-loop EQ/C observations show no
64/128 deadline overruns; an earlier loaded Windows overrun is retained.
Owner approved live OD/SUB/MULT/scheduled-STOP chain edits on 10 November.
Regression source **097c1cd** adds eight 64/128 Engine cases: peak **3.57628e-7**
against independent persistent stage owners (limit 1e-6), exact captured WAVs,
unchanged transport/history/STOP deadlines and zero watched callback new/delete.
Focused Windows **1/1 (2.88s)**, Linux **1/1 (0.94s)**, ASan/UBSan/leaks **1/1
(18.45s)** and TSAN **1/1 (30.18s)** pass; actual Windows field edits in all four
states pass at both blocks. Product runtime is unchanged. This closes the Q/C
routing prerequisite, not full-chain/device/release acceptance. No package/push.
Next: Arp settings committed with graph adoption and A at its user-selected chain
position, then shared GUI/CLI/MIDI/state. Details and exact commands:
[SIGNAL_CHAIN_MIGRATION.md](SIGNAL_CHAIN_MIGRATION.md).
Do not connect Arp as a fixed Q -> C -> A path.

### Arp Delay — Engine preparation transactions, 10 November 2026

The next internal slice connects the verified storage worker to a bounded typed
Engine request/result protocol. **Product A stays disabled:** no audio-chain,
GUI/CLI/MIDI or persisted settings are enabled by this protocol. DSP math, original
assets, logo, product IDs, current defaults and preset/state versions are unchanged.

Acceptance defined before implementation: enqueue is not success; only actual
graph adoption produces a `ready` receipt with exact request/track/generation.
One outstanding transaction per track includes unread terminal receipts, bounding
both queues to twenty accepted transactions. Invalid message inputs reject before
mutation. Audio checks current clear epoch; cancellation revisions fence queued
and in-flight work before adoption. Failed/busy/cancelled requests preserve an
existing graph. DEL/DEL ALL/STOP ALL invalidate unfinished requests; stopped
prepare retains old terminal results while cancelling old unfinished requests.
Callbacks neither wait for preparation nor allocate/free. Ordinary loop request
acknowledgements remain separate. Zero-frame calls may service internal storage,
like export; they do not advance audio or drain ordinary loop commands.

Windows Engine tests pass at 64/128, including a worker paused inside
allocation while cancellation/DEL/panic and a replacement arrive. Extended
tests add forty-slot exhaustion/recovery, unread ready versus queued results
through repeated prepare, and actual message/audio/worker concurrency. Source
**eebbbed** passes the new Windows CTest **6.51s**, Linux Release **0.43s**,
ASan/UBSan/leaks **1/1 (1.01s)** and TSAN **1/1 (1.60s)** with the previously
documented process-only `setarch x86_64 -R` launch; no sanitizer diagnostics.
Storage, connected-compressor and original-assets pass on both OSes. Linux's
full affected selection passes **5/5 (221.21s)**, including core **218.58s**.
Windows's full affected selection passes **5/5 (751.16s)**, including core
**728.43s**. No source changes after that verified snapshot; subsequent edits
record evidence only. No package, push or user-facing Arp enablement.

Owner routing clarification, 10 November: the effect chain is user-editable per
loop. `QCATFODBVKZG` is the original default, not a fixed processing path. The
reference `Loop.setSignalChain` normalizes/deduplicates the entered chain;
`AudioGraph.buildEffectsChain` processes enabled effects in that order and omits
effects absent from the chain. At that checkpoint native rendering hardcoded
Q -> C and the markup supplied the default. The candidate above supersedes that
limitation; its remaining acceptance gates are recorded separately.

Next prerequisite: shared GUI/CLI and persisted per-loop chain order, with native
processing following that order for ported effects. Preserve original effect codes
and leave unported effects explicitly unavailable. Cover reordering, omission,
bypass, save/restore and persistent effect ownership before connecting Arp Delay.
Arp product settings must commit with graph adoption and A must run at its selected
chain position. Then complete its GUI/CLI/MIDI/versioned persistence and organic
new-instance defaults. Do not implement a fixed Q -> C -> A path.
This internal storage API does not establish cross-queue GUI gesture ordering or
provide a user CLI command. Preserve existing frontend barriers when connecting
it. No sound-parity or device/headroom claim is added; prior DSP and WSL timing
evidence remains below. Details: [ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md).

### Arp Delay — bounded track storage, 10 November 2026

`ArpDelayStorage` now uses the existing LoopBank worker to prepare/retire graphs.
Audio owns fixed slots and borrows active graphs; no new worker, product action,
parameter ID, state version, GUI/CLI/MIDI control or audio-chain routing is added.
**A remains disabled.** The verified DSP below is unchanged.

Acceptance: twenty active tracks plus twenty replacement slots; one pending
request per track; atomic rejection at capacity; worker-only allocation/free;
exact active-owner preservation after OOM; cancellation/DEL/reset cannot adopt
obsolete work; stopped rate changes and shutdown join before releasing storage.
The new 64/128 tests compare exact stereo PCM/state against directly prepared
graphs, including after OOM and at 96k. They pause worker allocation to exercise
cancel/DEL plus a newer request, saturate/recover all forty slots, and exercise
real LoopBank worker/epoch/clear/reset/prepare/destruction and concurrent handoff.
An additional shared-worker test passes Windows: twenty Arp preparations and
replacements coexist with capture/finalization, NORM and pinned WAV export;
stored WAV and nonzero connected loop PCM exactly match a bank without Arp jobs.
Watched audio operations allocate/free nothing. Runtime ownership source is
**cbf9ab0**, extended shared-worker regression **ee3edcb**. Windows affected
selection passes **5/5 (706.95s)**, including full core **693.56s**; the extended
shared-worker storage binary also passes. Linux selection passes **5/5 (240.37s)**. The
extended storage test passes Release **1/1 (0.79s)**, ASan/UBSan/leaks **1/1
(2.89s)** and TSAN **1/1 (4.22s)**, no diagnostics. TSAN initially passed the
earlier cases, then its extended run failed before main with an unexpected memory
mapping. Preserve that failed log; the extended test passes with a process-only
`setarch x86_64 -R` launch. No system ASLR setting or compiler was changed.

Next: shared Engine command acceptance/outcomes and chain placement, then
GUI/CLI/MIDI/versioned state and organic new-instance defaults. Keep A disabled
until connected sound/state/interaction and full-bank gates pass. The recorded
Linux/WSL scheduling limit below remains open; storage tests are not device or
complete Engine headroom evidence. See [ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md).

### Arp Delay — combined streaming DSP slice, 10 November 2026

`ArpDelayGraph` now combines worklet, 50ms automation and stereo auto-pan;
**product A remains disabled** until track/command integration. Runtime source
**3f35d82** is verified in Windows and native WSL; later commits add timing and
sample-rate regressions only. EQ/compressor, assets/logo, IDs and dependencies
are unchanged. Full evidence/commands/history: [ARP_DELAY_MIGRATION.md](ARP_DELAY_MIGRATION.md).

Owner decisions: persistent per-loop delay/STAY/LFO across PLAY/RTRG/history;
live OD/SUB/MULT/STOP controls; **zero added latency with causal wake-up**;
organic defaults for **new instances**, preserving existing presets/saved sounds.
The organic voicing is still unimplemented. While tail <1e-5 the kernel waits
for the first left sample >=1e-5 and processes it immediately, freezing the prior
prefix. This approved difference from original lookahead is not sound parity.
Detector decay/rotation and worklet parameter latching retain a canonical 128
audio-sample clock independently of callback partitions; LFO/automation continue.

Exact native PCM/state, causal wake, mono/stereo/no input, in-place, 1/37/64/128/
257 partitions, 8..384k rates, OOM/reset/overflow and zero watched processing/
control/reset new/delete pass. Original 84-case kernel comparisons remain;
full original factory/worklet/automation/pan passes **21 actual Windows browser
fixtures / 63 comparisons**, peak **2.384185791015625e-7**. All replay against
Linux C++ at the same peak. Existing **66** pan/parameter comparisons also pass,
with **zero parameter-sample error** after repairing Float32 event anchors and
deferred lanes. Nominal clipping protects valid endpoint moves. Limits unchanged.

Windows kernel/stream/JS CTests **3/3 (31.80s)** and original assets pass. Linux
core/stream/JS/assets **4/4 (28.69s)**; ASan/UBSan/leaks **2/2 (20.71s)**, no
diagnostics. Linux WebKit, actual UI/device interaction and listening are untested
for A. The current Standalone still has its existing enabled features only.
Final rate-reuse test **d940280** also passes Windows, Linux **1/1 (1.62s)** and
ASan/UBSan/leaks **1/1 (18.26s)**; no runtime changes after the verified snapshot.

Coefficient caching repairs the measured single-sample dispatch CPU overhead.
Twenty combined graphs show no observed Windows overruns in the optimized run.
Linux normal-priority WSL wall timing still has isolated misses: a CPU diagnostic
records one **2220.3us wall / 594.4us thread CPU** miss at 64; thread CPU remains
below the period in every measured callback. This establishes off-CPU delay for
that observed miss, not reliable device scheduling or complete Engine headroom.
Retain both failed timing logs; do not rerun until a clean result or claim release
performance. No Engine/EQ/C mix, physical latency, hardware or wrapper claim.

The subsequent storage slice is above. Shared Engine/GUI/CLI/MIDI/versioned state
and organic defaults remain pending. Keep A disabled until end-to-end and
full-bank acceptance passes.

### Connected compressor — scoped software gates pass, finalized 10 November 2026

Owner approved a persistent compressor per loop across playback restarts and live
controls during OD/SUB/MULT/scheduled STOP. Those are intentional extensions of
the original per-graph recreation/frozen-running-controls behavior; retained
unchanged-JS tests document the difference. The C loop checkbox now reaches the
shared native processor through original-style controls, CLI `comp`, seven CC
Learn targets and versioned state. Original assets and existing IDs are unchanged.

Windows development Standalone, mouse host and production processor targets build.
Connected 64/128 PCM passes (peak <=1.78814e-7); all seven CC targets match timed
GUI controls exactly, including same-block learn, all 128 values/channels and
selected tracks 1/20. Persistent EQ -> compressor through RTRG/reverse/OD/history/
phase and ordinary STOP/PLAY, dry WAV invariance, scheduled STOP, panic/DEL, mask/epoch rejection,
combined-bank queue admission and callback new/delete checks pass. Production
processor/CLI/state checks pass at 64/128. Actual WebView2 compressor AND existing
EQ pointer/key/reload/MIDI/curve tests pass at 64/128. Focused CTests: 9/9 (18.92s).
All 60 original browser kernel fixtures still pass, peak 4.806921385380747e-7.
A broad MIDI processor run timed out waiting for one CLI reply; unchanged isolated
retry passed. No assertion/time limit was weakened. The failed test's owned
mailbox directory was removed; see ignored build logs for both runs.

Linux Standalone and production processor targets build. Focused CTests pass
**9/9 (15.63s)**; compressor/EQ processor/CLI and broad MIDI regressions pass at
64/128. Final strengthened connected test passes **1/1 (1.10s)** and both final
compressor processor invocations pass on **dc2fb4d**. Linux ASan/UBSan/leaks pass
**2/2 (24.56s)** and the expanded STOP/PLAY **1/1 (21.18s)**, no diagnostics.
Both platforms replay the 60 original Windows WebView2 PCM fixtures with the
same peak above; retained capture/OD/SUB audit passes **126 cases / 965,826
samples**, peak **7.394757495227111e-8** on each. This is not Linux WebKit UI/audio.

Warmed Release Engine measurements: twenty playing stereo loops with EQ+C,
512 warm / 2048 measured callbacks per steady/automated configuration, nonzero
post-MASTER output. Worst observed Windows **809.5/1766.1us**, Linux
**670.427/1028.92us** at 64/128; **0 observed overruns** in each run against
1333.33/2666.67us periods. Includes core processing/bounded telemetry; excludes
device callback, driver/UI/round-trip latency and any guaranteed worst case.
Initial insufficient-pool and silent-MASTER timing fixtures were rejected and
repaired; both conditions now fail explicitly rather than yielding accepted data.

Code **dc2fb4d** is committed and verified in native WSL storage. No package,
push, dependency upgrade or wrapper build. Linux WebKit interaction, physical
expression, listening/driver latency, full overlapping-browser transition
envelopes and long-session/release checks remain separate. Wet SAVE, full chain/
preset persistence and later effects remain disabled. Next: inventory original
**A (Arp Delay)** behavior and define its smallest complete shared GUI/CLI slice.
See [COMPRESSOR_MIGRATION.md](COMPRESSOR_MIGRATION.md) for exact scope/commands.

### Earlier compressor checkpoint — isolated DSP, 9 November 2026

At that earlier checkpoint, **C remained disabled** pending shared GUI/CLI/MIDI/state and
connected audio/lifecycle acceptance. Its new fixed-storage native kernel now
passes actual unchanged-original WebView2 sound comparisons: **60 cases,
1,966,080 stereo frames at 64/128**, final Windows/Linux replay peak
**4.806921385380747e-7** against the
unchanged **1e-6 peak / 1e-5 relative RMS** gates. Eleven original calculated
presets, lookahead/immediate dry routing, mono/stereo, static/extreme controls
and live retargets at four rates are included. Failing k-rate anchor, independent
dry/wet smoothing and one-sample timestamp-boundary cases were fixed, retained,
and passed without threshold changes. Faust was evaluated as owner-authorized;
this slice uses pinned BSD browser DSP/automation source for parity, adding no
Faust/Chromium runtime dependency. Existing original UI/reference assets,
sessions/IDs and connected EQ are unchanged. Details, exact commands and remaining
acceptance: [COMPRESSOR_MIGRATION.md](COMPRESSOR_MIGRATION.md).

On code **3c0769e**, final compressor CTests pass Windows **2/2 (.37s)** and
Linux **2/2 (.62s)**; Linux ASan/UBSan/leaks **1/1 (1.76s)**, no diagnostics.
The retained EQ kernel/automation and original-asset gates pass in the earlier
six-test groups: Windows **6/6 (43.08s)**, Linux **6/6 (44.45s)**. Those EQ/assets
were unchanged by the subsequent compressor-only math repairs. Both platforms
pass all 60 original browser PCM cases with identical final peak error. The
Linux replay is Windows WebView2 PCM versus Linux C++, not Linux WebKit UI/audio.
Twenty-stereo-kernel warmed measurements show **0/2048** observed overruns in
each steady/automated 64/128 run; worst Windows **359.9/769.7us**, Linux
**370.029/719.089us**, deadlines **1333.33/2666.67us**. This excludes production
routing, EQ, callback/device/round-trip latency and is not a worst-case guarantee.
The Windows build and WSL mirror are committed independently of publishing;
source snapshot **3c0769e** is preserved in the clean native Linux mirror.
See the compressor notes for failures/repairs, exact logs/commands and full timing.
Next is the connected C feature, before any later effect. No
new distributable or push is part of this checkpoint; the EQ preview below
remains the current product package.

Owner scope confirmed: **core looping/editing first**, then FX/SAVE/presets.
[LOOP_TRACK_BUTTONS.md](LOOP_TRACK_BUTTONS.md) records the inventory and remaining
eligibility gaps. Remaining unported FX/SAVE controls stay disabled.
Owner-requested Windows development package and GitHub push completed at
**0e1780a**: `dist/Aion-Looper-0.4.3-0e1780a57bad-win64-Standalone.zip` (6,612,161
bytes; SHA256 `0AB31C042F3CAA21AD58A4EDA18FE39E1F8FA059A94A4482FAE9A91B4A5E7BFA`).
Packaged EXE matches the built SHA256; CLI help works with the configured Python.
The ZIP now includes optional CLI, project/dependency notices, current EQ scope
and exact source archive. An initial Git archive argument-splitting failure was
fixed and packaging rerun. This preview predates the EQ MIDI increment below;
it is not a claim of release readiness or completed original-asset permissions.
**Current refreshed preview includes EQ MIDI Learn:**
`dist/Aion-Looper-0.4.3-055e76910e5a-win64-Standalone.zip`, **6,659,268 bytes**,
SHA256 `4D92F459A47130268936822BDDFC3AAF01F89FF02C20D39E9CC8E893AA807A00`.
Built source snapshot **055e769**; packaged EXE SHA256
`AC4C87B8271E6ED94AA3E109434AA9C016D0125964744107DD3B44E4F5A51BFB`
matches the build byte-for-byte. Required contents, source ZIP commit comment and
extracted CLI help all pass (`build/eq-midi-package-verified.json`). No installer,
Linux package, wrappers or version bump. Python is optional for CLI only. This is
a development preview; physical/Linux WebKit/listening and rights/release checks
remain as documented. Subsequent handoff-only commits do not change this binary.

### In progress: original Q/eQ DSP and graphic editing — 9 November 2026

**Selected-tab EQ MIDI Learn passes its Windows/Linux software scope.**
All 24 original effect-slider CC ranges now route to the selected EQ loop tab,
using shared GUI/CLI state, append-only targets 93..116 and version-4 MIDI state
(legacy versions 1/2/3 preserved). The new unchanged-JS gate covers **51,072** values;
64/128 Engine PCM, production MidiBuffer/external CLI and actual Windows pointers/
keys prove immediate learn bursts, tab isolation, conflicts, Escape/clear and
actual focused slider updates. See [EQ_MIGRATION.md](EQ_MIGRATION.md) for exact
scope, test-harness repairs and commands. Windows **6/6 (16.47s)** and Linux
**6/6 (13.72s)** CTests pass on production snapshot **6766d40**, as do production
EQ/external CLI at both block sizes, retained core/processor MIDI regressions
and Linux ASan/UBSan/leaks **1/1 (11.35s)**. CC-vs-GUI stereo peak error is **0**,
limit **1e-6**. New actual Windows document-replacement tests retain the selected
tab/mapping and cancel only temporary learning. The final parameter oracle uses
exact Float32 equality; final script **392750e** passes Windows **1/1 (.79s)**
and Linux **1/1 (2.10s)**. Dense 256-CC/20-stereo-loop Engine measurements show
zero observed overruns: worst Windows 64/128 **508.2/1068.2us**, Linux
**978.429/1493.24us**, deadlines **1333.33/2666.67us**. Device/wrapper/round-trip
latency and physical expression/Linux WebKit/listening remain separate checks.
Next: original compressor (C) reference/audio/state/GUI/CLI acceptance, before
the remaining FX and wet SAVE. No further effect is enabled in this increment. The earlier
connected baseline and persistent/live-control acceptance are retained below.
Per-loop eQ now uses the existing native processor from original track/live eQ
checkboxes, FX/eQ navigation, original numeric controls/presets and running-app
`aion.py eq` commands. The approved graph adds ten selectable draggable points,
six horizontal bandwidth ranges and fine/axis/keyboard editing. Other effects,
chain/full presets and wet SAVE remain disabled. Preserved UI/reference assets,
logo, APVTS IDs and existing action IDs are unchanged (EQ action appended).

The owner selected persistent per-track EQ on 9 November: source edits/history/
restarts retain filter state. Clear, panic and reconfiguration reset runtime
filters; DEL resets settings. Original 50ms target smoothing is retained; actual
browser measurement found the nominal toggle fade cancels itself. The extra
prototype native dip was removed; settled toggle gain matches the original.
Normal reference retriggers already reuse their graph;
fallback reconstruction/overlap transition identity is not claimed. Version-1
`LoopEqualizers` state preserves older presets (defaults/off) and rejects malformed
EQ before APVTS/MIDI mutation. Host-thread restores publish only through the
message producer, with atomic bounded batch admission and clear-epoch guards.

Windows: 8/8 focused CTests PASS (48.06s); original kernel/automation/asset/frontend/
bridge gates retained. New connected unchanged-worklet comparison: **16,384 stereo
frames at 64/128**, XY/preset smoothing/global render quantum/actual toggle gain,
final peak **7.40154114e-9**, limit **1e-6**. Record/OD/SUB dry WAV bytes are exact with
EQ enabled/bypassed; queue rejection, panic/DEL, full-bank restore and callback
new/delete checks pass. Production processor + **external CLI** pass independently
at 64/128, covering strict/atomic validation, pending intent serialization, legacy
and host-thread restore, full queue, MIDI panic cancellation, expanded track 20,
and same/changed sample-rate lifecycle. Actual WebView2 pointer/keyboard/graph/
bandwidth/reload/reset/DEL tests pass at 64/128, with nonzero native PCM and no
runtime exceptions. Full retained core realtime regression executable passed;
subsequent EQ-only repairs have focused connected/lifecycle regression coverage.

Warmed **actual Engine** (20 playing stereo loops, all ten EQ bands, 512 warm +
2048 measured calls/scenario) at 48kHz: final worst Windows 64/128 **447/734.6us**,
Linux **513.378/826.279us**,
deadlines **1333.33/2666.67us**, no observed overruns. This includes native mixing/
EQ/telemetry, not JUCE wrapper/device or round-trip latency; no worst-case promise.
See [EQ_MIGRATION.md](EQ_MIGRATION.md) for commands, repaired test-harness failures,
scope and remaining Linux/physical/listening/release checks. No package or push.

Linux 9/9 focused tests (54.20s), production EQ/CLI and retained production MIDI
regressions at 64/128, loop-edits/panic ownership and ASan/UBSan/leaks 2/2 (3.47s)
pass. Final toggle/CANCEL/bandwidth-CLI changes additionally pass **3/3 CTests
(1.12s)**, production/external CLI at both block sizes and **1/1 ASan/UBSan/leak
connected test (2.03s)**. Actual Windows 24-case/80,048-frame unchanged rebuild/scheduledFade
oracle proves the flat settled gain; the original prototype failed at 2.23e-6
against the unchanged 1e-6 limit. Windows final connected/bypass, processor and
actual UI 64/128 pass, including send-failure recovery, focused CLI changes and
double-click reset. Next parity work remains EQ MIDI Learn (original selected-tab
effect CC workflow), plus Linux WebKit/listening and release checks; do not move
to another effect under a claim of complete EQ controller parity.

Kernel evidence remains in [EQ_MIGRATION.md](EQ_MIGRATION.md): 481 unchanged-
worklet cases / 3,034,408 stereo frames (peak 4.99755863e-8), 15 target-automation
cases / 184,335 frames (4.99999986e-10), 48 actual-browser comparisons / 270,336
mono/stereo frames (8.57054142e-8), plus half-gain/range/overflow and zero tracked
new/delete tests. The owner-approved Nyquist repair preserves stored settings;
raw original low-rate failures remain separate regressions. Historical prototype
and isolated-kernel timings, browser retarget repair, all exact commands and
test-change rationale remain there. Retain prior core-loop evidence below.

Latest lifecycle repair: original armed CANCEL preserves EQ configuration.
Native finish/stop/panic cancellation now share that rule; explicit DEL resets
it. Tests inspect actual audio-owned EQ fields, not only the UI model. All 22
unchanged original stopRecording cases, Windows production/external CLI and real
64/128 main CANCEL pointers pass. Focused bandwidth CLI (`eq ... width`) uses the
same native inverse as graph handles. Final Linux core/processor/CLI/sanitizer
checks for this repair pass on committed snapshot **b0ce92d**.

**Owner-approved live controls (9 November):** EQ edits, presets and toggles respond
during OD/SUB/MULT and scheduled STOP. This intentionally improves the original
EffectManager.update/triggerRebuild limitation; it is not unchanged-JS parity in
those states. The implementation already supported this behavior; the new focused
acceptance adds eight real Engine scenarios (four states at 64/128), independent
filtered-PCM comparison at 1e-6, exact bypass/dry WAV, raw capture/history/source
invariants and unchanged STOP deadline/playback through its scheduled sample.
Windows focused CTests **2/2 PASS (6.15s)**; committed snapshot **1f80046** passes
Linux **2/2 (1.82s)** and ASan/UBSan/leaks **1/1 (10.66s)**. Eight new PCM cases peak
at **7.4505806e-9**, below the unchanged **1e-6** limit; tracked callback new/delete
remains zero. See EQ_MIGRATION.md for commands/results. This earlier tests/docs
increment preceded EQ MIDI Learn above. Linux WebKit/listening/device/release
checks remain separate; it changed no product code, UI assets, IDs or dependencies.

### Implemented preview: queued PLAY with STOP pending Undo/Redo — 9 November 2026

Acceptance: unchanged `Loop.undo/redo` restores stored PCM/history
without restarting the constructed future graph or changing its original start,
STOP or older Apply. Preserve pre-grid silence/visual zero and post-grid source
PCM/progress through STOP, forward/reverse and same/following-grid STOP. Local,
global, GUI and CLI share native restore; cover cancellation/prepare, full-pool
growth rejection, rapid history, NORM and 64/128 with no callback new/delete.
The old guards rejected this valid state; accepted RTRG behavior remains intact.

Initial reproduction `build/queued-stop-history-repro-windows.log`: native Undo
rejected the first unchanged-JS same-grid case. Native restore now treats a
constructed pending-STOP queue as a stopping graph, freezing its source and
retaining its visual clock for activation. Shared GUI/local/global controls admit
the state; no IDs/schema/assets/dependencies change. Implementation **2e74ff5**;
expanded production comparison **48d53e5**. Original-JS **120 cases**, **432 exact
stereo WAVs**, **5,148,336 frames per OS**, peak **6.37177635e-8** on both OSes,
unchanged **1e-6** PCM and **1e-12** progress limits PASS.

Actual Engine **152 cases per OS PASS**: tracks 0/19, forward/reverse, local/global,
same/following-grid STOP, older Apply, unchanged source stereo through both grids,
depth/length, cancellation/same/new-rate prepare and atomic full-pool growth
rejection before queued start. Zero callback new/delete. The retained **88**
stopping, **64** queued and **124** post-birth history cases PASS per OS. Their
existing assertions remain; the shared setup only adds the queued-state variant.
The prior RTRG exclusion for queued-STOP history is now removed: **258** integrated
RTRG Engine cases PASS per OS (eight more than the accepted checkpoint below).

Windows original pointers **4 combinations per block size, 64/128 PASS**: local/
global, forward/reverse, Undo/Redo/Undo before the original queue/STOP, exact
stored identities/depth/length, no premature source/progress/audio, natural STOP
and next nonzero PLAY using restored storage. Frontend eligibility/FIFO PASS.
Both Standalone builds and full production processor/external CLI suites PASS.
Eight new shared GUI/CLI **production RPC** cases per OS cover local/global
restore, same-grid silence and **nonzero original source PCM between start and
following-grid STOP**, retained deadlines and no processBlock new/delete. These
are shared production requests; the retained external Python/mailbox suite also
passes, not a claim of a newly tested physical controller or separate CLI engine.
Linux production ran alone after heavy checks.

Windows **11/11 retained CTests PASS, 559.79s**; Linux **12/12 PASS, 1083.02s**.
The full retained queued-history source corpus contributes **960 cases per OS**,
**23,625,040 frames**, **2,784 source snapshots**, peak **8.31378578e-8** under the
unchanged **1e-6** limit. It covers healthy queued history, rapid edits, Apply/
RTRG/NORM and cleanup. Its long runtime is integration evidence to reuse for
unchanged paths, not a reason to repeat all 960 cases on each UI/doc edit.
Connected capture/OD/SUB audit: **126 cases**, **965,826 frames per OS**, peak
**7.39475750e-8**, unchanged **1e-6**, PASS. Linux Debug ASan/UBSan/leaks **152**
queued STOP plus **88** stopping-history cases PASS, no diagnostics. No case was
deleted or tolerance relaxed. Source-oracle pre-grid visual suppression reuses
the already accepted queued-MULT rule and still checks exact post-grid progress.

After acceptance, identical ignored Release Engine timing fixtures ran Windows
then Linux without concurrent tests/builds: 48kHz stereo, 512-frame initial take,
overdub/local MULT history, 512 warm callbacks, **32 scenarios per OS**, local/
global Undo/Redo/Undo, reverse, same/following-grid STOP, pending Apply and clear.
Maximum observed 64/128 callbacks: Windows **116.3/162.1us**, Linux **192.4/158.5us**,
deadlines **1333.33/2666.67us**. Scoped Engine measurements, not full processBlock,
round-trip latency, worst-case/full-bank guarantees, OS rankings or speedups.
Harness `build/queued-stop-history-bench-src/bench.cpp`, SHA256
`760449339da4bd60b2b29849ac9df284f9c8653c4ead99d774366cefabbdf897`;
independent `build/queued-stop-history-bench-{windows,linux}` caches import each
OS's current Release core. Logs use `build/queued-stop-history-*` per checkout;
final production logs include `production-final-OS`, source metrics `native-OS`
(Windows `native-final-windows`), UI `ui-windows`, sanitizers `sanitizer-SELECTOR-linux`.

Focused reproduction, using existing caches and committed safe WSL sync:

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --queued-stop-history
./build/windows-core/Release/aion_core_tests.exe --pending-retrigger
node tests/QueuedMergeSources.cjs --queued-stop-history --native build/windows-core/Release/aion_start_probe.exe
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
cmake --build --preset windows-dev --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./tools/TestMouseWorkflow.ps1 -QueuedStopHistoryOnly
./build/windows/Release/aion_midi_processor_tests.exe
```

```sh
# Commit in Windows, sync the clean mirror; keep separate OS caches.
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --queued-stop-history
./build/linux/aion_core_tests --pending-retrigger
node tests/QueuedMergeSources.cjs --queued-stop-history --native build/linux/aion_start_probe
node tests/AudioParityAudit.cjs build/linux/aion_reference_probe
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --queued-stop-history
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --stopping-history
# Run alone after heavy validation.
./build/linux/aion_midi_processor_tests
```

Next: the original **Q/eQ** effect, first in `QCATFODBVKZG`, as an end-to-end
native FX slice with unchanged controls, chain/preset-compatible state and shared
GUI/CLI commands. Inspect `_createEQ`/its worklet, parameters and original sound
before implementation; no disconnected FX/SAVE/preset controls should be enabled.
Current source models exclude browser envelopes/resampling and advancing-worker
latency. Linux WebKit interaction, devices/physical latency, full-bank/long-session,
deferred wrappers/DAWs and Apple/release checks remain separate. No package/push.

### Implemented preview: pending RTRG and integrated core-button combinations — 9 November 2026

Acceptance: original `Loop.retrigger/play/_teardown` starts an unborn queued
PLAY or stopping graph immediately, cancels its old queue/STOP/Apply/STUT and
retains stored PCM/history. Healthy playing/constructed queued RTRG keeps its
existing 5ms source replacement. Exercise MULT, NORM, Undo/Redo, MERGE, reverse,
rate and rapid/deferred RTRG, obsolete callbacks, cancellation and prepare at
64/128. Capture/finalizing/empty states remain guarded in the processor; the
existing GUI command FIFO waits through finalization. GUI/CLI share this action.

Implementation **bcb65c8**, direct CLI/shortcut coverage **531386c**. Native
retires old source ownership before publishing a fresh
immediate graph receipt. The original RTRG button now admits those states. IDs,
schema, reference/preserved assets/logo and dependencies are unchanged. Initial
unborn rejection: `build/pending-rtrg-repro-windows.log`.

Original JS **240 cases**, **480 exact stereo WAVs** and **10,829,592 frames per
OS**, PASS under the unchanged **1e-6** PCM limit; Windows measured peak error
**8.29155743e-8**. Start metadata <=**1e-12**. Actual Engine **250 cases per OS**:
independent stopped-source
baseline compares every stereo sample from the first fresh callback (including
fades), tracks 0/19, reverse/rate/edit combinations, late callbacks, cancellation,
same/new-rate prepare and zero callback new/delete. Retained early-restart checks
PASS (64 source cases, 64 cancellation/FIFO cases, 32 silent-tail cases).
Frontend admission **88 cases PASS**. Windows retained CTest **19/19 PASS,
438.09s**, plus original STUT PASS; Linux **21/21 PASS, 1051.54s**, including the
new corpus and frontend, plus original STUT PASS. Prior queued/stopping MULT,
NORM, history and MERGE source modes pass with the updated oracle renderer.
Connected capture/OD/SUB audit **126 cases**, **965,826 frames per OS**, peak
**7.39475750e-8**, unchanged **1e-6** limit. Windows also ran `--loop-edits`,
`--meter-queued` and `--meter-overlap`; both OSes retain **132** queued STOP MULT
and **88** stopping-history Engine checks. No failing case/tolerance removed.

Actual Windows UI **10 scenarios per block size**, PASS **64/128**: unborn queue,
constructed queued STOP, sounding STOP, MULT/NORM/history/STUT/reverse-speed/MERGE
then RTRG, rapid healthy taps and `/` during a paused real worker finalization.
Old queue/STOP/cleanup deadlines pass without source revival or silencing the
new nonzero playback; stored clip/history remain intact. The first fixture used
the wrong `]` key and failed the deferred case. Corrected only test input to the
unchanged original `/`; reran all 20 scenarios, without product/tolerance changes.

Both Standalone builds and full production processor/external CLI suites PASS.
New actual external `loop retrigger 1` cases cover unborn queue and scheduled
STOP at 64/128, immediate PLAY, no old deadline and unchanged stored identity/
length/history. Linux production/CLI ran alone after heavy checks. Linux Debug
ASan/UBSan/leaks: new **250** Engine cases and retained **160** early-restart cases
PASS, no diagnostics. Scope is synthetic PCM and production message-loop/CLI,
not physical devices or driver latency. Logs use `build/pending-rtrg-*` in each
checkout; final UI/CLI logs include `ui-final-windows` and
`production-cli-final-{windows,linux}`.

The early-restart regression's former unborn/stopping RTRG rejection expectations
were implementation-coupled to this defect. They now assert immediate PLAY and
survival beyond the obsolete grid/STOP; Apply guards, cancellation, capacity and
PCM checks remain. Stronger replacement evidence is the unchanged original-method
corpus plus the independent first-sample Engine comparison. The source oracle
now honors actual source/volume disconnection at original teardown; its prior
modes are rerun to protect the shared renderer. It excludes browser gain/fade
envelopes/resampling and advancing worker latency.

After acceptance, identical ignored Release Engine timing fixtures ran Windows
then Linux with no concurrent tests/builds: 48kHz stereo, 512-frame clip, 512 warm
playing callbacks, **20 scenarios per OS** (unborn/queued STOP/sounding STOP,
reverse and pending Apply), fresh/healthy RTRG, playback beyond the old deadline
and clear. Maximum observed callbacks at 64/128: Windows **90.6/137.3us**, Linux
**108.981/146.55us**, against **1333.33/2666.67us** deadlines. These are scoped
Engine samples, not full processBlock/round-trip latency, a worst-case/full-bank
guarantee, OS ranking or speedup. Harness `build/pending-rtrg-bench-src/bench.cpp`
SHA256 `a820e4c13c02252cbe3c28372069964c6aba919658d7e240d0d1155d44fdf15e`;
separate `build/pending-rtrg-bench-{windows,linux}` caches import each OS's current
Release core; logs `build/pending-rtrg-bench-OS.log`.

Focused reproduction commands (existing caches; CMake/CTest resolution and safe
committed WSL sync in DEVELOPMENT_WORKFLOW.md; retained selections in CTest logs):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_dsp_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --pending-retrigger
./build/windows-core/Release/aion_core_tests.exe --early-restart
node tests/QueuedMergeSources.cjs --pending-rtrg --native build/windows-core/Release/aion_start_probe.exe
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
cmake --build --preset windows-dev --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./tools/TestMouseWorkflow.ps1 -PendingRetriggerOnly
./build/windows/Release/aion_midi_processor_tests.exe
```

```sh
# Commit in Windows and sync the clean mirror first; do not copy build caches.
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_dsp_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --pending-retrigger
./build/linux/aion_core_tests --early-restart
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^js_native_pending_retrigger$'
node tests/AudioParityAudit.cjs build/linux/aion_reference_probe
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --pending-retrigger
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --early-restart
# Run alone after the heavy checks.
./build/linux/aion_midi_processor_tests
```

At this checkpoint queued PLAY with STOP pending still guarded local/global
Undo/Redo; the newer entry above now covers that eligibility. Broader Linux
WebKit interaction, physical devices/drivers/latency, long-session/full-bank,
deferred wrappers/DAWs and Apple/release gates remain separate. No package/push.

### Implemented preview: MULT on queued PLAY with STOP pending — 7 November 2026

Small slice: a constructed graph can await its future start with a same-grid or
following-grid STOP already scheduled. Unchanged `Loop.multiply/multiplyInstant`
doubles stored PCM/local history even with input available; its existing source,
start/STOP, old buffer/rate and older Start event remain independent. No capture,
early audio or worker-result revival. New PLAY uses doubled storage. Preserve
NORM/history ownership, pre-start visual zero and post-start original progress;
failure/capacity rejection, DEL/panic and prepare must be safe at 64/128.

Implementation **d744339**. Native now admits only a constructed pending-stop queued source, freezes its
immutable source before storage replacement and retains the visual clock through
original grid activation. Shared MULT GUI/CLI dispatch enables this state; public
IDs/schema, preserved UI/reference/logo and dependencies are unchanged. Other
queued STOP actions (RTRG/history) retained their guards at that checkpoint;
the newer RTRG entry above supersedes that part of the historical status.

Initial rejection `build/queued-stop-multiply-repro-windows.log`. The first
implementation failed unchanged queued-state/paired-PCM assertions by making
the source audible early during finalization. The corrected path distinguishes
retained source ownership from an already sounding source; no tolerance relaxed.
New original-JS **96 PASS per OS**, **312 exact stereo WAVs** and **4,012,392 frames per OS**,
max **6.26096477e-8** (limit **1e-6**), progress <=**1e-12**. New actual Engine
**132 PASS**, tracks 0/19, forward/reverse, same/next-grid STOP, pending Apply,
worker completion before start/between grids/after STOP, OOM/cancellation/prepare/
full pool, exact stereo versus an unedited source and zero callback new/delete.
Frontend admission PASS. Actual Windows queued-STOP-MULT pointers PASS 64/128:
explicit production STOP RPC (same path as CLI), original MULT click, retained
start/STOP and source revision, silence, exact Undo/Redo identities and next
audible PLAY. Retained stopping-MULT pointer flow also PASS 64/128. Its first
attempt correctly refused the prior WebView's still-closing CDP port; unchanged
rerun passed after release (`-retained-ui-final-` log), without bypassing the guard.
Windows **14/14 retained CTests PASS, 297.72s**, Standalone and full production
processor/external CLI suite PASS. Connected capture/OD/SUB audit **126 PASS**,
**965,826 frames**, max **7.39475750e-8**, unchanged **1e-6**.

New Engine **132** and retained **100** stopping MULT, **288** post-birth MULT,
**108** pre-birth MULT, **212** queued-input MULT, **8** gated-input MULT, **108**
queued NORM and **88** stopping history PASS on each OS. Linux **15/15 CTests PASS,
476.81s**, including the new source corpus; connected capture/OD/SUB audit matches
Windows count/error. Linux Debug ASan/UBSan/leaks: **132** new queued STOP MULT,
**100** stopping MULT and **108** queued NORM PASS, no diagnostics. Linux
Standalone and isolated full production processor/external CLI suite PASS.
Acceptance finished on 7 November; the usage-limit interruption prevented only
the final notes/commit. Results were recovered from retained logs on 9 November;
unchanged passing audio suites were not repeated for this documentation recovery.

Source model uses unchanged original methods and node IO stubs/raw PCM. Browser
gain/STOP envelopes, resampling and advancing-worker sound remain excluded;
actual Engine delayed-worker ownership is checked separately. Broader future-
source visual suppression remains outside this comparison; this fixture covers
zero before the original start and progress after the latest source starts.
Windows pointer checks use the synthetic-PCM production processor/editor host.
Linux WebKit interaction, physical drivers/devices/latency, full-bank/long-session
validation, deferred wrappers/DAWs and Apple remain separate. Earlier unexplained
missing-button/CLI-timeout observations remain recorded below. No package/push.

After acceptance, identical ignored Engine timing fixtures ran Windows then Linux:
48kHz stereo, 512-frame capture, 512 warmed playing callbacks, explicit queued
source, same/next-grid STOP, input/no-input, forward/reverse, with/without Apply;
**32 scenarios per OS**, 64/128. Timed online MULT admission/worker completion,
playback through start/STOP, stopped Undo/Redo and clear. Setup/offline waits,
input generation, telemetry consumption and 1ms sleeps between worker polls are
excluded. Maxima per 64/128 callback: Windows **124.2us /286.0us**, Linux
**128.14us /185.05us**, deadlines **1333.33us /2666.67us**. These small Engine
observations do not establish full processBlock/round-trip latency, full-bank/
worst-case/long-session guarantees, a platform ranking or speedup.
Harness `build/queued-stop-multiply-bench-src/{CMakeLists.txt,bench.cpp}`, SHA256
`bb64a572a72ad99c800db551483a1384aa867cc3e89cd69d7390f70a6541228e`;
separate `build/queued-stop-multiply-bench-{windows,linux}` caches import their
current OS Release core, target `queued_bench`.

Focused reproduction: `aion_core_tests --queued-stop-multiply`;
`node tests/QueuedMergeSources.cjs --queued-stop-multiply --native PROBE`;
`tools/TestMouseWorkflow.ps1 -QueuedStopMultiplyOnly` and `-StoppingMultiplyOnly`.
Core/probe/Standalone/processor builds and safe committed WSL sync use the existing
commands in DEVELOPMENT_WORKFLOW.md and the preceding slice below. Retained core
selectors: `--stopping-multiply`, `--postbirth-multiply`, `--prebirth-multiply`,
`--queued-input-multiply`, `--gated-input-multiply`, `--queued-norm`, `--stopping-history`.
CTest selection: `^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_(normalization|reverse|history|multiply|dsp_kernels|queued_multiply|stutter|stopping_multiply|stopping_norm|stopping_history|queued_stop_multiply))$`.
Windows omits `queued_stop_multiply` (executed directly) and uses `-C Release`.
ASan/UBSan/leaks additionally execute the stopping-MULT and queued-NORM selectors.
Logs: `build/queued-stop-multiply-*-windows.log`, corresponding Linux logs in its
native checkout, including `-production-`, `-ctest-`, `-sanitizer-SELECTOR-` and
`-bench-`; successful Windows runs are `-engine-final-`, `-native-final-`,
`-ui-` and `-retained-ui-final-`. Earlier rejection/build/port-guard logs remain.
Next after acceptance: RTRG eligibility, then integrated core-button combinations
before FX/SAVE/presets.

### Implemented preview: MULT during scheduled STOP — 7 November 2026

Small slice: unchanged `Loop.multiply/multiplyInstant` doubles stored PCM during
scheduled STOP, even with input available, without restarting that graph or
changing its old buffer/rate/STOP deadline. One local history entry, original
visual progress on the doubled duration, older Start sources, Undo/Redo and NORM
must remain correct. Worker failure/capacity rejection preserves old PCM/history;
completion after STOP and DEL/panic/prepare must never revive playback.

Implementation **7da8f5b**. Native admission, original MULT control/shared GUI/CLI dispatch and worker
ownership now implement this case. Reuse bounded source/history leases, the
existing stored-edit deadline flag and history visual clock. No public IDs/state
schema, DSP kernel, dependencies, reference/preserved UI or logo changes.
`loop multiply TRACK` already uses this same processor; pending-stop queued MULT
and RTRG eligibility remain separate gaps.

Initial unchanged-JS rejection: `build/stopping-multiply-repro-windows.log`.
New source comparison **96 PASS each OS**, **360 exact stereo WAVs** and
**4,543,776 frames per OS**, peak error **6.26096477e-8** (unchanged **1e-6**);
progress <=**1e-12**. Mono/stereo, 8/48/96kHz, 17/512-frame clips, forward/reverse,
double/Undo/Redo/NORM/older Apply. New actual Engine **100 PASS**, tracks 0/19, 64/128,
forward/reverse, input/no-input, pending Apply, delayed worker through STOP,
local Undo/Redo, OOM/cancellation/prepare/full pool, exact stereo against an
unedited sounding source, zero callback new/delete. Frontend admission PASS.
Windows retained **14/14 CTests PASS (349.79s)**, connected capture/OD/SUB audit
**126 PASS / 965,826 frames / 7.39475750e-8**, Standalone build and full
production processor/external CLI suite PASS. Actual original MULT/Undo/Redo/NORM
pointer/PCM checks PASS at 64/128, forward/reverse, same source revision, stored
identities/history, retained deadline and natural STOP silence. Linux new Engine
100 plus retained Engine **288** post-birth MULT, **108** pre-birth MULT, **212**
queued-input MULT, **8** gated-input MULT, **88** stopping history, **120** stopping
NORM and **88** stopping MERGE PASS; same retained Engine checks pass on Windows.
Linux connected audit also passes with the same count/error; **15/15 Linux CTests
PASS (584.87s)**, including the new 96-case source corpus. Linux Debug
ASan/UBSan/leaks: new **100** stopping MULT, **88** stopping history and **288**
post-birth MULT PASS, no diagnostics. Linux Standalone build and full production
processor/external CLI suite PASS, run alone after heavy acceptance/sanitizers.

One misspelled Windows selector (`--queued-multiply-input`) entered the test
runner's default full suite; it was stopped, not counted as a completed suite.
Correct `--queued-input-multiply` and the remaining focused checks passed. Keep
the aborted log separate; no product failure or assertion/tolerance change.

Source comparison extends `QueuedMergeSources.cjs --multiply` using unchanged
reference methods and node IO stubs/raw PCM; browser gain/STOP envelopes,
resampling, advancing worker sound and pre-start future-source visual suppression
are excluded. Engine delayed-worker checks separately cover real STOP/ownership.
Actual Windows pointers use the production processor/editor and synthetic PCM,
not physical drivers/devices or OS-dialog interaction. Linux WebKit interaction,
full-bank/long-session/worst-case performance, deferred wrappers/DAWs and Apple
remain separate checks. Earlier unresolved missing-button/CLI-timeout observations
remain recorded below; this slice does not claim to resolve their unknown causes.
No package/push or next feature started.

After all acceptance finished, identical ignored Engine timing fixtures ran
Windows then Linux: 48kHz stereo, 512-frame capture plus OD, 512 warmed **playing**
callbacks, then stopping MULT (512 -> 1024), Undo/Redo, playback through STOP and
clear; input/no-input, forward/reverse, with/without older Apply, **16 scenarios
per OS**, 64/128. Online admission/worker-completion callbacks are timed; setup,
offline worker waits, input generation, telemetry consumption and the deliberate
1ms sleep between worker polls are excluded. Maximum observed per 64/128 callback:
Windows **111.3us /218.9us**, Linux **111.9us /152.3us**, versus **1333.33us /
2666.67us** deadlines. These small warmed Engine observations are not full
processBlock/round-trip latency, worst-case/full-bank/long-session guarantees, an
OS ranking or a speedup. Harness `build/stopping-multiply-bench-src/{CMakeLists.txt,bench.cpp}`,
bench.cpp SHA256 `ede5779b9258dda1f2aaff02b6d390098e563bf52660de6c1b555fd0112ad872`;
independent `build/stopping-multiply-bench-{windows,linux}` caches import each OS's
current Release core, target `queued_bench`. Logs `build/stopping-multiply-bench-{build-,}OS.log`.

Focused commands, using existing caches and the CMake/CTest resolution and safe
committed WSL sync in DEVELOPMENT_WORKFLOW.md:

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_dsp_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --stopping-multiply
node tests/QueuedMergeSources.cjs --multiply --native build/windows-core/Release/aion_start_probe.exe
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
cmake --build --preset windows-dev --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./tools/TestMouseWorkflow.ps1 -StoppingMultiplyOnly
./build/windows/Release/aion_midi_processor_tests.exe
```

```sh
# Local Windows commit, then clean SetupWslWorkspace.sh sync first.
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_dsp_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --stopping-multiply
ctest --test-dir build/linux --no-tests=error --output-on-failure --parallel 2 -R '^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_(normalization|reverse|history|multiply|dsp_kernels|queued_multiply|queued_multiply_sources|stutter|stopping_norm|stopping_history|stopping_multiply))$'
node tests/AudioParityAudit.cjs build/linux/aion_reference_probe
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --stopping-multiply
# Full production/CLI suite alone after all heavy checks finish.
./build/linux/aion_midi_processor_tests
```

Each OS retained core selectors: `--postbirth-multiply`, `--prebirth-multiply`,
`--queued-input-multiply`, `--gated-input-multiply`, `--stopping-history`,
`--stopping-norm`, `--stopping-merge`. Sanitizers also run `--stopping-history`
and `--postbirth-multiply`. Windows CTest uses the Linux regex minus
`stopping_multiply`, `-C Release` and its own cache; new 96 cases already ran directly.
Logs in each checkout: `build/stopping-multiply-{repro,probe-build,core-build,engine,native,front,audio,ctest,production-build,production,ui}-windows.log`
(as applicable), `build/stopping-multiply-{build,audio,ctest,production,sanitizer-build}-linux.log`,
and `build/stopping-multiply-{SELECTOR,sanitizer-SELECTOR}-OS.log`.
Next smallest gap: pending-stop **queued** MULT eligibility/source ownership;
then RTRG eligibility and integrated core-button combinations before FX/SAVE/presets.

### Implemented preview: Undo/Redo during scheduled STOP — 7 November 2026

Original `Loop.undo/redo` changes stored PCM/history without restarting a stopping
graph. Native local/global history now admits that state, retaining immutable
forward/reversed sounding PCM, source length/rate and any older phase sources
until the original STOP. Stored history/exports change immediately; next PLAY
uses restored PCM. A private audio-owned visual-clock flag keeps the playhead on
the restored duration through older phase events and clears on STOP/DEL/prepare
or a replacement using current storage. Existing telemetry is reused; no new
public IDs, schema, DSP kernel, reference/preserved UI, logo or dependencies.

Implementation **01d9642**; toolbar fix **873853d**. GUI local/global buttons and
existing CLI `loop undo/redo TRACK`, `loop undoGlobal/redoGlobal` share one
processor path. Actual mouse acceptance found an independent global-toolbar
guard still disabling stopping (and previously supported queued) history. It now
matches shared dispatch eligibility; no disabled-button bypass was used.

| Acceptance | Executed evidence |
| --- | --- |
| Original JS stored/source/visual oracle | **96 PASS** each OS, **360 exact stereo WAV exports**, **4,543,776 frames** each, max PCM error **6.41240175e-8**, unchanged **1e-6** limit. Mono/stereo 8/48/96kHz, 17/512-frame original clips, doubled-length Undo/Redo/rapid/NORM/older Apply. Unchanged `getProgress` assertions <=**1e-12** |
| Actual Engine, each OS | **88 PASS**, 64/128, tracks 0/19: local/global, same/changing length, forward/reverse, rapid history, original STOP/phase ownership, exact stereo against an unedited sounding source, clear/stop/prepare, full-pool atomic growth rejection, no callback new/delete |
| Retained Engine, each OS | **64** queued history, **124** post-birth history, **120** stopping NORM and **88** stopping MERGE PASS |
| Focused old post-birth source oracle, each OS | **48 PASS** near-grid `rapid-norm`, **192 sources**, **1,181,252 frames**, maximum **8.14015827e-8** (limit **1e-6**); remaining sequences reuse prior evidence |
| Connected capture/OD/SUB audit, each OS | **126 PASS**, **965,826 frames**, maximum **7.39475750e-8**, unchanged **1e-6** limit |
| Connected NORM/reverse/history/MULT/kernels; queued history, STUT; retained source fixture, assets/frontend/bridge/CLI admission | Windows **14/14 CTests PASS**, **176.96s**, new 96 history cases separately; Linux **15/15 PASS**, **368.16s**, including the new history gate |
| Actual Windows pointer/PCM | PASS 64/128: local/global original buttons, length/waveform/history/visual-clock metadata, STOP deadline and natural silence; queued global toolbar before/after source construction, original grid silence and exact restored identity |
| Standalone / full production processor+CLI | Build/full production processor+external CLI suite PASS each OS; Linux ran alone after reference/sanitizer acceptance ended |
| Native Linux Debug ASan/UBSan/leaks | **88** stopping, **64** queued and **124** post-birth history PASS, no diagnostics |

Reference rejection: `build/stopping-history-repro-windows.log`. Initial UI failure
`build/stopping-history-ui-windows.log` exposed the global-toolbar defect; passing
stopping and expanded queued-global reruns are `-ui-final-` and `-ui-accepted-`.
The capacity fixture uses manual capture and real 4096-frame block growth: an
initial 512-to-1024 edit correctly fits one allocated block despite no other free
blocks. The corrected 4096-to-8192 fixture proves rejection without PCM/history/
lease mutation. No assertion was relaxed or reference source modified.

The source oracle uses actual original methods with node IO stubs/raw PCM; mono
exports duplicate channels as in the native contract. Browser gain/STOP envelopes,
resampling and advancing-worker sound are excluded. Pending-Apply progress checks
begin after the original future source starts; its pre-start visual suppression
and broader future-source transitions are outside this comparison. Actual mouse
checks use the synthetic-PCM processor/editor host, not physical drivers/devices,
Linux WebKit, OS dialogs, wrappers/DAWs or Apple. Full-bank/worst-case and
long-session validation remain separate. This is preview acceptance, not release.
Earlier unresolved missing-button/CLI-timeout observations remain visible below.
No package or push.

Windows logs: `build/stopping-history-{probe-build,native,core-build,core-final,final-native,front,queued-history,postbirth-history,stopping-norm,stopping-merge,postbirth-native,audio,ctest,production,ui-accepted}-windows.log`.
Linux: `build/stopping-history-{build,stopping-history,queued-history,postbirth-history,stopping-norm,stopping-merge,postbirth-native,audio,ctest,production,sanitizer-build,sanitizer-stopping-history,sanitizer-queued-history,sanitizer-postbirth-history}-linux.log`.

After all acceptance jobs finished, identical ignored warmed Engine harnesses
ran Windows then Linux: 48kHz stereo, 512-frame capture plus OD and a doubled
1024-frame sounding source, 512 warm callbacks, local/global, forward/reverse,
with/without an older pending Apply; **16 scenarios per OS**, 64/128 samples.
Measured callbacks cover Undo/Redo/rapid Undo, playback through STOP, stopped Redo
and clear. Setup/offline waits and telemetry consumption are outside timing.
Maximum observed per 64/128 callback: Windows **148.5us /243us**, Linux
**108.9us /118.1us**; deadlines **1333.33us /2666.67us**. These small warmed
observations do not establish full-bank/worst-case/long-session performance,
physical round-trip latency, a platform ranking or a speedup.
Harness `build/stopping-history-bench-src/{CMakeLists.txt,bench.cpp}`; bench.cpp
SHA256 `091e04e8be4376a84b29e95e08980439ce4d289c4794b965992ff17ca56e0172`.
Separate `build/stopping-history-bench-{windows,linux}` caches import their own
current Release core; target `queued_bench`, logs
`build/stopping-history-bench-{build-,}OS.log`.

Focused commands (existing caches; CMake/CTest resolution and safe committed WSL
sync as in DEVELOPMENT_WORKFLOW.md):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_dsp_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --stopping-history
node tests/QueuedMergeSources.cjs --history --native build/windows-core/Release/aion_start_probe.exe
node tests/QueuedHistorySources.cjs --near-grid --only-sequence rapid-norm --native build/windows-core/Release/aion_start_probe.exe
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
cmake --build --preset windows-dev --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -StoppingHistoryOnly
```

```sh
# Local commit and clean SetupWslWorkspace.sh sync first.
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_dsp_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --stopping-history
ctest --test-dir build/linux --no-tests=error --output-on-failure --parallel 2 -R '^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_(stopping_history|normalization|reverse|history|multiply|dsp_kernels|queued_history|stutter|stopping_norm|merge_queued_destination|merge_stopping_destination))$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --stopping-history
# Full processor/CLI suite alone after other heavy acceptance jobs finish.
./build/linux/aion_midi_processor_tests
```

Retained core selectors: `--queued-history`, `--postbirth-history`,
`--stopping-norm`, `--stopping-merge`; sanitizers also execute the first two.
Linux also runs both Node commands above with its native probe paths. Windows
CTest uses the same regex minus `stopping_history`, `-C Release` and its own cache;
the new history corpus was already executed directly. No wrapper/version/dependency
change or repeated full-core audit; previous baseline plus affected gates.

**Next:** remaining MULT/RTRG pending-state eligibility and integrated core-button
combinations before FX/SAVE/presets. Stopping history meets the documented preview
slice gates above; do not infer full browser/driver or release acceptance.

### Implemented preview: NORM during scheduled STOP — 7 November 2026

Original `Loop.normalize` remains enabled after `Loop.scheduleStop`; native code
previously rejected/disabled that state. NORM now shares the existing stored-edit
lifetime used by MERGE: it preserves the audio-owned STOP/phase deadlines and
source, adds no local/global history, and cannot restart playback if its worker
finishes after STOP. GUI retains original NORM; CLI uses `loop normalize TRACK`.
Snapshot **8d708f2**. No public IDs, schema, DSP kernel, reference/preserved UI,
logo, dependency or target changes.

| Acceptance | Executed evidence |
| --- | --- |
| New actual Engine, Windows/Linux | **120 PASS** each, 64/128 and tracks 0/19: forward/reverse, changed/repeated NORM, accepted phase events, workers held across STOP, existing global history, independent exact stereo OOM rollback, STOP/DEL/prepare and no callback new/delete |
| Unchanged-JS source/deadline oracle | **48 PASS** each OS, **96 exact stereo WAVs**, **2,271,888 frames** each. Mono/stereo 8/48/96kHz, 17/512 frames, original `normalize`/`normalizeBuffer` and `scheduleStop`; max error **1.14235381e-7**, limit **1e-6** |
| Retained Engine, each OS | **108** queued NORM, **96** pre-birth NORM, **80** pending/active Apply/RTRG/OOM/full-pool NORM and **88** scheduled-stop MERGE cases PASS |
| Connected capture/OD/SUB audit, each OS | **126 PASS**, **965,826 frames**, maximum **7.39475750e-8**, unchanged **1e-6** limit |
| Original connected NORM/reverse/history/MULT/kernels; queued NORM and retained MERGE; frontend/bridge/assets/CLI admission | Windows **13/13 CTests PASS**, **143.76s**; new 48-case oracle separately. Linux **14/14 PASS**, **240.34s**, including the new NORM oracle |
| Actual Windows original NORM pointer/PCM | PASS 64/128, forward/reverse, repeated edits, waveform/length/reverse/history/deadline, continuing audible PCM and natural scheduled STOP |
| Standalone / production processor+CLI | Build/full production processor+external CLI suite PASS each OS; Linux suite ran alone after reference/sanitizer jobs ended |
| Native Linux Debug ASan/UBSan/leaks | **120** stopping NORM, **108** queued NORM, **88** stopping MERGE PASS; no diagnostics |

The reused source oracle gains `--normalize`; every prior MERGE case and its
numeric limits remain. Initial native rejection is preserved at
`build/stopping-norm-repro-windows.log`. Node IO stubs compare raw source PCM,
forward stored-buffer updates/reversed snapshots, exact stored exports and STOP
source deadlines, excluding browser gain/STOP envelopes, resampling and advancing
worker sound. Existing kernel/42-case connected NORM gates retain silence/unity
thresholds, short/long clips and stereo linkage; no tolerance was relaxed.

Actual Windows interaction uses the existing synthetic-PCM processor/editor host
and real CDP pointers, waiting for rendered/enabled controls before pressing.
No snapshot/audio/timing mocks or new product UI. Driver/physical input/controller,
Linux WebKit interaction, OS-dialog, wrapper/DAW, Apple, full-bank/worst-case and
long-session validation remain separate. This is preview acceptance, not release.
The two earlier unexplained missing-button/CLI-timeout observations below remain
open; this slice has not reproduced or explained them. No package or push.

Primary Windows logs: `build/stopping-norm-{core-build,core,native,queued-norm,prebirth-norm,norm-pending,stopping-merge,production-build,production,audio,ctest,ui}-windows.log`.
Linux: `build/stopping-norm-{build,stopping-norm,queued-norm,prebirth-norm,norm-pending,stopping-merge,production,audio,ctest,sanitizer-build,sanitizer-stopping-norm,sanitizer-queued-norm,sanitizer-stopping-merge}-linux.log`.

After all acceptance jobs ended, identical ignored warmed Engine harnesses ran
Windows then Linux: 48kHz stereo, one 512-frame clip, 512 warm callbacks,
forward/reverse changed/no-op NORM, **8 scenarios each OS** at 64/128 samples.
Measured callbacks cover admission/live result acceptance, playback through STOP
and clear. Setup, worker sleeps/offline waiting and telemetry consumption are
outside timing. Maximum observed per 64/128 callback: Windows **139.6us /224.5us**,
Linux **94.416us /114.216us**, against **1333.33us /2666.67us** deadlines. These
small warmed observations do not establish worst-case/full-bank/long-session
performance or physical round-trip latency, and imply no platform ranking/gain.
Harness `build/stopping-norm-bench-src/{CMakeLists.txt,bench.cpp}`; bench.cpp SHA256
`b4d524435486eac88a29e71524a136a9810c0fd5fb71f487d338a7260c844a73`.
Independent `build/stopping-norm-bench-{windows,linux}` caches import their own
current Release core; target `queued_bench`, logs
`build/stopping-norm-bench-{build-,}OS.log`.

Focused commands (existing caches; CMake/CTest paths and clean snapshot sync as
in DEVELOPMENT_WORKFLOW.md):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_dsp_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --stopping-norm
node tests/QueuedMergeSources.cjs --normalize --native build/windows-core/Release/aion_start_probe.exe
node tests/AudioParityAudit.cjs build/windows-core/Release/aion_reference_probe.exe
cmake --build --preset windows-dev --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -StoppingNormOnly
```

```sh
# Commit locally and sync the clean independent WSL mirror first.
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_dsp_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --stopping-norm
node tests/AudioParityAudit.cjs build/linux/aion_reference_probe
ctest --test-dir build/linux --no-tests=error --output-on-failure --parallel 2 -R '^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_(stopping_norm|normalization|reverse|history|multiply|dsp_kernels|queued_source_norm|queued_prebirth_norm|merge_queued_destination|merge_stopping_destination))$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --stopping-norm
# After other heavy acceptance processes finish, run the full processor suite alone.
./build/linux/aion_midi_processor_tests
```

Windows uses the same CTest regex minus `stopping_norm` (executed directly),
`-C Release` and its own cache. Retained core selectors: `--queued-norm`,
`--prebirth-norm`, `--norm-pending`, `--stopping-merge`; sanitizers also run
`--queued-norm` and `--stopping-merge`. No reconfiguration beyond CMake's changed
test-list regeneration, wrappers, packages, versions or dependency updates.

**Next:** scheduled-stop Undo/Redo eligibility, then remaining core-button
combinations. Original Undo/Redo replaces stored PCM without restarting a stopping
graph; preserve that distinction when establishing the next slice. NORM meets the
documented preview gates above. Keep FX/SAVE/presets explicitly unported until
core-button acceptance finishes.

### Implemented preview: scheduled-stop MERGE — 7 November 2026

Small slice: unchanged original `mergeLoops` admits a stored source and/or
nonempty destination after `Loop.scheduleStop`. Native GO and the existing shared
CLI now admit those states without restarting either source, dropping its
audio-owned STOP deadline, or modifying source PCM. Clone/mix/history and
success-only, identity/serial-validated source confirmation retain their
clear/STOP/OOM barriers. Destination STOP cancels its stale destructive
confirmation even if the worker finishes later; committed audio remains undoable.

The existing stored-edit worker lifetime retains audible playback until the
original STOP. Result acceptance restores stopping only while that deadline
still exists; an elapsed/cancelled STOP never restarts playback. No public IDs,
schema, parameters, reference/preserved UI assets, logo, dependencies or formats
change. CLI remains `loop merge DEST VALUE` (destination 1-based, source value
0-based; see CLI.md). Implementation snapshot **5fea6f7**; final pointer fixture
also waits for enabled post-stop history controls and checks actual phase/reverse/
waveform fields (an unavailable diagnostic field was caught during review).

Executed acceptance:

| Verification | Result / scope |
| --- | --- |
| Original JS stored source, Windows/Linux | **24 PASS** each: source stopping, clone/mix, mono/stereo 8/48/96kHz, exact stereo WAV/source preservation/Undo/Redo; max playback error **7.74698646e-8**, limit **1e-6** |
| Original JS source/deadline destination, Windows/Linux | **24 PASS** each, **48 exact stereo WAVs**, **1,135,944 sample frames**; forward/reverse, 17/512 frames, original `Loop.scheduleStop` source deadline, native stopping-to-stopped; max error **8.25520550e-8**, limit **1e-6** |
| Actual Engine, Windows/Linux | **88 PASS** each at 64/128, tracks 0/19, forward/reverse, source/destination/both, held workers across STOP/phase deadlines, independent exact source PCM, history, OOM/cancellation, no callback new/delete |
| Retained Engine gates, Windows/Linux | Ordinary MERGE, **120** queued destination, **32** queued source, **108** queued NORM, **96** pre-birth NORM and **80** pending/active Apply/RTRG/OOM/full-pool NORM cases PASS |
| Standalone / complete production processor suite | Build and complete suite PASS each OS, including actual external CLI/processor commands at 64/128; Linux suite ran alone after other acceptance jobs ended |
| Focused reference/frontend/bridge/sync CTest | Windows **9/9**, **110.91s**, new 48 stopping cases separately; Linux **11/11**, **165.72s**, including both new stopping gates |
| Actual Windows WebView pointer/dialog/WAV | PASS 64/128: source/destination/both scheduled STOP, forward/reverse, original GO, decline/confirm and source clear, exact saved stereo, post-stop Undo/Redo, all retained ordinary/queued MERGE/panic guards |
| Linux Debug ASan/UBSan/leaks | New **88** stopping cases, ordinary MERGE and **108** queued-NORM cases PASS; no diagnostics |

The node oracle uses unchanged original methods with node IO stubs/raw source
PCM. It excludes browser gain/STOP envelopes, resampling and advancing-worker
sound. Mono exports duplicate channels as in the native contract. Actual UI uses
the existing synthetic-PCM processor/editor host with deterministic file-picker
injection; not OS-dialog/driver/physical-device/latency, Linux WebKit, wrapper/DAW,
Apple, full-bank or long-session acceptance. Existing full-core baseline is
reused with affected suites above. This remains preview acceptance, not release.

Fixture repair: first attempts used a fresh direct native snapshot while the
rendered frontend was still showing PLAY (captured pointer evidence), or Undo
was still disabled just after STOP. New setup explicitly waits for rendered STOP,
master sync controls and enabled history buttons before input. Original behavior
checks remain; no product workaround, timeout extension or tolerance change. Keep failure
logs `build/stopping-merge-ui-windows.log`,
`build/stopping-merge-ui-diagnostic-windows.log` and
`build/stopping-merge-ui-rendered-windows.log`. Passing reruns are
`build/stopping-merge-ui-ready-windows.log` and
`build/stopping-merge-ui-final-windows.log`. Reference rejection reproduction:
`build/stopping-merge-{source,dest}-repro-windows.log`.

Primary Windows logs: `build/stopping-merge-{source-native,dest-native,core,merge,queued-merge-destination,queued-merge-source,queued-norm,prebirth-norm,norm-pending,production,ctest,ui-final}-windows.log`.
Linux: `build/stopping-merge-{build,stopping-merge,merge,queued-merge-destination,queued-merge-source,queued-norm,prebirth-norm,norm-pending,ctest,production,sanitizer-stopping-merge,sanitizer-merge,sanitizer-queued-norm}-linux.log`.

After acceptance jobs finished, the identical ignored warmed Engine harness ran
Windows then Linux: 48kHz stereo, two 512-frame clips, 512 warm callbacks,
source/destination/both scheduled STOP, forward/reverse, **12 scenarios each OS**.
It measures MERGE admission/live worker acceptance, playback through STOP,
post-stop Undo/Redo and clear. Setup, worker sleeps/offline waits and telemetry
consumption are outside measured callbacks. Maximum observed per 64/128 block:
Windows **152.7us /191.9us**, Linux **92.411us /104.434us**; deadlines
**1333.33us /2666.67us**. This is a small warmed core observation, not worst-case,
full-bank/long-session performance, total round-trip latency or a platform ranking.
No speedup claim. Harness `build/stopping-merge-bench-src/{CMakeLists.txt,bench.cpp}`;
bench.cpp SHA256 `8969866b7588aede2e3447fe6b619df7596f2e01cf2b1e6c44735069427e3b1b`.
Independent `build/stopping-merge-bench-{windows,linux}` caches import their own
current Release core; target `queued_bench`, logs
`build/stopping-merge-bench-{build-,}OS.log`.

Commands (existing caches; CMake/CTest resolution and snapshot sync as in
DEVELOPMENT_WORKFLOW.md):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --stopping-merge
node tests/MergeParity.cjs build/windows-core/Release/aion_start_probe.exe --stopping-source
node tests/QueuedMergeSources.cjs --scheduled-stop --native build/windows-core/Release/aion_start_probe.exe
cmake --build --preset windows-dev --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -MergeOnly
```

```sh
# Local Windows commit -> clean SetupWslWorkspace.sh sync first.
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
./build/linux/aion_core_tests --stopping-merge
ctest --test-dir build/linux --no-tests=error --output-on-failure --parallel 2 -R '^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_merge|js_native_merge_queued_source|original_merge_queued_destination|js_native_merge_queued_destination|js_native_merge_stopping_source|js_native_merge_stopping_destination|js_native_sync)$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --stopping-merge
```

Retained core selectors: `--merge`, `--queued-merge-destination`,
`--queued-merge-source`, `--queued-norm`, `--prebirth-norm`, `--norm-pending`.
Sanitizer run also uses `--merge` and `--queued-norm`. Windows nine-gate CTest
uses the same regex minus the two new stopping gates, already run directly.
Run the Linux full production suite alone after heavy acceptance processes:
`./build/linux/aion_midi_processor_tests`. No wrappers, package or push.

**Next:** integrated core-button combinations/remaining stopping-state
eligibility before FX/SAVE/presets. Scheduled-stop MERGE meets the documented
preview slice gates above. Keep the earlier missing-button and Linux CLI-timeout
observations below visible; this slice does not resolve or explain them.

### Implemented preview: queued-destination MERGE

Acceptance: unchanged `mergeLoops` mutates stored destination PCM without a source
restart. Preserve pending PLAY/graph ownership and the original audible grid;
constructed forward sources see the in-place mix, constructed reversed sources
retain their independent old snapshot. Keep pending Start/history sources, one
local/global history entry, immutable worker pins, source preservation and all
confirmation/cancellation/OOM guards. Source/destination may both be queued.

The existing queued NORM lifetime flag is renamed `queuedStoredEdit` internally
and shared by MERGE. No public IDs/schema/parameters change. Result telemetry
explicitly continues the pending PLAY receipt, and already accepted phase changes
remain valid if their deadline arrives during MERGE finalization. Original GO
now admits queued destinations. CLI uses the same existing `loop merge DEST VALUE`
processor command (see CLI.md). Preserved UI/reference assets and logo unchanged.

Snapshot **e820035**, building on **92737c7**. Executed acceptance:

| Verification | Result / exact scope |
| --- | --- |
| Unchanged-JS source/audio oracle, each OS | **96 PASS**, **240 exact stereo WAV exports**, **2,418,240 compared sample frames**, mono/stereo 8/48/96kHz, pre-birth/constructed source, pending Start and Undo/Redo. Mono source exports are duplicated stereo, as in the native contract. Maximum PCM error **8.25520550e-8**, unchanged **1e-6** limit |
| Actual Engine, each OS | **120 PASS**, 64/128, tracks 0/19; stopped/queued sources, real meter owner and full-width PLAY receipt continuation, workers held across grid/phase deadlines, global history/confirmation invalidation, OOM, STOP/clear/prepare and zero callback new/delete |
| Retained MERGE/source gates, each OS | `--merge` and **32** `--queued-merge-source` cases PASS: exact PCM/history, reversed ownership, pending STOP, full-pool/DEL/prepare/OOM and no stale clear |
| Shared NORM lifetime regression, each OS | **108 queued**, **96 pre-birth**, plus pending/active Apply/RTRG (**48** playback, **20** cancellation, **8** OOM, **4** full-pool) cases PASS; no callback new/delete |
| Standalone + complete production processor suite | Build/suite PASS each OS; Linux isolated rerun passes after the timeout observation below. Real external CLI actions at 64/128 pass; no new RPC, duplicated DSP or state schema |
| Focused CTest | Windows **8/8**, **58.85s**, plus new native destination oracle separately. Linux **9/9**, **166.25s**, including the new native oracle. Windows sync probe was then relinked to the current core and that gate rechecked separately |
| Actual Windows WebView pointer/dialog/export | PASS 64/128: queued forward/reversed destinations before/after graph birth, original GO, clone/source confirmations and guards, exact saved WAV/source preservation, Undo/Redo and closed volume gate. Retained source MERGE/panic tests remain |
| Native Linux Debug ASan/UBSan/leaks | `--queued-merge-destination` (**120**), `--merge` and `--queued-norm` (**108**) PASS, no diagnostics |

No Linux WebKit interaction, physical device/controller/latency, wrapper/DAW,
Apple, full-bank or long-session acceptance was executed. Existing full-core
baseline is reused with the affected suites above; this is preview acceptance,
not a release-ready claim. No package or push.

One first Linux production run timed out in the existing external CLI
`loop finish 1` **8-second** subprocess check, while the ASan run was also active.
The isolated full production rerun passes at unchanged deadlines, including both
CLI block sizes. Cause is **unresolved/unreproduced**; concurrency is an observation,
not an established diagnosis. No product/timeout/assertion change was made.
Preserve `build/queued-merge-dest-production-linux.log` and the failed test's
mailbox artifacts at `build/queued-merge-dest-cli-timeout` (moved from its own
untracked UUID directory without deleting/overwriting files). Successful rerun:
`build/queued-merge-dest-production-isolated-linux.log`. Investigate if it recurs.

The first regression rejected `merge 0 1` on a genuinely queued destination:
`build/queued-merge-dest-repro-windows.log`. Fixture corrections preserve every
numerical threshold: align sync after a nonzero clock (original zero-origin grid
fallback is intentional), and export mono oracle buffers as duplicated stereo.
The source oracle uses unchanged original methods with node IO stubs; it does not
claim full browser resampling/envelopes or advancing-worker audio equivalence.
The earlier missing-button observation remains unresolved/unreproduced.

Primary Windows logs: `build/queued-merge-dest-{native,final-core,merge,source-core,queued-norm,prebirth-norm,norm-pending,production,ctest,ui,sync-current}-windows.log`;
Linux: `build/queued-merge-dest-{build,core,merge,source-core,queued-norm,prebirth-norm,norm-pending,production-isolated,ctest,sanitizer-queued-merge-destination,sanitizer-merge,sanitizer-queued-norm}-linux.log`.

After acceptance processes ended, identical ignored warmed harnesses ran Windows
then Linux, using the current core: 48kHz stereo, 512-frame clips, 512 warm
callbacks, pre-birth/constructed forward/reverse destination, two observations
per combination (**16 total each OS**), live worker acceptance, Undo/Redo, original
grid playback and serial-validated source clear. Setup/worker sleeps/offline
waits/telemetry consumption are outside measured callbacks. Maximum observed:
Windows **176.1us /289.4us**, Linux **186.886us /195.849us** at **64 /128** samples
(deadlines **1333.33us /2666.67us**). This is not worst-case/full-bank performance,
round-trip latency or a platform ranking. No performance-gain claim.
Harness: `build/queued-merge-dest-bench-src/{CMakeLists.txt,bench.cpp}`;
SHA256 of bench.cpp `ea15def12ed90c6533110475fdfbd20da51ceae043d77f86faa02adecbca026d`.
Independent `build/queued-merge-dest-bench-{windows,linux}` caches import each OS's
current core; target `queued_bench`; logs `build/queued-merge-dest-bench-{build-,}OS.log`.

Focused commands (existing caches; resolve CMake/CTest as documented below):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --queued-merge-destination
node tests/QueuedMergeSources.cjs --native build/windows-core/Release/aion_start_probe.exe
./tools/TestMouseWorkflow.ps1 -MergeOnly
```

Linux substitutes its native `build/linux` cache and executable paths. Retained
core selectors: `--merge`, `--queued-merge-source`, `--queued-norm`,
`--prebirth-norm`, `--norm-pending`; production suite `aion_midi_processor_tests`.
Sanitizers: existing `build/overlap-asan`, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
UBSAN_OPTIONS=halt_on_error=1`, with the three selectors in the table above.
CTest regex: `^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_merge|js_native_merge_queued_source|original_merge_queued_destination|js_native_sync)$`
with `--no-tests=error --output-on-failure --parallel 2`; add
`js_native_merge_queued_destination` for Linux's executed nine-gate run. Its
Windows native comparison was executed directly. Build Standalone/mouse/processor
as in the previous checkpoint; Linux omits the Windows mouse host. No dependency
upgrade, wrapper or version bump.

**Next:** scheduled-stop MERGE eligibility, then the integrated core-button
bug-fixing pass. Keep both unresolved observations visible. FX/SAVE/presets remain
later; queued-source/destination functionality above is implemented preview.

### Implemented preview: MERGE from a queued source

Acceptance: read the original source's stored PCM before/after queued graph birth,
without changing its PLAY receipt, immutable graph/reversed snapshot or audible
grid. Clone into empty destinations stopped with source tempo metadata; mix into
ordinary existing destinations with one local/global Undo entry. Confirm clearing
only the same source waveform/clear epoch after a successful merge. Queued
**destinations** remain guarded and are the next separate lifecycle slice.

Implementation extends only source-state admission and the original confirmation
eligibility to `queued`. Existing worker mixing, lease ownership, IDs, serial
fences, history, RPC and generic CLI command stay shared. `loop merge 1 1` means
source Loop 2 into destination Loop 1; see CLI.md for index conventions.
Original assets/reference and logo remain untouched.

Implementation snapshot **97a42a4**; previous full-core/queued-input baseline
**953302b** is reused. Executed acceptance:

| Verification | Result / scope |
| --- | --- |
| Unchanged `mergeLoops` / `sliceBuffer` / `mixBuffersCircular`, each OS | **48 queued-source cases**, exact stereo WAV bytes, mono/stereo 8/48/96kHz, clone/circular mix, source preservation/queued metadata, Undo/Redo and committed playback <=1e-6; max **7.74698645e-8**. Retained **72 ordinary MERGE cases** also PASS |
| Actual Engine, each OS | **32 PASS**, 64/128, source tracks 0/19, pre-birth/constructed forward/reversed graphs, exact paired nonzero source PCM, original PLAY receipt/grid, clone/mix/history/confirmation, zero callback new/delete |
| Retained MERGE suite, each OS | PASS (`--merge`): immutable pins, playing phase/reverse, OOM, DEL/prepare/full-pool, exact 64/128 Engine WAV/PCM; new queued OOM/retry, constructed-source pending STOP, confirmed clear and no old-grid resurrection |
| Standalone + complete production processor suite, each OS | Build/tests PASS; common handler remains GUI/CLI accessible. CLI indexing/submission unit regression PASS; no new RPC or second DSP implementation |
| Focused CTest | **7/7 each OS**: CLI admission/indexing, unchanged assets, frontend, bridge lifecycle, ordinary/queued-source MERGE and JS/native sync. Windows **66.40s**, Linux **97.95s**; updated CLI test also executed directly in Windows |
| Actual Windows WebView pointer/dialog/export | PASS 64/128 (`-MergeOnly`): original GO/Undo/Redo, accepted/declined confirmations, queued pre-birth and constructed reversed source clone, exact saved stereo WAV/source preservation, pending-worker panic and active-capture guards |
| Native Linux Debug ASan/UBSan/leaks | `--merge` and all **32** `--queued-merge-source` cases PASS, no diagnostics |

No Linux WebKit interaction, physical driver/controller, wrapper/DAW, Apple or
long-session acceptance was performed. The source oracle compares unchanged
stored-buffer methods and committed playback; the paired Engine checks prove
native source ownership/grid preservation, not full browser transition-envelope
or worker-latency equivalence. The prior full core audit is reused; only affected
suites were rerun. No package, version bump or push.

New fixture repairs retain original assertions: timeline reset must precede sync
configuration; graph-owner tests initialise the event generation and consume the
real buffer receipt. Restoration must await its own 50ms confirmation. A sequential
CDP MERGE/STOP test also observed an extra dialog with ambiguous command timing;
it now uses the existing test host's real-callback finalization pause handshake,
asserts an actual pending job, then queues STOP before resume. No product timer,
assertion or tolerance changed. Original failures remain in
`build/queued-merge-source-{repro,ui,ui-final}-windows.log`.
The earlier `#loop-action-0` missing-target observation from the MULT session
remains unresolved/unreproduced, independently of these fixture repairs.

Logs: Windows `build/queued-merge-source-{native,final-merge,final-core,production,ctest,ui-owned}-windows.log`;
Linux `build/queued-merge-source-{build,merge,core,production,ctest,sanitizer-build,sanitizer-merge,sanitizer-core}-linux.log`.
The Windows CTest log includes the final queued oracle source-state assertions;
its CLI extension was checked directly after the seven-gate run.

Warmed callback observation, run sequentially after all builds/tests finished:
16 cases per OS, 48kHz stereo, 512-frame clips, 512 warm callbacks, pre-birth/
constructed forward/reverse source, clone/mix, live worker-result acceptance,
Undo/Redo, original-grid playback and serial-validated source clear. Worker sleeps,
setup/finalization waits and telemetry consumption stay outside measured callbacks.
Maximum observed across measured steps: Windows **225.9us / 266.4us**, Linux
**118.16us / 155.452us**, at **64 /128** samples respectively (deadlines
**1333.33us /2666.67us**). Single-loop observations are not full-bank/worst-case,
long-session, round-trip latency or a platform ranking.
Ignored identical harness `build/queued-merge-bench-src/{CMakeLists.txt,bench.cpp}`;
SHA256 of bench.cpp: `abb5c4cf19939eed07547d9a3e6bf3dfe1627815352ffa4231661c6f388dece6`.
Independent caches `build/queued-merge-bench-{windows,linux}`, target `queued_bench`,
import each OS's current core library; logs `build/queued-merge-bench-{build-,}OS.log`.

**Next:** queued-destination MERGE, with original source snapshots/phase/grid and
worker history/confirmation ownership acceptance before enabling its GO control.
Retain other guarded state gaps (including scheduled-stop eligibility) explicitly;
FX/SAVE/presets follow core editing. Investigate the missing-button observation if
it recurs; keep its failure diagnostics and original assertions.

Reproduce focused checks with existing caches:

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --merge
./build/windows-core/Release/aion_core_tests.exe --queued-merge-source
node tests/MergeParity.cjs build/windows-core/Release/aion_start_probe.exe --queued-source
./tools/TestMouseWorkflow.ps1 -MergeOnly
```

CTest selection:
`^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_merge|js_native_merge_queued_source|js_native_sync)$`,
with `--no-tests=error --output-on-failure --parallel 2`. New selectors supplement
full-suite coverage. Linux uses `build/linux`, paths without `Release/`/`.exe`,
and the clean committed-snapshot helper. Sanitizers use `build/overlap-asan` with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` and the
same two core selectors. Standalone/processor targets were built with
`cmake --build build/windows --config Release --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2`;
Linux omits `aion_mouse_host` and uses its native cache. No dependency upgrade or wrapper.

### Implemented preview: queued-source MULT input capture

Snapshots **27a726c**, strict-GCC formatting/diagnostics **69863dd**, gated-source
repair **f2560d2**. Baseline `d6c91e0` is reused. The existing shared Multiply
command now starts capture with available input after a queued graph has been
constructed. Recorder ownership stays separate from the original audible grid:
opening playback does not finish capture or discard older Start replacements.
A second MULT or main FINISH appends through the existing worker/DC/splice kernel,
adds local-only history and admits the replacement 5ms after result acceptance.
Takes below 128 samples retain PCM/history/source identity. Existing DEL/panic/
prepare, OOM, pool and phase-FIFO rollback remain. No new IDs, persistence schema,
parameters, queues, dependencies, logo or preserved UI changes.

An early RTRG can already be playing behind that same closed grid. The new
comparison reproduced its lost replacement deadline (**0 vs expected 3796** at
8kHz). Capture now recognises this queued-origin state too, including static/
dynamic cases; ordinary post-grid capture is unchanged. Failure logs:
`build/queued-input-repro-windows.log` (original admission rejection) and
`build/queued-input-gated-repro-windows.log` (gated deadline). The old unported-
input guard assertion was implementation-coupled: it is replaced by stronger
capture/audio/lifetime cases below. Its varispeed guard and all 288 old no-input
cases remain. The probe now forwards its Multiply argument; old zero calls retain
behavior. Full suites include the new cases; `--gated-input-multiply` is a focused
follow-up selector, not a product command.

| Executed verification | Result / exact scope |
| --- | --- |
| Original-JS input/source comparisons, Windows and Linux | **192 base + 48 gated-playing cases**, **96 + 24 exact stereo WAVs** each OS; mono/stereo source PCM <=1e-6, maximum observed **8.74844245e-8**. 8/48/96kHz, forward/reverse, short/append/grid crossing, pending Start overlap, static/dynamic gated origins |
| Real Engine | **212 PASS each OS**, tracks 0/19, 64/128; timestamped MULT/main FINISH, exact paired stereo, local history/Undo/Redo, held-worker clear/panic, prepare, OOM/pool/phase-FIFO exhaustion, zero callback new/delete |
| Standalone and complete production processor suites | PASS each OS, including shared GUI/CLI input capture and active-but-gated RTRG, exact 5ms/length/history and subsequent audible PCM |
| Existing source/reference/frontend/bridge/CLI gates | Windows **21/21 PASS**, **615.05s**; Linux **23/23 PASS**, **1154.89s**, including the new base input oracle. Gated follow-up runs separately; unchanged passing paths are reused |
| Retained no-input MULT and loop edits | **288** old source cases plus capture/reverse/history/Multiply/Start/MERGE suites PASS each OS |
| Native Linux Debug ASan/UBSan/leaks | Baseline **204** input cases + loop edits PASS; latest gated-only **8** cases PASS, no diagnostics. No full unchanged suite repeated after the gated-only admission repair |
| Actual Windows new original controls | PASS at 64/128: queued and already-active/gated RTRG input capture, crossing grid, main FINISH/MULT, stored identity/Undo/Redo, original silence gate and nonzero playback, input/no-input bus lifecycle |
| Integrated Windows Mouse/Export/MERGE | Passing full 64 and 128 reruns preserve all original hold/keyboard/Start/STUT/clear/export/MERGE assertions; see unresolved observation below |
| Running Standalone CLI/WebView | PASS: external instance/status/parameter roundtrip, same processor generation, original UI, no product AudioContext; browser sound oracles also pass. Master stays zero. This is not physical input/driver latency validation |

GCC initially rejected a compressed conditional under Werror; formatting repair
passed without DSP changes. One initial integrated 64 mouse run failed to find
`#loop-action-0` after held-STOP cancellation. Its cause is **unresolved and not
reproduced**: diagnostic full 64 and full 128 reruns pass. Do not claim this
observation fixed or infer zero bugs. Failure log
`build/queued-input-integrated-ui-windows.log`; reruns
`build/queued-input-integrated-diagnostic-windows.log` and
`build/queued-input-integrated-final128-windows.log`. Missing-target failures now
report document/ready state, native/card presence, button IDs and runtime errors.
No gesture, hold threshold, assertion or tolerance was removed or relaxed.

Primary logs: Windows `build/queued-input-{wav-native,finish-core,existing-mult,edits,production,ui-final,acceptance-ctest,cli-ui}-windows.log`;
Linux `build/queued-input-{fixed-build,core,existing-mult,edits,production,acceptance-ctest,sanitizer-build,sanitizer}-linux.log`.
Follow-up logs on both OS: `build/queued-input-gated-{build,native,core,production}-OS.log`
(Windows production build is `gated-production-build`), plus Windows `gated-ui`
and Linux `gated-sanitizer{,-build}`. Original/reference assets are unchanged.

Reproduce using existing caches (CMake/CTest resolution as below):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --queued-input-multiply
./build/windows-core/Release/aion_core_tests.exe --postbirth-multiply
./build/windows-core/Release/aion_core_tests.exe --loop-edits
node tests/QueuedMultiplySources.cjs --input-capture --native build/windows-core/Release/aion_start_probe.exe
cmake --build build/windows --config Release --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -PostbirthMultiplyOnly
./tools/TestMouseWorkflow.ps1 -SkipMidi
./tools/TestCliUi.ps1 -PythonPath 'C:/Users/jorge/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
```

Linux substitutes `build/linux`, no `--config`, and executable paths without
`Release/` or `.exe`. The input oracle now includes all 240 cases; the executed
follow-up used `--only-sequence capture-gated-playing`, reusing the 192-case pass.
Use the prior 21-test regex below for Windows. For the executed Linux 23-gate
selection, add `multiply_input_source` inside its `js_native_(...)` alternatives
and `input_source` inside `original_multiply_(...)`. CTest uses `--parallel 2
--no-tests=error --output-on-failure`. Native ASan cache: build
`build/overlap-asan --target aion_core_tests --parallel 2`, then
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests`
with `--queued-input-multiply`, `--loop-edits`, or focused
`--gated-input-multiply` for the separately executed follow-up.

After all acceptance processes ended, the same local ignored warmed harness ran
Windows then Linux, with current libraries, at 48kHz: 512-frame seed, 512 warm
callbacks, queued source, 4096-frame live input capture across its original grid,
main FINISH, live result acceptance, Undo/Redo/playback/clear, forward/reverse.
Worker readiness polling and setup are outside callback measurement. No offline
wait is included in measured finish/result callbacks.

| Maximum measured callback (single-loop observation) | 64 samples | 128 samples |
| --- | ---: | ---: |
| Windows Release | 103.2us | 122.0us |
| Native WSL Linux Release | 71.065us | 116.243us |
| Block deadline | 1333.33us | 2666.67us |

Local harness `build/queued-input-bench-src/{CMakeLists.txt,bench.cpp}`; identical
source SHA256 `232aeefbc4607123461a64cb05aead22b106673cab4b64322ccc258c1097b23e`.
Caches `build/queued-input-bench-{windows,linux}` import the current per-OS core
library. Build with `cmake --build CACHE --target queued_bench --parallel 2`
(Windows `--config Release`), then run `CACHE/queued_bench` (Windows
`CACHE/Release/queued_bench.exe`). Logs `build/queued-input-{final-bench-build,bench}-OS.log`.
This is not worst-case/full-bank, long-session, physical latency or a platform
performance ranking. No version bump, wrapper, package or push.

Recorder nodes in the JS oracle are IO stubs; it compares unchanged capture
handlers/conditioning/source clocks and stored stereo bytes. Browser recorder
quantum timing, broader envelopes/advancing worker latency, physical devices and
Linux WebKit interaction remain separate. **Next:** retain/investigate the UI
observation if it recurs; queued MERGE eligibility is the next functional slice.
FX/SAVE/presets remain after core loop editing.

### Verified repair: MULT worker/source tempo ordering

The next focused comparison reproduced a BPM-ordering error during no-input
MULT finalization (original-JS maximum PCM error **0.1245821860**, tolerance
**1e-6**). Replacement sources now retain the rate/master origin captured at
accepted MULT, alongside their existing Reverse/Start snapshot. Subsequent tempo
metadata remains current for later Apply. A failed worker replacement delivers
that accepted tempo to the surviving old source; the new OOM regression failed
before this recovery repair and retains exact stereo/history/identity assertions.
A second unchanged-JS corpus reproduced an old-source error (**0.0034077666**)
when BPM changed after source construction but before its +5ms initial rate.
The future-source rate guard now protects tempo writes as well as Speed writes.
No queues/capacities, public IDs, serialized state, preserved UI or dependency
versions change. No allocation, lock or full-buffer work was added to processing.

Acceptance: `--worker-tempo` and `--source-tempo` in
`tests/QueuedMultiplySources.cjs`, real held-worker Engine cases including OOM,
and production GUI/CLI parameter ordering before/after source construction.
The existing 240 post-birth MULT cases remain; 48 tempo/recovery cases are added.
The production metadata test retains its original eight Reverse/Start cases and
adds sixteen tempo cases at 64/128, early/near-grid and both directions.
`TestMouseWorkflow.ps1 -FinalizingControlsOnly` also changes the original BPM
control while the actual worker finalizer is paused and waits for its native
parameter acknowledgement. It preserves all prior Q/nudge and rapid FIFO cases.

**Scope:** these tempo targets precede the future initial-rate event, which
overrides them before the replacement starts. The source oracle does not model
an audible exponential ramp; live ramp/timestamp ties, advancing worker-latency
sound, physical drivers and release acceptance remain separate. Failure logs:
`build/multiply-worker-tempo-repro-windows.log`,
`build/multiply-worker-tempo-oom-repro-windows.log`, and
`build/multiply-source-tempo-repro-windows.log`. Product snapshot **82971d6**;
verified test-host acknowledgement repair **2978a46**. The first UI rerun caught
an asynchronous fixture race: finalizing telemetry can precede the message-thread
pause file. The fixture now awaits a fresh audio-owner acknowledgement and the
host retires stale markers on arm/resume. Exact frozen-clock, native dispatch,
state/PCM and original pointer assertions remain. Failed logs are retained as
`build/multiply-tempo-acceptance-ui{,-recovery}-windows.log`; final passing log is
`build/multiply-tempo-acceptance-ui-final-windows.log`. No product timing was
changed to repair this fixture.

| Executed for this repair | Result |
| --- | --- |
| Windows/Linux Standalone builds and complete production processor suites | PASS each; original eight metadata cases plus sixteen real GUI/CLI BPM-order cases, exact paired stereo and tracked zero callback new/delete |
| Focused real Engine post-birth MULT corpus | **288 PASS each OS**, including 48 new tempo/OOM cases, tracks 0/19 and 64/128; existing 240 cases retained |
| Affected loop-edit/core ownership suites | PASS each OS: capture/finalization, reverse, history, Multiply, Start and MERGE; cancellation, OOM, pool exhaustion and zero callback new/delete |
| Selected reference/frontend/bridge/CLI CTests | **21/21 PASS each**; Windows **614.66s**, Linux **951.10s**, parallel 2; no passing corpus rerun for the fixture-only repair |
| New original-JS tempo corpora | 192 cases each before/after source construction; mono/stereo, 8/48/96kHz, 1/17/512/4097 frames, both BPM directions, forward/reverse and early/near-grid; native PCM <=1e-6. Windows observed maximum **5.86491305e-8** |
| Adjacent STUT source PCM oracle | PASS each OS, unchanged 1e-6 threshold |
| Native Linux Debug ASan/UBSan/leaks | `--postbirth-multiply` and `--loop-edits` PASS, no diagnostics |
| Actual Windows original controls/bridge/PCM | Final fresh-acknowledgement run PASS at 64/128: Q/nudge, range keys, rapid local/global history and MULT FIFO, acknowledged worker-time BPM, retained grid and nonzero output |

Logs: Windows `build/multiply-source-tempo-{build,core,edits,production-build,production}-windows.log`,
`build/multiply-tempo-acceptance-ctest-windows.log`; native WSL
`build/multiply-tempo-{build,core,edits,production,sanitizer-build,sanitizer}-linux.log`
and `build/multiply-tempo-acceptance-ctest-linux.log`. Separate STUT summaries are
`build/multiply-tempo-stutter-{windows,linux}.log`. CTest summary logs retain the
per-test results; its shared `Testing/Temporary/LastTest.log` contains the separate
STUT run, not the full selection's raw stdout. Do not use that temporary file as
the 21-gate log.

Reproduce with existing per-OS caches and the CMake/CTest resolution below:

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --postbirth-multiply
./build/windows-core/Release/aion_core_tests.exe --loop-edits
cmake --build build/windows --config Release --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -FinalizingControlsOnly
```

Linux uses `cmake --build build/linux --target AionLooper_Standalone aion_midi_processor_tests aion_core_tests aion_start_probe aion_reference_probe --parallel 2`,
then `./build/linux/aion_core_tests --postbirth-multiply`, `--loop-edits`, and
`./build/linux/aion_midi_processor_tests`. The exact 21-gate selection, with
`ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R REGEX`
or Linux `--test-dir build/linux` without `-C`, is
`^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_(start_phase|queued_early_restart|queued_early_rtrg|queued_history|multiply_worker_tempo|multiply_source_tempo|multiply_worker_metadata|multiply_source_controls|queued_multiply_sources|queued_multiply|multiply|reverse|history|normalization|sync)|original_multiply_(worker_tempo|source_tempo))$`.
Run `^js_native_stutter$` separately. Sanitizers use the existing
`cmake --build build/overlap-asan --target aion_core_tests --parallel 2`, then
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests`
with each focused selector above. No wrapper builds, dependencies or packages.

After all acceptance processes ended, the unchanged ignored warmed callback
harness was relinked with the current Windows/Linux libraries and run sequentially
(Windows then Linux). Same 48kHz, initial 512-frame capture, stopped doubling,
constructed queued source, no-input MULT/history and playback, forward/reverse,
512 warm callbacks and worker settlement outside measurement as the prior run.

| Maximum measured callback (single-loop observation) | 64 samples | 128 samples |
| --- | ---: | ---: |
| Windows Release | 167.4us | 134.0us |
| Native WSL Linux Release | 115.301us | 183.901us |
| Block deadline | 1333.33us | 2666.67us |

Existing harness caches `build/postbirth-multiply-bench-{windows,linux}`;
commands `cmake --build CACHE --target queued_bench --parallel 2` (Windows adds
`--config Release`) and `CACHE/queued_bench` (Windows `CACHE/Release/queued_bench.exe`).
Build/measurement logs: `build/multiply-tempo-bench{,-build}-{windows,linux}.log`.
This is not worst-case/full-capacity, an audible live-tempo-ramp measurement,
long-session evidence, hardware latency or a platform performance ranking.
No full core suite was repeated: the preceding full audits are reused with the
new affected core/reference/processor/sanitizer/UI evidence above.

**Next:** constructed queued-source MULT with available input, retaining capture
across the original audible grid; then remaining queued MERGE eligibility.
The checklist defines the next observable acceptance before any guard is enabled.
FX/SAVE/presets follow core looping/editing. Linux WebKit interaction/listening,
physical devices and broader browser/ramp/worker-latency sound remain separate.

### Verified repairs: finalization admission and source-boundary fades

Implementation snapshots `e4e88a2`, diagnostic `b4aad45`, DSP repair `d006ef5`.
GUI Apply/Q/nudge/Speed/RTRG/NORM gestures use the existing bounded FIFO wait
sentinel through their target's finalization. CLI waits before admission, with a
three-second deadline plus existing bounded mailbox exchanges, generation/DEL
fences, and no replay of admitted commands. Raw Apply now rejects an unapplied
finalization request; Q/nudge retain atomic rollback. Mixer/metadata/DEL/panic
priority, queue bounds/expiry and all IDs/state remain unchanged.

The first Linux edit run exposed an intermittent pre-existing exact history
failure. Reproduction with failure-only diagnostics: setup clocks **3328 /3392**,
first unequal output frame **658**, maximum PCM error **0.00207536**. A nearly
integral source deadline was snapped by `deadline()`, while its tiny negative
phase residual wrapped zero to length-epsilon and restarted the boundary fade.
The phase residual now follows the same **four-double-epsilon representation-noise
bound** as `deadline()`. Genuine fractional delays/source offsets remain. The
new deterministic source regression at clocks 3945/4009 failed before the fix;
exact cursor/stereo PCM/fade and old history assertions all remain unchanged.
`--start-phase` is a focused test selection, not a product command.

| Executed verification for d006ef5 | Result |
| --- | --- |
| Windows/Linux Standalone builds and complete production processor suites | PASS; real external CLI and MidiBuffer, state/timer/lifetime checks, tracked zero callback new/delete |
| Full Windows/Linux core audio/ownership audits | PASS each; retained source/history/Multiply/cancellation/OOM/overflow and 64/128 cases |
| Original-JS/source/frontend/bridge/CLI CTests | **17/17 PASS each**; Windows 547.01s, Linux 759.72s, parallel 2 |
| Reproduced intermittent history failure | **50 consecutive Linux `--loop-edits` runs PASS** with repair; exact assertions retained |
| Native WSL Debug ASan/UBSan/leaks `--loop-edits` | PASS, no diagnostics |
| Actual Windows held-worker Q/nudge pointers | PASS at 64/128; native dispatch observed before/after pause, exact Start and continuing nonzero PCM |
| Integrated Windows pointer/PCM workflow | PASS at 64/128: main/holds, Start/Q/nudge, Speed/reset, RTRG/STUT, MULT/history, LIVE, waveform export and MERGE; no physical device opened |
| Standalone CLI/WebView smoke before DSP-only repair | PASS; same processor generation, original controls, current bridge, no product AudioContext |

Logs: `build/loop-buttons-wait-*` (initial dispatch repair),
`build/loop-buttons-roundoff-*` (final common DSP acceptance). The first UI spy
attempt incorrectly replaced frozen `AionNative.request`; the corrected test
observes actual JUCE backend invocations and retains both zero-before-resume and
exactly-one-after assertions. Its failed log remains
`build/loop-buttons-wait-ui-instrumentation-failure-windows.log`. Original failed
Linux edit and diagnostic logs remain; no tolerance was relaxed or case removed.

### Verified follow-up: rapid accepted history/MULT gestures

A failing production-resolver regression (`build/loop-buttons-rapid-repro-windows.log`)
showed that accepted UNDO/REDO or a second MULT could disappear when dispatched
behind a worker edit. Their resolvers now wait before deciding current history
or PCM eligibility; global history waits for the bank's finalizers before choosing
its current target. CLI extends its same admission policy to these actions;
global waits fence the bank's clear epochs. No DSP or state schema changed.

All original eligibility combinations remain covered: current finalizing controls
stay disabled, but an earlier accepted click survives until dispatch. Real FIFO
unit checks cover latest history target/depth, tracks 0/19, DEL/panic cancellation
and eleven CLI action kinds, including a global target on another track.
Actual Windows 64/128 pointers queue MULT/UNDO, NORM/REDO, repeated MULT and global
history behind a real held worker. The first pointer fixture had no valid global
history: original MULT creates local history only. It now uses a real trusted
keyboard/mouse overdub to seed global history, preserving that reference rule;
failed evidence is retained as `build/loop-buttons-rapid-ui-fixture-failure-windows.log`.
Implementation `f92c791`. Windows/Linux Standalone builds, all **4/4** affected
frontend/bridge/CLI/asset CTests (12.83s /8.89s), and complete production processor
suites pass. Actual Windows 64/128 Q/nudge plus five rapid local/global history/
MULT pointer sequences pass. No assertions or original history semantics were
weakened. Logs: `build/loop-buttons-rapid-{frontend,production,ctest,ui}-*`;
Linux build log `build/loop-buttons-rapid-build-linux.log`.

Reproduce with existing per-OS caches (CMake/CTest resolution is retained below):

```powershell
cmake --build build/windows --config Release --target AionLooper_Standalone aion_mouse_host aion_midi_processor_tests --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -FinalizingControlsOnly
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(cli_finalization_admission|native_frontend_commands|native_bridge_lifecycle|original_ui_bytes)$'
```

```sh
cmake --build build/linux --target AionLooper_Standalone aion_midi_processor_tests --parallel 2
./build/linux/aion_midi_processor_tests
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R '^(cli_finalization_admission|native_frontend_commands|native_bridge_lifecycle|original_ui_bytes)$'
```

For the DSP repair, build `aion_core_tests aion_start_probe aion_reference_probe`,
run `aion_core_tests --start-phase`, `--loop-edits` and the full executable.
Its 17-test CTest selection is
`^(cli_finalization_admission|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_(start_phase|queued_early_restart|queued_early_rtrg|queued_history|multiply_worker_metadata|multiply_source_controls|queued_multiply_sources|queued_multiply|multiply|reverse|history|normalization|sync))$`.
The integrated pointer regression command was `./tools/TestMouseWorkflow.ps1 -SkipMidi`.
ASan/UBSan used the existing `build/overlap-asan/aion_core_tests --loop-edits`,
with `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.

After all acceptance processes ended, the existing ignored warmed callback harness
was relinked against the current per-OS core libraries and run Windows then Linux.
Initial 512-frame capture, stopped doubling to 1024, constructed queued source,
no-input MULT/history and playback at 48kHz, forward/reverse, 512 warm callbacks;
worker settlement is outside measurement. No benchmark code changed.

| Observed maximum measured callback | 64 samples | 128 samples |
| --- | ---: | ---: |
| Windows Release | 87.8us | 230.6us |
| Native WSL Linux Release | 96.7us | 124.6us |
| Block deadline | 1333.33us | 2666.67us |

This single-loop observation is not a worst-case/full-capacity, long-session,
physical-latency or platform-ranking claim. Existing harness caches:
`build/postbirth-multiply-bench-{windows,linux}`; results and build logs:
`build/loop-buttons-final-bench-*`.

Common-DSP/reference/core/sanitizer evidence above is reused because this follow-up
changes admission only. Remaining limits: synthetic inputs are not physical MIDI,
driver latency or device evidence; Linux WebKit interaction/listening and broader
browser transition envelopes are not established. Worker/source pre-start tempo ordering is now verified above. Next core slice:
constructed queued-source MULT with live input and remaining merge eligibility. FX/presets and dry/wet SAVE follow the core work.

## Latest verified slice — Reverse/Start ordering during Multiply copying, 6 November 2026

Implementation `9887a0f`, test formatting `ee15161`, continuing `b32912b`.
A no-input MULT followed by a Reverse or Start metadata edit before worker-result
acceptance previously constructed its replacement from the **later** metadata.
The unchanged JS performs its doubling/restart synchronously, so following
metadata edits cannot rewrite that source. A reproduced 8kHz/17-frame forward
case differed by **0.18018454279563184**, against the unchanged **1e-6** limit:
`build/multiply-worker-metadata-repro-windows.log`.

MULT now captures two scalar metadata fields in its existing immutable job before
publication. Completion uses them to construct the reserved source, including
forward/reverse buffer leases and Start phase, without restoring them into the
user's current metadata. A later Apply uses the newer accepted controls. The
existing completion-now +5ms replacement, original volume grid, history, bounded
reservations/rollback and worker ownership remain. Stopped and before-birth paths
keep their existing behavior. No audio allocation/free, locks, I/O, full-clip work,
new queue or unbounded loop was introduced. No public ID/state schema, dependency,
frontend, preserved asset or logo changed; GUI/CLI retain one shared native action.

Acceptance covers Reverse alone, Start alone and both while a no-input Multiply
job is uncommitted. The replacement must use the accepted MULT metadata; newer
metadata must stay visible and apply to a following source. Current/old sources,
original silence grid, exact deadlines/PCM/history, cancellation and OOM safety
must remain intact. Speed/tempo and Apply received during the job are separate.

| Verification | Executed evidence |
| --- | --- |
| New source-order oracle | Windows/Linux **288/288 PASS each**, 576 sources, 4,032 observations, 7,087,512 predicted frames including 6,169,656 silent pre-grid frames; max absolute error **5.8649130496579716e-8**, unchanged **1e-6** tolerance on both OS |
| Native ownership/audio | Windows/Linux Release **240 cases each PASS**, including 48 new held-worker cases; tracks 0/19, reverse, 64/128, exact paired stereo and tracked zero callback new/delete. Full `--loop-edits` and `--meter-queued` PASS on both |
| Shared production processor | Both Standalone builds and full processor suites PASS, including eight new paired GUI/CLI metadata-ordering cases and existing external CLI/MIDI/state/timer checks, zero tracked processBlock new/delete |
| Actual interaction regression | Four Windows WebView2 synthetic-PCM MULT/Undo/Redo pointer cases PASS, reverse at 64/128, original grid silence/audibility; this does not force metadata arrival during the worker |
| Reference/regression integration | Windows/Linux **13/13 CTests PASS each**, parallel 2 (464.37s /683.41s). Includes old 384-source corpus, 384 post-commit control cases, pre-birth MULT and related phase/sync/history/NORM/reverse, frontend and asset checks |
| Sanitizers | Native WSL Debug **240 cases PASS** under ASan/UBSan/leak detection, no diagnostics |

Reproduce using existing caches (VS CMake/CTest resolution retained below):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe --parallel 2
foreach ($selection in '--postbirth-multiply','--loop-edits','--meter-queued') { & ./build/windows-core/Release/aion_core_tests.exe $selection; if ($LASTEXITCODE) { throw $selection } }
$metadataGate = '^(original_ui_bytes|native_frontend_commands|original_multiply_worker_metadata|js_native_(multiply_worker_metadata|multiply_source_controls|queued_multiply_sources|queued_multiply|multiply|reverse|history|normalization|start_phase|sync))$'
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R $metadataGate
cmake --build build/windows --config Release --target AionLooper_Standalone aion_midi_processor_tests aion_mouse_host --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -PostbirthMultiplyOnly
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
for selection in --postbirth-multiply --loop-edits --meter-queued; do ./build/linux/aion_core_tests "$selection" || exit; done
./build/linux/aion_midi_processor_tests
metadata_gate='^(original_ui_bytes|native_frontend_commands|original_multiply_worker_metadata|js_native_(multiply_worker_metadata|multiply_source_controls|queued_multiply_sources|queued_multiply|multiply|reverse|history|normalization|start_phase|sync))$'
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R "$metadata_gate"
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --postbirth-multiply
```

The new oracle can run alone with `node tests/QueuedMultiplySources.cjs --worker-metadata --native PROBE`;
omit `--native PROBE` for explicit reference-only assertions.
Final logs are under `build/multiply-worker-metadata-*`: per-OS `core`,
`production`, `ctest` and preserved raw `regression-results`; Windows
`serial-build`, `production-build`, `ui`, `loop-edits`, `meter-queued`; Linux
`build`, `edits`, `queued`, `asan-build`, `asan`. Both `format-build` logs verify
the formatting-only test fix; unchanged passing runtime suites were not rerun
for line breaks. Verification ran concurrently; its durations are not runtime
or comparative build-performance measurements.

After all verification jobs stopped, the existing ignored warmed benchmark was
rebuilt against the current per-OS core libraries and run Windows then Linux.
48kHz stereo, one 512-frame loop, 512 warm callbacks; queued graph birth, no-input
MULT admission, worker settlement **outside measurement**, Undo/Redo/Undo, playback
through the original grid and clear. Only Engine.process is timed; no forced
worker delay or metadata interaction is simulated by this performance harness.

| Platform | Maximum measured MULT admission | Maximum any measured callback, 64 /128 samples |
| --- | ---: | ---: |
| Windows Release | 69.8us | 90.7us /115.9us |
| Native WSL Linux Release | 109.817us | 89.1us /167.659us |

Block deadlines are 1333.33us /2666.67us. These observations do not establish
worst-case/full-capacity or long-session performance, worker/command latency,
physical round-trip latency or OS rankings. Existing harness paths:
`build/postbirth-multiply-bench-src`, `build/postbirth-multiply-bench-{windows,linux}`;
current results `build/multiply-worker-metadata-bench-{windows,linux}.jsonl`.
No benchmark source changes or distributables were needed.

`QueuedMultiplySources.cjs --worker-metadata` adds three groups of 96 cases,
executing unchanged synchronous MULT then metadata writes. Native commands are
sent before `wait`/beginBlock accepts the worker result at the same sample frame.
It checks the source model at mono/stereo, 8/48/96kHz, lengths 1/17/512/4097,
forward/reverse and early/1ms-before-grid. It does not emulate copying latency,
clock advancement during the copy, browser rendering, envelopes or resampling.
The 48 new Engine cases hold both workers outside the callback and advance audio
before/across the original grid. Only the observed engine receives metadata
edits; all stereo samples must equal an unchanged-control engine through commit,
replacement and playback. Updating the control engine before a later Apply then
checks that the newer metadata was preserved. `--postbirth-multiply` now runs
240 cases (48 new plus all 192 previous lifecycle/control/capacity cases).

Eight new production cases use shared GUI/CLI RPC on two real processors. MULT
and metadata enter one live callback; no worker result is accepted between those
commands. The existing explicit offline-render mode settles both jobs at the
same next sample boundary for exact paired PCM/deadline comparison. This uses no
product hook and does not add live audio waiting. Existing external CLI/state/
MIDI/timer regressions remain in the complete processor suite. Actual pointer
checks retain their narrower MULT/Undo/Redo scope; they do not force this race.

Initial iteration evidence is retained: an accidental old-corpus invocation after
a sandbox write failure is **not** the new negative; the separately named repro
above is. Two overlapping MSBuild invocations caused an object-write conflict;
both finished, MeterBufferTests.cpp was rebuilt serially, and only the final build/
runtime results count. Four new test-loop indentation warnings were corrected
without changing behavior/assertions; exact-float assertions intentionally retain
the test target's existing float-equality warning policy. No cases or tolerances
were removed/loosened. No distributable or push.

Next: define and verify worker-time Apply/Speed ordering and tempo changes during
the worker or pending replacement. Then port input-present MULT on constructed
grid-gated sources. Input-capture/pending-stop guards remain. Full browser audio,
Linux WebKit interaction, real worker/command latency, listening/devices and long
sessions/full-capacity performance remain unverified; wrappers/DAWs/Apple deferred.

## Previous verification slice — controls after Multiply source construction, 5 November 2026

Test snapshot `8af29d7`, coverage-assertion correction `c9861d9`, continuing
`2bc723f`. This is regression coverage of the existing implementation, not a new
DSP/UI feature. The slice covers the interval **after worker commit**, when the
replacement source has already been constructed but its 5ms start is pending.
Observable acceptance: Reverse/Start metadata cannot rewrite captured source PCM,
direction or offset; Speed targets the newest source's AudioParam timeline;
a later Apply captures the new metadata/control. Original grid silence, source
stop deadlines, history and paired Engine stereo must remain intact.

`QueuedMultiplySources.cjs --controls` adds 384 unchanged-JS/native cases: four
96-case groups (metadata, metadata/Speed/Apply, Speed before start and Speed at
its rounded native deadline), mono/stereo, 8/48/96kHz, lengths 1/17/512/4097,
forward/reverse and early/1ms-before-grid. It executes the original setters,
Loop.multiply/restart and AudioGraph methods. The source model now integrates
per-node setValueAtTime events in timestamp order, including same-time writes;
it does not emulate browser resampling, envelopes or tempo smoothing. The old
384-case corpus remains intact and is rerun because that model changed.

The Engine suite adds 32 cases (tracks 0/19, reverse, early/near grid, 64/128,
metadata with/without Speed). Only the observed engine receives the late controls;
its entire stereo output must equal the control engine through and beyond the
original grid. Together with the existing 160 cases, `--postbirth-multiply` runs
192 cases and retains delayed-worker, cancellation, OOM and capacity coverage.
No engine, frontend, bridge, command IDs, state schema, pinned dependencies,
preserved assets or logo changed. Existing GUI/CLI access and prior real-pointer
checks are retained; no new UI, driver, DAW or physical-device pass is claimed.

| Verification | Executed evidence |
| --- | --- |
| New source-control corpus | Windows/Linux **384 PASS each**, max absolute error **5.8649130496579716e-8** on both, unchanged **1e-6** tolerance |
| Existing source corpus after model update | Windows/Linux 384 PASS each; unchanged maximum **8.277312368853984e-8**, same 1e-6 tolerance and all original overlap/history/NORM cases |
| Engine control/ownership | Windows/Linux Release **192 cases each PASS**, including 32 new control cases, exact paired stereo and zero tracked callback new/delete |
| Sanitizers | Native WSL Debug **192 cases PASS** under ASan/UBSan/leak detection, no diagnostics |
| Assets/reference fixtures | Original asset bytes and both 384-case reference-only groups PASS on both OS; initial aggregate-coverage failures and corrected reruns retained |

Reproduce with the existing caches (VS CMake/CTest resolution retained below):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe --parallel 2
./build/windows-core/Release/aion_core_tests.exe --postbirth-multiply
node tests/QueuedMultiplySources.cjs --controls --native ./build/windows-core/Release/aion_start_probe.exe
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|original_multiply_source_controls|original_queued_multiply_sources|js_native_queued_multiply_sources)$'
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe --parallel 2
./build/linux/aion_core_tests --postbirth-multiply
node tests/QueuedMultiplySources.cjs --controls --native ./build/linux/aion_start_probe
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R '^(original_ui_bytes|original_multiply_source_controls|original_queued_multiply_sources|js_native_queued_multiply_sources)$'
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests --postbirth-multiply
```

The new native comparison is also registered as `js_native_multiply_source_controls`.
Per-OS logs: `build/multiply-controls-{build,core,source,ctest,fixture-rerun,regression-results}-{windows,linux}.log`;
Windows initial direct failure `build/multiply-controls-initial-windows.log`;
Linux `build/multiply-controls-{asan,asan-build}-linux.log`.
Windows initial CTest had 3 passes/1 fixture failure, Linux 3 passes/2 fixture
failures (reference and native aggregate). Corrected reference-only reruns pass;
final native results above are separate full direct runs, not inferred from the
initial comparisons. Passing unchanged old corpus/assets were not rerun again.
Concurrent verification timings are not runtime or build-performance benchmarks.
No production/Standalone rebuild or UI rerun is needed for these test-only edits;
prior evidence remains scoped to the previous implementation checkpoint.

The initial new corpus incorrectly inherited the old Apply-chain assertion that
some sources must overlap. Sequential MULT/Apply sources instead stop exactly at
the next source birth. The corrected control group asserts **zero** overlap; the
old corpus still requires nonzero overlap. No case, audio assertion or tolerance
was removed/loosened. Initial failures are retained separately from passing reruns.

Next: characterize Reverse/Start/speed and tempo changes **while the worker is
still copying**, including completion ordering and controls received across the
original grid. Tempo updates during a pending replacement also remain unverified.
Then port input-present MULT on constructed grid-gated sources; keep that guard.
Listening, Linux WebKit, devices, renderer/latency and long sessions remain open.
No package, push or release claim.

## Previous implementation slice — no-input Multiply after source construction, 5 November 2026

Implementation `8ad8761`, portable warning fix `5c97414`, continuing `3a52a52`.
When native input is unavailable, original MULT/keyboard and running-Standalone
CLI `loop multiply TRACK` now double a constructed queued source without moving
its original audible grid. Stored audio/history changes on worker completion;
existing sources retain their own buffers, lengths/rates and independent stops.
A replacement begins at completion audio-now +5ms. Playing no-input doubling
uses that same original timing, correcting an immediate-restart defect exposed
when rapid doubles crossed the grid. Stopped/pre-birth doubling is unchanged.
Queued input capture and pending-stop queued MULT remain separate guarded scope.
No public IDs, parameters/schema, capacity, pinned dependencies, preserved UI/
reference assets or ASCII logo changed. C++ remains the sole audio/state owner.

The existing worker prepares PCM. Completion reserves all phase/overlap capacity
before installing storage/history, pins old forward sources and retains reversed
snapshots with existing logical buffer versions. Reservation/OOM failure keeps old
PCM/history and sets `finalizationFailed`; successful command admission alone is
not proof of completed editing. Stop/clear/cleanup/prepare retire source/event
leases through the worker. Diff review found no new callback allocation/free,
locks, I/O or unbounded work; existing retirement-overflow retention until prepare
is unchanged. The 160-case allocation guard tracks callback new/delete at zero.

| Acceptance | Executed evidence / limit |
| --- | --- |
| Unchanged no-input source behavior | **384 native/reference cases on each OS**: mono/stereo, 8/48/96kHz, lengths 1/17/512/4097, forward/reverse, early/1ms-before-grid; double/cleanup, rapid doubles, two pending Applies, double/Undo/NORM |
| Source PCM / exact deadlines | 1,152 sources; 7,296 observations; 9,450,016 source-sum frames, including 8,226,208 silent pre-grid frames, 107,008 audible overlap samples and 1,107,056 distinct-buffer samples. Max absolute error **8.277312368853984e-8** on both OS, unchanged **1e-6** tolerance; exact 5ms replacement checks |
| Native ownership/lifecycle | **160 cases per OS**, tracks 0/19, reverse, 64/128: rapid/history/NORM/Apply, delayed worker across grid with old-source playback, stop/panic/clear/cleanup/same/new-rate prepare before/after commit, OOM, full pool/event/overlap rollback, input/varispeed guards; exact paired stereo and tracked zero callback new/delete |
| Existing regressions | Both OS full `--loop-edits` and `--meter-queued` PASS; all prior queued history/NORM/capacity/FIFO/cancellation coverage retained |
| Production GUI/CLI | Windows/Linux Standalone and full processor suites PASS, including eight new no-input shared-RPC forward/reverse early/near-grid MULT/Undo/Redo cases at 64/128, 5ms metadata, doubled/restored length/depth, silence/audibility, existing actual external Python CLI routing and MIDI/state/timer checks. New source timing uses shared RPC, not a separate external-process CLI source oracle |
| Actual original controls | Four Windows WebView2 synthetic-PCM cases: trusted no-input MULT/UNDO/REDO pointers, exact history/length, original pre-grid silence and later audible PCM, reverse, 64/128. Owned test host switches input bus only on message thread after audio pause acknowledgement, then restores it; no product RPC/parameter/hardware changes. 176 frontend eligibility states and retained shortcut/repeat cases PASS |
| Integration comparisons | Windows **16/16 CTests PASS**, 212.25s, plus separately executed native 384 corpus; Linux **17/17 PASS**, 676.41s, including native 384 corpus (342.69s). Parallel 2 and concurrent verification workloads; these times are not comparative runtime/build benchmarks |
| Sanitizers | Native WSL Debug ASan/UBSan/leaks: 160 new +108 pre-birth MULT +124 after-birth history +64 before-birth history +96 pre-birth NORM =**552 enumerated cases**, plus `--loop-edits`, all PASS without diagnostics |
| Remaining validation | Browser renderer/envelopes/resampling, worker/command latency, control/tempo interleavings during delayed work or pending replacement, listening, physical audio/MIDI devices, Linux WebKit interaction and long sessions/full-capacity performance remain open. Deferred wrappers/DAWs/Apple not built/tested; no release readiness claim |

Baseline rejection and the initial rapid near-grid deadline failure are retained
in `build/postbirth-multiply-negative-windows.log` and
`build/postbirth-multiply-rapid-deadline-negative-windows.log`. The first narrower
96-case pass did not establish rapid behavior: expanding the corpus exposed the
playing path's deadline zero versus required 5ms (3545 in the reproduced 8kHz
case). Correcting that path makes the full 384 pass. No cases/tolerances removed
or loosened. Linux initially failed `-Werror=misleading-indentation`; splitting
the guarded statement from following assignments fixes it without behavior or
warning-policy changes. Windows targets rebuilt successfully with that portable
fix; unchanged passing runtime/UI suites were not repeated for a line break.

After verification jobs stopped, the same ignored warmed harness ran Windows then
Linux: 48kHz stereo, one 512-frame loop doubled while stopped, 512 warm callbacks,
explicit queued source birth, no-input MULT admission, worker settlement outside
measurement, Undo/Redo/Undo, playback through original grid and clear. Only
Engine.process is timed; no forced worker delay in this harness.

| Platform | Maximum measured MULT admission callback | Maximum any measured callback, 64 /128 samples |
| --- | ---: | ---: |
| Windows Release | 97.5us | 133.3us /222.2us |
| Native WSL Linux Release | 42.376us | 100.252us /118.046us |

Block deadlines: 1333.33us /2666.67us. These observations do not establish
worst-case/full-capacity/long-session continuity, edit/command/worker latency,
physical round-trip latency or OS rankings. Raw harness/results:
`build/postbirth-multiply-bench-src`, `build/postbirth-multiply-bench-{windows,linux}.jsonl`.
Binaries: `build/postbirth-multiply-bench-windows/Release/queued_bench.exe` and
`build/postbirth-multiply-bench-linux/queued_bench`.

Reproduce with existing caches (VS CMake/CTest resolution retained below):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe --parallel 2
foreach ($selection in '--postbirth-multiply','--loop-edits','--meter-queued') { & ./build/windows-core/Release/aion_core_tests.exe $selection; if ($LASTEXITCODE) { throw $selection } }
node tests/QueuedMultiplySources.cjs --native ./build/windows-core/Release/aion_start_probe.exe
$postbirthGate = '^(original_ui_bytes|native_frontend_commands|original_queued_multiply_sources|js_native_(queued_multiply|queued_history|multiply|history|normalization|queued_prebirth_norm|queued_prebirth_late_norm|queued_source_norm|queued_source_norm_phase|sync|reverse|meter_graph|start_phase))$'
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R $postbirthGate
cmake --build build/windows --config Release --target AionLooper_Standalone aion_midi_processor_tests aion_mouse_host --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -PostbirthMultiplyOnly
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
for selection in --postbirth-multiply --loop-edits --meter-queued; do ./build/linux/aion_core_tests "$selection" || exit; done
./build/linux/aion_midi_processor_tests
postbirth_gate='^(original_ui_bytes|native_frontend_commands|original_queued_multiply_sources|js_native_(queued_multiply_sources|queued_multiply|queued_history|multiply|history|normalization|queued_prebirth_norm|queued_prebirth_late_norm|queued_source_norm|queued_source_norm_phase|sync|reverse|meter_graph|start_phase))$'
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R "$postbirth_gate"
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
for selection in --postbirth-multiply --prebirth-multiply --postbirth-history --queued-history --prebirth-norm --loop-edits; do env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests "$selection" || exit; done
```

Final evidence: `build/postbirth-multiply-core-windows.log`, Linux
`build/postbirth-multiply---postbirth-multiply-linux.log`; per-OS
`build/postbirth-multiply---{loop-edits,meter-queued}-{windows,linux}.log`,
`build/postbirth-multiply-production-{windows,linux}.log`,
`build/postbirth-multiply-ctest-{windows,linux}.log`, preserved raw CTest
`build/postbirth-multiply-regression-results-{windows,linux}.log`.
Windows direct corpus `build/postbirth-multiply-source-windows.log`, UI
`build/postbirth-multiply-ui-windows.log`, final build
`build/postbirth-multiply-portable-build-windows.log`;
Linux `build/postbirth-multiply-asan-linux.log` and initial compile failures
`build/postbirth-multiply-{initial-werror,asan-initial-werror}-linux.log`.
No package or push; Windows remains publishing owner, WSL receives guarded snapshots.

Next: characterize control/tempo ordering (Reverse, Start, speed) during delayed
no-input worker completion and the pending 5ms replacement before claiming those
interleavings. Then port input-present MULT on a constructed but grid-gated source,
including <128-frame discard, append/history, original gate/restart and cancellation.
Keep that capture guard until its own end-to-end acceptance passes.

## Previous verified slice — queued Multiply before source construction, 5 November 2026

Implementation `f226c56`, continuing `9c4717b`. Original MULT/keyboard and running
Standalone CLI `loop multiply TRACK` now double a queued loop before its source
callback constructs playback. Stored PCM doubles, local Undo gains one entry and
Redo clears; the immutable PLAY receipt, callback deadline and original grid stay
intact. Repeated doubling and Undo/Redo share the existing native action/history.
After-birth queued MULT and pending-stop queued MULT remain separate, disabled
scope. No public IDs, parameters, persistence schema, expanded capacity, pinned
dependencies, preserved UI/reference assets or ASCII logo changed.

Doubling remains on the existing worker. One fixed-size audio-owner slot defers
an arriving conditional graph request until its outcome; it does not block audio
or the performance FIFO. Completion publishes the PCM continuation before the
validated graph result. Pending duplicate identities are idempotent; malformed or
conflicting receipts cannot overwrite the slot. Source construction uses current
committed PCM/length/direction/rate. Cancellation, old cleanup, generation loss
and prepare cannot revive playback; OOM retains old PCM/history and capacity
rejection happens before mutation. No callback allocation/free/locks/I/O added.
Final diff review covered slot lifetime, bounded work and cancellation/ownership.

| Acceptance | Executed evidence / limit |
| --- | --- |
| Original queued doubling, history, grid and source PCM | **192 unchanged-JS/native cases per OS**, mono/stereo, 8/48/96kHz, lengths 1/17/512/4097, forward/reverse, early/late birth, single/repeated doubling and Undo/Redo; 8,484,512 compared channel samples, including pre-grid silence across 4,224,976 frames |
| Error / tolerance | Maximum absolute PCM error **4.4128859011394184e-8** on Windows and Linux; unchanged **1e-6** tolerance. Exact reference channel-copy/history assertions also pass. Source/interpolation model excludes browser renderer/envelopes/resampling and asynchronous worker/command latency |
| Native worker/ownership boundaries | **108 cases per OS** at tracks 0/19, forward/reverse, 64/128: before/in-flight/late callback, exact original deadline/grid, doubled length/depth, paired stereo, duplicate and conflicting full-width receipts, stop/panic/clear/cleanup, same/new-rate prepare, OOM, full pool, FIFO retry, stream loss and recovery; zero tracked callback new/delete |
| Existing regressions | Both OS full `--loop-edits` and `--meter-queued` PASS, retaining before/after-birth history, NORM, Apply/RTRG, capacity, FIFO, cancellation and OOM coverage |
| Production GUI/CLI | Both Release Standalone builds and complete production processor suites PASS, including eight new shared-RPC forward/reverse early/late MULT/Undo/Redo cases across 64/128, existing external Python CLI routing, state/MIDI/timers and zero tracked processBlock new/delete; no separate external-process queued-MULT source oracle |
| Original controls | Four actual Windows WebView2 synthetic-PCM cases: trusted queued MULT/UNDO/REDO pointers, doubled/restored length and history, unchanged original grid, automatic queued callback, forward/reverse silence then nonzero audio at 64/128; 88 frontend MULT eligibility cases and existing unchanged shortcut/repeat cases PASS |
| Selected reference/frontend/assets | **16/16 CTests PASS per OS**, parallel 2. Windows total 253.33s (new source gate 93.28s); native WSL Linux 397.21s (146.54s). These verification workloads are not comparative runtime/build benchmarks |
| Sanitizers | Existing native WSL Debug ASan/UBSan/leak cache: 108 Multiply +124 after-birth history +64 before-birth history +96 before-birth NORM =**392 enumerated cases**, plus full `--loop-edits`, all PASS without diagnostics |
| Outstanding release validation | Listening, physical audio/MIDI devices, browser renderer/envelopes/resampling, Linux WebKit interaction, worker/command latency and long sessions remain open. Deferred wrappers/DAWs/Apple were not built or tested. No distributable/release claim |

The reference gate first reproduced rejection at baseline (`build/queued-multiply-negative-windows.log`).
No old cases were removed or tolerances loosened. New paired cleanup fixtures
retire the same ticket on both engines; stream-loss recovery drains invalidated
facts before prepare, preserving the existing queue-index/ownership contract.
A stale MSVC dispatch object initially launched the default suite: that run was
interrupted, CoreTests.cpp explicitly rebuilt, and only final focused/regression
results above count. Initial sandboxed UI failed before Runtime.enable; the exact
owned test host was closed and all four cases rerun successfully outside the
sandbox. Failed/interrupted evidence is retained separately, not counted as PASS.

After all verification jobs stopped, the same ignored warmed harness ran Windows
then Linux: 48kHz stereo, one 512-frame loop, 512 warm playing callbacks, queued
MULT admission, worker settlement outside measurement, graph birth, Undo/Redo/Undo,
playback through the original grid and clear. Only Engine.process is timed.

| Platform | Maximum measured MULT admission callback | Maximum any measured callback, 64 /128 samples |
| --- | ---: | ---: |
| Windows Release | 68.6us | 84.3us /193.3us |
| Native WSL Linux Release | 49.477us | 111.074us /135.712us |

Block deadlines are 1333.33us /2666.67us. These single-loop samples do not establish
worst-case/full-capacity/long-session continuity, worker completion or command
latency, physical round-trip latency or OS rankings. The harness does not force
worker delay; deterministic in-flight behavior is covered by the 108-case suite.
Ignored harness/results: `build/queued-multiply-bench-src`,
`build/queued-multiply-bench-{windows,linux}.jsonl`; binaries under
`build/queued-multiply-bench-windows/Release/queued_bench.exe` and
`build/queued-multiply-bench-linux/queued_bench`.

Reproduce with existing caches (Windows VS CMake/CTest resolution retained below):

```powershell
cmake --build build/windows-core --config Release --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe --parallel 2
foreach ($selection in '--prebirth-multiply','--loop-edits','--meter-queued') { & ./build/windows-core/Release/aion_core_tests.exe $selection; if ($LASTEXITCODE) { throw $selection } }
$multiplyGate = '^(original_ui_bytes|native_frontend_commands|original_queued_multiply|js_native_(queued_multiply|queued_history|multiply|history|normalization|queued_prebirth_norm|queued_prebirth_late_norm|queued_source_norm|queued_source_norm_phase|sync|reverse|meter_graph|start_phase))$'
ctest --test-dir build/windows-core -C Release --parallel 2 --no-tests=error --output-on-failure -R $multiplyGate
cmake --build build/windows --config Release --target AionLooper_Standalone aion_midi_processor_tests aion_mouse_host --parallel 2
./build/windows/Release/aion_midi_processor_tests.exe
./tools/TestMouseWorkflow.ps1 -QueuedMultiplyOnly
```

```sh
cmake --build build/linux --target aion_core_tests aion_start_probe aion_reference_probe aion_meter_graph_probe aion_midi_processor_tests AionLooper_Standalone --parallel 2
for selection in --prebirth-multiply --loop-edits --meter-queued; do ./build/linux/aion_core_tests "$selection" || exit; done
./build/linux/aion_midi_processor_tests
multiply_gate='^(original_ui_bytes|native_frontend_commands|original_queued_multiply|js_native_(queued_multiply|queued_history|multiply|history|normalization|queued_prebirth_norm|queued_prebirth_late_norm|queued_source_norm|queued_source_norm_phase|sync|reverse|meter_graph|start_phase))$'
ctest --test-dir build/linux --parallel 2 --no-tests=error --output-on-failure -R "$multiply_gate"
cmake --build build/overlap-asan --target aion_core_tests --parallel 2
for selection in --prebirth-multiply --postbirth-history --queued-history --prebirth-norm --loop-edits; do env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/overlap-asan/aion_core_tests "$selection" || exit; done
```

Final logs: Windows `build/queued-multiply-core-final-windows.log`; Linux
`build/queued-multiply---prebirth-multiply-linux.log`; per OS
`build/queued-multiply---{loop-edits,meter-queued}-{windows,linux}.log`,
`build/queued-multiply-production-{windows,linux}.log`,
`build/queued-multiply-ctest-{windows,linux}.log`, and preserved raw oracle
`build/queued-multiply-source-results-{windows,linux}.log`.
Windows `build/queued-multiply-ui-final-windows.log`; Linux
`build/queued-multiply-asan-linux.log`. No package or push; Windows remains
publishing owner and WSL receives guarded committed snapshots.

Historical next step (no-input branch completed above; input capture remains pending): characterize **MULT after source construction but before the
original audible grid**. Original Loop is then playing and may capture input;
compare its source/buffer/stop/record/history behavior before changing the native
queued guard. Keep capture, varispeed and fallback boundaries explicit; do not
simply enable the control or replace the independent source with latest storage.

## Previous verified slice — queued history after source creation, 5 November 2026

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

Historical next step (completed by the slice above): characterize **queued Multiply before source construction**.
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
