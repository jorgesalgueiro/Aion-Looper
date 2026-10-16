use cpal::{SampleFormat, SupportedBufferSize, SupportedStreamConfigRange};
use looper_audio::device::{choose_input, choose_output};

fn range(channels: u16, format: SampleFormat) -> SupportedStreamConfigRange {
    SupportedStreamConfigRange::new(
        channels,
        44100,
        96000,
        SupportedBufferSize::Range { min: 64, max: 512 },
        format,
    )
}

#[test]
fn output_selection_prefers_stereo_float_and_preserves_requested_values() {
    let (config, format) = choose_output(
        [
            range(1, SampleFormat::F32),
            range(2, SampleFormat::I16),
            range(2, SampleFormat::F32),
        ],
        48000,
        Some(128),
    )
    .unwrap();
    assert_eq!(config.channels, 2);
    assert_eq!(format, SampleFormat::F32);
    assert_eq!(config.sample_rate, 48000);
    assert_eq!(config.buffer_size, cpal::BufferSize::Fixed(128));
}

#[test]
fn unsupported_requests_do_not_silently_change_rate_or_buffer() {
    for (rate, size) in [
        (192000, Some(128)),
        (48000, Some(32)),
        (48000, Some(1024)),
        (0, None),
        (48000, Some(0)),
    ] {
        assert!(choose_output([range(2, SampleFormat::F32)], rate, size).is_err());
    }
    assert!(choose_output([range(8, SampleFormat::F32)], 48000, Some(128)).is_err());
}

#[test]
fn mono_integer_fallback_is_supported_without_forcing_a_buffer() {
    let (config, format) = choose_output([range(1, SampleFormat::I16)], 44100, None).unwrap();
    assert_eq!(config.channels, 1);
    assert_eq!(format, SampleFormat::I16);
    assert_eq!(config.buffer_size, cpal::BufferSize::Default);
}

#[test]
fn input_selection_preserves_exact_rate_buffer_and_supported_layout() {
    let (config, format) = choose_input(
        [range(1, SampleFormat::I16), range(2, SampleFormat::F32)],
        48000,
        Some(128),
    )
    .unwrap();
    assert_eq!(config.channels, 2);
    assert_eq!(config.sample_rate, 48000);
    assert_eq!(config.buffer_size, cpal::BufferSize::Fixed(128));
    assert_eq!(format, SampleFormat::F32);
    let (mono, _) = choose_input([range(1, SampleFormat::I16)], 44100, None).unwrap();
    assert_eq!(mono.channels, 1);
    assert!(choose_input([range(2, SampleFormat::F32)], 48000, Some(1024)).is_err());
    assert!(choose_input([range(2, SampleFormat::F32)], 192000, None).is_err());
    let (wide, _) = choose_input([range(8, SampleFormat::F32)], 48000, None).unwrap();
    assert_eq!(wide.channels, 8);
    let (_, format) = choose_input([range(8, SampleFormat::I24)], 48000, None).unwrap();
    assert_eq!(format, SampleFormat::I24);
    assert!(choose_input([range(129, SampleFormat::F32)], 48000, None).is_err());
}
