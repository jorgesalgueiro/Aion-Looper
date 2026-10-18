# SDK and toolchain inventory

Only dependencies needed by the selected audio targets are included. SDK caches
and installed tools are excluded from Git; the source build pins its dependencies.

| Component | Location / acquisition | Scope |
| --- | --- | --- |
| JUCE 9.0.4 | `.deps/JUCE`, pinned CMake FetchContent | Common framework and platform adapters |
| VST3 SDK | JUCE `modules/juce_audio_processors_headless/format_types/VST3_SDK` | Windows/macOS/Linux VST3 |
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
