//! Offline operations ported from Ascii-Looper's AudioEngine.
//! Run on a worker or with the audio device stopped, never in its callback.

use crate::{bounded, Error, Frame};
use std::f64::consts::{FRAC_PI_2, PI};

/// Normalize both channels using a shared peak and the source's silence threshold.
pub fn normalize(audio: &mut [Frame]) -> bool {
    let peak = audio
        .iter()
        .flatten()
        .fold(0.0_f64, |peak, &sample| peak.max(f64::from(sample).abs()));
    if !peak.is_finite() || peak <= 0.0001 || (peak - 1.0).abs() <= 0.0001 {
        return false;
    }
    for frame in audio {
        for sample in frame {
            *sample = bounded(f64::from(*sample) / peak);
        }
    }
    true
}

/// Reverse complete stereo frames, preserving channel identity.
pub fn reverse(audio: &mut [Frame]) {
    audio.reverse();
}

/// Copy a selected frame range without fades, resampling or channel changes.
/// Selection snapping and ratio-to-frame conversion belong to the UI adapter.
pub fn slice(audio: &[Frame], start: usize, length: usize) -> Result<Vec<Frame>, Error> {
    let end = start.checked_add(length).ok_or(Error::InvalidParameter)?;
    if length == 0 || end > audio.len() || audio.iter().flatten().any(|s| !s.is_finite()) {
        return Err(Error::InvalidParameter);
    }
    Ok(audio[start..end].to_vec())
}

/// Linear export fades, including the source's non-overlap rule for short takes.
/// This differs from the loop seam crossfade and runs only on the worker.
pub fn export_fades(audio: &mut [Frame], sample_rate: u32, seconds: f64) -> Result<(), Error> {
    if sample_rate == 0
        || !seconds.is_finite()
        || seconds < 0.0
        || audio.iter().flatten().any(|sample| !sample.is_finite())
    {
        return Err(Error::InvalidParameter);
    }
    let mut fade = (seconds * f64::from(sample_rate)).floor() as usize;
    if fade >= audio.len().div_ceil(2) {
        fade = audio.len() / 3;
    }
    for i in 0..fade {
        let gain = i as f64 / fade as f64;
        for c in 0..2 {
            audio[i][c] = bounded(f64::from(audio[i][c]) * gain);
            let last = audio.len() - 1 - i;
            audio[last][c] = bounded(f64::from(audio[last][c]) * gain);
        }
    }
    Ok(())
}

/// Instant multiply: repeat the take without changing its rate or channels.
pub fn repeat(audio: &[Frame], times: usize) -> Result<Vec<Frame>, Error> {
    if audio.is_empty() || times == 0 {
        return Err(Error::InvalidParameter);
    }
    let length = audio
        .len()
        .checked_mul(times)
        .ok_or(Error::InvalidCapacity)?;
    let mut output = Vec::new();
    output
        .try_reserve_exact(length)
        .map_err(|_| Error::InvalidCapacity)?;
    for _ in 0..times {
        output.extend_from_slice(audio);
    }
    Ok(output)
}

/// Interactive MULT: remove captured DC, fade its splice, then append it.
/// The original take remains bit-identical. Run on the control worker.
pub fn append_capture(
    audio: &[Frame],
    captured: &[Frame],
    sample_rate: u32,
) -> Result<Vec<Frame>, Error> {
    if audio.is_empty()
        || captured.is_empty()
        || sample_rate == 0
        || audio
            .iter()
            .chain(captured)
            .flatten()
            .any(|sample| !sample.is_finite())
    {
        return Err(Error::InvalidParameter);
    }
    let length = audio
        .len()
        .checked_add(captured.len())
        .ok_or(Error::InvalidCapacity)?;
    let mut output = Vec::new();
    output
        .try_reserve_exact(length)
        .map_err(|_| Error::InvalidCapacity)?;
    output.extend_from_slice(audio);
    output.extend_from_slice(captured);
    let take = &mut output[audio.len()..];
    remove_dc(take);
    seam_fade(take, sample_rate, 0.008)?;
    Ok(output)
}

fn remove_dc(take: &mut [Frame]) {
    for channel in 0..2 {
        let mean = take
            .iter()
            .fold(0.0, |sum, frame| sum + f64::from(frame[channel]))
            / take.len() as f64;
        for frame in take.iter_mut() {
            frame[channel] = bounded(f64::from(frame[channel]) - mean);
        }
    }
}

/// Source recording extraction modes. Timing is resolved to frames by the UI
/// at submission; all PCM work happens on the control worker.
#[derive(Clone, Copy, Debug)]
pub enum RecordingMode {
    Free,
    Dynamic {
        target: usize,
    },
    Precision {
        target: usize,
        start: usize,
        destination_offset: usize,
    },
    SyncFallback {
        target: usize,
    },
}

/// Finalize first-recording PCM, preserving source operation order (DC last).
pub fn finalize_recording(
    captured: &[Frame],
    sample_rate: u32,
    latency_seconds: f64,
    mode: RecordingMode,
) -> Result<Vec<Frame>, Error> {
    if captured.is_empty()
        || sample_rate == 0
        || !latency_seconds.is_finite()
        || latency_seconds < 0.0
        || captured.iter().flatten().any(|s| !s.is_finite())
    {
        return Err(Error::InvalidParameter);
    }
    let mut output;
    match mode {
        RecordingMode::Precision {
            target,
            start,
            destination_offset,
        } => {
            if target == 0 {
                return Err(Error::InvalidParameter);
            }
            output = Vec::new();
            output
                .try_reserve_exact(target)
                .map_err(|_| Error::InvalidCapacity)?;
            output.resize(target, [0.0; 2]);
            let copy = target
                .saturating_sub(destination_offset)
                .min(captured.len().saturating_sub(start));
            if copy > 0 {
                output[destination_offset..destination_offset + copy]
                    .copy_from_slice(&captured[start..start + copy]);
            }
            let tail_start = start.saturating_add(copy);
            let tail = (sample_rate as usize / 50).min(captured.len().saturating_sub(tail_start));
            if tail > 0 && destination_offset == 0 && copy == target {
                // Original indexed writes beyond the destination are ignored.
                for i in 0..tail.min(target) {
                    for c in 0..2 {
                        output[i][c] = (f64::from(output[i][c])
                            + f64::from(captured[tail_start + i][c])
                                * (1.0 - i as f64 / tail as f64))
                            .clamp(-1.0, 1.0) as f32;
                    }
                }
            } else {
                let fade = 256.min(copy);
                for i in 0..fade {
                    for sample in &mut output[destination_offset + copy - 1 - i] {
                        *sample = bounded(
                            f64::from(*sample) * (i as f64 / fade as f64 * FRAC_PI_2).sin(),
                        );
                    }
                }
                seam_fade(&mut output, sample_rate, 0.010)?;
            }
        }
        RecordingMode::Free
        | RecordingMode::Dynamic { .. }
        | RecordingMode::SyncFallback { .. } => {
            let length = match mode {
                RecordingMode::Dynamic { target } => {
                    if target == 0 {
                        return Err(Error::InvalidParameter);
                    }
                    target.min(captured.len())
                }
                _ => captured.len(),
            };
            output = captured[..length].to_vec();
            compensate_latency(&mut output, sample_rate, latency_seconds)?;
            if let RecordingMode::SyncFallback { target } = mode {
                if target == 0 {
                    return Err(Error::InvalidParameter);
                }
                if output.len() > target {
                    let tail = target.min(output.len() - target);
                    let fade = tail.min(sample_rate as usize / 20);
                    for i in 0..tail {
                        let gain = if i >= tail - fade {
                            (tail - i) as f64 / fade as f64
                        } else {
                            1.0
                        };
                        let tail_frame = output[target + i];
                        for (sample, tail_sample) in output[i].iter_mut().zip(tail_frame) {
                            *sample = (f64::from(*sample) + f64::from(tail_sample) * gain)
                                .clamp(-1.0, 1.0) as f32;
                        }
                    }
                    output.truncate(target);
                } else {
                    seam_fade(&mut output, sample_rate, 0.015)?;
                }
            } else {
                seam_fade(&mut output, sample_rate, 0.015)?;
            }
        }
    }
    remove_dc(&mut output);
    Ok(output)
}

/// Finalize a raw overdub/substitute take on the worker in source order:
/// DC removal, latency rotation/fades, splice fade, circular punch mix.
pub fn finalize_punch(
    destination: &mut [Frame],
    captured: &[Frame],
    offset: i64,
    sample_rate: u32,
    feedback: f64,
    latency_seconds: f64,
) -> Result<(), Error> {
    if destination.is_empty()
        || captured.is_empty()
        || sample_rate == 0
        || !feedback.is_finite()
        || !(0.0..=1.0).contains(&feedback)
        || !latency_seconds.is_finite()
        || latency_seconds < 0.0
        || destination
            .iter()
            .chain(captured)
            .flatten()
            .any(|sample| !sample.is_finite())
    {
        return Err(Error::InvalidParameter);
    }
    let mut take = captured.to_vec();
    remove_dc(&mut take);
    compensate_latency(&mut take, sample_rate, latency_seconds)?;
    seam_fade(&mut take, sample_rate, 0.008)?;
    mix_circular(destination, &take, offset, sample_rate, feedback)
}

/// Source seam crossfade with per-channel zero-crossing searches and edge fades.
/// Apply once when capturing/importing a take, never again on project reload.
pub fn seam_fade(audio: &mut [Frame], sample_rate: u32, seconds: f64) -> Result<(), Error> {
    if sample_rate == 0 || !seconds.is_finite() || seconds < 0.0 {
        return Err(Error::InvalidParameter);
    }
    let length = audio.len();
    if length < 4 {
        return Ok(());
    }
    let seconds = if seconds == 0.0 { 0.015 } else { seconds };
    let fade = ((f64::from(sample_rate) * seconds).floor() as usize).min(length / 5);
    if fade < 2 {
        return Ok(());
    }
    let search = (fade / 4).min(256);
    for channel in [0, 1] {
        let mut head = 0;
        for i in 1..search {
            if audio[i - 1][channel] <= 0.0 && audio[i][channel] > 0.0 {
                head = i;
                break;
            }
        }
        let mut tail = length;
        // The JS loop uses a strict lower bound: i > length - 1 - search.
        if search > 1 {
            for i in ((length - search)..=(length - 2)).rev() {
                if audio[i - 1][channel] <= 0.0 && audio[i][channel] > 0.0 {
                    tail = i;
                    break;
                }
            }
        }
        let overlap = fade.min(tail - head);
        if overlap < 2 {
            continue;
        }
        let tail_start = tail - overlap;
        for i in 0..overlap {
            let angle = FRAC_PI_2 * i as f64 / overlap as f64;
            let sum = f64::from(audio[head + i][channel]) * angle.sin().sqrt()
                + f64::from(audio[tail_start + i][channel]) * angle.cos().sqrt();
            audio[head + i][channel] = bounded(sum);
        }
        let edge = 8.min(overlap / 10);
        for i in 0..edge {
            let gain = 0.5 - 0.5 * (PI * i as f64 / edge as f64).cos();
            audio[i][channel] = bounded(f64::from(audio[i][channel]) * gain);
            audio[length - 1 - i][channel] =
                bounded(f64::from(audio[length - 1 - i][channel]) * gain);
        }
    }
    Ok(())
}

/// Circular punch mix, including the source's fade envelopes and peak soft clip.
/// Offset is in frames (signed for wraparound); stereo layout is preserved.
pub fn mix_circular(
    destination: &mut [Frame],
    take: &[Frame],
    offset: i64,
    sample_rate: u32,
    feedback: f64,
) -> Result<(), Error> {
    if sample_rate == 0 || !feedback.is_finite() || !(0.0..=1.0).contains(&feedback) {
        return Err(Error::InvalidParameter);
    }
    if destination.is_empty() || take.is_empty() {
        return Ok(());
    }
    let length = i64::try_from(destination.len()).map_err(|_| Error::InvalidCapacity)?;
    let mut position = offset.rem_euclid(length) as usize;
    let fade = (sample_rate as usize / 100).min(take.len() / 10).max(1);
    for (i, frame) in take.iter().enumerate() {
        let edge = if i < fade {
            Some(i)
        } else if i >= take.len() - fade {
            Some(take.len() - 1 - i)
        } else {
            None
        };
        let (old_gain, new_gain) = match edge {
            Some(index) => {
                let angle = index as f64 / fade as f64 * FRAC_PI_2;
                (feedback + (1.0 - feedback) * angle.cos(), angle.sin())
            }
            None => (feedback, 1.0),
        };
        for (channel, &sample) in frame.iter().enumerate() {
            let sum =
                f64::from(destination[position][channel]) * old_gain + f64::from(sample) * new_gain;
            destination[position][channel] = if sum > 0.95 {
                (0.95 + 0.05 * ((sum - 0.95) * 10.0).tanh()) as f32
            } else if sum < -0.95 {
                (-0.95 + 0.05 * ((sum + 0.95) * 10.0).tanh()) as f32
            } else {
                sum as f32
            };
        }
        position += 1;
        if position == destination.len() {
            position = 0;
        }
    }
    Ok(())
}

/// Rotate a captured take left by the measured input/output compensation.
/// Matches the original 5 ms fade at the internal wrap boundary.
pub fn compensate_latency(
    audio: &mut [Frame],
    sample_rate: u32,
    seconds: f64,
) -> Result<(), Error> {
    if sample_rate == 0 || !seconds.is_finite() {
        return Err(Error::InvalidParameter);
    }
    if audio.is_empty() || seconds < 0.0001 {
        return Ok(());
    }
    let shift = ((seconds * f64::from(sample_rate)).floor() as usize).min(audio.len());
    if shift == audio.len() || shift == 0 {
        return Ok(());
    }
    audio.rotate_left(shift);
    let wrap = audio.len() - shift;
    let fade = (sample_rate as usize / 200).min(shift).min(wrap);
    for i in 0..fade {
        let angle = i as f64 / fade as f64 * FRAC_PI_2;
        for channel in [0, 1] {
            audio[wrap - fade + i][channel] =
                bounded(f64::from(audio[wrap - fade + i][channel]) * angle.cos());
            audio[wrap + i][channel] = bounded(f64::from(audio[wrap + i][channel]) * angle.sin());
        }
    }
    Ok(())
}
