use looper_audio::{capture::InputReader, device::Options, input};
use looper_core::Frame;
use looper_files::Format;
use std::fs::OpenOptions;
use std::io::Write;
use std::path::PathBuf;
use std::time::{Duration, Instant};

struct Recording {
    path: PathBuf,
    device: Options,
    rate: u32,
    seconds: f64,
    format: Format,
}
fn parse(args: &[String]) -> Result<Recording, String> {
    let path = args.get(1).ok_or("Usage: rust-looper record OUTPUT.wav [--seconds N] [--rate HZ] [--host HOST] [--device NAME_OR_ID] [--buffer FRAMES] [--format float32|pcm24|pcm16]")?;
    let mut recording = Recording {
        path: path.into(),
        device: Options::default(),
        rate: 48000,
        seconds: 5.0,
        format: Format::Float32,
    };
    let mut options = args[2..].iter();
    while let Some(key) = options.next() {
        let value = options
            .next()
            .ok_or_else(|| format!("Missing value for {key}."))?;
        match key.as_str() {
            "--host" => recording.device.host = Some(value.clone()),
            "--device" => recording.device.device = Some(value.clone()),
            "--seconds" => {
                recording.seconds = value.parse().map_err(|_| "Invalid capture duration.")?
            }
            "--rate" => recording.rate = value.parse().map_err(|_| "Invalid sample rate.")?,
            "--buffer" => {
                recording.device.buffer_frames =
                    Some(value.parse().map_err(|_| "Invalid buffer size.")?)
            }
            "--format" => {
                recording.format = match value.as_str() {
                    "float32" => Format::Float32,
                    "pcm24" => Format::Pcm24,
                    "pcm16" => Format::Pcm16,
                    _ => return Err("Format must be float32, pcm24 or pcm16.".into()),
                }
            }
            _ => return Err(format!("Unknown record option: {key}")),
        }
    }
    if !recording.seconds.is_finite()
        || !(0.0..=120.0).contains(&recording.seconds)
        || recording.seconds == 0.0
    {
        return Err("Capture duration must be greater than zero and at most 120 seconds.".into());
    }
    if !(8000..=192000).contains(&recording.rate) || recording.device.buffer_frames == Some(0) {
        return Err("Sample rate must be 8000..192000 Hz and buffer size must be positive.".into());
    }
    Ok(recording)
}

// Control worker only. Refuse discontinuities rather than silently shortening a take.
fn drain(reader: &mut InputReader, frames: &mut Vec<Frame>, target: usize) -> Result<(), String> {
    let status = reader.status();
    if status.dropped_frames > 0
        || status.nonfinite_samples > 0
        || status.malformed_blocks > 0
        || status.stream_errors > 0
    {
        return Err(format!(
            "Capture interrupted or invalid; no WAV written: {status:?}"
        ));
    }
    while frames.len() < target {
        let Some(frame) = reader.pop() else {
            break;
        };
        if frame.position != frames.len() as u64 {
            return Err("Capture frame discontinuity; no WAV written.".into());
        }
        frames.push(frame.samples);
    }
    Ok(())
}

pub fn run(args: &[String]) -> Result<(), String> {
    let recording = parse(args)?;
    // Reject existing output before opening the input device. Recheck atomically at save.
    if recording
        .path
        .try_exists()
        .map_err(|e| format!("Output path: {e}"))?
    {
        return Err("Recording output already exists; choose a new path.".into());
    }
    let target = (recording.seconds * f64::from(recording.rate)).round() as usize;
    if target == 0 {
        return Err("Capture duration is shorter than one sample.".into());
    }
    let mut frames = Vec::new();
    frames
        .try_reserve_exact(target)
        .map_err(|e| format!("Allocate capture: {e}"))?;
    let mut input = input::open(
        recording.rate,
        &recording.device,
        (recording.rate / 2) as usize,
    )?;
    println!(
        "Input: {} / {}, {} Hz, {} ch, {:?}, requested buffer {:?}",
        input.info.host,
        input.info.device,
        input.info.config.sample_rate,
        input.info.config.channels,
        input.info.sample_format,
        input.info.config.buffer_size
    );
    let channels = input.info.config.channels.min(2);
    input.start()?;
    let deadline = Instant::now() + Duration::from_secs_f64(recording.seconds + 5.0);
    while frames.len() < target {
        drain(&mut input.reader, &mut frames, target)?;
        if frames.len() == target {
            break;
        }
        if Instant::now() >= deadline {
            return Err("Input stream stalled; no WAV written.".into());
        }
        std::thread::sleep(Duration::from_millis(5));
    }
    let reader = input.finish();
    let status = reader.status();
    if status.stream_errors > 0
        || status.dropped_frames > 0
        || status.nonfinite_samples > 0
        || status.malformed_blocks > 0
    {
        return Err(format!(
            "Capture did not finish cleanly; no WAV written: {status:?}"
        ));
    }
    let bytes = looper_files::encode(&frames, recording.rate, channels, recording.format)
        .map_err(|e| format!("Encode recording: {e:?}"))?;
    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&recording.path)
        .map_err(|e| format!("Create recording: {e}"))?;
    file.write_all(&bytes)
        .map_err(|e| format!("Write recording: {e}"))?;
    println!(
        "Recorded {} frames to {}, largest callback {} frames, {} dropped frames.",
        frames.len(),
        recording.path.display(),
        status.max_callback_frames,
        status.dropped_frames
    );
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use looper_audio::capture;
    fn args(values: &[&str]) -> Vec<String> {
        values.iter().map(|value| (*value).into()).collect()
    }
    #[test]
    fn capture_options_reject_invalid_requests_before_opening_a_device() {
        for options in [
            vec!["record"],
            vec!["record", "out.wav", "--seconds", "NaN"],
            vec!["record", "out.wav", "--seconds", "121"],
            vec!["record", "out.wav", "--rate", "0"],
            vec!["record", "out.wav", "--buffer", "0"],
            vec!["record", "out.wav", "--force"],
        ] {
            assert!(parse(&args(&options)).is_err());
        }
        let options = parse(&args(&[
            "record",
            "out.wav",
            "--rate",
            "44100",
            "--seconds",
            "0.25",
            "--format",
            "pcm24",
        ]))
        .unwrap();
        assert_eq!(options.rate, 44100);
        assert_eq!(options.format, Format::Pcm24);
    }
    #[test]
    fn synthetic_capture_round_trips_exact_frames_through_float_wav() {
        let (mut writer, mut reader) = capture::connect(4).unwrap();
        let mut frames = Vec::with_capacity(3);
        writer.capture(&[0.125_f32, -0.25, 0.5, -0.75], 2);
        drain(&mut reader, &mut frames, 3).unwrap();
        writer.capture(&[1.25_f32, -1.5, 2.0, -2.0], 2);
        drain(&mut reader, &mut frames, 3).unwrap();
        let wave = looper_files::decode(
            &looper_files::encode(&frames, 48000, 2, Format::Float32).unwrap(),
        )
        .unwrap();
        assert_eq!(
            wave.frames,
            vec![[0.125, -0.25], [0.5, -0.75], [1.25, -1.5]]
        );
        assert_eq!(reader.pop().unwrap().position, 3);
    }
    #[test]
    fn dropped_or_invalid_input_cannot_be_exported_as_a_clean_recording() {
        for samples in [&[0.1_f32, 0.2, 0.3][..], &[f32::NAN][..]] {
            let (mut writer, mut reader) = capture::connect(2).unwrap();
            writer.capture(samples, 1);
            assert!(drain(&mut reader, &mut Vec::new(), 2).is_err());
        }
    }
}
