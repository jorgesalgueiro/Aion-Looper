//! Dedicated input control thread; offline DSP work cannot delay capture reads.
use looper_audio::{
    capture::{self, InputReader, InputWriter},
    device, input,
};
use serde::Deserialize;
use serde_json::{json, Value};
use std::time::{Duration, Instant};

const INPUT_LEASE: Duration = Duration::from_secs(30);
const MAX_TOKEN: u64 = (1_u64 << 53) - 1;

#[derive(Deserialize, Default)]
pub struct Request {
    pub id: u64,
    pub session: String,
    pub operation: String,
    #[serde(default)]
    pub sample_rate: u32,
    #[serde(default)]
    pub host: Option<String>,
    #[serde(default)]
    pub device: Option<String>,
    #[serde(default)]
    pub token: u64,
    #[serde(default)]
    pub synthetic: bool,
}
enum Source {
    Device(input::LiveInput),
    Synthetic {
        reader: InputReader,
        writer: InputWriter,
        started: Instant,
        frames: u64,
        rate: u32,
    },
}
impl Source {
    fn reader(&mut self) -> &mut InputReader {
        match self {
            Self::Device(input) => &mut input.reader,
            Self::Synthetic { reader, .. } => reader,
        }
    }
    fn advance(&mut self) {
        if let Self::Synthetic {
            writer,
            started,
            frames,
            rate,
            ..
        } = self
        {
            let target = (started.elapsed().as_secs_f64() * f64::from(*rate)) as u64;
            // Bounded synthetic work, available only in the hidden integration test.
            let count = target.saturating_sub(*frames).min(16384);
            for _ in 0..count {
                let sample = ((*frames as f64 * 440.0 * std::f64::consts::TAU / f64::from(*rate))
                    .sin()
                    * 0.125) as f32;
                writer.capture(&[sample, sample * 0.5], 2);
                *frames += 1;
            }
        }
    }
}
struct Handle {
    token: u64,
    session: String,
    source: Source,
    next: u64,
    touched: Instant,
}
pub struct Service {
    handles: Vec<Handle>,
    sequence: u64,
    allow_synthetic: bool,
}
impl Service {
    pub fn new(allow_synthetic: bool) -> Self {
        Self {
            handles: Vec::new(),
            sequence: 0,
            allow_synthetic,
        }
    }
    /// Release device streams if their document stops consuming input.
    pub fn expire(&mut self) {
        self.handles
            .retain(|handle| handle.touched.elapsed() < INPUT_LEASE);
    }
    pub fn execute(&mut self, request: Request) -> Value {
        self.expire();
        let result = self.perform(&request);
        match result {
            Ok(value) => json!({"id":request.id,"session":request.session,"ok":true,"value":value}),
            Err(error) => {
                json!({"id":request.id,"session":request.session,"ok":false,"error":error})
            }
        }
    }
    fn perform(&mut self, request: &Request) -> Result<Value, String> {
        if request.session.is_empty() || request.session.len() > 128 {
            return Err("Invalid input session.".into());
        }
        match request.operation.as_str() {
            "input_devices" => {
                if request.synthetic {
                    if !self.allow_synthetic {
                        return Err("Synthetic input is available only in self-test mode.".into());
                    }
                    return Ok(
                        json!({"devices":[{"host":"SelfTest","device":"synthetic","label":"Synthetic native input"}]}),
                    );
                }
                let mut devices = Vec::new();
                for host in device::available_hosts() {
                    if let Ok(entries) = device::devices(Some(&host)) {
                        for entry in entries
                            .into_iter()
                            .filter(|entry| !entry.input_ranges.is_empty())
                        {
                            devices.push(json!({"host":host,"device":entry.id,"label":entry.name}));
                        }
                    }
                }
                Ok(json!({"devices":devices}))
            }
            "input_open" => {
                if request.session.is_empty() {
                    return Err("Missing input session.".into());
                }
                if !(8000..=192000).contains(&request.sample_rate) {
                    return Err("Unsupported input sample rate.".into());
                }
                if request.synthetic && !self.allow_synthetic {
                    return Err("Synthetic input is available only in self-test mode.".into());
                }
                // A new document retires the preceding document's device streams.
                self.handles
                    .retain(|handle| handle.session == request.session);
                if self.handles.len() >= 4 {
                    return Err("Native input supports at most four simultaneous sources.".into());
                }
                let token = self
                    .sequence
                    .checked_add(1)
                    .filter(|token| *token <= MAX_TOKEN)
                    .ok_or("Input token exhausted.")?;
                let (source, info) = if request.synthetic {
                    let (writer, reader) = capture::connect((request.sample_rate / 2) as usize)?;
                    (
                        Source::Synthetic {
                            reader,
                            writer,
                            started: Instant::now(),
                            frames: 0,
                            rate: request.sample_rate,
                        },
                        json!({"host":"SelfTest","device":"synthetic","label":"Synthetic native input","channels":2,"sample_rate":request.sample_rate}),
                    )
                } else {
                    let input = input::open(
                        request.sample_rate,
                        &device::Options {
                            host: request.host.clone(),
                            device: request.device.clone(),
                            buffer_frames: None,
                        },
                        (request.sample_rate / 2) as usize,
                    )?;
                    let info = json!({"host":input.info.host,"device":input.info.device_id,"label":input.info.device,"channels":input.info.config.channels.min(2),"device_channels":input.info.config.channels,"sample_rate":input.info.config.sample_rate});
                    input.start()?;
                    (Source::Device(input), info)
                };
                self.sequence = token;
                self.handles.push(Handle {
                    token,
                    session: request.session.clone(),
                    source,
                    next: 0,
                    touched: Instant::now(),
                });
                Ok(json!({"token":token,"info":info}))
            }
            "input_read" => {
                let index = self
                    .handles
                    .iter()
                    .position(|handle| {
                        handle.token == request.token && handle.session == request.session
                    })
                    .ok_or("Native input is no longer active.")?;
                self.handles[index].touched = Instant::now();
                let result = read(&mut self.handles[index]);
                if result.is_err() {
                    self.handles.remove(index);
                }
                result
            }
            "input_close" => {
                self.handles.retain(|handle| {
                    !(handle.token == request.token && handle.session == request.session)
                });
                Ok(json!({}))
            }
            _ => Err("Unknown native input operation.".into()),
        }
    }
}
fn read(handle: &mut Handle) -> Result<Value, String> {
    handle.source.advance();
    let reader = handle.source.reader();
    let status = reader.status();
    if status.dropped_frames > 0
        || status.stream_errors > 0
        || status.nonfinite_samples > 0
        || status.malformed_blocks > 0
    {
        return Err(format!("Native input interrupted: {status:?}"));
    }
    let position = handle.next;
    let mut frames = Vec::with_capacity(4096);
    for _ in 0..4096 {
        let Some(frame) = reader.pop() else {
            break;
        };
        if frame.position != handle.next {
            return Err("Native input frame discontinuity.".into());
        }
        handle.next += 1;
        frames.push(frame.samples);
    }
    Ok(
        json!({"pcm":crate::bridge::encode_pcm(&frames),"position":position,"length":frames.len(),"frames":status.frames,"max_callback_frames":status.max_callback_frames}),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    fn request(operation: &str, session: &str, token: u64) -> Request {
        Request {
            operation: operation.into(),
            session: session.into(),
            token,
            sample_rate: 48000,
            synthetic: true,
            ..Request::default()
        }
    }
    #[test]
    fn abandoned_inputs_expire_and_only_matching_reads_refresh_the_lease() {
        let mut service = Service::new(true);
        let first = service.execute(request("input_open", "test", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        let second = service.execute(request("input_open", "test", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        for handle in &mut service.handles {
            handle.touched = Instant::now() - Duration::from_secs(29);
        }
        let first_before = service.handles[0].touched;
        assert_eq!(
            service.execute(request("input_read", "other", first))["ok"],
            false
        );
        assert_eq!(service.handles[0].touched, first_before);
        assert_eq!(
            service.execute(request("input_read", "test", first))["ok"],
            true
        );
        assert!(service.handles[0].touched > first_before);
        // Advance the second source beyond its lease, without sleeping or hardware.
        service.handles[1].touched = Instant::now() - INPUT_LEASE - Duration::from_secs(1);
        service.expire();
        assert_eq!(service.handles.len(), 1);
        assert_eq!(service.handles[0].token, first);
        assert_eq!(
            service.execute(request("input_read", "test", second))["ok"],
            false
        );
        service.handles[0].touched = Instant::now() - INPUT_LEASE - Duration::from_secs(1);
        assert_eq!(
            service.execute(request("input_read", "test", first))["ok"],
            false
        );
        assert!(service.handles.is_empty());
        assert_eq!(
            service.execute(request("input_open", "test", 0))["ok"],
            true
        );
    }

    #[test]
    fn invalid_input_identity_cannot_retire_an_existing_capture_or_wrap_tokens() {
        let mut service = Service::new(true);
        assert_eq!(
            service.execute(request("input_open", "test", 0))["ok"],
            true
        );
        assert_eq!(
            service.execute(request("input_open", &"x".repeat(129), 0))["ok"],
            false
        );
        assert_eq!(service.handles.len(), 1);
        service.sequence = MAX_TOKEN;
        assert_eq!(
            service.execute(request("input_open", "test", 0))["ok"],
            false
        );
        assert_eq!(service.handles.len(), 1);
        assert_eq!(service.sequence, MAX_TOKEN);
    }
    #[test]
    fn input_sessions_are_bounded_and_retired_without_opening_hardware() {
        let mut service = Service::new(true);
        for _ in 0..4 {
            assert_eq!(
                service.execute(request("input_open", "first", 0))["ok"],
                true
            );
        }
        assert_eq!(
            service.execute(request("input_open", "first", 0))["ok"],
            false
        );
        let reply = service.execute(request("input_open", "second", 0));
        assert_eq!(reply["ok"], true);
        assert_eq!(service.handles.len(), 1);
        let token = reply["value"]["token"].as_u64().unwrap();
        assert_eq!(
            service.execute(request("input_read", "first", token))["ok"],
            false
        );
        service.execute(request("input_close", "first", token));
        assert_eq!(service.handles.len(), 1);
        service.execute(request("input_close", "second", token));
        assert!(service.handles.is_empty());
    }
    #[test]
    fn synthetic_capture_cannot_be_requested_in_a_normal_application() {
        let mut service = Service::new(false);
        assert_eq!(
            service.execute(request("input_open", "test", 0))["ok"],
            false
        );
        let mut bad_rate = request("input_open", "test", 0);
        bad_rate.sample_rate = 0;
        assert_eq!(service.execute(bad_rate)["ok"], false);
        assert_eq!(service.execute(request("input_open", "", 0))["ok"], false);
        assert!(service.handles.is_empty());
    }
    #[test]
    fn reads_preserve_frame_positions_and_close_faulted_input() {
        let mut service = Service::new(true);
        let (mut writer, reader) = capture::connect(4).unwrap();
        writer.capture(&[0.125_f32, -0.25, 0.5, -0.75], 2);
        service.handles.push(Handle {
            token: 7,
            session: "test".into(),
            next: 0,
            touched: Instant::now(),
            source: Source::Synthetic {
                writer,
                reader,
                started: Instant::now() + std::time::Duration::from_secs(60),
                frames: 2,
                rate: 8000,
            },
        });
        let response = service.execute(request("input_read", "test", 7));
        assert_eq!(response["ok"], true);
        assert_eq!(response["value"]["position"], 0);
        assert!(response["value"]["length"].as_u64().unwrap() >= 2);
        let position = service.handles[0].next;
        let response = service.execute(request("input_read", "test", 7));
        assert_eq!(response["ok"], true);
        assert_eq!(response["value"]["position"], position);
        if let Source::Synthetic { writer, .. } = &mut service.handles[0].source {
            writer.metrics().stream_error();
        }
        assert_eq!(
            service.execute(request("input_read", "test", 7))["ok"],
            false
        );
        assert!(service.handles.is_empty());
    }
}
