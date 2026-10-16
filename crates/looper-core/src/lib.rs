//! Device-independent looper foundation. Full UI and DSP parity are pending.
//! Buffers are allocated at construction; processing and commands do not allocate.
//! Commands must be delivered by the audio owner at sample boundaries.

#![forbid(unsafe_code)]

pub mod buffer;
pub mod checkpoint;
pub mod schedule;
use schedule::{Schedule, ScheduleStatus, ScheduledCommand};

pub const MAX_TRACKS: usize = 20;
pub type Frame = [f32; 2];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum State {
    Empty,
    Recording,
    Stopped,
    Playing,
    Overdubbing,
    Substituting,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    InvalidCapacity,
    InvalidTrack,
    InvalidState,
    InvalidParameter,
    BlockSizeMismatch,
    ScheduleInPast,
    ScheduleFull,
    ScheduleOrder,
}

#[derive(Clone, Copy, Debug)]
pub enum Command {
    Record,
    Finish,
    Play,
    Stop,
    Clear,
    Overdub,
    Substitute,
    Mute(bool),
    Gain(f32),
    Feedback(f32),
    /// Restart playback at a source-frame offset, wrapping to the take's length.
    Retrigger(usize),
}

struct Track {
    audio: checkpoint::Storage,
    length: usize,
    position: usize,
    state: State,
    muted: bool,
    gain: f32,
    feedback: f32,
}

impl Track {
    fn new(capacity: usize) -> Result<Self, Error> {
        let mut audio = Vec::new();
        audio
            .try_reserve_exact(capacity)
            .map_err(|_| Error::InvalidCapacity)?;
        audio.resize(capacity, [0.0; 2]);
        Ok(Self {
            audio: checkpoint::Storage::Local(audio.into_boxed_slice()),
            length: 0,
            position: 0,
            state: State::Empty,
            muted: false,
            gain: 1.0,
            feedback: 0.80,
        })
    }

    fn command(&mut self, command: Command) -> Result<(), Error> {
        match command {
            Command::Record if self.audio.is_empty() => return Err(Error::InvalidCapacity),
            Command::Record if self.state == State::Empty => {
                self.position = 0;
                self.length = 0;
                self.state = State::Recording;
            }
            Command::Finish if self.state == State::Recording && self.length > 0 => {
                self.position = 0;
                self.state = State::Playing;
            }
            Command::Finish if matches!(self.state, State::Overdubbing | State::Substituting) => {
                self.state = State::Playing;
            }
            Command::Play if self.state == State::Stopped => {
                self.position = 0;
                self.state = State::Playing;
            }
            Command::Retrigger(offset) if matches!(self.state, State::Playing | State::Stopped) => {
                self.position = offset % self.length;
                self.state = State::Playing;
            }
            Command::Stop if self.state != State::Empty => {
                self.state = if self.length == 0 {
                    State::Empty
                } else {
                    State::Stopped
                };
            }
            Command::Clear => {
                // Stale storage is inaccessible until overwritten by a new take.
                self.length = 0;
                self.position = 0;
                self.state = State::Empty;
            }
            Command::Overdub if self.state == State::Playing => self.state = State::Overdubbing,
            Command::Substitute if self.state == State::Playing => self.state = State::Substituting,
            Command::Mute(muted) => self.muted = muted,
            Command::Gain(gain) if gain.is_finite() && (0.0..=4.0).contains(&gain) => {
                self.gain = gain
            }
            Command::Feedback(feedback)
                if feedback.is_finite() && (0.0..=1.0).contains(&feedback) =>
            {
                self.feedback = feedback;
            }
            Command::Gain(_) | Command::Feedback(_) => return Err(Error::InvalidParameter),
            _ => return Err(Error::InvalidState),
        }
        Ok(())
    }

    fn process(&mut self, input: Frame) -> Frame {
        if self.state == State::Recording {
            self.audio.write(self.length, input);
            self.length += 1;
            if self.length == self.audio.len() {
                // Defined capacity policy: finish the take, never grow in the callback.
                self.position = 0;
                self.state = State::Playing;
            }
            return [0.0; 2];
        }
        if !matches!(
            self.state,
            State::Playing | State::Overdubbing | State::Substituting
        ) {
            return [0.0; 2];
        }
        let old = self.audio.read(self.position);
        if self.state == State::Overdubbing {
            self.audio.write(
                self.position,
                [
                    bounded(f64::from(old[0]) * f64::from(self.feedback) + f64::from(input[0])),
                    bounded(f64::from(old[1]) * f64::from(self.feedback) + f64::from(input[1])),
                ],
            );
        } else if self.state == State::Substituting {
            self.audio.write(self.position, input);
        }
        self.position = (self.position + 1) % self.length;
        // Capture monitoring is separate. A punch is heard on the next cycle.
        if self.muted {
            [0.0; 2]
        } else {
            [
                bounded(f64::from(old[0]) * f64::from(self.gain)),
                bounded(f64::from(old[1]) * f64::from(self.gain)),
            ]
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TrackStatus {
    pub state: State,
    pub length_frames: usize,
    pub position_frames: usize,
    pub muted: bool,
}

pub struct Engine {
    tracks: [Track; MAX_TRACKS],
    clock: u64,
    schedule: Schedule,
}

fn bounded(sample: f64) -> f32 {
    sample.clamp(-f64::from(f32::MAX), f64::from(f32::MAX)) as f32
}

impl Engine {
    /// Capacity is stereo frames per track. Memory is capacity * 8 * 20 bytes.
    /// Call on the control thread, before starting the device callback.
    pub fn new(capacity_frames: usize) -> Result<Self, Error> {
        if capacity_frames == 0 {
            return Err(Error::InvalidCapacity);
        }
        Self::with_capacities([capacity_frames; MAX_TRACKS])
    }

    /// Control-thread allocation: assign capacity only to needed tracks. Zero
    /// reserves no PCM for that track and rejects recording until reconfigured.
    pub fn with_capacities(capacities: [usize; MAX_TRACKS]) -> Result<Self, Error> {
        capacities
            .iter()
            .try_fold(0_usize, |sum, &capacity| sum.checked_add(capacity))
            .and_then(|frames| frames.checked_mul(std::mem::size_of::<Frame>()))
            .filter(|&bytes| bytes > 0 && bytes <= isize::MAX as usize)
            .ok_or(Error::InvalidCapacity)?;
        let mut tracks = Vec::with_capacity(MAX_TRACKS);
        for capacity in capacities {
            tracks.push(Track::new(capacity)?);
        }
        Ok(Self {
            tracks: tracks.try_into().map_err(|_| Error::InvalidCapacity)?,
            clock: 0,
            schedule: Schedule::new(),
        })
    }

    pub fn command(&mut self, track: usize, command: Command) -> Result<(), Error> {
        self.tracks
            .get_mut(track)
            .ok_or(Error::InvalidTrack)?
            .command(command)
    }

    pub fn status(&self, track: usize) -> Result<TrackStatus, Error> {
        let track = self.tracks.get(track).ok_or(Error::InvalidTrack)?;
        Ok(TrackStatus {
            state: track.state,
            length_frames: track.length,
            position_frames: track.position,
            muted: track.muted,
        })
    }

    pub fn clock_frames(&self) -> u64 {
        self.clock
    }

    /// Queue in nondecreasing absolute-frame order. Equal-frame commands run in
    /// insertion order, immediately before processing that frame. State checks
    /// happen at execution; rejected events are counted without blocking others.
    pub fn schedule(&mut self, event: ScheduledCommand) -> Result<(), Error> {
        self.schedule.push(event, self.clock)
    }

    pub fn schedule_status(&self) -> ScheduleStatus {
        self.schedule.status
    }

    /// Cancel pending operations for a removed/cleared track without affecting others.
    pub fn cancel_scheduled(&mut self, track: usize) -> Result<(), Error> {
        if track >= MAX_TRACKS {
            return Err(Error::InvalidTrack);
        }
        self.schedule.cancel(track);
        Ok(())
    }

    /// Control-worker operation: device processing must be stopped or ownership
    /// transferred to this worker. Copies PCM, never swaps/free buffers in callback.
    pub fn load_stopped(&mut self, track: usize, audio: &[Frame]) -> Result<(), Error> {
        let track = self.tracks.get_mut(track).ok_or(Error::InvalidTrack)?;
        if !matches!(track.state, State::Empty | State::Stopped) {
            return Err(Error::InvalidState);
        }
        if audio.is_empty() || audio.len() > track.audio.len() {
            return Err(Error::InvalidCapacity);
        }
        if audio.iter().flatten().any(|value| !value.is_finite()) {
            return Err(Error::InvalidParameter);
        }
        for (index, frame) in audio.iter().enumerate() {
            track.audio.write(index, *frame);
        }
        track.length = audio.len();
        track.position = 0;
        track.state = State::Stopped;
        Ok(())
    }

    /// Read local storage for a worker-side edit/export while processing is stopped.
    /// Atomic plugin storage instead uses its Checkpoint reader.
    pub fn audio(&self, track: usize) -> Result<&[Frame], Error> {
        let track = self.tracks.get(track).ok_or(Error::InvalidTrack)?;
        match &track.audio {
            checkpoint::Storage::Local(audio) => Ok(&audio[..track.length]),
            checkpoint::Storage::Shared(_) => Err(Error::InvalidState),
        }
    }

    /// Raw floating-point mix. Output protection and monitoring belong to the host.
    /// Malformed input values are silenced before entering loop storage.
    pub fn process(&mut self, input: &[Frame], output: &mut [Frame]) -> Result<(), Error> {
        if input.len() != output.len() {
            output.fill([0.0; 2]);
            return Err(Error::BlockSizeMismatch);
        }
        for (input, output) in input.iter().zip(output.iter_mut()) {
            *output = self.process_frame(*input);
        }
        Ok(())
    }

    /// One device-independent stereo frame; no allocation or locking.
    pub fn process_frame(&mut self, input: Frame) -> Frame {
        while let Some(event) = self.schedule.pop_due(self.clock) {
            let result = self.command(event.track, event.command);
            self.schedule.result(result);
        }
        let input = input.map(|sample| if sample.is_finite() { sample } else { 0.0 });
        let mut mix = [0.0_f64; 2];
        for track in &mut self.tracks {
            let frame = track.process(input);
            mix[0] += f64::from(frame[0]);
            mix[1] += f64::from(frame[1]);
        }
        self.clock = self.clock.wrapping_add(1);
        mix.map(bounded)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn render(engine: &mut Engine, input: &[Frame]) -> Vec<Frame> {
        let mut output = vec![[0.0; 2]; input.len()];
        engine.process(input, &mut output).unwrap();
        output
    }

    fn take(engine: &mut Engine, track: usize, frames: &[Frame]) {
        engine.command(track, Command::Record).unwrap();
        render(engine, frames);
        if engine.status(track).unwrap().state == State::Recording {
            engine.command(track, Command::Finish).unwrap();
        }
    }

    #[test]
    fn stereo_recording_wraps_without_channel_loss() {
        let mut engine = Engine::new(8).unwrap();
        take(&mut engine, 0, &[[0.25, -0.5], [0.5, -0.25]]);
        assert_eq!(
            render(&mut engine, &[[0.0; 2]; 5]),
            vec![
                [0.25, -0.5],
                [0.5, -0.25],
                [0.25, -0.5],
                [0.5, -0.25],
                [0.25, -0.5]
            ]
        );
        assert_eq!(engine.clock_frames(), 7);
    }

    #[test]
    fn overdub_obeys_feedback_and_phase_across_wrap() {
        let mut engine = Engine::new(8).unwrap();
        take(&mut engine, 0, &[[0.5; 2], [0.25; 2]]);
        render(&mut engine, &[[0.0; 2]]);
        engine.command(0, Command::Feedback(0.5)).unwrap();
        engine.command(0, Command::Overdub).unwrap();
        render(&mut engine, &[[0.125; 2], [0.25; 2]]);
        engine.command(0, Command::Finish).unwrap();
        assert_eq!(
            render(&mut engine, &[[0.0; 2]; 2]),
            vec![[0.25; 2], [0.5; 2]]
        );
    }

    #[test]
    fn substitute_replaces_only_punched_frames() {
        let mut engine = Engine::new(8).unwrap();
        take(&mut engine, 0, &[[0.5; 2], [0.25; 2]]);
        engine.command(0, Command::Substitute).unwrap();
        render(&mut engine, &[[0.125; 2]]);
        engine.command(0, Command::Finish).unwrap();
        assert_eq!(
            render(&mut engine, &[[0.0; 2]; 2]),
            vec![[0.25; 2], [0.125; 2]]
        );
    }

    #[test]
    fn mute_keeps_transport_and_capture_running() {
        let mut engine = Engine::new(8).unwrap();
        take(&mut engine, 0, &[[0.5; 2], [0.25; 2]]);
        engine.command(0, Command::Mute(true)).unwrap();
        assert_eq!(render(&mut engine, &[[0.0; 2]]), vec![[0.0; 2]]);
        engine.command(0, Command::Mute(false)).unwrap();
        assert_eq!(render(&mut engine, &[[0.0; 2]]), vec![[0.25; 2]]);
    }

    #[test]
    fn capacity_finishes_recording_without_growth() {
        let mut engine = Engine::new(2).unwrap();
        take(&mut engine, 0, &[[0.5; 2]; 2]);
        assert_eq!(engine.status(0).unwrap().state, State::Playing);
        assert_eq!(render(&mut engine, &[[0.0; 2]]), vec![[0.5; 2]]);
    }

    #[test]
    fn clear_and_shorter_rerecord_do_not_leak_old_audio() {
        let mut engine = Engine::new(8).unwrap();
        take(&mut engine, 0, &[[0.5; 2]; 4]);
        engine.command(0, Command::Clear).unwrap();
        take(&mut engine, 0, &[[0.125; 2]]);
        assert_eq!(render(&mut engine, &[[0.0; 2]; 4]), vec![[0.125; 2]; 4]);
    }

    #[test]
    fn block_partition_does_not_change_sample_timing() {
        let mut a = Engine::new(8).unwrap();
        let mut b = Engine::new(8).unwrap();
        for engine in [&mut a, &mut b] {
            take(engine, 0, &[[0.5; 2], [0.125; 2], [0.25; 2]]);
        }
        let whole = render(&mut a, &[[0.0; 2]; 17]);
        let split: Vec<_> = [1, 3, 2, 11]
            .into_iter()
            .flat_map(|n| render(&mut b, &vec![[0.0; 2]; n]))
            .collect();
        assert_eq!(whole, split);
    }

    #[test]
    fn invalid_requests_are_rejected_without_advancing_clock() {
        let mut engine = Engine::new(8).unwrap();
        assert_eq!(
            engine.command(20, Command::Record),
            Err(Error::InvalidTrack)
        );
        assert_eq!(engine.command(0, Command::Play), Err(Error::InvalidState));
        assert_eq!(
            engine.command(0, Command::Gain(f32::NAN)),
            Err(Error::InvalidParameter)
        );
        let mut output = [[1.0; 2]; 2];
        assert_eq!(
            engine.process(&[[0.0; 2]], &mut output),
            Err(Error::BlockSizeMismatch)
        );
        assert_eq!(output, [[0.0; 2]; 2]);
        assert_eq!(engine.clock_frames(), 0);
        assert!(matches!(Engine::new(0), Err(Error::InvalidCapacity)));
    }

    #[test]
    fn nonfinite_input_is_not_recorded() {
        let mut engine = Engine::new(8).unwrap();
        take(&mut engine, 0, &[[f32::NAN, f32::INFINITY]]);
        assert_eq!(render(&mut engine, &[[0.0; 2]]), vec![[0.0; 2]]);
    }

    #[test]
    fn extreme_finite_overdubs_and_gains_do_not_poison_the_mix() {
        let mut engine = Engine::new(8).unwrap();
        take(&mut engine, 0, &[[f32::MAX; 2]]);
        engine.command(0, Command::Gain(4.0)).unwrap();
        engine.command(0, Command::Overdub).unwrap();
        assert!(render(&mut engine, &[[f32::MAX; 2]; 8])
            .iter()
            .flatten()
            .all(|value| value.is_finite()));
    }
}
