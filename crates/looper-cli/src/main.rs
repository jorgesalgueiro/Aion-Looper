//! Initial worker-side native tools; no device or webview is opened.
#![forbid(unsafe_code)]

use looper_core::buffer;
use looper_files::{decode, encode, Format};
use std::fs::OpenOptions;
use std::io::Write;
use std::path::PathBuf;
mod live;
mod record;

const HELP: &str = "Rust-Looper native sample tools
Usage: rust-looper INPUT.wav OUTPUT.wav [options]
       rust-looper devices [--host HOST]
       rust-looper record OUTPUT.wav [--seconds N] [--rate HZ] [--host HOST]
                         [--device NAME_OR_ID] [--buffer FRAMES] [--format FORMAT]
       rust-looper play INPUT.wav [--host HOST] [--device NAME_OR_ID]
                       [--buffer FRAMES] [--seconds N] [--gain 0..1]

  --normalize          Normalize the shared stereo peak
  --reverse            Reverse the take, keeping channels together
  --repeat N           Repeat the take N times (instant multiply)
  --latency-ms N       Apply input/output latency compensation
  --seam-ms N          Apply the original loop seam crossfade once
  --format FORMAT      pcm16, pcm24 or float32 (default: input format)
  --force              Allow overwriting an existing output file
  --help               Show this help

Operations run in this order: latency, seam, normalize, reverse, repeat.
PCM exports clip to full scale; float32 retains headroom. No resampling.
Playback defaults to five seconds at 25% master gain. Device rates must match
the WAV; no automatic resampling. Recording defaults to five seconds at 48 kHz
in float32 WAV, up to 120 seconds; existing files are never overwritten.
Native duplex capture/playback and the UI device bridge are pending.";

struct Options {
    input: PathBuf,
    output: PathBuf,
    normalize: bool,
    reverse: bool,
    repeat: usize,
    latency_ms: Option<f64>,
    seam_ms: Option<f64>,
    format: Option<Format>,
    force: bool,
}

fn parse(args: Vec<String>) -> Result<Option<Options>, String> {
    if args.is_empty() || args == ["--help"] || args == ["-h"] {
        println!("{HELP}");
        return Ok(None);
    }
    if args.len() < 2 {
        return Err("Provide input and output WAV paths; see --help.".into());
    }
    let mut options = Options {
        input: PathBuf::from(&args[0]),
        output: PathBuf::from(&args[1]),
        normalize: false,
        reverse: false,
        repeat: 1,
        latency_ms: None,
        seam_ms: None,
        format: None,
        force: false,
    };
    let mut args = args.into_iter().skip(2);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--normalize" => options.normalize = true,
            "--reverse" => options.reverse = true,
            "--force" => options.force = true,
            "--repeat" => {
                options.repeat = args
                    .next()
                    .ok_or("Missing repeat count.")?
                    .parse()
                    .map_err(|_| "Invalid repeat count.")?;
                if options.repeat == 0 {
                    return Err("Repeat count must be positive.".into());
                }
            }
            "--latency-ms" | "--seam-ms" => {
                let value: f64 = args
                    .next()
                    .ok_or("Missing time in milliseconds.")?
                    .parse()
                    .map_err(|_| "Invalid time in milliseconds.")?;
                if !value.is_finite() || value < 0.0 {
                    return Err("Time must be finite and nonnegative.".into());
                }
                if arg == "--latency-ms" {
                    options.latency_ms = Some(value);
                } else {
                    options.seam_ms = Some(value);
                }
            }
            "--format" => {
                options.format = Some(
                    match args.next().ok_or("Missing export format.")?.as_str() {
                        "pcm16" => Format::Pcm16,
                        "pcm24" => Format::Pcm24,
                        "float32" => Format::Float32,
                        _ => return Err("Format must be pcm16, pcm24 or float32.".into()),
                    },
                );
            }
            _ => return Err(format!("Unknown option: {arg}")),
        }
    }
    Ok(Some(options))
}

fn run(options: Options) -> Result<(), String> {
    let input_path = options
        .input
        .canonicalize()
        .map_err(|e| format!("Input path: {e}"))?;
    if options
        .output
        .canonicalize()
        .is_ok_and(|path| path == input_path)
    {
        return Err("Choose a different output path; the source file is preserved.".into());
    }
    let bytes = std::fs::read(&options.input).map_err(|e| format!("Read input: {e}"))?;
    let mut wave = decode(&bytes).map_err(|e| format!("Decode WAV: {e:?}"))?;
    if let Some(ms) = options.latency_ms {
        buffer::compensate_latency(&mut wave.frames, wave.sample_rate, ms / 1000.0)
            .map_err(|e| format!("Latency compensation: {e:?}"))?;
    }
    // Explicit zero means no seam processing, rather than the source's default fade.
    if let Some(ms) = options.seam_ms.filter(|&value| value > 0.0) {
        buffer::seam_fade(&mut wave.frames, wave.sample_rate, ms / 1000.0)
            .map_err(|e| format!("Seam processing: {e:?}"))?;
    }
    if options.normalize {
        buffer::normalize(&mut wave.frames);
    }
    if options.reverse {
        buffer::reverse(&mut wave.frames);
    }
    if options.repeat != 1 {
        wave.frames =
            buffer::repeat(&wave.frames, options.repeat).map_err(|e| format!("Multiply: {e:?}"))?;
    }
    let format = options.format.unwrap_or(wave.format);
    let bytes = encode(&wave.frames, wave.sample_rate, wave.channels, format)
        .map_err(|e| format!("Encode WAV: {e:?}"))?;
    let mut file = OpenOptions::new()
        .write(true)
        .create(options.force)
        .truncate(options.force)
        .create_new(!options.force)
        .open(&options.output)
        .map_err(|e| format!("Open output: {e}; use --force to overwrite an existing output."))?;
    file.write_all(&bytes)
        .map_err(|e| format!("Write output: {e}"))?;
    println!(
        "Wrote {} frames, {} Hz, {} channel(s), {:?} to {}",
        wave.frames.len(),
        wave.sample_rate,
        wave.channels,
        format,
        options.output.display()
    );
    Ok(())
}

fn main() {
    let args: Vec<_> = std::env::args().skip(1).collect();
    let result = if args.first().is_some_and(|arg| arg == "record") {
        record::run(&args)
    } else if args
        .first()
        .is_some_and(|arg| arg == "devices" || arg == "play")
    {
        live::run(&args)
    } else {
        parse(args).and_then(|options| match options {
            Some(options) => run(options),
            None => Ok(()),
        })
    };
    if let Err(error) = result {
        eprintln!("Rust-Looper: {error}");
        std::process::exit(1);
    }
}
