# Migration status

| Capability | Foundation status | Release acceptance |
| --- | --- | --- |
| Original runtime assets | 16 files preserved with SHA256 | Automated byte identity checks |
| Native build formats | CMake targets/platform guards configured | Compile/load/validate each OS+format |
| Original UI runtime | Read-only static preview; legacy JS blocked | Full view adapter and original interaction tests |
| IPC/APVTS | `hello`, validated `masterGain` RPC, state XML | Session/generation tests, gestures, automation/state stress |
| Audio owner | Muted stereo passthrough, 20 ms master ramp, finite protection | Hardware/DAW audio, 64/128 stress and allocation instrumentation |
| Host transport | BPM/PPQ/play observation | Meter/cycle/seek and sample-accurate scheduling/sync |
| Loop recording/editing/multiply/undo | Not ported | Reference PCM/state/UI fixtures |
| Drone voices/sampler/tracker/FX | Not ported | Algorithm/timing/CPU and saved-state fixtures |
| MIDI learn/FCB1010/Oxygen/HID | Designed, not implemented | Real hardware, reconnect/panic and learn matrix |
| MIDI clock I/O | Designed, not implemented | PLL jitter/timeout/start/stop/SPP/host-loop tests |
| `.alp` / native projects | Not ported | Existing project fixtures, no missing features/data |
| Windows ASIO | SDK option configured, not hardware-tested | TOPPING/other driver duplex at 64/128 |
| macOS/iOS/AU/AUv3 | Conditional targets/presets only | Xcode, devices, permissions, host lifecycle and signing |
| Commercial distribution | Not ready | Licensing, signing, installers, validation and complete parity |

This phase makes no production-readiness or completed feature-parity claim.
