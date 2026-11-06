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
| Speed, 1/2, 1x | Native rate controls and original source tests; shared finalization dispatch wait | Source rate/tempo edits during worker jobs need dedicated audio-order comparison |
| RTRG / STUT | Native shared commands, unchanged-JS source/fragment/progress and real pointer/lifetime gates | Retain documented transition-envelope limits |
| MULT | Ordinary input capture/doubling, queued before source birth, and no-input constructed sources verified | Constructed queued source with input capture is still deliberately guarded |
| UNDO / REDO | Local/global worker history, queued sources and exact stereo gates | Audit accepted rapid history/MULT gestures queued behind finalization |
| NORM | Native immutable edit, no-history semantics and queued-source gates | Retain OOM/cancellation and current finalization dispatch checks |
| DEL; MERGE source / GO | Native cleanup and immutable worker merge, original confirmation/history/PCM gates | Queued destination/source merge states remain guarded |
| Waveform click WAV | Native stored-PCM export, filenames, bounded leases and picker lifecycle | OS cancel/overwrite and non-Windows dialogs remain separate checks |
| SAVE (Dry/Wet) | **Unported; disabled** | Native wet rendering, chain state, rate/pan/volume, three-second tail, paired filenames and lifecycle checks; dry-only export is not equivalent |
| FX, FULL PRESET, CHAIN PRESET, manual chain, per-effect controls | **Unported; disabled**, except the independently ported Reverse control | Native effects/state/presets plus original GUI and shared CLI, deterministic sound and interaction acceptance |

Finish the current finalization dispatch repair first. Then audit rapid queued
history/MULT gestures, implement the guarded input-capture MULT slice, and address
remaining merge eligibility before the effects/SAVE/preset engine. Each functional
slice must pass its relevant Windows/Linux tests before the next. The final
integrated pass must exercise combinations rather than merely enable controls.
No new effects implementation, dependency, package or product redesign is implied
by this inventory. Existing sessions, IDs, expanded capacity and logo are preserved.
