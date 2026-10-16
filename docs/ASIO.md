# Windows ASIO input

The ordinary published preview through 20261016.3 uses WASAPI. ASIO must be
compiled in; changing a device name in that preview does not enable it.

On this PC the connected E2x2 is exposed by `TOPPING Pro USB Audio Device`.
In the ASIO-enabled desktop app, start audio and select
`ASIO / TOPPING Pro USB Audio Device` in the existing input device selector.
Other registered drivers also appear; a registry entry alone does not mean its
hardware is connected or that it has input channels. A driver-open failure is
reported rather than silently switching to WASAPI.

For direct launch with this driver:

```powershell
& './dist/Aion-Looper-Windows/Aion-Looper.exe' --input-host ASIO --input-device 'TOPPING Pro USB Audio Device'
```

These launch options choose the default native input. An explicit selection in
the input selector takes precedence. Monitoring still starts off; enable MON to
hear input. Playback currently uses Web Audio and its Windows output device;
this is ASIO capture, not native ASIO duplex output.

## Build and test

The project-local Windows GNU build uses:

```powershell
./scripts/setup-asio.ps1
./scripts/build-desktop.ps1 -Asio
./scripts/test-desktop.ps1 -PhysicalInput -AsioDriver 'TOPPING Pro USB Audio Device' -Seconds 30
./scripts/package-windows.ps1 -SkipBuild -Asio
```

Setup downloads the official Steinberg SDK, libclang 18.1.1 Windows wheel
(verified against its PyPI SHA256), and official Clang 18.1.8 source headers.
7-Zip is required to extract the headers. Dependencies stay under `.toolchain`;
no global environment variables or driver settings are changed. Existing
CPAL_ASIO_DIR, LIBCLANG_PATH and BINDGEN_EXTRA_CLANG_ARGS take precedence.

The vendored asio-sys 0.4.0 has a narrow opaque-class binding fix for Windows
GNU C++ base-class tail padding. C ABI structure layout checks remain enabled.
Driver-list refresh reads registered names without loading each driver; opening
an input stops enumeration when it reaches the selected driver.

The recorder now collects 4096-frame chunks and transfers ownership on stop,
reducing large take serialization. Long-take fallback DC removal and waveform
scans yield between batches. Original UI sources remain unchanged. Diagnostics
also yield while scanning captured samples so they do not induce transport gaps.

## Verification and limits

The first ASIO test successfully captured at 48 kHz with 128-frame callbacks but
reproduced a stop-time underrun. After the responsive finalization fixes, a
15-second physical recording passed with zero reported underruns/overruns and
maximum observed packet gap 23 ms. Audio stayed in memory and output was muted.
Further results are recorded in INPUT_TESTING.md. These trials do not establish
long-session drift, audible quality, round-trip latency or ASIO output support.

ASIO test archives use the distinct `Aion-Looper-Windows-x64-ASIO-local.zip`
name. They are local testing builds. The downloaded SDK offers GPL-3.0 or a
separate Steinberg license, while this repository declares GPL-2.0-only; public
redistribution of a combined ASIO binary needs the licensing choice resolved.
This change does not alter the project's license or replace published previews.

The corrected native-finalization path also passed a 3-second ASIO recording with zero reported gaps and maximum packet gap 34 ms. Bulk DSP replies are now serialized on the worker and delivered in 32 KiB PCM parts with session/order validation, preventing large script delivery from blocking live input.

A second corrected 3-second ASIO trial passed with zero reported underruns/overruns (35 ms maximum packet gap). Rust audio tests and all 31 desktop unit tests pass with ASIO compiled in; 97 JavaScript and 131 packaged graphical checks pass.

Final strict Windows ASIO Clippy passed. The shared fixes also passed Linux desktop/CLI/CLAP packaging, the packaged CLAP ABI check and 132 Ubuntu graphical checks.
