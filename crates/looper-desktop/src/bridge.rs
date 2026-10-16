use base64::{engine::general_purpose::STANDARD, Engine as _};
use looper_core::{buffer, Frame};
use serde::Deserialize;
use serde_json::{json, Value};
const MAX_FRAMES: usize = 1_000_000;
const MAX_WAV_BYTES: usize = 16_000_000;
#[derive(Debug, Default, Deserialize)]
pub struct Request {
    pub id: u64,
    pub operation: String,
    #[serde(default)]
    pub session: String,
    #[serde(default)]
    pub frames: Vec<Frame>,
    #[serde(default)]
    pub times: usize,
    #[serde(default)]
    pub pcm: Option<String>,
    #[serde(default)]
    pub take_pcm: Option<String>,
    #[serde(default)]
    pub sample_rate: u32,
    #[serde(default)]
    pub offset: i64,
    #[serde(default = "default_feedback")]
    pub feedback: f64,
    #[serde(default)]
    pub latency_seconds: f64,
    #[serde(default)]
    pub record_mode: String,
    #[serde(default)]
    pub target_frames: usize,
    #[serde(default)]
    pub start_sample: usize,
    #[serde(default)]
    pub destination_offset: usize,
    #[serde(default)]
    pub wav: Option<String>,
    #[serde(default)]
    pub channels: u16,
    #[serde(default)]
    pub positive_full_scale: bool,
    #[serde(default)]
    pub positive_reciprocal: bool,
    #[serde(default)]
    pub fade_seconds: f64,
    #[serde(default)]
    pub stretch_ratio: f64,
    #[serde(default)]
    pub pitch_semitones: f64,
}
fn default_feedback() -> f64 {
    0.80
}

fn decode_pcm(encoded: &str) -> Result<Vec<Frame>, String> {
    if encoded.len() > (MAX_FRAMES * 8).div_ceil(3) * 4 {
        return Err("PCM payload exceeds capacity.".into());
    }
    let bytes = STANDARD
        .decode(encoded)
        .map_err(|_| "Invalid PCM encoding.")?;
    if !bytes.len().is_multiple_of(8) || bytes.len() / 8 > MAX_FRAMES {
        return Err("Invalid stereo PCM length.".into());
    }
    let mut frames = Vec::with_capacity(bytes.len() / 8);
    for bytes in bytes.as_chunks::<8>().0 {
        let frame = [
            f32::from_le_bytes(bytes[..4].try_into().unwrap()),
            f32::from_le_bytes(bytes[4..].try_into().unwrap()),
        ];
        if frame.iter().any(|value| !value.is_finite()) {
            return Err("Nonfinite PCM rejected.".into());
        }
        frames.push(frame);
    }
    Ok(frames)
}

pub(crate) fn encode_pcm(frames: &[Frame]) -> String {
    let mut bytes = Vec::with_capacity(frames.len() * 8);
    for frame in frames {
        for sample in frame {
            bytes.extend_from_slice(&sample.to_le_bytes());
        }
    }
    STANDARD.encode(bytes)
}
/// Serialize bulk DSP responses off the UI thread and bound each script payload.
pub fn encode_replies(mut response: Value) -> Vec<String> {
    let mut replies = Vec::new();
    if let Some(pcm) = response["value"]
        .as_object_mut()
        .and_then(|value| value.remove("pcm"))
    {
        if let Some(encoded) = pcm.as_str().filter(|pcm| pcm.len() > 65536) {
            let total = encoded.len().div_ceil(32768);
            for (index, chunk) in encoded.as_bytes().chunks(32768).enumerate() {
                replies.push(
                    serde_json::to_string(&json!({"kind":"native_pcm_chunk",
                    "id":response["id"],"session":response["session"],"index":index,"total":total,
                    "pcm":std::str::from_utf8(chunk).expect("encoded PCM is ASCII")}))
                    .unwrap(),
                );
            }
            response["value"]["pcm_parts"] = json!(total);
        } else {
            response["value"]["pcm"] = pcm;
        }
    }
    replies.push(serde_json::to_string(&response).unwrap());
    replies
}

pub fn execute(mut request: Request) -> Value {
    let binary = request.pcm.is_some();
    let result = (|| -> Result<Value, String> {
        if let Some(pcm) = &request.pcm {
            request.frames = decode_pcm(pcm)?;
        }
        if request.frames.len() > MAX_FRAMES {
            return Err("Native edit capacity exceeded.".into());
        }
        if request
            .frames
            .iter()
            .flatten()
            .any(|sample| !sample.is_finite())
        {
            return Err("Nonfinite PCM rejected.".into());
        }
        match request.operation.as_str() {
            "capabilities" => {
                return Ok(
                    json!({"desktop":true,"ui_audio":"compatibility","pcm_format":"stereo-f32le-base64","stretch_engine":"Rubber Band 4.0.0 R3","native_operations":["normalize","reverse","slice","stretch","repeat","append_capture","finalize_punch","finalize_recording","mix","decode_wav","encode_wav"]}),
                )
            }
            "stretch" => {
                let output = looper_stretch::stretch(
                    &request.frames,
                    request.sample_rate,
                    request.channels,
                    request.stretch_ratio,
                    request.pitch_semitones,
                )
                .map_err(|e| format!("Rubber Band stretch: {e:?}"))?;
                return Ok(
                    json!({"pcm":encode_pcm(&output),"length":output.len(),"sample_rate":request.sample_rate,"channels":request.channels,"engine":"rubberband-r3"}),
                );
            }
            "normalize" => {
                let changed = buffer::normalize(&mut request.frames);
                return Ok(if binary {
                    json!({"pcm":encode_pcm(&request.frames),"changed":changed})
                } else {
                    json!({"frames":request.frames,"changed":changed})
                });
            }
            "slice" => {
                request.frames =
                    buffer::slice(&request.frames, request.start_sample, request.target_frames)
                        .map_err(|e| format!("Slice sample: {e:?}"))?;
            }
            "finalize_recording" => {
                if request.target_frames > MAX_FRAMES
                    || request
                        .frames
                        .len()
                        .checked_add(request.target_frames)
                        .is_none_or(|n| n > MAX_FRAMES)
                {
                    return Err("Recording finalization exceeds capacity.".into());
                }
                let mode = match request.record_mode.as_str() {
                    "free" => buffer::RecordingMode::Free,
                    "dynamic" => buffer::RecordingMode::Dynamic {
                        target: request.target_frames,
                    },
                    "precision" => buffer::RecordingMode::Precision {
                        target: request.target_frames,
                        start: request.start_sample,
                        destination_offset: request.destination_offset,
                    },
                    "sync_fallback" => buffer::RecordingMode::SyncFallback {
                        target: request.target_frames,
                    },
                    _ => return Err("Unknown recording mode.".into()),
                };
                request.frames = buffer::finalize_recording(
                    &request.frames,
                    request.sample_rate,
                    request.latency_seconds,
                    mode,
                )
                .map_err(|e| format!("Finalize recording: {e:?}"))?;
            }
            "decode_wav" => {
                let encoded = request.wav.as_deref().ok_or("Missing WAV data.")?;
                if encoded.len() > MAX_WAV_BYTES.div_ceil(3) * 4 {
                    return Err("WAV payload exceeds capacity.".into());
                }
                let bytes = STANDARD
                    .decode(encoded)
                    .map_err(|_| "Invalid WAV encoding.")?;
                if bytes.len() > MAX_WAV_BYTES {
                    return Err("WAV payload exceeds capacity.".into());
                }
                let scaling = if !request.positive_full_scale {
                    looper_files::IntegerScaling::Symmetric
                } else if request.positive_reciprocal {
                    looper_files::IntegerScaling::PositiveReciprocal
                } else {
                    looper_files::IntegerScaling::PositiveFullScale
                };
                let wave = looper_files::decode_with_integer_scaling(&bytes, MAX_FRAMES, scaling)
                    .map_err(|e| format!("Decode WAV: {e:?}"))?;
                if wave.frames.is_empty() {
                    return Err("Empty WAV take.".into());
                }
                return Ok(
                    json!({"pcm":encode_pcm(&wave.frames),"sample_rate":wave.sample_rate,"channels":wave.channels,"length":wave.frames.len()}),
                );
            }
            "encode_wav" => {
                buffer::export_fades(
                    &mut request.frames,
                    request.sample_rate,
                    request.fade_seconds,
                )
                .map_err(|e| format!("WAV export fades: {e:?}"))?;
                let bytes = looper_files::encode(
                    &request.frames,
                    request.sample_rate,
                    request.channels,
                    looper_files::Format::Pcm16,
                )
                .map_err(|e| format!("Encode WAV: {e:?}"))?;
                return Ok(json!({"wav":STANDARD.encode(bytes)}));
            }
            "reverse" => buffer::reverse(&mut request.frames),
            "append_capture" => {
                let captured =
                    decode_pcm(request.take_pcm.as_deref().ok_or("Missing captured PCM.")?)?;
                if request
                    .frames
                    .len()
                    .checked_add(captured.len())
                    .is_none_or(|len| len > MAX_FRAMES)
                {
                    return Err("Native append result exceeds capacity.".into());
                }
                request.frames =
                    buffer::append_capture(&request.frames, &captured, request.sample_rate)
                        .map_err(|e| format!("Append capture: {e:?}"))?;
            }
            "repeat" => {
                if request.times == 0
                    || request
                        .frames
                        .len()
                        .checked_mul(request.times)
                        .is_none_or(|len| len > MAX_FRAMES)
                {
                    return Err("Native edit result exceeds capacity.".into());
                }
                request.frames =
                    buffer::repeat(&request.frames, request.times).map_err(|e| format!("{e:?}"))?;
            }
            "mix" | "finalize_punch" => {
                let take = decode_pcm(request.take_pcm.as_deref().ok_or("Missing punch PCM.")?)?;
                if request.frames.len() + take.len() > MAX_FRAMES {
                    return Err("Combined PCM exceeds capacity.".into());
                }
                if request.operation == "finalize_punch" {
                    buffer::finalize_punch(
                        &mut request.frames,
                        &take,
                        request.offset,
                        request.sample_rate,
                        request.feedback,
                        request.latency_seconds,
                    )
                    .map_err(|error| format!("Native punch finalization: {error:?}"))?;
                } else {
                    buffer::mix_circular(
                        &mut request.frames,
                        &take,
                        request.offset,
                        request.sample_rate,
                        request.feedback,
                    )
                    .map_err(|error| format!("Native mix: {error:?}"))?;
                }
            }
            _ => return Err("Unknown native operation.".into()),
        }
        if binary {
            Ok(json!({"pcm":encode_pcm(&request.frames)}))
        } else {
            Ok(json!({"frames":request.frames}))
        }
    })();
    match result {
        Ok(value) => json!({"id":request.id,"session":request.session,"ok":true,"value":value}),
        Err(error) => json!({"id":request.id,"session":request.session,"ok":false,"error":error}),
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn native_stretch_bridge_returns_r3_metadata_and_rejects_invalid_jobs() {
        let request = || Request {
            operation: "stretch".into(),
            pcm: Some(encode_pcm(&vec![[0.0; 2]; 8192])),
            sample_rate: 48000,
            channels: 1,
            stretch_ratio: 1.5,
            ..Request::default()
        };
        let result = execute(request());
        assert_eq!(result["ok"], true);
        assert_eq!(result["value"]["engine"], "rubberband-r3");
        assert_eq!(result["value"]["length"], 12288);
        assert_eq!(result["value"]["channels"], 1);
        assert_eq!(
            decode_pcm(result["value"]["pcm"].as_str().unwrap()).unwrap(),
            vec![[0.0; 2]; 12288]
        );
        for (ratio, channels, pitch) in [(200.0, 1, 0.0), (1.5, 3, 0.0), (1.5, 1, 25.0)] {
            let mut invalid = request();
            invalid.stretch_ratio = ratio;
            invalid.channels = channels;
            invalid.pitch_semitones = pitch;
            assert_eq!(execute(invalid)["ok"], false);
        }
    }
    #[test]
    fn selection_bridge_bounds_ranges_and_reports_normalization_outcome() {
        let request = || Request {
            operation: "slice".into(),
            pcm: Some(encode_pcm(&[[0.25, -0.5], [0.5, -0.25], [0.125, -0.125]])),
            start_sample: 1,
            target_frames: 2,
            ..Request::default()
        };
        let result = execute(request());
        assert_eq!(result["ok"], true);
        assert_eq!(
            decode_pcm(result["value"]["pcm"].as_str().unwrap()).unwrap(),
            [[0.5, -0.25], [0.125, -0.125]]
        );
        for (start, length) in [(0, 0), (2, 2), (usize::MAX, 1)] {
            let mut invalid = request();
            invalid.start_sample = start;
            invalid.target_frames = length;
            assert_eq!(execute(invalid)["ok"], false);
        }
        for (frames, changed) in [
            (vec![[0.25, -0.5]], true),
            (vec![[0.0, 1.0]], false),
            (vec![[0.0, 0.0]], false),
        ] {
            let result = execute(Request {
                operation: "normalize".into(),
                frames,
                ..Request::default()
            });
            assert_eq!(result["value"]["changed"], changed);
        }
    }
    #[test]
    fn faded_wav_export_matches_original_bytes_and_validates_fades() {
        #[allow(dead_code, clippy::excessive_precision)]
        mod reference {
            include!("../../looper-core/tests/fixtures/original_dsp.rs");
        }
        for (input, seconds, expected) in [
            (
                reference::EXPORT_LONG_INPUT,
                0.01,
                reference::EXPORT_LONG_WAV,
            ),
            (
                reference::EXPORT_SHORT_INPUT,
                0.01,
                reference::EXPORT_SHORT_WAV,
            ),
            (
                reference::EXPORT_TINY_INPUT,
                0.01,
                reference::EXPORT_TINY_WAV,
            ),
            (
                reference::EXPORT_ZERO_INPUT,
                0.0,
                reference::EXPORT_ZERO_WAV,
            ),
            (
                reference::EXPORT_FULL_INPUT,
                1.0,
                reference::EXPORT_FULL_WAV,
            ),
        ] {
            let result = execute(Request {
                operation: "encode_wav".into(),
                pcm: Some(encode_pcm(input)),
                sample_rate: 1000,
                channels: 2,
                fade_seconds: seconds,
                ..Request::default()
            });
            assert_eq!(result["ok"], true);
            assert_eq!(
                STANDARD
                    .decode(result["value"]["wav"].as_str().unwrap())
                    .unwrap(),
                expected
            );
        }
        for seconds in [-1.0, f64::NAN, f64::INFINITY] {
            let result = execute(Request {
                operation: "encode_wav".into(),
                frames: vec![[0.25, -0.5]; 16],
                sample_rate: 48000,
                channels: 2,
                fade_seconds: seconds,
                ..Request::default()
            });
            assert_eq!(result["ok"], false);
        }
    }
    #[test]
    fn recording_bridge_checks_capacity_and_preserves_precision_extraction() {
        #[allow(dead_code, clippy::excessive_precision)]
        mod reference {
            include!("../../looper-core/tests/fixtures/original_dsp.rs");
        }
        let request = || Request {
            operation: "finalize_recording".into(),
            pcm: Some(encode_pcm(reference::RECORD_RAW)),
            record_mode: "precision".into(),
            sample_rate: 1000,
            latency_seconds: 0.009,
            target_frames: 90,
            start_sample: 22,
            ..Request::default()
        };
        let result = execute(request());
        assert_eq!(result["ok"], true);
        let actual = decode_pcm(result["value"]["pcm"].as_str().unwrap()).unwrap();
        assert_eq!(actual.len(), reference::RECORD_PRECISION_TAIL.len());
        for (actual, expected) in actual
            .iter()
            .flatten()
            .zip(reference::RECORD_PRECISION_TAIL.iter().flatten())
        {
            assert!((actual - expected).abs() < 0.000002);
        }
        let mut invalid = request();
        invalid.target_frames = MAX_FRAMES;
        assert_eq!(execute(invalid)["ok"], false);
        let mut invalid = request();
        invalid.target_frames = usize::MAX;
        assert_eq!(execute(invalid)["ok"], false);
        let mut invalid = request();
        invalid.target_frames = 0;
        assert_eq!(execute(invalid)["ok"], false);
        let mut invalid = request();
        invalid.record_mode = "unknown".into();
        assert_eq!(execute(invalid)["ok"], false);
        let mut invalid = request();
        invalid.latency_seconds = -1.0;
        assert_eq!(execute(invalid)["ok"], false);
        let mut invalid = request();
        invalid.pcm = Some(String::new());
        assert_eq!(execute(invalid)["ok"], false);
    }
    #[test]
    fn complete_punch_bridge_forwards_latency_and_bounds_combined_audio() {
        #[allow(dead_code, clippy::excessive_precision)]
        mod reference {
            include!("../../looper-core/tests/fixtures/original_dsp.rs");
        }
        let request = || Request {
            operation: "finalize_punch".into(),
            pcm: Some(encode_pcm(reference::PUNCH_INPUT)),
            take_pcm: Some(encode_pcm(reference::PUNCH_RAW)),
            offset: -3,
            sample_rate: 1000,
            feedback: 0.8,
            latency_seconds: 0.009,
            ..Request::default()
        };
        let result = execute(request());
        assert_eq!(result["ok"], true);
        let actual = decode_pcm(result["value"]["pcm"].as_str().unwrap()).unwrap();
        for (a, b) in actual
            .iter()
            .flatten()
            .zip(reference::PUNCH_OVERDUB_SHORT.iter().flatten())
        {
            assert!((a - b).abs() < 0.000002);
        }
        let mut invalid = request();
        invalid.latency_seconds = -1.0;
        assert_eq!(execute(invalid)["ok"], false);
        let mut invalid = request();
        invalid.take_pcm = Some(encode_pcm(&vec![[0.0; 2]; MAX_FRAMES]));
        assert_eq!(execute(invalid)["ok"], false);
    }
    #[test]
    fn native_wav_import_retains_layout_rate_and_float_headroom() {
        for channels in [1, 2] {
            for format in [
                looper_files::Format::Pcm16,
                looper_files::Format::Pcm24,
                looper_files::Format::Float32,
            ] {
                let bytes =
                    looper_files::encode(&[[0.125, -0.25], [1.5, -2.0]], 44100, channels, format)
                        .unwrap();
                let expected = looper_files::decode(&bytes).unwrap();
                let response = execute(Request {
                    operation: "decode_wav".into(),
                    wav: Some(STANDARD.encode(&bytes)),
                    ..Request::default()
                });
                assert_eq!(response["ok"], true);
                assert_eq!(response["value"]["channels"], channels);
                assert_eq!(response["value"]["sample_rate"], 44100);
                assert_eq!(response["value"]["length"], 2);
                assert_eq!(
                    decode_pcm(response["value"]["pcm"].as_str().unwrap()).unwrap(),
                    expected.frames
                );
            }
        }
        let response = execute(Request {
            operation: "decode_wav".into(),
            wav: Some("!".into()),
            ..Request::default()
        });
        assert_eq!(response["ok"], false);
    }
    #[test]
    fn extensible_import_worker_retains_pcm_layout_and_rejects_wrong_subtypes() {
        for channels in [1, 2] {
            for format in [
                looper_files::Format::Pcm16,
                looper_files::Format::Pcm24,
                looper_files::Format::Float32,
            ] {
                let plain =
                    looper_files::encode(&[[1.5, -0.25], [-0.0, -1.0]], 48000, channels, format)
                        .unwrap();
                let old_fmt_size = u32::from_le_bytes(plain[16..20].try_into().unwrap()) as usize;
                let bits = u16::from_le_bytes(plain[34..36].try_into().unwrap());
                let mut bytes = plain[..36].to_vec();
                bytes[16..20].copy_from_slice(&40_u32.to_le_bytes());
                bytes[20..22].copy_from_slice(&0xfffe_u16.to_le_bytes());
                bytes.extend_from_slice(&22_u16.to_le_bytes());
                bytes.extend_from_slice(&bits.to_le_bytes());
                bytes.extend_from_slice(&(if channels == 1 { 4_u32 } else { 3 }).to_le_bytes());
                bytes.extend_from_slice(&[
                    plain[20], 0, 0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71,
                ]);
                bytes.extend_from_slice(&plain[20 + old_fmt_size..]);
                let length = (bytes.len() - 8) as u32;
                bytes[4..8].copy_from_slice(&length.to_le_bytes());
                let response = execute(Request {
                    operation: "decode_wav".into(),
                    wav: Some(STANDARD.encode(&bytes)),
                    ..Request::default()
                });
                assert_eq!(response["ok"], true);
                assert_eq!(response["value"]["sample_rate"], 48000);
                assert_eq!(response["value"]["channels"], channels);
                assert_eq!(response["value"]["length"], 2);
                assert_eq!(
                    decode_pcm(response["value"]["pcm"].as_str().unwrap()).unwrap(),
                    looper_files::decode(&plain).unwrap().frames
                );
                bytes[59] = 0; // A matching format prefix is not a matching GUID.
                assert_eq!(
                    execute(Request {
                        operation: "decode_wav".into(),
                        wav: Some(STANDARD.encode(&bytes)),
                        ..Request::default()
                    })["ok"],
                    false
                );
            }
        }
    }
    #[test]
    fn native_loop_export_matches_original_wav16_bytes() {
        #[allow(dead_code, clippy::excessive_precision)]
        mod reference {
            include!("../../looper-core/tests/fixtures/original_dsp.rs");
        }
        let response = execute(Request {
            operation: "encode_wav".into(),
            pcm: Some(encode_pcm(&[[-1.0, 1.0], [0.125, -0.25], [0.0, 0.5]])),
            sample_rate: 48000,
            channels: 2,
            ..Request::default()
        });
        assert_eq!(response["ok"], true);
        assert_eq!(
            STANDARD
                .decode(response["value"]["wav"].as_str().unwrap())
                .unwrap(),
            reference::WAV16_REFERENCE
        );
        let response = execute(Request {
            operation: "encode_wav".into(),
            channels: 3,
            ..Request::default()
        });
        assert_eq!(response["ok"], false);
    }
    #[test]
    fn ui_edits_use_native_dsp_and_errors_keep_the_request_id() {
        let response = execute(Request {
            id: 7,
            operation: "normalize".into(),
            frames: vec![[0.25, -0.5]],
            times: 0,
            ..Request::default()
        });
        assert_eq!(response["value"]["frames"], json!([[0.5, -1.0]]));
        let response = execute(Request {
            id: 8,
            operation: "unknown".into(),
            frames: vec![],
            times: 0,
            ..Request::default()
        });
        assert_eq!(response["id"], 8);
        assert_eq!(response["ok"], false);
    }
    #[test]
    fn binary_pcm_preserves_bits_and_rejects_malformed_data() {
        let frames = [[f32::MIN_POSITIVE, -0.0], [0.12345679, -1.5]];
        let encoded = encode_pcm(&frames);
        let decoded = decode_pcm(&encoded).unwrap();
        for (a, b) in decoded.iter().flatten().zip(frames.iter().flatten()) {
            assert_eq!(a.to_bits(), b.to_bits());
        }
        assert!(decode_pcm("!").is_err());
        assert!(decode_pcm(&STANDARD.encode([0_u8; 7])).is_err());
        assert!(decode_pcm(&encode_pcm(&[[f32::NAN, 0.0]])).is_err());
    }
    #[test]
    fn binary_native_mix_matches_original_reference_including_punch_and_wrap() {
        #[allow(dead_code, clippy::excessive_precision)]
        mod reference {
            include!("../../looper-core/tests/fixtures/original_dsp.rs");
        }
        for (feedback, expected) in [
            (0.8, reference::MIX_OUTPUT),
            (0.0, reference::SUBSTITUTE_OUTPUT),
        ] {
            let response = execute(Request {
                id: 42,
                operation: "mix".into(),
                pcm: Some(encode_pcm(reference::MIX_INPUT)),
                take_pcm: Some(encode_pcm(reference::MIX_TAKE)),
                sample_rate: 1000,
                offset: -3,
                feedback,
                ..Request::default()
            });
            assert_eq!(response["ok"], true);
            let actual = decode_pcm(response["value"]["pcm"].as_str().unwrap()).unwrap();
            for (actual, expected) in actual.iter().flatten().zip(expected.iter().flatten()) {
                assert!((actual - expected).abs() < 0.000002);
            }
        }
    }
}
#[test]
fn captured_append_preserves_original_prefix_and_rejects_invalid_payloads() {
    let original = [[1.5, -0.0], [-0.25, 0.5]];
    let request = || Request {
        operation: "append_capture".into(),
        pcm: Some(encode_pcm(&original)),
        take_pcm: Some(encode_pcm(&[[0.25, -0.5]; 160])),
        sample_rate: 48000,
        ..Request::default()
    };
    let response = execute(request());
    assert_eq!(response["ok"], true);
    let result = decode_pcm(response["value"]["pcm"].as_str().unwrap()).unwrap();
    assert_eq!(result.len(), 162);
    for (before, after) in original.iter().flatten().zip(result.iter().flatten()) {
        assert_eq!(before.to_bits(), after.to_bits());
    }
    assert!(result[2..].iter().flatten().all(|sample| *sample == 0.0));
    let mut invalid = request();
    invalid.take_pcm = None;
    assert_eq!(execute(invalid)["ok"], false);
    let mut invalid = request();
    invalid.sample_rate = 0;
    assert_eq!(execute(invalid)["ok"], false);
    let mut invalid = request();
    invalid.take_pcm = Some(encode_pcm(&vec![[0.0; 2]; MAX_FRAMES]));
    assert_eq!(execute(invalid)["ok"], false);
}

#[test]
fn bulk_dsp_replies_preserve_identity_and_bound_script_payloads() {
    let pcm = "ABCD".repeat(25000);
    let encoded = encode_replies(
        json!({"id":7,"session":"document","ok":true,"value":{"pcm":pcm,"length":12500}}),
    );
    assert_eq!(encoded.len(), 5);
    let mut restored = String::new();
    for (index, text) in encoded[..4].iter().enumerate() {
        assert!(text.len() < 33000);
        let part: Value = serde_json::from_str(text).unwrap();
        assert_eq!(part["index"], index);
        assert_eq!(part["total"], 4);
        assert_eq!(part["id"], 7);
        assert_eq!(part["session"], "document");
        restored.push_str(part["pcm"].as_str().unwrap());
    }
    assert_eq!(restored, pcm);
    let end: Value = serde_json::from_str(encoded.last().unwrap()).unwrap();
    assert_eq!(end["value"]["pcm_parts"], 4);
    assert!(end["value"]["pcm"].is_null());
    let small = json!({"id":8,"ok":true,"value":{"pcm":"AAAA"}});
    assert_eq!(
        serde_json::from_str::<Value>(&encode_replies(small.clone())[0]).unwrap(),
        small
    );
}
