//! Native input stream setup and ownership; all device calls stay off the callback.
use crate::capture::{self, InputReader, InputWriter};
use crate::device::{choose_input, host, Options};
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{FromSample, SampleFormat, SizedSample, StreamConfig};
use std::time::Duration;

#[derive(Clone, Debug)]
pub struct InputInfo {
    pub host: String,
    pub device: String,
    pub device_id: String,
    pub config: StreamConfig,
    pub sample_format: SampleFormat,
}
pub struct LiveInput {
    stream: cpal::Stream,
    pub reader: InputReader,
    pub info: InputInfo,
}
impl LiveInput {
    pub fn start(&self) -> Result<(), String> {
        self.stream.play().map_err(|e| format!("Start input: {e}"))
    }
    /// Drop the stream before handing the remaining queued frames to the worker.
    pub fn finish(self) -> InputReader {
        drop(self.stream);
        self.reader
    }
}
fn build<T: SizedSample>(
    device: &cpal::Device,
    config: StreamConfig,
    mut writer: InputWriter,
) -> Result<cpal::Stream, String>
where
    f32: FromSample<T>,
{
    let metrics = writer.metrics();
    device
        .build_input_stream(
            config,
            move |data: &[T], _| writer.capture(data, usize::from(config.channels)),
            move |_| metrics.stream_error(),
            Some(Duration::from_secs(5)),
        )
        .map_err(|e| format!("Build input stream: {e}"))
}

/// Opens paused, uses an exact rate, and rejects unsupported layouts/buffers.
pub fn open(sample_rate: u32, options: &Options, ring_frames: usize) -> Result<LiveInput, String> {
    // Validate storage before accessing any device.
    let (writer, reader) = capture::connect(ring_frames)?;
    let host = host(options.host.as_deref())?;
    let device = if let Some(selection) = &options.device {
        if host.id().name().eq_ignore_ascii_case("ASIO") {
            // ASIO names are registry keys. Stop at the selected driver instead
            // of loading every following driver before creating its stream.
            host.input_devices()
                .map_err(|e| format!("Enumerate ASIO input: {e}"))?
                .find(|device| device.to_string() == *selection
                    || device.id().is_ok_and(|id| id.to_string() == *selection))
                .ok_or("Selected ASIO input driver could not be opened. Check its control panel, connected interface and other applications using it.")?
        } else {
            let matches: Vec<_> = host
                .input_devices()
                .map_err(|e| format!("Enumerate input: {e}"))?
                .filter(|device| {
                    device.to_string() == *selection
                        || device.id().is_ok_and(|id| id.to_string() == *selection)
                })
                .collect();
            if matches.len() != 1 {
                return Err("Input device must match one unique name or device ID.".into());
            }
            matches.into_iter().next().unwrap()
        }
    } else {
        host.default_input_device()
            .ok_or("No default input device.")?
    };
    let ranges = device
        .supported_input_configs()
        .map_err(|e| format!("Read input configurations: {e}"))?;
    let (config, sample_format) = choose_input(ranges, sample_rate, options.buffer_frames)?;
    let info = InputInfo {
        host: host.id().name().to_owned(),
        device: device.to_string(),
        device_id: device
            .id()
            .map_err(|e| format!("Read input device ID: {e}"))?
            .to_string(),
        config,
        sample_format,
    };
    let stream = match sample_format {
        SampleFormat::F32 => build::<f32>(&device, config, writer),
        SampleFormat::F64 => build::<f64>(&device, config, writer),
        SampleFormat::I32 => build::<i32>(&device, config, writer),
        SampleFormat::I24 => build::<cpal::I24>(&device, config, writer),
        SampleFormat::I16 => build::<i16>(&device, config, writer),
        SampleFormat::U16 => build::<u16>(&device, config, writer),
        _ => return Err("Input sample format not implemented.".into()),
    }?;
    Ok(LiveInput {
        stream,
        reader,
        info,
    })
}
