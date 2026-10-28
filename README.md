# Aion-Looper

**Build a performance, one loop at a time.**

Aion-Looper is becoming a live looping instrument for Windows and Linux, with a
default workspace of **10 loop tracks, 10 drone tracks and 10 sampler tracks**,
hands-on controls, and a distinctive ASCII interface.
Record a phrase, layer another, reshape the arrangement, and turn a small idea
into a whole piece — from a guitar or voice to a keyboard or an ambient texture.

It brings the character of Jorge Salgueiro's **Ascii-Looper** into a native
C++ audio engine, with a Standalone app and plugins for your DAW. The long-term
vision combines looping, drones, sampling, sequencing and effects in one musical
workspace, playable with a mouse, keyboard, MIDI controller or command line.

> **Current status: native development preview, version 0.4.3 plus ongoing source
> updates.** The core looper is playable; the complete workstation is still being
> ported. Older downloadable builds may lack features described here. For the
> current implementation, build from source. **Recorded audio is temporary and
> is not yet saved inside projects or DAW sessions. Export loops before closing.**

## What you can play today

- **Stereo live looping.** Record, play, stop, overdub, replace with SUB, mute,
  solo and shape each loop with volume, pan and feedback.
- **Play freely or follow a grid.** OFF, STATIC and DYNAMIC recording modes,
  internal master BPM, bars and meter, a metronome, and independent visual and
  audible count-in.
- **Reshape a phrase while performing.** Reverse on the next play/retrigger,
  change speed, shift or quantize the start, retrigger, and hold STUT to repeat
  a fragment before returning to the full loop.
- **Build and recover arrangements.** MULT extends or doubles loops; MERGE
  combines them. Normalize a quiet take, or use per-loop and global Undo/Redo
  for supported audio edits.
- **See and keep your takes.** Recorded-loop waveforms, a DYNAMIC recording
  trace, editable loop names, and WAV export of each loop's stored audio.
- **Perform with MIDI.** Note, CC and Program Change looper actions, MIDI Learn
  on supported loop controls, CC volume/pan control, and saved MIDI mappings.
- **Control the running app from a terminal.** The development CLI shares the
  Standalone's session, devices and native command layer.

The original ASCII layout, loop cards and familiar controls remain the design
contract. Controls whose engines have not been ported stay disabled. Drone voices,
sampler/tracker/sequencing engines, effects, full project persistence, remaining
import/export workflows and full DAW/MIDI-clock following are still ahead.
See the [feature-by-feature parity checklist](docs/PARITY.md) for exact coverage.

## Choose how to run it

| Platform | Standalone | Plugin formats in scope | Validation / availability |
| --- | --- | --- | --- |
| Windows x64 | Yes | VST3, CLAP | Development builds; scoped REAPER checks recorded |
| Linux | Yes | CLAP, LV2 | Development builds; scoped Ardour LV2 checks recorded |
| macOS | Future | AU | Build configuration prepared; platform acceptance pending |
| iOS | Future | AUv3 | Build configuration prepared; device acceptance pending |

Format availability does not mean every workflow has passed in every host: [validation notes](docs/VALIDATION.md) describe
the checks actually run. Apple delivery is future work.

### Your first loop: Windows or Linux

1. Launch **Aion-Looper Standalone**. For Linux, source-build instructions are below.
   Windows currently needs the **WebView2 Evergreen Runtime** (a separate embedded
   runtime, not the Edge browser);
   Linux needs the GTK/WebKit runtime libraries installed with the build dependencies.
2. Open the window's **Options** and select your audio input/output device and
   buffer size. Enable **Unmute Input** in the Standalone. Start with a buffer
   your device handles reliably; 64/128 samples are development targets, not a
   guarantee of total latency.
3. Raise **MASTER VOLUME** gradually: output starts muted and input monitoring
   starts off. Choose **OFF** sync for a simple free-length first recording.
4. Press a loop's main button to record, then **FINISH** to end the take. Use
   **PLAY** if Auto-play after Rec is off. Layer with OD/SUB or record another slot.
5. Click the recorded waveform to export the stored loop to WAV. This exports
   the recorded audio, rather than a rendered mix of playback controls/effects.

The default number keys **1–0** address the ten default loop tracks.
**Space / STOP ALL** stops performance and cancels queued starts. On a
playing loop's main STOP button or assigned number key, a **600 ms hold cuts
playback while keeping audio**; a continuous **4-second hold deletes the loop
and its history**. **DEL** deletes directly.

Same-rate audio-device reinitialization retains recorded audio but stops loops;
changing the sample rate clears recorded audio. Export important takes first.
The in-app manuals explain the original controls and workflows.

### Use it in a DAW

Build the plugin formats below, then add the relevant output folder to your
host's plugin search paths and rescan:

| System | Build output folders | Suggested workflow |
| --- | --- | --- |
| Windows | `build/windows/AionLooper_artefacts/Release/VST3` and `build/windows/AionLooper_artefacts/Release/CLAP` | Load VST3 or CLAP in REAPER |
| Linux | `build/linux/AionLooper_artefacts/Release/LV2` and `build/linux/AionLooper_artefacts/Release/CLAP` | Load LV2 in Ardour; use a CLAP-capable host for CLAP |

Route the track's audio and MIDI into the plugin, open its editor, and raise
MASTER VOLUME. The plugin uses the **DAW's routing and devices**; configure
physical inputs in the DAW. Host transport information can be observed, but full
musical host synchronization and MIDI Clock are not implemented yet: use the
internal master grid for the currently supported timing workflow.

DAW session recall does **not** currently restore recorded loop audio. Export
your takes separately. Plugin installation and host-specific validation details
are in the [build guide](docs/BUILDING.md) and [validation notes](docs/VALIDATION.md).

### Use it from the command line

Start a freshly built Standalone, then run these commands from the repository
with Python 3.8+ (`python3` on Linux):

```sh
python tools/aion.py instances
python tools/aion.py status
python tools/aion.py parameter masterGain 0.5
python tools/aion.py loop record 1
python tools/aion.py loop finish 1
python tools/aion.py loop stopImmediate 1
python tools/aion.py rename 1 "First idea"
```

The default loop tracks are numbered **1–10**. The development engine also retains
additional internal slots for compatibility; see the CLI guide for command limits.
With several Standalone instances open,
select one with `--instance INSTANCE_ID`. This client controls the already
running app on the same computer and OS user; it does not control DAW plugin
instances. It is a development tool, currently distributed as source.

The goal is CLI access alongside every newly ported feature. Current commands,
MIDI Learn examples, JSON requests and result/timeout behavior are documented in
the [CLI guide](docs/CLI.md).

## Build from source on Linux

You need **Git, CMake 3.22+, a C++20 compiler and Python 3.8+**. Dependencies are
downloaded at pinned revisions on the first configure. Node.js is optional for
additional frontend/reference tests. Run the following commands from a terminal:

```sh
git clone https://github.com/jorgesalgueiro/Aion-Looper.git
cd Aion-Looper
```

### Ubuntu / Debian-based Linux

These package names match the Ubuntu build setup; other distributions may use
different names. Ninja is the generator used by the Linux preset.

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git python3 pkg-config \
  libasound2-dev libjack-jackd2-dev libfreetype6-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev libgl1-mesa-dev libegl1-mesa-dev \
  libwebkit2gtk-4.1-dev libgtk-3-dev
cmake --preset linux
cmake --build --preset linux --parallel 2
ctest --preset linux
./build/linux/AionLooper_artefacts/Release/Standalone/Aion-Looper
```

This builds Standalone, CLAP and LV2. ALSA/JACK
development libraries enable the corresponding JUCE backends; physical device
and low-latency performance still need testing with your own setup.

Additional development commands, core tests and dependency overrides are in the
[build guide](docs/BUILDING.md). Packages follow feature acceptance; development
builds are not release-ready products.

## Under the hood

| Layer | Technology and purpose |
| --- | --- |
| Audio and plugin framework | **C++20 + JUCE 9.0.4**, with CMake presets and pinned dependency revisions |
| Interface | Original **HTML/CSS/JavaScript ASCII presentation**, embedded as offline assets in `juce::WebBrowserComponent`; WebView2 on Windows and WebKit on Linux |
| Native integration | Validated JS/C++ commands and state synchronization; `AudioProcessorValueTreeState` for host parameters and state |
| Audio engine | Preallocated capture storage, bounded queues, sample-based scheduling, smoothing, and worker-side audio edits/finalization |
| Plugin formats | JUCE wrappers, **clap-juce-extensions** for CLAP, and JUCE's native LV2 support |
| Verification | Native tests, unchanged-JavaScript comparisons, allocation checks, UI/PCM workflows and scoped plugin-validator/DAW checks |

Audio processing belongs in C++; JavaScript handles presentation and interaction.
The engine is being tested at 64/128-sample blocks with variable timing and
lifecycle cases. Passing these tests does not establish full sound parity,
physical round-trip latency or production readiness.

## Roadmap

The order below reflects the migration priorities, not promised release dates.
Each slice must pass its own reference, audio and interaction checks before the
next one advances.

| Milestone | What it brings | Current status / next gate |
| --- | --- | --- |
| Native looper foundation | Default ten-loop workspace, capture/playback, OD/SUB, mixer, history, MULT/MERGE, WAV export | Implemented for documented preview scope; broader hardware and long-session acceptance remains |
| Faithful performance and feedback | Original gestures, waveforms, live traces, timing and visual meters | Many controls verified; queued source transitions, overlap/late phase, remaining metering and full browser sound comparisons continue |
| CLI coverage | The same musical actions available to scripts and terminal users | Running-Standalone development client implemented; extend alongside new ports and bundle later |
| Projects, editing and routing | Save/reload audio sessions, remaining imports/exports and routing workflows | Pending; recorded audio currently remains session-only |
| Full instrument workspace | Default ten-drone and ten-sampler banks, tracker/sequencing and effects | Pending native ports and reference parity checks |
| Connected performance | Full host/MIDI-clock sync, wider MIDI Learn, controller workflows and HID/gamepad adapter | Looper MIDI and selected CC Learn implemented; clock, remaining targets and device coverage pending |
| Windows and Linux releases | Reliable Standalone/plugins, real-device stress, host workflows, installers and distribution | Development builds and scoped checks exist; release acceptance and licensing work remain |
| macOS and iOS | Standalone/AU on macOS; Standalone/AUv3 on iOS | Future phase; configuration prepared, Apple builds/devices/signing not accepted |

## Follow the project

- [Migration status](docs/PARITY.md): what's implemented and what's missing.
- [Current engineering handoff](docs/MIGRATION_CURRENT.md): the active slice and next steps.
- [Validation](docs/VALIDATION.md) and [reference audit](docs/REFERENCE_AUDIT.md): evidence and limits.
- [Architecture](docs/ARCHITECTURE.md) and [migration blueprint](docs/MIGRATION_BLUEPRINT.md): how the port fits together.
- [Build guide](docs/BUILDING.md) and [CLI guide](docs/CLI.md): reproducible commands and usage.

When reporting a problem, include your OS, build/commit, Standalone or plugin
format, DAW version if applicable, audio device, sample rate, buffer size, and
the shortest steps that reproduce it.

## Credits and licensing

Aion-Looper continues Jorge Salgueiro's Ascii-Looper, preserving its visual identity
and using its original behavior as the reference for the native migration.

**Aion-Looper is free and open-source software.** Project-owned code is licensed
under **AGPL-3.0-only**; see [LICENSE](LICENSE). This choice matches JUCE 9's
open-source licensing route. You can study, modify and share the software under
those terms; distributing it includes obligations to provide corresponding source.

Original Ascii-Looper UI files retain their GPL-2.0 notices. Third-party libraries
and SDKs retain their own licenses. Rights/permissions for combining the original
GPL-2.0-only assets with the AGPL application and the remaining dependency inventory
must be checked before release; the new license does not relicense third-party
work. See [LICENSING.md](docs/LICENSING.md).
