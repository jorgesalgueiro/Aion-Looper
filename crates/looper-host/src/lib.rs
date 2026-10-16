//! Shared processing boundary for future VST3, CLAP and LV2 wrappers.
//! This crate is not a loadable plugin. It opens no audio/MIDI devices or windows.
#![forbid(unsafe_code)]

use looper_core::{Command, Engine, Frame, MAX_TRACKS};

pub const MAX_EVENTS: usize = 1024;
pub const MAX_BLOCK_FRAMES: usize = 65_536;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    Configuration,
    Active,
    Inactive,
    BufferLayout,
    Events,
    Transport,
    Core(looper_core::Error),
}

/// Normalize all musical positions/tempo to quarter notes in format wrappers.
/// Values describe the start of this block, not the engine's monotonic clock.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Transport {
    pub sample_position: Option<i64>,
    pub quarter_position: Option<f64>,
    pub tempo: Option<f64>,
    pub signature: Option<(u16, u16)>,
    pub playing: bool,
}

impl Transport {
    fn valid(self) -> bool {
        self.quarter_position.is_none_or(f64::is_finite)
            && self.tempo.is_none_or(|t| t.is_finite() && t > 0.0)
            && self
                .signature
                .is_none_or(|(n, d)| n > 0 && d.is_power_of_two())
    }
}

#[derive(Clone, Copy, Debug)]
pub enum Input<'a> {
    Silent,
    Mono(&'a [f32]),
    Stereo { left: &'a [f32], right: &'a [f32] },
}

impl Input<'_> {
    fn valid(self, frames: usize) -> bool {
        match self {
            Self::Silent => true,
            Self::Mono(data) => data.len() == frames,
            Self::Stereo { left, right } => left.len() == frames && right.len() == frames,
        }
    }
    fn frame(self, index: usize) -> Frame {
        let frame = match self {
            Self::Silent => [0.0; 2],
            Self::Mono(data) => [data[index]; 2],
            Self::Stereo { left, right } => [left[index], right[index]],
        };
        frame.map(|v| if v.is_finite() { v } else { 0.0 })
    }
}

#[derive(Clone, Copy, Debug)]
pub enum Action {
    Track {
        track: usize,
        command: Command,
    },
    /// Host output preserves floating-point headroom; no device limiter is added.
    MasterGain {
        gain: f32,
        ramp_frames: u32,
    },
    MonitorGain {
        gain: f32,
        ramp_frames: u32,
    },
}

impl Action {
    fn valid(self) -> bool {
        match self {
            Self::Track { track, command } => {
                track < MAX_TRACKS
                    && match command {
                        Command::Gain(g) => g.is_finite() && (0.0..=4.0).contains(&g),
                        Command::Feedback(g) => g.is_finite() && (0.0..=1.0).contains(&g),
                        _ => true,
                    }
            }
            Self::MasterGain { gain, .. } => gain.is_finite() && (0.0..=4.0).contains(&gain),
            Self::MonitorGain { gain, .. } => gain.is_finite() && (0.0..=1.0).contains(&gain),
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct Event {
    pub offset: usize,
    pub action: Action,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Report {
    pub frames: usize,
    pub rejected_commands: usize,
    pub last_error: Option<looper_core::Error>,
    /// A host seek/loop jump; free-running loop playback is not reset by it yet.
    pub discontinuity: bool,
}

#[derive(Clone, Copy)]
struct Ramp {
    value: f32,
    target: f32,
    remaining: u32,
}
impl Ramp {
    fn new(value: f32) -> Self {
        Self {
            value,
            target: value,
            remaining: 0,
        }
    }
    fn set(&mut self, target: f32, frames: u32) {
        self.target = target;
        self.remaining = frames;
        if frames == 0 {
            self.value = target;
        }
    }
    fn next(&mut self) -> f32 {
        if self.remaining > 0 {
            self.value += (self.target - self.value) / self.remaining as f32;
            self.remaining -= 1;
        }
        self.value
    }
}

pub struct Processor {
    engine: Engine,
    sample_rate: f64,
    max_block: usize,
    active: bool,
    master: Ramp,
    monitor: Ramp,
    transport: Option<Transport>,
    expected_position: Option<i64>,
}

impl Processor {
    /// Plugin variant: atomic PCM permits control-thread snapshots without a
    /// second audio mirror. The audio owner publishes checkpoint transactions.
    pub fn prepare_shared(
        sample_rate: f64,
        max_block: usize,
        capacities: [usize; MAX_TRACKS],
    ) -> Result<(Self, std::sync::Arc<looper_core::checkpoint::Checkpoint>), Error> {
        if !sample_rate.is_finite()
            || !(8_000.0..=192_000.0).contains(&sample_rate)
            || !(1..=MAX_BLOCK_FRAMES).contains(&max_block)
        {
            return Err(Error::Configuration);
        }
        let (engine, checkpoint) =
            looper_core::checkpoint::Checkpoint::prepare(capacities).map_err(Error::Core)?;
        Ok((
            Self {
                engine,
                sample_rate,
                max_block,
                active: false,
                master: Ramp::new(1.0),
                monitor: Ramp::new(0.0),
                transport: None,
                expected_position: None,
            },
            checkpoint,
        ))
    }
    pub fn is_capturing(&self) -> bool {
        (0..MAX_TRACKS).any(|track| {
            self.engine.status(track).is_ok_and(|s| {
                matches!(
                    s.state,
                    looper_core::State::Recording
                        | looper_core::State::Overdubbing
                        | looper_core::State::Substituting
                )
            })
        })
    }
    /// Allocate loop storage on the control thread before host activation.
    pub fn prepare(
        sample_rate: f64,
        max_block: usize,
        capacities: [usize; MAX_TRACKS],
    ) -> Result<Self, Error> {
        if !sample_rate.is_finite()
            || !(8_000.0..=192_000.0).contains(&sample_rate)
            || !(1..=MAX_BLOCK_FRAMES).contains(&max_block)
        {
            return Err(Error::Configuration);
        }
        Ok(Self {
            engine: Engine::with_capacities(capacities).map_err(Error::Core)?,
            sample_rate,
            max_block,
            active: false,
            master: Ramp::new(1.0),
            monitor: Ramp::new(0.0),
            transport: None,
            expected_position: None,
        })
    }
    pub fn sample_rate(&self) -> f64 {
        self.sample_rate
    }
    pub fn engine(&self) -> &Engine {
        &self.engine
    }
    /// Import/edit/state replacement is only allowed while deactivated.
    pub fn engine_mut(&mut self) -> Result<&mut Engine, Error> {
        if self.active {
            Err(Error::Active)
        } else {
            Ok(&mut self.engine)
        }
    }
    pub fn transport(&self) -> Option<Transport> {
        self.transport
    }
    pub fn activate(&mut self) -> Result<(), Error> {
        if self.active {
            return Err(Error::Active);
        }
        self.active = true;
        self.transport = None;
        self.expected_position = None;
        Ok(())
    }
    pub fn deactivate(&mut self) {
        self.active = false;
    }
    pub fn set_max_block_frames(&mut self, frames: usize) -> Result<(), Error> {
        if self.active {
            return Err(Error::Active);
        }
        if !(1..=MAX_BLOCK_FRAMES).contains(&frames) {
            return Err(Error::Configuration);
        }
        self.max_block = frames;
        Ok(())
    }
    /// Host reset is an audio-thread operation: stop voices/capture, preserve
    /// recorded PCM and parameter values, and cancel queued commands without freeing.
    pub fn reset(&mut self) {
        for track in 0..MAX_TRACKS {
            let _ = self.engine.cancel_scheduled(track);
            if self
                .engine
                .status(track)
                .is_ok_and(|s| s.state != looper_core::State::Empty)
            {
                let _ = self.engine.command(track, Command::Stop);
            }
        }
        self.master.set(self.master.target, 0);
        self.monitor.set(self.monitor.target, 0);
        self.transport = None;
        self.expected_position = None;
    }

    fn validate(
        &self,
        input: Input<'_>,
        left: &[f32],
        right: &[f32],
        events: &[Event],
        transport: Option<Transport>,
    ) -> Result<(), Error> {
        if !self.active {
            return Err(Error::Inactive);
        }
        let frames = left.len();
        if frames > self.max_block || right.len() != frames || !input.valid(frames) {
            return Err(Error::BufferLayout);
        }
        if events.len() > MAX_EVENTS {
            return Err(Error::Events);
        }
        let mut prior = 0;
        for event in events {
            if event.offset < prior
                || (event.offset >= frames && !(frames == 0 && event.offset == 0))
                || !event.action.valid()
            {
                return Err(Error::Events);
            }
            prior = event.offset;
        }
        if transport.is_some_and(|t| !t.valid()) {
            return Err(Error::Transport);
        }
        Ok(())
    }
    fn apply(&mut self, action: Action, report: &mut Report) {
        match action {
            Action::Track { track, command } => {
                if let Err(error) = self.engine.command(track, command) {
                    report.rejected_commands += 1;
                    report.last_error = Some(error);
                }
            }
            Action::MasterGain { gain, ramp_frames } => self.master.set(gain, ramp_frames),
            Action::MonitorGain { gain, ramp_frames } => self.monitor.set(gain, ramp_frames),
        }
    }
    /// Borrow host planar buffers. Structural errors silence all outputs and leave
    /// engine/transport untouched. Equal-offset events preserve host order.
    /// Zero-frame calls flush offset-zero controls without advancing either clock.
    /// No allocation, I/O, device calls or editor access occurs here.
    pub fn process(
        &mut self,
        input: Input<'_>,
        left: &mut [f32],
        right: &mut [f32],
        events: &[Event],
        transport: Option<Transport>,
    ) -> Result<Report, Error> {
        if let Err(error) = self.validate(input, left, right, events, transport) {
            left.fill(0.0);
            right.fill(0.0);
            return Err(error);
        }
        let frames = left.len();
        let mut report = Report {
            frames,
            ..Report::default()
        };
        if frames == 0 {
            for event in events {
                self.apply(event.action, &mut report);
            }
            return Ok(report);
        }
        report.discontinuity = transport
            .and_then(|t| t.sample_position)
            .zip(self.expected_position)
            .is_some_and(|(actual, expected)| actual != expected);
        self.expected_position = transport.and_then(|t| {
            t.sample_position.and_then(|position| {
                position.checked_add(if t.playing { frames as i64 } else { 0 })
            })
        });
        self.transport = transport;
        let mut next = 0;
        for index in 0..frames {
            while next < events.len() && events[next].offset == index {
                self.apply(events[next].action, &mut report);
                next += 1;
            }
            let input = input.frame(index);
            let frame = self.engine.process_frame(input);
            let monitor = f64::from(self.monitor.next());
            let master = f64::from(self.master.next());
            let mixed = |channel| {
                ((f64::from(frame[channel]) + f64::from(input[channel]) * monitor) * master)
                    .clamp(-f64::from(f32::MAX), f64::from(f32::MAX)) as f32
            };
            left[index] = mixed(0);
            right[index] = mixed(1);
        }
        Ok(report)
    }
}
