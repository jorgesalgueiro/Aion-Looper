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
| Normalize, overdub/substitute undo/redo | Rust worker; original NORM/UNDO/REDO controls for stopped takes; bounded session history | Existing workflow |
| Global undo, reverse, multiply, merge | Pending in native transport/UI | Existing workflow; several offline operations already use Rust |
| Pan, speed, start offsets, retrigger/stutter, solo | Pending UI/engine integration | Existing workflow |
| BPM/bar sync, quantization, metronome, tracker | Pending native integration | Existing workflow |
| Effects, synths, sampler, full MIDI/gamepad mapping | Pending native integration | Existing workflow |
| Waveforms and detailed per-track meters | Pending metadata/render integration | Existing workflow |
| JACK/CoreAudio native duplex, macOS/iOS applications | Pending | Not established by Windows/WSLg checks |
| CLAP | Separate native prototype, not full app parity | Not the standalone browser UI |

Next functional priorities: existing project interchange, global undo and remaining editing,
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
