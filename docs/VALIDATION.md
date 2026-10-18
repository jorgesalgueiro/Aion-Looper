# Foundation validation — 2026-10-18

Environment: Windows host, Ubuntu 24.04 under WSL; GCC 13.3.0, CMake 3.28.3,
Ninja 1.11.1, Python 3.12.3. Windows validation also used Microsoft Build Tools
2022 17.14.42 (MSVC toolset 14.44.35207), Windows SDK 10.0.26100.0,
CMake 3.31.6-msvc6 and bundled Python 3.12.14. JUCE 9.0.4 and CLAP extension revisions are pinned
in `cmake/Dependencies.cmake`. This validates a foundation, not feature parity.

## Completed

- JUCE-independent Debug configure/build succeeded.
- CTest: `core_realtime_contract` and `original_ui_bytes` passed (2/2).
- Core test: queue capacity/overflow/FIFO, 100,000 concurrent SPSC messages,
  exact sample-32 gain command, 20 ms ramp reaching target, matching 64/128-frame
  output, finite output after infinite input, sample counter and no observed
  C++ new/delete while processing. C malloc/aligned allocation and JUCE internals
  are outside this instrumentation; no hardware latency claim is made.
- All 16 preserved reference files match SHA256 and the available reference
  checkout byte-for-byte. ZIP generation is deterministic; modified/unknown
  files are rejected by the packer.
- Linux JUCE configure and initial Debug build succeeded for Standalone, VST3,
  CLAP and native LV2, including VST3 moduleinfo and LV2 manifest generation.
- Unsupported macOS AU selection on Linux fails with the intended diagnostic.
- Linux compiler accepted the processor/APVTS and browser resource-provider code.
- Microsoft C++ Build Tools installation completed. Its final result requested a
  Windows restart; no automatic restart was performed. CMake works and a small
  Windows API program compiled, linked against WinMM/COM/User32, and ran successfully.
- Windows Debug core configure/build succeeded with MSVC; both CTest tests passed (2/2).
- Full Windows JUCE configure succeeded for Standalone, VST3 and CLAP with ASIO ON
  and WebView2 SDK 1.0.3485.44. This is not a Windows plugin binary/hardware test.

## Native package and remaining validation

Final Linux Debug build and CTest passed (2/2) after the packaging fix. The ZIP
`dist/Aion-Looper-0.1.0-Linux.zip` is 122,535,458 bytes and contains Standalone,
VST3, CLAP and LV2 artifacts, including VST3/LV2 metadata. Checks passed for
`GetPluginFactory`, `clap_entry`, `lv2_descriptor`, linked library resolution,
ZIP integrity, and exclusion of JUCE development SDK files from the runtime ZIP.
The local validation cache uses GNU thin intermediate archives to reduce writes
on the Windows-mounted drive (`CMAKE_CXX_ARCHIVE_CREATE=<CMAKE_AR> qcT <TARGET>
<LINK_FLAGS> <OBJECTS>`); the packaged products are normal ELF binaries.
Windows native wrappers, ASIO/Topping input, physical ALSA/JACK/CoreAudio, macOS, iOS/AUv3,
host loading, browser rendering/IPC execution, plugin validators, signing and
notarization have not been tested here. No Windows package has been produced.

The current engine is smoothed stereo pass-through, muted at startup. UI controls
are disabled; dynamic panels are pending the native view-model adapter. MIDI is
passed through, not mapped or clock-synchronized. Loop, drone, tracker and FX DSP
ports remain pending; see `PARITY.md`.
