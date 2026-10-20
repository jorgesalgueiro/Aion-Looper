# Build and package

This is a foundation/preview. Only tests actually recorded in VALIDATION.md are
verified; commands for other systems are setup instructions, not validation claims.
CMake >=3.22, C++20 and Python >=3.8 are required. Dependencies are fetched at pinned
revisions. Offline overrides: `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE` and
`-DFETCHCONTENT_SOURCE_DIR_CLAP_JUCE_EXTENSIONS=/path/to/clap-juce-extensions`.
JUCE overrides must be the pinned commit, not an arbitrary installed version.

## Portable core

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

Tests cover bounded/concurrent SPSC, sample-deadline gain, smoothing, 64/128 block
invariance, nonfinite input and no C++ new/delete in core processing. They do not
certify all host/JUCE paths, hardware latency or full feature parity.

## Ubuntu/Linux

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git python3 pkg-config   libasound2-dev libjack-jackd2-dev libfreetype6-dev libfontconfig1-dev   libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev   libxrandr-dev libxrender-dev libgl1-mesa-dev libegl1-mesa-dev   libwebkit2gtk-4.1-dev libgtk-3-dev
cmake --preset linux
cmake --build --preset linux --parallel 2
ctest --preset linux
cmake --install build/linux --prefix dist/linux
cpack --config build/linux/CPackConfig.cmake -B dist -G ZIP
```

Targets: `AionLooper_Standalone`, `AionLooper_VST3`, `AionLooper_CLAP`,
`AionLooper_LV2`, `aion_core_tests`. JUCE natively supplies LV2; do not use an old
fork of JUCE_LV2. The browser subprocess is embedded for plugin hosts. Default
JUCE Linux backends include ALSA/JACK when their development libraries are present;
this is not evidence of real JACK hardware validation.

## Windows x64

Install Visual Studio 2022 with Desktop development with C++, Windows SDK, CMake,
Python 3 and NuGet. In a developer PowerShell:

```powershell
# Locate the installed Build Tools and portable NuGet on this PC:
$vsRoot = & 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe' -latest -products '*' -property installationPath
$env:Path = "$PWD/.toolchain/bin;$vsRoot/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin;$env:Path"
nuget install Microsoft.Web.WebView2 -Version 1.0.3485.44 -OutputDirectory .deps/nuget
cmake --preset windows -DJUCE_WEBVIEW2_PACKAGE_LOCATION="$PWD/.deps/nuget"
cmake --build --preset windows --parallel 2
ctest --test-dir build/windows -C Release --output-on-failure
cmake --install build/windows --config Release --prefix dist/windows
cpack --config build/windows/CPackConfig.cmake -C Release -B dist -G ZIP
```

After a version bump, verify Windows file metadata matches CMake's project version:

```powershell
(Get-Item 'build/windows/AionLooper_artefacts/Release/Standalone/Aion-Looper.exe').VersionInfo | Select-Object FileVersion,ProductVersion
```

The WebView2 SDK version above is the version recommended by pinned JUCE's
FindWebView2.cmake. Users also require the Evergreen WebView2 Runtime. The CMake
option selects WebView2 explicitly, preventing legacy Internet Explorer fallback.
Optional ASIO (JUCE 9 bundles the default headers; their licence still applies):

```powershell
cmake --preset windows -DJUCE_WEBVIEW2_PACKAGE_LOCATION="$PWD/.deps/nuget" `
  -DAION_ENABLE_ASIO=ON
```

To use separately obtained headers, add `-DAION_ASIO_SDK="D:/SDKs/ASIOSDK"`.
That root must contain `common/iasiodrv.h`; the build enables JUCE_ASIO_USE_EXTERNAL_SDK.
No external ASIO SDK is required for the default headers. The local toolchain setup
and installation results are recorded in SDKS.md and VALIDATION.md. Native Windows
build/hardware verification remains separate from SDK installation.

## macOS universal, AU and AUv3

Install Xcode/command-line tools, CMake and Python. Xcode is required for AUv3.

```sh
cmake --preset macos
cmake --build --preset macos --parallel 2
ctest --test-dir build/macos -C Release --output-on-failure
cmake --install build/macos --config Release --prefix dist/macos
cpack --config build/macos/CPackConfig.cmake -C Release -B dist -G ZIP
```

Targets include Standalone, VST3, CLAP, AU and AUv3. Add
`-DAION_BUILD_LV2=ON` if a macOS LV2 target is wanted. Product signing, hardened
runtime settings and notarization must be configured with the owner's identities;
this source does not embed certificates or disable release security to fake signing.

## iOS device and simulator

Run on macOS with Xcode. iOS builds Standalone and AUv3, not desktop formats.

```sh
cmake --preset ios -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=YOUR_TEAM_ID
cmake --build --preset ios -- -allowProvisioningUpdates
# Separate simulator tree; never mix device and simulator caches:
cmake -S . -B build/ios-simulator -G Xcode -DCMAKE_SYSTEM_NAME=iOS   -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0   -DAION_BUILD_AUV3=ON -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build/ios-simulator --config Debug --target AionLooper_Standalone AionLooper_AUv3
```

For signed distribution, archive the generated Xcode app scheme and export with
the owner's provisioning/export options, e.g. `xcodebuild -project
build/ios/AionLooper.xcodeproj -scheme AionLooper_Standalone -configuration Release
-destination 'generic/platform=iOS' -archivePath dist/Aion-Looper.xcarchive archive`.
The exact signing/export plist depends on the Apple account and distribution type.
AUv3 containing-app, microphone permission, interruption/background behavior and
mobile WebKit lifecycle require real device tests before distribution.

## Validation before a release

Load each artifact in REAPER/Ardour and representative AU/AUv3 hosts. Run pluginval,
CLAP validator and `auval`; add their pinned versions/results to CI before claiming
compliance. Verify clock seeks/wraps, state restore, MIDI disconnect/panic, editor
reopen, webview isolation, sanitizers and 64/128 maximum-load callback allocation/
CPU tests. Packaging is not signing, notarization, installation or feature parity.

## Native loop preview quick test (0.4.2)

Build Windows Standalone/VST3/CLAP with `-DAION_ENABLE_ASIO=ON`, then open the
Standalone executable. In Options select the interface/ASIO driver and buffer size.
Enable input with JUCE's Unmute Input control. The native input meter should respond.
Raise VOLUME in the original MASTER OUTPUT panel; MONITOR in INPUT MIXER is
optional and defaults off. The original loop cards use native DSP: RECORD,
FINISH, STOP and PLAY. Press `+` to toggle global overdub or `-` to toggle
substitute, then press a playing loop's main button; FINISH exits that capture
mode. Feedback defaults to 80% and double-click resets it. The original STOP ALL
and DEL ALL buttons act on the bank. SOLO is exclusive and preserves explicit
mutes. RTRG (or `/`) resets Start to zero with sync OFF, or snaps it to the
current master sixteenth with STATIC/DYNAMIC sync. Playing replacements start
5ms after the tap; stopped loops start immediately without waiting for the grid.

Keys 1-0 address loops 1-10; Shift+1-0 address 11-20. Holding a loop key for
600 ms stops a playing loop or clears a stopped loop, matching the reference
press/release behavior. Space stops all and `/` retriggers the selected loop.
Shortcuts ignore text/form focus, repeats and Ctrl/Alt/Meta combinations; lost
focus cancels hold timers. Auto-play after Rec now controls whether FINISH starts
playback or leaves the take stopped. Native sync remains OFF until implemented.

Use headphones/direct monitoring as appropriate for the physical setup. Session
recordings are temporary: DAW/project state currently stores parameters, not PCM.
Export and undo remain disabled. Changing sample rate clears session audio; changing
buffer size at the same rate retains PCM but stops playback. These are preview
limits, not completed reference parity.

### Native WebView2 UI smoke test

With Node.js 22+ installed, start a dedicated test instance and use its temporary
loopback debugger port. This test keeps output muted, never records microphone
input, checks visible loop controls and exercises parameter validation/Stop All.
Close other preview test instances first.

```powershell
$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = '--remote-debugging-address=127.0.0.1 --remote-debugging-port=9234'
$preview = Start-Process 'build/windows/AionLooper_artefacts/Release/Standalone/Aion-Looper.exe' -WindowStyle Hidden -PassThru
Remove-Item Env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
node tests/NativeUiSmoke.cjs
$preview.CloseMainWindow()
```

Screenshots are written under `build/`. Ordinary launches do not enable the debugger.

The Windows preview ZIP contains an EXE and plugin bundles, not an installer. It
requires the Microsoft Visual C++ 2015-2022 x64 Redistributable and Evergreen
WebView2 Runtime. The local MSVC build imports MSVCP140/VCRUNTIME140/140_1. SDKs,
development libraries and browser profiles are not included in the runtime ZIP.

### Native pan and speed (0.4.0)

Use the existing Pan and Speed sliders. Speed ranges from 0.25x to 2x in manual
mode. The 1/2 button (or H on the selected loop) toggles 1x to 0.5x and any other
rate back to 1x. The 1x button and double-clicking Speed reset to unity. Select a
loop by pressing its key or clicking inside its card. Rate changes preserve the
playback phase and ramp for 20 ms. Pan uses the reference equal-power 0-10 law.
The reference graph folds stereo to mono before its left/right pan gains; the
native output now does the same, including the center attenuation. This changes
the previous preview's stereo passthrough playback; captured PCM is still stereo.

Overdub/substitute require speed 1x after the ramp settles; speed changes during
recording or an active edit are rejected. The current fractional reader uses
linear interpolation and is not a high-quality antialiasing resampler. Browser
DSP waveform equivalence and listening/hardware validation remain pending.

### Capture finalization preview (0.4.2)

FINISH displays the existing SAVING label while a worker processes the take. New
capture/edit entry waits for that job. Existing audio continues unchanged during an
edit; its completed result replaces it at a callback block boundary. Stop prevents
late autoplay; clear discards the result. A failed take reports its failure and
keeps prior audio. This is asynchronous processing, not immediate sample-zero
FINISH playback. Session recordings remain temporary and are not project storage.

`ctest --test-dir build/windows -C Release --output-on-failure` runs the four
component suites. Run `node tests/AudioParityAudit.cjs
build/windows/Release/aion_reference_probe.exe` for the connected PCM gate; on Linux
use `build/core/aion_reference_probe`. Do not skip this gate because components pass.


The core CTest suite also runs frontend command-ordering tests and the connected
NORM comparison when Node is available. For detailed normalization PCM results:

```sh
node tests/NormalizationParity.cjs build/core/aion_reference_probe
```

On Windows use `build/windows/Release/aion_reference_probe.exe`. These tests
execute the unchanged reference DSP, not a reimplementation of the JS oracle.


The incremental core suite includes `js_native_reverse`, comparing original JS
source selection/PCM with native reverse playback. To inspect its scoped report:

```sh
node tests/ReverseParity.cjs build/core/aion_reference_probe
```

Use the existing standalone build for interactive checks during migration.
Routine feature changes do not require a version bump or new ZIP/plugin package.


The core CTest suite now includes `js_native_history`. To inspect its unchanged-JS
history and restored-PCM comparison directly:

```sh
node tests/HistoryParity.cjs build/core/aion_reference_probe
```

On Windows use `build/windows/Release/aion_reference_probe.exe`. Undo/Redo testing
uses the same existing standalone build; no new package/version is needed.

The core CTest suite includes `js_native_multiply`. Run its unchanged-JS
capture/append/doubling/history and committed PCM report directly with:

```sh
node tests/MultiplyParity.cjs build/core/aion_reference_probe
```

On Windows use `build/windows/Release/aion_reference_probe.exe`. Only the existing
Standalone target needs rebuilding for this migration increment; no ZIP, installer
or plugin-format package is required.

The main workflow gate is `js_native_sync`. It runs the unchanged JS recorder,
SyncManager helpers, extraction/DC/fade functions and effective-rate getter against
the native clock/capture path:

```sh
node tests/SyncParity.cjs build/core/aion_reference_probe
```

Use `build/windows/Release/aion_reference_probe.exe` on Windows. Rebuild only
`AionLooper_Standalone`, `aion_core_tests`, `aion_reference_probe`, and (when core
changes require relinking) `aion_dsp_probe`, then run CTest and AudioParityAudit.
The WebView2 smoke test uses the existing localhost port 9234 test setup. For
repeatable native input tests, an isolated `WEBVIEW2_USER_DATA_FOLDER` under
`build/` avoids sharing a test browser session; test-only
`--disable-background-timer-throttling --disable-renderer-backgrounding` flags and
CDP focus emulation model foreground input. The app stays muted and fixtures do
not capture a microphone. Close and wait for the test instance to exit. No SDK,
plugin-format build, installer, ZIP or version bump is needed for this increment.


### Hardware-free mouse acceptance (Windows)

Build the optional test host and the existing preview app, without packaging:

```powershell
& .toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe --build build/windows --config Release --target aion_mouse_host AionLooper_Standalone --parallel 4
& tools/TestMouseWorkflow.ps1
```

The runner exercises both 64- and 128-sample blocks on localhost CDP port 9235.
It links the production processor/editor/IPC, supplies generated stereo PCM on a
separate processing thread, and checks actual committed-loop playback. It never
opens an audio device or uses snapshot fixtures. Its private WebView profiles
live under `build/`; existing WebView environment settings are restored and each
owned host is closed. A port already in use is an error. The optional target is
excluded from default builds and is never installed or packaged. This supplements
`NativeUiSmoke.cjs` (port 9234); it does not validate physical device routing,
OS audio scheduling, monitoring latency, or the Standalone wrapper's input gate.

Run `TestMouseWorkflow.ps1` and `NativeUiSmoke.cjs` serially. Concurrent native
GUI windows can lose OS pointer capture/focus even with CDP focus emulation;
a cancelled hold must not be mistaken for an uninterrupted hold. Headless CTest
and audio comparisons can run independently alongside either GUI suite.

The existing `tools/TestMouseWorkflow.ps1` gate now also clicks both original
Start nudge buttons against real captured PCM at 64/128 samples. The existing
`tools/TestCountInUi.ps1` runner additionally runs `tests/NudgeWebAudio.cjs` to
reproduce the tiny-clip negative-offset browser failure and verify playable
wrapped offsets, before the native bridge/empty-loop/invalid-direction smoke.
These use the incremental build and require no additional package or install.

`js_native_waveform` uses the existing `aion_start_probe` to compare immutable
worker peaks against unchanged JS generation. `native_frontend_commands` also
compares original canvas operations across window widths, states and DPR values.
`tools/TestMouseWorkflow.ps1` now checks actual captured-waveform pixels and cache
payload omission at 64/128 samples, saving `build/native-mouse-waveform.png`.
Continue using incremental targets; no extra package/install is required.

For live DYNAMIC input acceptance, incrementally build `aion_live_input_probe`
alongside Standalone and `aion_mouse_host`. The existing `tools/TestCountInUi.ps1`
runner also runs `tests/LiveInputWebAudio.cjs` (hardware-free OfflineAudioContext
mono/stereo downmix comparison). `tools/TestMouseWorkflow.ps1` checks native red
trace/yellow markers, next-take ownership and mid-take sync-off playhead at 64/128,
saving `build/native-live-input.png`. No additional package/install is needed.


Hold the existing **STUT** button on a playing loop to repeat its current short
fragment. Release anywhere to resume the full loop at the fragment's current
cursor. The fragment follows the current BPM when pressed; it is bounded by half
the clip and its end, with a 10ms requested minimum. Blur, pointer cancellation,
editor reload/close, STOP ALL and deletion release STUT. Starting a replacement
source also clears its temporary region. This does not edit the recording.

The focused source gate is `js_native_stutter`. `tools/TestCountInUi.ps1` also runs
`tests/StutterWebAudio.cjs` against the unchanged JS methods in actual WebAudio.
`tools/TestMouseWorkflow.ps1` checks real production PCM and pointer ownership at
64/128 samples; run GUI/CDP scripts with local-process/localhost access and one
GUI suite at a time. Rebuild existing targets; no package/version bump is needed.

### Export a recorded loop

Click the existing loop waveform, then choose the WAV destination. This exports
stored stereo PCM16 at the capture sample rate, like the original web interface;
playback volume/pan/reverse/speed/Start/STUT are not baked in. Empty waveforms do
nothing. Canceling preserves audio/history. Closing or reloading the editor cancels
an unchosen destination. A selected background write completes.

No new installer is needed: incrementally rebuild the existing Standalone. The
focused gate is `ctest --test-dir build/windows-core -C Release --output-on-failure
-R "core_realtime_contract|js_native_wav_export"`; Ubuntu uses `build/core`.
`tools/TestMouseWorkflow.ps1` runs retained main workflows and export mouse/file
checks at 64/128; `-ExportOnly` runs just export checks. The fixture substitutes a
deterministic picker and writes test WAVs under `build/windows/Release/wav-export-64`
and `wav-export-128`; it opens no audio device. `aion_mouse_host --native-picker`
instead exercises the real JUCE picker with synthetic audio. Do not package the
test host. Run `AudioParityAudit.cjs` and `TestCountInUi.ps1` as usual.

The test picker completes only after a `<filename>.choose` marker in its export
directory. The supplied scripts manage this; no timed picker assumptions are
needed. Count-in diagnostics use `pcm-evidence.json`, `pcm-reset` and `pcm-stop`
in the same directory, recording actual callback PCM/state without a device.
Launching `aion_mouse_host` without arguments also selects the real picker and
localhost CDP port 9235. `node tests/NativePickerProbe.cjs prepare` records synthetic
PCM and opens it; `export`, `status` and `cancel` support subsequent checks. This
debug port/configuration belongs only to the excluded test host, not release apps.
