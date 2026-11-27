# B / convolution reverb migration

## Owner-approved unified Reverb direction — 24 November 2026

Combine the future convolution and machine/plate-facing controls into **one
Reverb effect**, retaining **B** as the visible letter rather than adding a
second V button for new workspaces. This is a product plan change, not an
implemented second engine or permission to expose a nonfunctional toggle.

- One Reverb panel has an explicit **Convolution / Machine** engine toggle.
  Plate-oriented native models may use a descriptive label once the actual
  model is selected and verified; the original Machine is a modulated-feedback
  convolution network, not proof of a physical plate implementation.
- Presets carry their engine selection. Choosing a preset selects that engine
  and its parameter set; manual switching restores the other engine's last
  settings. Keep separate per-engine values, preset identity and controls rather
  than mapping unlike Decay/Duration/Feedback/Warp ranges by guesswork.
- Preserve existing convolution preset names/IDs/values, including `plate` and
  `bright-plate`. Their names alone must not change their engine. Original
  Machine presets retain their own identity and values in a distinct preset
  family; display grouping can distinguish the two families in one selector.
- Keep legacy B/V codes, order, independent active flags and saved settings
  readable. Do not remove V from the parser or silently collapse an older chain
  containing both effects: two independently ordered stages are not equivalent
  to one selected engine. Retain that data until lossless versioned conversion
  and its legacy presentation are implemented and tested.
- Switching engines must use prepared, memory-admitted DSP and a bounded
  crossfade/retirement path. Do not allocate, rebuild an IR or destroy a kernel
  on audio. Retain each engine's settings, not a second audio renderer kept alive
  indefinitely without a resource decision.
- GUI and running-app CLI use the same engine-selection command/state;
  persistence and MIDI mapping retain stable IDs and explicit engine ownership.
  No new command, state field or mapping is advertised until connected.
- Keep all author/source/license credits for both selected implementations and
  any imported presets/IRs. The machine/plate backend remains to be selected,
  credited, implemented and validated under the native-quality owner policy.

Acceptance adds preset-driven selection, manual switching and round-trip per-
engine settings, save/reload/editor reopen, rapid/cancelled switches, OOM,
legacy B-only/V-only/B+V preservation and sample-rate/block-size changes at
64/128. User reorderable chains remain user reorderable. Existing verified
convolution DSP and current application behavior stay unchanged in this planning
update. Both product reverb controls remain unavailable in the present preview.

Current scope: settings/IR preparation, uniform/non-uniform convolution and the
prepared stereo wet processor below are internal components. B remains disabled
in the product until graph ownership and shared GUI/CLI/state/MIDI integration
meet their gates. No listening/device/release acceptance is implied.

## Offline preparation foundation — 24 November 2026

This is an internal component stage, **not a working product reverb**. B remains
disabled. No GUI/CLI/audio routing, MIDI targets, APVTS parameters or persistence
schema are added yet. The `catalog` test probe is a developer diagnostic, not a
running-Standalone command. Those product paths must share the eventual native
processor; do not expose disconnected controls or a second audio engine.

### Original control contract

`ReverbSettings` retains six controls in original widget order: Volume .1–3,
Mix 0–1, Duration .1–5 seconds, Decay .1–5, Damp 500–20000 Hz and Pre-delay
0–.25 seconds. Defaults, steps, eight room labels and all 17 preset names remain.
Room selection resets only Duration/Decay. Presets apply room first, then explicit
overrides: e.g. bright-plate uses plate Decay 1.2 with Duration 2; tight-drum uses
small Decay .8 with Duration .4. Numeric edits select Custom even at equal values;
room/preset changes preserve enable. Invalid edits preserve the entire old value.

The unchanged original `update`, `updateReverbRoom`, `applyGenericPreset` methods
are executed against three prior states. The preserved definitions/methods are
byte-checked against `.reference/Ascii-Looper`; the native catalogue matches 93
room/preset/manual-routing cases. This proves settings semantics, not sound.
Native validation additionally rejects unknown rooms and inconsistent preset IDs.

### Native impulse model, preparation version 1

The remaining-effects quality policy favors an organic native model over identical
browser PCM. This first model is a deterministic dense-noise stereo impulse,
not a measured room or physical plate/spring model. Room names retain their
original Duration/Decay mapping, not an invented acoustic simulation claim.

- Explicit 32-bit seed, independent channel streams and reproducibility on the
  same build/platform. Do not promise cross-platform libm bit identity.
- Exponential envelope `exp(-(1.5 + 1.5 * decay) * frame / length)`: the entire
  Decay range has an effect, with higher values tightening the tail.
- Raised-cosine edges, up to 10ms (bounded to one quarter of the impulse), with
  exact zero endpoints. Weighted DC removal retains the decay and edge envelope.
- Unit energy per channel, preventing raw gain from growing with length/rate.
  An eventual convolution backend must not silently normalize it a second time.
- IR key: sample rate, Duration, Decay and seed. Repeated preparation reuses
  storage with no new/free; Volume/Mix/Damp/Pre-delay/enable are downstream live
  controls and do not redefine this IR. No new product controls are introduced.

`ReverbImpulse::prepare` is worker-only. Never mutate/prepare/destroy a published
impulse concurrently with audio reads. A replacement is built privately and
published only after both channels succeed. Validation, OOM at either channel,
and cancellation preserve all old stereo PCM and metadata. Cancellation is
checked before allocation, every 1024 samples in each preparation pass and
before publication; a later cancellation still needs an owner/adoption fence.
The optional cancellation atomic must outlive the synchronous preparation call.

Rates are bounded to 8–384kHz; length is `floor(rate * duration)`. Maximum raw IR
storage is 1,920,000 stereo frames / 15,360,000 bytes. Held old + candidate raw
storage may require twice that. `requiredBytes` counts coefficient storage only:
future FFT plans, spectra, overlap, transitions and the global 20-track pool need
their own reservation/budget before preparation. There is no connected ownership
queue/pool or callback convolution in this stage.

### Earlier backend research (preparation checkpoint)

The existing pinned [JUCE convolution API](https://docs.juce.com/master/classjuce_1_1dsp_1_1Convolution.html)
offers stereo partitioned/non-uniform processing and explicit load/process
threading requirements. Its `prepare` makes the latest loaded IR ready before
processing. The [MIT FFTConvolver implementation](https://github.com/HiFi-LoFi/FFTConvolver/tree/non-uniform)
offers a pure C++ two-stage alternative with an included FFT. At the earlier
preparation checkpoint no source was copied or dependency added. The kernel
slice below now reuses its pinned AudioFFT subset.

That checkpoint's next step was to audit process/reset/destruction and verify
convolution against independent short direct-FIR and long impulse/tail oracles.
Require stereo/mono isolation, declared latency, variable blocks including 64/128,
zero callback allocations/locks, bounded reset and worker-only retirement.
Per-loop preparation/adoption still needs explicit OOM/cancellation outcomes,
live smoothing/crossfades, user-reorderable B routing, versioned settings and
shared GUI/CLI/MIDI access. Current presets, IDs, logo and other effects remain.

### Convolution kernel slice — 24 November 2026

Acceptance: independent double-precision direct FIR within absolute `3e-6`,
zero algorithmic latency, unchanged IR gain/quiet tail coefficients, channel
isolation, 64/128/variable/in-place blocks, complete tail and ring wrap, reset
without stale history, atomic OOM/cancel/invalid preparation, no callback new/free,
and no callback locks/I/O or IR-length clearing. B is still disabled pending the
remaining wet path, preparation ownership and shared GUI/CLI/state integration.

Reused **AudioFFT** from HiFi-LoFi/FFTConvolver, pinned commit
`f2cdeb04c42141d2caec19ca4f137398b2a76b85`. The three upstream files are unmodified,
vendored with hashes/MIT text and Ooura's author-published redistribution
permission. Only portable Ooura is compiled; no new platform runtime, FFTW,
JUCE dependency or version upgrade. Init builds tables off audio; audited
fft/ifft paths use preallocated arrays without locks/allocation/I/O. Source
provenance and audit are in `third_party/audiofft/README.md`.

Why a small Aion history layer: upstream FFTConvolver `reset` deletes spectra,
and its raw-pointer incremental initialization is not transactional on OOM.
JUCE's ready-made engine would add JUCE to the deliberately independent core;
its reset also clears input spectrum history proportional to IR size. The new
`PartitionedConvolver` reuses FFT mathematics but owns storage transactionally.
Its uniform 128-frame partitions cache previous-partition products; partial
current FFTs provide zero latency for short/irregular calls. No normalization
or near-zero coefficient trimming is applied. A saturating valid-history count
invalidates old spectra at reset; only fixed 128-frame overlap/256-frame scratch
is cleared. Preparation/destruction cannot overlap processing. There is no live
adoption thread/queue in this slice; that still needs generation fencing.

This uniform kernel is an internal correctness primitive, **not a claim of
acceptable 20-track long-reverb CPU**. Before connecting B, use a non-uniform
tail arrangement around the verified primitive (or justify another backend).
At the maximum 1,920,000 frames, four spectrum vectors hold 30,960,000 bytes per
mono instance, plus fixed scratch/state/FFT storage and allocator overhead.
Stereo doubles that; original IRs and old/candidate overlap are additional.
The future pool must reserve the whole graph, not only raw IR bytes.

Process accepts at most 4096 frames, exact in-place or disjoint spans, rejecting
invalid length/unprepared calls without state/output mutation. Input nonfinite
values become silence; only excessive finite values beyond +/-1e6 are limited.
Prepared IR coefficients must be finite and within +/-1e6. Normal audio and the
unit-energy generated IR are unaffected. There is no rate conversion in this
primitive: the worker must supply an IR at the engine sample rate.

Community research also considered [Dragonfly](https://github.com/michaelwillis/dragonfly-reverb)
(GPLv3, Freeverb3-based algorithmic reverbs with room/studio/hall preset banks)
and [Signalsmith Basics](https://github.com/Signalsmith-Audio/basics/blob/main/include/signalsmith-basics/reverb.h)
(MIT algorithmic reverb). Neither is an interchangeable convolution backend or
six-control preset bank. No code/presets from them are imported here. Preserve
the current 17 settings presets; community preset adaptation or measured IR
material needs explicit parameter mapping, per-asset rights and listening
acceptance, rather than relabeling another engine's settings as equivalent.

Focused commands (core-only; no product UI/CLI changes in this slice):

```powershell
cmake --build build/windows-core --config Release --target aion_convolution_tests aion_reverb_preparation_tests --parallel 4
ctest --test-dir build/windows-core -C Release -R '^(convolution_kernel_contract|reverb_preparation_contract|original_reverb_settings_contract)$' --output-on-failure --no-tests=error
```

After a coherent local commit and the documented WSL snapshot sync:

```sh
cmake --build build/linux --target aion_convolution_tests aion_reverb_preparation_tests --parallel 2
ctest --test-dir build/linux -R '^(convolution_kernel_contract|reverb_preparation_contract|original_reverb_settings_contract)$' --output-on-failure --no-tests=error
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_convolution_tests aion_reverb_preparation_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(convolution_kernel_contract|reverb_preparation_contract|original_reverb_settings_contract)$' --output-on-failure --no-tests=error
```

The generated-IR tests compare actual convolution output, unlike the earlier
immutable-reads-only checks. They cover eight rates, both channels at 64/128,
an independent short direct FIR oracle, a 65,537-frame dense tail, ring wrap,
an isolated `1e-8` final tap checked at `1e-13` to detect trimming, silent IRs,
successful replacement and zero/nonzero recovery,
all allocation failure sites, early/allocation-triggered cancellation, repeated
maximum-size resets, numerical input protection and invalid calls. Lock freedom
is source-audited, not proved by the new/delete instrumentation. Live worker
cancellation races, graph transitions, twenty-track deadlines, product sound,
listening, device and release validation remain separate pending work.

The initial extreme-input fixture incorrectly demanded exact zero/identity
samples next to a 1e6 impulse after float FFT roundoff. It failed on Windows.
The sanitisation regression now compares bit-exactly against the same kernel
fed manually sanitised PCM and separately requires all-nonfinite input to emit
exact silence and extreme output to remain finite. No DSP or ordinary-audio
`3e-6` direct-FIR tolerance was changed.

Executed results: Windows Release 3/3 (0.89s), native WSL Linux Release 3/3
(1.09s), core-only Debug ASan/UBSan/leak 3/3 (14.72s), no reported sanitizer
errors or suppressions. The final expanded quiet-tail/replacement fixture at
`ca61094` passed Windows 1/1 (0.56s), Linux 1/1 (0.52s), sanitizer 1/1 (13.67s).
The unchanged preparation/settings tests were not rerun for that test-only
expansion. Both Release probes report nine OOM sites and peak ordinary PCM
error `5.61065e-08`. Sanitizer cache flags remain Debug,
`-fsanitize=address,undefined -fno-omit-frame-pointer`, matching linker flags,
plugin OFF. These timings are test wall times, not callback deadline or physical
latency measurements. Normal source hashes match Windows, committed Git blobs
and the native WSL checkout; explicit `-text` attributes preserve upstream bytes.
No app/GUI/device/listening/DAW/build/package/push validation is implied.

### Non-uniform long-tail stage — 24 November 2026

`NonUniformConvolver` now composes the verified 128-frame head with a 1024-frame
tail. The head covers IR samples 0..1023. The remaining coefficients use a
1024-frame-latency tail kernel: its latency exactly accounts for their IR offset,
so the total convolution still has **zero algorithmic latency**. No IR trimming,
renormalization, resampling or control/preset changes. Both parts keep using the
unmodified, credited HiFi-LoFi AudioFFT / Takuya Ooura backend. No new dependency.
The two-stage partition concept was studied in HiFi-LoFi FFTConvolver; its engine
implementation was not copied. See [credits](../CREDITS.md) for the distinction.

Observable acceptance retains the exact same independent direct-FIR/impulse
oracles and tolerances (`3e-6` ordinary PCM, `1e-13` isolated quiet final tap),
adds 1023/1024/1025 and 2047/2048/2049 IR boundaries, and exercises both kernels
from the same contract fixture. All previous uniform checks remain. OOM and
cancellation now begin with an old multi-partition tail and partial tail input;
cancellation is injected at every candidate allocation, including after head
preparation. Successful publication owns the whole candidate, never a new head
paired with an old tail. Reset invalidates history and clears fixed scratch;
prepare/destruction still belong exclusively off audio.

The tail's past spectra stay immutable during an input partition. After consuming
`f` of its 1024 frames, it has multiplied `ceil(validPartitions * f / 1024)` old
partitions. Only the current partition's product and a forward/inverse FFT occur
at completion. This distributes multiply work across short callbacks instead of
performing a full-tail burst. Internal audio-owner `lastTailWork()` counts only
tail products/transforms; it is not cross-thread telemetry or a CPU measurement.
An aligned 64/128-frame regression warms 64 dense tail partitions, checks the
per-call product bound and two transforms exactly once per 1024 frames, and
requires multiply work on calls between FFT boundaries. The process loop copies
at most 128 input samples to fixed scratch before overwriting in-place output.
Rejected calls preserve both history and work diagnostics.

Maximum admitted IR remains 1,920,000 frames; process calls remain bounded to
4096. At that limit the tail has 1874 partitions (1025 bins each). Four tail
spectra arrays hold 30,733,600 bytes; four head arrays add 16,512 bytes, plus
fixed state/FFT scratch, allocator overhead and any retained raw IRs. Stereo and
old/candidate overlap are additional. Larger partitions reduce processing
frequency, **not order-of-magnitude memory use**. The future graph pool must
admit the complete storage footprint. This is not twenty-track CPU/deadline or
physical latency acceptance; broad profiling remains deferred by owner policy.

B stays disabled until a complete wet path (pre-delay/damping/mix/gain), bounded
graph preparation/adoption/retirement, and shared GUI/CLI/MIDI/state integration
pass their own gates. No live swaps, product sound, listening, hardware or release
claim is made by these component tests.

Source `8555fb3` results: Windows Release 2/2 (1.48s), native WSL Linux Release
2/2 (1.32s), core-only Debug ASan/UBSan/leak 2/2 (29.12s), with no reported
sanitizer errors/suppressions. Non-uniform peak independent PCM error
`5.93611e-08`; retained uniform contract peak `6.81991e-08` after adding boundary
cases. Nine uniform and 19 non-uniform allocation sites exercised, plus
cancellation at every site. The sanitizer cache retains plugin OFF, Debug,
address/undefined instrumentation and frame pointers. Times are test wall times,
not a kernel speed comparison or callback deadline measurement. Package helper
PowerShell syntax and diff checks passed. No new package, app/UI/device/DAW test
or push was performed. Original assets/reference and upstream FFT bytes unchanged.

```powershell
cmake --build build/windows-core --config Release --target aion_nonuniform_convolution_tests aion_convolution_tests --parallel 4
ctest --test-dir build/windows-core -C Release -R '^(nonuniform_convolution_contract|convolution_kernel_contract)$' --output-on-failure --no-tests=error
```

After a coherent local commit and documented WSL snapshot sync:

```sh
cmake --build build/linux --target aion_nonuniform_convolution_tests aion_convolution_tests --parallel 2
ctest --test-dir build/linux -R '^(nonuniform_convolution_contract|convolution_kernel_contract)$' --output-on-failure --no-tests=error
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_nonuniform_convolution_tests aion_convolution_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(nonuniform_convolution_contract|convolution_kernel_contract)$' --output-on-failure --no-tests=error
```

### Prepared stereo wet processor — 24 November 2026

`Reverb` composes two non-uniform convolvers with stereo pre-delay, wet-only
damping, linear dry/wet mix and post-mix volume. This retains the original
`_createReverb` control/routing order (credited to Jorge Salgueiro/Ascii-Looper),
without copying browser filter code. AudioFFT/Ooura credits and notices remain;
no new third-party source/dependency or preserved UI asset changes. Native
smoothing/crossfades/TPT state follow Aion's existing Delay approach, with the
bilinear Q=.5 coefficients independently checked in direct form.

Acceptance is actual stereo audio against direct fractional-delay/sparse-FIR and
direct-form low-pass equations at absolute `3e-6`, plus independently generated
dense IR audio, original control order, sample-indexed live edit/block consistency,
zero callback new/free, fixed reset, and complete failed-preparation rollback.
Steady values are latched at preparation, so there is no initial gain ramp.

- Volume/Mix/enable use 10ms one-pole smoothing. Damp's derived TPT coefficient
  uses the same smoothing; `tan` is cached on accepted Damp changes, with the
  normal steady-state filter denominator cached outside per-frame recomputation.
- Damp remains stored at 500..20000 Hz. Only the derived frequency is bounded
  to `.45 * sampleRate`, keeping low-rate processing finite/stable. Q=.5 gives
  a non-resonant wet-only low-pass. No claim of browser filter PCM identity.
- Pre-delay supports exact zero through .25s, fractional linear reads and two
  read heads with a 20ms constant-sum fade. Edits during a fade update the latest
  desired position; after the current fade it starts the latest pending fade.
  There is no unbounded queue, read-position jump or equal-power DC gain bump.
- A settled bypass is exact dry and stops wet computation. After its enable ramp
  reaches zero, convolution/filter history and pre-delay validity are reset before
  the call returns or another chunk starts. Reenable fades in a fresh wet path;
  old bypassed tails cannot reappear. Ordinary PLAY/RTRG/history restarts must not
  reset this persistent processor. A mix of zero keeps processing wet history.
- The maximum pre-delay ring is 96,002 stereo frames / 768,016 storage bytes
  at 384kHz, in addition to two convolvers and fixed scratch/state. Reset only
  invalidates ring indices/counts; it never clears that ring on audio. Numerical
  input/output is finite and limited only beyond +/-1e6; tiny filter states below
  1e-24 are cleared, matching the existing native Delay protection policy.

`prepare(rate, settings, leftIR, rightIR, cancellation)` is worker-only. Both IRs
must contain exactly `floor(rate * Duration)` frames; coefficient validation is
provided by convolution preparation. No raw IR pointer survives preparation.
Both channels and the ring publish together only after success. All candidate
allocations (40 for the tested two-tail graph), including the second-channel FFT
and final ring, can fail/cancel without changing the old stereo running state.
The outer graph-owner publication/late-cancellation fence is still future work.

Audio-owner `set` changes Volume/Mix/Damp/Pre-delay/enable/metadata without new/free.
Duration or Decay changes reject atomically and require a new worker-prepared
processor. Rejected controls/calls do not mutate state. Calls accept 0..4096
frames, four equal-length spans, disjoint or exactly aligned in-place input/output
(including swapped channels); output channels must be distinct, and partial
overlap is outside the contract. Two-channel scratch is captured before outputs
are overwritten. No locks/I/O/logging or IR-size rebuild/copy/clear is introduced
on the callback.

The fixture checks eight rates (8–384kHz), zero/fractional/maximum pre-delay,
64/128/irregular blocks (72 combinations plus in-place replay), stereo isolation,
swapped outputs, all 17 preset settings, generated dense IRs at 8/48kHz, analytic
gain/mix/bypass ramps, rapid pre-delay DC continuity, sample-offset edits including
bypass reversal, late-tail bypass erasure, destructive reset, new sample-rate
preparation, invalid spans/controls and every allocation/cancel site. There is no
product UI/CLI/MIDI feature or new saved schema here, so no disconnected controls
are exposed and no actual UI/host/device claim is made.

Source `ccaa80c` executed results: Windows Release 1/1 (1.14s), native WSL Linux
Release 1/1 (0.78s), core-only Debug ASan/UBSan/leak 1/1 (25.02s), no reported
sanitizer errors or suppressions. Both Release probes report peak PCM error
`1.19209e-07`, below the unchanged `3e-6` tolerance, with all 40 candidate
allocation/cancel sites covered. Existing Debug/address+undefined/frame-pointer
and plugin-OFF flags were confirmed unchanged. Timings are test wall times, not
deadline/physical latency results. UI/reference and upstream FFT bytes unchanged.
The documentation-only follow-up removes a repeated credits paragraph and records
these results; it does not rerun unchanged passing audio suites.

Focused commands:

```powershell
cmake --build build/windows-core --config Release --target aion_reverb_tests --parallel 4
ctest --test-dir build/windows-core -C Release -R '^reverb_stereo_processor_contract$' --output-on-failure --no-tests=error
```

After committing and the documented WSL snapshot sync:

```sh
cmake --build build/linux --target aion_reverb_tests --parallel 2
ctest --test-dir build/linux -R '^reverb_stereo_processor_contract$' --output-on-failure --no-tests=error
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_reverb_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^reverb_stereo_processor_contract$' --output-on-failure --no-tests=error
```

Next: complete graph memory admission, bounded per-loop worker request/adoption/
retirement with cancellation and history lifetime fencing, then shared product
GUI/CLI/persistence/MIDI and live transition acceptance. These tests are not
twenty-track CPU/deadline, physical latency, listening or release certification.

### Bounded convolution graph ownership — 24 November 2026

Internal `ReverbStorage` follows Aion's existing `DelayStorage` phase protocol:
20 track identities, 40 construction-time heap slots, one exclusive audio owner
and one worker. It does not start a thread or bind to LoopBank/GUI/CLI yet.
The future Machine engine is not represented as an available mode; legacy B/V
state and the unified-Reverb plan remain unchanged.

Acceptance: reject requests before mutation when slot/memory capacity is exhausted;
prepare and destroy only off audio; preserve old PCM on every worker allocation
failure; reject obsolete queued/preparing/ready results after cancellation/clear;
crossfade independent prepared stereo outputs at 64/128; retain only the latest
accepted per-control edits; and recover after exhaustion/reset/rate changes with
zero audio-owner new/delete. Existing standalone wet-processor tests remain.

- `request` validates/reserves the temporary stereo IR, both complete convolution
  graphs, FFT plans/working arrays, pre-delay and fixed state before publishing
  a slot. `serviceWorker` reclaims retired owners then prepares at most one slot.
  Settings/seed/request identity are immutable during queued/preparing ownership.
- The default per-pool reservation budget is 256MiB. `preparationBytes` methods
  account for pinned portable Ooura arrays and implementation-object allowance.
  All vectors reserve 64 bytes of standard-library alignment slack; MSVC's pinned
  Release STL adds a 39-byte prefix to large vectors. Temporary raw IRs are also
  included. The complete reservation stays held until worker reclamation, even
  after those IR vectors have been released. Fixed pool metadata is additional
  construction storage. This is a conservative requested-allocation accounting
  model, **not RSS, general allocator/OS bookkeeping or an application-wide RAM
  cap**; the other effect pools and future global admission remain separate.
- The initial measured allocation test failed at 8kHz/5s: 1,789,169 requested
  bytes versus 1,788,944 reserved. The missing MSVC alignment prefixes were traced
  and added to the formula; the measured-payload <= reservation gate was retained.
  Evidence: `build/reverb-storage-reservation-negative.txt`. Reaudit the formula
  when FFT/vector backends or toolchains change.
- `poll` acquires ready results only for the exact pending slot, latches controls
  and uses a constant-sum 20ms fade from dry/old to new. It marks the old owner
  retired only after its final audio use. A third replacement is rejected while
  two owners are pinned. Rendering advances fades; routing integration must
  account for stopped/omitted stages rather than leaving fades unserviced.
- Audio `setControl` supports existing live indices 0/1/4/5; Duration/Decay still
  require worker replacement. Each control and enable has its own revision. At
  adoption only fields edited after that request override its preset, preserving
  its other settings. The unrendered new processor latches these targets before
  the adoption fade; ongoing live edits update both pinned owners with their own
  shape settings and retain each processor's normal smoothing.
- `cancel`, `clear`, `clearAll` and `reset` never destroy on audio. Cancelled slot
  results cannot overwrite a newer request. Reset cancels pending work, retires
  the previous owner and resets the retained current DSP. `prepareStopped` and
  `releaseStopped`, including destruction, require both owners quiescent.
- `View` reports the current per-track adoption stamp/IR seed and bounded status only.
  Installation serials are internal to one prepared lifetime, not new parameter
  or session IDs. The const graph borrow is audio-owner-only and expires on
  lifecycle mutation. No shared pointer/refcount or large snapshot was added.

Tests measure preparation payload at 8/48/384kHz including 1024/1025-frame split
boundaries and maximum 5s IRs, exercise independent old/new stereo fade PCM at
64/128, all 20 simultaneous tracks, 40-slot and byte-budget exhaustion, retirement
backpressure, before/after-adoption controls, every one of 42 worker allocation
failures, mid-transition reset and quiescent rate replacement. A real separate
worker pauses during allocation while the audio owner cancels and queues a new
identity; the old result must not install. This is not ThreadSanitizer or physical
audio/driver evidence. The tests instrument C++ new/delete on the audio owner;
source review checks phase/lifetime rules and absence of locks/I/O.

```powershell
cmake --build build/windows-core --config Release --target aion_reverb_storage_tests aion_reverb_tests --parallel 4
ctest --test-dir build/windows-core -C Release -R '^(reverb_storage_contract|reverb_stereo_processor_contract)$' --output-on-failure --no-tests=error
```

After committing and the documented WSL snapshot sync:

```sh
cmake --build build/linux --target aion_reverb_storage_tests aion_reverb_tests --parallel 2
ctest --test-dir build/linux -R '^(reverb_storage_contract|reverb_stereo_processor_contract)$' --output-on-failure --no-tests=error
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_reverb_storage_tests aion_reverb_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(reverb_storage_contract|reverb_stereo_processor_contract)$' --output-on-failure --no-tests=error
```

Next: connect this convolution owner to the existing LoopBank worker/render path,
destructive/prepare/capture lifecycle barriers and coherent bank/state restore,
then shared GUI/CLI/MIDI/persistence. Loop/clip generation fencing and global
multi-effect atomic restore are still integration gates. B stays disabled. No
Machine engine, product switch, listening/device, twenty-track CPU/deadline,
Standalone UI or release acceptance is claimed by this internal pool.

Source `f4f8d97` verification: Windows ownership + stereo processor 2/2 (1.68s),
then final added memory-boundary ownership 1/1 (0.54s); native WSL Linux Release
2/2 (1.26s); core-only Debug ASan/UBSan/leak 2/2 (30.49s), no reported errors or
suppression. Debug/address+undefined/frame-pointer/plugin-OFF flags confirmed.
Preserved UI/logo/reference and vendored FFT bytes unchanged. These are test wall
times, not a callback-deadline benchmark. No app/UI/host/hardware run, package or
push. The memory negative and correction above remain recorded; no numerical or
reservation assertion was weakened.

Integration seam found in existing source: `Engine.cpp` calls `loops.tick` once
per sample; `LoopBank::tick` sequences the user-reorderable per-track chain. Do
not implement B by invoking this FFT processor on one sample repeatedly. Either
prepare a scalar-compatible streaming head/tail path or add a bounded chunked
render seam while preserving sample-accurate events, capture/routing dependencies,
chain order and existing transition state. Do not silently add buffering latency.
This is an integration architecture prerequisite, not broad deferred CPU tuning.

### Scalar-compatible render seam — 24 November 2026

`NonUniformConvolver` now uses an immediate **128-tap direct FIR** head, a
128-frame delayed FFT stage for IR samples 128..1023, and the existing 1024-frame
delayed stage for the remaining IR. Each delayed stage's buffering exactly
matches its IR offset. Total algorithmic latency remains zero. AudioFFT/Ooura
sources and notices are unchanged; the direct FIR loop/history is Aion's own
code. The old uniform `PartitionedConvolver` and its regressions are retained.

This resolves the kernel side of Engine's per-sample `loops.tick` integration
constraint without changing Engine transport, capture or user chain ordering.
New bounded `tick` entry points on the non-uniform kernel, stereo `Reverb` and
`ReverbStorage` share their existing `process` implementation and lifecycle.
No new GUI/CLI commands, state IDs or available Machine mode are implied.

Acceptance retains all previous direct-FIR/impulse PCM tolerances (`3e-6`, plus
the isolated `1e-13` quiet-tail check), 40 stereo-preparation and 42 ownership OOM
sites, cancellation and memory-reservation checks. A separate streaming fixture
adds bit-exact scalar versus 64/128 processing (kernel also 257), sample-offset
live edits, bypass sleep/wake, prepared replacement, reset, cancellation/clear,
new-result recovery and zero C++ new/delete on scalar calls at 8/48kHz.

`lastDirectProducts`, `lastNearWork` and the retained `lastTailWork` are
audio-owner diagnostics only. They verify <=128 direct products per scalar call,
bounded distributed spectrum products, **two transforms per completed 128-frame
near partition and two per 1024-frame far partition**. Partial/scalar calls do
not recompute FFTs. Short IRs fitting the direct head use no FFT at all. This is
an operation-scheduling check, not a callback deadline or twenty-track CPU claim.

Reset invalidates the direct history with a saturating validity count and resets
fixed stage buffers; no IR-sized scan/new/free. Candidate preparation retains
the old complete state on failure/cancel. Byte reservations now account for the
actual direct/near/far layout and vector-prefix allowances; the measured-payload
<= admitted-reservation gate remains unchanged. No allocation coverage was
removed to accommodate the internal layout change: the long stereo graph still
has 40 allocation sites and the pool worker has 42 including temporary IRs.

```powershell
cmake --build build/windows-core --config Release --target aion_reverb_streaming_tests aion_nonuniform_convolution_tests aion_reverb_tests aion_reverb_storage_tests --parallel 4
ctest --test-dir build/windows-core -C Release -R '^(reverb_streaming_contract|nonuniform_convolution_contract|reverb_storage_contract|reverb_stereo_processor_contract)$' --output-on-failure --no-tests=error
```

After the documented committed-snapshot WSL sync:

```sh
cmake --build build/linux --target aion_reverb_streaming_tests aion_nonuniform_convolution_tests aion_reverb_tests aion_reverb_storage_tests --parallel 2
ctest --test-dir build/linux -R '^(reverb_streaming_contract|nonuniform_convolution_contract|reverb_storage_contract|reverb_stereo_processor_contract)$' --output-on-failure --no-tests=error
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_reverb_streaming_tests aion_nonuniform_convolution_tests aion_reverb_tests aion_reverb_storage_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(reverb_streaming_contract|nonuniform_convolution_contract|reverb_storage_contract|reverb_stereo_processor_contract)$' --output-on-failure --no-tests=error
```

Next is actual LoopBank worker/render/destructive-lifecycle binding and coherent
bank/state restore, then shared product GUI/CLI/MIDI/persistence. The scalar
seam is tested in components, not yet called by the production Engine. B stays
disabled; no physical/listening/DAW/Standalone UI or release acceptance is claimed.

Executed source `fb69c82`: Windows Release 4/4 (4.14s), native WSL Linux Release
4/4 (2.99s), core-only Debug ASan/UBSan/leak 4/4 (57.30s), no reported sanitizer
errors or suppressions. All four tests retain their 60s per-test timeout. Existing
Debug/address+undefined/frame-pointer/plugin-OFF cache flags confirmed intact.
Windows diagnostic probes report non-uniform direct-FIR peak `6.35922e-08` and
stereo wet peak `5.96046e-08`; retained limits are unchanged. The new streaming
contract requires bit-exact scalar/block equality within each build, not across
different libm implementations/platforms. C++ audio-owner new/delete, 19 mono,
40 stereo and 42 pool-worker OOM coverage, cancellation and payload reservation
checks all pass. These are test wall times/operation counts, not a CPU speedup,
physical latency or twenty-track deadline measurement. No preserved UI/reference/
logo or vendored FFT changes, no product engine binding, package or push.

### LoopBank worker/render/lifecycle binding — 25 November 2026

`LoopBank` now owns a construction-time heap `ReverbStorage`, following the
accepted Delay ownership pattern (`LoopBankReverb.cpp`, `ReverbStorage::set`,
`LoopView::reverb*`). The prepared scalar `Reverb::tick` path is called from the
existing per-sample chain at code **B**:

- `requestReverb` admits only an enabled, valid, epoch/revision-matching prepared
  request; the worker prepares the stereo IR/graph off audio.
- `pollReverbs` publishes metadata only at the matching worker installation;
  `beginBlock` is the sole poll point. A newer accepted live edit cancels a
  not-yet-adopted request so a stale completion cannot overwrite it.
- `setReverb` treats live controls 0/1/4/5 like Delay: a same-shape edit applies
  to both pinned transition owners. Disabled metadata may select a new shape
  without allocating; enabling that shape still requires a prepared request and
  is rejected atomically (no metadata mutation) otherwise.
- `cancelReverb`/`resetReverb`/`resetReverbs` cover armed-cancel metadata restore,
  lane reuse, STOP ALL, timeline reset and DEL. A real `clear` retires the graph
  and resets `info.reverb`; `preserveArp` resets the runtime but keeps metadata.
- Reinserting B in `setSignalChain` starts a fresh instance (history/reset), like
  D. While B is temporarily absent, an in-flight replacement crossfade advances
  with silence so it cannot leak PCM or strand an owner.
- `tickReverb` obeys the existing STOP/clear source gate (`sourceActive`), so
  panic cannot resurrect wet tails; disabled runtime settles to exact dry.
- `trackView().reverbTransitioning` mirrors owned transition state for future UI
  telemetry. No bridge/CLI/state/MIDI/JSON/parameter change is included.

New `reverb_loopbank_connected_contract` (4 rate/block combinations) compares the
bank against an independent `ReverbStorage` oracle fed the recovered pre-merger
source at <=2e-6, with measured Windows peak error `1.49012e-08`. It covers
prepared adoption acknowledgement, stale epoch/revision rejection, live mix,
shape replacement crossfade, timeline reset, disable-to-exact-dry/re-enable,
off-chain transition completion, reinsert reset, prepared-shape rejection,
disabled-shape metadata, prepared delivery of that shape, pending-replacement
invalidation, STOP ALL source gating and DEL invalidation. Four `reverb_loopbank_stereo_*` cases add independent
left/right recovery, loop pans 0/2/5/8/10, hard-pan silence, immutable PCM and
finite output (Windows peak error `1.23721e-08`). Every audio-owner call stays in
`AudioScope`: no new/delete on the callback.

```powershell
cmake --build build/windows-core --config Release --target aion_reverb_connected_tests --parallel 4
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(reverb_loopbank_connected_contract|reverb_loopbank_stereo_8000_64|reverb_loopbank_stereo_8000_128|reverb_loopbank_stereo_48000_64|reverb_loopbank_stereo_48000_128|reverb_storage_contract|reverb_streaming_contract|reverb_stereo_processor_contract|delay_loopbank_connected_contract|signal_chain_connected_contract)$'
```

Executed source `1fe4e67`: focused Windows Release 12/12 passed (20.55s) and the
full `core_realtime_contract` passed (991.75s); native WSL Linux Release 12/12
passed (7.39s) plus the full Linux `core_realtime_contract` (452.04s); core-only
Debug ASan/UBSan/leak 12/12 passed (140.08s) with no reported sanitizer errors
or suppressions. PCM peaks are identical on Windows and Linux (`1.49012e-08`
mono, `1.23721e-08` stereo). Test wall times are not CPU/latency or deadline
claims. The full `core_realtime_contract` suite now keeps
its two large test `Engine` instances and the per-fixture `EngineView` snapshots
on the heap; adding the reverb view fields had pushed that single test frame
past the default 1 MB Windows stack. No assertion, tolerance or audio behavior
changed, and repeated `--midi` runs are deterministic.

Next is coherent bank/state restore (including the future arp/effects restore
hook for B), then shared product GUI/CLI/MIDI/persistence. B stays disabled in
the product surface; no listening, physical, DAW or Standalone UI acceptance is
claimed by this binding.

### Ordered Engine B transactions — 25 November 2026

`Engine` now owns the same tracked request/result protocol for B that D uses,
without exposing a product command. `enqueueReverbEdit` publishes a
`ReverbEditRequest` descriptor and one ordered `LoopCommand` only after a shared
FIFO preflight; one unread receipt per track remains the admission bound, and
transaction identities are never reused across prepare. `popReverbEdit` returns
the immutable terminal fact (`ready/cancelled/stale/busy/failed`) with the
request, generation, clear epoch and settings revision it actually committed.

The audio owner validates the current edit identity, epoch and revision, then:

- a same-shape live edit (or any disabled metadata edit) commits in the ordered
  callback at its exact sample deadline (`setReverb`), with no worker wait;
- an enable that needs a new impulse/graph defers `ready` until the worker
  installs it, matching the adoption receipt to the storage installation and
  revision; a request refused while a prior 20ms adoption fade is live reports
  `busy`;
- STOP ALL, DEL, MIDI Stop, prepare and changed epochs/revisions cancel or stale
  the pending receipt without converting it into an ordinary UI acknowledgement.

New `reverb_engine_transactions_contract` covers validation, one-outstanding,
bare-command rejection, ordinary-loop acknowledgement isolation, adoption and
live receipts, busy/stale, zero-block cancellation and ID reuse, MIDI Stop,
committed-then-DEL, FIFO saturation, twenty unread receipts and generation
recovery at 64/128, with zero tracked callback heap operations. New
`reverb_engine_audio_contract` drives the actual Engine callback against an
independent `ReverbStorage` oracle moved to the same target at the same sample;
prepared adoption, a sample-offset live mix, disabled-shape metadata (settling
to bit-exact dry) and a prepared re-enable all match at <=2e-6. Measured Windows
peak error is `1.49012e-08` at 8/48 kHz and 64/128 with audible change >0.059.

Next: the reverb settings owner and bank/state restore (including the
arp/effects restore hook), then shared GUI/CLI/MIDI/persistence. No product
bridge/CLI/GUI/state/MIDI command, preset identity or B toggle is enabled yet.

Executed source `3a9381d`: focused Windows Release 11/11 passed (23.44s) and the
full `core_realtime_contract` passed (1012.69s); native WSL Linux Release focused
11/11 passed (5.76s) plus the full Linux `core_realtime_contract` (428.01s);
core-only Debug ASan/UBSan/leak 11/11 passed (116.07s) with no reported errors or
suppressions. The Engine PCM peak is `1.49012e-08` on both platforms. Wall times
are not CPU/latency or callback-deadline claims.

### Coordinated B bank restore — 25 November 2026

`ReverbStorage` now has the same all-or-nothing bank protocol as `DelayStorage`.
`collectBankSlots` validates every member and the aggregate payload reservation
before publishing any work; only enabled members claim worker slots. Disabled
members publish nothing. `bankStatus` requires *every* member ready, so a partial
worker prefix can never reach audio. `prepareBankCommit` applies/reset each
prepared graph and aborts the whole bank if any shape is refused; `commitBank`
then swaps all twenty owners together, crossfading each enabled target and
fading an old owner to dry for disabled ones. One failed member rolls back the
whole bank and retains every old target. An all-off bank needs no worker work
and retires all old owners after the bounded dry fade. Cancellation, prepare,
clear and per-track edits cancel the bank; a new bank is excluded while a
per-track request or adoption fade is live.

`LoopBank::requestReverbBank`/`cancelReverbBank` validate each track's clear
epoch, settings revision and per-track pending state before admission, publish a
`pending` result, and commit metadata/revisions for all twenty tracks only at
the matching worker completion. `Engine::enqueueReverbBank`/`popReverbBank`/
`cancelReverbBank` add the descriptor+ordered-command admission, one outstanding
receipt including unread terminals, shared never-reused transaction identities,
and `ready/busy/stale/cancelled/failed` correlation with the actual audio
ownership. STOP ALL, DEL, MIDI Stop, prepare and changed epochs/revisions cancel
or stale the bank without touching ordinary UI acknowledgements.

New `reverb_bank_contract` covers invalid-member atomic rejection with no worker
prefix, bank/edit exclusion, nineteen-member partial non-commit, sample-exact
old-owner PCM against twenty independent `Reverb` models, injected worker OOM
rollback, the all-off old-to-dry transition and full retirement, aggregate
budget admission, full-FIFO and unread-receipt bounds, busy/stale/cancel,
all-off next-boundary commit at a future sample, and old-generation retention /
new-generation recovery through prepare at 64/128 with zero tracked callback
heap operations.

Next is the non-audio reverb settings owner and persistence adapters, then the
shared GUI/CLI/MIDI surface. B stays disabled and no product state/command ID is
advertised yet.

Executed source `c58ab43`: focused Windows Release 32/32 passed (47.95s; the
separately built Delay MIDI audio case passed 0.88s) and the full
`core_realtime_contract` passed (990.81s); native WSL Linux Release focused
32/32 passed (15.28s) plus the full Linux `core_realtime_contract` (428.60s);
core-only Debug ASan/UBSan/leak 32/32 passed (303.02s) with no reported errors or
suppressions. Old-owner PCM is sample-exact and the all-off fade is within 1e-7
on both platforms. Wall times are not CPU/latency or callback-deadline claims.

### Non-audio B settings owner — 25 November 2026

`ReverbSettingsOwner` mirrors the accepted `DelaySettingsOwner` contract for the
non-audio side: explicit known-state `bind` (all twenty committed settings and
pending state must match the supplied `EngineView`), per-track candidates and
intents, exact `ReverbEditResult`/`ReverbBankResult` receipt matching, and a
committed record that advances only on a proven native adoption. Precision is
retained exactly; `intent()` exposes an uncommitted candidate while `track()`
keeps the last proven fact. A clear-epoch advance rolls that track back and
cancels its pending edit; a generation change or an unexplained external
revision detaches rather than guessing; the saved bank remains readable after
detach for an explicit rebind.

`restore` publishes a coordinated bank only when no track is pending and every
member is valid, marks every record pending, and commits all twenty (or just the
non-cleared tracks when DEL races the receipt) only on the exact bank receipt.
Waiting on a per-track receipt excludes further edits and another bank. The
owner deliberately has no MIDI receipt path and no coordinated Arp/effects
restore hook yet; those remain separate, gated slices.

New `reverb_settings_owner_contract` covers invalid-edit atomicity, double-bind
rejection, exact-precision intent vs committed, pending exclusion, unrelated
global acknowledgements, stale snapshots, cancellation and request-ID reuse,
worker-OOM rollback of enable intent, prepared enable/room adoption, disable
retention, full-FIFO admission failure, unread-success vs late cancel/clear,
bank invalid-member/duplicate/pending exclusion, full-bank commit, OOM rollback,
per-track cancellation, ID reuse, unread all-off plus clear reconciliation,
same-rate rebind, rate-change detach/rebind, queued-intent cancellation and
external-revision detach at 64/128.

Executed source `270c652`: focused Windows Release 33/33 passed (50.42s) and the
full `core_realtime_contract` passed (1013.67s); native WSL Linux Release
focused 33/33 passed (15.44s) plus the full Linux `core_realtime_contract`
(433.48s); core-only Debug ASan/UBSan/leak focused 33/33 passed (325.37s) with
no reported errors or suppressions. These are test wall times, not CPU/latency
claims.

### Independent integration audit — 27 November 2026

Scope: eight commits after `37206b4` through `d5c69c8`, covering LoopBank/worker
binding, Engine edit/bank protocols, ReverbStorage banks, settings ownership and
the added tests. Git records the project author, not the coding provider; this
is the recent OpenCode/DeepSeek-assisted work identified by the owner. Review
focused on transaction interleavings, cancellation, lifetime, state consistency,
shared MIDI acknowledgements and callback safety. The existing fixture heap
moves preserve assertions and Windows' stack limit; they were retained.

**Fixed: installation stamp collision (high integration impact).** Storage used
`++installation` for banks but assigned `slot.request` for individual adoption.
Those independent counters can have the same value. Starting from an enabled
bank, a Duration edit prepared and installed new DSP, but Engine interpreted the
unchanged stamp as no installation and returned cancelled; the settings owner
then detached with old committed settings. `poll` now increments the same
per-track adoption counter as `commitBank`; bank/single admission reject exhausted
counters before mutation. Request transactions, seeds, IDs and DSP are unchanged.
The regression includes enabled-bank -> prepared edit and all-off bank -> enable.

**Fixed: MIDI Stop acknowledgement contamination.** The purge helper excluded
Delay banks and single Reverb edits, but not Reverb banks. A private bank ID
could falsely advance ordinary `lastRequestId` and set `rejected`. The helper
now excludes `LoopAction::reverbBank` too. Tests separately place the future bank
in the pending-command slot and behind a queued ordinary command, asserting the
correct ordinary acknowledgement AND the separately correlated bank cancellation.

Before fixes, all six 64/128 audit cases failed; the exact failures remain in
`build/reverb-audit-before.txt`. After fixes, Windows Release 16/16 affected tests
passed (11.68s); existing owner/transactions/bank/LoopBank stereo/storage checks
remain unchanged. All new Engine callbacks track C++ new/delete. The tests
reproduce protocol/state failures rather than changing mathematical tolerances.

```powershell
cmake --build build/windows-core --config Release --target aion_reverb_audit_tests aion_reverb_owner_tests aion_reverb_bank_tests aion_reverb_engine_tests aion_reverb_engine_audio_tests aion_reverb_connected_tests aion_reverb_storage_tests --parallel 4
ctest --test-dir build/windows-core -C Release -R '^(reverb_audit_.*|reverb_settings_owner_contract|reverb_bank_contract|reverb_engine_transactions_contract|reverb_engine_audio_contract|reverb_loopbank_connected_contract|reverb_loopbank_stereo_.*|reverb_storage_contract)$' --output-on-failure --no-tests=error
cmake --build build/windows-core --config Release --target aion_core_tests --parallel 4
build/windows-core/Release/aion_core_tests.exe --midi
```

After committing and the documented clean WSL snapshot sync, from native storage:

```sh
reverb_audit_targets='aion_reverb_audit_tests aion_reverb_owner_tests aion_reverb_bank_tests aion_reverb_engine_tests aion_reverb_engine_audio_tests aion_reverb_connected_tests aion_reverb_storage_tests'
reverb_audit_tests='^(reverb_audit_.*|reverb_settings_owner_contract|reverb_bank_contract|reverb_engine_transactions_contract|reverb_engine_audio_contract|reverb_loopbank_connected_contract|reverb_loopbank_stereo_.*|reverb_storage_contract)$'
cmake --build build/linux --target $reverb_audit_targets --parallel 2
ctest --test-dir build/linux -R "$reverb_audit_tests" --output-on-failure --no-tests=error
cmake -S . -B build/drive-sanitized # preserve the existing core-only sanitizer cache
cmake --build build/drive-sanitized --target $reverb_audit_targets --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R "$reverb_audit_tests" --output-on-failure --no-tests=error
```

Executed source `6a7e054`: Windows Release 16/16 (11.68s), native WSL Linux
Release 16/16 (5.16s), core-only Debug ASan/UBSan/leak 16/16 (102.46s), no reported
errors/suppressions. Current Debug/plugin-OFF/address+undefined/frame-pointer
flags and all original timeouts were retained. Test wall times are not runtime
performance/latency measurements.

The existing Windows `aion_core_tests --midi` passed after rebuilding that binary;
it does not run the full default core suite. Actual output includes 6144 CC
lookup/filter cases, 240 learned MUTE cases, 224 play/stop + 28 mute cancelled-note
recovery cases, Note/CC/PC routing, sustain/panic/holds, zero callback new/delete
and exact native WAV/sample-offset checks at 64/128. This is synthetic core MIDI,
not physical controllers, JUCE device routing, editor interaction or driver tests.

Independent full-suite rerun at source `4670bc0` closed that focused-only gap:
Windows Release `core_realtime_contract` passed (1038.84s) and native WSL Linux
passed (436.35s, 91 PASS sections). No test, tolerance, timeout or build flag was
changed.

No new audio model, parameter/action/preset IDs, bridge/GUI/CLI/persistence or
Machine-mode feature was added. Preserved UI/logo/reference and third-party
sources/notices remain unchanged. Reverb stays disabled in the product surface.
This is a bounded audit with regression fixes, not a certification that the
application is bug-free or that UI/listening/device/DAW/release checks passed.

### B state persistence — 27 November 2026

`AionProcessor` now persists B independently of the joint Q/C/chain/A/D restore.
`readReverbs`/`saveReverbs` use a versioned `LoopReverbs` node with one `Reverb`
row per track (`track`, `enabled`, `room`, `preset`, six exact-precision
controls). An absent node restores the original disabled defaults for pre-B
sessions; duplicate nodes, wrong version/count/tag/track, nested children,
missing fields, non-finite/non-canonical numbers, unknown rooms, out-of-range
presets/controls or preset/room/value mismatch reject the complete state before
any engine, owner or APVTS mutation.

`serviceReverb` is the sole serialized non-audio B owner service, reached by the
existing message Timer through `serviceEffectsRestore` (not a second producer):
it drains exact receipts, binds only verified known settings, fences ordinary
DEL acknowledgements, tracks shadow clear epochs, protects an accepted load until
its exact bank receipt, and submits the coordinated bank only when the confirmed
bank differs. A failed/cancelled bank retains the confirmed records and stops
retrying until a new load/clear. Prepare keeps the accepted intent, save drains
receipts first, and STOP ALL cancels pending B work while DEL resets only its
track. No MIDI learn, web-panel adapter or Machine engine is added; the shared
command/CLI layer is described below.

New `aion_reverb_state_processor_tests` (64/128) covers exact versioned
round-trip through a second processor and a stopped load before first prepare,
immediate save of an accepted-not-yet-admitted intent, 32 whole-state rejections
(type/version/duplicate/track/field/range/room/preset/number cases), pre-B
defaults, and zero callback heap operations. The retained
`aion_delay_state_processor_tests` still passes 64/128 and the Standalone Release
links.

The shared command layer now exposes B without a web adapter:
`operation:"reverb"` supports `get/toggle/enable/reset/preset/room/set` through
the same validated `ReverbSettingsOwner` path, and snapshots include `reverb`,
`reverbBusy`, `reverbPending`, `reverbRequestId` and `reverbReady`.
`tools/aion.py reverb <track> <action> [--preset|--room|--parameter|--value]`
provides the same actions with the existing instance-selection, expiry,
generation and busy/ownership checks. The state test exercises set/toggle/snapshot
through the public request path without an editor. The preserved web B panel
still needs its native adapter before it can be enabled, so B remains
hidden/disabled in the GUI.

Executed source `5af180b`: Windows Release `aion_reverb_state_processor_tests`
64/128 passed (32 whole-state rejections each) together with the retained Delay
state cases (four native state tests, 3.59s); native WSL Linux also passed 64/128
and its Standalone Release links. The shared command/CLI addition was verified at
source `3a8d8d5`: Windows and native WSL Linux reverb state 64/128 (including
set/toggle/snapshot through the public request path) and Delay state 64/128 pass,
both Standalone Release targets link, and `tools/aion.py` compiles and lists
`reverb` on both platforms. The core-only sanitizer build does not include the
JUCE processor target; the state paths add no audio-callback work and the
processor tests still track zero callback new/delete.

### Earlier preparation verification

Source checkpoint `bb5b6f6`: final Windows Release 2/2 passed (0.39s), native WSL
Linux Release 2/2 passed (0.57s), core-only Debug ASan/UBSan/leak 2/2 passed
(1.59s), no reported sanitizer errors or suppressions. Initial Windows checks
(0.84s) and cache/rollback follow-up (0.43s) also passed. The fixture covers all controls/presets/rooms,
precision/validation, eight rates, same-seed PCM, unit energy, DC rejection,
zero edges, stereo correlation, monotonic Decay distribution, max rate/duration,
both allocation failures and cancellation before/during allocation. 64/128 cases
are immutable coefficient reads, **not audio callbacks or convolution proof**.
The first Makefile sanitizer build could not find the new target; regenerating
that existing cache fixed the invocation before its tests ran. Flags were
confirmed unchanged: Debug, plugin builds OFF, address/undefined instrumentation
and frame pointers. This is not ThreadSanitizer or a sanitized JUCE DSP run.

```powershell
cmake --build build/windows-core --config Release --target aion_reverb_preparation_tests --parallel 4
ctest --test-dir build/windows-core -C Release -R '^(reverb_preparation_contract|original_reverb_settings_contract)$' --output-on-failure --no-tests=error
```

After committing/syncing via SetupWslWorkspace.sh, run from the native WSL root:

```sh
cmake --build build/linux --target aion_reverb_preparation_tests --parallel 2
ctest --test-dir build/linux -R '^(reverb_preparation_contract|original_reverb_settings_contract)$' --output-on-failure --no-tests=error
cmake -S . -B build/drive-sanitized # preserve the existing core-only sanitizer cache
cmake --build build/drive-sanitized --target aion_reverb_preparation_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(reverb_preparation_contract|original_reverb_settings_contract)$' --output-on-failure --no-tests=error
```

No actual UI/device,
listening, DAW, sustained performance or release claim. Do not package this stage
as a working B feature or rerun unrelated passing suites routinely.

### Independent persistence/command review — 27 November 2026

Reviewed `5af180b` and `3a8d8d5` through checkpoint `105a443`. The new panic
regression failed 2/2 at 64/128 before the fix (`build/reverb-review-before.txt`):
STOP ALL cancelled an individual edit, but the state service resubmitted the
unconfirmed shadow settings as a bank. Terminal individual receipts now roll
shadow intent back to committed settings without automatic retry. Only explicit
load/prepare intent can prepare a bank. Loads mark that intent before older clear
acknowledgements arrive and cancel obsolete B edit/bank work. State tests add
panic/save rollback, fresh-edit recovery, clear-before/after-load, an edit
superseded by a load, and an in-flight bank superseded by the prior committed bank.

New `aion_reverb_command_tests` drives the public production request owner at
64/128, plus `tests/ReverbCliTests.py` as a separate process through the actual
LocalCommandServer. It checks exact receipts, all six controls and endpoints,
17 presets, eight rooms, enabled preparation/live edits, original precision,
strict invalid-command rollback, accepted-intent persistence, reset/bypass and
expanded-track isolation. All processor tests track zero callback new/delete.
The external CLI harness uses synthetic callbacks; it does not test a physical
device, the desktop holder's automatic server startup or the pending web adapter.

```powershell
cmake --build build/windows --config Release --target aion_reverb_command_tests aion_reverb_state_processor_tests aion_delay_state_processor_tests aion_joint_effects_processor_tests AionLooper_Standalone --parallel 2
ctest --test-dir build/windows -C Release -R '^native_(reverb_commands|reverb_state_processor|delay_state_processor|joint_effects_processor)_(64|128)$' --output-on-failure --no-tests=error
ctest --test-dir build/windows-core -C Release -R '^(reverb_.*|original_reverb_settings_contract)$' --output-on-failure --no-tests=error
```

Windows core reverb/reference passed 20/20 (16.45s). An initial invocation used
`build/windows`, where the core executables were not built: nine were Not Run
and the reference launcher failed to spawn its probe. Rebuilt the correct
`build/windows-core` targets and all 20 passed; no product/test tolerance change.
Final source `9ce8a6b`: Windows processor/state/CLI/adjacent restore 8/8 passed
(29.33s, `build/reverb-review-native-final.txt`), and Standalone Release links.
The native Linux mirror was clean, then fast-forwarded with the documented
SetupWslWorkspace helper. Its affected processor targets and Standalone Release
built; combined native/core/reference 28/28 passed (16.41s,
`build/reverb-review-linux-tests.txt`). Commands in the native root:

```sh
cmake --build build/linux --target aion_reverb_command_tests aion_reverb_state_processor_tests aion_delay_state_processor_tests aion_joint_effects_processor_tests AionLooper_Standalone --parallel 2
ctest --test-dir build/linux -R '^(native_(reverb_commands|reverb_state_processor|delay_state_processor|joint_effects_processor)_(64|128)|reverb_.*|original_reverb_settings_contract)$' --output-on-failure --no-tests=error
```

Core DSP is unchanged, so prior sanitizer DSP evidence remains historical;
the JUCE state/command paths are not covered by that core-only sanitizer build.
No full unrelated suite, WebView2 interaction, physical device/listening/DAW or
release acceptance was run. The preserved B panel adapter is still the next
product slice, followed by coordinated effects restore and MIDI receipts/learn.

### B web panel adapter and real GUI acceptance — 27 November 2026

The preserved B panel is now a native presentation adapter
(`resources/native/reverb.js`), embedded with the other adapter scripts and
loaded before the looper. It adds no WebAudio, DSP, state schema or MIDI target:
every gesture uses the existing `operation:"reverb"` request through
`ReverbSettingsOwner` and the existing Engine/LoopBank reverb path.

- Original panel: preset selector with the 17 original names/order; Volume
  .1–3/.01, Mix 0–1/.01, Duration .1–5/.1, Decay .1–5/.1, Damp 500–20000/100 and
  Pre-delay 0–.25/.005 with the original labels and two-decimal readouts. Numeric
  edits show Custom (the native owner clears the preset), reset restores defaults
  and preserves enable, and a control's double-click restores its original
  initial value. Rooms stay preset-selected exactly as in the preserved panel;
  the native `room` command remains available to the CLI without a new control.
- Main and LIVE rows bind the original `revB` toggle
  (`#loop-N-reverb`, `#live-loop-N-reverb`): it selects that track in the shared
  effects panel and sends the same toggle command. Row label clicks reveal the
  panel without a command. Controls follow `audioReady`; a focused slider follows
  authoritative native values, while an active pointer drag or key gesture is
  left to the user until release.
- Commands wait while `reverbBusy` and settle only on the exact
  `reverbRequestId` receipt; a later CLI/other-controller request cannot resolve
  or reject this editor's edit, and pending/ready/expiry/generation/supersession
  keep the Delay policy shape. Tagged replies now include `reverb` in the
  17-significant-digit JSON round trip on both C++ and bridge sides, so exact
  native doubles survive the real IPC bridge.

Executed with the panel-adapter source (this checkpoint):

- `node tests/FrontendTests.cjs` passes. New cases drive the production adapter
  against a small DOM harness: 17-preset catalogue/order, six original ranges and
  initial disabled state, authoritative update/readouts, preset/set/toggle/reset
  command mapping, focused/drag value ownership, main-row toggle/label wiring,
  readiness and editor-close. The same file runs the production receipt policy
  (seven Reverb generation/supersession/pending/expiry/later-controller/ready/
  failed cases), the production `sendReverb` path (disabled, transport failure,
  native rejection, receipt error, success) and the embedding/wiring checks.
- `node tests/BridgeTests.cjs` passes with exact reverb precision over the
  tagged bridge and an immediate slot release for a malformed reverb payload.
- `tools/TestReverbUi.ps1` (real WebView2 editor, actual CDP pointer/keyboard,
  synthetic production PCM; hardware-free host) passes at **64 and 128**:
  B row and panel enable, 18-option preset catalogue, all six controls edited by
  trusted keyboard input with Custom clearing, original preset + room, Reset,
  exact bridge precision, panel bypass/re-enable, reload retaining settings and
  nonzero playback, LIVE toggle, final bypass after reload with unchanged
  waveform identity, no product `AudioContext` and no runtime exceptions. One
  128 launch did not initialize the editor inside the original 10 s wait and was
  rerun with a 20 s wait; that first attempt is recorded as inconclusive, not a
  pass.
- The retained `tools/TestDelayUi.ps1` passes at 64/128 with the B adapter
  loaded, covering panel coexistence. Retained `aion_reverb_command_tests`,
  `aion_reverb_state_processor_tests`, `aion_delay_state_processor_tests` and
  `aion_joint_effects_processor_tests` pass at 64/128; callback new/delete stays
  zero. The Windows Release Standalone and test host link.
- Windows core reverb/reference subset `^(reverb_.*|original_reverb_settings_contract)$`
  passed 20/20 (15.59 s) at this source, retaining the unchanged settings oracle,
  bank/edit/MIDI-Stop ownership, streaming/storage/preparation, stereo PCM,
  LoopBank and Engine audio contracts.
- The committed snapshot was fast-forwarded to the native WSL checkout with
  `tools/SetupWslWorkspace.sh` (Linux was clean at `9ce8a6b`). The Linux Release
  Standalone linked and the affected CTest set passed 10/10 (22.16 s):
  `native_frontend_commands`, `native_bridge_lifecycle`, and the reverb command,
  reverb state, Delay state and joint-effects processor cases at 64/128. The
  same core reverb/reference subset passed 20/20 (6.03 s) on Linux with Node 18.

Not run or not established: Linux WebKit/GUI interaction; sanitizer run for these
JUCE paths; listening, physical devices/controllers and DAW/host workflows;
packaging. `tools/TestSignalChainUi.ps1 -Blocks 64` fails its fixed 120 ms DOM
check in this hidden-window session both with and without this change (verified
against the stashed baseline): the snapshot build itself measured 0.66 ms while
the WebView2 IPC round trip was ~300 ms, so that suite is inconclusive here and
the text-only chain tooltip update is not counted as validated by it. B is now
enabled in the GUI; the unified Machine engine, B MIDI learn/CC targets and the
coordinated effects restore hook remain pending.


## Coordinated Arp/Reverb/effects restore — design, 27 November 2026

Status: **design only, no production code changed.** This is the next gated slice
for B. It follows the already-accepted Delay precedent (opt-in fields on the Arp
bank, a single joint receipt and storage-level commit) rather than inventing a
second restore mechanism.

### Observable acceptance

A saved-state load (host state set, processor restoreState and a changed-rate
prepare) restores Q/C/chain/A/D **and B** as one admitted transaction:

- all twenty B settings, clear epochs and revisions are validated before any
  worker job or audio graph change;
- if any B storage/impulse preparation fails, the whole joint bank fails and the
  previously playing graphs and committed B settings remain untouched;
- the single terminal receipt names B (its epoch/revision arrays), and the
  non-audio owner adopts B only from that receipt;
- a DEL on one track during preparation cancels the joint bank or preserves that
  track's post-clear owner exactly as the accepted Delay/per-track rules require;
- panic, STOP ALL, clear, prepare, supersession and unread receipts keep the
  current bounded/cancellation semantics;
- external accepted B edits cannot interleave with the joint transaction;
- callback new/delete stays zero and the existing audio tolerances are unchanged.

### Current state and why B is separate

B already has complete individual and B-only bank transactions
(Engine::enqueueReverbEdit/enqueueReverbBank, LoopBank::requestReverbBank,
ReverbSettingsOwner). The Processor restores B independently in serviceReverb,
while serviceEffectsRestore submits the existing Arp/effects bank
(Q/C/chain/A/D). A session load can therefore install B outside the joint
atomicity and after the other effects, and B failures/clears cannot cancel the
joint transaction. Delay no longer has this split: its settings travel in
LoopBank::EffectsBank with restoreDelays.

### Data model

1. LoopBank::EffectsBank gains ReverbBank reverbs{}, ReverbEpochs
   reverbRevisions{} and bool restoreReverbs=false (default false preserves
   Arp-only and existing Delay callers). validEffectsBank requires every
   validReverbSettings and reverbRevisions[id] != UINT64_MAX when the flag is set.
2. The Arp bank request already embeds EffectsBank; no new payload queue or
   second producer is added. ArpBankResult gains bool includesReverbs=false and
   LoopBank::ReverbEpochs reverbSettingsRevisions{} (mirroring
   includesDelays/delaySettingsRevisions).
3. Engine::enqueueArpBank requires no outstanding B bank and no outstanding
   per-track B edit when restoreReverbs is set, and publishes
   coordinatedReverbBank (a release/acquire atomic, like coordinatedDelayBank)
   for the admission lifetime. B edit/bank admission rejects while that flag is
   set; a B edit accepted before joint admission is excluded by revision checks.
4. LoopBank::requestArpBank (audio owner) verifies no pending B bank/edit and
   matching B revisions, then requests all selected banks: Arp, Delay (existing)
   and B. Any failure rolls back every staged bank before jobs can install; the
   audio graphs already in use are never touched.
5. LoopBank::pollArps owns the joint terminal state: stale clear-epoch/revision
   changes cancel all staged banks (cancelArpBank also cancels the B bank), a
   worker failure in any stage fails the joint result, and only when Arp, Delay
   and B are all ready are they committed in the same callback before
   info.reverb/revision are advanced and the Q/C/chain/drives metadata is
   applied. pollReverbs must not independently validate, cancel or commit a
   storage bank while the joint request owns it; it continues to service
   per-track B results and reverbStorage->poll().
6. EngineArpBank adds B stale checks (all twenty epochs/revisions), draws the
   receipt's B arrays/revision ownership from the already-committed view, and
   reports includesReverbs. Cancellation of the joint request retires the B
   storage bank through the same owner; unread receipts remain bounded.

### Processor ordering

7. restoreState keeps publishing the exact B candidate and reverbRestorePending;
   prepareToPlay keeps its stopped-lifecycle intent. No new intent format or
   state version is needed.
8. serviceEffectsRestore is the only submitter while the joint intent exists: it
   includes reverbOwner's committed/known B bank only after the B owner is bound
   and no B edit/bank is pending, then accepts the single receipt through a new
   ReverbSettingsOwner::acceptEffectsRestore(view, candidate, receipt, epochs,
   revisions) (exact request/generation/callback-serial/epoch/revision proofs,
   cleared-track semantics, no retry). Only then does it clear
   reverbRestorePending.
9. serviceReverb returns early while the joint intent is active (the current
   serviceDelay rule), keeps servicing individual edit receipts, clear epochs
   and intent rollback, and stops submitting a competing B-only bank for state
   loads. A failed joint bank retains the previous committed B values and
   reverbRestoreFailed semantics.
10. STOP ALL/DEL/MIDI Stop clear/cancel paths keep their existing ordering: the
    effect bank is cancelled before ordinary commands; DEL cancels only its
    track's pending work and its newer clear epoch invalidates stale adoptions.
    No audio-thread allocation, lock, file or UI work is introduced.

### Failure and cancellation matrix (must be covered)

| Event | Required outcome |
| --- | --- |
| B worker OOM / impulse build failure | joint failed, Arp/D/B staged banks cancelled, old audio and committed B kept |
| Arp or Delay stage fails while B is ready | same cancelled/failed terminal receipt; B is never adopted alone |
| clear-epoch/revision change on any track before commit | joint cancelled (or that track's exact post-clear owner preserved as today) |
| DEL during preparation | no old-graph hybrid; receipt exposes the per-track epoch outcome |
| STOP ALL / panic / prepare | joint cancelled, queued jobs retired, unread receipts retained |
| external B edit/command during joint admission | rejected; a later independent edit after admission does not steal the receipt |
| superseding state load | newest intent wins; the older transaction cannot adopt |
| unread terminal receipt | no new joint bank or B edit until consumed (bounded) |

### Planned tests

- Core admission/ownership: mirror effects_delay_coordinated_bank_* with B —
  both readiness orders, OOM in each of the three storages, all-off commit,
  stale final-track revisions, independent B edit/unread-receipt exclusion,
  B/Arp/D cancellation, panic, DEL and prepare, same-ID recovery.
- Actual Engine audio at 8/48 kHz and 64/128: joint Q/C/chain/A/D/B restore with
  independent PCM oracles for the B wet/dry output; old audio continuous during
  preparation; OOM rollback; B disabled/all-off and enabled transitions; zero
  callback new/delete. Reuse the existing 2e-6 stereo tolerance and the existing
  mono-downmix/centre-pan input model; no new browser-PCM gate is invented.
- Processor: state load restoring all effects plus B through one receipt;
  save-load-save identity; B failure/rollback; clear/DEL; superseding load;
  prepare/rate change; no-editor Timer service; zero callback heap operations.
- GUI/state: after a load the B panel and row toggle show the restored settings
  and the existing exact bridge precision is retained.

### Risks and limits

The Arp bank is the single audio-side transaction for effects; adding a third
storage increases its cancellation surface. The design therefore keeps B opt-in,
reuses the existing ReverbStorage bank APIs and worker ownership, adds no new
queue, allocation, lock or parameter/state ID, and preserves the accepted Delay
and standalone B behavior when the flag is false. Machine engine, B MIDI
learn/CC targets, listening/device/DAW and Linux WebKit UI remain outside this
slice. Implementation proceeds core bank first, then Processor ordering, then
GUI/state acceptance, with the affected Windows and Linux tests at each step.

### Processor joint B restore submission — 27 November 2026

The product Processor now submits B through the existing Arp/effects bank:

- serviceEffectsRestore is the sole submitter while a state/prepare B intent
  exists. It requires a bound B owner and no pending per-track B edit, adds
  effects.reverbs and reverbRevisions, and adopts B only from the exact terminal
  receipt through the new ReverbSettingsOwner::acceptEffectsRestore (request,
  generation, callback serial, clear-epoch and per-track revision proofs, plus
  the cleared-track outcome rules).
- serviceReverb no longer submits a competing B-only bank. It binds/mirrors the
  owner, resolves pending clears and shadow epochs, services individual edit
  receipts, and clears a matching intent without a bank; the separate B-only
  ReverbSettingsOwner::restore path is retained only for its owner tests.
- A clear admitted after the load is ordered before the joint receipt. On a
  cancelled receipt with a newer clear epoch, the load intent is retained and the
  next service submits the cleared candidate (the cleared track reset, the other
  tracks still restored) instead of latching a terminal failure. Failed/busy
  receipts without a newer clear retain the previous committed B and the
  reverbRestoreFailed outcome; a matching intent needs no bank.

Acceptance (Windows Release):

- aion_reverb_state_processor_tests 64/128 pass with exact versioned roundtrip,
  32 whole-state rejections, pre-B defaults, stopped prepare, command
  cancellation and ordered load/clear recovery, zero callback heap operations.
  The fixture drives the production timerCallback, so submission, adoption and
  clear/load ordering use the real owner path.
- Reverb command, Delay state and joint-effects processor tests pass at 64/128;
  the Windows Release Standalone and mouse host link.
- Actual WebView2 Reverb and Delay UI tests pass at 64/128 (panel/commands,
  reload, nonzero PCM, unchanged clip) with the joint restore active.

Linux (clean native WSL checkout at this commit): the Release Standalone links,
and the native reverb command/state, Delay state and joint-effects processor
cases pass 8/8 at 64/128 (10.81 s). The reverb settings-owner contract, the new
coordinated Arp/B bank set and the reverb/Arp bank contracts pass 8/8 (12.87 s).

Not yet established: MIDI learn/receipts, Machine mode and listening/device/DAW
acceptance. The joint B PCM oracle and the GUI host-state fixture are recorded
below.

### Coordinated B PCM oracle — 27 November 2026

New aion_effects_reverb_bank_audio_tests renders the actual coordinated Arp/B
bank through the production Engine and compares every settled sample against an
independent ReverbStorage bank scheduled at the same receipt boundary:

- two identical Engines capture and play a synthetic loop; the test Engine's B
  owner is restored only through the joint bank while the independent bank is
  committed when the exact receipt is consumed, so first-enable and replacement
  shape fades start at the same block;
- a second bank replaces the B shape; a third bank arms a single worker
  allocation failure, which fails the joint receipt while the accepted B owner,
  audio, transport and history remain unchanged; the oracle's staged bank is
  cancelled;
- all twenty tracks' receipt revisions are checked; callback new/delete remains
  zero under the audio guard.

Executed: 8000 and 48000 Hz at 64/128 all pass. Maximum absolute error is
7.45058e-09 (limit unchanged 2e-6), audible change 0.0362–0.0525, and the Engine
output matches the independent B bank sample-for-sample after each commit. CTest
`effects_reverb_coordinated_bank_audio_contract` passes (0.81 s); the full B
bank set is 6/6 on Windows. The clean native WSL checkout builds the same test
and passes the B bank set 6/6 (9.82 s; the PCM case 0.15 s).

### Coordinated core Arp/Reverb/effects bank — 27 November 2026

Implements the audio-side half of the design above. The product Processor still
uses the separate B bank: the new fields default to restoreReverbs=false, so no
state-load, command, parameter, GUI or saved-state version changes yet.

- LoopBank::EffectsBank adds opt-in B settings/revisions; validEffectsBank checks
  them only when the flag is set.
- requestArpBank stages Arp, Delay and B storages; B admission requires no
  pending B bank/edit and matching revisions, preflights capacity, and rolls back
  every staged bank if any request or the final publication fails.
- pollArps validates all coordinated revisions, treats a lost (cancelled) staged
  bank as failure, requires the selected storages ready, prepares and commits
  them in one callback, then advances info.reverb/request/revision together with
  the existing metadata. pollReverbs defers its bank to the joint owner through
  the new ReverbStorage::poll(deferBank) and keeps servicing per-track work.
- cancelArpBank cancels the staged B bank. EngineArpBank adds B admission
  exclusion, stale checks, includesReverbs and B revision arrays to its receipt,
  and a coordinatedReverbBank admission flag. Engine::cancelReverbBank cancels
  the joint bank, and B edit/bank admission is rejected while the joint owns the
  transaction (including unread receipts).

Acceptance (Windows Release, this checkpoint):

- New aion_effects_reverb_bank_tests: admission plus scenarios 0-3 at 64/128,
  covering Arp+B and three-storage Arp+Delay+B. Admission exercises invalid and
  stale descriptors, B edit/bank and unread-receipt exclusion, and the audio-side
  aggregate budget rejection with no worker allocation (armed-failure counter
  untouched). Scenarios verify atomic commit, exact receipt revision proofs,
  B-worker OOM rollback with the old owners retained, reverb cancellation
  invalidating the joint bank, three-storage commit and same-ID recovery.
  Tracked callback new/delete remains zero.
- CTest coordinated/bank set 18/18 (97.40 s, including the new admission and four
  B scenarios at both block sizes). Retained reverb core/reference subset 20/20
  (15.37 s), the existing Delay coordinated scenarios and Arp bank contract pass.
  The processor command/state, Delay state and joint-effects tests pass at
  64/128 and the Windows Release Standalone links.

Linux (committed snapshot fast-forwarded to the clean native WSL checkout): the
Release Standalone links, coordinated/bank 18/18 (18.22 s), reverb
core/reference 20/20 (6.34 s) and the native reverb command/state, Delay state and
joint-effects processor cases 8/8 at 64/128 (10.59 s).

Limits: the Processor does not submit or adopt this bank yet, so no product
behavior changes and B joint PCM is covered here by owner identity/commit rather
than a new audio oracle. MIDI receipts/learn, Machine mode and
listening/device/DAW acceptance remain separate. Next: connect
serviceEffectsRestore/serviceReverb and ReverbSettingsOwner::acceptEffectsRestore,
then add the B joint PCM audio test and GUI/state acceptance.

### GUI host-state B restore acceptance — 27 November 2026

The hardware-free WebView2 host gains a test-only state channel: it watches
state-save/state-load request files, calls the production
getStateInformation/setStateInformation and acknowledges by request id. No RPC,
debug command or product surface is added; the host remains a test target.

TestReverbUi.ps1 now configures B dark-cave through the shared command, saves the
host state, switches to small, loads the saved state, and verifies the snapshot B
settings, the panel preset and volume readout, the main-row revB toggle, and
unchanged loop PCM/waveform identity. This exercises the real path: state load to
readReverbs, joint restore submission/adoption, snapshot and panel update.

Executed: Reverb UI passes at 64 and 128 with the retained panel/reload/LIVE
coverage and host-state round trip. The retained Delay UI passes at 64/128 with
the same host. Linux GUI/WebKit, MIDI learn, Machine mode and listening/device/DAW
acceptance remain separate.

### B MIDI continuous learn — 27 November 2026

Conservative B MIDI mapping is now connected end to end. Only the live controls
that do not change the prepared impulse shape are exposed: Volume, Mix, Damp and
Pre-delay map to appended continuous targets 153–156 after Delay's 147–152;
Duration and Decay stay unmapped (a shape edit needs a prepared transaction).

- MidiSettings version 9 writes 157 bindings. Versions 1–8 keep their exact old
  lengths (v8 reads 153) and the four new slots start unassigned.
- Engine::applyReverbMidi applies the exact CC value to the committed B track and
  publishes a bounded receipt with clear epoch and before/after revision. The
  ReverbSettingsOwner consumes receipts under its owner generation, suspends
  dispatch during joint restore/edit intent, and detaches on any unexplained
  revision, exactly like the accepted Delay owner.
- Message 153 = Volume, 154 = Mix, 155 = Damp, 156 = Pre-delay as CC.
  Right-click learn and Shift-right-click clear work on those four sliders;
  a focused learned slider follows hardware values, an unbound focused slider is
  protected, and drags/keys keep ownership until release. Duration/Decay sliders
  stay unlearnable.

Executed (Windows Release): native Reverb MIDI passes at 64/128 (learn, all four
targets across every channel and representative values, full 0–127 Mix sweep,
bounded 512-receipt saturation and drain recovery, joint-restore suspension,
unselected tracks, zero callback heap, v8 to v9 migration). Delay MIDI passes at
64/128, and the production MIDI suite's 64/128 sections pass. Retained reverb
state/command, Delay state and joint-effects tests pass; Reverb UI passes at
64/128 with right-click learn, CC application, clear and an unmapped
Duration/Decay check; Delay UI passes at 64/128 and FrontendTests pass.

Linux (clean native WSL checkout at this commit): the standalone and all affected
test binaries link; native Reverb MIDI 64/128, Delay MIDI 64/128 and the
production MIDI suite pass 5/5 (25.80 s), and the reverb command/state, Delay
state and joint-effects processor cases pass 8/8 (9.52 s).

Limits: no MIDI mapping for Duration/Decay, Machine engine, pitch/expression
targets, physical controllers or Linux WebKit/listening/DAW acceptance. The host
state load now suspends B MIDI immediately (not only on the next Timer tick), so
a packet between the load and its first service cannot edit the old owner.

### Independent integration audit and audible B MIDI — 27 November 2026

Reviewed the panel/bridge, coordinated Arp/Delay/B bank, Processor adoption and
four-control MIDI changes from `8b61e0a` through `53c7c71`. One production defect
was reproduced with a valid 384 kHz, all-twenty, five-second B load: aggregate
budget refusal leaves the confirmed native owner unchanged and retains the
failed host intent, but MIDI was resumed unconditionally. A subsequent CC could
then change the audio owner against that intent. The new production regression
failed at both block sizes before the fix (`build/reverb-midi-failed-restore-before.txt`).
The message service now suspends B MIDI while failed intent differs from the
confirmed bank and clears a resolved failure only after pending edits settle.
It also checks the failure delivered in that same service before returning MIDI
ownership. Tests verify rejection of CC during failure and fresh-load recovery;
both 64/128 pass (`build/reverb-midi-failed-restore-fixed.txt`).

New `aion_reverb_midi_audio_tests` / `reverb_midi_audio_contract` compares actual
paired Engine PCM against scheduled ordinary ReverbSettingsOwner edits. It uses
captured nonzero audio, prepared enabled convolution, all four live controls,
preassigned and first-packet-consumed same-block provisional Learn, sample
offsets 0/17/last, channels 1/16 and nonmonotonic 0/127/32/64 values, 8/48 kHz and
64/128. Both output channels must remain within the existing `1e-6` limit and
settings must match exactly with owners still bound. Unread MIDI followed by
DEL, fresh post-clear CC, suspend/recovery and zero callback new/delete remain
asserted. No old test or tolerance was removed/relaxed. The first learned-audio
fixture omitted the required learnGeneration and therefore never armed Learn;
correcting that fixture stamp made its retained PCM assertion pass.

Windows core checks: coordinated bank admission/scenarios/PCM and owner 7/7
passed; the final MIDI audio test passed (1.89s). FrontendTests and BridgeTests
pass. Final native/UI/Linux evidence follows in the verification checkpoint.
No DSP algorithm, product control, ID, schema, logo, preserved reference or
third-party source was changed. Physical controller, listening/device/DAW,
Linux WebKit, shape-control MIDI and Machine acceptance remain separate.
