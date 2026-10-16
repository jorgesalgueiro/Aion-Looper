# Input migration checkpoint — 2026-10-16

The current Windows and Linux packages include native input lifecycle fixes.
The original `ui/` source remains unchanged.

- Device enumeration now runs on a separate bounded worker (two queued requests).
  Previously, refreshing the selector blocked input reads long enough to overflow
  the half-second native ring. The Windows physical test reproduced 62,880 dropped
  frames on the E2x2 interface before this fix.
- Native input streams expire after 30 seconds without matching-session reads.
  The owner wakes once per second even when no IPC arrives, releasing abandoned
  devices. Session IDs and JavaScript-safe tokens are bounded.
- The compatibility transport now uses a nominal 64 ms prefill instead of 16 ms.
  A regression simulation reproduces starvation with 32 ms IPC batches at the old
  setting and verifies continuous processing with the new bounded prefill.
  This adds buffering; it is not measured round-trip latency or a replacement for
  native duplex audio. Underruns remain visible and are not hidden or reset.
- Physical test failures now include sample-free device, frame and transport
  diagnostics. The test compares reported gap counters over a 1.5-second recording
  window after prefill; output stays muted and captured PCM is discarded.

Ubuntu has a usable `RDPSource` through WSLg PulseAudio. Install
`libasound2-plugins` and `pulseaudio-utils`, then run:

```sh
cd /mnt/d/Jorge/Documents/Codex/Aion-Looper
PULSE_SERVER=unix:/mnt/wslg/PulseServer PULSE_SOURCE=RDPSource \
ALSA_CONFIG_PATH="$PWD/scripts/alsa-wslg.conf" \
bash scripts/test-desktop-linux.sh --physical-input
```

This config applies to that process only; no system ALSA defaults are changed.
WSLg is a virtualized microphone route, not native Linux hardware validation.
Windows physical test: `scripts/test-desktop.ps1 -PhysicalInput`.
Synthetic routing/recording tests use the same runners without the physical flag.
Reports are `verification/physical-input-standalone.json` and
`verification/linux-physical-input.json`; they retain no captured audio.

Input callback tests cover finite samples, channel conversion, packed int24,
wide interfaces, frame continuity, overflow and allocation-free callbacks.
Lifecycle regressions cover abandoned sources and invalid session/token identities.
Frontend/worklet tests cover stop/replacement, retry, context suspension, channel
routing, jitter, bounded storage and error reporting.

## Results

- Six Rust input lifecycle/adapter checks passed on Windows and Ubuntu.
- All 21 native audio callback/capture/configuration tests passed on Ubuntu,
  including the allocation counter. All 15 frontend/worklet input tests passed.
- Strict input-crate Clippy passed on both platforms.
- Ubuntu/WSLg recorded 65,920 finite frames at 44.1 kHz, with nonzero signal,
  and successful device replacement/release. One run had no reported gaps; the
  final repeat detected one underrun after finalization, so gap-free operation
  is not established.
- Windows/E2x2 records finite nonzero stereo input at 48 kHz and the native stream
  stays live. The device-enumeration overflow is fixed. The strengthened quality
  test detects an underrun after recording stops on both platforms: the last counter snapshot
  before stop is zero and the post-finalization snapshot is one. Those reports
  are periodic, so they do not establish the exact sample at which it occurred.
  The quality test intentionally fails instead of claiming gap-free capture.

Next input work: move remaining finalization/PCM transfer work off the UI thread
and verify continuous monitoring across recording finalization, then extend to
long-running drift/latency tests. A larger compatibility prefill alone does not
solve all scheduling stalls. ASIO/JACK hardware and direct Linux devices
remain unverified. WSLg results cannot substitute for those hardware checks.

The Windows graphical regression also checks browser rejection/fallback for
extensible layouts that its decoder/calibration does not support. Supported
layouts still require native dispatch and sample-exact comparisons remain strict.

Final packaged graphical regressions: 129 checks passed on Windows and 130
on Ubuntu, including synthetic MIC/LINE, monitoring, stereo/L/R recording,
overdub/substitute and device removal. These synthetic passes do not override
the failed physical quality reports described above.

