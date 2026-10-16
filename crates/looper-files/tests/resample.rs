use looper_files::{
    decode_resampled_with_limit, encode,
    resample::{convert, output_frames},
    Error, Format,
};
use std::f64::consts::TAU;
fn tone(rate: u32, hz: f64, length: usize) -> Vec<[f32; 2]> {
    (0..length)
        .map(|i| {
            let x = (TAU * hz * i as f64 / f64::from(rate)).sin() as f32;
            [x, -0.5 * x]
        })
        .collect()
}
#[test]
fn same_rate_is_bit_exact_and_reuses_storage() {
    let input = vec![[0.0, -0.0], [3.5, -7.0], [f32::MIN_POSITIVE, 0.25]];
    let ptr = input.as_ptr();
    let expected = input.clone();
    let result = convert(input, 48000, 48000, 3).unwrap();
    assert_eq!(ptr, result.as_ptr());
    for (a, b) in result.iter().flatten().zip(expected.iter().flatten()) {
        assert_eq!(a.to_bits(), b.to_bits());
    }
}
#[test]
fn conversion_preserves_duration_frequency_stereo_and_headroom() {
    for (from, to) in [
        (44100, 48000),
        (48000, 44100),
        (96000, 48000),
        (8000, 192000),
        (192000, 8000),
    ] {
        let input = tone(from, 1000.0, from as usize / 20);
        let output = convert(input, from, to, to as usize / 20).unwrap();
        assert_eq!(output.len(), to as usize / 20);
        let mut error = 0.0;
        let mut count = 0;
        // Exclude the endpoint extension in destination-frame units, including
        // extreme upsampling where one input frame spans 24 output frames.
        let guard = 128.max((64_u64 * u64::from(to)).div_ceil(u64::from(from)) as usize);
        for (i, frame) in output
            .iter()
            .enumerate()
            .skip(guard)
            .take(output.len().saturating_sub(2 * guard))
        {
            let expected = (TAU * 1000.0 * i as f64 / f64::from(to)).sin();
            error += (f64::from(frame[0]) - expected).powi(2);
            count += 1;
            assert!((frame[1] + 0.5 * frame[0]).abs() < 1e-6);
        }
        assert!(
            (error / count as f64).sqrt() < 0.0002,
            "{from}->{to} RMS error {}",
            (error / count as f64).sqrt()
        );
    }
    let dc = convert(vec![[3.5, -7.0]; 100], 44100, 48000, 109).unwrap();
    assert!(dc
        .iter()
        .all(|v| (v[0] - 3.5).abs() < 1e-5 && (v[1] + 7.0).abs() < 1e-5));
}
#[test]
fn downsampling_suppresses_out_of_band_aliases() {
    let output = convert(tone(48000, 12000.0, 4800), 48000, 16000, 1600).unwrap();
    let rms = (output[128..1472]
        .iter()
        .map(|v| f64::from(v[0]).powi(2))
        .sum::<f64>()
        / 1344.0)
        .sqrt();
    assert!(rms < 0.0001, "Out-of-band RMS {rms}");
}
#[test]
fn impulse_has_no_filter_delay_or_channel_swap() {
    let mut input = vec![[0.0; 2]; 1000];
    input[441] = [1.0, 0.0];
    let out = convert(input, 44100, 48000, 1088).unwrap();
    let peak = out
        .iter()
        .enumerate()
        .max_by(|(_, a), (_, b)| a[0].abs().total_cmp(&b[0].abs()))
        .unwrap()
        .0;
    assert_eq!(peak, 480);
    assert!(out.iter().all(|v| v[1] == 0.0));
}
#[test]
fn capacity_and_rates_are_checked_before_decoded_pcm_allocation() {
    let mut bytes = encode(&[[0.25; 2]; 441], 44100, 2, Format::Float32).unwrap();
    // With an undersized destination, capacity wins over a malformed sample.
    bytes[58..62].copy_from_slice(&f32::NAN.to_le_bytes());
    assert_eq!(
        decode_resampled_with_limit(&bytes, 48000, 479).unwrap_err(),
        Error::InvalidSize
    );
    assert_eq!(
        decode_resampled_with_limit(&bytes, 48000, 480).unwrap_err(),
        Error::NonfiniteSample
    );
    for rate in [0, 7999, 192001, u32::MAX] {
        assert_eq!(output_frames(4, rate, 48000), Err(Error::InvalidRate));
        assert_eq!(output_frames(4, 48000, rate), Err(Error::InvalidRate));
    }
    assert_eq!(output_frames(0, 48000, 44100), Err(Error::InvalidSize));
    assert_eq!(
        output_frames(32_000_001, 48000, 48000),
        Err(Error::InvalidSize)
    );
    assert_eq!(output_frames(1, 192000, 8000), Ok(1));
    assert_eq!(
        convert(vec![[f32::INFINITY; 2]], 48000, 44100, 1),
        Err(Error::NonfiniteSample)
    );
}
#[test]
fn input_budget_uses_duration_rather_than_destination_frame_count() {
    let bytes = encode(&vec![[0.25, -0.125]; 72000], 96000, 2, Format::Pcm16).unwrap();
    let (wave, source) = decode_resampled_with_limit(&bytes, 48000, 36000).unwrap();
    assert_eq!(source, 96000);
    assert_eq!(wave.sample_rate, 48000);
    assert_eq!(wave.frames.len(), 36000);
    assert_eq!(
        decode_resampled_with_limit(&bytes, 48000, 35999).unwrap_err(),
        Error::InvalidSize
    );
    let empty = encode(&[], 48000, 2, Format::Pcm16).unwrap();
    assert_eq!(
        decode_resampled_with_limit(&empty, 48000, 100).unwrap_err(),
        Error::InvalidSize
    );
}
