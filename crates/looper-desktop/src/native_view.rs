// Reuse the reference presentation without loading its browser audio runtime.
const ORIGINAL: &str = include_str!("../../../ui/index.html");
const LOOP_UI: &str = include_str!("../../../ui/Modules/loopTracks.js");
pub fn html() -> String {
    let mut html = ORIGINAL.to_owned();
    while let Some(start) = html.find("<script") {
        let end = start
            + html[start..]
                .find("</script>")
                .expect("Original script must close")
            + 9;
        html.replace_range(start..end, "");
    }
    html.replace("<title>ASCII Looper v0.76.16</title>", "<title>Aion Looper — Native Audio</title>")
        .replace("href=\"data:,\"", "href=\"favicon.ico\"")
        .replace("</head>", "<link rel=\"stylesheet\" href=\"native-view.css\"><script src=\"native-original-renderer.js\" defer></script><script src=\"native-app.js\" defer></script></head>")
}
pub fn renderer() -> String {
    let start = LOOP_UI
        .find("    static createEffectToggleHTML(")
        .expect("Original loop renderer start");
    let end = LOOP_UI
        .find("    static formatMeterLevel(")
        .expect("Original loop renderer end");
    // These methods only render markup. Their model is supplied by the native controller.
    format!("window.AionOriginalRenderer = (state, I18n, SamplerManager, effectColors) => {{ class UIManager {{ {} }} return UIManager; }};", &LOOP_UI[start..end])
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn original_structure_logo_and_styles_are_preserved_without_original_scripts() {
        let html = html();
        for id in [
            "header",
            "startOverlay",
            "mod-menu",
            "mod-sync",
            "mod-input",
            "mod-master",
            "loopContainer",
            "ws-loops",
        ] {
            assert!(html.contains(&format!("id=\"{id}\"")));
        }
        let logo = ORIGINAL
            .split("<span class=\"rainbow-text\">")
            .nth(1)
            .unwrap()
            .split("</span>")
            .next()
            .unwrap();
        assert!(html.contains(logo));
        assert!(html.contains("href=\"styles.css\""));
        assert_eq!(html.matches("<script").count(), 2);
        assert!(!html.contains("src=\"main.js\""));
        let renderer = renderer();
        assert!(renderer.contains("loop-track-header"));
        assert!(!renderer.contains("new Audio"));
    }
}
