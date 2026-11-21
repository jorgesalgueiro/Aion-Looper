# D / Delay implementation checkpoint

Owner priority (21 November 2026): remaining loop features precede broad CPU
tuning. This is an **internal DSP/settings component**, not an enabled product
feature. Existing GUI/CLI/session IDs and the running audio chain are unchanged.

## Contract and acceptance

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

## Reproducible checks

Windows, existing core cache (PowerShell):

```powershell
$cmake = '.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = '.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
& $cmake -S . -B build/windows-core
& $cmake --build build/windows-core --config Release --target aion_delay_tests --parallel 4
& $ctest --test-dir build/windows-core -C Release -R '^(delay_component_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
```

Linux, after committing the Windows snapshot and using `SetupWslWorkspace.sh`:

```sh
cd /home/jorge/projects/Aion-Looper
cmake -S . -B build/linux
cmake --build build/linux --target aion_delay_tests --parallel 2
ctest --test-dir build/linux -R '^(delay_component_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
# Existing ASan/UBSan Debug cache, plugin builds disabled:
cmake -S . -B build/drive-sanitized
cmake --build build/drive-sanitized --target aion_delay_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/drive-sanitized -R '^(delay_component_contract|original_delay_settings_contract)$' --output-on-failure --no-tests=error
```

## Remaining before D is a working feature

Executed 21 November 2026, source snapshot `e75c210`:

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

Worker-prepared bounded per-track ownership/retirement with OOM/cancellation
tests; editable-chain integration; shared commands, CLI, MIDI Learn, native UI,
versioned persistence and editor lifecycle. Then connected Engine 64/128 PCM,
actual GUI/CLI interaction and listening acceptance. Keep D unavailable until
these pass; do not start another effect or package this component as complete.
Broad full-workspace CPU tuning remains deferred, not silently accepted.
