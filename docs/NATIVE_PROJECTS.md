# Native projects and WAV export

WAV imports can now be brought into stopped loops; see NATIVE_IMPORT.md.

Use SAVE PROJ in the original Project Menu to save the current native session.
Finish recording, overdubbing or substituting first. Saving is supported while
completed takes play: capture/output callbacks continue, and PCM is read through
a coherent Rust checkpoint. Controls changing during a snapshot may require retry.

Project Name labels the file. Native projects are stored in `AionProjects` beside
the executable. Each save creates a new timestamped `.aion` version. Previous
versions remain intact. Loop SAVE exports that take as a dry stereo float32 WAV
in the same directory, retaining the sample rate and exact float samples.

To reopen:

1. Save the current session before closing audio.
2. Press CLOSE & CLEAR to release its streams and unsaved in-memory takes.
3. Select a saved project in the startup selector and press LOAD PROJECT.
4. Press START AION LOOPER to connect your chosen interface.

All takes reopen stopped and monitoring starts off. Loop audio, names, gain,
feedback, mute, master level and input gain are restored. Playback phase and
monitoring are deliberately not resumed. The ASIO device is selected locally;
a saved project does not force a different audio device.

The full `AionProjects` folder is your data. Keep it when replacing the portable
app and copy it beside a newly extracted executable. Closing does not delete
saved projects or exported WAVs. Unsaved changes still disappear on close.

`.aion` is a versioned native format, not the original `.alp` format. Existing
Ascii-Looper projects still open in compatibility mode. Native `.alp` import,
effects, synths, sync and other pending feature state are not implemented or
claimed to be preserved by this format. See NATIVE_PARITY.md.

The reader validates dimensions and a checksum, rejects truncated/corrupt files,
and bounds total capacity to 32 million stereo frames (256 MB). Disk access and
encoding run on the Rust service worker, never on the device callback. A save
uses a temporary file and commits only after flushing; it never replaces an
older version. Load errors keep an already prepared project intact, and loading
is rejected while another native audio session is open.

Automated persistence/WAV tests save synthetic input only, in a separate
`NativeTestProjects` folder. Physical ASIO tests keep output muted and do not
save microphone recordings.
