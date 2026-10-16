//! Native ASIO duplex: PCM and loop transport never visit the UI thread.
use crate::{capture, device};
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{FromSample, Sample, SampleFormat, SizedSample, StreamConfig};
use looper_core::checkpoint::{Checkpoint, Replacement, Take};
use looper_core::{Command, Engine, State, TrackStatus, MAX_TRACKS};
use rtrb::{Consumer, Producer, RingBuffer};
use std::sync::atomic::{AtomicU32, AtomicU64, AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::Duration;

const QUEUE: usize = 256;
#[derive(Clone, Copy, Debug)]
pub enum Control {
    Track { track: usize, command: Command },
    FinishStopped { track: usize },
    Master(f32),
    Monitor(bool),
    InputGain(f32),
}
#[derive(Default)]
struct TrackMetrics {
    state: AtomicU32,
    length: AtomicUsize,
    position: AtomicUsize,
    muted: AtomicU32,
}
#[derive(Default)]
struct Metrics {
    frames: AtomicU64,
    underruns: AtomicU64,
    discontinuities: AtomicU64,
    command_errors: AtomicU64,
    stream_errors: AtomicU64,
    malformed: AtomicU64,
    input_peak: AtomicU32,
    output_peak: AtomicU32,
    callback_frames: AtomicUsize,
    tracks: [TrackMetrics; MAX_TRACKS],
}
#[derive(Debug)]
pub struct Status {
    pub frames: u64,
    pub underruns: u64,
    pub discontinuities: u64,
    pub command_errors: u64,
    pub stream_errors: u64,
    pub malformed: u64,
    pub input_peak: f32,
    pub output_peak: f32,
    pub callback_frames: usize,
    pub capture: capture::Status,
    pub tracks: [TrackStatus; MAX_TRACKS],
}
struct ImportJob {
    prepared: Replacement,
    previous: Option<Arc<Checkpoint>>,
    result: Result<(), looper_core::Error>,
    ticket: Arc<AtomicU32>,
}
pub struct Controller {
    producer: Producer<Control>,
    metrics: Arc<Metrics>,
    capture_metrics: Arc<capture::Metrics>,
    checkpoint: Option<Arc<Checkpoint>>,
    imports: Producer<Box<ImportJob>>,
    retired: Consumer<Box<ImportJob>>,
    import_ticket: Option<Arc<AtomicU32>>,
}
impl Controller {
    pub fn begin_import(
        &mut self,
        track: usize,
        audio: &[looper_core::Frame],
    ) -> Result<(), String> {
        self.begin_replacement(track, audio, false)
    }
    /// Offline edits share the import retirement path, retaining gain/feedback/mute.
    pub fn begin_edit(&mut self, track: usize, audio: &[looper_core::Frame]) -> Result<(), String> {
        self.begin_replacement(track, audio, true)
    }
    fn begin_replacement(
        &mut self,
        track: usize,
        audio: &[looper_core::Frame],
        editing: bool,
    ) -> Result<(), String> {
        let _ = self.finish_import();
        if self.import_ticket.is_some() || self.producer.slots() != QUEUE {
            return Err("Native controls or an import are pending. Retry when they finish.".into());
        }
        let checkpoint = self
            .checkpoint
            .as_ref()
            .ok_or("Edits require checkpoint storage")?;
        let prepared = if editing {
            checkpoint.prepare_edit(track, audio)
        } else {
            checkpoint.prepare_replacement(track, audio)
        }
        .map_err(|e| format!("Prepare take: {e:?}"))?;
        let ticket = Arc::new(AtomicU32::new(0));
        let job = Box::new(ImportJob {
            prepared,
            previous: None,
            result: Err(looper_core::Error::InvalidState),
            ticket: Arc::clone(&ticket),
        });
        self.imports
            .push(job)
            .map_err(|_| "An import is already queued.".to_owned())?;
        self.import_ticket = Some(ticket);
        Ok(())
    }
    /// Reclaims both replaced and rejected PCM on this worker, not the callback.
    pub fn finish_import(&mut self) -> Option<Result<(), String>> {
        let job = self.retired.pop().ok()?;
        self.import_ticket = None;
        if job.result.is_ok() {
            self.checkpoint = Some(job.prepared.checkpoint());
        }
        let result = job
            .result
            .map_err(|e| format!("Import rejected: {e:?}. Stop the target loop first."));
        drop(job);
        Some(result)
    }
    /// A queued import can be cancelled before the callback starts its commit.
    pub fn cancel_import(&self) -> bool {
        self.import_ticket.as_ref().is_some_and(|t| {
            t.compare_exchange(0, 3, Ordering::AcqRel, Ordering::Acquire)
                .is_ok()
        })
    }

    /// Worker-side readiness for operations requiring a stable pre-command take.
    pub fn controls_pending(&self) -> bool {
        self.import_ticket.is_some() || self.producer.slots() != QUEUE
    }

    pub fn snapshot(&self, max_frames: usize) -> Result<Vec<Take>, String> {
        if self.import_ticket.is_some() || self.producer.slots() != QUEUE {
            return Err("Controls are pending. Try saving again.".into());
        }
        self.checkpoint.as_ref().ok_or("Snapshots are unavailable")?
            .snapshot(max_frames, || ()).map(|(takes, ())| takes)
            .map_err(|_| "Finish recording or punch operations before saving; retry if controls are changing.".into())
    }

    pub fn send(&mut self, control: Control) -> Result<(), String> {
        let valid = match control {
            Control::Track { track, command } => {
                track < MAX_TRACKS
                    && match command {
                        Command::Gain(value) => value.is_finite() && (0.0..=4.0).contains(&value),
                        Command::Feedback(value) => {
                            value.is_finite() && (0.0..=1.0).contains(&value)
                        }
                        _ => true,
                    }
            }
            Control::FinishStopped { track } => track < MAX_TRACKS,
            Control::Master(value) | Control::InputGain(value) => {
                value.is_finite() && (0.0..=4.0).contains(&value)
            }
            Control::Monitor(_) => true,
        };
        if !valid {
            return Err("Invalid native audio control.".into());
        }
        self.producer
            .push(control)
            .map_err(|_| "Native audio control queue is full.".into())
    }
    pub fn status(&self) -> Status {
        let m = &self.metrics;
        Status {
            frames: m.frames.load(Ordering::Acquire),
            underruns: m.underruns.load(Ordering::Relaxed),
            discontinuities: m.discontinuities.load(Ordering::Relaxed),
            command_errors: m.command_errors.load(Ordering::Relaxed),
            stream_errors: m.stream_errors.load(Ordering::Relaxed),
            malformed: m.malformed.load(Ordering::Relaxed),
            input_peak: f32::from_bits(m.input_peak.load(Ordering::Relaxed)),
            output_peak: f32::from_bits(m.output_peak.load(Ordering::Relaxed)),
            callback_frames: m.callback_frames.load(Ordering::Relaxed),
            capture: self.capture_metrics.status(),
            tracks: std::array::from_fn(|i| {
                let t = &m.tracks[i];
                TrackStatus {
                    state: match t.state.load(Ordering::Relaxed) {
                        1 => State::Recording,
                        2 => State::Stopped,
                        3 => State::Playing,
                        4 => State::Overdubbing,
                        5 => State::Substituting,
                        _ => State::Empty,
                    },
                    length_frames: t.length.load(Ordering::Relaxed),
                    position_frames: t.position.load(Ordering::Relaxed),
                    muted: t.muted.load(Ordering::Relaxed) != 0,
                }
            }),
        }
    }
}
pub struct Renderer {
    engine: Engine,
    reader: capture::InputReader,
    consumer: Consumer<Control>,
    metrics: Arc<Metrics>,
    master: f32,
    master_target: f32,
    monitor: f32,
    monitor_target: f32,
    input_gain: f32,
    input_target: f32,
    next_input: u64,
    input_started: bool,
    checkpoint: Option<Arc<Checkpoint>>,
    imports: Consumer<Box<ImportJob>>,
    retired: Producer<Box<ImportJob>>,
}
/// Allocate all loop storage and queues before either device stream starts.
pub fn connect(
    engine: Engine,
    reader: capture::InputReader,
    capture_metrics: Arc<capture::Metrics>,
    gain: f32,
) -> Result<(Controller, Renderer), String> {
    connect_checkpoint(engine, reader, capture_metrics, gain, None)
}
pub fn connect_checkpoint(
    engine: Engine,
    reader: capture::InputReader,
    capture_metrics: Arc<capture::Metrics>,
    gain: f32,
    checkpoint: Option<Arc<Checkpoint>>,
) -> Result<(Controller, Renderer), String> {
    if !gain.is_finite() || !(0.0..=4.0).contains(&gain) {
        return Err("Invalid master gain.".into());
    }
    let (producer, consumer) = RingBuffer::new(QUEUE);
    let (import_tx, import_rx) = RingBuffer::new(1);
    let (retire_tx, retire_rx) = RingBuffer::new(1);
    let metrics = Arc::new(Metrics::default());
    Ok((
        Controller {
            producer,
            metrics: Arc::clone(&metrics),
            capture_metrics,
            checkpoint: checkpoint.clone(),
            imports: import_tx,
            retired: retire_rx,
            import_ticket: None,
        },
        Renderer {
            engine,
            reader,
            consumer,
            metrics,
            master: gain,
            master_target: gain,
            monitor: 0.0,
            monitor_target: 0.0,
            input_gain: 1.0,
            input_target: 1.0,
            next_input: 0,
            input_started: false,
            checkpoint,
            imports: import_rx,
            retired: retire_tx,
        },
    ))
}
impl Renderer {
    pub fn render<T: Sample + FromSample<f32>>(&mut self, output: &mut [T], channels: usize) {
        if !(1..=2).contains(&channels) || !output.len().is_multiple_of(channels) {
            output.fill(T::EQUILIBRIUM);
            self.metrics.malformed.fetch_add(1, Ordering::Relaxed);
            return;
        }
        // A prepared take commits at a callback boundary. Neither PCM copying
        // nor destruction happens here; the worker reclaims the boxed job.
        if self.retired.slots() > 0 {
            if let Ok(mut job) = self.imports.pop() {
                if job
                    .ticket
                    .compare_exchange(0, 1, Ordering::AcqRel, Ordering::Acquire)
                    .is_ok()
                {
                    job.result = job.prepared.apply(&mut self.engine);
                    if job.result.is_ok() {
                        job.previous = self.checkpoint.replace(job.prepared.checkpoint());
                        self.checkpoint.as_ref().unwrap().publish(&self.engine);
                    }
                }
                // There is one producer and we checked the slot before popping.
                // Keep the job queued for worker-side destruction.
                let pushed = self.retired.push(job);
                debug_assert!(pushed.is_ok());
            }
        }
        // Playback changes phase only, which is deliberately not persisted. Keep
        // the checkpoint epoch stable during read-only playback so long snapshots
        // can finish without stopping the device or copying PCM in the callback.
        let checkpoint = self.checkpoint.clone();
        let changing = !self.consumer.is_empty()
            || (0..MAX_TRACKS).any(|i| {
                matches!(
                    self.engine.status(i).unwrap().state,
                    State::Recording | State::Overdubbing | State::Substituting
                )
            });
        let mut transaction = checkpoint.as_ref().filter(|_| changing).map(|c| c.begin());
        for _ in 0..64 {
            let Ok(control) = self.consumer.pop() else {
                break;
            };
            if transaction.is_none() {
                transaction = checkpoint.as_ref().map(|c| c.begin());
            }
            let result = match control {
                Control::Track { track, command } => self.engine.command(track, command),
                Control::FinishStopped { track } => self
                    .engine
                    .command(track, Command::Finish)
                    .and_then(|()| self.engine.command(track, Command::Stop)),
                Control::Master(gain) => {
                    self.master_target = gain;
                    Ok(())
                }
                Control::Monitor(enabled) => {
                    self.monitor_target = if enabled { 1.0 } else { 0.0 };
                    Ok(())
                }
                Control::InputGain(gain) => {
                    self.input_target = gain;
                    Ok(())
                }
            };
            if result.is_err() {
                self.metrics.command_errors.fetch_add(1, Ordering::Relaxed);
            }
        }
        let mut input_peak = 0.0_f32;
        let mut output_peak = 0.0_f32;
        for frame in output.chunks_exact_mut(channels) {
            let input = if let Some(captured) = self.reader.pop() {
                if captured.position != self.next_input {
                    self.metrics.discontinuities.fetch_add(1, Ordering::Relaxed);
                }
                self.next_input = captured.position + 1;
                self.input_started = true;
                captured.samples
            } else {
                if self.input_started {
                    self.metrics.underruns.fetch_add(1, Ordering::Relaxed);
                }
                [0.0; 2]
            };
            self.master += (self.master_target - self.master) * 0.015625;
            self.monitor += (self.monitor_target - self.monitor) * 0.015625;
            self.input_gain += (self.input_target - self.input_gain) * 0.015625;
            let input = input.map(|sample| {
                (f64::from(sample) * f64::from(self.input_gain))
                    .clamp(-f64::from(f32::MAX), f64::from(f32::MAX)) as f32
            });
            input_peak = input_peak.max(input[0].abs()).max(input[1].abs());
            let playback = self.engine.process_frame(input);
            let mixed = std::array::from_fn::<_, 2, _>(|c| {
                crate::realtime::protect(
                    (f64::from(playback[c]) + f64::from(input[c]) * f64::from(self.monitor))
                        * f64::from(self.master),
                )
            });
            output_peak = output_peak.max(mixed[0].abs()).max(mixed[1].abs());
            if channels == 1 {
                frame[0] = T::from_sample((mixed[0] + mixed[1]) * 0.5);
            } else {
                frame[0] = T::from_sample(mixed[0]);
                frame[1] = T::from_sample(mixed[1]);
            }
        }
        for i in 0..MAX_TRACKS {
            let status = self.engine.status(i).expect("valid track");
            let t = &self.metrics.tracks[i];
            t.state.store(
                match status.state {
                    State::Empty => 0,
                    State::Recording => 1,
                    State::Stopped => 2,
                    State::Playing => 3,
                    State::Overdubbing => 4,
                    State::Substituting => 5,
                },
                Ordering::Relaxed,
            );
            t.length.store(status.length_frames, Ordering::Relaxed);
            t.position.store(status.position_frames, Ordering::Relaxed);
            t.muted.store(u32::from(status.muted), Ordering::Relaxed);
        }
        self.metrics
            .input_peak
            .store(input_peak.to_bits(), Ordering::Relaxed);
        self.metrics
            .output_peak
            .store(output_peak.to_bits(), Ordering::Relaxed);
        self.metrics
            .callback_frames
            .fetch_max(output.len() / channels, Ordering::Relaxed);
        self.metrics
            .frames
            .store(self.engine.clock_frames(), Ordering::Release);
        if transaction.is_some() {
            checkpoint.as_ref().unwrap().publish(&self.engine);
        }
        drop(transaction);
    }
}
#[derive(Debug)]
pub struct Info {
    pub host: String,
    pub device: String,
    pub sample_rate: u32,
    pub input_channels: u16,
    pub output_channels: u16,
}
pub struct Live {
    // Stop output before releasing its input producer and device driver.
    output: cpal::Stream,
    input: cpal::Stream,
    pub controller: Controller,
    pub info: Info,
}
impl Live {
    pub fn start(&self) -> Result<(), String> {
        self.input
            .play()
            .map_err(|e| format!("Start native input: {e}"))?;
        self.output
            .play()
            .map_err(|e| format!("Start native output: {e}"))
    }
}
fn output<T: SizedSample + FromSample<f32>>(
    device: &cpal::Device,
    config: StreamConfig,
    mut renderer: Renderer,
) -> Result<cpal::Stream, String> {
    let metrics = Arc::clone(&renderer.metrics);
    device
        .build_output_stream(
            config,
            move |data: &mut [T], _| renderer.render(data, usize::from(config.channels)),
            move |_| {
                metrics.stream_errors.fetch_add(1, Ordering::Relaxed);
            },
            Some(Duration::from_secs(5)),
        )
        .map_err(|e| format!("Build native output: {e}"))
}
pub fn open(
    engine: Engine,
    checkpoint: Option<Arc<Checkpoint>>,
    rate: u32,
    options: &device::Options,
    gain: f32,
) -> Result<Live, String> {
    let host = device::host(options.host.as_deref())?;
    if !host.id().name().eq_ignore_ascii_case("ASIO") {
        return Err("Native duplex currently requires an ASIO build and driver. Other native duplex backends are pending.".into());
    }
    let selection = options
        .device
        .as_deref()
        .ok_or("Select an ASIO driver before starting native audio.")?;
    let device=host.input_devices().map_err(|e|format!("Enumerate ASIO input: {e}"))?
        .find(|device|device.to_string()==selection||device.id().is_ok_and(|id|id.to_string()==selection))
        .ok_or("Selected ASIO driver could not be opened. Close other audio applications and check the interface connection.")?;
    let (input_config, input_format) = device::choose_input(
        device
            .supported_input_configs()
            .map_err(|e| format!("ASIO input configs: {e}"))?,
        rate,
        options.buffer_frames,
    )?;
    let (output_config, output_format) = device::choose_output(
        device
            .supported_output_configs()
            .map_err(|e| format!("ASIO output configs: {e}"))?,
        rate,
        options.buffer_frames,
    )?;
    let (writer, reader) = capture::connect((rate / 2) as usize)?;
    let capture_metrics = writer.metrics();
    let (controller, renderer) =
        connect_checkpoint(engine, reader, capture_metrics, gain, checkpoint)?;
    let input = match input_format {
        SampleFormat::F32 => crate::input::build::<f32>(&device, input_config, writer),
        SampleFormat::F64 => crate::input::build::<f64>(&device, input_config, writer),
        SampleFormat::I32 => crate::input::build::<i32>(&device, input_config, writer),
        SampleFormat::I24 => crate::input::build::<cpal::I24>(&device, input_config, writer),
        SampleFormat::I16 => crate::input::build::<i16>(&device, input_config, writer),
        SampleFormat::U16 => crate::input::build::<u16>(&device, input_config, writer),
        _ => return Err("ASIO input format is unsupported.".into()),
    }?;
    let output = match output_format {
        SampleFormat::F32 => output::<f32>(&device, output_config, renderer),
        SampleFormat::F64 => output::<f64>(&device, output_config, renderer),
        SampleFormat::I32 => output::<i32>(&device, output_config, renderer),
        SampleFormat::I16 => output::<i16>(&device, output_config, renderer),
        SampleFormat::U16 => output::<u16>(&device, output_config, renderer),
        _ => return Err("ASIO output format is unsupported.".into()),
    }?;
    Ok(Live {
        output,
        input,
        controller,
        info: Info {
            host: host.id().name().into(),
            device: device.to_string(),
            sample_rate: rate,
            input_channels: input_config.channels,
            output_channels: output_config.channels,
        },
    })
}
