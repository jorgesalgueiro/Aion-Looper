#![forbid(unsafe_code)]
mod assets;
mod bridge;
mod native_audio;
mod native_input;
mod native_midi;
use serde_json::{json, Value};
use std::path::PathBuf;
use std::sync::mpsc::sync_channel;
use std::sync::{
    atomic::{AtomicU32, Ordering},
    Arc,
};
use std::time::{Duration, Instant};
use tao::event::{Event, WindowEvent};
use tao::event_loop::{ControlFlow, EventLoopBuilder};
use tao::window::WindowBuilder;
use wry::{PermissionKind, PermissionResponse};
use wry::{WebContext, WebViewBuilder};
enum UiEvent {
    Reply(Value),
    EncodedReply(String),
    Report(Value),
}
fn native_launch(args: &[String], native_default: bool) -> bool {
    match args.first().map(String::as_str) {
        Some("--native-self-test") => return true,
        Some("--self-test" | "--input-smoke-test") => return false,
        _ => {}
    }
    if args.iter().any(|arg| arg == "--compatibility-audio") {
        return false;
    }
    native_default || args.iter().any(|arg| arg == "--native-audio")
}
pub fn run(args: Vec<String>) -> Result<(), Box<dyn std::error::Error>> {
    let native_test = args.first().is_some_and(|arg| arg == "--native-self-test");
    let native_mode = native_launch(&args, cfg!(all(target_os = "windows", feature = "asio")));
    let native_test_physical = args.iter().any(|arg| arg == "--native-test-physical");
    let input_smoke = args.first().is_some_and(|arg| arg == "--input-smoke-test");
    let test_report =
        if native_test || input_smoke || args.first().is_some_and(|arg| arg == "--self-test") {
            Some(PathBuf::from(
                args.get(1).ok_or("Missing self-test report path.")?,
            ))
        } else {
            None
        };
    let argument = |name: &str| {
        args.windows(2)
            .find(|pair| pair[0] == name)
            .map(|pair| pair[1].clone())
    };
    let native_idle_seconds: f64 = argument("--native-idle-seconds")
        .unwrap_or_else(|| "0".into())
        .parse()
        .map_err(|_| "Invalid native idle test duration.")?;
    if !native_idle_seconds.is_finite() || !(0.0..=35.0).contains(&native_idle_seconds) {
        return Err("Native idle test duration must be between 0 and 35 seconds.".into());
    }
    let input_selection =
        json!({"host":argument("--input-host"),"device":argument("--input-device")});
    let input_test_seconds: f64 = argument("--input-test-seconds")
        .unwrap_or_else(|| "1.5".into())
        .parse()
        .map_err(|_| "Invalid input test duration.")?;
    if !input_test_seconds.is_finite() || !(1.0..=60.0).contains(&input_test_seconds) {
        return Err("Input test duration must be between 1 and 60 seconds.".into());
    }
    let profile = if let Some(path) = &test_report {
        path.parent()
            .unwrap_or(std::path::Path::new("."))
            .join("webview-self-test")
    } else {
        let executable = std::env::current_exe()?;
        let directory = executable.parent().ok_or("No executable directory.")?;
        let profile = directory.join("Rust-Looper-profile");
        let legacy = directory.parent().map(|parent| {
            parent
                .join("Rust-Looper-Windows")
                .join("Rust-Looper-profile")
        });
        // Reuse an existing portable profile after the package rename.
        if !profile.exists() {
            legacy.filter(|path| path.is_dir()).unwrap_or(profile)
        } else {
            profile
        }
    };
    std::fs::create_dir_all(&profile)?;
    // WebContext creates WebKitGTK objects, so GTK must be ready before it.
    // Tao also initializes GTK later, when constructing its event loop.
    #[cfg(target_os = "linux")]
    gtk::init()?;
    let mut context = WebContext::new(Some(profile));
    let authorized = Arc::new(AtomicU32::new(0));
    let permission_authorized = Arc::clone(&authorized);
    let event_loop = EventLoopBuilder::<UiEvent>::with_user_event().build();
    let proxy = event_loop.create_proxy();
    let (sender, receiver) = sync_channel::<bridge::Request>(16);
    let (input_sender, input_receiver) = sync_channel::<native_input::Request>(16);
    let (input_device_sender, input_device_receiver) = sync_channel::<native_input::Request>(2);
    let (engine_sender, engine_receiver) = sync_channel::<native_audio::Message>(16);
    let (midi_sender, midi_receiver) = sync_channel::<native_midi::Request>(16);
    let input_proxy = proxy.clone();
    let allow_synthetic = test_report.is_some();
    let engine_shutdown = engine_sender.clone();
    let engine_proxy = proxy.clone();
    std::thread::spawn(move || {
        let mut service = native_audio::Service::new(allow_synthetic);
        loop {
            match engine_receiver.recv_timeout(service.wait_duration()) {
                Ok(native_audio::Message::Shutdown) => {
                    service.shutdown();
                    break;
                }
                Ok(native_audio::Message::Request(request)) => {
                    if engine_proxy
                        .send_event(UiEvent::Reply(service.execute(request)))
                        .is_err()
                    {
                        break;
                    }
                }
                Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {}
                Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => break,
            }
            service.pump();
        }
    });
    let midi_proxy = proxy.clone();
    std::thread::spawn(move || {
        let mut service = native_midi::Service::new(allow_synthetic);
        loop {
            match midi_receiver.recv_timeout(service.wait_duration()) {
                Ok(request) => {
                    if midi_proxy
                        .send_event(UiEvent::Reply(service.execute(request)))
                        .is_err()
                    {
                        break;
                    }
                }
                Err(std::sync::mpsc::RecvTimeoutError::Timeout) => service.expire(),
                Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => break,
            }
            if let Some(packet) = service.pump() {
                if midi_proxy.send_event(UiEvent::Reply(packet)).is_err() {
                    break;
                }
            }
        }
    });
    std::thread::spawn(move || {
        let mut service = native_input::Service::new(allow_synthetic);
        loop {
            match input_receiver.recv_timeout(service.wait_duration()) {
                Ok(request) => {
                    if input_proxy
                        .send_event(UiEvent::Reply(service.execute(request)))
                        .is_err()
                    {
                        break;
                    }
                }
                Err(std::sync::mpsc::RecvTimeoutError::Timeout) => service.expire(),
                Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => break,
            }
            for packet in service.pump() {
                if input_proxy.send_event(UiEvent::Reply(packet)).is_err() {
                    return;
                }
            }
        }
    });
    // Device enumeration probes drivers and can take seconds. Keep it away from
    // the capture owner so refreshing the selector cannot overflow live inputs.
    let device_proxy = proxy.clone();
    std::thread::spawn(move || {
        let mut service = native_input::Service::new(allow_synthetic);
        while let Ok(request) = input_device_receiver.recv() {
            if device_proxy
                .send_event(UiEvent::Reply(service.execute(request)))
                .is_err()
            {
                break;
            }
        }
    });
    let reply_proxy = proxy.clone();
    std::thread::spawn(move || {
        while let Ok(request) = receiver.recv() {
            for reply in bridge::encode_replies(bridge::execute(request)) {
                if reply_proxy
                    .send_event(UiEvent::EncodedReply(reply))
                    .is_err()
                {
                    return;
                }
            }
        }
    });
    #[cfg(target_os = "windows")]
    let app_icon = {
        use tao::platform::windows::IconExtWindows;
        Some(tao::window::Icon::from_resource(1, None)?)
    };
    #[cfg(not(target_os = "windows"))]
    let app_icon = None;
    let window = WindowBuilder::new()
        .with_title(if native_mode {
            "Aion Looper — Native Audio"
        } else {
            "Aion Looper — Compatibility Audio"
        })
        .with_window_icon(app_icon)
        .with_inner_size(tao::dpi::LogicalSize::new(1440.0, 960.0))
        .with_visible(test_report.is_none())
        .build(&event_loop)?;
    let mut builder = WebViewBuilder::new_with_web_context(&mut context)
        .with_custom_protocol("rustlooper".into(), move |_, request| {
            let path=request.uri().path();
            assets::response(if native_mode && matches!(path,"/"|"/index.html") {"/native-index.html"}else{path})
        })
        .with_ipc_handler(move |request| {
            let url = request.uri().to_string();
            if !(url.starts_with("https://rustlooper.localhost/")
                || url.starts_with("rustlooper://localhost/"))
                || request.body().len() > 24_000_000
            {
                return;
            }
            let Ok(value) = serde_json::from_str::<Value>(request.body()) else {
                return;
            };
            if value["operation"] == "authorize_audio" || value["operation"] == "authorize_midi" {
                let bit = if value["operation"] == "authorize_audio" {
                    1
                } else {
                    2
                };
                authorized.fetch_or(bit, Ordering::Release);
                let _ = proxy.send_event(UiEvent::Reply(
                    json!({"id":value["id"],"session":value["session"],"ok":true,"value":{}}),
                ));
                return;
            }
            if value["kind"] == "self_test" {
                let _ = proxy.send_event(UiEvent::Report(value));
            } else if value["operation"].as_str().is_some_and(|operation|operation.starts_with("engine_")) {
                let id=value["id"].clone();let session=value["session"].clone();
                let error=if !native_mode {Some("Restart with --native-audio to use the native engine.")}
                else if value["operation"]=="engine_open" && authorized.load(Ordering::Acquire)&1==0 {Some("Start audio before opening the native engine.")}
                else {match serde_json::from_value::<native_audio::Request>(value){
                    Ok(request)=>if engine_sender.try_send(native_audio::Message::Request(request)).is_ok(){None}else{Some("Native engine queue full.")},
                    Err(_)=>Some("Invalid native engine request."),
                }};
                if let Some(error)=error{let _=proxy.send_event(UiEvent::Reply(json!({"id":id,"session":session,"ok":false,"error":error})));}
            } else if value["operation"].as_str().is_some_and(|operation|operation.starts_with("midi_")) {
                let id=value["id"].clone();let session=value["session"].clone();
                let error=if value["operation"]=="midi_open"&&authorized.load(Ordering::Acquire)&2==0 {
                    Some("Request MIDI access through the workstation first.")
                } else {
                    match serde_json::from_value::<native_midi::Request>(value) {
                        Ok(request)=>if midi_sender.try_send(request).is_ok(){None}else{Some("Native MIDI queue full.")},
                        Err(_)=>Some("Invalid native MIDI request."),
                    }
                };
                if let Some(error)=error{let _=proxy.send_event(UiEvent::Reply(json!({"id":id,"session":session,"ok":false,"error":error})));}
            } else if value["operation"].as_str().is_some_and(|operation|operation.starts_with("input_")) {
                let id = value["id"].clone();
                let session = value["session"].clone();
                let error = if value["operation"] == "input_open" && authorized.load(Ordering::Acquire) & 1 == 0 {
                    Some("Press START before opening a native audio input.")
                } else {
                    match serde_json::from_value::<native_input::Request>(value) {
                        Ok(request) => {
                            let queue = if request.operation == "input_devices" {
                                &input_device_sender
                            } else {
                                &input_sender
                            };
                            if queue.try_send(request).is_ok() { None } else { Some("Native input queue full.") }
                        },
                        Err(_) => Some("Invalid native input request."),
                    }
                };
                if let Some(error) = error { let _ = proxy.send_event(UiEvent::Reply(json!({"id":id,"session":session,"ok":false,"error":error}))); }
            } else {
                let id = value["id"].clone();
                let session = value["session"].clone();
                match serde_json::from_value::<bridge::Request>(value) {
                    Ok(message) => {
                        if sender.try_send(message).is_err() {
                            let _ = proxy.send_event(UiEvent::Reply(
                                json!({"id":id,"session":session,"ok":false,"error":"Native worker queue full."}),
                            ));
                        }
                    }
                    Err(_) => {
                        let _ = proxy.send_event(UiEvent::Reply(
                            json!({"id":id,"session":session,"ok":false,"error":"Invalid native request."}),
                        ));
                    }
                }
            }
        })
        .with_permission_handler(move |kind| {
            let mask = permission_authorized.load(Ordering::Acquire);
            match kind {
                PermissionKind::Microphone if mask & 1 != 0 => PermissionResponse::Allow,
                PermissionKind::Midi if mask & 2 != 0 => PermissionResponse::Allow,
                _ => PermissionResponse::Default,
            }
        })
        .with_navigation_handler(|url| {
            url.starts_with("https://rustlooper.localhost/")
                || url.starts_with("rustlooper://localhost/")
        })
        .with_url("rustlooper://localhost/index.html");
    #[cfg(target_os = "windows")]
    {
        use wry::WebViewBuilderExtWindows;
        builder = builder.with_https_scheme(true);
        if test_report.is_some() {
            builder =
                builder.with_additional_browser_args("--autoplay-policy=no-user-gesture-required");
        }
    }
    builder = builder.with_initialization_script(format!(
        "window.AionNativeInputSelection={input_selection};window.AionInputTestSeconds={input_test_seconds};window.AionNativeTestPhysical={native_test_physical};window.AionNativeIdleSeconds={native_idle_seconds};"
    ));
    if test_report.is_some() {
        builder = builder.with_initialization_script(if native_test {
            include_str!("../assets/native-smoke-test.js")
        } else if input_smoke {
            include_str!("../assets/input-smoke-test.js")
        } else {
            include_str!("../assets/self-test.js")
        });
    }
    #[cfg(not(target_os = "linux"))]
    let webview = builder.build(&window)?;
    #[cfg(target_os = "linux")]
    let webview = {
        use tao::platform::unix::WindowExtUnix;
        use wry::WebViewBuilderExtUnix;
        builder.build_gtk(
            window
                .default_vbox()
                .ok_or("Missing GTK content container.")?,
        )?
    };
    let deadline = Instant::now()
        + Duration::from_secs(45)
        + if input_smoke || native_test {
            Duration::from_secs_f64(
                input_test_seconds
                    + if native_test {
                        native_idle_seconds
                    } else {
                        0.0
                    },
            )
        } else {
            Duration::ZERO
        };
    event_loop.run(move |event, _, flow| {
        *flow = if test_report.is_some() {
            ControlFlow::WaitUntil(deadline)
        } else {
            ControlFlow::Wait
        };
        match event {
            Event::WindowEvent {
                event: WindowEvent::CloseRequested,
                ..
            } => {
                let _ = engine_shutdown.try_send(native_audio::Message::Shutdown);
                *flow = ControlFlow::Exit;
            }
            Event::UserEvent(UiEvent::EncodedReply(value)) => {
                let _ = webview.evaluate_script(&format!("window.RustLooper?.receive({value});"));
            }
            Event::UserEvent(UiEvent::Reply(value)) => {
                let _ = webview.evaluate_script(&format!("window.RustLooper?.receive({value});"));
            }
            Event::UserEvent(UiEvent::Report(value)) => {
                if let Some(path) = &test_report {
                    let _ = std::fs::write(path, serde_json::to_vec_pretty(&value).unwrap());
                    *flow = ControlFlow::ExitWithCode(if value["ok"] == true { 0 } else { 1 });
                }
            }
            Event::NewEvents(_) if test_report.is_some() && Instant::now() >= deadline => {
                if let Some(path) = &test_report {
                    let _ =
                        std::fs::write(path, b"{\"ok\":false,\"error\":\"UI startup timed out\"}");
                }
                *flow = ControlFlow::ExitWithCode(1);
            }
            _ => (),
        }
    });
}

#[cfg(test)]
mod launch_tests {
    use super::native_launch;
    #[test]
    fn asio_default_uses_native_while_compatibility_and_test_modes_stay_explicit() {
        let args = |values: &[&str]| {
            values
                .iter()
                .map(|value| (*value).to_owned())
                .collect::<Vec<_>>()
        };
        assert!(native_launch(&[], true));
        assert!(!native_launch(&[], false));
        assert!(!native_launch(&args(&["--compatibility-audio"]), true));
        assert!(native_launch(&args(&["--native-audio"]), false));
        for mode in ["--self-test", "--input-smoke-test"] {
            assert!(!native_launch(&args(&[mode, "report.json"]), true));
        }
        assert!(native_launch(
            &args(&["--native-self-test", "report.json"]),
            false
        ));
    }
}
