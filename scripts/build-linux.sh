#!/usr/bin/env bash
# Creates a Linux x86_64 package; system GTK/WebKit/audio libraries are required.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/aion-looper/target-linux}"
# Resolve relative target paths before referring to built artifacts.
mkdir -p "$CARGO_TARGET_DIR"
CARGO_TARGET_DIR="$(cd "$CARGO_TARGET_DIR" && pwd)"
export CARGO_TARGET_DIR
cargo build --locked --release -p looper-desktop -p looper-cli -p looper-clap --features looper-desktop/jack,looper-cli/jack
package=dist/Aion-Looper-Linux
mkdir -p "$package"
cp "$CARGO_TARGET_DIR/release/Aion-Looper" "$package/"
cp "$CARGO_TARGET_DIR/release/rust-looper" "$package/"
cp "$CARGO_TARGET_DIR/release/liblooper_clap.so" "$package/Aion-Looper-Prototype.clap"
cp LICENSE "$package/"
cp crates/looper-stretch/vendor/rubberband-4.0.0/COPYING "$package/RubberBand-LICENSE.txt"
cp crates/looper-desktop/assets/vendor/SoundTouch-LICENSE.txt "$package/"
# Cargo metadata locates the exact dependency licenses without hardcoded registry paths.
cargo metadata --locked --format-version 1 > "$CARGO_TARGET_DIR/aion-package-metadata.json"
python3 - "$CARGO_TARGET_DIR/aion-package-metadata.json" "$package" <<'PY'
import json, pathlib, shutil, sys
packages = json.loads(pathlib.Path(sys.argv[1]).read_text())["packages"]
for name, filename in [("clap-sys", "LICENSE-MIT"), ("midir", "LICENSE")]:
    package = next(p for p in packages if p["name"] == name)
    source = pathlib.Path(package["manifest_path"]).parent / filename
    shutil.copyfile(source, pathlib.Path(sys.argv[2]) / (name + "-LICENSE.txt"))
PY
cp docs/LINUX.md "$package/README.md"
cp docs/PLUGIN_STATE.md "$package/"
cp docs/NATIVE_MODE.md docs/NATIVE_PROJECTS.md docs/NATIVE_PARITY.md docs/NATIVE_IMPORT.md "$package/"
cargo run --locked -p looper-clap --example host -- "$package/Aion-Looper-Prototype.clap"
sha256sum "$package/Aion-Looper" "$package/rust-looper" "$package/Aion-Looper-Prototype.clap"
