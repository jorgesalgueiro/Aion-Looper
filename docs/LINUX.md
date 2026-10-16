# Linux and Ubuntu/WSL

Ubuntu is installed on this PC as the WSL2 distribution `Ubuntu` (24.04).
The active source remains `D:\Jorge\Documents\Codex\Aion-Looper`, available
inside Ubuntu as `/mnt/d/Jorge/Documents/Codex/Aion-Looper`.
Linux artifacts use `~/.cache/aion-looper/target-linux` by default, separate from
the Windows GNU artifacts. Override `CARGO_TARGET_DIR` if needed.

Install Rust with rustup (stable, rustfmt and clippy), Node.js, Python 3 and:

```sh
sudo apt-get update
sudo apt-get install -y build-essential pkg-config libasound2-dev \
  libjack-jackd2-dev libgtk-3-dev libwebkit2gtk-4.1-dev nodejs python3
```

From Ubuntu:

```sh
cd /mnt/d/Jorge/Documents/Codex/Aion-Looper
. "$HOME/.cargo/env"
bash scripts/check-linux.sh
bash scripts/build-linux.sh
```

The build script creates `dist/Aion-Looper-Linux/` with the desktop executable,
the native CLI (`rust-looper`) and `Aion-Looper-Prototype.clap`, then loads the
packaged plugin through its exported CLAP ABI. This package requires the system
GTK3, WebKitGTK 4.1, ALSA and JACK libraries; it is not a self-contained AppImage.
The CLAP prototype is an audio effect with the host's generic parameter editor.
It does not yet include the original editor, MIDI instrument or synth/FX.

Run `./dist/Aion-Looper-Linux/Aion-Looper` in a graphical Linux session or WSLg.
Run `./dist/Aion-Looper-Linux/rust-looper devices` for native device enumeration.
JACK is enabled in both standalone executables; select the host explicitly when
using the CLI (`devices --host JACK`). A running JACK server and configured audio
routing are required for device operation. WSLg audio is virtualized and does not
validate native Linux hardware latency, ASIO or CoreAudio.

A Linux build does not validate macOS or iOS. The `ios-core` CI job checks the
workspace against the iOS target. Wry/Tao have
iOS support, but this repository does not yet package an installable iOS app;
audio-session integration, signing and device testing remain separate work.

Run `bash scripts/test-desktop-linux.sh` in a graphical session for the muted,
synthetic desktop integration suite. It never opens a physical microphone.

For opt-in WSLg microphone checks, use the process-scoped ALSA configuration
and command in [input testing](INPUT_TESTING.md).

