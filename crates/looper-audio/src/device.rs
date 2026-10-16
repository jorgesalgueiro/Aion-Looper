//! CPAL device setup and output stream ownership, away from the callback.
use crate::realtime::{connect, Control, Controller, Renderer};
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{
    BufferSize, FromSample, SampleFormat, SizedSample, StreamConfig, SupportedBufferSize,
    SupportedStreamConfigRange,
};
use looper_core::Engine;
use std::time::Duration;

#[derive(Clone, Debug, Default)]
pub struct Options {
    pub host: Option<String>,
    pub device: Option<String>,
    pub buffer_frames: Option<u32>,
}

#[derive(Clone, Debug)]
pub struct DeviceInfo {
    pub host: String,
    pub name: String,
    pub id: String,
    pub output_ranges: Vec<SupportedStreamConfigRange>,
    pub input_ranges: Vec<SupportedStreamConfigRange>,
}

pub fn host(name: Option<&str>) -> Result<cpal::Host, String> {
    match name {
        None | Some("default") => Ok(cpal::default_host()),
        Some(name) => {
            let id = cpal::available_hosts()
                .into_iter()
                .find(|id| id.name().eq_ignore_ascii_case(name))
                .ok_or_else(|| {
                    format!("Audio host '{name}' is unavailable in this build or on this machine.")
                })?;
            cpal::host_from_id(id).map_err(|e| format!("Open audio host: {e}"))
        }
    }
}

pub fn available_hosts() -> Vec<String> {
    cpal::available_hosts()
        .into_iter()
        .map(|id| id.name().to_owned())
        .collect()
}

/// Registered ASIO names without loading drivers or interrupting active capture.
#[cfg(all(target_os = "windows", feature = "asio"))]
pub fn asio_input_names() -> Vec<String> {
    asio_sys::Asio::new().driver_names()
}

pub fn devices(host_name: Option<&str>) -> Result<Vec<DeviceInfo>, String> {
    let host = host(host_name)?;
    let host_name = host.id().name().to_owned();
    host.devices()
        .map_err(|e| format!("Enumerate devices: {e}"))?
        .map(|device| {
            Ok(DeviceInfo {
                host: host_name.clone(),
                name: device.to_string(),
                id: device
                    .id()
                    .map_err(|e| format!("Read device ID: {e}"))?
                    .to_string(),
                output_ranges: device
                    .supported_output_configs()
                    .map(|ranges| ranges.collect())
                    .unwrap_or_default(),
                input_ranges: device
                    .supported_input_configs()
                    .map(|ranges| ranges.collect())
                    .unwrap_or_default(),
            })
        })
        .collect()
}

/// Choose an exact rate and requested buffer, preferring stereo floating point.
/// Never silently resample or replace a requested fixed buffer with the default.
pub fn choose_output(
    ranges: impl IntoIterator<Item = SupportedStreamConfigRange>,
    sample_rate: u32,
    buffer_frames: Option<u32>,
) -> Result<(StreamConfig, SampleFormat), String> {
    choose_config(ranges, sample_rate, buffer_frames, "output")
}

pub fn choose_input(
    ranges: impl IntoIterator<Item = SupportedStreamConfigRange>,
    sample_rate: u32,
    buffer_frames: Option<u32>,
) -> Result<(StreamConfig, SampleFormat), String> {
    choose_config(ranges, sample_rate, buffer_frames, "input")
}

fn choose_config(
    ranges: impl IntoIterator<Item = SupportedStreamConfigRange>,
    sample_rate: u32,
    buffer_frames: Option<u32>,
    direction: &str,
) -> Result<(StreamConfig, SampleFormat), String> {
    if sample_rate == 0 || buffer_frames == Some(0) {
        return Err("Sample rate and buffer size must be positive.".into());
    }
    ranges.into_iter().filter_map(|range| {
        let channel_rank = match range.channels() { 2 => 0, 1 => 100, channels @ 3..=128 if direction == "input" => 200 + i32::from(channels), _ => return None };
        let format_rank = match range.sample_format() {
            SampleFormat::F32 => 0, SampleFormat::F64 => 1, SampleFormat::I32 => 2,
            SampleFormat::I24 if direction == "input" => 3,
            SampleFormat::I16 => 4, SampleFormat::U16 => 5, _ => return None,
        };
        if let (Some(frames), SupportedBufferSize::Range { min, max }) = (buffer_frames, range.buffer_size()) {
            if frames < *min || frames > *max { return None; }
        }
        let supported = range.try_with_sample_rate(sample_rate)?;
        let format = supported.sample_format();
        let mut config = supported.config();
        config.buffer_size = buffer_frames.map(BufferSize::Fixed).unwrap_or(BufferSize::Default);
        Some((channel_rank + format_rank, config, format))
    }).min_by_key(|(rank, _, _)| *rank).map(|(_, config, format)| (config, format))
        .ok_or_else(|| format!("No supported {direction} configuration for {sample_rate} Hz, buffer {buffer_frames:?}."))
}

#[derive(Clone, Debug)]
pub struct OutputInfo {
    pub host: String,
    pub device: String,
    pub config: StreamConfig,
    pub sample_format: SampleFormat,
}

pub struct LiveOutput {
    // Dropped before the control endpoints; the host stops callbacks first.
    stream: cpal::Stream,
    pub controller: Controller,
    pub info: OutputInfo,
}

impl LiveOutput {
    pub fn start(&self) -> Result<(), String> {
        self.stream.play().map_err(|e| format!("Start output: {e}"))
    }
    pub fn pause(&self) -> Result<(), String> {
        self.stream
            .pause()
            .map_err(|e| format!("Pause output: {e}"))
    }
}

fn build<T: SizedSample + FromSample<f32>>(
    device: &cpal::Device,
    config: StreamConfig,
    mut renderer: Renderer,
    controller: &Controller,
) -> Result<cpal::Stream, String> {
    let metrics = controller.metrics();
    device
        .build_output_stream(
            config,
            move |data: &mut [T], _| renderer.render(&[], data, usize::from(config.channels)),
            move |_| metrics.stream_error(),
            Some(Duration::from_secs(5)),
        )
        .map_err(|e| format!("Build output stream: {e}"))
}

/// Streams are built paused. The caller explicitly starts playback and owns stop.
pub fn open_output(
    engine: Engine,
    sample_rate: u32,
    options: &Options,
    gain: f32,
) -> Result<LiveOutput, String> {
    if !gain.is_finite() || !(0.0..=1.0).contains(&gain) {
        return Err("Master gain must be between zero and one.".into());
    }
    let host = host(options.host.as_deref())?;
    let device = if let Some(selection) = &options.device {
        let matches: Vec<_> = host
            .output_devices()
            .map_err(|e| format!("Enumerate output: {e}"))?
            .filter(|device| {
                device.to_string() == *selection
                    || device.id().is_ok_and(|id| id.to_string() == *selection)
            })
            .collect();
        if matches.len() != 1 {
            return Err("Output device must match one unique name or device ID.".into());
        }
        matches.into_iter().next().unwrap()
    } else {
        host.default_output_device()
            .ok_or("No default output device.")?
    };
    let ranges = device
        .supported_output_configs()
        .map_err(|e| format!("Read output configurations: {e}"))?;
    let (config, sample_format) = choose_output(ranges, sample_rate, options.buffer_frames)?;
    let info = OutputInfo {
        host: host.id().name().to_owned(),
        device: device.to_string(),
        config,
        sample_format,
    };
    let (mut controller, renderer) = connect(engine);
    controller
        .send(Control::MasterGain {
            gain,
            ramp_frames: (sample_rate / 200).max(1),
        })
        .map_err(|_| "Initial control queue full.")?;
    let stream = match sample_format {
        SampleFormat::F32 => build::<f32>(&device, config, renderer, &controller),
        SampleFormat::F64 => build::<f64>(&device, config, renderer, &controller),
        SampleFormat::I32 => build::<i32>(&device, config, renderer, &controller),
        SampleFormat::I16 => build::<i16>(&device, config, renderer, &controller),
        SampleFormat::U16 => build::<u16>(&device, config, renderer, &controller),
        _ => return Err("Output sample format not implemented.".into()),
    }?;
    Ok(LiveOutput {
        stream,
        controller,
        info,
    })
}
