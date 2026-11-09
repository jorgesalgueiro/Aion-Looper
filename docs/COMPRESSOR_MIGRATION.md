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
Full BSD notices and modification provenance are in `third_party/notices` and
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

Windows actual WebView2 **Edg/151.0.4129.107** passes **60 comparisons, 1,966,080
stereo frames**, maximum absolute difference **9.586730955746248e-7**, unchanged
limits **1e-6 peak / 1e-5 relative RMS**. Cases: 8/22.05/32/44.1/48/96/192 kHz
defaults, all eleven presets at 48 kHz, silence/impulse/step, linked stereo/mono,
immediate dry, partial mix, zero attack/unity ratio, extreme settings and original
live retarget/repeated-target sequences at 22.05/44.1/48/96 kHz, all at 64/128.
`build/compressor-browser-windows.log` records results. Runtime contexts are test-only,
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

Linux and final benchmark results are recorded in the current handoff after execution.

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
