# SDK and toolchain inventory

Only dependencies needed by the selected audio targets are included. SDK caches
and installed tools are excluded from Git; the source build pins its dependencies.

| Component | Location / acquisition | Scope |
| --- | --- | --- |
| JUCE 9.0.4 | `.deps/JUCE`, pinned CMake FetchContent | Common framework and platform adapters |
| VST3 SDK | JUCE `modules/juce_audio_processors_headless/format_types/VST3_SDK` | Windows VST3; Linux target removed |
| LV2 SDK | JUCE `modules/juce_audio_processors_headless/format_types/LV2_SDK` | Native JUCE LV2 wrapper |
| CLAP SDK + extension | `.deps/clap-juce-extensions`, including recursive submodules | Desktop CLAP |
| ASIO interface headers | JUCE `modules/juce_audio_devices/native/asio` | Windows ASIO build; device driver is separate |
| WebView2 SDK 1.0.3485.44 | `.deps/nuget/Microsoft.Web.WebView2.1.0.3485.44` | Windows embedded editor |
| WebView2 runtime | Existing Windows installation | Browser execution; separate from SDK |
| NuGet CLI 7.9.0.83 | `.toolchain/bin/nuget.exe`, Microsoft-signed official download | SDK package restore |
| MSVC + Windows SDK + CMake | Microsoft Build Tools 2022 C++ workload | Native Windows compilation |
| GCC/CMake/Ninja/Python | Existing Ubuntu installation | Linux build and asset embedding |
| ALSA/JACK/WebKitGTK/GTK/X11/OpenGL development packages | Existing Ubuntu packages, see BUILDING.md | Linux audio and editor |
| Xcode, macOS/iOS SDKs, AudioUnit/CoreAudio frameworks | Install Xcode on a supported Mac | AU/AUv3, macOS/iOS compilation and signing |

The downloaded WebView2 NuGet archive SHA256 is
`bc09150b179246ac90189649b13be8e6b11b3ac200e817e18df106e1f3cf489e`.
CMake uses the SDK parent directory via `JUCE_WEBVIEW2_PACKAGE_LOCATION`.

Build Tools 2022 17.14.42, MSVC toolset 14.44.35207 and Windows SDK 10.0.26100.0
are installed. CMake 3.31.6-msvc6 runs, and the MSVC core tests pass. The final
Microsoft installer result requests a Windows restart to finish setup. Windows
configure/tests use Codex's bundled Python 3.12.14 through `Python3_EXECUTABLE`;
this is already cached for the configured Windows build trees.

Microsoft's Build Tools workload is `Microsoft.VisualStudio.Workload.VCTools`
with recommended components, including the Windows SDK and CMake tools. The local
installation is under `.toolchain/vs2022`; the package cache uses `.deps/vc` because
Microsoft limits that cache path to 50 characters. Use the installer's `vswhere.exe`
to locate CMake/MSVC rather than assuming a fresh shell has updated PATH.

A hardware manufacturer's ASIO driver must match the actual interface model; a
"Topping" brand name alone is insufficient to select a driver. No generic driver
is installed or device configuration changed by SDK setup.

Apple's SDKs ship through Xcode and require macOS. This Windows/Ubuntu computer
cannot compile/test/sign the Apple targets locally. Use a Mac with Xcode and the
provided macOS/iOS presets. No third-party repackaged Apple SDK is substituted.

Official setup references:
- https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022
- https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.3485.44
- https://developer.apple.com/xcode/system-requirements/

See VALIDATION.md for installation/build results; an SDK's presence does not imply
that hardware or plugin-host behavior has been validated.


## Optional Windows VST3 validation tool

[Tracktion pluginval 1.0.4](https://github.com/Tracktion/pluginval/releases/tag/v1.0.4)
is a local test dependency, not a bundled SDK or product runtime. The official
pluginval_Windows.zip asset was retrieved from that exact release on 22 October
2026. Its pinned SHA256 is
C08E61CE3B96DB41636F8EC7E76F4C7E2C13EBDAC7FA1B5A1F52B4F32EC715AB;
the extracted executable is
F4AC5C31C5544A434F73E686B816861D92C22AE849D69AD693681CBC9522FCD8.
These are fingerprints of the retrieved official asset; GitHub supplied no
upstream asset digest. They identify the tested bytes, not an independent
publisher signature. tools/TestVst3Pluginval.ps1 -DownloadTool performs the
explicit project-local setup; ordinary builds do not fetch the tool.

Primary usage/CI guidance: [pluginval README](https://github.com/Tracktion/pluginval)
and [official CI guide](https://github.com/Tracktion/pluginval/blob/v1.0.4/docs/Adding%20pluginval%20to%20CI.md).
The pinned [CMake source](https://github.com/Tracktion/pluginval/blob/v1.0.4/CMakeLists.txt)
omits ExtremeTests.cpp; do not infer allocation-test coverage from a strictness
number. BUILDING.md documents the exact commands and retained coverage limits.


## Optional Windows CLAP validator

Official [free-audio/clap-validator release 0.4.1](https://github.com/free-audio/clap-validator/releases/tag/0.4.1),
Windows asset clap-validator-0.4.1-127-g152b982-windows.zip:

- Archive SHA256 (also published in GitHub release API):
  D935C3AF0A45C3911EA2E900F4AA5D6709DAC82BB485F0C4CE28648AB2CD0C10
- Extracted executable SHA256:
  825439AD3FF98770A24656013EEB11591407687B97058E1DB9549C4F7D53E38A
- Ignored project-local directory: .toolchain/clap-validator/0.4.1.

TestClapValidator.ps1 -DownloadTool performs explicit setup, not a build-time
network dependency. Full usage: [official README](https://github.com/free-audio/clap-validator).
State cache refresh follows the [CLAP parameter specification](https://github.com/free-audio/clap/blob/main/include/clap/ext/params.h)
and pinned clap-juce-extensions audioProcessorChanged implementation. No wrapper
fork or dependency upgrade was required. See BUILDING.md for the opt-in gate.


## Optional Ubuntu x86_64 CLAP validator

The same [official 0.4.1 release](https://github.com/free-audio/clap-validator/releases/tag/0.4.1)
provides clap-validator-0.4.1-127-g152b982-ubuntu-22.04.zip.

- Published archive SHA256:
  49edadcfb407ea0dd946ce418300e853fbd2660fa4b0d00e4f19ff8eef24ad90
- Executable SHA256, extracted from that verified archive:
  d9a78bfe781719de450bc5013141496eb13e1d9a538c67de1b8d325628670dbf
- Ignored location: .toolchain/clap-validator/0.4.1-linux.

The official zip contains a tarball with a historical 0.3.2 filename; the actual
executable reports 0.4.1. TestLinuxClap.py checks archive and executable hashes and
reads only the regular clap-validator member without general path extraction.
--download-tool explicitly sets up this local tool, then validates the built
Debug CLAP by default. The gate is optional and never downloads during CMake.
Linux dependencies follow [JUCE's official Linux dependency documentation](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).
The current machine already supplies ALSA/JACK/GTK/WebKit development libraries;
no system package installation was needed for this increment.


Routing/lifetime checks also consulted the official
[StandalonePluginHolder API](https://docs.juce.com/master/classjuce_1_1StandalonePluginHolder.html)
and [MidiDeviceListConnection API](https://docs.juce.com/master/classjuce_1_1MidiDeviceListConnection.html),
plus the actual pinned holder constructor: currentInstance is assigned before
processor creation. The existing connection remains owned/reset by AionProcessor
in Standalone; plugin hosts supply their MIDI through processBlock.


## Optional Linux x86_64 pluginval

Official [Tracktion pluginval v1.0.4](https://github.com/Tracktion/pluginval/releases/tag/v1.0.4)
asset: pluginval_Linux.zip, downloaded directly from that release (no third-party
mirror). The release API supplies no digest for this historical asset; these
SHA256 values pin the downloaded official archive and its sole binary:

- Archive: c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352
- Executable: d3d342ea92293b0699a5c983937211e3faf2b0f38145b31cb8d78424a25e10ba
- Ignored location: .toolchain/pluginval/1.0.4-linux/pluginval.

TestLinuxVst3.py --download-tool reads only the expected pluginval ZIP member,
checks both hashes and runs the opt-in gate. Ordinary CMake builds never download
it. This official binary is built with JUCE 8.0.3; the product retains pinned
JUCE 9.0.4. No dependency upgrade, system install or redistribution is implied.
GUI checks require an X display; see BUILDING.md for commands and VALIDATION.md
for outcomes and the omitted allocation/external-validator limitations.


The Linux pluginval 1.0.4 pin is also used by TestLinuxLv2.py; no additional
validator download or SDK installation is required. Lv2AudioProbe.py follows
[official LV2 Atom layouts](https://github.com/lv2/lv2/blob/main/include/lv2/atom/atom.h)
and the core/urid/options/atom/patch headers in the product's pinned JUCE LV2_SDK.
It sends block-start patch:Set messages through the public sequence port. Python
ctypes supplies only host structures/buffers, not an alternate product processor.
