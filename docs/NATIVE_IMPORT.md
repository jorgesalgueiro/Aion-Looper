# Native WAV import

Start native audio, then drop one WAV file onto an empty or stopped loop track.
Other loops can keep playing. Alternatively, paste the full WAV path in the
Project Menu, choose the target loop and press IMPORT WAV. The imported loop
stays stopped; press its original PLAY button or number key to hear it.

The file is read and decoded in Rust. Supported inputs are mono/stereo PCM16,
PCM24 and IEEE float32 WAV (including supported extensible headers). Mono is
duplicated into stereo, matching the native engine's frame layout. No decoded
PCM or file contents are transported through JavaScript.

The WAV sample rate must match native audio. Rate mismatches, compressed formats,
nonfinite samples, empty takes and files exceeding the track's configured
capacity are rejected. Normal sessions reserve 30 seconds per track. Native
resampling, compressed audio decoding and growing a running track are pending;
there is no silent truncation or playback-rate substitution.

As in the original Ascii-Looper import, Rust applies the 15 ms seam fade once.
The target's level and feedback reset to 1 and 80%, and its session name clears.
Its mute state remains as before. Project save/reopen retains this processed take
without fading it again. SAVE PROJ before replacing a take you want to keep.

Decoding and allocating the replacement buffer happen on the Rust service
worker. At a callback boundary, the audio owner rechecks the target state and
swaps the prepared buffer. The old PCM and checkpoint are returned to the
worker for destruction. Other tracks retain their audio, playback position and
sample clock. A playing/recording/punching target is rejected unchanged. Decode,
rate, capacity and cancelled-queue failures also preserve the existing take.
Only one prepared import can be pending. Import may temporarily reserve another
buffer as large as the target's capacity.

The native OS file-drop adapter is enabled only in native mode, preserving the
compatibility interface's HTML file-drop behavior. Windows drop positions are
converted from physical pixels to logical coordinates before DOM hit testing.
The graphical tests exercise the DOM routing and the Rust import path; an actual
Explorer/Finder drag gesture has not been automated. The full-path controls offer
an alternative. iOS file-picker integration remains pending.

Verification: 38 native GUI checks on Windows and Ubuntu; 131 Windows compatibility
GUI checks; 38 desktop unit tests, 27 audio tests and 31 core tests. Callback heap
accounting covers successful, rejected and cancelled imports. A 30-second TOPPING
ASIO test at 48 kHz / 128 frames imported a five-second synthetic WAV while another
loop played, with zero reported underruns or dropped frames. Output stayed muted;
no microphone recordings were saved.
