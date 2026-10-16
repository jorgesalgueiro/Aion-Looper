fn main() {
    println!("cargo:rerun-if-changed=native/stretch.cpp");
    println!("cargo:rerun-if-changed=vendor/rubberband-4.0.0");
    let mut build = cc::Build::new();
    build
        .cpp(true)
        .std("c++17")
        .warnings(false)
        .define("RUBBERBAND_STATIC", None)
        .include("vendor/rubberband-4.0.0")
        .file("vendor/rubberband-4.0.0/single/RubberBandSingle.cpp")
        .file("native/stretch.cpp");
    let target = std::env::var("TARGET").unwrap();
    if target.ends_with("windows-gnu") {
        build.cpp_link_stdlib(None);
        {
            let library = "libstdc++.dll.a";
            let found = std::process::Command::new(build.get_compiler().path())
                .arg(format!("-print-file-name={library}"))
                .output()
                .expect("C++ library search");
            let path = String::from_utf8(found.stdout).unwrap();
            let path = std::path::Path::new(path.trim());
            if path.is_file() {
                // Do not put a newer MinGW CRT ahead of Rust's bundled CRT.
                // Stage only the C++ runtime import archive.
                let output = std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap());
                std::fs::copy(path, output.join(library)).expect("Stage C++ runtime archive");
            }
        }
    }
    build.compile("looper_rubberband");
    if target.ends_with("windows-gnu") {
        println!("cargo:rustc-link-lib=dylib=stdc++");
    }
    if target.contains("apple") {
        println!("cargo:rustc-link-lib=framework=Accelerate");
    }
}
