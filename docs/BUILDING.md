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
