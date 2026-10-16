use looper_files::{decode, decode_with_limit, Error, Format, IntegerScaling};

// Independent fixture writer: fields follow Microsoft's WAVEFORMATEXTENSIBLE
// layout. It intentionally does not use the production WAV encoder.
fn wave(channels: u16, bits: u16, valid: u16, tag: u16, samples: &[u32]) -> Vec<u8> {
    let size = samples.len() * usize::from(bits / 8);
    let mut bytes = vec![0; 68 + size + size % 2];
    bytes[..4].copy_from_slice(b"RIFF");
    let length = (bytes.len() - 8) as u32;
    bytes[4..8].copy_from_slice(&length.to_le_bytes());
    bytes[8..16].copy_from_slice(b"WAVEfmt ");
    bytes[16..20].copy_from_slice(&40_u32.to_le_bytes());
    for (offset, value) in [
        (20, 0xfffe),
        (22, channels),
        (32, channels * bits / 8),
        (34, bits),
        (36, 22),
        (38, valid),
        (44, tag),
    ] {
        bytes[offset..offset + 2].copy_from_slice(&value.to_le_bytes());
    }
    bytes[24..28].copy_from_slice(&48000_u32.to_le_bytes());
    bytes[28..32].copy_from_slice(&(48000 * u32::from(channels * bits / 8)).to_le_bytes());
    bytes[40..44].copy_from_slice(&(if channels == 1 { 4_u32 } else { 3 }).to_le_bytes());
    bytes[50..60].copy_from_slice(&[0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71]);
    bytes[60..64].copy_from_slice(b"data");
    bytes[64..68].copy_from_slice(&(size as u32).to_le_bytes());
    for (index, sample) in samples.iter().enumerate() {
        let start = 68 + index * usize::from(bits / 8);
        bytes[start..start + usize::from(bits / 8)]
            .copy_from_slice(&sample.to_le_bytes()[..usize::from(bits / 8)]);
    }
    bytes
}

#[test]
fn extensible_integer_containers_preserve_sign_precision_layout_and_scaling() {
    for (bits, valid) in [(16, 16), (24, 16), (32, 16), (24, 24), (32, 24)] {
        for channels in [1, 2] {
            let maximum = (1_u32 << (valid - 1)) - 1;
            let shift = bits - valid;
            let bytes = wave(
                channels,
                bits,
                valid,
                1,
                &[
                    maximum << shift,
                    (1_u32 << (valid - 1)) << shift,
                    1 << shift,
                    ((1_u32 << valid) - 1) << shift,
                ],
            );
            let decoded = decode(&bytes).unwrap();
            assert_eq!(decoded.channels, channels);
            assert_eq!(decoded.sample_rate, 48000);
            assert_eq!(
                decoded.format,
                if valid == 16 {
                    Format::Pcm16
                } else {
                    Format::Pcm24
                }
            );
            let samples: Vec<_> = decoded
                .frames
                .iter()
                .flat_map(|frame| frame[..channels as usize].iter().copied())
                .collect();
            let unit = 1.0 / (1_u32 << (valid - 1)) as f32;
            assert_eq!(samples, [maximum as f32 * unit, -1.0, unit, -unit]);
            if channels == 1 {
                assert!(decoded.frames.iter().all(|frame| frame[0] == frame[1]));
            }
            for scaling in [
                IntegerScaling::PositiveFullScale,
                IntegerScaling::PositiveReciprocal,
            ] {
                let browser =
                    looper_files::decode_with_integer_scaling(&bytes, 4, scaling).unwrap();
                assert_eq!(browser.frames[0][0], 1.0);
            }
            assert_eq!(
                decode_with_limit(&bytes, 0).unwrap_err(),
                Error::InvalidSize
            );
        }
    }
}

#[test]
fn extensible_float_preserves_headroom_and_signed_zero() {
    for channels in [1, 2] {
        let bytes = wave(
            channels,
            32,
            32,
            3,
            &[1.5_f32.to_bits(), (-0.0_f32).to_bits()],
        );
        let decoded = decode(&bytes).unwrap();
        assert_eq!(decoded.format, Format::Float32);
        assert_eq!(decoded.frames[0][0], 1.5);
        let zero = if channels == 1 {
            decoded.frames[1][0]
        } else {
            decoded.frames[0][1]
        };
        assert_eq!(zero.to_bits(), (-0.0_f32).to_bits());
    }
    assert_eq!(
        decode(&wave(1, 32, 32, 3, &[f32::INFINITY.to_bits()])).unwrap_err(),
        Error::NonfiniteSample
    );
}

#[test]
fn malformed_extensions_and_unsupported_routing_are_rejected() {
    let bytes = wave(2, 32, 24, 1, &[0x7fffff00, 0x80000000]);
    for size in 0..bytes.len() {
        assert!(decode(&bytes[..size]).is_err());
    }
    for (offset, value, error) in [
        (36, 21, Error::InvalidHeader),
        (36, 23, Error::InvalidHeader),
        (38, 0, Error::InvalidHeader),
        (38, 33, Error::InvalidHeader),
        (38, 20, Error::UnsupportedFormat),
        (40, 4, Error::UnsupportedFormat),
        (44, 2, Error::UnsupportedFormat),
        (46, 1, Error::UnsupportedFormat),
        (59, 0, Error::UnsupportedFormat),
        (68, 1, Error::InvalidHeader),
        (32, 0, Error::InvalidHeader),
    ] {
        let mut invalid = bytes.clone();
        invalid[offset] = value;
        assert_eq!(decode(&invalid).unwrap_err(), error, "offset {offset}");
    }
    let mut unspecified = bytes;
    unspecified[40..44].fill(0);
    assert!(decode(&unspecified).is_ok());
    // A declared fmt chunk too short for its extension must fail before any
    // extension indexing, even when the RIFF size itself is correct.
    let mut short = wave(1, 16, 16, 1, &[0]);
    short.drain(38..60);
    short[16..20].copy_from_slice(&18_u32.to_le_bytes());
    let length = (short.len() - 8) as u32;
    short[4..8].copy_from_slice(&length.to_le_bytes());
    assert_eq!(decode(&short).unwrap_err(), Error::InvalidHeader);
}
