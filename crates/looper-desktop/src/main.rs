#![forbid(unsafe_code)]
#![cfg_attr(
    all(target_os = "windows", not(debug_assertions)),
    windows_subsystem = "windows"
)]
fn main() -> Result<(), Box<dyn std::error::Error>> {
    rust_looper_desktop::run(std::env::args().skip(1).collect())
}
