# Native MIDI input

The desktop opens MIDI inputs through pinned midir 0.11.1. Windows uses WinMM;
the library also provides ALSA on Linux and CoreMIDI on macOS/iOS. Only the Windows
build has been exercised here. The original interface and App MIDI handlers stay
unchanged: notes, velocity-zero note-off, CC learn, pitch bend, program changes
and transport messages pass through the native adapter to those handlers.

Input callbacks copy short MIDI 1.0 messages into fixed queues, without heap
operations, logging or locks in our callback. Each of up to 16 inputs has 1,024
slots. A dedicated worker drains at most 256 messages per batch, with round-robin
fairness. It checks activity every 2 ms and discovers ports every second. These
intervals are scheduling targets, not measured end-to-end latency guarantees.

Rust pushes batches to the WebView. Only one unacknowledged batch can be queued;
acknowledgements carry document session, input token and batch sequence. An idle
heartbeat keeps the ownership lease alive without a recurring JavaScript timer.
An unresponsive document loses its inputs after 30 seconds. Page exit closes
inputs; late packets from another document or input generation are rejected.

Disconnects, reconnection generations and overflow release tracked held notes
before rebinding. Overflow discards the affected queue and reports dropped
messages rather than replaying a partial backlog. Device-open failures remain
visible as closed inputs and are retried on discovery.

The adapter implements the MIDIAccess subset used by this application, with input
maps, state changes and message events. SysEx and MIDI output are unavailable.
Mapping, synth playback and transport still run in the compatibility UI/audio
engine; native sample-accurate MIDI-to-audio scheduling remains unfinished.

Synthetic input is accepted only in desktop self-test mode. Tests cover callback
allocation, FIFO/overflow, message validation, owner tokens, push backpressure,
replay rejection, held-note cleanup, reconnects and the original UI routing/learn.
Read-only Windows discovery found zero attached MIDI inputs on this machine.
Physical controller timing, driver behavior and audible synth output remain
unverified. No microphone recording is needed for these checks.
