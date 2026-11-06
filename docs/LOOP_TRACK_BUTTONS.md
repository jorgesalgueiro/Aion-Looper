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
| RTRG / STUT | Native shared commands, unchanged-JS source/fragment/progress and real pointer/lifetime gates | Retain documented transition-envelope limits |
| MULT | Ordinary capture/doubling, queued pre-birth doubling, constructed and active/gated source input capture verified in the native preview | Retain recorder-quantum/worker-latency and platform/interaction limits in MIGRATION_CURRENT |
| UNDO / REDO | Local/global worker history, queued sources and exact stereo gates | Rapid finalization FIFO verified for local/global history and MULT; retain lifecycle/concurrency limits in MIGRATION_CURRENT |
| NORM | Native immutable edit, no-history semantics and queued-source gates | Retain OOM/cancellation and current finalization dispatch checks |
| DEL; MERGE source / GO | Native cleanup and immutable worker merge, original confirmation/history/PCM gates | Queued destination/source merge states remain guarded |
| Waveform click WAV | Native stored-PCM export, filenames, bounded leases and picker lifecycle | OS cancel/overwrite and non-Windows dialogs remain separate checks |
| SAVE (Dry/Wet) | **Unported; disabled** | Native wet rendering, chain state, rate/pan/volume, three-second tail, paired filenames and lifecycle checks; dry-only export is not equivalent |
| FX, FULL PRESET, CHAIN PRESET, manual chain, per-effect controls | **Unported; disabled**, except the independently ported Reverse control | Native effects/state/presets plus original GUI and shared CLI, deterministic sound and interaction acceptance |

Finalization dispatch, rapid queued history/MULT and the integral-sample source
fade repair are verified. Worker/source pre-start tempo ordering is verified.
Queued-source input capture and main FINISH now have shared GUI/CLI, original-JS
PCM/WAV and 64/128 ownership/pointer coverage. Next address remaining merge
eligibility before the effects/SAVE/preset engine. The owner confirmed core
looping/editing first. Each functional
slice must pass its relevant Windows/Linux tests before the next. The final
integrated pass must exercise combinations rather than merely enable controls.
No new effects implementation, dependency, package or product redesign is implied
by this inventory. Existing sessions, IDs, expanded capacity and logo are preserved.

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
claim it fixed. Queued MERGE eligibility is next.
