//! Bounded offline sample-rate conversion. Never use on an audio callback.
use crate::Error;
use looper_core::Frame;
use std::f64::consts::PI;

const PHASES: usize = 1024;
const MAX_SOURCE_FRAMES: usize = 32_000_000;

/// Duration rounded to the nearest destination frame; nonempty input stays nonempty.
pub fn output_frames(length: usize, source_rate: u32, target_rate: u32) -> Result<usize, Error> {
    if !(8000..=192000).contains(&source_rate) || !(8000..=192000).contains(&target_rate) {
        return Err(Error::InvalidRate);
    }
    if length == 0 || length > MAX_SOURCE_FRAMES {
        return Err(Error::InvalidSize);
    }
    let frames = ((length as u128 * u128::from(target_rate) + u128::from(source_rate / 2))
        / u128::from(source_rate))
    .max(1);
    usize::try_from(frames).map_err(|_| Error::InvalidSize)
}

/// Blackman-windowed sinc, 32 lobes, with a low-pass cutoff at 94% of the
/// lower Nyquist frequency. Interpolated phase kernels avoid per-sample trig.
/// Edge samples extend the source endpoints; no loop wrap or extra fade.
/// Same-rate conversion returns the original allocation and exact sample bits.
pub fn convert(
    audio: Vec<Frame>,
    source_rate: u32,
    target_rate: u32,
    max_frames: usize,
) -> Result<Vec<Frame>, Error> {
    let length = output_frames(audio.len(), source_rate, target_rate)?;
    if length > max_frames {
        return Err(Error::InvalidSize);
    }
    if audio.iter().flatten().any(|v| !v.is_finite()) {
        return Err(Error::NonfiniteSample);
    }
    if source_rate == target_rate {
        return Ok(audio);
    }
    let cutoff = 0.94 * (f64::from(target_rate) / f64::from(source_rate)).min(1.0);
    let radius = (32.0 / cutoff).ceil() as isize;
    let taps = (2 * radius + 1) as usize;
    let mut kernels = Vec::new();
    kernels
        .try_reserve_exact((PHASES + 1) * taps)
        .map_err(|_| Error::InvalidSize)?;
    for phase in 0..=PHASES {
        let fraction = phase as f64 / PHASES as f64;
        let start = kernels.len();
        let mut sum = 0.0;
        for tap in -radius..=radius {
            let distance = tap as f64 - fraction;
            let x = distance / radius as f64;
            let weight = if x.abs() >= 1.0 {
                0.0
            } else {
                let sinc = if distance.abs() < 1e-12 {
                    cutoff
                } else {
                    (PI * cutoff * distance).sin() / (PI * distance)
                };
                sinc * (0.42 + 0.5 * (PI * x).cos() + 0.08 * (2.0 * PI * x).cos())
            };
            kernels.push(weight);
            sum += weight;
        }
        for weight in &mut kernels[start..] {
            *weight /= sum;
        }
    }
    let mut output = Vec::new();
    output
        .try_reserve_exact(length)
        .map_err(|_| Error::InvalidSize)?;
    for frame in 0..length {
        let position = frame as u128 * u128::from(source_rate);
        let center = (position / u128::from(target_rate)) as isize;
        let phase =
            (position % u128::from(target_rate)) as f64 * PHASES as f64 / f64::from(target_rate);
        let index = phase.floor() as usize;
        let fraction = phase - index as f64;
        let a = &kernels[index * taps..(index + 1) * taps];
        let b = &kernels[(index + 1) * taps..(index + 2) * taps];
        let mut sample = [0.0_f64; 2];
        for (tap, (a, b)) in a.iter().zip(b).enumerate() {
            let weight = a + (b - a) * fraction;
            let source =
                audio[(center + tap as isize - radius).clamp(0, audio.len() as isize - 1) as usize];
            for channel in 0..2 {
                sample[channel] += f64::from(source[channel]) * weight;
            }
        }
        output.push(sample.map(|s| s.clamp(-f64::from(f32::MAX), f64::from(f32::MAX)) as f32));
    }
    Ok(output)
}
