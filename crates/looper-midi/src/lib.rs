//! Native MIDI input, with fixed-size callback messages and worker-side hotplug.
#![forbid(unsafe_code)]
pub mod transport;
use midir::{Ignore, MidiInput, MidiInputConnection};
use serde::Serialize;
use std::time::{Duration, Instant};
use transport::{Reader, Writer};
pub const MAX_PORTS: usize = 16;
pub const MAX_EVENTS: usize = 256;
#[derive(Clone, Debug, Serialize, PartialEq, Eq)]
pub struct Port {
    pub id: String,
    pub name: String,
    pub connection: String,
    pub generation: u64,
}
#[derive(Debug, Serialize)]
pub struct Event {
    pub port: String,
    pub data: Vec<u8>,
    pub age_us: u64,
    pub timestamp_us: u64,
}
#[derive(Debug, Serialize)]
pub struct Snapshot {
    pub ports: Vec<Port>,
    pub events: Vec<Event>,
    pub reset: Vec<String>,
    pub dropped: u64,
}
struct Input {
    info: Port,
    reader: Reader,
    last_dropped: u64,
    _connection: Option<MidiInputConnection<Writer>>,
    synthetic_writer: Option<Writer>,
}
enum Backend {
    Native(MidiInput),
    Synthetic(bool),
}
pub struct Hub {
    backend: Backend,
    inputs: Vec<Input>,
    ports: Vec<Port>,
    epoch: Instant,
    refreshed: Option<Instant>,
    next: usize,
    generation: u64,
    dirty: bool,
}
impl Hub {
    /// Discovery must not consume input before the UI has bound its handlers.
    pub fn initial_snapshot(&self) -> Snapshot {
        Snapshot {
            ports: self.ports.clone(),
            events: Vec::new(),
            reset: Vec::new(),
            dropped: 0,
        }
    }
    pub fn native() -> Result<Self, String> {
        let input =
            MidiInput::new("Rust-Looper discovery").map_err(|e| format!("MIDI discovery: {e}"))?;
        let mut hub = Self::new(Backend::Native(input));
        hub.refresh()?;
        Ok(hub)
    }
    /// Development transport. Desktop IPC gates this behind its self-test mode.
    pub fn synthetic() -> Self {
        let mut hub = Self::new(Backend::Synthetic(true));
        hub.refresh().unwrap();
        hub
    }
    fn new(backend: Backend) -> Self {
        Self {
            backend,
            inputs: Vec::new(),
            ports: Vec::new(),
            epoch: Instant::now(),
            refreshed: None,
            next: 0,
            generation: 0,
            dirty: false,
        }
    }
    pub fn set_connected(&mut self, connected: bool) -> Result<(), String> {
        let Backend::Synthetic(current) = &mut self.backend else {
            return Err("Synthetic MIDI only.".into());
        };
        *current = connected;
        self.refresh()
    }
    pub fn inject(&mut self, messages: &[Vec<u8>]) -> Result<(), String> {
        if !matches!(self.backend, Backend::Synthetic(_)) {
            return Err("Synthetic MIDI only.".into());
        }
        if messages.len() > MAX_EVENTS || messages.iter().any(|m| !transport::valid(m)) {
            return Err("Invalid MIDI test messages.".into());
        }
        let writer = self
            .inputs
            .first_mut()
            .and_then(|p| p.synthetic_writer.as_mut())
            .ok_or("Synthetic port disconnected.")?;
        for message in messages {
            writer.capture(0, message);
        }
        Ok(())
    }
    fn refresh(&mut self) -> Result<(), String> {
        let mut desired = Vec::new();
        match &self.backend {
            Backend::Synthetic(connected) => {
                if *connected {
                    desired.push(Port {
                        id: "self-test-midi".into(),
                        name: "Synthetic native MIDI".into(),
                        connection: "open".into(),
                        generation: 0,
                    });
                }
            }
            Backend::Native(discovery) => {
                for port in discovery.ports().into_iter().take(MAX_PORTS) {
                    let id = port.id();
                    if desired.iter().any(|p: &Port| p.id == id) {
                        continue;
                    }
                    if let Ok(name) = discovery.port_name(&port) {
                        desired.push(Port {
                            id,
                            name,
                            connection: "closed".into(),
                            generation: 0,
                        });
                    }
                }
            }
        }
        self.inputs.retain(|input| {
            desired
                .iter()
                .any(|p| p.id == input.info.id && p.name == input.info.name)
        });
        for info in &mut desired {
            if let Some(input) = self.inputs.iter().find(|p| p.info.id == info.id) {
                info.connection = input.info.connection.clone();
                info.generation = input.info.generation;
                continue;
            }
            let (writer, reader) = transport::channel(self.epoch);
            match &self.backend {
                Backend::Synthetic(_) => {
                    info.connection = "open".into();
                    self.generation = self
                        .generation
                        .checked_add(1)
                        .ok_or("MIDI generation exhausted.")?;
                    info.generation = self.generation;
                    self.inputs.push(Input {
                        info: info.clone(),
                        reader,
                        last_dropped: 0,
                        _connection: None,
                        synthetic_writer: Some(writer),
                    });
                }
                Backend::Native(discovery) => {
                    let Some(port) = discovery.find_port_by_id(&info.id) else {
                        continue;
                    };
                    let Ok(mut source) = MidiInput::new("Rust-Looper input") else {
                        continue;
                    };
                    source.ignore(Ignore::Sysex);
                    if let Ok(connection) = source.connect(
                        &port,
                        "Rust-Looper capture",
                        |timestamp, data, writer| writer.capture(timestamp, data),
                        writer,
                    ) {
                        info.connection = "open".into();
                        self.generation = self
                            .generation
                            .checked_add(1)
                            .ok_or("MIDI generation exhausted.")?;
                        info.generation = self.generation;
                        self.inputs.push(Input {
                            info: info.clone(),
                            reader,
                            last_dropped: 0,
                            _connection: Some(connection),
                            synthetic_writer: None,
                        });
                    }
                }
            }
        }
        self.dirty |= self.ports != desired;
        self.ports = desired;
        self.refreshed = Some(Instant::now());
        Ok(())
    }
    pub fn has_activity(&self) -> bool {
        self.dirty
            || self
                .refreshed
                .is_none_or(|t| t.elapsed() >= Duration::from_secs(1))
            || self
                .inputs
                .iter()
                .any(|p| p.reader.has_messages() || p.reader.dropped() != p.last_dropped)
    }
    pub fn poll(&mut self) -> Result<Snapshot, String> {
        if self
            .refreshed
            .is_none_or(|time| time.elapsed() >= Duration::from_secs(1))
        {
            self.refresh()?;
        }
        let now = self.epoch.elapsed().as_micros().min(u128::from(u64::MAX)) as u64;
        let mut events = Vec::new();
        let mut reset = Vec::new();
        let mut dropped = 0_u64;
        for input in &mut self.inputs {
            let count = input.reader.dropped();
            dropped = dropped.saturating_add(count);
            if count != input.last_dropped {
                input.reader.discard();
                input.last_dropped = count;
                reset.push(input.info.id.clone());
            }
        }
        let length = self.inputs.len();
        // Round-robin prevents one busy clock/controller port starving another.
        for _ in 0..MAX_EVENTS {
            let mut found = false;
            for _ in 0..length {
                let index = self.next % length;
                self.next = (index + 1) % length;
                let input = &mut self.inputs[index];
                if let Some(message) = input.reader.pop() {
                    events.push(Event {
                        port: input.info.id.clone(),
                        data: message.data[..usize::from(message.length)].to_vec(),
                        age_us: now.saturating_sub(message.received_us),
                        timestamp_us: message.timestamp_us,
                    });
                    found = true;
                    break;
                }
            }
            if !found {
                break;
            }
        }
        self.dirty = false;
        Ok(Snapshot {
            ports: self.ports.clone(),
            events,
            reset,
            dropped,
        })
    }
}
