//! Worker-side RIFF/WAVE import and export. Never call these from the callback.
#![forbid(unsafe_code)]

use looper_core::Frame;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Format {
    Pcm16,
    Pcm24,
    Float32,
}

impl Format {
    fn bits(self) -> u16 {
        match self {
            Self::Pcm16 => 16,
            Self::Pcm24 => 24,
            Self::Float32 => 32,
        }
    }
    fn tag(self) -> u16 {
        if self == Self::Float32 {
            3
        } else {
            1
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    InvalidHeader,
    InvalidSize,
    InvalidRate,
    UnsupportedFormat,
    NonfiniteSample,
}

#[derive(Debug)]
pub struct Wave {
    pub sample_rate: u32,
    /// Mono imports are duplicated to stereo frames; this retains file layout.
    pub channels: u16,
    pub format: Format,
    pub frames: Vec<Frame>,
}

fn u16_at(bytes: &[u8], offset: usize) -> u16 {
    u16::from_le_bytes(bytes[offset..offset + 2].try_into().unwrap())
}
fn u32_at(bytes: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes(bytes[offset..offset + 4].try_into().unwrap())
}

/// Encode mono/stereo without resampling. Integer export clips to [-1,1] and
/// truncates as in the original WAV16 encoder. Float export retains headroom.
/// For mono export only the left channel is written. No dither is applied.
pub fn encode(
    frames: &[Frame],
    sample_rate: u32,
    channels: u16,
    format: Format,
) -> Result<Vec<u8>, Error> {
    if !(1..=2).contains(&channels) {
        return Err(Error::UnsupportedFormat);
    }
    if sample_rate == 0 {
        return Err(Error::InvalidRate);
    }
    if frames.iter().any(|frame| {
        frame[..channels as usize]
            .iter()
            .any(|sample| !sample.is_finite())
    }) {
        return Err(Error::NonfiniteSample);
    }
    let align = channels * (format.bits() / 8);
    let rate = sample_rate
        .checked_mul(u32::from(align))
        .ok_or(Error::InvalidRate)?;
    let data_length = frames
        .len()
        .checked_mul(usize::from(align))
        .ok_or(Error::InvalidSize)?;
    let data_length = u32::try_from(data_length).map_err(|_| Error::InvalidSize)?;
    let overhead = if format == Format::Float32 {
        50_u32
    } else {
        36
    };
    let riff_length = data_length
        .checked_add(overhead)
        .ok_or(Error::InvalidSize)?;
    let padded = riff_length
        .checked_add(data_length % 2)
        .ok_or(Error::InvalidSize)?;
    let mut output = Vec::new();
    output
        .try_reserve_exact(padded as usize + 8)
        .map_err(|_| Error::InvalidSize)?;
    output.extend_from_slice(b"RIFF");
    output.extend_from_slice(&padded.to_le_bytes());
    output.extend_from_slice(b"WAVEfmt ");
    output.extend_from_slice(
        &(if format == Format::Float32 {
            18_u32
        } else {
            16
        })
        .to_le_bytes(),
    );
    output.extend_from_slice(&format.tag().to_le_bytes());
    output.extend_from_slice(&channels.to_le_bytes());
    output.extend_from_slice(&sample_rate.to_le_bytes());
    output.extend_from_slice(&rate.to_le_bytes());
    output.extend_from_slice(&align.to_le_bytes());
    output.extend_from_slice(&format.bits().to_le_bytes());
    if format == Format::Float32 {
        output.extend_from_slice(&0_u16.to_le_bytes()); // WAVEFORMATEX extension size.
        output.extend_from_slice(b"fact");
        output.extend_from_slice(&4_u32.to_le_bytes());
        output.extend_from_slice(&(frames.len() as u32).to_le_bytes());
    }
    output.extend_from_slice(b"data");
    output.extend_from_slice(&data_length.to_le_bytes());
    for frame in frames {
        for &sample in &frame[..channels as usize] {
            match format {
                Format::Float32 => output.extend_from_slice(&sample.to_le_bytes()),
                Format::Pcm16 => {
                    let sample = f64::from(sample).clamp(-1.0, 1.0);
                    let quantized = (sample * if sample < 0.0 { 32768.0 } else { 32767.0 }) as i16;
                    output.extend_from_slice(&quantized.to_le_bytes());
                }
                Format::Pcm24 => {
                    let sample = f64::from(sample).clamp(-1.0, 1.0);
                    let quantized =
                        (sample * if sample < 0.0 { 8388608.0 } else { 8388607.0 }) as i32;
                    output.extend_from_slice(&quantized.to_le_bytes()[..3]);
                }
            }
        }
    }
    if data_length % 2 != 0 {
        output.push(0);
    }
    Ok(output)
}

/// Read bounded RIFF chunks, including unknown/padded metadata chunks. Supports
/// uncompressed PCM16, PCM24 and IEEE float32, one or two channels, including
/// extensible WAV with PCM16/24 left-aligned in wider containers. Compressed
/// formats, RF64 and resampling are not implemented yet.
pub fn decode(bytes: &[u8]) -> Result<Wave, Error> {
    decode_with_limit(bytes, usize::MAX)
}

/// Reject oversized audio before allocating decoded PCM storage.
pub fn decode_with_limit(bytes: &[u8], max_frames: usize) -> Result<Wave, Error> {
    decode_with_scaling(bytes, max_frames, false)
}

/// Desktop compatibility adapter: some browser decoders map the largest positive
/// integer to +1 rather than using the negative full-scale divisor on both signs.
/// Native file tools retain the standard symmetric scaling by default.
pub fn decode_with_scaling(
    bytes: &[u8],
    max_frames: usize,
    positive_full_scale: bool,
) -> Result<Wave, Error> {
    decode_with_integer_scaling(
        bytes,
        max_frames,
        if positive_full_scale {
            IntegerScaling::PositiveFullScale
        } else {
            IntegerScaling::Symmetric
        },
    )
}

/// Integer conversion selected by a desktop decoder probe. Reciprocal mode
/// preserves the host's float32 multiplication rounding as well as its peak.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum IntegerScaling {
    Symmetric,
    PositiveFullScale,
    PositiveReciprocal,
}

impl IntegerScaling {
    fn convert(self, raw: i32, bits: u16) -> f32 {
        let divisor = if self != Self::Symmetric && raw > 0 {
            (1_u32 << (bits - 1)) - 1
        } else {
            1_u32 << (bits - 1)
        } as f32;
        if self == Self::PositiveReciprocal && raw > 0 {
            raw as f32 * (1.0 / divisor)
        } else {
            raw as f32 / divisor
        }
    }
}

pub fn decode_with_integer_scaling(
    bytes: &[u8],
    max_frames: usize,
    scaling: IntegerScaling,
) -> Result<Wave, Error> {
    if bytes.len() < 12 || &bytes[..4] != b"RIFF" || &bytes[8..12] != b"WAVE" {
        return Err(Error::InvalidHeader);
    }
    let end = (u32_at(bytes, 4) as usize)
        .checked_add(8)
        .ok_or(Error::InvalidSize)?;
    if end < 12 || end > bytes.len() {
        return Err(Error::InvalidSize);
    }
    let mut offset = 12_usize;
    let mut fmt = None;
    let mut data = None;
    while offset < end {
        if end - offset < 8 {
            return Err(Error::InvalidSize);
        }
        let length = u32_at(bytes, offset + 4) as usize;
        let start = offset + 8;
        let stop = start.checked_add(length).ok_or(Error::InvalidSize)?;
        let next = stop.checked_add(length % 2).ok_or(Error::InvalidSize)?;
        if next > end {
            return Err(Error::InvalidSize);
        }
        match &bytes[offset..offset + 4] {
            b"fmt " => {
                if fmt.is_some() || length < 16 {
                    return Err(Error::InvalidHeader);
                }
                fmt = Some(&bytes[start..stop]);
            }
            b"data" => {
                if data.is_some() {
                    return Err(Error::InvalidHeader);
                }
                data = Some(&bytes[start..stop]);
            }
            _ => (),
        }
        offset = next;
    }
    let fmt = fmt.ok_or(Error::InvalidHeader)?;
    let data = data.ok_or(Error::InvalidHeader)?;
    let channels = u16_at(fmt, 2);
    let sample_rate = u32_at(fmt, 4);
    let container_bits = u16_at(fmt, 14);
    let mut tag = u16_at(fmt, 0);
    let mut valid_bits = container_bits;
    if tag == 0xfffe {
        // WAVEFORMATEXTENSIBLE: require the full extension and the complete
        // standard PCM/IEEE GUID, not just its two-byte format prefix.
        if fmt.len() < 40 || u16_at(fmt, 16) < 22 || usize::from(u16_at(fmt, 16)) + 18 > fmt.len() {
            return Err(Error::InvalidHeader);
        }
        if fmt[26..40] != [0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71] {
            return Err(Error::UnsupportedFormat);
        }
        tag = u16_at(fmt, 24);
        valid_bits = u16_at(fmt, 18);
        if valid_bits == 0 || valid_bits > container_bits {
            return Err(Error::InvalidHeader);
        }
        // The engine represents ordinary mono or left/right stereo. Other
        // speaker assignments would need explicit routing, not reinterpretation.
        let mask = u32_at(fmt, 20);
        if !matches!((channels, mask), (1, 0 | 4) | (2, 0 | 3)) {
            return Err(Error::UnsupportedFormat);
        }
    }
    let format = match (tag, valid_bits) {
        (1, 16) => Format::Pcm16,
        (1, 24) => Format::Pcm24,
        (3, 32) => Format::Float32,
        _ => return Err(Error::UnsupportedFormat),
    };
    if !matches!(container_bits, 16 | 24 | 32)
        || container_bits < valid_bits
        || (format == Format::Float32 && container_bits != 32)
    {
        return Err(Error::UnsupportedFormat);
    }
    if !(1..=2).contains(&channels) {
        return Err(Error::UnsupportedFormat);
    }
    if sample_rate == 0 {
        return Err(Error::InvalidRate);
    }
    let align = usize::from(channels * (container_bits / 8));
    let rate = sample_rate
        .checked_mul(align as u32)
        .ok_or(Error::InvalidRate)?;
    if u16_at(fmt, 12) as usize != align || u32_at(fmt, 8) != rate || data.len() % align != 0 {
        return Err(Error::InvalidHeader);
    }
    if data.len() / align > max_frames {
        return Err(Error::InvalidSize);
    }
    let mut frames = Vec::new();
    frames
        .try_reserve_exact(data.len() / align)
        .map_err(|_| Error::InvalidSize)?;
    for block in data.chunks_exact(align) {
        let mut frame = [0.0; 2];
        let sample_size = usize::from(container_bits / 8);
        for (channel, sample) in frame.iter_mut().enumerate().take(usize::from(channels)) {
            let offset = channel * sample_size;
            *sample = match format {
                Format::Pcm16 | Format::Pcm24 => {
                    let mut packed = [0_u8; 4];
                    packed[..sample_size].copy_from_slice(&block[offset..offset + sample_size]);
                    let padding = container_bits - valid_bits;
                    let raw = u32::from_le_bytes(packed);
                    if raw & ((1_u32 << padding) - 1) != 0 {
                        return Err(Error::InvalidHeader);
                    }
                    // Sign-extend the container before removing low padding.
                    let raw = ((raw << (32 - container_bits)) as i32) >> (32 - valid_bits);
                    scaling.convert(raw, valid_bits)
                }
                Format::Float32 => {
                    f32::from_le_bytes(block[offset..offset + 4].try_into().unwrap())
                }
            };
            if !sample.is_finite() {
                return Err(Error::NonfiniteSample);
            }
        }
        if channels == 1 {
            frame[1] = frame[0];
        }
        frames.push(frame);
    }
    Ok(Wave {
        sample_rate,
        channels,
        format,
        frames,
    })
}
