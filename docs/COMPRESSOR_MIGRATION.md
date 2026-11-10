# Compressor migration — native loop effect

The native loop **C** effect is connected in the development build; scoped Windows
and Linux software checks pass. This is not a
release or a claim that drone/sampler compressors or remaining FX are ported.
No preserved UI/reference assets or existing parameter/action IDs changed. No
product AudioContext, dependency upgrade, package or wrapper build.

## Connected acceptance and owner decisions — 9 November 2026

The owner explicitly approved **one persistent compressor per loop across source
restarts** and **live controls during OD/SUB/MULT/scheduled STOP**. Original
per-graph reset/frozen controls remain recorded by unchanged-JS tests; these two
extensions are not described as identical transition sounds. Enable/re-enable
creates a fresh detector; DEL/panic/timeline prepare clear its delay. Existing
source/STOP output gates remain authoritative; full browser overlapping-source
transition envelopes, listening and hardware latency remain separate validation.

The default original chain is `QCATFODBVKZG`: native EQ precedes compressor,
then existing loop volume/pan and MASTER. The compressor never changes immutable
clips, capture/feedback, history or dry export. Fixed per-track DSP storage is
allocated at construction; audio controls use the existing bounded FIFO and
sample clock. GUI, external Standalone CLI and MIDI share that audio owner.
Curve generation/serialization stay on the message thread (321 points, <=10 Hz
UI requests); the original display knee remains distinct from actual compression.

Native `LoopCompressors` v1 stores twenty complete entries, validates all before
APVTS/MIDI/effect restore, and defaults missing legacy data to bypassed originals.
EQ+compressor banks are admitted to the command FIFO all-or-neither (40 commands),
not claimed as a single sample-atomic forty-track render transaction. Queue-full
restore retries; invalid state/commands preserve accepted intent. MIDI v5 appends
CC IDs 117..123; readers retain v1/v2/v3/v4 assignments and selected tab unchanged.
Controls follow the shared EQ/FX loop tab, work without an editor, do not enable
a bypassed compressor and retain conflict/cancel/channel rules. Physical devices
remain unverified by the synthetic tests.

Windows executed evidence: connected Engine 64/128 peak <=1.78814e-7 against an
independent compressor render after existing centre pan/output limiting; all
seven CC controls exact PCM versus sample-timed GUI commands. The original
isolated browser oracle independently protects the DSP. Live OD/SUB/MULT/STOP,
EQ composition, restart/reverse/history/phase, raw WAV invariance, cancellation,
mask/epoch/full-queue admission and zero callback new/delete pass. Production
processor/CLI/state and real WebView2 pointer/key/curve/MIDI/reload checks pass at
64/128, as do existing EQ UI tests. The UI test initially captured state before
its keyboard edit was acknowledged; it now waits for the exact +1 control value
before comparing reload, without changing product code or assertion tolerance.
The connected oracle accounts for existing pre-pan gain and final output clipping;
its first incomplete harness failed, was corrected, and all PCM thresholds stayed
at 1e-6. Legacy MIDI test writer assertions now expect v5/unknown v6, retaining
v1..v3 and adding v4 coverage. One broad production MIDI CLI timeout passed on an
unchanged isolated retry; retain that observation separately from audio failures.

Commands (Windows VS CMake/CTest executables as documented in DEVELOPMENT_WORKFLOW):

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_compressor_connected_tests aion_compressor_tests aion_compressor_probe aion_effects_engine_benchmark
ctest --test-dir build/windows-core -C Release --output-on-failure -R '^(compressor_realtime_contract|compressor_connected_contract|original_compressor_contract|eq_connected_contract|js_native_eq_connected|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_eq_midi)$'
cmake --build --preset windows-dev --target AionLooper_Standalone aion_mouse_host aion_compressor_processor_tests aion_eq_processor_tests aion_midi_processor_tests
build/windows/Release/aion_compressor_processor_tests.exe 64
build/windows/Release/aion_compressor_processor_tests.exe 128
tools/TestCompressorUi.ps1
tools/TestEqUi.ps1
node tests/CompressorWebAudio.cjs --fixtures build/compressor-browser-fixtures.json
build/windows-core/Release/aion_effects_engine_benchmark.exe
```

Fixture JSON is ignored local evidence; rerun TestCompressorWebAudio.ps1 to
regenerate from the actual unchanged browser methods. No later effect or new
distributable is included in this checkpoint.

## Final connected verification — recorded 10 November 2026

Code `dc2fb4d`, independent clean native WSL checkout:

- Linux focused CTests **9/9 (15.63s)**; final STOP/PLAY connected **1/1 (1.10s)**.
- Compressor and existing EQ production processor/state/MIDI/external CLI pass
  at 64/128; broad MIDI suite passes. Final compressor invocations retain an
  assigned v4 EQ target/channel while appended compressor targets default off.
- Linux ASan/UBSan/leaks **2/2 (24.56s)**; expanded STOP/PLAY **1/1 (21.18s)**,
  no diagnostics. Callback allocation/free checks pass.
- Both platforms retain all **60 / 1,966,080 stereo-frame** browser fixtures,
  peak **4.806921385380747e-7**, unchanged **1e-6 peak / 1e-5 relative RMS** gates.
  Linux replays Windows WebView2 PCM; it is not a Linux WebKit comparison.
- Retained capture/OD/SUB audit: **126 cases / 965,826 samples**, peak
  **7.394757495227111e-8** on both platforms.

Linux `xvfb-run` was unavailable; hardware-free processor tests used the existing
WSL display `:0` without opening an editor. Windows actual compressor AND EQ
pointer/key/curve/MIDI/reload checks pass at 64/128. Linux WebKit interaction,
physical expression, listening/driver latency, full browser transition envelopes
and long-session/release checks remain separate.

Additional exact Linux commands (existing independent caches):

```sh
cmake --build build/linux --parallel 2 --target AionLooper_Standalone aion_compressor_tests aion_compressor_probe aion_compressor_connected_tests aion_compressor_processor_tests aion_eq_processor_tests aion_midi_processor_tests aion_effects_engine_benchmark aion_reference_probe
ctest --test-dir build/linux --output-on-failure -R '^(compressor_realtime_contract|compressor_connected_contract|original_compressor_contract|eq_connected_contract|js_native_eq_connected|original_ui_bytes|native_frontend_commands|native_bridge_lifecycle|js_native_eq_midi)$'
build/linux/aion_compressor_processor_tests 64
build/linux/aion_compressor_processor_tests 128
build/linux/aion_eq_processor_tests 64
build/linux/aion_eq_processor_tests 128
build/linux/aion_midi_processor_tests
node tests/CompressorWebAudio.cjs --probe build/linux/aion_compressor_probe --fixtures build/compressor-browser-fixtures.json
node tests/AudioParityAudit.cjs build/linux/aion_reference_probe
cmake -S . -B build/overlap-asan
cmake --build build/overlap-asan --parallel 2 --target aion_compressor_tests aion_compressor_connected_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --output-on-failure -R '^compressor_(realtime|connected)_contract$'
build/linux/aion_effects_engine_benchmark
```

`overlap-asan` is the retained Debug/plugin-OFF cache with
`-fsanitize=address,undefined -fno-omit-frame-pointer`. Logs: Windows
`build/compressor-windows-focused-ctest.log`, `compressor-ui.log`,
`compressor-eq-ui.log`, `compressor-legacy-processor-{64,128}.log`,
`compressor-midi-processor-retry.log`; Linux `build/compressor-linux-focused-ctest.log`,
`compressor-final-connected-test.log`, `compressor-final-processor-{64,128}.log`,
`compressor-final-connected-asan-test.log`, `compressor-linux-midi-processor.log`.
Browser replay/audio audit and timing logs use the corresponding platform names.
Logs and fixtures remain ignored local evidence.

### Connected callback observations

Release Engine `process`, 48 kHz, twenty playing stereo loops, all EQ+C enabled,
512 warm / 2048 measured callbacks per run. Automation changes both effect banks
every sixteen callbacks. Accepted fixtures require all twenty sources playing
and nonzero post-MASTER output; insufficient-pool/silent-MASTER runs were rejected.

| Platform | Block | Automated | Mean us | p99 us | Maximum us | Observed overruns |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| Windows | 64 | No | 434.122 | 531.5 | 636.1 | 0 |
| Windows | 64 | Yes | 524.120 | 651.8 | 809.5 | 0 |
| Windows | 128 | No | 998.684 | 1253.4 | 1766.1 | 0 |
| Windows | 128 | Yes | 851.236 | 1012.7 | 1392.1 | 0 |
| Linux | 64 | No | 397.704 | 497.077 | 561.222 | 0 |
| Linux | 64 | Yes | 376.059 | 473.532 | 670.427 | 0 |
| Linux | 128 | No | 783.762 | 911.638 | 1028.92 | 0 |
| Linux | 128 | Yes | 768.464 | 912.743 | 1008.92 | 0 |

Deadlines: 1333.33/2666.67us. Includes LoopBank, smoothing, core mix and bounded
telemetry; excludes device/driver/UI and total round-trip latency. These are
observations, not a worst-case guarantee. Logs:
`build/compressor-{windows,linux}-engine-timing.log`.

## Reference and acceptance defined before implementation

`resources/ui/Modules/loopTracks.js` `_createCompressor` creates a built-in Web
Audio compressor, then linear makeup gain, wet gain, and an **immediate dry path**.
There is no dry delay compensation. `effects.js` supplies seven controls and eleven
presets. `applyCompressorPreset` calculates makeup from threshold/ratio (capped
at 25 dB), ignoring the literal preset gain. Applying the "default" preset therefore
does not equal untouched default gain 2. `smoothSetParam` targets over 50 ms.
The original display's quadratic knee is a display, not the actual DSP transfer
curve; do not replace the browser's exponential-knee detector with that drawing.

| Observable requirement | Current native evidence | Remaining |
| --- | --- | --- |
| All seven controls, original defaults, eleven calculated presets | Exact Float32 handler/range comparisons; GUI/CLI/MIDI/state | Physical controller/listening checks |
| Stereo linked detector, mono duplication, wet lookahead, immediate dry path | Actual unchanged `_createCompressor` PCM; connected routing/toggles | Full overlapping browser graph transitions |
| Attack/adaptive release, knee, makeup, linear gain/mix | Browser oracle <=1e-6 peak / <=1e-5 relative RMS; connected live state/history/STOP/PLAY | Linux WebKit audio/interaction |
| Variable callbacks and sample rates | Fixed storage/subdivision/reset, production prepare, 64/128 Engine timings | Driver/round-trip/long-session checks |
| Invalid controls, PCM and ownership | Atomic rejection, numerical recovery, callback new/delete, full queues/clear/restore, sanitizers | Integration/release stress |

`Compressor` is a single-owner fixed-storage kernel with a 1024-frame stereo delay.
It retains the browser's `min(floor(.006f * rate), 1023)` wet lookahead, including
the original high-rate cap; do not call this total application latency. Parameters
are k-rate at 128 frames, detector-envelope updates at 32; both clocks survive
64/128 and arbitrary caller subdivisions. Makeup/wet/dry gains are a-rate, with
independent Float32 smoothing. There is no per-control dynamic storage ownership
transfer. Dry export and existing capture are unchanged.

## Faust and source choice

Faust is owner-authorized as an option. Its maintained [compressor library](https://faustlibraries.grame.fr/libs/compressors/)
provides compressor and lookahead building blocks. They are not evidence of this
particular Web Audio knee, automatic makeup or adaptive envelope matching. For this
slice the smaller parity path adapts the [original browser DSP](https://github.com/chromium/chromium/blob/39095d03d67beb956ec7983cb4a51833536dc894/third_party/blink/renderer/platform/audio/dynamics_compressor.cc)
and [AudioParam timeline](https://github.com/chromium/chromium/blob/39095d03d67beb956ec7983cb4a51833536dc894/third_party/blink/renderer/modules/webaudio/audio_param_handler.cc)
from pinned revision `39095d03d67beb956ec7983cb4a51833536dc894` into portable C++.
The logarithm uses a small portable scalar adaptation of [AMD's BSD non-FMA
algorithm](https://github.com/amd/aocl-libm-ose/blob/5760c0a6a5c650aaeaee81d5d83592fc2da66207/src/isa/avx/masm/log10f.asm),
pinned at `5760c0a6a5c650aaeaee81d5d83592fc2da66207`, retaining arithmetic order
and tables without assembly or CPU dispatch. Full BSD notices and modification
provenance are in `third_party/notices` and
included by installation/package rules. No runtime Chromium, fdlibm, Faust library
or Faust compiler is introduced. Evaluate Faust for later effects when it reduces
work **and** unchanged-reference audio acceptance demonstrates equivalent behavior.

## Failing-then-fixed acceptance

Initial static PCM passed, but rapid threshold retargeting failed by 0.0326056.
The original compressor's k-rate `.value` anchors the preceding quantum's first
sample, unlike a-rate GainNode parameters. Recursive Float32 smoothing, separate
dry/wet histories and the browser's practical convergence threshold are required.
One remaining 0.0000410 failure was exactly the first sample at frame 14336:
`currentTime = frame/rate` round-trips just above that integer frame. The original
GainNode leaves that sample on the old event and starts the new timestamped event
at its ceil boundary. The fix retains the prior boundary sample without moving
the new automation's anchor or phase. All failed cases remain; no limit was loosened.
The first JS topology check also needed `Array.from` to compare a VM-realm array;
its graph-link and original-value assertions were retained.

The first Linux replay passed its six core/reference checks, then exposed a
**0.00007605** vocal-preset mismatch: platform `log10f` differs by one ULP and
changes a knee-search branch. Simply rounding a double logarithm repaired that
preset but failed the retained 44.1 kHz automation case by **0.00005912**. Neither
failure was waived. The portable AMD arithmetic matches the Windows CRT on
10,000 sampled inputs; sixteen cases that differ from double-log rounding are
retained as exact component regressions alongside full browser PCM acceptance.
Compressor compilation disables FMA contraction on non-MSVC compilers to retain
the separate Float32 operations. This is source arithmetic, not an AMD hardware
requirement. A subsequent **1.43084e-6** sustain failure exposed another detail:
Blink's `fdlibm::powf/asinf` wrappers evaluate their double functions before
rounding to float. The native float overloads were corrected accordingly. A final
**1.19463e-6** automation failure at the next knee search exposed glibc float-power
rounding of the linear threshold; double evaluation with the original Float32
exponent restores the measured browser result. All original cases remain and
the same peak/RMS gates apply. Final platform replay results follow below.

Actual WebView2 **Edg/151.0.4129.107** supplied **60 comparisons, 1,966,080
stereo frames**. Final Windows and Linux native replay both pass at maximum
absolute difference **4.806921385380747e-7**, unchanged
limits **1e-6 peak / 1e-5 relative RMS**. Cases: 8/22.05/32/44.1/48/96/192 kHz
defaults, all eleven presets at 48 kHz, silence/impulse/step, linked stereo/mono,
immediate dry, partial mix, zero attack/unity ratio, extreme settings and original
live retarget/repeated-target sequences at 22.05/44.1/48/96 kHz, all at 64/128.
`build/compressor-browser-windows.log` records the original acquisition;
`build/compressor-replay-windows-final.log` and `build/compressor-linux-final.log`
record final native replay. Runtime contexts are test-only,
in an owned blank document; the production mixer is never bootstrapped there.

The ignored `build/compressor-browser-fixtures.json` records exact browser version,
input/output PCM, events and reference-method SHA256. `--fixtures` verifies that hash
before replay against another native build. Linux replay is **Windows browser oracle
versus Linux C++**, not proof of Linux WebKit's rendering or interaction.

## Reproduce

Configured Windows core build (Visual Studio CMake/CTest on PATH):

```powershell
cmake --build build/windows-core --config Release --parallel 2 --target aion_compressor_tests aion_compressor_probe aion_compressor_benchmark
ctest --test-dir build/windows-core -C Release --output-on-failure --no-tests=error -R '^(compressor_realtime_contract|original_compressor_contract)$'
# Requires the already-built, owned development aion_mouse_host, no physical device:
./tools/TestCompressorWebAudio.ps1
# Replay the freshly captured browser PCM without opening another WebView:
node tests/CompressorWebAudio.cjs --fixtures build/compressor-browser-fixtures.json
./build/windows-core/Release/aion_compressor_benchmark.exe
```

Commit the coherent Windows snapshot and run the existing WSL workspace helper
before Linux commands; never overwrite a dirty mirror. Browser fixture copying is
explicit test data transfer, not source/cache synchronization:

```sh
cd /home/jorge/projects/Aion-Looper
cmake --build build/linux --parallel 3 --target aion_compressor_tests aion_compressor_probe aion_compressor_benchmark
ctest --test-dir build/linux --output-on-failure --no-tests=error -R '^(compressor_realtime_contract|original_compressor_contract)$'
cp /mnt/d/Jorge/Documents/Codex/Aion-Looper/build/compressor-browser-fixtures.json build/compressor-browser-fixtures.json
node tests/CompressorWebAudio.cjs --probe build/linux/aion_compressor_probe --fixtures build/compressor-browser-fixtures.json
build/linux/aion_compressor_benchmark
cmake --build build/overlap-asan --parallel 2 --target aion_compressor_tests
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --output-on-failure --no-tests=error -R '^compressor_realtime_contract$'
```

The first sanitizer command could not find the new target in the older Makefiles
cache. Reconfigure it once with `cmake -S . -B build/overlap-asan` before building
the new target; this preserves its existing Debug, `AION_BUILD_PLUGIN=OFF` and
`-fsanitize=address,undefined -fno-omit-frame-pointer` flags. The rerun passes.

## Final verification — code 3c0769e

Final new component/reference CTests: Windows **2/2 (.37s)** and Linux **2/2 (.62s)**.
Linux ASan/UBSan/leak checks **1/1 (1.76s)**, no diagnostics. Logs:
`build/compressor-final-core-windows.log`, `build/compressor-linux-final.log`,
`build/compressor-asan-linux.log`. Original asset bytes and retained EQ kernel/
automation passed in the earlier six-test groups, Windows **6/6 (43.08s)** and
Linux **6/6 (44.45s)**; later repairs change only compressor math, not EQ/assets.
Zero watched new/delete includes controls, reset, rate changes and processing.
No GUI/CLI/processor, physical device, plugin, listening or Linux WebKit acceptance
is claimed for the disconnected C feature. The owned browser host was used only
as an isolated audio oracle; existing product EQ/CLI/UI code was not changed.

Manual Release measurements: twenty stereo compressor kernels, 48 kHz, 512 warm
and 2048 measured callbacks per scenario. Each retarget run changes threshold,
ratio, makeup and mix every sixteen callbacks on all twenty kernels. Windows
then Linux were measured sequentially after other build/test jobs finished.

| Platform | Block | Automation | p99 us | Max us | Overruns / 2048 |
| --- | --- | --- | ---: | ---: | ---: |
| Windows | 64 | No | 306.7 | 346.2 | 0 |
| Windows | 64 | Yes | 316.3 | 359.9 | 0 |
| Windows | 128 | No | 543.8 | 769.7 | 0 |
| Windows | 128 | Yes | 572.9 | 704.0 | 0 |
| Linux | 64 | No | 259.005 | 293.750 | 0 |
| Linux | 64 | Yes | 263.897 | 370.029 | 0 |
| Linux | 128 | No | 438.377 | 719.089 | 0 |
| Linux | 128 | Yes | 483.965 | 630.224 | 0 |

Logs: `build/compressor-benchmark-{windows,linux}-final.log`. Callback periods
are 1333.33/2666.67us. These are **isolated kernel** measurements: no EQ, LoopBank,
production callback, driver or round-trip latency, and no guaranteed worst case.
No package, wrapper build, dependency upgrade or GitHub push was performed.

## Next slice and remaining limits

Inventory original A (Arp Delay) behavior before implementing its smallest complete
shared GUI/CLI slice. Preserve the accepted compressor cases above. Remaining
effects/full-chain/wet SAVE stay disabled. Separate platform/hardware/listening
and release checks listed above remain pending; software gates are not release acceptance.
