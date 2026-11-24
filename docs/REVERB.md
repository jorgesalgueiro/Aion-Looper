# B / convolution reverb migration

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
