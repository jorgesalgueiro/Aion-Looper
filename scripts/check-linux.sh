#!/usr/bin/env bash
# Linux audit; run from Ubuntu/WSL or a native Linux checkout.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
# Keep ELF artifacts separate from the Windows target directory in shared checkouts.
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/aion-looper/target-linux}"
command -v cargo >/dev/null || { echo 'Install Rust and add ~/.cargo/bin to PATH.' >&2; exit 1; }
command -v node >/dev/null || { echo 'Install Node.js to verify the original UI and bridge.' >&2; exit 1; }
pkg-config --exists alsa jack gtk+-3.0 webkit2gtk-4.1
cargo fmt --all --check
cargo clippy --locked --workspace --all-targets -- -D warnings
cargo test --locked --workspace
cargo check --locked -p looper-cli -p looper-desktop --features jack
node --test crates/looper-desktop/tests/*.test.cjs
(cd ui && node --test tests/*.test.cjs)
