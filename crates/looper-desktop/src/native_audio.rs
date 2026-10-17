//! Native audio session owner. Only controls/status cross the webview boundary.
use looper_audio::{
    capture, device,
    duplex::{self, Control},
};
use looper_core::{checkpoint::Checkpoint, Command, MAX_TRACKS};
use looper_files::project::{self, Project};
use serde::Deserialize;
use serde_json::{json, Value};
use std::collections::VecDeque;
use std::time::{Duration, Instant};

// PCM history lives on the worker only: at most 64 MB across all tracks/stacks.
const HISTORY_FRAMES: usize = 8_000_000;
#[derive(Default)]
struct EditHistory {
    undo: VecDeque<(usize, Vec<looper_core::Frame>, bool)>,
    redo: VecDeque<(usize, Vec<looper_core::Frame>, bool)>,
}
impl EditHistory {
    fn clear_track(&mut self, track: usize) {
        self.undo.retain(|(t, _, _)| *t != track);
        self.redo.retain(|(t, _, _)| *t != track);
    }
    fn target(&self, redo: bool) -> Option<usize> {
        if redo {
            self.redo
                .iter()
                .rev()
                .find(|entry| entry.2)
                .map(|entry| entry.0)
        } else {
            self.undo.back().map(|entry| entry.0)
        }
    }
    fn new_edit(&mut self, track: usize) {
        self.redo.retain(|entry| entry.0 != track);
        // Match the reference: a new edit clears global redo while other loops'
        // per-loop redo buffers remain available through their own controls.
        for entry in &mut self.redo {
            entry.2 = false;
        }
    }
    fn count(&self, track: usize, redo: bool) -> usize {
        (if redo { &self.redo } else { &self.undo })
            .iter()
            .filter(|(t, _, _)| *t == track)
            .count()
    }
    fn push(&mut self, track: usize, audio: Vec<looper_core::Frame>, redo: bool) {
        let stack = if redo { &mut self.redo } else { &mut self.undo };
        stack.push_back((track, audio, true));
        while stack.iter().filter(|(t, _, _)| *t == track).count() > 5 {
            let index = stack.iter().position(|(t, _, _)| *t == track).unwrap();
            stack.remove(index);
        }
        while self
            .undo
            .iter()
            .chain(&self.redo)
            .map(|(_, a, _)| a.len())
            .sum::<usize>()
            > HISTORY_FRAMES
        {
            if self.undo.pop_front().is_none() {
                self.redo.pop_front();
            }
        }
    }
}
use std::{
    fs,
    io::{BufReader, BufWriter, Read, Write},
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
    #[serde(default)]
    pub path: String,
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
    history: EditHistory,
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
                history: EditHistory::default(),
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
            "engine_edit" => {
                if !matches!(
                    r.command.as_str(),
                    "normalize" | "undo" | "redo" | "global_undo" | "global_redo"
                ) {
                    return Err("Invalid native edit.".into());
                }
                let redo = matches!(r.command.as_str(), "redo" | "global_redo");
                let track = if r.command.starts_with("global_") {
                    h.history
                        .target(redo)
                        .ok_or("No global native edit or punch pass to restore.")?
                } else {
                    r.track
                };
                if track >= h.tracks {
                    return Err("Invalid native track.".into());
                }
                if h.source.controller().status().tracks[track].state != looper_core::State::Stopped
                {
                    return Err(
                        "Stop the target loop before editing; other loops can keep playing.".into(),
                    );
                }
                let mut takes = h.source.controller().snapshot(project::MAX_FRAMES)?;
                let current = std::mem::take(&mut takes[track].audio);
                drop(takes);
                if current.is_empty() || current.len() > HISTORY_FRAMES {
                    return Err("Take is empty or exceeds the native editing history limit (8 million frames).".into());
                }

                let mut normalized = Vec::new();
                let index;
                let audio = if r.command == "normalize" {
                    normalized.clone_from(&current);
                    if !looper_core::buffer::normalize(&mut normalized) {
                        return Ok(json!({"changed":false}));
                    }
                    index = None;
                    &normalized
                } else {
                    let stack = if redo {
                        &h.history.redo
                    } else {
                        &h.history.undo
                    };
                    let i = stack
                        .iter()
                        .rposition(|(t, _, _)| *t == track)
                        .ok_or("No native edit or punch pass to restore.")?;
                    index = Some(i);
                    &stack[i].1
                };
                h.source.controller().begin_edit(track, audio)?;
                finish_replacement(&mut h.source)?;
                if let Some(i) = index {
                    let stack = if redo {
                        &mut h.history.redo
                    } else {
                        &mut h.history.undo
                    };
                    stack.remove(i);
                    h.history.push(track, current, !redo);
                } else {
                    h.history.new_edit(track);
                    h.history.push(track, current, false);
                }
                Ok(json!({"changed":true,"track":track}))
            }
            "engine_project_import" => {
                if r.track >= h.tracks {
                    return Err("Invalid native track.".into());
                }
                if !matches!(
                    h.source.controller().status().tracks[r.track].state,
                    looper_core::State::Empty | looper_core::State::Stopped
                ) {
                    return Err(
                        "Stop the target loop before importing. Other loops can keep playing."
                            .into(),
                    );
                }
                let path = std::path::Path::new(&r.path);
                if r.path.len() > 4096
                    || !path.is_absolute()
                    || path
                        .extension()
                        .is_none_or(|e| !e.eq_ignore_ascii_case("wav"))
                {
                    return Err("Choose an absolute path to a WAV file.".into());
                }
                const MAX_BYTES: u64 = 256_016_384;
                let metadata = fs::metadata(path).map_err(|e| format!("Read WAV: {e}"))?;
                if !metadata.is_file() || metadata.len() > MAX_BYTES {
                    return Err("Choose a regular WAV file smaller than 256 MB.".into());
                }
                let mut bytes = Vec::new();
                fs::File::open(path)
                    .map_err(|e| e.to_string())?
                    .take(MAX_BYTES + 1)
                    .read_to_end(&mut bytes)
                    .map_err(|e| e.to_string())?;
                if bytes.len() as u64 > MAX_BYTES {
                    return Err("WAV file is too large.".into());
                }
                let rate = h.info["sample_rate"].as_u64().unwrap() as u32;
                let (mut wave,source_rate)=looper_files::decode_resampled_with_limit(&bytes,rate,h.capacity).map_err(|e|format!("Decode/convert WAV: {e:?}. Supported: 8–192 kHz mono/stereo PCM16, PCM24 or float32 within the loop duration capacity."))?;
                looper_core::buffer::seam_fade(&mut wave.frames, rate, 0.015)
                    .map_err(|e| format!("Import seam: {e:?}"))?;
                h.source.controller().begin_import(r.track, &wave.frames)?;
                finish_replacement(&mut h.source)?;
                h.history.clear_track(r.track);
                Ok(
                    json!({"imported":true,"track":r.track,"frames":wave.frames.len(),"sample_rate":rate,"source_sample_rate":source_rate,"resampled":source_rate!=rate,"channels":wave.channels,"gain":1.0,"feedback":0.8,"seam_seconds":0.015}),
                )
            }
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
                    json!({"transport":"native-duplex","web_audio":false,"history":{"undo_track":h.history.target(false),"redo_track":h.history.target(true)},"info":h.info,"frames":s.frames,"underruns":s.underruns,
                "discontinuities":s.discontinuities,"command_errors":s.command_errors,"stream_errors":s.stream_errors,"malformed_blocks":s.malformed,
                "input_peak":s.input_peak,"output_peak":s.output_peak,"callback_frames":s.callback_frames,
                "capture":{"frames":s.capture.frames,"dropped_frames":s.capture.dropped_frames,"errors":s.capture.stream_errors,"nonfinite_samples":s.capture.nonfinite_samples,"malformed_blocks":s.capture.malformed_blocks},
                "tracks":s.tracks[..h.tracks].iter().enumerate().map(|(i,t)|json!({"undo":h.history.count(i,false),"redo":h.history.count(i,true),"state":format!("{:?}",t.state).to_lowercase(),"length":t.length_frames,"position":t.position_frames,"muted":t.muted,"stuttering":s.stuttering[i]})).collect::<Vec<_>>()}),
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
                        "retrigger" | "stutter_start" => {
                            let status = h.source.controller().status().tracks[r.track];
                            if (command == "stutter_start"
                                && status.state != looper_core::State::Playing)
                                || (command == "retrigger"
                                    && !matches!(
                                        status.state,
                                        looper_core::State::Playing | looper_core::State::Stopped
                                    ))
                            {
                                return Err("Retrigger needs a stopped/playing take; stutter needs playback.".into());
                            }
                            let rate = h.info["sample_rate"].as_u64().unwrap() as usize;
                            // Reference formula at the native mode's fixed 120 BPM:
                            // one sixteenth, at most half a take, at least 10 ms.
                            let fragment = (rate / 8)
                                .min(status.length_frames / 2)
                                .max(rate / 100)
                                .max(1);
                            Control::Track {
                                track: r.track,
                                command: if command == "retrigger" {
                                    Command::Retrigger(0)
                                } else {
                                    Command::StutterStart(fragment)
                                },
                            }
                        }
                        "stutter_stop" => Control::Track {
                            track: r.track,
                            command: Command::StutterStop,
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
                let punch = matches!(r.command.as_str(), "overdub" | "substitute");
                // The service is the sole control producer. A coherent playing take
                // with an empty control queue cannot start writing before this send.
                // Snapshot/copy happens on this worker while playback keeps running.
                let mut before_punch = None;
                let mut history_notice = None;
                if punch {
                    // Drain earlier slider/play/stop requests before taking history.
                    // This preserves ordered controls without snapshotting old settings.
                    let deadline = Instant::now() + Duration::from_millis(100);
                    while h.source.controller().controls_pending() {
                        h.source.pump();
                        if Instant::now() >= deadline {
                            return Err("Native controls are still pending; retry the punch when audio resumes.".into());
                        }
                        std::thread::sleep(Duration::from_millis(1));
                    }
                    if h.source.controller().status().tracks[r.track].state
                        != looper_core::State::Playing
                    {
                        return Err("Start playback before overdub or substitute.".into());
                    }
                    match h.source.controller().snapshot(project::MAX_FRAMES) {
                        Ok(mut takes) => {
                            let take = &mut takes[r.track];
                            if !take.playing || take.audio.is_empty() {
                                return Err("Start playback before overdub or substitute.".into());
                            }
                            if take.audio.len() > HISTORY_FRAMES {
                                history_notice = Some("Punch started without undo: the take exceeds the 8 million frame history limit.");
                            } else {
                                before_punch = Some(std::mem::take(&mut take.audio));
                            }
                        }
                        Err(_) => history_notice = Some("Punch started without undo: finish other captures/punches and wait for controls before starting an undoable pass."),
                    }
                }
                h.source.controller().send(control)?;
                if matches!(r.command.as_str(), "record" | "clear")
                    || (punch && before_punch.is_none())
                {
                    h.history.clear_track(r.track);
                }
                if punch && before_punch.is_none() {
                    h.history.new_edit(r.track);
                }
                if let Some(audio) = before_punch {
                    h.history.new_edit(r.track);
                    h.history.push(r.track, audio, false);
                }
                match control {
                    Control::Master(value) => h.master = value,
                    Control::InputGain(value) => h.input_gain = value,
                    _ => {}
                }
                Ok(
                    json!({"queued":true,"undo_recorded":if punch {Some(history_notice.is_none())} else {None},"history_notice":history_notice}),
                )
            }
            _ => Err("Unknown native audio request.".into()),
        }
    }
}

fn finish_replacement(source: &mut Source) -> Result<(), String> {
    let deadline = Instant::now() + Duration::from_secs(5);
    loop {
        source.pump();
        if let Some(result) = source.controller().finish_import() {
            return result;
        }
        if Instant::now() > deadline && source.controller().cancel_import() {
            return Err("Audio callback did not accept the take in time; the existing take was kept. Close audio if the device has stopped.".into());
        }
        std::thread::sleep(Duration::from_millis(1));
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

    fn pcm(service: &mut Service) -> Vec<looper_core::checkpoint::Take> {
        service
            .handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .snapshot(project::MAX_FRAMES)
            .unwrap()
    }
    fn control(service: &mut Service, token: u64, command: &str, value: f32) {
        let mut r = request("engine_control", token);
        r.command = command.into();
        r.value = value;
        assert_eq!(service.execute(r)["ok"], true);
        blocks(service, 1);
    }
    fn edit(service: &mut Service, token: u64, command: &str) -> Value {
        let mut r = request("engine_edit", token);
        r.command = command.into();
        service.execute(r)
    }
    #[test]
    fn normalize_history_is_exact_preserves_mix_and_other_playback_and_rejects_active_targets() {
        let mut service = Service::new(true);
        let token = service.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        blocks(&mut service, 1);
        control(&mut service, token, "record", 0.0);
        blocks(&mut service, 4);
        control(&mut service, token, "finish", 0.0);
        for (name, value) in [("gain", 0.5), ("feedback", 0.6), ("mute", 1.0)] {
            control(&mut service, token, name, value);
        }
        let before = pcm(&mut service).remove(0);
        // Another loop keeps playing during all offline edits.
        service
            .handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .begin_import(1, &[[0.2, -0.1]; 256])
            .unwrap();
        blocks(&mut service, 1);
        assert!(service
            .handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .finish_import()
            .unwrap()
            .is_ok());
        let mut play = request("engine_control", token);
        play.command = "play".into();
        play.track = 1;
        assert_eq!(service.execute(play)["ok"], true);
        blocks(&mut service, 1);
        assert_eq!(edit(&mut service, token, "normalize")["ok"], true);
        let normalized = pcm(&mut service);
        assert!(normalized[1].playing);
        assert_eq!(normalized[1].audio, vec![[0.2, -0.1]; 256]);
        assert_eq!(
            (
                normalized[0].gain,
                normalized[0].feedback,
                normalized[0].muted
            ),
            (0.5, 0.6, true)
        );
        let peak = normalized[0]
            .audio
            .iter()
            .flatten()
            .copied()
            .map(f32::abs)
            .fold(0.0, f32::max);
        assert!((peak - 1.0).abs() < 1e-6);
        assert_eq!(
            edit(&mut service, token, "normalize")["value"]["changed"],
            false
        );
        assert_eq!(service.handle.as_ref().unwrap().history.count(0, false), 1);
        assert_eq!(edit(&mut service, token, "undo")["ok"], true);
        assert_eq!(pcm(&mut service)[0], before);
        assert_eq!(edit(&mut service, token, "undo")["ok"], false);
        assert_eq!(edit(&mut service, token, "redo")["ok"], true);
        assert_eq!(pcm(&mut service)[0], normalized[0]);
        assert_eq!(edit(&mut service, token, "undo")["ok"], true);
        assert_eq!(edit(&mut service, token, "normalize")["ok"], true);
        assert_eq!(
            edit(&mut service, token, "redo")["ok"],
            false,
            "new edit invalidates redo"
        );
        control(&mut service, token, "play", 0.0);
        assert_eq!(edit(&mut service, token, "undo")["ok"], false);
        assert_eq!(
            service.handle.as_ref().unwrap().history.count(0, false),
            1,
            "rejection retains history"
        );
        control(&mut service, token, "overdub", 0.0);
        assert_eq!(
            service.handle.as_ref().unwrap().history.count(0, false),
            2,
            "punch keeps older edits and snapshots the previous take"
        );
        control(&mut service, token, "finish", 0.0);
        control(&mut service, token, "clear", 0.0);
        assert_eq!(
            edit(&mut service, token, "undo")["ok"],
            false,
            "never resurrect deleted takes"
        );
        service.shutdown();
    }
    fn punch_request(service: &mut Service, token: u64, command: &str) -> Value {
        let mut r = request("engine_control", token);
        r.command = command.into();
        service.execute(r)
    }
    #[test]
    fn punch_history_restores_exact_pcm_for_both_modes_and_rejects_invalid_or_pending_passes() {
        let mut s = Service::new(true);
        let token = s.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        blocks(&mut s, 1);
        s.handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .begin_import(0, &[[0.125, -0.25]; 512])
            .unwrap();
        blocks(&mut s, 1);
        s.handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .finish_import()
            .unwrap()
            .unwrap();
        for (name, value) in [("gain", 0.5), ("feedback", 0.6), ("mute", 1.0)] {
            control(&mut s, token, name, value);
        }
        s.handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .begin_import(1, &[[0.2, -0.1]; 256])
            .unwrap();
        blocks(&mut s, 1);
        s.handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .finish_import()
            .unwrap()
            .unwrap();
        let mut other = request("engine_control", token);
        other.track = 1;
        other.command = "play".into();
        s.execute(other);
        blocks(&mut s, 1);
        let original = pcm(&mut s).remove(0);
        assert_eq!(punch_request(&mut s, token, "overdub")["ok"], false);
        for mode in ["overdub", "substitute"] {
            control(&mut s, token, "play", 0.0);
            let before = pcm(&mut s).remove(0).audio;
            let begun = punch_request(&mut s, token, mode);
            assert_eq!(begun["ok"], true, "{begun}");
            assert_eq!(begun["value"]["undo_recorded"], true);
            blocks(&mut s, 5);
            let count = s.handle.as_ref().unwrap().history.count(0, false);
            assert_eq!(
                punch_request(&mut s, token, mode)["ok"],
                false,
                "duplicate punch must not clear history"
            );
            assert_eq!(
                edit(&mut s, token, "undo")["ok"],
                false,
                "finish the pass before undo"
            );
            assert_eq!(s.handle.as_ref().unwrap().history.count(0, false), count);
            control(&mut s, token, "finish", 0.0);
            let punched = pcm(&mut s).remove(0);
            assert_ne!(punched.audio, before);
            assert_eq!(edit(&mut s, token, "undo")["ok"], true);
            assert_eq!(pcm(&mut s)[0].audio, before);
            assert_eq!(edit(&mut s, token, "redo")["ok"], true);
            assert_eq!(pcm(&mut s)[0], punched);
            let other = pcm(&mut s).remove(1);
            assert!(other.playing);
            assert_eq!(other.audio, vec![[0.2, -0.1]; 256]);
            assert_eq!(
                (punched.gain, punched.feedback, punched.muted),
                (original.gain, original.feedback, original.muted)
            );
        }
        assert_eq!(edit(&mut s, token, "undo")["ok"], true);
        control(&mut s, token, "play", 0.0);
        assert_eq!(punch_request(&mut s, token, "overdub")["ok"], true);
        blocks(&mut s, 3);
        control(&mut s, token, "finish", 0.0);
        assert_eq!(
            edit(&mut s, token, "redo")["ok"],
            false,
            "new punch invalidates redo"
        );
        let before = pcm(&mut s).remove(0);
        let count = s.handle.as_ref().unwrap().history.count(0, false);
        assert_eq!(
            punch_request(&mut s, token, "substitute")["ok"],
            false,
            "stopped pass rejected"
        );
        assert_eq!(pcm(&mut s)[0], before);
        assert_eq!(s.handle.as_ref().unwrap().history.count(0, false), count);
        control(&mut s, token, "play", 0.0);
        let mut pending = request("engine_control", token);
        pending.command = "stop".into();
        s.execute(pending);
        assert_eq!(
            punch_request(&mut s, token, "overdub")["ok"],
            false,
            "queued stop is applied before deciding whether to start a pass"
        );
        blocks(&mut s, 1);
        assert_eq!(s.handle.as_ref().unwrap().history.count(0, false), count);
        control(&mut s, token, "clear", 0.0);
        assert_eq!(edit(&mut s, token, "undo")["ok"], false);
        s.shutdown();
    }
    #[test]
    fn concurrent_capture_keeps_running_and_explicitly_reports_unavailable_punch_history() {
        let mut s = Service::new(true);
        let token = s.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        control(&mut s, token, "record", 0.0);
        blocks(&mut s, 4);
        control(&mut s, token, "finish", 0.0);
        assert_eq!(edit(&mut s, token, "normalize")["ok"], true);
        control(&mut s, token, "play", 0.0);
        let mut record = request("engine_control", token);
        record.track = 1;
        record.command = "record".into();
        s.execute(record);
        blocks(&mut s, 2);
        let frames = s
            .handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .status()
            .tracks[1]
            .length_frames;
        let begun = punch_request(&mut s, token, "overdub");
        assert_eq!(begun["ok"], true);
        assert_eq!(begun["value"]["undo_recorded"], false);
        assert!(begun["value"]["history_notice"]
            .as_str()
            .unwrap()
            .contains("without undo"));
        blocks(&mut s, 4);
        let status = s.handle.as_mut().unwrap().source.controller().status();
        assert_eq!(status.tracks[0].state, looper_core::State::Overdubbing);
        assert_eq!(status.tracks[1].state, looper_core::State::Recording);
        assert!(status.tracks[1].length_frames > frames);
        assert_eq!(
            s.handle.as_ref().unwrap().history.count(0, false),
            0,
            "untracked punch invalidates stale history"
        );
        control(&mut s, token, "finish", 0.0);
        let mut finish = request("engine_control", token);
        finish.track = 1;
        finish.command = "finish".into();
        s.execute(finish);
        blocks(&mut s, 1);
        assert_eq!(edit(&mut s, token, "undo")["ok"], false);
        s.shutdown();
    }
    fn edit_track(service: &mut Service, token: u64, track: usize, command: &str) -> Value {
        let mut r = request("engine_edit", token);
        r.track = track;
        r.command = command.into();
        service.execute(r)
    }
    #[test]
    fn global_history_uses_rust_order_and_keeps_local_and_global_redo_consistent() {
        let mut s = Service::new(true);
        let token = s.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        blocks(&mut s, 1);
        for track in 0..2 {
            s.handle
                .as_mut()
                .unwrap()
                .source
                .controller()
                .begin_import(track, &[[0.125, -0.25]; 256])
                .unwrap();
            blocks(&mut s, 1);
            s.handle
                .as_mut()
                .unwrap()
                .source
                .controller()
                .finish_import()
                .unwrap()
                .unwrap();
        }
        assert_eq!(edit(&mut s, token, "global_undo")["ok"], false);
        let original = pcm(&mut s);
        for track in 0..2 {
            assert_eq!(edit_track(&mut s, token, track, "normalize")["ok"], true);
        }
        let normalized = pcm(&mut s);
        let status = s.execute(request("engine_status", token));
        assert_eq!(status["value"]["history"]["undo_track"], 1);
        // Global commands ignore a caller's track: the worker chooses the latest entry.
        let undone = edit_track(&mut s, token, usize::MAX, "global_undo");
        assert_eq!(undone["ok"], true);
        assert_eq!(undone["value"]["track"], 1);
        assert_eq!(pcm(&mut s)[1], original[1]);
        assert_eq!(edit_track(&mut s, token, 0, "undo")["ok"], true);
        let status = s.execute(request("engine_status", token));
        assert!(status["value"]["history"]["undo_track"].is_null());
        assert_eq!(status["value"]["history"]["redo_track"], 0);
        for track in 0..2 {
            assert_eq!(edit(&mut s, token, "global_redo")["value"]["track"], track);
        }
        assert_eq!(pcm(&mut s), normalized);
        assert_eq!(edit(&mut s, token, "global_undo")["value"]["track"], 1);
        assert_eq!(edit_track(&mut s, token, 0, "undo")["ok"], true);
        assert_eq!(edit_track(&mut s, token, 0, "normalize")["ok"], true);
        assert_eq!(
            edit(&mut s, token, "global_redo")["ok"],
            false,
            "new edit clears the global redo timeline"
        );
        assert_eq!(
            s.handle.as_ref().unwrap().history.count(1, true),
            1,
            "other loop keeps its local redo like the original UI"
        );
        assert_eq!(edit_track(&mut s, token, 1, "redo")["ok"], true);
        assert_eq!(pcm(&mut s), normalized);
        let mut clear = request("engine_control", token);
        clear.track = 1;
        clear.command = "clear".into();
        s.execute(clear);
        blocks(&mut s, 1);
        assert_eq!(
            s.execute(request("engine_status", token))["value"]["history"]["undo_track"],
            0,
            "removed loop cannot remain the global target"
        );
        control(&mut s, token, "play", 0.0);
        assert_eq!(
            edit(&mut s, token, "global_undo")["ok"],
            false,
            "playing target rejected without consuming history"
        );
        assert_eq!(s.handle.as_ref().unwrap().history.target(false), Some(0));
        control(&mut s, token, "stop", 0.0);
        let mut capture = request("engine_control", token);
        capture.track = 1;
        capture.command = "record".into();
        s.execute(capture);
        blocks(&mut s, 1);
        assert_eq!(
            edit(&mut s, token, "global_undo")["ok"],
            false,
            "active capture prevents snapshots without consuming history"
        );
        let mut finish = request("engine_control", token);
        finish.track = 1;
        finish.command = "finish".into();
        s.execute(finish);
        blocks(&mut s, 1);
        assert_eq!(edit(&mut s, token, "global_undo")["value"]["track"], 0);
        assert_eq!(pcm(&mut s)[0], original[0]);
        s.shutdown();
        let fresh = s.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        assert_eq!(
            edit(&mut s, fresh, "global_redo")["ok"],
            false,
            "session history is never resurrected"
        );
    }
    #[test]
    fn global_targets_follow_step_and_pcm_budget_eviction() {
        let mut history = EditHistory::default();
        for track in [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0] {
            history.push(track, vec![[0.0; 2]; 1], false);
        }
        assert_eq!(history.count(0, false), 5);
        assert_eq!(history.target(false), Some(0));
        history.clear_track(0);
        assert_eq!(history.target(false), Some(1));
        history.push(2, vec![[0.0; 2]; HISTORY_FRAMES], true);
        assert_eq!(history.target(false), None);
        assert_eq!(history.target(true), Some(2));
        history.new_edit(3);
        assert_eq!(history.target(true), None);
        assert_eq!(history.count(2, true), 1);
        history.clear_track(2);
        assert_eq!(history.target(true), None);
    }
    #[test]
    fn transport_controls_preserve_pcm_and_history_and_release_on_punch_stop_or_clear() {
        let mut s = Service::new(true);
        let token = s.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        control(&mut s, token, "record", 0.0);
        blocks(&mut s, 5);
        control(&mut s, token, "finish", 0.0);
        assert_eq!(edit(&mut s, token, "normalize")["ok"], true);
        let original = pcm(&mut s).remove(0).audio;
        assert_eq!(
            punch_request(&mut s, token, "stutter_start")["ok"],
            false,
            "stopped stutter rejected on worker"
        );
        control(&mut s, token, "retrigger", 0.0);
        control(&mut s, token, "stutter_start", 0.0);
        blocks(&mut s, 6);
        let status = s.execute(request("engine_status", token));
        assert_eq!(status["value"]["tracks"][0]["stuttering"], true);
        assert_eq!(pcm(&mut s)[0].audio, original);
        assert_eq!(s.handle.as_ref().unwrap().history.count(0, false), 1);
        control(&mut s, token, "stutter_stop", 0.0);
        assert_eq!(
            s.execute(request("engine_status", token))["value"]["tracks"][0]["stuttering"],
            false
        );
        control(&mut s, token, "stutter_start", 0.0);
        control(&mut s, token, "overdub", 0.0);
        assert_eq!(
            s.execute(request("engine_status", token))["value"]["tracks"][0]["stuttering"],
            false
        );
        control(&mut s, token, "finish", 0.0);
        assert_eq!(edit(&mut s, token, "undo")["ok"], true);
        assert_eq!(pcm(&mut s)[0].audio, original);
        control(&mut s, token, "retrigger", 0.0);
        control(&mut s, token, "stutter_start", 0.0);
        control(&mut s, token, "stop", 0.0);
        assert_eq!(
            s.execute(request("engine_status", token))["value"]["tracks"][0]["stuttering"],
            false
        );
        control(&mut s, token, "clear", 0.0);
        control(&mut s, token, "stutter_stop", 0.0);
        assert_eq!(
            punch_request(&mut s, token, "retrigger")["ok"],
            false,
            "empty retrigger rejected on worker"
        );
        assert_eq!(
            s.execute(request("engine_status", token))["value"]["command_errors"],
            0
        );
        s.shutdown();
    }
    #[test]
    fn history_caps_steps_and_total_pcm_across_both_stacks() {
        let mut history = EditHistory::default();
        for _ in 0..9 {
            history.push(0, vec![[0.0; 2]; 8], false);
        }
        assert_eq!(history.count(0, false), 5);
        history.push(1, vec![[0.0; 2]; HISTORY_FRAMES / 2], false);
        history.push(2, vec![[0.0; 2]; HISTORY_FRAMES / 2], true);
        assert_eq!(history.count(0, false), 0);
        assert_eq!(
            history
                .undo
                .iter()
                .chain(&history.redo)
                .map(|(_, a, _)| a.len())
                .sum::<usize>(),
            HISTORY_FRAMES
        );
        history.clear_track(2);
        assert_eq!(history.count(2, true), 0);
    }
    #[test]
    fn wav_import_decodes_on_worker_applies_reference_seam_and_keeps_other_playback() {
        let mut service = Service::new(true);
        let dir = std::env::temp_dir().join(format!(
            "aion-import-test-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        fs::create_dir(&dir).unwrap();
        let token = service.execute(request("engine_open", 0))["value"]["token"]
            .as_u64()
            .unwrap();
        for cmd in ["record", "finish", "play"] {
            let mut r = request("engine_control", token);
            r.command = cmd.into();
            assert_eq!(service.execute(r)["ok"], true);
            blocks(&mut service, 4);
        }
        let original = service
            .handle
            .as_mut()
            .unwrap()
            .source
            .controller()
            .snapshot(32000000)
            .unwrap()[0]
            .audio
            .clone();
        let path = dir.join("fixture.wav");
        let mut expected = Vec::new();
        for format in [
            looper_files::Format::Pcm16,
            looper_files::Format::Pcm24,
            looper_files::Format::Float32,
        ] {
            for (source_rate, channels) in [
                (48000, 1),
                (48000, 2),
                (44100, 1),
                (44100, 2),
                (96000, 1),
                (96000, 2),
            ] {
                let input = vec![[0.125, -0.375]; 2000];
                let bytes = looper_files::encode(&input, source_rate, channels, format).unwrap();
                fs::write(&path, &bytes).unwrap();
                expected = looper_files::decode_resampled_with_limit(&bytes, 48000, 48000)
                    .unwrap()
                    .0
                    .frames;
                looper_core::buffer::seam_fade(&mut expected, 48000, 0.015).unwrap();
                let mut r = request("engine_project_import", token);
                r.track = 1;
                r.path = path.to_string_lossy().into_owned();
                let imported = service.execute(r);
                assert_eq!(imported["ok"], true, "{imported}");
                assert_eq!(imported["value"]["source_sample_rate"], source_rate);
                assert_eq!(imported["value"]["resampled"], source_rate != 48000);
                let takes = service
                    .handle
                    .as_mut()
                    .unwrap()
                    .source
                    .controller()
                    .snapshot(32000000)
                    .unwrap();
                assert_eq!(takes[0].audio, original);
                assert!(takes[0].playing);
                assert_eq!(takes[1].audio, expected);
                assert!(!takes[1].playing);
            }
        }
        for bytes in [
            b"broken WAV".to_vec(),
            looper_files::encode(&[[0.2; 2]; 44101], 44100, 2, looper_files::Format::Float32)
                .unwrap(),
            looper_files::encode(&[[0.2; 2]; 48001], 48000, 2, looper_files::Format::Float32)
                .unwrap(),
        ] {
            fs::write(&path, bytes).unwrap();
            let mut r = request("engine_project_import", token);
            r.track = 1;
            r.path = path.to_string_lossy().into_owned();
            assert_eq!(service.execute(r)["ok"], false);
            assert_eq!(
                service
                    .handle
                    .as_mut()
                    .unwrap()
                    .source
                    .controller()
                    .snapshot(32000000)
                    .unwrap()[1]
                    .audio,
                expected
            );
        }
        let mut playing = request("engine_project_import", token);
        playing.path = path.to_string_lossy().into_owned();
        assert_eq!(service.execute(playing)["ok"], false);
        service.shutdown();
        fs::remove_file(path).unwrap();
        fs::remove_dir(dir).unwrap();
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
