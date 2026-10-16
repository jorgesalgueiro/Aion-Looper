# Native WAV import

The shared `looper-files` decoder accepts ordinary RIFF/WAVE PCM16, packed PCM24
and IEEE float32. It also accepts WAVE_FORMAT_EXTENSIBLE files with these
precisions, including 16-bit PCM in 24/32-bit containers and 24-bit PCM in 32-bit
containers. The CLI uses the same decoder as the desktop worker.

Extension parsing follows Microsoft's
[WAVEFORMATEXTENSIBLE specification](https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatextensible)
and [PCM container alignment rules](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible).
The decoder checks the complete PCM/IEEE subtype GUID, extension size, valid
precision, byte rate, frame alignment and zero padding bits. Truncated headers,
nonfinite float samples and oversized decoded takes are rejected.

Supported speaker masks are unspecified (zero), mono front-center (4) and stereo
front-left/front-right (3). Other assignments require explicit channel routing
and are rejected. Mono remains mono on export and duplicates internally for the
stereo engine. Float32 retains headroom and signed zero. Decoded integer files
export at their valid precision, so 24-in-32 input defaults to packed PCM24 output
in the CLI. Export headers remain the original ordinary RIFF format.

The desktop imports matching-context-rate files through Rust, up to one million
decoded frames and sixteen million file bytes. Integer conversion is calibrated
against the webview separately for each precision, container width and header
kind. The calibration uses an equivalent extensible header for extensible input;
ordinary PCM calibration is not assumed to apply to a different container.
Unknown calibration, unsupported files and worker errors use the preserved
browser decoder with an independent copy of the original bytes. Resampling,
compressed files, full 32-bit integer PCM, other valid precisions, RF64 and
multichannel files remain outside this native path.

Tests use independent extensible fixtures covering sign/extrema, wider
containers, mono/stereo, alternate integer scaling, float headroom/signed zero,
capacity limits, truncation and malformed extensions. Desktop bridge tests cover
dispatch, independent calibration caches and unsupported-header fallback. The
packaged WebView2 self-test compares supported imports sample-for-sample against
the actual browser decoder with output muted. These new packaged-app checks
have not yet run: the rebuild is blocked by unavailable cloud-backed files;
see `VERIFICATION.md` for the current result.

On Windows, if a synced build directory reports a temporary archive as "in use",
keep build products outside the synced folder. The desktop packager honors the
standard Cargo override and still places the application in `dist/`:

```powershell
$env:CARGO_TARGET_DIR = Join-Path $env:TEMP 'Aion-Looper-build'
.\scripts\cargo.ps1 test --offline --workspace
.\scripts\build-desktop.ps1
```

This changes the build location for the current shell session. It does not move
source files or change the installed toolchain.
