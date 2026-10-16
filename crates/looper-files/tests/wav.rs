use looper_files::{decode, decode_with_limit, encode, Error, Format};

#[test]
fn bounded_decode_rejects_frame_count_before_decoded_storage_is_allocated() {
    let bytes = encode(&[[0.25, -0.5]; 3], 48000, 2, Format::Float32).unwrap();
    assert_eq!(
        decode_with_limit(&bytes, 2).unwrap_err(),
        Error::InvalidSize
    );
    assert_eq!(decode_with_limit(&bytes, 3).unwrap().frames.len(), 3);
}

#[test]
fn browser_integer_scaling_is_explicit_and_leaves_native_defaults_unchanged() {
    for format in [Format::Pcm16, Format::Pcm24] {
        let bytes = encode(&[[1.0, -1.0], [0.125, -0.25]], 48000, 2, format).unwrap();
        let native = decode(&bytes).unwrap();
        let browser = looper_files::decode_with_scaling(&bytes, 2, true).unwrap();
        assert!(native.frames[0][0] < 1.0);
        assert_eq!(browser.frames[0], [1.0, -1.0]);
        assert!(browser.frames[1][0] > native.frames[1][0]);
        assert_eq!(browser.frames[1][1], native.frames[1][1]);
    }
    // The peak alone cannot distinguish division from rounded reciprocal
    // multiplication. This interior value differs by one float32 step.
    let mut bytes = encode(&[[0.0, -0.25]], 48000, 2, Format::Pcm16).unwrap();
    bytes[44..46].copy_from_slice(&8784_i16.to_le_bytes());
    let divided = looper_files::decode_with_scaling(&bytes, 1, true).unwrap();
    let reciprocal = looper_files::decode_with_integer_scaling(
        &bytes,
        1,
        looper_files::IntegerScaling::PositiveReciprocal,
    )
    .unwrap();
    assert_eq!(divided.frames[0][0].to_bits(), 0x3e89_4113);
    assert_eq!(reciprocal.frames[0][0].to_bits(), 0x3e89_4112);
    assert_eq!(reciprocal.frames[0][1], divided.frames[0][1]);
    assert_eq!(decode(&bytes).unwrap().frames[0][0], 8784.0_f32 / 32768.0);
}

#[allow(dead_code, clippy::excessive_precision)]
mod reference {
    include!("../../looper-core/tests/fixtures/original_dsp.rs");
}

#[test]
fn wav16_export_is_byte_identical_to_the_original_encoder() {
    let audio = [[-1.0, 1.0], [0.125, -0.25], [0.0, 0.5]];
    assert_eq!(
        encode(&audio, 48000, 2, Format::Pcm16).unwrap(),
        reference::WAV16_REFERENCE
    );
}

#[test]
fn float_export_retains_every_sample_and_headroom() {
    let audio = [[-1.25, 2.0], [0.000000123, -0.75], [f32::MIN_POSITIVE, 0.0]];
    let bytes = encode(&audio, 96000, 2, Format::Float32).unwrap();
    assert_eq!(&bytes[38..42], b"fact");
    let wav = decode(&bytes).unwrap();
    assert_eq!(wav.sample_rate, 96000);
    assert_eq!(wav.channels, 2);
    assert_eq!(wav.format, Format::Float32);
    assert_eq!(wav.frames, audio);
}

#[test]
fn pcm24_has_correct_sign_extrema_and_quantization() {
    let audio = [[-1.0, 1.0], [0.125, -0.25], [2.0, -2.0]];
    let bytes = encode(&audio, 44100, 2, Format::Pcm24).unwrap();
    assert_eq!(&bytes[44..50], &[0, 0, 128, 255, 255, 127]);
    let wav = decode(&bytes).unwrap();
    assert_eq!(wav.frames[0][0], -1.0);
    assert_eq!(wav.frames[0][1], 1.0 - 1.0 / 8388608.0);
    assert_eq!(wav.frames[2], [wav.frames[0][1], wav.frames[0][0]]);
    // Positive and negative full-scale use the original asymmetric conversion.
    for (a, b) in wav.frames[1].iter().zip(audio[1]) {
        assert!((*a - b).abs() <= 1.0 / 8388608.0);
    }
}

#[test]
fn mono_pcm24_padding_and_duplication_are_valid() {
    let bytes = encode(&[[0.5, 9.0]], 48000, 1, Format::Pcm24).unwrap();
    assert_eq!(bytes.len(), 48);
    assert_eq!(u32::from_le_bytes(bytes[40..44].try_into().unwrap()), 3);
    assert_eq!(bytes[47], 0);
    let wav = decode(&bytes).unwrap();
    assert_eq!(wav.channels, 1);
    assert_eq!(wav.frames[0][0], wav.frames[0][1]);
}

#[test]
fn unknown_odd_sized_chunks_are_skipped_with_their_padding() {
    let mut bytes = encode(&[[0.25; 2]], 48000, 2, Format::Pcm16).unwrap();
    let chunk = [b'J', b'U', b'N', b'K', 1, 0, 0, 0, 42, 0];
    bytes.splice(12..12, chunk);
    let size = (bytes.len() - 8) as u32;
    bytes[4..8].copy_from_slice(&size.to_le_bytes());
    assert_eq!(decode(&bytes).unwrap().frames.len(), 1);
}

#[test]
fn truncation_and_malformed_headers_are_rejected_without_panics() {
    let bytes = encode(&[[0.25; 2]; 4], 48000, 2, Format::Float32).unwrap();
    for length in 0..bytes.len() {
        assert!(decode(&bytes[..length]).is_err());
    }
    let mut malformed = bytes.clone();
    malformed[32..34].copy_from_slice(&0_u16.to_le_bytes());
    assert!(decode(&malformed).is_err());
    malformed = bytes;
    malformed[54..58].copy_from_slice(&u32::MAX.to_le_bytes());
    assert!(decode(&malformed).is_err());
}

#[test]
fn nonfinite_samples_and_invalid_configuration_are_rejected() {
    assert_eq!(
        encode(&[[f32::NAN; 2]], 48000, 2, Format::Float32),
        Err(Error::NonfiniteSample)
    );
    assert_eq!(
        encode(&[[0.0; 2]], 0, 2, Format::Pcm16),
        Err(Error::InvalidRate)
    );
    assert_eq!(
        encode(&[[0.0; 2]], 48000, 3, Format::Pcm16),
        Err(Error::UnsupportedFormat)
    );
    let mut bytes = encode(&[[0.0; 2]], 48000, 2, Format::Float32).unwrap();
    bytes[58..62].copy_from_slice(&f32::INFINITY.to_le_bytes());
    assert!(matches!(decode(&bytes), Err(Error::NonfiniteSample)));
}
