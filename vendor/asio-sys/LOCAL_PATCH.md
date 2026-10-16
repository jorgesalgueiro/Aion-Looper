# Local asio-sys patch

Source: RustAudio/cpal asio-sys 0.4.0 (Apache-2.0), as published on crates.io.
The upstream license is included as LICENSE. The Steinberg SDK is downloaded
separately by scripts/setup-asio.ps1 and is not vendored here.

Change: bindgen treats AsioDrivers as opaque. Under Windows GNU, its C++ base
class has tail padding that the derived class reuses. Rust repr(C) cannot model
that inheritance layout, causing E0080 in the generated class offset check.
Only helpers.cpp constructs this helper class; Rust never reads its fields.
Concrete ASIO C ABI structures retain generated layout checks.
