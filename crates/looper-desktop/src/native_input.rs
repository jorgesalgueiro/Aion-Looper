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
    #[serde(default)]
    pub streaming: bool,
    #[serde(default)]
    pub sequence: u64,
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
struct Delivery {
    sequence: u64,
    waiting: bool,
    sent: Instant,
}
struct Handle {
    token: u64,
    session: String,
    source: Source,
    next: u64,
    touched: Instant,
    delivery: Option<Delivery>,
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
    pub fn wait_duration(&self) -> Duration {
        if self.handles.iter().any(|handle| handle.delivery.is_some()) {
            Duration::from_millis(2)
        } else {
            Duration::from_secs(1)
        }
    }
    /// At most one unacknowledged PCM event per source; at most four sources.
    pub fn pump(&mut self) -> Vec<Value> {
        self.expire();
        let mut events = Vec::new();
        let mut index = 0;
        while index < self.handles.len() {
            let handle = &mut self.handles[index];
            let Some(delivery) = &handle.delivery else {
                index += 1;
                continue;
            };
            let waiting = delivery.waiting;
            let heartbeat = delivery.sent.elapsed() >= Duration::from_secs(1);
            handle.source.advance();
            let status = handle.source.reader().status();
            let interrupted = status.dropped_frames > 0
                || status.stream_errors > 0
                || status.nonfinite_samples > 0
                || status.malformed_blocks > 0;
            if !interrupted && (waiting || (status.frames <= handle.next && !heartbeat)) {
                index += 1;
                continue;
            }
            let delivery = handle.delivery.as_mut().unwrap();
            let sequence = delivery
                .sequence
                .checked_add(1)
                .filter(|sequence| *sequence <= MAX_TOKEN);
            let result = if sequence.is_some() {
                read(handle)
            } else {
                Err("Input sequence exhausted.".into())
            };
            let sequence = sequence.unwrap_or(MAX_TOKEN);
            match result {
                Ok(packet) => {
                    let delivery = handle.delivery.as_mut().unwrap();
                    delivery.sequence = sequence;
                    delivery.waiting = true;
                    delivery.sent = Instant::now();
                    events.push(json!({"kind":"native_input","token":handle.token,"session":handle.session,"sequence":sequence,"packet":packet}));
                    index += 1;
                }
                Err(error) => {
                    events.push(json!({"kind":"native_input","token":handle.token,"session":handle.session,"sequence":sequence,"error":error}));
                    self.handles.remove(index);
                }
            }
        }
        events
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
                    #[cfg(all(target_os = "windows", feature = "asio"))]
                    if host.eq_ignore_ascii_case("ASIO") {
                        for name in device::asio_input_names() {
                            devices.push(
                                json!({"host":host,"device":format!("asio:{name}"),"label":name}),
                            );
                        }
                        continue;
                    }
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
                    delivery: request.streaming.then(|| Delivery {
                        sequence: 0,
                        waiting: true,
                        sent: Instant::now(),
                    }),
                });
                Ok(json!({"token":token,"info":info,"streaming":request.streaming}))
            }
            "input_read" => {
                let index = self
                    .handles
                    .iter()
                    .position(|handle| {
                        handle.token == request.token && handle.session == request.session
                    })
                    .ok_or("Native input is no longer active.")?;
                if self.handles[index].delivery.is_some() {
                    return Err("Streaming input requires acknowledgements, not reads.".into());
                }
                self.handles[index].touched = Instant::now();
                let result = read(&mut self.handles[index]);
                if result.is_err() {
                    self.handles.remove(index);
                }
                result
            }
            "input_ack" => {
                let handle = self
                    .handles
                    .iter_mut()
                    .find(|handle| {
                        handle.token == request.token && handle.session == request.session
                    })
                    .ok_or("Native input is no longer active.")?;
                let delivery = handle.delivery.as_mut().ok_or("Input is not streaming.")?;
                if !delivery.waiting || delivery.sequence != request.sequence {
                    return Err("Invalid input acknowledgement.".into());
                }
                delivery.waiting = false;
                handle.touched = Instant::now();
                Ok(json!({}))
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
            delivery: None,
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
    fn open_stream(service: &mut Service) -> u64 {
        let mut open = request("input_open", "push", 0);
        open.streaming = true;
        let reply = service.execute(open);
        assert_eq!(reply["value"]["streaming"], true);
        let token = reply["value"]["token"].as_u64().unwrap();
        if let Source::Synthetic { started, .. } = &mut service.handles.last_mut().unwrap().source {
            *started = Instant::now() + Duration::from_secs(60);
        }
        token
    }
    fn acknowledge(service: &mut Service, token: u64, sequence: u64) -> Value {
        let mut ack = request("input_ack", "push", token);
        ack.sequence = sequence;
        service.execute(ack)
    }
    fn feed(service: &mut Service, token: u64, samples: &[f32]) {
        let handle = service
            .handles
            .iter_mut()
            .find(|handle| handle.token == token)
            .unwrap();
        if let Source::Synthetic { writer, .. } = &mut handle.source {
            writer.capture(samples, 2);
        }
    }
    #[test]
    fn pushed_input_waits_for_matching_ack_and_never_consumes_pcm_twice() {
        let mut service = Service::new(true);
        let token = open_stream(&mut service);
        feed(&mut service, token, &[1.5, -0.0, 0.25, -0.5]);
        assert!(service.pump().is_empty());
        assert_eq!(
            service.execute(request("input_ack", "other", token))["ok"],
            false
        );
        assert_eq!(acknowledge(&mut service, token, 0)["ok"], true);
        let events = service.pump();
        assert_eq!(events.len(), 1);
        assert_eq!(events[0]["sequence"], 1);
        assert_eq!(events[0]["packet"]["position"], 0);
        assert_eq!(
            events[0]["packet"]["pcm"],
            crate::bridge::encode_pcm(&[[1.5, -0.0], [0.25, -0.5]])
        );
        feed(&mut service, token, &[0.75, -0.125]);
        assert!(service.pump().is_empty());
        assert_eq!(
            service.execute(request("input_read", "push", token))["ok"],
            false
        );
        assert_eq!(acknowledge(&mut service, token, 2)["ok"], false);
        assert!(service.pump().is_empty());
        assert_eq!(acknowledge(&mut service, token, 1)["ok"], true);
        let events = service.pump();
        assert_eq!(events.len(), 1);
        assert_eq!(events[0]["sequence"], 2);
        assert_eq!(events[0]["packet"]["position"], 2);
        assert_eq!(events[0]["packet"]["length"], 1);
        assert_eq!(acknowledge(&mut service, token, 1)["ok"], false);
        if let Source::Synthetic { writer, .. } = &mut service.handles[0].source {
            writer.metrics().stream_error();
        }
        let events = service.pump();
        assert_eq!(events.len(), 1);
        assert!(events[0]["error"].as_str().unwrap().contains("interrupted"));
        assert!(service.handles.is_empty());
        assert!(service.pump().is_empty());
    }
    #[test]
    fn four_streams_have_independent_bounded_delivery_and_idle_heartbeats() {
        let mut service = Service::new(true);
        let tokens: Vec<_> = (0..4).map(|_| open_stream(&mut service)).collect();
        for &token in &tokens {
            assert_eq!(acknowledge(&mut service, token, 0)["ok"], true);
            feed(&mut service, token, &[0.25, -0.5]);
        }
        let events = service.pump();
        assert_eq!(events.len(), 4);
        for (event, token) in events.iter().zip(&tokens) {
            assert_eq!(event["token"], *token);
        }
        assert!(service.pump().is_empty());
        for &token in &tokens {
            assert_eq!(acknowledge(&mut service, token, 1)["ok"], true);
        }
        for handle in &mut service.handles {
            handle.delivery.as_mut().unwrap().sent = Instant::now() - Duration::from_secs(2);
        }
        let events = service.pump();
        assert_eq!(events.len(), 4);
        assert!(events
            .iter()
            .all(|event| event["packet"]["length"] == 0 && event["sequence"] == 2));
        assert!(service.pump().is_empty());
        for handle in &mut service.handles {
            handle.touched = Instant::now() - INPUT_LEASE - Duration::from_secs(1);
        }
        assert!(service.pump().is_empty());
        assert!(service.handles.is_empty());
    }
    #[test]
    fn pushed_packets_are_bounded_and_sequence_exhaustion_closes_capture() {
        let mut service = Service::new(true);
        let token = open_stream(&mut service);
        assert_eq!(acknowledge(&mut service, token, 0)["ok"], true);
        feed(&mut service, token, &vec![0.125; 10000]);
        let events = service.pump();
        assert_eq!(events[0]["packet"]["length"], 4096);
        assert_eq!(acknowledge(&mut service, token, 1)["ok"], true);
        let events = service.pump();
        assert_eq!(events[0]["packet"]["position"], 4096);
        assert_eq!(events[0]["packet"]["length"], 904);
        assert_eq!(acknowledge(&mut service, token, 2)["ok"], true);
        service.handles[0].delivery.as_mut().unwrap().sequence = MAX_TOKEN;
        feed(&mut service, token, &[0.25, -0.5]);
        let events = service.pump();
        assert!(events[0]["error"]
            .as_str()
            .unwrap()
            .contains("sequence exhausted"));
        assert!(service.handles.is_empty());
    }
}
