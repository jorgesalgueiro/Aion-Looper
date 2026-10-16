use std::{env, fs, path::PathBuf, process::Command};
fn main() {
    println!("cargo:rerun-if-changed=../../ui/favicon.ico");
    println!("cargo:rerun-if-env-changed=WINDRES");
    println!("cargo:rerun-if-env-changed=RC");
    if env::var("CARGO_CFG_TARGET_OS").as_deref() != Ok("windows") {
        return;
    }
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let icon = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap())
        .join("../../ui/favicon.ico")
        .canonicalize()
        .expect("Original Ascii-Looper favicon is missing");
    let icon = icon
        .to_string_lossy()
        .trim_start_matches(r"\\?\")
        .replace('\\', "/");
    let resource = out.join("app-icon.rc");
    fs::write(&resource, format!("1 ICON \"{icon}\"\n")).unwrap();
    let msvc = env::var("CARGO_CFG_TARGET_ENV").as_deref() == Ok("msvc");
    let artifact = out.join(if msvc { "app-icon.res" } else { "app-icon.o" });
    let mut command = if msvc {
        let explicit = env::var_os("RC").map(PathBuf::from);
        let compiler = explicit
            .or_else(|| {
                let kits = env::var_os("WindowsSdkDir")
                    .map(PathBuf::from)
                    .or_else(|| {
                        env::var_os("ProgramFiles(x86)")
                            .map(|root| PathBuf::from(root).join("Windows Kits/10"))
                    })?;
                let mut versions: Vec<_> = fs::read_dir(kits.join("bin"))
                    .ok()?
                    .filter_map(Result::ok)
                    .map(|entry| entry.path())
                    .collect();
                versions.sort();
                versions.reverse();
                versions
                    .into_iter()
                    .map(|version| version.join("x64/rc.exe"))
                    .find(|path| path.is_file())
            })
            .unwrap_or_else(|| PathBuf::from("rc.exe"));
        let mut command = Command::new(compiler);
        command
            .arg("/nologo")
            .arg("/c65001")
            .arg("/fo")
            .arg(&artifact)
            .arg(&resource);
        command
    } else {
        let mut command = Command::new(env::var_os("WINDRES").unwrap_or_else(|| "windres".into()));
        command
            .arg("--input")
            .arg(&resource)
            .arg("--output")
            .arg(&artifact)
            .arg("--output-format=coff");
        command
    };
    let status = command.status().expect(
        "Windows icon resource compiler unavailable. Install the Windows SDK or MinGW windres.",
    );
    assert!(status.success(), "Windows icon compilation failed");
    println!(
        "cargo:rustc-link-arg-bin=Aion-Looper={}",
        artifact.display()
    );
}
