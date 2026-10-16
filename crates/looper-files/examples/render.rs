//! Render a scheduled native loop to a WAV without opening an audio device.
use looper_core::schedule::ScheduledCommand;
use looper_core::{buffer, Command, Engine};
use looper_files::{encode, Format};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "native-loop.wav".to_owned());
    let rate = 48000_usize;
    let mut engine = Engine::new(rate + 1).map_err(|e| format!("Engine: {e:?}"))?;
    let mut take: Vec<_> = (0..rate)
        .map(|i| {
            let t = i as f64 / rate as f64;
            [
                (std::f64::consts::TAU * 220.0 * t).sin() as f32 * 0.15,
                (std::f64::consts::TAU * 330.0 * t).sin() as f32 * 0.15,
            ]
        })
        .collect();
    buffer::seam_fade(&mut take, rate as u32, 0.015).map_err(|e| format!("Seam: {e:?}"))?;
    engine
        .load_stopped(0, &take)
        .map_err(|e| format!("Import: {e:?}"))?;
    for (frame, command) in [
        (0, Command::Play),
        (rate as u64 * 2, Command::Mute(true)),
        (rate as u64 * 3, Command::Mute(false)),
        (rate as u64 * 4, Command::Retrigger(0)),
    ] {
        engine
            .schedule(ScheduledCommand {
                frame,
                track: 0,
                command,
            })
            .map_err(|e| format!("Schedule: {e:?}"))?;
    }
    let input = [[0.0; 2]; 128];
    let mut output = vec![[0.0; 2]; rate * 5];
    for block in output.chunks_mut(input.len()) {
        engine
            .process(&input[..block.len()], block)
            .map_err(|e| format!("Render: {e:?}"))?;
    }
    let bytes =
        encode(&output, rate as u32, 2, Format::Float32).map_err(|e| format!("WAV: {e:?}"))?;
    std::fs::write(&path, bytes)?;
    println!("Rendered 5 seconds of native stereo looping to {path} (float32, 48 kHz).");
    Ok(())
}
