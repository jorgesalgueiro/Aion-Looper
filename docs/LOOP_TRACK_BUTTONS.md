# Loop-track completion checklist

Owner request: finish all loop-track button functions, then perform an integrated
bug-fixing pass. This checklist describes the native preview, not release acceptance.
The original markup and behavior remain in `.reference/Ascii-Looper/Modules/loopTracks.js`;
`projectManager.js:exportDryWet` defines SAVE. Preserved assets are unchanged.

| Original controls | Native status and evidence | Remaining work |
| --- | --- | --- |
| RECORD / FINISH / PLAY / STOP / STOP NOW / CANCEL; held DEL | Shared GUI/CLI processor; original input/sync and actual 64/128 mouse/keyboard PCM gates retained | Physical device/latency and long-session checks are separate |
| MUTE / UNM, SOLO, volume, pan, feedback | Shared native controls; original mixer comparisons and MIDI volume/pan gates retained | FX-dependent interactions remain pending |
| Start, Q, left/right nudge | Original phase/PCM comparisons; finalization dispatch repair in current checkpoint | Broader overlapping browser transitions remain limited as recorded in REFERENCE_AUDIT |
| Speed, 1/2, 1x | Native rate controls and original source tests; shared finalization dispatch wait | Pre-start source/worker tempo ordering verified; live exponential tempo ramps remain outside that comparison |
| RTRG / STUT | Native shared commands, unchanged-JS source/fragment/progress and real pointer/lifetime gates; RTRG now admits unborn queued and pending-STOP sources with Windows/Linux audio/ownership, direct CLI and 20 Windows UI combination checks | Retain documented transition-envelope and non-Windows interaction limits |
| MULT | Ordinary capture/doubling, queued pre-birth doubling, constructed and active/gated source input capture; stored doubling during scheduled STOP, including constructed queued sources, passes Windows/Linux audio/ownership and Windows pointers | Retain recorder-quantum/worker-latency and platform/interaction limits in MIGRATION_CURRENT; integrated combinations remain pending |
| UNDO / REDO | Local/global history, including constructed queued PLAY with STOP pending; Windows/Linux original-JS WAV/PCM, Engine ownership, production GUI/CLI and actual Windows local/global pointers pass | Rapid finalization FIFO verified for local/global history and MULT; retain lifecycle/concurrency and non-Windows interaction limits in MIGRATION_CURRENT |
| NORM | Native immutable edit, no-history semantics and queued-source gates; scheduled-stop source/deadline gates now pass on Windows/Linux with Windows pointer acceptance | Retain OOM/cancellation, STOP/phase deadlines and current finalization dispatch checks |
| DEL; MERGE source / GO | Native cleanup and immutable worker merge, original confirmation/history/PCM gates | Queued-source clone/mix verified in Windows/Linux audio and Windows pointer tests; queued-destination and scheduled-stop source/destination audio/ownership and Windows pointer checks also pass; other stopping-state controls require their own eligibility audit |
| Waveform click WAV | Native stored-PCM export, filenames, bounded leases and picker lifecycle | OS cancel/overwrite and non-Windows dialogs remain separate checks |
| SAVE (Dry/Wet) | **Unported; disabled** | Native wet rendering, chain state, rate/pan/volume, three-second tail, paired filenames and lifecycle checks; dry-only export is not equivalent |
| FX, FULL PRESET, CHAIN PRESET, manual chain, per-effect controls | **Product integration unported; disabled**, except Reverse. Original Q/eQ isolated DSP/smoothing and curve/bandwidth math now pass Windows/Linux and actual Windows WebAudio sound gates; [EQ_MIGRATION.md](EQ_MIGRATION.md) | Connect shared GUI/CLI/state/presets and owner-approved graphic point/range editing; complete routing/lifecycle/real UI acceptance before enabling Q/eQ, then remaining effects |

Finalization dispatch, rapid queued history/MULT and the integral-sample source
fade repair are verified. Worker/source pre-start tempo ordering is verified.
Queued-source input capture and main FINISH now have shared GUI/CLI, original-JS
PCM/WAV and 64/128 ownership/pointer coverage. Queued-source MERGE now passes
its Windows/Linux audio and Windows interaction gates. Queued destinations and
scheduled-stop source/destination states now pass those gates too. Pending/stopping
RTRG and its MULT/NORM/history/MERGE/STUT/rate/reverse/rapid/finalizing combinations,
and queued PLAY with STOP pending local/global history, now pass the scoped
acceptance in MIGRATION_CURRENT. Next is the original Q/eQ effect, with shared
GUI/CLI/state and sound comparison, before the remaining FX/SAVE/preset engine.
The owner confirmed core
looping/editing first. Each functional
slice must pass its relevant Windows/Linux tests before the next. The final
integrated pass must exercise combinations rather than merely enable controls.
Q/eQ DSP groundwork is in progress; its complete product controls remain disabled.
No dependency change or package is implied by this inventory. Existing sessions,
IDs, expanded capacity and logo are preserved.

Retained input-capture MULT acceptance: a constructed
queued source corresponds to the original JS playing graph before its audible
grid. An available input must start capture immediately and retain that graph's
original volume gate; crossing the grid must not end capture. A second press
appends the conditioned input and creates one local history entry, while fewer
than 128 captured samples preserve PCM/history and resume the same source.
Compare unchanged `startMultiply`/`processMultiplyData`, original source PCM and
post-commit Apply/Undo/Redo. Cover forward/reverse, early/near-grid, captures that
finish before/after the grid, timestamped 64/128 GUI/CLI controls, DEL/panic/prepare,
OOM/pool exhaustion, pending sources and zero callback new/delete. Audio/ownership
comparisons pass on Windows/Linux; original pointer checks pass on Windows.
Linux WebKit interaction remains pending. Pending-stop capture stays guarded.
Preserve the unexplained initial
integrated mouse missing-target observation and its diagnostic reruns; do not
claim it fixed. Queued and scheduled-stop MERGE preview acceptance is recorded
in MIGRATION_CURRENT.md; other stopping-state controls remain separate work.
