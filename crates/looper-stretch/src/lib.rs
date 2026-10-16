//! Native Rubber Band 4 R3 offline stretching. Call only on a control worker.
//! No native pointers or exceptions escape this module.
use looper_core::Frame;
pub const MAX_FRAMES: usize = 1_000_000;

#[derive(Debug, PartialEq, Eq)]
pub enum Error {
    InvalidParameter,
    Capacity,
    Native(i32),
    NonfiniteOutput,
}

unsafe extern "C" {
    fn looper_stretch(
        input: *const f32,
        frames: u32,
        rate: u32,
        channels: u32,
        ratio: f64,
        pitch: f64,
        output: *mut f32,
        expected: u32,
    ) -> i32;
}

/// Preserve stereo alignment; mono uses one native channel and duplicates the
/// result into the engine's transport pair. Output has round(input * ratio) frames.
pub fn stretch(
    input: &[Frame],
    rate: u32,
    channels: u16,
    ratio: f64,
    semitones: f64,
) -> Result<Vec<Frame>, Error> {
    if input.is_empty()
        || !(8000..=192000).contains(&rate)
        || !(1..=2).contains(&channels)
        || !ratio.is_finite()
        || ratio <= 0.01
        || !semitones.is_finite()
        || !(-24.0..=24.0).contains(&semitones)
        || input.iter().flatten().any(|s| !s.is_finite())
    {
        return Err(Error::InvalidParameter);
    }
    let length = (input.len() as f64 * ratio).round().max(1.0);
    if input.len() > MAX_FRAMES || length > MAX_FRAMES as f64 {
        return Err(Error::Capacity);
    }
    let mut output: Vec<Frame> = Vec::new();
    output
        .try_reserve_exact(length as usize)
        .map_err(|_| Error::Capacity)?;
    output.resize(length as usize, [0.0; 2]);
    // SAFETY: Frame is [f32;2], contiguous and aligned. Both buffers remain live
    // for this synchronous call. Lengths bound native reads/writes, and the C++
    // shim catches every exception. It retains neither pointer after returning.
    let result = unsafe {
        looper_stretch(
            input.as_ptr().cast(),
            input.len() as u32,
            rate,
            u32::from(channels),
            ratio,
            2.0_f64.powf(semitones / 12.0),
            output.as_mut_ptr().cast(),
            output.len() as u32,
        )
    };
    if result != 0 {
        return Err(Error::Native(result));
    }
    if output.iter().flatten().any(|s| !s.is_finite()) {
        return Err(Error::NonfiniteOutput);
    }
    Ok(output)
}
