//! Load the shipped library through its exported C ABI, with no audio devices.
#[path = "../tests/common/mod.rs"]
mod common;
use clap_sys::{entry::clap_plugin_entry, process::CLAP_PROCESS_CONTINUE};
use common::*;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let path = std::env::args_os()
        .nth(1)
        .ok_or("Usage: host <plugin.clap or shared library>")?;
    let path = std::fs::canonicalize(path)?;
    // Only load the project artifact explicitly supplied by the caller.
    let library = unsafe { libloading::Library::new(&path) }?;
    let entry = unsafe { library.get::<*const clap_plugin_entry>(b"clap_entry\0")? };
    let entry = unsafe { &**entry };
    let a = Harness::new(entry);
    let b = Harness::new(entry);
    assert!(a.activate(128));
    assert!(a.start());
    assert!(b.activate(128));
    assert!(b.start());
    let (status, left, right) = a.render(
        &[0.1, 0.2, 0.3, 0.4, 0.0, 0.0, 0.0, 0.0],
        &[-0.1, -0.2, -0.3, -0.4, 0.0, 0.0, 0.0, 0.0],
        &mut Events::new(&[(0, 100, 1.0), (4, 100, 2.0)]),
        true,
    );
    assert_eq!(status, CLAP_PROCESS_CONTINUE);
    assert_eq!(left, [0.0, 0.0, 0.0, 0.0, 0.1, 0.2, 0.3, 0.4]);
    assert_eq!(right, left.iter().map(|v| -v).collect::<Vec<_>>());
    assert_eq!(
        b.render(&[0.0; 4], &[0.0; 4], &mut Events::new(&[]), false)
            .1,
        [0.0; 4]
    );
    a.stop();
    a.deactivate();
    assert!(a.activate(64));
    assert!(a.start());
    assert_eq!(
        a.render(&[0.0; 4], &[0.0; 4], &mut Events::new(&[]), false)
            .1,
        [0.1, 0.2, 0.3, 0.4]
    );
    a.reset();
    assert_eq!(
        a.render(&[0.0; 4], &[0.0; 4], &mut Events::new(&[]), false)
            .1,
        [0.0; 4]
    );
    assert_eq!(
        a.render(
            &[0.0; 4],
            &[0.0; 4],
            &mut Events::new(&[(0, 100, 3.0)]),
            false
        )
        .1,
        [0.1, 0.2, 0.3, 0.4]
    );
    let mut state = StateBytes::new(Vec::new());
    assert!(a.save(&mut state));
    let restored = Harness::new(entry);
    assert!(restored.load(&mut StateBytes::new(state.data.clone())));
    assert!(restored.activate(64));
    assert!(restored.start());
    assert_eq!(
        restored
            .render(&[0.0; 4], &[0.0; 4], &mut Events::new(&[]), false)
            .1,
        [0.1, 0.2, 0.3, 0.4]
    );
    // Active load requests a host restart; this deterministic host applies it explicitly.
    assert!(b.load(&mut StateBytes::new(state.data)));
    b.stop();
    b.deactivate();
    assert!(b.activate(64));
    assert!(b.start());
    assert_eq!(
        b.render(&[0.0; 4], &[0.0; 4], &mut Events::new(&[]), false)
            .1,
        [0.1, 0.2, 0.3, 0.4]
    );
    drop(restored);
    drop(a);
    drop(b);
    unsafe {
        entry.deinit.unwrap()();
        entry.deinit.unwrap()();
        entry.deinit.unwrap()();
    }
    println!("{{\"ok\":true,\"exported_factory\":true,\"stereo_capture_playback\":true,\"in_place\":true,\"instance_isolation\":true,\"reactivation_retains_audio\":true,\"reset_retains_take\":true,\"state_roundtrip\":true,\"active_state_restart\":true}}");
    Ok(())
}
