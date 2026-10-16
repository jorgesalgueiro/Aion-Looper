# CLAP prototype persistence

The CLAP state extension now embeds the twenty stereo takes, per-track gain,
feedback, mute and play/stop flags, sample rate and all 22 prototype parameter
values. Restored playing loops start at frame zero. Capture position, schedules,
the transient gain ramp and original synth/FX/arrangement data are not persisted.
This is a versioned plugin stream, separate from standalone `.alp` sessions.

PCM uses exact little-endian float32 bits, including finite headroom and signed
zero. The stream carries `RLCLAP01`, version 1, bounded counts/lengths, metadata
and an FNV-1a corruption checksum. Unknown versions, invalid/nonfinite values,
oversized takes, truncation, checksum mismatch and trailing bytes are rejected.
The bounds are the existing eight seconds per track, 8–192 kHz and twenty tracks
(30,720,000 stereo frames maximum). State callbacks handle short reads/writes,
EOF and negative/oversized callback results. Loading commits only after complete
validation and allocation; failures preserve the current instance.

Only the plugin uses optional atomic PCM storage. Local standalone buffers remain
ordinary float32. Each stereo frame still occupies eight bytes, with no second
permanent PCM mirror. The audio owner brackets take changes with a generation
transaction and publishes metadata. The main thread reads atomic samples and
validates the generation before serializing. Continuous playback does not change
the stored take, so saving can proceed while the audio callback keeps running.
Master/monitor automation does not invalidate unchanged PCM; scalar parameter
values are read atomically after copying the take. Coherent PCM does not imply a
single sample-exact timestamp for all independent scalar control values.

Saves during recording, overdub or substitute fail explicitly, as do saves racing
a take edit. Finish the capture/punch before saving. A save failure never reports
a partial recording as a complete snapshot. An unwound write transaction poisons
its checkpoint, preventing later saves of uncertain PCM.

Inactive restore replaces state on the main thread. Active restore prepares a
replacement on that thread and requests the host's restart cycle. Old audio runs
until deactivation, where old storage is reclaimed and the replacement is
installed; reactivation restores the gains. Pending state is immediately visible
to state saving and parameter reads. Only one pending restore is retained. A host
must honor `request_restart`; no PCM is allocated or freed by the audio callback.
Sample-rate conversion is still pending: saved takes require the saved sample
rate at activation. Save/load uses additional temporary memory on the main thread
and is not a streaming-to-disk recorder or a measured low-memory export path.

Verification includes concurrent playback/saving, coherent reader/writer races,
capture refusal, corrupt-state rollback, partial streams, headroom/signed-zero
round trips and active-load restart. The packaged library is loaded through its
actual C ABI and restores deterministic recorded PCM. REAPER 7.43 passed 33 checks,
including a project save/reopen with an exact matching state chunk; that host
project contained no takes. Report: `verification/reaper-clap.json`, including the
tested binary hash. That passing host report is for the build before the final
master/monitor automation snapshot refinement; the final bundle passes its
updated C-ABI restore test. A subsequent hidden repeat stalled before writing
progress; the passing host report is retained separately. Earlier save/reopen harness attempts
also timed out; its project reopen now uses the documented prompt-free option
only for this disposable test project. Physical capture, REAPER PCM rendering,
long sessions and the original embedded plugin editor remain unverified.

Run `scripts/test-reaper-clap.ps1 -StateRoundTrip` for the isolated host test.
References: [CLAP state API](https://github.com/free-audio/clap/blob/main/include/clap/ext/state.h),
[REAPER project API](https://www.reaper.fm/sdk/reascript/reascripthelp.html#Main_openProject).
