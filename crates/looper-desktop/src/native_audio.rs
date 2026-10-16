//! Native audio session owner. Only controls/status cross the webview boundary.
use looper_audio::{
    capture, device,
    duplex::{self, Control},
};
use looper_core::{checkpoint::Checkpoint, Command, MAX_TRACKS};
use looper_files::project::{self, Project};
use serde::Deserialize;
use serde_json::{json, Value};
use std::time::{Duration, Instant};
use std::{
    fs,
    io::{BufReader, BufWriter, Write},
    path::PathBuf,
};
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
    #[serde(default)]
    pub project: String,
    #[serde(default)]
    pub names: Vec<String>,
}
pub enum Message {
    Request(Box<Request>),
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
    capacity: usize,
    master: f32,
    input_gain: f32,
}
pub struct Service {
    handle: Option<Handle>,
    sequence: u64,
    allow_synthetic: bool,
    project_dir: PathBuf,
    pending: Option<Project>,
}
impl Service {
    pub fn new(allow_synthetic: bool) -> Self {
        Self {
            handle: None,
            sequence: 0,
            allow_synthetic,
            project_dir: std::env::current_exe()
                .unwrap_or_else(|_| PathBuf::from("."))
                .parent()
                .unwrap_or(std::path::Path::new("."))
                .join(if allow_synthetic {
                    "NativeTestProjects"
                } else {
                    "AionProjects"
                }),
            pending: None,
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
        if r.operation == "engine_projects" {
            let mut projects = Vec::new();
            if self.project_dir.is_dir() {
                for entry in fs::read_dir(&self.project_dir).map_err(|e| e.to_string())? {
                    let entry = entry.map_err(|e| e.to_string())?;
                    if entry.file_type().map_err(|e| e.to_string())?.is_file()
                        && entry.path().extension().is_some_and(|e| e == "aion")
                    {
                        if let Some(name) = entry.file_name().to_str() {
                            projects.push(name.to_owned());
                        }
                    }
                }
            }
            projects.sort();
            projects.reverse();
            return Ok(json!({"projects":projects,"directory":self.project_dir}));
        }
        if r.operation == "engine_project_load" {
            if self.handle.is_some() {
                return Err(
                    "Save your current project and close audio before loading another project."
                        .into(),
                );
            }
            let path = self.project_dir.join(safe_name(&r.project)?);
            if path.extension().is_none_or(|ext| ext != "aion") {
                return Err("Choose a native .aion project.".into());
            }
            let file = fs::File::open(path).map_err(|e| format!("Open project: {e}"))?;
            if file.metadata().map_err(|e| e.to_string())?.len() > 256_016_384 {
                return Err("Project is too large.".into());
            }
            let project =
                project::decode(BufReader::new(file)).map_err(|e| format!("Load project: {e}"))?;
            if project.takes.len() > 10 {
                return Err(
                    "The native desktop interface currently supports ten loop tracks.".into(),
                );
            }
            let value = json!({"sample_rate":project.rate,"tracks":project.takes.len(),"names":project.names,"master":project.master,"input_gain":project.input_gain});
            self.pending = Some(project);
            return Ok(value);
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
            if self.pending.is_none()
                && frames
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
            let (rate, tracks, capacity, gain, input_gain, names) = if let Some(p) = &self.pending {
                (
                    p.rate,
                    p.takes.len(),
                    p.capacity,
                    p.master,
                    p.input_gain,
                    p.names.clone(),
                )
            } else {
                (
                    r.sample_rate,
                    r.tracks,
                    frames,
                    r.gain,
                    1.0,
                    vec![String::new(); r.tracks],
                )
            };
            let (mut engine, checkpoint) =
                Checkpoint::prepare(std::array::from_fn(
                    |i| if i < tracks { capacity } else { 0 },
                ))
                .map_err(|e| format!("Prepare native loops: {e:?}"))?;
            if let Some(project) = &self.pending {
                for (i, take) in project.takes.iter().enumerate() {
                    if !take.audio.is_empty() {
                        engine
                            .load_stopped(i, &take.audio)
                            .map_err(|e| format!("Restore take: {e:?}"))?;
                    }
                    for command in [
                        Command::Gain(take.gain),
                        Command::Feedback(take.feedback),
                        Command::Mute(take.muted),
                    ] {
                        engine
                            .command(i, command)
                            .map_err(|e| format!("Restore setting: {e:?}"))?;
                    }
                }
            }
            checkpoint.publish(&engine);
            let (source, info) = if r.synthetic {
                let (writer, reader) = capture::connect((rate / 2) as usize)?;
                let (controller, renderer) = duplex::connect_checkpoint(
                    engine,
                    reader,
                    writer.metrics(),
                    gain,
                    Some(checkpoint),
                )?;
                (
                    Source::Synthetic(Box::new(Synthetic {
                        writer,
                        renderer,
                        controller,
                        started: Instant::now(),
                        frames: 0,
                        rate,
                    })),
                    json!({"host":"SelfTest","device":"Synthetic duplex","sample_rate":rate,"input_channels":2,"output_channels":2}),
                )
            } else {
                let live = duplex::open(
                    engine,
                    Some(checkpoint),
                    rate,
                    &device::Options {
                        host: r.host.clone(),
                        device: r.device.clone(),
                        buffer_frames: None,
                    },
                    gain,
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
                tracks,
                capacity,
                master: gain,
                input_gain,
            });
            self.handle
                .as_mut()
                .unwrap()
                .source
                .controller()
                .send(Control::InputGain(input_gain))?;
            let settings: Vec<Value> = self
                .pending
                .as_ref()
                .map(|p| {
                    p.takes
                        .iter()
                        .map(|t| json!({"gain":t.gain,"feedback":t.feedback}))
                        .collect()
                })
                .unwrap_or_else(|| {
                    (0..tracks)
                        .map(|_| json!({"gain":1.0,"feedback":0.8}))
                        .collect()
                });
            self.pending = None;
            return Ok(
                json!({"token":token,"info":info,"capacity_frames":capacity,"tracks":tracks,"names":names,"master":gain,"input_gain":input_gain,"settings":settings,"restored_stopped":true,"transport":"native-duplex","web_audio":false}),
            );
        }
        let h = self
            .handle
            .as_mut()
            .filter(|h| h.token == r.token && h.session == r.session)
            .ok_or("Native audio session is closed or replaced.")?;
        match r.operation.as_str() {
            "engine_project_export" => {
                if r.track >= h.tracks {
                    return Err("Invalid native track.".into());
                }
                let takes = h.source.controller().snapshot(project::MAX_FRAMES)?;
                if takes[r.track].audio.is_empty() {
                    return Err("Record a take before exporting.".into());
                }
                let bytes = looper_files::encode(
                    &takes[r.track].audio,
                    h.info["sample_rate"].as_u64().unwrap() as u32,
                    2,
                    looper_files::Format::Float32,
                )
                .map_err(|e| format!("Export WAV: {e:?}"))?;
                let name = write_version(
                    &self.project_dir,
                    &format!("Loop-{}", r.track + 1),
                    "wav",
                    |writer| writer.write_all(&bytes).map_err(|e| e.to_string()),
                )?;
                Ok(json!({"file":name,"directory":self.project_dir,"exported":true}))
            }
            "engine_project_save" => {
                let base = safe_name(&r.project)?;
                if base.len() > 80 {
                    return Err("Project names must fit 80 UTF-8 bytes.".into());
                }
                if r.names.len() != h.tracks
                    || r.names.iter().any(|n| n.len() > 256 || n.contains('\0'))
                {
                    return Err("Invalid loop names.".into());
                }
                let mut takes = h.source.controller().snapshot(project::MAX_FRAMES)?;
                takes.truncate(h.tracks);
                let project = Project {
                    rate: h.info["sample_rate"].as_u64().unwrap() as u32,
                    capacity: h.capacity,
                    master: h.master,
                    input_gain: h.input_gain,
                    names: r.names.clone(),
                    takes,
                };
                let name = write_version(&self.project_dir, base, "aion", |writer| {
                    project::encode(&project, writer).map_err(|e| e.to_string())
                })?;
                Ok(json!({"project":name,"directory":self.project_dir,"saved":true}))
            }
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
                match control {
                    Control::Master(value) => h.master = value,
                    Control::InputGain(value) => h.input_gain = value,
                    _ => {}
                }
                Ok(json!({"queued":true}))
            }
            _ => Err("Unknown native audio request.".into()),
        }
    }
}

fn write_version(
    dir: &std::path::Path,
    base: &str,
    extension: &str,
    write: impl FnOnce(&mut BufWriter<fs::File>) -> Result<(), String>,
) -> Result<String, String> {
    fs::create_dir_all(dir).map_err(|e| format!("Create project folder: {e}"))?;
    let unique = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_err(|e| e.to_string())?
        .as_nanos();
    let name = format!("{base}-{unique}.{extension}");
    let path = dir.join(&name);
    let temp = dir.join(format!("{name}.partial"));
    let mut created = false;
    let result = (|| -> Result<(), String> {
        let file = fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temp)
            .map_err(|e| e.to_string())?;
        created = true;
        let mut writer = BufWriter::new(file);
        write(&mut writer)?;
        writer.flush().map_err(|e| e.to_string())?;
        writer.get_ref().sync_all().map_err(|e| e.to_string())?;
        drop(writer);
        if path.exists() {
            return Err("Filename already exists; retry saving.".into());
        }
        fs::rename(&temp, &path).map_err(|e| e.to_string())?;
        Ok(())
    })();
    if result.is_err() && created {
        let _ = fs::remove_file(&temp);
    }
    result?;
    Ok(name)
}
fn safe_name(name: &str) -> Result<&str, String> {
    if name.is_empty()
        || name.len() > 240
        || name.starts_with('.')
        || name.ends_with('.')
        || name.contains("..")
        || !name
            .chars()
            .all(|c| c.is_alphanumeric() || " _-.".contains(c))
    {
        return Err(
            "Use a project name with letters, numbers, spaces, underscores or hyphens.".into(),
        );
    }
    Ok(name)
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
    fn projects_preserve_pcm_and_settings_without_replacing_existing_takes_on_error() {
        let mut service = Service::new(true);
        service.project_dir = std::env::temp_dir().join(format!(
            "aion-project-test-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        let token = service.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        let mut r = request("engine_control", token);
        r.command = "record".into();
        service.execute(r);
        blocks(&mut service, 4);
        let mut save = request("engine_project_save", token);
        save.project = "Voz".into();
        save.names = vec!["Voice".into(), "Bass".into()];
        assert_eq!(
            service.execute(save)["ok"],
            false,
            "Active capture must not produce a partial project"
        );
        for (command, value) in [
            ("finish", 0.0),
            ("play", 0.0),
            ("gain", 0.5),
            ("feedback", 0.6),
            ("mute", 1.0),
            ("master", 0.0),
            ("input_gain", 1.25),
        ] {
            let mut r = request("engine_control", token);
            r.command = command.into();
            r.value = value;
            assert_eq!(service.execute(r)["ok"], true);
        }
        blocks(&mut service, 2);
        let original = service
            .handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .snapshot(32000000)
            .unwrap();
        let exported = service.execute(request("engine_project_export", token));
        assert_eq!(exported["ok"], true, "{exported}");
        let wave_path = service
            .project_dir
            .join(exported["value"]["file"].as_str().unwrap());
        let wave = looper_files::decode(&fs::read(&wave_path).unwrap()).unwrap();
        assert_eq!(wave.frames, original[0].audio);
        fs::remove_file(wave_path).unwrap();
        let mut saved_names = Vec::new();
        for _ in 0..2 {
            let mut save = request("engine_project_save", token);
            save.project = "Voz".into();
            save.names = vec!["Voice".into(), "Bass".into()];
            let saved = service.execute(save);
            assert_eq!(saved["ok"], true, "{saved}");
            saved_names.push(saved["value"]["project"].as_str().unwrap().to_owned());
        }
        assert_ne!(saved_names[0], saved_names[1]);
        let mut load = request("engine_project_load", 0);
        load.project = saved_names[0].clone();
        assert_eq!(
            service.execute(load)["ok"],
            false,
            "Loading must not discard an open session"
        );
        assert_eq!(
            service
                .handle
                .as_mut()
                .unwrap()
                .source
                .controller()
                .snapshot(32000000)
                .unwrap(),
            original
        );
        service.execute(request("engine_close", token));
        let mut bad = request("engine_project_load", 0);
        bad.project = "../outside.aion".into();
        assert_eq!(service.execute(bad)["ok"], false);
        let mut load = request("engine_project_load", 0);
        load.project = saved_names[0].clone();
        assert_eq!(service.execute(load)["ok"], true);
        let opened = service.execute(request("engine_open", 0));
        assert_eq!(opened["ok"], true, "{opened}");
        assert_eq!(opened["value"]["names"][0], "Voice");
        assert_eq!(opened["value"]["input_gain"], 1.25);
        blocks(&mut service, 1);
        let restored = service
            .handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .snapshot(32000000)
            .unwrap();
        assert_eq!(restored[0].audio, original[0].audio);
        assert_eq!(restored[0].gain, 0.5);
        assert_eq!(restored[0].feedback, 0.6);
        assert!(restored[0].muted);
        assert!(!restored[0].playing);
        service.shutdown();
        for name in saved_names {
            fs::remove_file(service.project_dir.join(name)).unwrap();
        }
        fs::remove_dir(&service.project_dir).unwrap();
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
