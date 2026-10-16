use looper_audio::device::{self, Options};
use looper_audio::realtime::Control;
use looper_core::{Command, Engine, MAX_TRACKS};
use std::time::{Duration, Instant};

pub fn run(args: &[String]) -> Result<(), String> {
    let list = args[0] == "devices";
    let mut index = if list { 1 } else { 2 };
    if !list && args.len() < 2 {
        return Err("Usage: rust-looper play INPUT.wav [--host HOST] [--device NAME_OR_ID] [--buffer FRAMES] [--seconds N] [--gain 0..1]".into());
    }
    let mut options = Options::default();
    let mut seconds = 5.0_f64;
    let mut gain = 0.25_f32;
    while index < args.len() {
        let key = &args[index];
        let value = args
            .get(index + 1)
            .ok_or_else(|| format!("Missing value for {key}."))?;
        match key.as_str() {
            "--host" => options.host = Some(value.clone()),
            "--device" if !list => options.device = Some(value.clone()),
            "--buffer" if !list => {
                let size = value.parse().map_err(|_| "Invalid buffer size.")?;
                if size == 0 {
                    return Err("Buffer size must be positive.".into());
                }
                options.buffer_frames = Some(size);
            }
            "--seconds" if !list => {
                seconds = value.parse().map_err(|_| "Invalid playback duration.")?;
                if !seconds.is_finite() || !(0.0..=3600.0).contains(&seconds) || seconds == 0.0 {
                    return Err(
                        "Playback duration must be greater than zero and at most 3600 seconds."
                            .into(),
                    );
                }
            }
            "--gain" if !list => {
                gain = value.parse().map_err(|_| "Invalid gain.")?;
                if !gain.is_finite() || !(0.0..=1.0).contains(&gain) {
                    return Err("Gain must be between zero and one.".into());
                }
            }
            _ => return Err(format!("Unknown option for {}: {key}", args[0])),
        }
        index += 2;
    }
    if list {
        println!("Available hosts: {}", device::available_hosts().join(", "));
        for info in device::devices(options.host.as_deref())? {
            println!("{} / {}\n  ID: {}", info.host, info.name, info.id);
            for range in info.output_ranges {
                println!(
                    "  OUT: {} ch, {}..{} Hz, {:?}, buffer {:?}",
                    range.channels(),
                    range.min_sample_rate(),
                    range.max_sample_rate(),
                    range.sample_format(),
                    range.buffer_size()
                );
            }
            for range in info.input_ranges {
                println!(
                    "  IN: {} ch, {}..{} Hz, {:?}, buffer {:?}",
                    range.channels(),
                    range.min_sample_rate(),
                    range.max_sample_rate(),
                    range.sample_format(),
                    range.buffer_size()
                );
            }
        }
        return Ok(());
    }
    let bytes = std::fs::read(&args[1]).map_err(|e| format!("Read WAV: {e}"))?;
    let wave = looper_files::decode(&bytes).map_err(|e| format!("Decode WAV: {e:?}"))?;
    let mut capacities = [0; MAX_TRACKS];
    capacities[0] = wave.frames.len();
    let mut engine =
        Engine::with_capacities(capacities).map_err(|e| format!("Allocate take: {e:?}"))?;
    engine
        .load_stopped(0, &wave.frames)
        .map_err(|e| format!("Import take: {e:?}"))?;
    engine
        .command(0, Command::Play)
        .map_err(|e| format!("Prepare loop: {e:?}"))?;
    // Drop decoding storage before starting the callback.
    let rate = wave.sample_rate;
    drop(wave);
    drop(bytes);
    let mut output = device::open_output(engine, rate, &options, gain)?;
    println!(
        "Output: {} / {}, {} Hz, {} ch, {:?}, requested buffer {:?}",
        output.info.host,
        output.info.device,
        output.info.config.sample_rate,
        output.info.config.channels,
        output.info.sample_format,
        output.info.config.buffer_size
    );
    output.start()?;
    let deadline = Instant::now() + Duration::from_secs_f64(seconds);
    while Instant::now() < deadline {
        if output.controller.status().stream_errors > 0 {
            return Err("Native stream reported an error; playback stopped.".into());
        }
        std::thread::sleep(Duration::from_millis(10));
    }
    let ramp_frames = (rate / 200).max(1);
    output
        .controller
        .send(Control::MasterGain {
            gain: 0.0,
            ramp_frames,
        })
        .map_err(|_| "Control queue full while stopping.")?;
    let target = output.controller.status().frames + u64::from(ramp_frames);
    let stop_deadline = Instant::now() + Duration::from_millis(100);
    while output.controller.status().frames < target && Instant::now() < stop_deadline {
        std::thread::sleep(Duration::from_millis(5));
    }
    let status = output.controller.status();
    if status.frames == 0 || status.stream_errors > 0 || status.command_errors > 0 {
        return Err(format!("Playback did not finish cleanly: {status:?}"));
    }
    println!(
        "Playback complete: {} frames, largest callback {} frames, {} stream errors.",
        status.frames, status.max_callback_frames, status.stream_errors
    );
    Ok(())
}
