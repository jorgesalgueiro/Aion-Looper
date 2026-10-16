//! Native audio session owner. Only controls/status cross the webview boundary.
use looper_audio::{
    capture, device,
    duplex::{self, Control},
};
use looper_core::{Command, Engine, MAX_TRACKS};
use serde::Deserialize;
use serde_json::{json, Value};
use std::time::{Duration, Instant};
#[derive(Default, Deserialize)]
pub struct Request {
    pub id: u64,
    pub session: String,
    pub operation: String,
    #[serde(default)]
    pub token: u64,
    #[serde(default)]
    pub host: Option<String>,
    #[serde(default)]
    pub device: Option<String>,
    #[serde(default)]
    pub sample_rate: u32,
    #[serde(default)]
    pub tracks: usize,
    #[serde(default)]
    pub seconds: u32,
    #[serde(default)]
    pub gain: f32,
    #[serde(default)]
    pub value: f32,
    #[serde(default)]
    pub track: usize,
    #[serde(default)]
    pub command: String,
    #[serde(default)]
    pub synthetic: bool,
}
pub enum Message {
    Request(Request),
    Shutdown,
}
struct Synthetic {
    writer: capture::InputWriter,
    renderer: duplex::Renderer,
    controller: duplex::Controller,
    started: Instant,
    frames: u64,
    rate: u32,
}
impl Synthetic {
    fn block(&mut self) {
        let mut input = [0.0_f32; 256];
        for (i, frame) in input.as_chunks_mut::<2>().0.iter_mut().enumerate() {
            let v = (((self.frames + i as u64) as f64) * 440.0 * std::f64::consts::TAU
                / f64::from(self.rate))
            .sin() as f32
                * 0.125;
            frame[0] = v;
            frame[1] = v * 0.5;
        }
        self.writer.capture(&input, 2);
        let mut output = [0.0_f32; 256];
        self.renderer.render(&mut output, 2);
        self.frames += 128;
    }
}
enum Source {
    Live(Box<duplex::Live>),
    Synthetic(Box<Synthetic>),
}
impl Source {
    fn controller(&mut self) -> &mut duplex::Controller {
        match self {
            Self::Live(live) => &mut live.controller,
            Self::Synthetic(source) => &mut source.controller,
        }
    }
    fn pump(&mut self) {
        if let Self::Synthetic(source) = self {
            let target = (source.started.elapsed().as_secs_f64() * f64::from(source.rate)) as u64;
            for _ in 0..32 {
                if source.frames + 128 > target {
                    break;
                }
                source.block();
            }
        }
    }
}
struct Handle {
    token: u64,
    session: String,
    source: Source,
    info: Value,
    tracks: usize,
}
pub struct Service {
    handle: Option<Handle>,
    sequence: u64,
    allow_synthetic: bool,
}
impl Service {
    pub fn new(allow_synthetic: bool) -> Self {
        Self {
            handle: None,
            sequence: 0,
            allow_synthetic,
        }
    }
    pub fn wait_duration(&self) -> Duration {
        if matches!(
            self.handle.as_ref().map(|h| &h.source),
            Some(Source::Synthetic(_))
        ) {
            Duration::from_millis(2)
        } else {
            Duration::from_secs(1)
        }
    }
    pub fn pump(&mut self) {
        if let Some(h) = &mut self.handle {
            h.source.pump();
        }
    }
    pub fn shutdown(&mut self) {
        self.handle = None;
    }
    pub fn execute(&mut self, request: Request) -> Value {
        let result = self.perform(&request);
        match result {
            Ok(value) => json!({"id":request.id,"session":request.session,"ok":true,"value":value}),
            Err(error) => {
                json!({"id":request.id,"session":request.session,"ok":false,"error":error})
            }
        }
    }
    fn perform(&mut self, r: &Request) -> Result<Value, String> {
        if r.session.is_empty() || r.session.len() > 128 {
            return Err("Invalid native audio session.".into());
        }
        if r.operation == "engine_devices" {
            let devices: Vec<Value> = {
                #[cfg(all(target_os = "windows", feature = "asio"))]
                {
                    device::asio_input_names().into_iter().map(|name|json!({"host":"ASIO","device":format!("asio:{name}"),"label":name})).collect()
                }
                #[cfg(not(all(target_os = "windows", feature = "asio")))]
                {
                    Vec::new()
                }
            };
            return Ok(json!({"devices":devices}));
        }
        if r.operation == "engine_open" {
            if r.synthetic && !self.allow_synthetic {
                return Err("Synthetic native audio is test-only.".into());
            }
            if !(8000..=192000).contains(&r.sample_rate)
                || !(1..=MAX_TRACKS).contains(&r.tracks)
                || !(1..=120).contains(&r.seconds)
                || !r.gain.is_finite()
                || !(0.0..=4.0).contains(&r.gain)
            {
                return Err("Invalid native audio configuration.".into());
            }
            let frames = (r.sample_rate as usize)
                .checked_mul(r.seconds as usize)
                .ok_or("Native audio capacity overflow.")?;
            if frames
                .checked_mul(r.tracks)
                .is_none_or(|total| total > 32_000_000)
            {
                return Err(
                    "Native audio storage exceeds 256 MB. Reduce loop count or duration.".into(),
                );
            }
            if self.handle.as_ref().is_some_and(|h| h.session == r.session) {
                return Err("Close native audio before changing devices.".into());
            }
            let token = self
                .sequence
                .checked_add(1)
                .filter(|n| *n < (1_u64 << 53))
                .ok_or("Native audio token exhausted.")?;
            self.handle = None;
            let engine =
                Engine::with_capacities(std::array::from_fn(
                    |i| if i < r.tracks { frames } else { 0 },
                ))
                .map_err(|e| format!("Prepare native loops: {e:?}"))?;
            let (source, info) = if r.synthetic {
                let (writer, reader) = capture::connect((r.sample_rate / 2) as usize)?;
                let (controller, renderer) =
                    duplex::connect(engine, reader, writer.metrics(), r.gain)?;
                (
                    Source::Synthetic(Box::new(Synthetic {
                        writer,
                        renderer,
                        controller,
                        started: Instant::now(),
                        frames: 0,
                        rate: r.sample_rate,
                    })),
                    json!({"host":"SelfTest","device":"Synthetic duplex","sample_rate":r.sample_rate,"input_channels":2,"output_channels":2}),
                )
            } else {
                let live = duplex::open(
                    engine,
                    r.sample_rate,
                    &device::Options {
                        host: r.host.clone(),
                        device: r.device.clone(),
                        buffer_frames: None,
                    },
                    r.gain,
                )?;
                let info = json!({"host":live.info.host,"device":live.info.device,"sample_rate":live.info.sample_rate,"input_channels":live.info.input_channels,"output_channels":live.info.output_channels});
                live.start()?;
                (Source::Live(Box::new(live)), info)
            };
            self.sequence = token;
            self.handle = Some(Handle {
                token,
                session: r.session.clone(),
                source,
                info: info.clone(),
                tracks: r.tracks,
            });
            return Ok(
                json!({"token":token,"info":info,"capacity_frames":frames,"tracks":r.tracks,"transport":"native-duplex","web_audio":false}),
            );
        }
        let h = self
            .handle
            .as_mut()
            .filter(|h| h.token == r.token && h.session == r.session)
            .ok_or("Native audio session is closed or replaced.")?;
        match r.operation.as_str() {
            "engine_close" => {
                self.handle = None;
                Ok(json!({}))
            }
            "engine_status" => {
                let s = h.source.controller().status();
                Ok(
                    json!({"transport":"native-duplex","web_audio":false,"info":h.info,"frames":s.frames,"underruns":s.underruns,
                "discontinuities":s.discontinuities,"command_errors":s.command_errors,"stream_errors":s.stream_errors,"malformed_blocks":s.malformed,
                "input_peak":s.input_peak,"output_peak":s.output_peak,"callback_frames":s.callback_frames,
                "capture":{"frames":s.capture.frames,"dropped_frames":s.capture.dropped_frames,"errors":s.capture.stream_errors,"nonfinite_samples":s.capture.nonfinite_samples,"malformed_blocks":s.capture.malformed_blocks},
                "tracks":s.tracks[..h.tracks].iter().map(|t|json!({"state":format!("{:?}",t.state).to_lowercase(),"length":t.length_frames,"position":t.position_frames,"muted":t.muted})).collect::<Vec<_>>()}),
                )
            }
            "engine_control" => {
                let control = match r.command.as_str() {
                    "master" => Control::Master(r.value),
                    "input_gain" => Control::InputGain(r.value),
                    "monitor" if r.value == 0.0 || r.value == 1.0 => {
                        Control::Monitor(r.value == 1.0)
                    }
                    command if r.track < h.tracks => match command {
                        "record" => Control::Track {
                            track: r.track,
                            command: Command::Record,
                        },
                        "finish" => Control::FinishStopped { track: r.track },
                        "play" => Control::Track {
                            track: r.track,
                            command: Command::Play,
                        },
                        "stop" => Control::Track {
                            track: r.track,
                            command: Command::Stop,
                        },
                        "clear" => Control::Track {
                            track: r.track,
                            command: Command::Clear,
                        },
                        "overdub" => Control::Track {
                            track: r.track,
                            command: Command::Overdub,
                        },
                        "substitute" => Control::Track {
                            track: r.track,
                            command: Command::Substitute,
                        },
                        "mute" if r.value == 0.0 || r.value == 1.0 => Control::Track {
                            track: r.track,
                            command: Command::Mute(r.value == 1.0),
                        },
                        "gain" => Control::Track {
                            track: r.track,
                            command: Command::Gain(r.value),
                        },
                        "feedback" => Control::Track {
                            track: r.track,
                            command: Command::Feedback(r.value),
                        },
                        _ => return Err("Unknown native audio control.".into()),
                    },
                    _ => return Err("Invalid native track or control.".into()),
                };
                h.source.controller().send(control)?;
                Ok(json!({"queued":true}))
            }
            _ => Err("Unknown native audio request.".into()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn request(operation: &str, token: u64) -> Request {
        Request {
            operation: operation.into(),
            session: "doc".into(),
            token,
            sample_rate: 48000,
            tracks: 2,
            seconds: 1,
            synthetic: true,
            ..Default::default()
        }
    }
    fn blocks(service: &mut Service, count: usize) {
        if let Source::Synthetic(source) = &mut service.handle.as_mut().unwrap().source {
            for _ in 0..count {
                source.block();
            }
        } else {
            panic!("expected synthetic")
        }
    }
    #[test]
    fn native_session_controls_use_only_metadata_and_reject_stale_owners() {
        let mut s = Service::new(true);
        let opened = s.execute(request("engine_open", 0));
        let token = opened["value"]["token"].as_u64().unwrap();
        assert_eq!(opened["value"]["web_audio"], false);
        let mut record = request("engine_control", token);
        record.command = "record".into();
        assert_eq!(s.execute(record)["ok"], true);
        blocks(&mut s, 3);
        let status = s.execute(request("engine_status", token));
        assert_eq!(status["value"]["tracks"][0]["state"], "recording");
        assert_eq!(status["value"]["tracks"][0]["length"], 384);
        assert!(status["value"]["pcm"].is_null());
        let mut stale = request("engine_close", token);
        stale.session = "old".into();
        assert_eq!(s.execute(stale)["ok"], false);
        assert!(s.handle.is_some());
        let mut finish = request("engine_control", token);
        finish.command = "finish".into();
        s.execute(finish);
        blocks(&mut s, 1);
        assert_eq!(
            s.execute(request("engine_status", token))["value"]["tracks"][0]["state"],
            "stopped"
        );
        s.execute(request("engine_close", token));
        assert!(s.handle.is_none());
        assert_eq!(s.execute(request("engine_status", token))["ok"], false);
    }
    #[test]
    fn synthetic_audio_and_storage_are_bounded_and_window_shutdown_releases() {
        assert_eq!(
            Service::new(false).execute(request("engine_open", 0))["ok"],
            false
        );
        let mut s = Service::new(true);
        let mut large = request("engine_open", 0);
        large.tracks = 20;
        large.seconds = 120;
        large.sample_rate = 192000;
        assert_eq!(s.execute(large)["ok"], false);
        assert!(s.handle.is_none());
        let token = s.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        assert_eq!(s.execute(request("engine_open", 0))["ok"], false);
        let mut stale = request("engine_status", token);
        stale.session = "old".into();
        s.execute(stale);
        assert!(s.handle.is_some());
        s.pump();
        assert!(
            s.handle.is_some(),
            "UI inactivity must not close the native engine"
        );
        s.shutdown();
        assert!(s.handle.is_none());
        let next = s.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        assert_ne!(next, token);
    }
}
