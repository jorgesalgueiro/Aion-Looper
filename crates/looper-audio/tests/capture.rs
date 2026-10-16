use looper_audio::capture::connect;

#[test]
fn stereo_float_input_preserves_headroom_bits_and_device_clock() {
    let (mut writer, mut reader) = connect(4).unwrap();
    writer.capture(&[1.5_f32, -2.0, -0.0, f32::MIN_POSITIVE], 2);
    let first = reader.pop().unwrap();
    assert_eq!(first.position, 0);
    assert_eq!(first.samples, [1.5, -2.0]);
    let second = reader.pop().unwrap();
    assert_eq!(second.position, 1);
    assert_eq!(second.samples[0].to_bits(), (-0.0_f32).to_bits());
    assert_eq!(second.samples[1].to_bits(), f32::MIN_POSITIVE.to_bits());
    assert_eq!(reader.status().frames, 2);
    assert_eq!(reader.status().max_callback_frames, 2);
    assert!(reader.pop().is_none());
}

#[test]
fn mono_integer_and_double_input_convert_without_channel_crosstalk() {
    let (mut writer, mut reader) = connect(16).unwrap();
    writer.capture(&[i16::MIN, 0, i16::MAX], 1);
    writer.capture(&[i32::MIN, i32::MAX], 2);
    writer.capture(&[0_u16, 32768, u16::MAX, 32768], 2);
    writer.capture(&[0.125_f64, -0.25], 2);
    assert_eq!(reader.pop().unwrap().samples, [-1.0; 2]);
    assert_eq!(reader.pop().unwrap().samples, [0.0; 2]);
    assert_eq!(reader.pop().unwrap().samples, [32767.0 / 32768.0; 2]);
    assert_eq!(reader.pop().unwrap().samples, [-1.0, 1.0]);
    assert_eq!(reader.pop().unwrap().samples, [-1.0, 0.0]);
    assert_eq!(reader.pop().unwrap().samples, [32767.0 / 32768.0, 0.0]);
    assert_eq!(reader.pop().unwrap().samples, [0.125, -0.25]);
}

#[test]
fn full_ring_preserves_older_audio_and_exposes_the_exact_gap() {
    let (mut writer, mut reader) = connect(2).unwrap();
    writer.capture(&[0.1_f32, 0.2, 0.3, 0.4], 1);
    assert_eq!(reader.pop().unwrap().position, 0);
    assert_eq!(reader.pop().unwrap().position, 1);
    writer.capture(&[0.5_f32], 1);
    let next = reader.pop().unwrap();
    assert_eq!(next.position, 4);
    assert_eq!(next.samples, [0.5; 2]);
    assert_eq!(reader.status().dropped_frames, 2);
    assert_eq!(reader.status().frames, 5);
}

#[test]
fn invalid_blocks_do_not_advance_clock_and_invalid_samples_are_counted() {
    assert!(connect(0).is_err());
    assert!(connect(1_000_001).is_err());
    let (mut writer, mut reader) = connect(4).unwrap();
    writer.capture(&[1.0_f32], 0);
    writer.capture(&[1.0_f32], 2);
    writer.capture(&[1.0_f32; 129], 129);
    writer.capture(&[f32::NAN, f32::INFINITY, -1.25, 0.25], 2);
    assert_eq!(reader.pop().unwrap().position, 0);
    assert_eq!(reader.pop().unwrap().samples, [-1.25, 0.25]);
    writer.metrics().stream_error();
    let status = reader.status();
    assert_eq!(status.frames, 2);
    assert_eq!(status.malformed_blocks, 3);
    assert_eq!(status.nonfinite_samples, 2);
    assert_eq!(status.stream_errors, 1);
}

#[test]
fn callback_partitioning_does_not_change_samples_or_clock() {
    let source = [0.1_f32, -0.2, 0.3, -0.4, 0.5, -0.6];
    let (mut writer_a, mut reader_a) = connect(4).unwrap();
    let (mut writer_b, mut reader_b) = connect(4).unwrap();
    writer_a.capture(&source, 2);
    writer_b.capture(&source[..2], 2);
    writer_b.capture(&source[2..], 2);
    while let Some(frame) = reader_a.pop() {
        assert_eq!(Some(frame), reader_b.pop());
    }
    assert!(reader_b.pop().is_none());
}

#[test]
fn wider_interfaces_capture_the_first_pair_without_changing_the_frame_clock() {
    let (mut writer, mut reader) = connect(4).unwrap();
    writer.capture(&[0.125_f32, -0.25, 0.75, 1.0, 0.5, -0.75, 0.1, 0.2], 4);
    let first = reader.pop().unwrap();
    assert_eq!(first.position, 0);
    assert_eq!(first.samples, [0.125, -0.25]);
    let second = reader.pop().unwrap();
    assert_eq!(second.position, 1);
    assert_eq!(second.samples, [0.5, -0.75]);
    assert_eq!(reader.status().frames, 2);
}

#[test]
fn packed_24_bit_device_capture_retains_small_samples_and_full_scale() {
    use cpal::Sample;
    let (mut writer, mut reader) = connect(4).unwrap();
    writer.capture(
        &[
            cpal::I24::from_sample(-1.0_f32),
            cpal::I24::from_sample(1.0_f32 / 8388608.0),
        ],
        2,
    );
    assert_eq!(reader.pop().unwrap().samples, [-1.0, 1.0 / 8388608.0]);
}

#[test]
fn input_frames_cross_threads_without_sharing_callback_state() {
    let (mut writer, mut reader) = connect(128).unwrap();
    std::thread::spawn(move || writer.capture(&[0.125_f32; 100], 1))
        .join()
        .unwrap();
    for position in 0..100 {
        let frame = reader.pop().unwrap();
        assert_eq!(frame.position, position);
        assert_eq!(frame.samples, [0.125; 2]);
    }
    assert_eq!(reader.status().dropped_frames, 0);
}
