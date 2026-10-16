use std::borrow::Cow;
use wry::http::Response;
macro_rules! ui {
    ($path:literal) => {
        include_bytes!(concat!(env!("CARGO_MANIFEST_DIR"), "/../../ui/", $path))
    };
}
pub fn response(path: &str) -> Response<Cow<'static, [u8]>> {
    let (mime, content): (&str, Cow<'static, [u8]>) = match path {
        "/native-index.html" => (
            "text/html; charset=utf-8",
            Cow::Borrowed(include_bytes!("../assets/native-index.html")),
        ),
        "/native-app.js" => (
            "application/javascript",
            Cow::Borrowed(include_bytes!("../assets/native-app.js")),
        ),
        "/" | "/index.html" => {
            let html = std::str::from_utf8(ui!("index.html"))
                .unwrap()
                .replace("ASCII Looper", "Aion Looper")
                .replace("ASCII LOOPER", "AION LOOPER")
                .replace(
                    "https://cdn.jsdelivr.net/npm/soundtouchjs@0.1.30/dist/soundtouch.min.js",
                    "./vendor/soundtouch.min.js",
                )
                .replace(
                    "</body>",
                    "<script src=\"native-bridge.js\" defer></script><script src=\"native-input.js\" defer></script><script src=\"native-midi.js\" defer></script></body>",
                );
            ("text/html; charset=utf-8", Cow::Owned(html.into_bytes()))
        }
        "/styles.css" => ("text/css; charset=utf-8", Cow::Borrowed(ui!("styles.css"))),
        "/main.js" => {
            let mut source = std::str::from_utf8(ui!("main.js")).unwrap().to_owned();
            let edits: Vec<[&str; 2]> =
                serde_json::from_str(include_str!("../assets/input-edits.json")).unwrap();
            for [from, to] in edits {
                source = source.replace(from, to);
            }
            source = source
                .replace("ASCII Looper", "Aion Looper")
                .replace("ASCII LOOPER", "AION LOOPER");
            ("application/javascript", Cow::Owned(source.into_bytes()))
        }
        "/Modules/i18n.js" => (
            "application/javascript",
            Cow::Borrowed(ui!("Modules/i18n.js")),
        ),
        "/Modules/debug.js" => (
            "application/javascript",
            Cow::Borrowed(ui!("Modules/debug.js")),
        ),
        "/Modules/effects.js" => (
            "application/javascript",
            Cow::Borrowed(ui!("Modules/effects.js")),
        ),
        "/Modules/scales.js" => (
            "application/javascript",
            Cow::Borrowed(ui!("Modules/scales.js")),
        ),
        "/Modules/droneSynth.js" => (
            "application/javascript",
            Cow::Borrowed(ui!("Modules/droneSynth.js")),
        ),
        "/Modules/audioEngine.js" => (
            "application/javascript",
            Cow::Borrowed(ui!("Modules/audioEngine.js")),
        ),
        "/Modules/loopTracks.js" => {
            let mut source = std::str::from_utf8(ui!("Modules/loopTracks.js"))
                .unwrap()
                .to_owned();
            let edits: Vec<[String; 2]> =
                serde_json::from_str(include_str!("../assets/multiply-edits.json")).unwrap();
            for [from, to] in edits {
                source = source.replacen(&from, &to, 1);
            }
            let edits: Vec<[String; 2]> =
                serde_json::from_str(include_str!("../assets/punch-edits.json")).unwrap();
            for [from, to] in edits {
                source = source.replacen(&from, &to, 1);
            }
            let edits: Vec<[String; 2]> =
                serde_json::from_str(include_str!("../assets/recording-edits.json")).unwrap();
            for [from, to] in edits {
                source = source.replacen(&from, &to, 1);
            }
            ("application/javascript", Cow::Owned(source.into_bytes()))
        }
        "/Modules/tracker.js" => {
            let mut source = std::str::from_utf8(ui!("Modules/tracker.js"))
                .unwrap()
                .to_owned();
            let edits: Vec<[String; 2]> =
                serde_json::from_str(include_str!("../assets/stretch-edits.json")).unwrap();
            for [from, to] in edits {
                source = source.replacen(&from, &to, 1);
            }
            ("application/javascript", Cow::Owned(source.into_bytes()))
        }
        "/Modules/projectManager.js" => {
            let mut source = std::str::from_utf8(ui!("Modules/projectManager.js"))
                .unwrap()
                .to_owned();
            let edits: Vec<[&str; 2]> =
                serde_json::from_str(include_str!("../assets/project-edits.json")).unwrap();
            for [from, to] in edits {
                source = source.replacen(from, to, 1);
            }
            let edits: Vec<[String; 2]> =
                serde_json::from_str(include_str!("../assets/export-edits.json")).unwrap();
            for [from, to] in edits {
                source = source.replace(&from, &to);
            }
            ("application/javascript", Cow::Owned(source.into_bytes()))
        }
        "/vendor/soundtouch.min.js" | "/soundtouch.min.js" => (
            "application/javascript",
            Cow::Borrowed(include_bytes!("../assets/vendor/soundtouch.min.js")),
        ),
        "/native-bridge.js" => (
            "application/javascript",
            Cow::Borrowed(include_bytes!("../assets/native-bridge.js")),
        ),
        "/native-input.js" => (
            "application/javascript",
            Cow::Borrowed(include_bytes!("../assets/native-input.js")),
        ),
        "/native-midi.js" => (
            "application/javascript",
            Cow::Borrowed(include_bytes!("../assets/native-midi.js")),
        ),
        "/native-input-worklet.js" => (
            "application/javascript",
            Cow::Borrowed(include_bytes!("../assets/native-input-worklet.js")),
        ),
        "/favicon.ico" => ("image/x-icon", Cow::Borrowed(ui!("favicon.ico"))),
        "/harmony-effect.afx" => ("application/json", Cow::Borrowed(ui!("harmony-effect.afx"))),
        _ => {
            return Response::builder()
                .status(404)
                .body(Cow::Borrowed(&b"Not found"[..]))
                .unwrap()
        }
    };
    Response::builder()
        .header("Content-Type", mime)
        .header("Cache-Control", "no-cache")
        .header("X-Content-Type-Options", "nosniff")
        .body(content)
        .unwrap()
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn sample_lab_only_replaces_stretch_dispatch_and_context_guard() {
        let original = std::str::from_utf8(ui!("Modules/tracker.js")).unwrap();
        let served = response("/Modules/tracker.js");
        let mut restored = std::str::from_utf8(served.body()).unwrap().to_owned();
        let edits: Vec<[String; 2]> =
            serde_json::from_str(include_str!("../assets/stretch-edits.json")).unwrap();
        for [from, to] in edits {
            assert_eq!(original.matches(&from).count(), 1);
            assert_eq!(restored.matches(&to).count(), 1);
            restored = restored.replacen(&to, &from, 1);
        }
        assert_eq!(restored, original);
    }
    #[test]
    fn loop_adapters_only_replace_capture_finalization() {
        let original = std::str::from_utf8(ui!("Modules/loopTracks.js")).unwrap();
        let served = response("/Modules/loopTracks.js");
        let mut restored = std::str::from_utf8(served.body()).unwrap().to_owned();
        let mut edits: Vec<[String; 2]> =
            serde_json::from_str(include_str!("../assets/multiply-edits.json")).unwrap();
        edits.extend(
            serde_json::from_str::<Vec<[String; 2]>>(include_str!("../assets/punch-edits.json"))
                .unwrap(),
        );
        edits.extend(
            serde_json::from_str::<Vec<[String; 2]>>(include_str!(
                "../assets/recording-edits.json"
            ))
            .unwrap(),
        );
        for [from, to] in edits {
            assert_eq!(original.matches(&from).count(), 1);
            assert_eq!(restored.matches(&to).count(), 1);
            restored = restored.replacen(&to, &from, 1);
        }
        assert_eq!(restored, original);
    }
    #[test]
    fn native_input_preserves_original_routing_and_only_changes_capture_calls() {
        let original = std::str::from_utf8(ui!("main.js")).unwrap();
        let response = response("/main.js");
        let mut served = std::str::from_utf8(response.body()).unwrap().to_owned();
        let edits: Vec<[&str; 2]> =
            serde_json::from_str(include_str!("../assets/input-edits.json")).unwrap();
        for ([from, to], count) in edits.into_iter().zip([2, 1, 1, 1]) {
            assert_eq!(original.matches(from).count(), count);
            assert_eq!(served.matches(to).count(), count);
            served = served.replace(to, from);
        }
        assert_eq!(
            served,
            original
                .replace("ASCII Looper", "Aion Looper")
                .replace("ASCII LOOPER", "AION LOOPER")
        );
        assert_eq!(self::response("/native-input-worklet.js").status(), 200);
    }
    #[test]
    fn project_adapters_only_replace_save_audio_and_export_finalization() {
        let original = std::str::from_utf8(ui!("Modules/projectManager.js")).unwrap();
        let response = response("/Modules/projectManager.js");
        let mut served = std::str::from_utf8(response.body()).unwrap().to_owned();
        let mut edits: Vec<[String; 2]> =
            serde_json::from_str(include_str!("../assets/project-edits.json")).unwrap();
        edits.extend(
            serde_json::from_str::<Vec<[String; 2]>>(include_str!("../assets/export-edits.json"))
                .unwrap(),
        );
        for [from, to] in edits {
            let count = original.matches(&from).count();
            assert!(count > 0);
            assert_eq!(served.matches(&to).count(), count);
            served = served.replace(&to, &from);
        }
        assert_eq!(served, original);
    }
    #[test]
    fn original_interface_and_offline_assets_are_embedded() {
        let response = response("/index.html");
        let html = std::str::from_utf8(response.body()).unwrap();
        assert!(html.contains("id=\"btn-start\""));
        assert!(html.contains("./vendor/soundtouch.min.js"));
        assert!(!html.contains("https://cdn.jsdelivr.net"));
        assert_eq!(
            self::response("/styles.css").body().as_ref(),
            ui!("styles.css")
        );
    }
    #[test]
    fn protocol_does_not_serve_arbitrary_files() {
        for path in [
            "/../Cargo.toml",
            "/%2e%2e/LICENSE",
            "/tests/module-smoke.test.cjs",
        ] {
            assert_eq!(response(path).status(), 404);
        }
    }
    #[test]
    fn native_audio_view_loads_no_browser_audio_engine_or_pcm_bridge() {
        for path in ["/native-index.html", "/native-app.js"] {
            let served = response(path);
            assert_eq!(served.status(), 200);
            let source = std::str::from_utf8(served.body()).unwrap();
            for dependency in [
                "AudioContext",
                "AudioWorklet",
                "native-input.js",
                "soundtouch",
            ] {
                assert!(
                    !source.contains(dependency),
                    "native view imports {dependency}"
                );
            }
        }
    }
}
