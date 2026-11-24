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

### Backend research and next step

The existing pinned [JUCE convolution API](https://docs.juce.com/master/classjuce_1_1dsp_1_1Convolution.html)
offers stereo partitioned/non-uniform processing and explicit load/process
threading requirements. Its `prepare` makes the latest loaded IR ready before
processing. The [MIT FFTConvolver implementation](https://github.com/HiFi-LoFi/FFTConvolver/tree/non-uniform)
offers a pure C++ two-stage alternative with an included FFT. No source was
copied, dependency added, backend selected or pinned version upgraded here.

Next: audit a backend's actual process/reset/destruction paths, then verify
convolution against independent short direct-FIR and long impulse/tail oracles.
Require stereo/mono isolation, declared latency, variable blocks including 64/128,
zero callback allocations/locks, bounded reset and worker-only retirement. Only
then add per-loop preparation/adoption with explicit OOM/cancellation outcomes,
live smoothing/crossfades, user-reorderable B routing, versioned settings and
shared GUI/CLI/MIDI access. Current presets, IDs, logo and other effects remain.

### Verification

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
