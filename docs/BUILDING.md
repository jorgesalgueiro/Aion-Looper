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
and DEL ALL buttons act on the bank. `X` invokes DEL ALL with the same original
confirmation; cancelling preserves every loop. SOLO is exclusive and preserves
explicit mutes. RTRG (or `/`) resets Start to zero with sync OFF, or snaps it to
the current master sixteenth with STATIC/DYNAMIC sync. Playing replacements start
5ms after the tap; stopped loops start immediately without waiting for the grid.

Keys 1-0 address loops 1-10; Shift+1-0 address 11-20. A press starting on STOP/
STOP NOW cuts playback at 600ms and deletes audio only after four uninterrupted
seconds. Early release, blur, cancellation and STOP ALL cancel deletion; holding
RECORD/FINISH/CANCEL never deletes a take. Space stops all and `/` retriggers the
selected loop. Text editing and Ctrl/Alt/Meta protect ordinary shortcuts;
Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z provide Undo/Redo. The original `X` fallback also
requests confirmation on key repeat. Confirmed DEL ALL cancels held gestures and
older queued actions; cancelled confirmation sends no clear command. Auto-play
after Rec controls whether FINISH starts playback or leaves the take stopped.
If a loop key-up is missed, release the physical key and press it again after
more than ten seconds from its original press. The fresh press performs the
current loop action; OS repeat does not recover it or reset the recovery age.
Holding RECORD longer than ten seconds never automatically finishes/deletes it.
Internal Master BPM STATIC/DYNAMIC sync is implemented; host/MIDI clock is pending.

Use headphones/direct monitoring as appropriate for the physical setup. Session
recordings are temporary: DAW/project state currently stores parameters, not PCM.
Loop WAV export and loop/global Undo/Redo are implemented. Changing sample rate
clears session audio; changing buffer size at the same rate retains PCM but stops playback. These are preview
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

### Merge loops

Use the existing MERGE dropdown on the destination loop, select a source and
press GO. An empty destination receives an independent, stopped copy. An existing
destination keeps its length and mixes the source circularly into stored audio;
Undo/Redo restores the previous/merged take. Accept the original confirmation to
clear the source, or decline to keep it. Stale confirmations cannot delete a new
source recording; STOP ALL cancels pending source-clear notices.
Merge is available when capture/finalization is idle, using the same single-worker
ownership restriction as native normalization. Wait for a take to finish before
merging it.

Incrementally build the existing Standalone and `aion_mouse_host` targets. Run
`ctest --test-dir build/windows-core -C Release --output-on-failure -R
"core_realtime_contract|js_native_merge|native_frontend_commands"` (Ubuntu uses
`build/core`). `tools/TestMouseWorkflow.ps1 -MergeOnly` runs focused native mouse,
confirmation and saved-PCM checks at 64/128; the default runner retains main
performance and WAV export gates as well. Test WAVs remain under the existing
build directories. No installer, ZIP or test-host installation is needed.

### MIDI looper testing

Enable MIDI in the original mapping panel. In Standalone select the desired
native input port; in a plugin route MIDI from the host. Click a loop's MIDI
field and send a Note, CC or Program Change to learn it. Right-click clears it;
Escape cancels learning; SAVE MAPPINGS persists native settings (a DAW saves
plugin state). Learning consumes the message without performing its action.
Defaults are notes 36-45 for loops 1-10, 46 OD, 47 SUB and 48 SUS. Loops 11-20
can be learned. PC 0 is valid. Program Changes act once per message; CC acts on
a positive edge and must return to zero before another action. Neither is a
held-delete gesture. Notes retain 600ms cut and four-second uninterrupted
STOP-origin deletion. The existing EN/ES/PT manual panels include these rules.

Incrementally build AionLooper_Standalone, aion_mouse_host and the excluded
`aion_midi_processor_tests` in the existing Windows tree. Run:

```powershell
ctest --test-dir build/windows-core -C Release --output-on-failure
build/windows/Release/aion_midi_processor_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File tools/TestMouseWorkflow.ps1 -MidiOnly
```

The default mouse runner also retains main/export/merge checks at both 64/128.
Ubuntu core uses `ctest --test-dir build/core --output-on-failure` under WSL;
ASan/UBSan uses build/core-asan with ASAN_OPTIONS=detect_leaks=1. GUI suites run
sequentially. Synthetic MIDI file injection exists only in the excluded test
host; release builds have no MIDI injection RPC. The optional real-device helper
`node tests/MidiHardwareProbe.cjs status` observes an explicitly debug-launched
Standalone on localhost 9236 and never synthesizes physical MIDI. No package or
version bump is needed. Continuous expression/parameter learn, synth MIDI,
clock I/O and gamepad remain separate increments.

Foot-controller burst handling: repeated identical PC messages within 20ms count
as one tap. Separate taps at/after 20ms remain valid. This prevents the tested
controller's three packets per pedal press from stopping and restarting a loop.
With sync enabled, STOPPING waits for the grid boundary, then stays STOPPED.

### MIDI expression: loop volume and pan

Right-click an existing loop volume or pan slider, then move an expression pedal,
knob or fader that sends MIDI CC. Only the learn event is consumed, including CC zero. Following movement controls
the slider immediately, even before its mapping acknowledgement reaches the UI.
Volume follows the original 0..2 range. Pan follows the original integer 0..10
steps. Repeated positive CC values are continuous, unlike button CC mappings.
Shift+right-click clears that slider's binding; Escape or 10 seconds cancels learn.
SAVE MAPPINGS stores these with the original Note/CC/PC assignments. Older saved
pedal mappings load unchanged. MIDI clock, synth and other continuous targets
remain separate work.

Use the same existing Standalone, aion_mouse_host and aion_midi_processor_tests
build targets. js_native_midi now also compares the unchanged CC handler and
original volume/pan setters. core_realtime_contract compares exact stereo PCM
against scheduled native slider controls at 64/128. TestMouseWorkflow.ps1 -MidiOnly
covers real slider right-click/clear, CC zero/repeats/pan steps, duplicate mapping
rejection, ten-second timeout, Escape and reload. The default runner retains all
main/export/merge tests. No new package or version bump is needed.

### Minimal Windows test distribution

After the existing Release Standalone passes its gates, run
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/PackageStandalone.ps1`.
It packages only the already-built Standalone, test instructions, current scope
and dependency notices into one commit-labelled ZIP under `dist/`. No installer,
plugin-format rebuild, SDK cache or version bump is required. Extract it and run
`Standalone/Aion-Looper.exe`; Evergreen WebView2 Runtime is required. The manifest
records the executable SHA256, source commit and any tracked local overrides
(including a locally edited logo), so a local preview is not mistaken for a clean
commit build. Physical audio/driver and DAW validation remain separate gates.

### IPC reload regression

`native_bridge_lifecycle` exercises old-document completions with reused numeric
IDs, current-token success/error, queue/deadline recovery and pagehide cleanup.
`tools/TestMouseWorkflow.ps1 -MidiOnly` also tests actual WebView2 reload delivery
against the production native processor at 64/128. After that focused run, use
`tools/TestMouseWorkflow.ps1 -SkipMidi` for main/WAV/merge acceptance without
repeating MIDI; the default runner still runs every workflow. These are existing
local test targets and do not require a new package.


### Numeric keypad regression

`tools/TestMouseWorkflow.ps1` includes actual CDP keypad record/finish/play/stop
checks at both 64 and 128 samples. Numeric keypad 1..0 selects loops 1..10 even
with Shift. Main-row Shift+digits retains the existing upper-bank behavior.
NumLock-off navigation (Insert, End, arrows, Home, PageUp/PageDown and Clear)
issues no loop commands. Releasing a numeric press after NumLock/Shift changes
must cancel its old cut/delete timers and preserve playback after four seconds.
These synthetic browser events exercise the production processor/editor with
real captured PCM; physical keyboard/OS/layout checks remain separate acceptance.


### Windows VST3 host-interface validation (opt-in)

The built VST3 can be loaded and tested by pinned Tracktion pluginval 1.0.4.
This uses the public VST3 interface, including editor opening/automation,
parameter/state/thread tests and audio processing. It supplements direct native
processor tests; it does not replace the original-JS/PCM or physical/DAW gates.
Run from the repository root after the Windows setup above:

```powershell
cmake --build --preset windows --target AionLooper_VST3 --parallel 2
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File tools/TestVst3Pluginval.ps1 -DownloadTool
```

DownloadTool is explicit and optional. It downloads the official fixed release
only when its local archive is missing, verifies pinned archive/executable
SHA256 values and unpacks into ignored .toolchain/pluginval/1.0.4. It does not
install plugins or tools globally, package the app, or change audio/MIDI settings.
Omit DownloadTool for subsequent offline runs. Alternate copies of the same
pinned executable can be passed with ValidatorPath. Default strictness is 9,
seed 1337, rates 44100/48000/96000 and blocks 64/128/256/512/1024, with GUI tests
retained. Every run saves vendor logs plus result.json under build/pluginval;
the report fingerprints the executable, VST3 binary, harness and source checkout,
checks exact test completion and matrix coverage, and rejects a changed binary.
Inherited validator options are sanitized for the child and restored afterwards.
The wrapper waits for pluginval's exit; a bare GUI executable invocation in
PowerShell must not be mistaken for completed validation.

To register the same check in this native CMake build:

```powershell
$pluginvalPath = (Resolve-Path '.toolchain/pluginval/1.0.4/pluginval.exe').Path
cmake -S . -B build/windows "-DAION_PLUGINVAL_EXECUTABLE:FILEPATH=$pluginvalPath"
ctest --test-dir build/windows -C Release -R '^vst3_pluginval$' --no-tests=error --output-on-failure
```

The serial test has a 300-second CTest timeout and keeps pluginval's 30-second
no-progress watchdog. Default/portable builds never require the tool or download
it. Clear AION_PLUGINVAL_EXECUTABLE to disable this optional CTest entry.

Scope limits: the official release's CMake source omits ExtremeTests.cpp, so its
allocation test is not executed even at level 9. result.json reports this as
allocationTestExecuted=false. The external Steinberg validator is also absent
and explicitly reported as unexecuted. Preserve the separate instrumented native
allocation tests; this is not full RT allocation certification, recorded-loop
project validation, physical controller/driver testing, a REAPER/Ardour session,
or validation of CLAP/LV2/AU/AUv3 or another operating system.


## Optional Windows CLAP validation

Use the existing build; no package, plugin installation or physical device is
needed. From the repository root in PowerShell, after building the CLAP target:

```powershell
# One-time project-local download: official 0.4.1 Windows asset and SHA256 checks.
./tools/TestClapValidator.ps1 -DownloadTool
# Subsequent direct runs are offline:
./tools/TestClapValidator.ps1

$cmake = './.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = './.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
$validator = (Resolve-Path '.toolchain/clap-validator/0.4.1/clap-validator.exe').Path
& $cmake -S . -B build/windows "-DAION_CLAP_VALIDATOR_EXECUTABLE:FILEPATH=$validator"
& $cmake --build build/windows --config Release --target AionLooper_CLAP --parallel 2
& $ctest --test-dir build/windows -C Release -R '^clap_validator$' --no-tests=error --output-on-failure
```

The gate is opt-in and serial. Ordinary portable/default builds do not require or
download a validator. Run it after any processor/state/CLAP-wrapper changes. All
44 pinned tests must appear exactly once for this module/ID; unapproved failures,
warnings or skipped supported tests fail the gate. TOML configuration in the run
directory or ancestors is refused to prevent silent filtering. Logs and
result.json under build/clap-validator retain source/tool/harness/binary hashes,
arguments, statuses, skips and warnings; the binary must stay unchanged.

Current Windows result: 33 pass, 10 skips (Unix dynamic linking, float64 audio,
CLAP-native wildcard notes, preset discovery and optional port-layout extensions),
and one retained state-invalid-random warning. JUCE's setStateInformation returns
void, so the wrapper cannot report rejected random bytes; direct processor tests
verify rejection without state mutation/host notification. This gate does not
test the GUI or prove zero allocation, actual DAW/device operation, other OSes,
complete project persistence or full Ascii-Looper parity.


## Original input-wheel behavior

Scrolling over a slider or number field without Shift leaves its value unchanged
and removes focus, matching Ascii-Looper. Holding Shift lets the browser handle
that wheel gesture normally; it does not add a custom adjustment mode. Text
fields and other targets retain ordinary scrolling/focus behavior. Native audio
parameters are still changed through the existing IPC controls.

The portable native_frontend_commands test compares the unchanged original
listener across 630 cases, including capture/passive flags and blur receiver.
TestMouseWorkflow.ps1 additionally sends trusted browser wheel events to MASTER,
loop Volume, BPM and a loop name, at 64 and 128 samples, then runs the existing
mouse/audio/export/merge/MIDI workflows. The host uses synthetic PCM and opens no
physical audio device. Browser cancellation uses explicit passive:false as in
[the wheel-event documentation](https://developer.mozilla.org/en-US/docs/Web/API/Element/wheel_event).


## Optional Linux production-processor and CLAP gates

These supplement the portable core suite with the actual JUCE processor and ELF
CLAP wrapper. No plugin installation/package or physical audio device is needed.
The processor executable uses JUCE GUI initialization; the recorded run uses the
existing WSLg DISPLAY. It does not open an editor/browser window.
The recorded native Linux checks use the existing Debug tree, not a Release
latency benchmark. From the repository root in Ubuntu:

```sh
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Debug \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/.deps/JUCE" \
  -DFETCHCONTENT_SOURCE_DIR_CLAP_JUCE_EXTENSIONS="$PWD/.deps/clap-juce-extensions" \
  -DAION_TEST_NATIVE_PROCESSOR=ON
cmake --build build/linux --parallel 2 --target \
  AionLooper_Standalone AionLooper_VST3 AionLooper_CLAP AionLooper_LV2 \
  aion_midi_processor_tests
# Explicit one-time local validator setup + complete validation:
python3 tools/TestLinuxClap.py --download-tool
# Register the optional format gate now that the executable exists:
cmake -S . -B build/linux \
  -DAION_LINUX_CLAP_VALIDATOR_EXECUTABLE="$PWD/.toolchain/clap-validator/0.4.1-linux/clap-validator"
ctest --test-dir build/linux -R '^(native_midi_processor|linux_clap_validator)$' \
  --no-tests=error --output-on-failure
# Subsequent direct validator runs need no network:
python3 tools/TestLinuxClap.py
```

AION_TEST_NATIVE_PROCESSOR also exposes the same optional CTest on Windows;
explicitly build aion_midi_processor_tests first. Both native tests are serial,
opt-in and excluded from default builds. Portable/core tests still need no JUCE
validator. The Linux CLAP gate pins the official Ubuntu x86_64 tool/archive,
requires all 44 tests exactly once, requires Unix RTLD_NOW to execute, rejects
ancestor TOML filters and records hashes/statuses/limits under build/linux-clap-validator.
Its timeout terminates only its own validator process group. Unsupported optional
extensions and the existing JUCE void state-load warning remain explicit; GUI,
arbitrary malloc/allocation, real DAW/device operation and full project recall
are outside this vendor test's coverage.


Host-routed/synthetic processors do not subscribe to OS MIDI device changes.
Only an actual Standalone holder requests that watcher. This allows native
processor/host tests without an ALSA sequencer device while retaining Standalone
hotplug cancellation. midiDevices.deviceWatchRequested is routing diagnostics,
not proof that a physical controller or OS notification was tested.

## Optional Linux VST3 host validation

The existing Linux VST3 can be checked through the official Tracktion pluginval
1.0.4 host. This gate requires an X display (WSLg works); editor tests stay
enabled. It needs no audio device, plugin installation or package. From Ubuntu
at the repository root, after building AionLooper_VST3:

```sh
# Explicit one-time project-local tool download and validation:
python3 tools/TestLinuxVst3.py --download-tool
cmake -S . -B build/linux \
  -DAION_LINUX_PLUGINVAL_EXECUTABLE="$PWD/.toolchain/pluginval/1.0.4-linux/pluginval"
ctest --test-dir build/linux -R '^linux_vst3_pluginval$' \
  --no-tests=error --output-on-failure
# Subsequent direct runs need no network; override --plugin for another tree:
python3 tools/TestLinuxVst3.py
```

The helper pins both official archive and executable hashes, uses level 9 and
seed 1337, and requires completed editor/automation/state/thread/fuzz tests plus
all 15 combinations of 44100/48000/96000 Hz and 64/128/256/512/1024 samples.
Inherited pluginval options are removed only from the child environment so
SKIP_GUI_TESTS or DISABLED_TESTS cannot weaken coverage. The parent environment
is unchanged. An outer 240-second timeout terminates only the owned validator
process group. stdout/stderr and result.json record source, tool, harness,
before/after module hashes and explicit test limits under build/linux-pluginval.
The serial optional CTest has a 300-second limit; default builds need no tool.

This tests the public VST3 wrapper and editor lifecycle, not the rendered HTML
control workflow. The runtime policy deliberately blocks original inline scripts
and legacy WebAudio modules; WebKit may print CSP refusal messages for those
blocked resources. Native adapters are injected separately as JUCE user scripts.
The recorded WSLg run also reports a libEGL/DRI3 acceleration warning. Neither
message is evidence of HTML interaction or accelerated-rendering acceptance.
The official binary omits the allocation test, and no external Steinberg
validator is configured: result.json records both as false. Actual DAW sessions,
recorded-loop project recall, JACK/ALSA hardware, Release latency/CPU and Apple
platforms remain separate gates.


## Optional Linux LV2 host and stereo audio validation

The existing JUCE LV2 bundle has two complementary acceptance checks: official
pluginval 1.0.4 editor/host tests and a public LV2 ABI nonzero stereo probe. They
use the built Debug bundle without installing a plugin or creating a package.
From Ubuntu at the repository root, after building AionLooper_LV2:

```sh
# Reuses the same pinned official Linux tool as the VST3 gate:
python3 tools/TestLinuxLv2.py --download-tool
cmake -S . -B build/linux \
  -DAION_LINUX_LV2_PLUGINVAL_EXECUTABLE="$PWD/.toolchain/pluginval/1.0.4-linux/pluginval"
ctest --test-dir build/linux -R '^linux_lv2_(pluginval|audio)$' \
  --no-tests=error --output-on-failure
# Direct runs; --plugin can select another build tree:
python3 tools/TestLinuxLv2.py
python3 tests/Lv2AudioProbe.py \
  --plugin build/linux/AionLooper_artefacts/Debug/LV2/Aion-Looper.lv2
```

TestLinuxLv2.py reuses the pinned runner in TestLinuxVst3.py, retaining level 9,
seed 1337, all required completion markers, editor tests and the 15 rate/buffer
combinations. The existing VST3 entry point and defaults remain compatible.
LV2 reports are separate under build/linux-lv2-pluginval. They record the format,
entry-point/shared-harness hashes and before/after hashes of libAion-Looper.so,
manifest.ttl, dsp.ttl and ui.ttl. Missing metadata or a wrong-format bundle fails
before host execution. No DISPLAY fails instead of silently skipping GUI tests.
Both formats sanitize only the child pluginval environment and retain the same
owned-process timeout, assertion/leak checks and coverage limits.

Lv2AudioProbe.py loads the actual LV2 descriptor with ctypes and public LV2
core/URID/options/atom/patch structures. It checks descriptor URI and port metadata,
provides required host features and connects distinct stereo input/output buffers.
It sends patch:Set masterGain/inputMonitor values through the LV2 sequence port,
checks default mute, unity/half gain, monitor off, master zero and unmute recovery.
At 44100/48000/96000 Hz and 64/128 samples it compares settled stereo PCM after the
existing 20ms gain ramp, requires finite overwritten output and nonzero samples,
and verifies zero reported latency. It hashes the module/metadata before and
after the run and emits JSON evidence. The 90-second serial CTest rejects JUCE
assertion/leak output. Python host buffers/URI mapping are not allocation profiling.

These are public-wrapper checks, not physical audio/MIDI, full HTML interaction,
recorded-loop/project DAW workflows or Release CPU/latency certification. The
vendor binary still omits allocation tests. Expected blocked-legacy-script CSP
and WSLg libEGL warnings remain documented above; no browser policy was loosened.
The official LV2 [Atom layouts](https://github.com/lv2/lv2/blob/main/include/lv2/atom/atom.h)
and the pinned JUCE LV2 wrapper/SDK are the ABI references used by the probe.


## Original double-click loop slider resets

Double-click Speed to return to 1.0x or Feedback to return to 80%, including while
the range has keyboard focus. Focus stays on the control, as in the original JS.
The native input adapter now explicitly moves the focused thumb; the existing
handler still sends the normal native parameter command. No extra synthetic
input/change event, blur, style change or audio-thread work is introduced.

native_frontend_commands compares the unchanged original reset/setter methods
across 560 cases covering all 20 tracks, both reset types, initial values and
sync on/off. tools/TestMouseWorkflow.ps1 uses trusted double clicks on the actual
focused controls at 64/128 samples, verifies DOM/native reset values and retains
playing state, committed clip identity/length/history and nonzero audio. The
remaining mouse/export/merge/MIDI workflows run afterward. No package is needed.
Event ordering was checked against the [browser dblclick documentation](https://developer.mozilla.org/en-US/docs/Web/API/Element/dblclick_event).

## Real Windows REAPER and Ubuntu Ardour sessions

Run these checks against existing binaries; no install, ZIP or version bump is
needed. REAPER uses a new resource directory and process for each format, removes
the test master hardware outputs, and closes only that process. It never copies
your configuration/license or modifies your existing project. From the repo:

```powershell
python tools/TestReaper.py
# Optional installed host location / one-format diagnostic:
python tools/TestReaper.py --reaper 'C:/Program Files/REAPER (x64)/reaper.exe' --format CLAP
```

The Windows probe requires the existing Release VST3 and CLAP in build/windows.
It observes the exact module path in the owned REAPER process and hashes it before
and after. At 48kHz it renders default mute, unity, half gain, monitor off, master
zero, recovery and project reopen. Every target render is compared sample by
sample after the first 100ms, with finite/nonzero checks and a tolerance derived
from the WAV format. Track-state restore is checked after deliberately mutating
both parameters. With the audio device closed while stopped, the script processes
queued changes through renders before state reads, mutation checks and restore
checks. RPP files and all intermediate renders are retained for diagnosis. This
is offline processing, not 64/128 hardware callback or real-time latency evidence.
See the official [REAPER API](https://www.reaper.fm/sdk/reascript/reascripthelp.html).

From Ubuntu, with the existing Debug LV2 bundle and a working WSLg/display:

```sh
# One-time test-host dependency, if not already installed:
sudo apt-get install --no-install-recommends ardour
python3 tools/TestArdour.py
```

This runs Ardour's own ardour8-lua session engine, documented in its
[Lua scripting manual](https://manual.ardour.org/lua-scripting/). Each 64/128 run
uses an isolated HOME/XDG profile, a stereo bus and the Dummy sine backend at
48kHz. It loads the actual local LV2, discovers all twelve APVTS controls through
Ardour's property-automation API, tests native mute/unity/half gain/off/recovery
and saves/reopens the session. MIDI meter channels precede audio channels; the
probe derives the offset from the actual meter configuration. Stereo peak gain
ratios are checked independently of host control state. This is not a PCM or GTK
editor-interaction oracle. Lua errors can return exit code zero, so explicit PASS
and error/assertion/leak rejection remain mandatory. Metadata/module hashes must
stay unchanged. Ardour 8.4 does not load CLAP; its Linux acceptance path is LV2,
with CLAP validator coverage kept separately.

Evidence is retained under build/reaper-daw and build/ardour-daw in unique run
folders (JSON, logs, synthetic audio and disposable sessions). Neither test proves
recorded-loop project storage, MIDI clock, host-follow scheduling, arbitrary
callback malloc freedom, physical ASIO/JACK/CoreAudio or Apple acceptance.

## Requested deployment format selection

The existing CMake options already support the requested matrix. Explicitly select
these options when configuring deployment trees; optional extra wrappers in older
cache/default configurations do not establish additional release support:

```sh
cmake --preset windows -DAION_BUILD_STANDALONE=ON -DAION_BUILD_VST3=ON -DAION_BUILD_CLAP=ON
cmake --preset linux -DAION_BUILD_STANDALONE=ON -DAION_BUILD_VST3=OFF -DAION_BUILD_CLAP=ON -DAION_BUILD_LV2=ON
cmake --preset macos -DAION_BUILD_STANDALONE=ON -DAION_BUILD_AU=ON -DAION_BUILD_VST3=OFF -DAION_BUILD_CLAP=OFF -DAION_BUILD_AUV3=OFF
cmake --preset ios -DAION_BUILD_STANDALONE=ON -DAION_BUILD_AUV3=ON
```

Use each platform's own toolchain and the build commands above. The Apple commands
are instructions for a Mac, not validation performed on this Windows/Ubuntu PC.
Only package after the selected feature's acceptance passes and a test package is
needed; the current increment adds validation tools and creates no package.

## Original synchronized Speed control

In STATIC/DYNAMIC sync, native Speed edits now use the original UIManager setter:
values below 0.38 select 0.25x, below 0.75 select 0.5x, below 1.5 select 1x,
and the rest select 2x. The focused thumb snaps as it does in the original JS.
OFF keeps free speed and the original focused-thumb behavior. The existing half/
reset actions share this routing; setting a range value creates no extra native
command. No original asset, DSP algorithm or host-sync authority changes.

native_frontend_commands executes the unchanged original setLoopSpeed across
1,560 mode/focus/track/boundary cases. tools/TestMouseWorkflow.ps1 uses trusted
ArrowRight input on focused original ranges at both 64/128, checking both native
rate and visible position while preserving clip state, identity, history and
nonzero recorded audio. This supplements the existing DSP/effective-rate and
connected PCM gates; it does not certify browser resampling or driver latency.
The browser [range value semantics](https://developer.mozilla.org/en-US/docs/Web/HTML/Reference/Elements/input/range)
were checked while keeping the original snap thresholds unchanged.

The retained SyncParity.cjs native/reference matrix now includes 0.25x as well as
0.5x/1x/2x: 144 effective-rate comparisons across manual, STATIC and DYNAMIC
settings, alongside its unchanged capture/extraction PCM cases. Run it directly
with the existing aion_reference_probe or through js_native_sync.


The loop status display regression is part of `native_frontend_commands` and
`tools/TestMouseWorkflow.ps1`. The source oracle executes unchanged
`UIManager.updateLoopDisplays` with canvas drawing absent, comparing state
symbols/text and mute/duration formatting across 20 loops. The production-editor
mouse workflow checks pending STOP `[.]` while recorded audio continues, then
stopped `[S]` after a second STOP, preserving PCM length and undo history at
64/128. Rebuild the existing app/mouse host after changing embedded adapters;
no installer, ZIP or version bump is needed for this display repair.


Global performance-status parity is also included in `native_frontend_commands`
and `tests/MidiWorkflow.cjs` through `tools/TestMouseWorkflow.ps1` at 64/128.
The reference updater checks header precedence and independent capture colours;
the actual native test toggles global OD off during an ongoing overdub and checks
original computed background, recording label and preserved audio/history.
Native WebView2 tests need Windows desktop access. A CDP/browser startup timeout
is a test failure; preserve its log and close only the owned test host before a
retry. Do not interpret it as hardware or feature acceptance.


The LIVE PERFORMANCE loop section is covered by `native_frontend_commands` and
`tools/TestMouseWorkflow.ps1` at 64/128. The gate uses trusted pointer/keyboard
input against the production processor/editor: LIVE header record/finish/play/
stop, shared reverse/name state, unported-effect isolation, tab changes without
audio commands, and nonzero committed audio/clip/history preservation. Reference
oracles compare original card markup and updater/panel states independently.
Screenshots are retained as `build/native-live-loops-64.png` and `-128.png`.
The live master mixer, drone/sampler engines and other effects are still pending.
Use the existing app/test targets; this restoration needs no installer or ZIP.


LIVE loop mixer control checks use the same incremental build and
`tools/TestMouseWorkflow.ps1` at 64/128. They cover the original twenty volume/M/S
strips, focused gain zero/max, exact silent mute/solo exclusion and nonzero
recovery, shared main-view values, clip/history preservation and stable fader
identity across tab entry. `native_frontend_commands` retains 1,200 complete
original-renderer comparisons and 48 production strip-wheel cases checked against
the original inline handler. Existing non-Shift input wheel protection remains.
Screenshots: `build/native-live-mixer-64.png` and `-128.png`. This controls increment
does not complete RMS/held-peak meters or master/input/drone/sampler mixing.
Rebuild existing targets; no package or version bump is needed.


LIVE loop volume MIDI Learn is included in `tools/TestMouseWorkflow.ps1 -MidiOnly`
at 64/128. The normal full run includes the same MIDI gate. Right-click either
the main loop volume or its original LIVE fader to learn; Shift+right-click clears
the shared binding, Escape or ten seconds cancels. Save Mappings and editor reload
retain the same binding, without adding a native target or changing state schema.
The targeted test injects raw MidiBuffer packets through the synthetic host, never
the user's hardware MIDI input. Existing packages remain their named source
snapshot; rebuild the normal app/test targets to check this increment. No package
or version bump is required for this adapter fix.


Direct loop-action MIDI Learn uses the existing `tools/TestMouseWorkflow.ps1
-MidiOnly` gate at 64/128. Right-click a loop's main RECORD/PLAY/STOP/etc. button
to learn its existing Note/CC/Program Change mapping; Shift+right-click clears it.
The original MIDI mapping field is the same binding and still supports its usual
focus/blur and right-click-clear gestures. Escape or editor reload cancels direct
learning. Only the twenty primary loop action buttons are enabled in this step;
other action/effect buttons remain separate migration work. The new test covers
actual stop/play audio, same-target field handoff, consume-on-learn, CC rearming,
PC burst debounce, held-right-click isolation, save/reload/clear and clip/history
preservation. Rebuild existing targets; no new package/version bump is needed.


### LIVE loop-header MIDI Learn

In LIVE, right-click a loop header (including its name/number area) and send a
Note, CC or Program Change. It shares the primary loop button's existing MIDI
mapping. Shift+right-click clears that same binding; Escape cancels learning.
Save Mappings retains it across editor reloads. The yellow learning outline is
shared by the header, main button and original mapping field. FX controls and
card backgrounds are excluded, and disabled primary actions cannot start learning
from LIVE. This extends the requested native MIDI configuration without changing
the original visual layout. Run `tools/TestMouseWorkflow.ps1 -MidiOnly` for the
64/128 synthetic pointer/MIDI/audio gate; it does not verify physical hardware.


### Main header/state MIDI Learn

Right-click the main loop header, its number or state text to learn Note/CC/
Program Change. It uses the same binding as the primary button and LIVE header;
Shift+right-click clears, Escape cancels and Save Mappings retains accepted
assignments. All four aliases share the yellow learning outline. Names, waveforms
and other nested controls retain their own behavior and cannot start this header
learn action. Disabled primary actions also disable header learning. The actual
64/128 pointer/MIDI/audio gate remains `tools/TestMouseWorkflow.ps1 -MidiOnly`.


### MIDI panic and fresh-note recovery

STOP ALL / MIDI Stop cancels old Note gestures; the next NoteOn works immediately
without waiting for an old NoteOff. This also reclaims a full 64-Note press pool.
Active uncancelled repeats still debounce and CC buttons still need zero before
their next positive edge. The fix adds no controls, mapping changes or packages.
Run the portable `core_realtime_contract` / `js_native_midi` tests, the production
`native_midi_processor` gate and `tools/TestMouseWorkflow.ps1 -MidiOnly` at 64/128.
The core gate compares exact stored stereo WAV and covers seven cancellation
sources; only Stop/stopAll have an unchanged-JS transport oracle. ASan/UBSan core
and MIDI checks use the existing build/core-asan with leak detection enabled.

### Independent MUTE MIDI Learn

Right-click a loop's original MUTE/UNM button or its LIVE M button, then send a
Note, positive CC, or Program Change. The first packet learns without toggling;
the next press toggles latched mute. Release does not restore mute, and holding
this independent control does not cut/delete the loop. Both button locations
show the same binding. Escape cancels learning; Shift+right-click clears it.
Use Save Mappings to persist the assignment. Avoid duplicate input assignments.

Version 3 mapping state reads older version 1/2 mappings with new MUTE slots
unassigned. Mapping persistence does not implement recorded-loop/project saving.
The core CTest suites, native processor gate and TestMouseWorkflow.ps1 -MidiOnly
cover synthetic input at 64/128; -SkipMidi retains mouse/keyboard/export checks.
Physical MUTE pedal and recorded-loop DAW MIDI workflows need separate checks.
This increment rebuilds existing development binaries without a package or
version bump.

### Actual REAPER MIDI recording and MUTE gate

Use the existing development binaries; no installer or ZIP is needed:

```powershell
python tools/TestReaper.py --format CLAP --midi
python tools/TestReaper.py --format VST3 --midi
```

Both commands pass the current scoped gate after the per-channel VST3 bridge
repair. The original failing VST3 PC0 result remains in VALIDATION.md as regression
evidence. Each gate remains strict: Lua's terminal PASS only confirms renders;
Python independently validates PCM, phase and saved assignments. The default command without --midi retains
the older monitor-only scope. The both-format --midi command still stops at its first
failure; use separate commands to inspect format-specific evidence.

The MIDI gate records fresh loops from synthetic stereo via Note36 or channel-specific PC127, finishes them,
then tests assigned Note/CC/PC MUTE, duplicate pulses, releases and recovery. It
compares returned phase samples and center pan and reopens a project to exercise
the saved assignment/item by recording fresh audio. This is assignment persistence,
not saved-loop persistence or editor Learn/hardware testing. Only the disposable
instance's state is configured, using documented vst_chunk/clap_chunk APIs.
MIDI renders require verified 1x offline speed to allow worker finalization;
full-speed offline and real driver 64/128 acceptance are not certified.

Portable fixture positive/rejection checks run on Windows or Ubuntu:

```powershell
ctest --test-dir build/windows-core -C Release -R reaper_midi_fixture --output-on-failure
```

```sh
ctest --test-dir build/core -R reaper_midi_fixture --output-on-failure
```

The fixture rejects malformed/ambiguous state, silent failed playback, monitor
leakage, changed loop phase/pan and lost recalled assignments. REAPER's public
[ReaScript API](https://www.reaper.fm/sdk/reascript/reascripthelp.html) documents
the chunk, MIDI_Get/SetAllEvts and configuration APIs used. The config change uses
persist=0 within the owned isolated profile, never the user's project/preferences.

### Building and checking the VST3 program bridge

The normal native CMake build generates the patched wrapper from the pinned JUCE
source. No additional SDK or modified dependency checkout is required. The app's
UI and preset/mapping schema are unchanged; VST3 gains sixteen hidden routing
parameters and per-channel command lists required by the host protocol.

```powershell
cmake --build build/windows --config Release --target AionLooper_VST3 aion_midi_processor_tests aion_vst3_program_tests --parallel 2
ctest --test-dir build/windows -C Release -R "vst3_program_routing|vst3_wrapper_generation|native_midi_processor" --output-on-failure
python tools/TestReaper.py --midi
```

```sh
cmake --build build/linux --target AionLooper_VST3 aion_midi_processor_tests aion_vst3_program_tests -j2
ctest --test-dir build/linux -R 'vst3_program_routing|vst3_wrapper_generation|native_midi_processor' --output-on-failure
```

Core-only builds also include vst3_program_routing without JUCE/SDK dependencies.
The real host gate adds all sixteen channel-specific PC0/PC127 routes and rejects
wrong-channel and unmapped-program actions. It now checks 20 MIDI renders per
format plus the original seven monitor renders. REAPER VST3's observed PC capture
period is 7168 frames versus nominal 7200; its PC event timing remains a host
acceptance limitation, distinct from preserving supplied sample offsets. The
phase oracle measures one exact period before mute and compares recovery at the
same phase; it rejects ambiguity and keeps the original fixed Note timing and
CLAP 7200-frame checks. Use the existing rebuilt binaries; no package is required.


## Offline recording finalization regression

Keep the default paced test and run the distinct full-speed test after building
the existing Windows VST3/CLAP targets:

```powershell
python tools/TestReaper.py --midi --format both
python tools/TestReaper.py --midi --full-speed --format both
```

The latter requires --midi, explicitly sets project projrenderlimit=0 and keeps
all seven monitor/recall renders and twenty MIDI PCM/phase cases per format.
The default still verifies 1x pacing and at least 0.9 seconds per one-second
MIDI render. Full-speed is a host mode/configuration contract; system load and
render-dialog overhead need not make total test wall time shorter than audio.
Both commands use their own disposable REAPER profile, hardware outputs removed,
module identity/hashes and owned-process timeout. They do not test an existing
user session or physical MIDI device.

```sh
cmake --build build/core --target aion_offline_tests -j2
ctest --test-dir build/core --output-on-failure -R '^offline_finalization$'
```

For the Windows Visual Studio core tree use build/windows-core, --config Release
and ctest -C Release. The test checks exact entire-render PCM across 1/64/128
samples, mono/stereo, simultaneous handoffs, edits/history and worker ownership.
It does not replace the retained live/reference, sanitizer and production
processor gates. No installer or package is required for this repair.
