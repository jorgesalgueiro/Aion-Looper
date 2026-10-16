#![forbid(unsafe_code)]
mod assets;
mod bridge;
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
    Report(Value),
}
pub fn run(args: Vec<String>) -> Result<(), Box<dyn std::error::Error>> {
    let input_smoke = args.first().is_some_and(|arg| arg == "--input-smoke-test");
    let test_report = if input_smoke || args.first().is_some_and(|arg| arg == "--self-test") {
        Some(PathBuf::from(
            args.get(1).ok_or("Missing self-test report path.")?,
        ))
    } else {
        None
    };
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
    let (midi_sender, midi_receiver) = sync_channel::<native_midi::Request>(16);
    let input_proxy = proxy.clone();
    let allow_synthetic = test_report.is_some();
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
            if reply_proxy
                .send_event(UiEvent::Reply(bridge::execute(request)))
                .is_err()
            {
                break;
            }
        }
    });
    let window = WindowBuilder::new()
        .with_title("Aion Looper")
        .with_inner_size(tao::dpi::LogicalSize::new(1440.0, 960.0))
        .with_visible(test_report.is_none())
        .build(&event_loop)?;
    let mut builder = WebViewBuilder::new_with_web_context(&mut context)
        .with_custom_protocol("rustlooper".into(), |_, request| {
            assets::response(request.uri().path())
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
    if test_report.is_some() {
        builder = builder.with_initialization_script(if input_smoke {
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
    let deadline = Instant::now() + Duration::from_secs(45);
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
            } => *flow = ControlFlow::Exit,
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
