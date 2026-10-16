# Native Sample Lab stretching

The default supported Sample Lab PROCESS path uses Rubber Band Library 4.0.0,
R3/Finer, replacing SoundTouch JS processing on the UI thread. This change was
requested as a quality improvement; its waveform need not match the old engine.
The existing ratio, pitch, selection, preview and save controls remain unchanged.

Rubber Band is vendored under `crates/looper-stretch/vendor/`. The official tag,
archive hash and license provenance are in `crates/looper-stretch/UPSTREAM.md`.
Upstream documentation:
https://breakfastquay.com/rubberband/integration.html

The Rust adapter prepares bounded interleaved float32 PCM and calls a small C++
shim. The shim owns its engine/planar vectors, catches all C++ exceptions and
retains no borrowed pointers. It uses two-pass offline study/process, R3,
ChannelsTogether and ThreadingNever. Offline mode handles initial delay; final
input is explicitly flushed and only the requested rounded output length is
returned. A shorter native output is an error, never a silently padded tail.
All native processing is on the desktop control worker, outside audio callbacks.

Input and output each support at most 1,000,000 frames; mono/stereo, 8–192 kHz,
ratio > 0.01 and pitch within ±24 semitones. The existing pitch slider remains
±12 semitones. Wider or larger jobs and native failures retain the bundled
SoundTouch JS compatibility implementation. Neutral settings retain the original
slice-only shortcut. This is an offline editor integration, not a live low-latency
stretcher in the playback callback. The native worker timeout is 60 seconds.

PCM and parameters are captured before awaiting the worker. Results require the
same raw source, audio context and edit revision; in-place normalization during
processing also cancels an obsolete result. Context replacement cannot trigger
a fallback using the new context. Failed supported jobs use their saved snapshot
for legacy fallback. The processing flag is released after success/failure.

Validation includes synthetic tone duration/frequency, stereo RMS relationship,
mono identity, tail flushing, silence, short captures and metadata/capacity rejection.
The normal Windows executable exercises the original PROCESS button and confirms
R3 output and independent octave transposition at 1.5x duration. Subjective
vocal/drum/mix listening comparisons, long jobs and non-Windows execution remain
to be assessed. No claim of callback latency follows from these offline tests.
