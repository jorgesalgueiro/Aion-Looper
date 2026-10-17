# Ascii-Looper parity in native mode

The original program remains the behavioral reference. Reusing its presentation
is not full feature parity. The compatibility launcher retains the legacy
workflows while individual native workflows are completed and verified.

| Workflow | Native Rust mode | Compatibility mode |
| --- | --- | --- |
| Original HTML/CSS, ASCII logos, loop markup | Reused, with supported native bindings | Original full UI |
| ASIO capture/output, recording/playback, overdub/substitute | Native; physical TOPPING testing | Native input feeding legacy browser graph |
| Mute, gain, feedback, monitoring, basic number keys | Connected to Rust | Existing workflow |
| Project save/reopen | Versioned `.aion`, current native takes/settings | Original `.alp` workflow |
| Individual WAV export | Dry stereo float32, Rust worker | Existing dry/wet exports |
| Existing `.alp` import and project interchange | Pending | Existing workflow |
| WAV/audio import | 8–192 kHz mono/stereo WAV converted to the session rate in Rust; stopped target; other tracks continue | Existing workflow, with Rust WAV decoding where supported |
| Normalize, overdub/substitute and global undo/redo | Rust worker; original loop/master buttons and Ctrl/Cmd+Z; stopped targets; bounded session history | Existing workflow |
| Instant multiply | Rust worker; original MULT doubles a stopped take with local/global undo/redo | Existing stopped-loop fallback |
| Reverse, live multiply capture, merge | Pending in native transport/UI | Existing workflow; several offline operations already use Rust |
| Retrigger and hold-to-stutter | Rust callback; original RTRG/STUT buttons; reference 120 BPM fragment | Existing workflow, including musical sync |
| Pan, speed, start offsets, solo | Pending UI/engine integration | Existing workflow |
| BPM/bar sync, quantization, metronome, tracker | Pending native integration | Existing workflow |
| Effects, synths, sampler, full MIDI/gamepad mapping | Pending native integration | Existing workflow |
| Waveforms and detailed per-track meters | Pending metadata/render integration | Existing workflow |
| JACK/CoreAudio native duplex, macOS/iOS applications | Pending | Not established by Windows/WSLg checks |
| CLAP | Separate native prototype, not full app parity | Not the standalone browser UI |

Next functional priorities: existing project interchange and remaining editing,
transport timing and sync, then effects/synths and their complete saved state.
Finish and verify each workflow against the reference before enabling its UI.
The compatibility interface must remain available until those workflows reach
native parity. Public release packages may lag these local ASIO previews.

Native edit/punch history stores at most five entries per loop per stack and 8 million stereo frames
(64 MB) across all loops and undo/redo stacks. Other loops may keep playing; finish
all captures/punches first. Mix settings and mute are preserved. History is session-only;
recording, successful import and clear invalidate the target history. A punch adds
a pre-pass snapshot when all captures are finished and the take fits the history
budget. Concurrent recording/punching remains available without undo; the UI reports
this and clears stale target history. Saves
keep the resulting PCM, not the undo stacks. Silent/already-normalized takes are no-ops.

Global undo/redo uses the same bounded PCM history as local editing. The original
master buttons show the next affected loop (UNDO L1 / REDO L1). Stop the target
and finish all captures before applying history; other loops may keep playing.
Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z operate on the global timeline outside text fields.
A new normalize/punch operation clears global redo, preserving other loops' local
redo buffers as in the reference. Clear/import/record and eviction remove stale
targets; closing audio resets history. Live playback undo remains pending.

RTRG restarts a stopped or playing take at frame zero. STUT holds a fragment from
the current callback position; release resumes the full loop at that phase. Stop,
clear, punch and retrigger cancel the fragment. Pointer release/cancel, lost capture,
window blur and page hiding release held controls. Fragment length follows the
reference at 120 BPM (125 ms, limited to half the take, minimum 10 ms, clipped at
the take end). Native BPM sync, quantized retrigger and rate changes remain pending.
The hold is transient: it changes no PCM, history or saved project state.

MULT on a stopped take doubles its PCM exactly, without rate/channel changes or
an added fade. Local/global undo and redo restore the original/doubled length.
Other loops can keep playing; finish all captures first. Both the destination
capacity and the 8 million frame editing budget are checked before allocating
the repeated PCM. Failure keeps the existing take and history. Live multiply
capture remains pending, and MULT stays disabled on playing/capturing targets.
