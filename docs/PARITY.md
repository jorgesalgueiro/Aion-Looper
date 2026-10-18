# Migration status

| Capability | Foundation status | Release acceptance |
| --- | --- | --- |
| Original runtime assets | 16 files preserved with SHA256 | Automated byte identity checks |
| Native build formats | CMake targets/platform guards configured | Compile/load/validate each OS+format |
| Original UI runtime | Original loop cards, module placement, tabs and keyboard gestures; legacy audio JS blocked | Full view adapter and original interaction tests |
| IPC/APVTS | Coherent snapshots, generation/expiry validation, loop commands, master/monitor/auto-play APVTS; parameter XML only | Session/generation tests, gestures, automation/state stress |
| Audio owner | 20-slot native looper, bounded pool, smoothed gain/mute, finite protection | Hardware/DAW audio, 64/128 stress and allocation instrumentation |
| Host transport | BPM/PPQ/play observation | Meter/cycle/seek and sample-accurate scheduling/sync |
| Loop recording/play/stop/overdub/substitute | Single capture owner and worker reference finalization; committed manual PCM comparison passes | Reference behavioral fixtures, hardware and long-session tests |
| Loop pan/varispeed | Native reference pan law, manual 0.25x-2x with phase/rate smoothing; half/reset | Antialiasing, browser audio comparison, sync ratios |
| Loop editing/multiply/undo/export | Not ported | Reference PCM/state/UI fixtures |
| Drone voices/sampler/tracker/FX | Not ported | Algorithm/timing/CPU and saved-state fixtures |
| MIDI learn/FCB1010/Oxygen/HID | Designed, not implemented | Real hardware, reconnect/panic and learn matrix |
| MIDI clock I/O | Designed, not implemented | PLL jitter/timeout/start/stop/SPP/host-loop tests |
| `.alp` / native projects | Not ported | Existing project fixtures, no missing features/data |
| Windows ASIO | Windows binaries compile with bundled SDK; not hardware-tested | TOPPING/other driver duplex at 64/128 |
| macOS/iOS/AU/AUv3 | Conditional targets/presets only | Xcode, devices, permissions, host lifecycle and signing |
| Commercial distribution | Not ready | Licensing, signing, installers, validation and complete parity |

This phase makes no production-readiness or completed feature-parity claim.

See `REFERENCE_AUDIT.md`: the latest worker integration passes the committed-PCM diagnostic; historical direct-write failures are retained for context. These are separate acceptance gates.
