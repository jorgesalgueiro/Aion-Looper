use looper_files::{decode, encode, Format};
use std::path::PathBuf;
use std::process::Command;
use std::sync::atomic::{AtomicU64, Ordering};

static NEXT: AtomicU64 = AtomicU64::new(0);

#[test]
fn recording_refuses_existing_files_and_bad_options_before_device_startup() {
    let fixture = Fixture::new();
    let before = std::fs::read(&fixture.input).unwrap();
    let output = Command::new(env!("CARGO_BIN_EXE_rust-looper"))
        .arg("record")
        .arg(&fixture.input)
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("already exists"));
    assert_eq!(std::fs::read(&fixture.input).unwrap(), before);
    let output = Command::new(env!("CARGO_BIN_EXE_rust-looper"))
        .arg("record")
        .arg(&fixture.output)
        .args(["--seconds", "NaN"])
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(!fixture.output.exists());
}

struct Fixture {
    directory: PathBuf,
    input: PathBuf,
    output: PathBuf,
}
impl Fixture {
    fn new() -> Self {
        let directory = std::env::temp_dir().join(format!(
            "rust-looper-test-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        std::fs::create_dir(&directory).unwrap();
        let input = directory.join("input.wav");
        let output = directory.join("output.wav");
        std::fs::write(
            &input,
            encode(&[[0.125, -0.25], [0.25, -0.5]], 48000, 2, Format::Float32).unwrap(),
        )
        .unwrap();
        Self {
            directory,
            input,
            output,
        }
    }
    fn command(&self) -> Command {
        let mut command = Command::new(env!("CARGO_BIN_EXE_rust-looper"));
        command.arg(&self.input).arg(&self.output);
        command
    }
}
impl Drop for Fixture {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.input);
        let _ = std::fs::remove_file(&self.output);
        let _ = std::fs::remove_dir(&self.directory);
    }
}

#[test]
fn native_sample_workflow_normalizes_reverses_and_doubles_the_take() {
    let fixture = Fixture::new();
    let original = std::fs::read(&fixture.input).unwrap();
    let output = fixture
        .command()
        .args(["--normalize", "--reverse", "--repeat", "2"])
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let wave = decode(&std::fs::read(&fixture.output).unwrap()).unwrap();
    assert_eq!(
        wave.frames,
        [[0.5, -1.0], [0.25, -0.5], [0.5, -1.0], [0.25, -0.5]]
    );
    assert_eq!(wave.sample_rate, 48000);
    assert_eq!(std::fs::read(&fixture.input).unwrap(), original);
}

#[test]
fn existing_output_is_preserved_unless_force_is_explicit() {
    let fixture = Fixture::new();
    std::fs::write(&fixture.output, b"existing-file").unwrap();
    assert!(!fixture.command().output().unwrap().status.success());
    assert_eq!(std::fs::read(&fixture.output).unwrap(), b"existing-file");
    assert!(fixture
        .command()
        .args(["--force", "--format", "pcm24"])
        .output()
        .unwrap()
        .status
        .success());
    assert_eq!(
        decode(&std::fs::read(&fixture.output).unwrap())
            .unwrap()
            .format,
        Format::Pcm24
    );
}

#[test]
fn invalid_requests_do_not_create_output_or_overwrite_the_source() {
    let fixture = Fixture::new();
    for args in [
        ["--repeat", "0"],
        ["--latency-ms", "NaN"],
        ["--format", "bogus"],
    ] {
        assert!(!fixture
            .command()
            .args(args)
            .output()
            .unwrap()
            .status
            .success());
        assert!(!fixture.output.exists());
    }
    let original = std::fs::read(&fixture.input).unwrap();
    let output = Command::new(env!("CARGO_BIN_EXE_rust-looper"))
        .arg(&fixture.input)
        .arg(&fixture.input)
        .arg("--force")
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert_eq!(std::fs::read(&fixture.input).unwrap(), original);
}
