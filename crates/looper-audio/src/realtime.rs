//! Bounded SPSC control bridge and device-independent callback renderer.
use cpal::{FromSample, Sample};
use looper_core::schedule::ScheduledCommand;
use looper_core::{Command, Engine, Frame};
use rtrb::{Consumer, Producer, RingBuffer};
use std::sync::atomic::{AtomicU32, AtomicU64, AtomicUsize, Ordering};
use std::sync::Arc;

pub const CONTROL_CAPACITY: usize = 256;
pub const COMMANDS_PER_CALLBACK: usize = 64;

#[derive(Clone, Copy, Debug)]
pub enum Control {
    Now { track: usize, command: Command },
    At(ScheduledCommand),
    Cancel { track: usize },
    MasterGain { gain: f32, ramp_frames: u32 },
}

#[derive(Default)]
pub struct Metrics {
    frames: AtomicU64,
    command_errors: AtomicU64,
    stream_errors: AtomicU64,
    malformed_blocks: AtomicU64,
    max_callback_frames: AtomicUsize,
    peak: AtomicU32,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Status {
    pub frames: u64,
    pub command_errors: u64,
    pub stream_errors: u64,
    pub malformed_blocks: u64,
    pub max_callback_frames: usize,
    pub peak: f32,
}

impl Metrics {
    /// Values are individually atomic, not a transactional engine snapshot.
    pub fn status(&self) -> Status {
        Status {
            frames: self.frames.load(Ordering::Relaxed),
            command_errors: self.command_errors.load(Ordering::Relaxed),
            stream_errors: self.stream_errors.load(Ordering::Relaxed),
            malformed_blocks: self.malformed_blocks.load(Ordering::Relaxed),
            max_callback_frames: self.max_callback_frames.load(Ordering::Relaxed),
            peak: f32::from_bits(self.peak.load(Ordering::Relaxed)),
        }
    }

    /// The backend error callback records a counter without formatting or logging.
    pub fn stream_error(&self) {
        self.stream_errors.fetch_add(1, Ordering::Relaxed);
    }
}

pub struct Controller {
    producer: Producer<Control>,
    metrics: Arc<Metrics>,
}

impl Controller {
    /// Full queues return the unchanged message for control-thread retry/reporting.
    pub fn send(&mut self, control: Control) -> Result<(), Control> {
        self.producer
            .push(control)
            .map_err(|rtrb::PushError::Full(control)| control)
    }
    pub fn status(&self) -> Status {
        self.metrics.status()
    }
    pub fn metrics(&self) -> Arc<Metrics> {
        Arc::clone(&self.metrics)
    }
}

pub struct Renderer {
    engine: Engine,
    consumer: Consumer<Control>,
    metrics: Arc<Metrics>,
    gain: f32,
    target_gain: f32,
    ramp_remaining: u32,
}

/// Allocate on the control thread before starting the device.
pub fn connect(engine: Engine) -> (Controller, Renderer) {
    let (producer, consumer) = RingBuffer::new(CONTROL_CAPACITY);
    let metrics = Arc::new(Metrics::default());
    let controller = Controller {
        producer,
        metrics: Arc::clone(&metrics),
    };
    let renderer = Renderer {
        engine,
        consumer,
        metrics,
        gain: 0.0,
        target_gain: 0.0,
        ramp_remaining: 0,
    };
    (controller, renderer)
}

impl Renderer {
    fn controls(&mut self) {
        // A producer cannot make callback work unbounded by continually refilling.
        for _ in 0..COMMANDS_PER_CALLBACK {
            let Ok(control) = self.consumer.pop() else {
                break;
            };
            let accepted = match control {
                Control::Now { track, command } => self.engine.command(track, command).is_ok(),
                Control::At(event) => self.engine.schedule(event).is_ok(),
                Control::Cancel { track } => self.engine.cancel_scheduled(track).is_ok(),
                Control::MasterGain { gain, ramp_frames } => {
                    if gain.is_finite() && (0.0..=1.0).contains(&gain) {
                        self.target_gain = gain;
                        self.ramp_remaining = ramp_frames;
                        if ramp_frames == 0 {
                            self.gain = gain;
                        }
                        true
                    } else {
                        false
                    }
                }
            };
            if !accepted {
                self.metrics.command_errors.fetch_add(1, Ordering::Relaxed);
            }
        }
    }

    /// Empty input means silence (output-only mode). Nonempty input must contain
    /// one stereo frame per output device frame. Mono output averages L and R.
    /// All allocations, device calls and parameter formatting stay outside here.
    pub fn render<T: Sample + FromSample<f32>>(
        &mut self,
        input: &[Frame],
        output: &mut [T],
        channels: usize,
    ) {
        if !(1..=2).contains(&channels)
            || !output.len().is_multiple_of(channels)
            || (!input.is_empty() && input.len() != output.len() / channels)
        {
            output.fill(T::EQUILIBRIUM);
            self.metrics
                .malformed_blocks
                .fetch_add(1, Ordering::Relaxed);
            return;
        }
        self.controls();
        let rejected_before = self.engine.schedule_status().rejected;
        let mut peak = 0.0_f32;
        let frame_count = output.len() / channels;
        for (index, device_frame) in output.chunks_exact_mut(channels).enumerate() {
            if self.ramp_remaining > 0 {
                self.gain += (self.target_gain - self.gain) / self.ramp_remaining as f32;
                self.ramp_remaining -= 1;
            }
            let frame = self
                .engine
                .process_frame(input.get(index).copied().unwrap_or([0.0; 2]));
            let protected = frame.map(|sample| protect(f64::from(sample) * f64::from(self.gain)));
            if channels == 1 {
                let mono = (protected[0] + protected[1]) * 0.5;
                device_frame[0] = T::from_sample(mono);
                peak = peak.max(mono.abs());
            } else {
                device_frame[0] = T::from_sample(protected[0]);
                device_frame[1] = T::from_sample(protected[1]);
                peak = peak.max(protected[0].abs()).max(protected[1].abs());
            }
        }
        self.metrics
            .frames
            .store(self.engine.clock_frames(), Ordering::Relaxed);
        self.metrics.command_errors.fetch_add(
            self.engine.schedule_status().rejected - rejected_before,
            Ordering::Relaxed,
        );
        self.metrics
            .max_callback_frames
            .fetch_max(frame_count, Ordering::Relaxed);
        self.metrics.peak.store(peak.to_bits(), Ordering::Relaxed);
    }
}

/// Zero-lookahead safety curve from the original master waveshaper. Oversampling
/// and the complete master processing chain have not been ported yet.
pub(crate) fn protect(sample: f64) -> f32 {
    if !sample.is_finite() {
        return 0.0;
    }
    let magnitude = sample.abs().min(1.0);
    let shaped = if magnitude <= 0.875 {
        magnitude
    } else {
        magnitude - (magnitude - 0.875).powi(2) / 0.25
    };
    (sample.signum() * shaped) as f32
}
