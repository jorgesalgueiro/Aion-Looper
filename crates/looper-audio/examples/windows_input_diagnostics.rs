//! Paused Windows input diagnostics. No streams or graphs are started; no audio is read.
#[cfg(windows)]
fn main() -> windows::core::Result<()> {
    use windows::core::Interface;
    use windows::Security::Authorization::AppCapabilityAccess::{
        AppCapability, AppCapabilityAccessStatus,
    };
    use windows::Win32::{
        Media::Audio::*,
        System::{Com::*, WinRT::*},
    };
    unsafe {
        RoInitialize(RO_INIT_MULTITHREADED)?;
        let access = AppCapability::Create(&"microphone".into())
            .and_then(|capability| capability.CheckAccess());
        println!(
            "Windows microphone permission: {}",
            match access {
                Ok(AppCapabilityAccessStatus::Allowed) => "Allowed".to_owned(),
                other => format!("{other:?}"),
            }
        );
        let enumerator: IMMDeviceEnumerator =
            CoCreateInstance(&MMDeviceEnumerator, None, CLSCTX_ALL)?;
        let device = enumerator.GetDefaultAudioEndpoint(eCapture, eConsole)?;
        let mut success = false;
        // Fresh clients prevent one successful probe from causing AlreadyInitialized
        // errors in later probes. Each client remains paused and is then released.
        for (label, raw, modern) in [
            ("WASAPI shared", false, false),
            ("WASAPI raw", true, false),
            ("WASAPI shared v3", false, true),
        ] {
            let client: IAudioClient = device.Activate(CLSCTX_ALL, None)?;
            let client3: IAudioClient3 = client.cast()?;
            if raw {
                let properties = AudioClientProperties {
                    cbSize: std::mem::size_of::<AudioClientProperties>() as u32,
                    bIsOffload: false.into(),
                    eCategory: AudioCategory_Media,
                    Options: AUDCLNT_STREAMOPTIONS_RAW,
                };
                println!(
                    "Raw mode properties: {:?}",
                    client3.SetClientProperties(&properties)
                );
            }
            let format = client.GetMixFormat()?;
            let result = if modern {
                let (mut default, mut fundamental, mut minimum, mut maximum) = (0, 0, 0, 0);
                client3
                    .GetSharedModeEnginePeriod(
                        format,
                        &mut default,
                        &mut fundamental,
                        &mut minimum,
                        &mut maximum,
                    )
                    .and_then(|()| {
                        client3.InitializeSharedAudioStream(
                            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                            default,
                            format,
                            None,
                        )
                    })
            } else {
                client.Initialize(
                    AUDCLNT_SHAREMODE_SHARED,
                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                    100000,
                    0,
                    format,
                    None,
                )
            };
            CoTaskMemFree(Some(format.cast()));
            println!("{label} paused Initialize: {result:?}");
            success |= result.is_ok();
        }
        let graph_settings = windows::Media::Audio::AudioGraphSettings::Create(
            windows::Media::Render::AudioRenderCategory::Media,
        )?;
        graph_settings.SetQuantumSizeSelectionMode(
            windows::Media::Audio::QuantumSizeSelectionMode::LowestLatency,
        )?;
        let graph_result =
            windows::Media::Audio::AudioGraph::CreateAsync(&graph_settings)?.join()?;
        println!("AudioGraph creation: {:?}", graph_result.Status());
        if let Ok(graph) = graph_result.Graph() {
            let input = graph
                .CreateDeviceInputNodeAsync(windows::Media::Capture::MediaCategory::Other)?
                .join()?;
            println!(
                "AudioGraph input: {:?}; extended error: {:?}",
                input.Status(),
                input.ExtendedError()
            );
            if let Ok(node) = input.DeviceInputNode() {
                success = true;
                node.Close()?;
            }
            graph.Close()?;
        }
        println!("Any paused capture initialization succeeded: {success}. No microphone samples captured.");
        Ok(())
    }
}
#[cfg(not(windows))]
fn main() {
    println!("Windows-only paused input diagnostic.");
}
