use looper_core::Frame;
use looper_stretch::{stretch, Error, MAX_FRAMES};
const RATE: u32 = 48000;
fn tone(length: usize) -> Vec<Frame> {
    (0..length)
        .map(|i| {
            let s = (i as f64 * 440.0 * std::f64::consts::TAU / RATE as f64).sin() as f32 * 0.4;
            [s, s * 0.5]
        })
        .collect()
}
fn frequency(audio: &[Frame]) -> f64 {
    let start = audio.len() / 4;
    let end = audio.len() * 3 / 4;
    let crossings = (start + 1..end)
        .filter(|&i| audio[i - 1][0] <= 0.0 && audio[i][0] > 0.0)
        .count();
    crossings as f64 * RATE as f64 / (end - start) as f64
}
#[test]
fn r3_stretches_without_transposing_and_preserves_stereo_relationship() {
    for ratio in [0.75, 1.5] {
        let result = stretch(&tone(48000), RATE, 2, ratio, 0.0).unwrap();
        assert_eq!(result.len(), (48000.0 * ratio).round() as usize);
        assert!((frequency(&result) - 440.0).abs() < 5.0);
        let error = (result
            .iter()
            .map(|f| f64::from(f[1] - f[0] * 0.5).powi(2))
            .sum::<f64>()
            / result.len() as f64)
            .sqrt();
        // R3 couples channel phase but does not promise bit-identical gain
        // linearity at transient edges. Bound relationship error over the take.
        assert!(error < 0.001, "stereo RMS mismatch {error}");
    }
}
#[test]
fn pitch_is_independent_of_duration_and_mono_has_no_transport_channel_drift() {
    for (pitch, expected) in [(-12.0, 220.0), (12.0, 880.0)] {
        let result = stretch(&tone(48000), RATE, 1, 1.0, pitch).unwrap();
        assert_eq!(result.len(), 48000);
        assert!((frequency(&result) - expected).abs() < 8.0);
        assert!(result.iter().all(|f| f[0].to_bits() == f[1].to_bits()));
    }
}
#[test]
fn final_input_is_flushed_and_silence_and_short_takes_stay_finite() {
    let mut audio = vec![[0.0; 2]; 24000];
    audio[23000..].copy_from_slice(&tone(1000));
    let result = stretch(&audio, RATE, 2, 1.5, 0.0).unwrap();
    assert!(result[result.len() * 3 / 4..]
        .iter()
        .any(|f| f[0].abs() > 0.01));
    for length in [64, 8192] {
        let result = stretch(&vec![[0.0; 2]; length], RATE, 2, 1.5, 0.0).unwrap();
        assert!(result.iter().flatten().all(|s| *s == 0.0));
    }
}
#[test]
fn invalid_metadata_and_excess_output_are_rejected_before_native_processing() {
    let audio = tone(16);
    for (rate, channels, ratio, pitch) in [
        (0, 2, 1.5, 0.0),
        (RATE, 0, 1.5, 0.0),
        (RATE, 3, 1.5, 0.0),
        (RATE, 2, 0.01, 0.0),
        (RATE, 2, f64::NAN, 0.0),
        (RATE, 2, 1.5, f64::INFINITY),
        (RATE, 2, 1.5, 25.0),
    ] {
        assert_eq!(
            stretch(&audio, rate, channels, ratio, pitch),
            Err(Error::InvalidParameter)
        );
    }
    assert_eq!(
        stretch(&audio, RATE, 2, MAX_FRAMES as f64, 0.0),
        Err(Error::Capacity)
    );
    assert_eq!(
        stretch(&[], RATE, 2, 1.5, 0.0),
        Err(Error::InvalidParameter)
    );
    assert_eq!(
        stretch(&[[f32::NAN, 0.0]], RATE, 2, 1.5, 0.0),
        Err(Error::InvalidParameter)
    );
}
