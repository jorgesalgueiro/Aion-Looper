use looper_core::{buffer, Frame};

#[allow(dead_code, clippy::excessive_precision)]
mod reference {
    include!("fixtures/original_dsp.rs");
}

fn compare(actual: &[Frame], expected: &[Frame]) {
    assert_eq!(actual.len(), expected.len());
    for (i, (a, b)) in actual.iter().zip(expected).enumerate() {
        for channel in 0..2 {
            assert!(
                (a[channel] - b[channel]).abs() < 0.000002,
                "frame {i}, channel {channel}: {} != {}",
                a[channel],
                b[channel]
            );
        }
    }
}

#[test]
fn seam_zero_crossings_and_edge_fades_match_javascript() {
    let mut audio = reference::SEAM_INPUT.to_vec();
    buffer::seam_fade(&mut audio, 1000, 0.025).unwrap();
    compare(&audio, reference::SEAM_OUTPUT);
}

#[test]
fn selected_slice_matches_original_floor_boundaries_without_fades() {
    let sliced = buffer::slice(reference::SLICE_INPUT, 7, 22).unwrap();
    compare(&sliced, reference::SLICE_OUTPUT);
    assert_eq!(
        buffer::slice(reference::SLICE_INPUT, 0, 37).unwrap(),
        reference::SLICE_INPUT
    );
    for (start, length) in [(0, 0), (37, 1), (20, 18), (usize::MAX, 1)] {
        assert!(buffer::slice(reference::SLICE_INPUT, start, length).is_err());
    }
    assert!(buffer::slice(&[[f32::NAN, 0.0]], 0, 1).is_err());
}

#[test]
fn export_linear_fades_match_source_and_preserve_short_takes() {
    for (input, seconds, expected) in [
        (
            reference::EXPORT_LONG_INPUT,
            0.01,
            reference::EXPORT_LONG_OUTPUT,
        ),
        (
            reference::EXPORT_SHORT_INPUT,
            0.01,
            reference::EXPORT_SHORT_OUTPUT,
        ),
        (
            reference::EXPORT_TINY_INPUT,
            0.01,
            reference::EXPORT_TINY_OUTPUT,
        ),
        (
            reference::EXPORT_ZERO_INPUT,
            0.0,
            reference::EXPORT_ZERO_OUTPUT,
        ),
        (
            reference::EXPORT_FULL_INPUT,
            1.0,
            reference::EXPORT_FULL_OUTPUT,
        ),
    ] {
        let mut audio = input.to_vec();
        buffer::export_fades(&mut audio, 1000, seconds).unwrap();
        compare(&audio, expected);
    }
    let mut audio = reference::EXPORT_LONG_INPUT.to_vec();
    for (rate, seconds) in [
        (0, 0.01),
        (1000, -1.0),
        (1000, f64::INFINITY),
        (1000, f64::NAN),
    ] {
        assert!(buffer::export_fades(&mut audio, rate, seconds).is_err());
        assert_eq!(audio, reference::EXPORT_LONG_INPUT);
    }
    assert!(buffer::export_fades(&mut [], 1000, 0.01).is_ok());
}

#[test]
fn recording_modes_match_complete_original_finalizer() {
    use buffer::RecordingMode::*;
    for (mode, latency, expected) in [
        (Free, 0.0, reference::RECORD_FREE_ZERO),
        (Free, 0.009, reference::RECORD_FREE_LATENCY),
        (Free, 1.0, reference::RECORD_FREE_FULL),
        (
            Dynamic { target: 120 },
            0.009,
            reference::RECORD_DYNAMIC_TRIM,
        ),
        (
            Dynamic { target: 200 },
            0.009,
            reference::RECORD_DYNAMIC_SHORT,
        ),
        (Free, 0.009, reference::RECORD_DYNAMIC_INVALID_BAR),
        (
            Precision {
                target: 90,
                start: 22,
                destination_offset: 0,
            },
            0.009,
            reference::RECORD_PRECISION_TAIL,
        ),
        (
            Precision {
                target: 90,
                start: 0,
                destination_offset: 18,
            },
            0.009,
            reference::RECORD_PRECISION_LATE,
        ),
        (
            Precision {
                target: 200,
                start: 22,
                destination_offset: 0,
            },
            0.009,
            reference::RECORD_PRECISION_SHORT,
        ),
        (
            Precision {
                target: 8,
                start: 22,
                destination_offset: 0,
            },
            0.009,
            reference::RECORD_PRECISION_TINY,
        ),
        (
            SyncFallback { target: 90 },
            0.009,
            reference::RECORD_SYNC_FOLD,
        ),
        (
            SyncFallback { target: 200 },
            0.009,
            reference::RECORD_SYNC_SHORT,
        ),
    ] {
        compare(
            &buffer::finalize_recording(reference::RECORD_RAW, 1000, latency, mode).unwrap(),
            expected,
        );
    }
    for mode in [
        Dynamic { target: 0 },
        SyncFallback { target: 0 },
        Precision {
            target: 0,
            start: 0,
            destination_offset: 0,
        },
    ] {
        assert!(buffer::finalize_recording(reference::RECORD_RAW, 1000, 0.0, mode).is_err());
    }
    assert!(buffer::finalize_recording(&[], 1000, 0.0, Free).is_err());
    assert!(buffer::finalize_recording(&[[f32::NAN, 0.0]], 1000, 0.0, Free).is_err());
    assert!(buffer::finalize_recording(reference::RECORD_RAW, 0, 0.0, Free).is_err());
    assert!(buffer::finalize_recording(reference::RECORD_RAW, 1000, -1.0, Free).is_err());
    assert!(buffer::finalize_recording(reference::RECORD_RAW, 1000, f64::NAN, Free).is_err());
}

#[test]
fn shared_stereo_normalization_matches_javascript() {
    let mut audio = reference::NORMALIZE_INPUT.to_vec();
    assert!(buffer::normalize(&mut audio));
    compare(&audio, reference::NORMALIZE_OUTPUT);
    assert!(!buffer::normalize(&mut audio));
    assert!(!buffer::normalize(&mut [[0.0; 2]; 8]));
    assert!(!buffer::normalize(&mut [[0.00001; 2]; 8]));
}

#[test]
fn circular_overdub_punch_fades_clipping_and_negative_offsets_match_javascript() {
    let mut audio = reference::MIX_INPUT.to_vec();
    buffer::mix_circular(&mut audio, reference::MIX_TAKE, -3, 1000, 0.8).unwrap();
    compare(&audio, reference::MIX_OUTPUT);
}

#[test]
fn substitute_punch_fades_match_javascript() {
    let mut audio = reference::MIX_INPUT.to_vec();
    buffer::mix_circular(&mut audio, reference::MIX_TAKE, -3, 1000, 0.0).unwrap();
    compare(&audio, reference::SUBSTITUTE_OUTPUT);
}

#[test]
fn latency_rotation_and_wrap_fades_match_javascript() {
    let mut audio = reference::LATENCY_INPUT.to_vec();
    buffer::compensate_latency(&mut audio, 1000, 0.009).unwrap();
    compare(&audio, reference::LATENCY_OUTPUT);
    let original = audio.clone();
    buffer::compensate_latency(&mut audio, 1000, 1.0).unwrap();
    assert_eq!(audio, original);
}

#[test]
fn reverse_and_multiply_preserve_stereo_channels() {
    let mut audio = vec![[0.25, -0.5], [0.125, -0.25]];
    buffer::reverse(&mut audio);
    assert_eq!(audio, vec![[0.125, -0.25], [0.25, -0.5]]);
    assert_eq!(
        buffer::repeat(&audio, 2).unwrap(),
        vec![[0.125, -0.25], [0.25, -0.5], [0.125, -0.25], [0.25, -0.5]]
    );
    assert!(buffer::repeat(&audio, usize::MAX).is_err());
    assert!(buffer::repeat(&audio, 0).is_err());
}

#[test]
fn interactive_multiply_matches_original_dc_removal_and_splice_fade() {
    let result =
        buffer::append_capture(reference::APPEND_INPUT, reference::APPEND_TAKE, 1000).unwrap();
    compare(&result, reference::APPEND_OUTPUT);
    for (before, after) in reference::APPEND_INPUT
        .iter()
        .flatten()
        .zip(result.iter().flatten())
    {
        assert_eq!(before.to_bits(), after.to_bits());
    }
    assert!(buffer::append_capture(reference::APPEND_INPUT, &[], 48000).is_err());
    assert!(buffer::append_capture(reference::APPEND_INPUT, reference::APPEND_TAKE, 0).is_err());
    assert!(buffer::append_capture(reference::APPEND_INPUT, &[[f32::NAN, 0.0]], 48000).is_err());
    let negative_zero =
        buffer::append_capture(reference::APPEND_INPUT, &[[-0.0; 2]; 128], 48000).unwrap();
    assert!(negative_zero[reference::APPEND_INPUT.len()..]
        .iter()
        .flatten()
        .all(|sample| sample.to_bits() == (-0.0_f32).to_bits()));
}

#[test]
fn complete_punch_pipeline_matches_original_finalizer() {
    for (latency, overdub, substitute) in [
        (
            0.0,
            reference::PUNCH_OVERDUB_ZERO,
            reference::PUNCH_SUBSTITUTE_ZERO,
        ),
        (
            0.009,
            reference::PUNCH_OVERDUB_SHORT,
            reference::PUNCH_SUBSTITUTE_SHORT,
        ),
        (
            1.0,
            reference::PUNCH_OVERDUB_FULL,
            reference::PUNCH_SUBSTITUTE_FULL,
        ),
    ] {
        for (feedback, expected) in [(0.8, overdub), (0.0, substitute)] {
            let mut actual = reference::PUNCH_INPUT.to_vec();
            buffer::finalize_punch(
                &mut actual,
                reference::PUNCH_RAW,
                -3,
                1000,
                feedback,
                latency,
            )
            .unwrap();
            compare(&actual, expected);
        }
    }
    let mut audio = reference::PUNCH_INPUT.to_vec();
    for (feedback, latency) in [(0.8, -1.0), (0.8, f64::NAN), (1.1, 0.0)] {
        assert!(buffer::finalize_punch(
            &mut audio,
            reference::PUNCH_RAW,
            0,
            1000,
            feedback,
            latency
        )
        .is_err());
        assert_eq!(audio, reference::PUNCH_INPUT);
    }
}
