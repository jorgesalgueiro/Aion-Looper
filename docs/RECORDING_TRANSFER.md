# Responsive recording finalization

First recordings still finalize through the existing Rust worker, preserving its
DSP and the original UI. This change prevents PCM transfer from monopolizing the
UI thread while live native input is being delivered.

The physical Windows test previously showed a 74 ms input polling gap against a
64 ms transport prefill, followed by an underrun after recording stop. Transfers
larger than 4,096 frames now yield between bounded batches:

- Snapshot submitted PCM before yielding, retaining the original capture and frozen
  mode, tempo, latency and extraction settings.
- Pack 4,096 frames or encode 32,768 bytes per task before returning to the event
  loop. Native input read replies and control events can run between these tasks.
- Decode and validate the entire returned PCM before filling its private output
  buffer, also in bounded tasks. Signed zero and exact float32 bits are retained.
- Keep the generated original fallback and outer take/context/transport ownership
  checks. A malformed response uses the submitted snapshot; a stale result cannot
  restore a cleared or replaced recording.

Small jobs keep their existing path. The native one-million-combined-frame bound,
worker queue limits and 64 ms input prefill are unchanged. Tasks use a shared
MessageChannel to avoid timer throttling in hidden WebKit views; browsers without
that API retain a timer fallback. This is cooperative
transfer on the UI thread, not a second audio thread or a new native duplex engine.
It reduces individual blocking intervals at the cost of longer overall offline
completion. Other offline operations and browser audio scheduling remain work.

The input diagnostics now report maximum read/poll intervals, latest underrun
render time and whether transport prefill has completed. The L/R graphical test
waits for replacement-source prefill instead of comparing a startup-silent take
against an already flowing source; its balance threshold is unchanged.

## Verification

- Three sequential Windows/E2x2 physical capture trials passed with zero new
  reported underruns/overruns across recording and finalization. Maximum polling
  intervals were 42, 40 and 48 ms. Captured audio was discarded; output was muted.
- The packaged Windows graphical suite passed all 130 checks.
- All 81 desktop JavaScript tests passed, including new large-transfer tests that
  verify event-loop progress during both transfer directions, exact submitted PCM,
  signed zero and malformed-response fallback on the private snapshot.
- Linux desktop/CLI/CLAP packaging and the packaged CLAP ABI check passed.
  The WSLg physical retest now completes rather than timing out, but still reports
  one post-recording underrun. This fix is not yet verified as gap-free on WSLg.

Short local trials are not a guarantee of gap-free live performance, long-session
clock stability, audio quality or round-trip latency. ASIO/JACK hardware, direct
Linux audio devices, macOS/iOS execution and full native playback remain pending.

## Windows CI build fix

The first hosted Windows MSVC check failed compiling Rubber Band: Windows
`min`/`max` macros expanded inside `std::min`/`std::max` and produced C2589 errors.
The upstream vendored Visual Studio project already defines `NOMINMAX`; the Rust
MSVC build now does too, without changing the vendored sources or GNU flags.
Platform verification no longer cancels Linux when another platform fails.

The current Linux graphical suite also passed 131 checks. WSLg physical capture
still reports one post-recording underrun; that limitation remains open.

The subsequent acknowledged input transport replaces frontend polling. Short Windows and WSLg physical trials now pass without reported gaps; see INPUT_TESTING.md for bounds, checks and remaining limits.
