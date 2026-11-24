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
- `View` reports current request installation/IR seed and bounded status only.
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
