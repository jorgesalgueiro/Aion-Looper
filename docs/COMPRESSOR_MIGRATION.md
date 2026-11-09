# Compressor migration — isolated DSP slice

The original **C** effect remains disabled in the product. This slice establishes
its DSP/preset/automation contract before connected GUI/CLI/MIDI/state work.
No preserved UI/reference file, existing parameter/action ID or session format
changes. No product AudioContext, dependency upgrade, package or wrapper build.

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
| All seven controls, original defaults, eleven calculated presets | Exact Float32 preset handler comparison | Product commands, state and widgets |
| Stereo linked detector, mono duplication, wet lookahead, immediate dry path | Actual unchanged `_createCompressor` OfflineAudioContext PCM | Connected LoopBank routing/ownership and toggles |
| Original attack/adaptive release, knee, automatic makeup, linear gain/mix | Static and live browser oracle, peak <=1e-6 and relative RMS <=1e-5 | Connected special-state/transport/history acceptance |
| Variable callbacks and sample rates | Fixed storage, exact subdivision/mono/recovery/reset checks | Production callback timing with EQ + compressor |
| Safe invalid controls and PCM | Atomic rejection, NaN/Inf quarantine, extreme finite recovery, zero new/delete | Product queue exhaustion/cancellation/restore lifecycle |

`Compressor` is a single-owner fixed-storage kernel with a 1024-frame stereo delay.
It retains the browser's `min(floor(.006f * rate), 1023)` wet lookahead, including
the original high-rate cap; do not call this total application latency. Parameters
are k-rate at 128 frames, detector-envelope updates at 32; both clocks survive
64/128 and arbitrary caller subdivisions. Makeup/wet/dry gains are a-rate, with
independent Float32 smoothing. There is no audio-thread storage ownership transfer
yet because the effect is not connected. Dry export and existing capture are unchanged.

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

## Next connected slice

Append the compressor settings/actions without renumbering EQ/MIDI or state IDs;
validate versioned restore atomically. Connect Q -> C to the shared LoopBank DSP,
bounded GUI/CLI commands, original C checkbox/panel/curve and presets, then original
selected-effect MIDI Learn. Define persistent/rebuilt compressor ownership and
live special-state behavior from reference/owner decisions before choosing it;
EQ-specific approvals must not silently become compressor sound changes. Retain
raw capture/dry export, source tails, queued gain and MASTER ordering. Test toggles,
clear/panic/prepare, editor reopen, sessions and 64/128 real processor/UI PCM before
enabling the product control. Remaining effects/full-chain/wet SAVE stay pending.
