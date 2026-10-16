mod common;
use clap_sys::process::CLAP_PROCESS_CONTINUE;
use common::*;
fn plugin() -> Harness {
    Harness::new(&looper_clap::clap_entry)
}
fn recorded() -> Harness {
    let h = plugin();
    assert!(h.activate(64));
    assert!(h.start());
    h.render(
        &[2.0, -0.5, 0.0, 0.0],
        &[-0.0, 0.25, 0.0, 0.0],
        &mut Events::new(&[(0, 100, 1.0), (2, 100, 2.0)]),
        false,
    );
    h
}
fn saved(h: &Harness) -> Vec<u8> {
    let mut bytes = StateBytes::new(Vec::new());
    assert!(h.save(&mut bytes));
    bytes.data
}
#[test]
fn active_playback_saves_float_exact_takes_and_inactive_restore_replays_them() {
    let h = recorded();
    let bytes = saved(&h);
    let restored = plugin();
    assert!(restored.load(&mut StateBytes::new(bytes.clone())));
    assert_eq!(saved(&restored), bytes);
    assert_eq!(restored.value(100), 2.0);
    assert!(restored.activate(64));
    assert!(restored.start());
    let (status, l, r) = restored.render(&[0.0; 4], &[0.0; 4], &mut Events::new(&[]), false);
    assert_eq!(status, CLAP_PROCESS_CONTINUE);
    assert_eq!(l, [2.0, -0.5, 2.0, -0.5]);
    assert_eq!(r, [-0.0, 0.25, -0.0, 0.25]);
    assert_eq!(r[0].to_bits(), 0.0f32.to_bits()); // mixer arithmetic, stored bits verified by round-trip bytes
}
#[test]
fn capture_and_punch_saves_fail_without_writing_a_partial_snapshot() {
    let h = plugin();
    assert!(h.activate(64));
    assert!(h.start());
    for command in [1.0, 6.0, 7.0] {
        h.render(
            &[0.25; 2],
            &[0.5; 2],
            &mut Events::new(&[(0, 100, command)]),
            false,
        );
        let mut bytes = StateBytes::new(Vec::new());
        assert!(!h.save(&mut bytes));
        assert!(bytes.data.is_empty());
        h.render(
            &[0.0; 2],
            &[0.0; 2],
            &mut Events::new(&[(0, 100, 2.0)]),
            false,
        );
        assert!(!saved(&h).is_empty());
    }
}
#[test]
fn corrupt_truncated_unknown_and_trailing_states_leave_the_existing_session_intact() {
    let source = recorded();
    let bytes = saved(&source);
    source.stop();
    source.deactivate();
    let mut corrupt = bytes.clone();
    corrupt[220] ^= 1;
    let mut unknown = bytes.clone();
    unknown[8] = 99;
    let mut huge = bytes.clone();
    huge[204..208].copy_from_slice(&u32::MAX.to_le_bytes());
    let mut trailing = bytes.clone();
    trailing.push(1);
    for data in [
        corrupt,
        unknown,
        huge,
        trailing,
        bytes[..bytes.len() - 1].to_vec(),
        Vec::new(),
    ] {
        assert!(!source.load(&mut StateBytes::new(data)));
        assert_eq!(saved(&source), bytes);
    }
}
#[test]
fn state_stream_partial_io_errors_are_explicit_failures() {
    let h = recorded();
    let original = saved(&h);
    for (chunk, fail, invalid) in [
        (0, usize::MAX, false),
        (7, 10, false),
        (7, usize::MAX, true),
    ] {
        let mut out = StateBytes::new(Vec::new());
        out.chunk = chunk;
        out.fail_at = fail;
        out.invalid_count = invalid;
        assert!(!h.save(&mut out));
    }
    h.stop();
    h.deactivate();
    for (fail, invalid) in [(10, false), (usize::MAX, true)] {
        let mut input = StateBytes::new(original.clone());
        input.fail_at = fail;
        input.invalid_count = invalid;
        assert!(!h.load(&mut input));
        assert_eq!(saved(&h), original);
    }
}
#[test]
fn active_load_waits_for_host_restart_and_never_replaces_pcm_on_the_audio_thread() {
    let h = recorded();
    let donor = recorded();
    donor.flush(&mut Events::new(&[(0, 0, 0.5)]));
    let wanted = saved(&donor);
    assert!(h.load(&mut StateBytes::new(wanted.clone())));
    assert_eq!(h.value(0), 0.5);
    assert_eq!(saved(&h), wanted);
    // Test host deliberately defers request_restart: old audio keeps playing.
    assert_eq!(
        h.render(&[0.0; 2], &[0.0; 2], &mut Events::new(&[]), false)
            .1,
        [2.0, -0.5]
    );
    h.stop();
    h.deactivate();
    assert!(h.activate(64));
    assert!(h.start());
    assert_eq!(
        h.render(&[0.0; 2], &[0.0; 2], &mut Events::new(&[]), false)
            .1,
        [1.0, -0.25]
    );
    assert_eq!(saved(&h), wanted);
}
#[test]
fn never_activated_state_and_parameter_restore_do_not_retrigger_loop_commands() {
    let a = plugin();
    a.flush(&mut Events::new(&[
        (0, 0, 0.5),
        (0, 1, 0.25),
        (0, 100, 1.0),
    ]));
    let bytes = saved(&a);
    let b = plugin();
    assert!(b.load(&mut StateBytes::new(bytes.clone())));
    assert_eq!(b.value(0), 0.5);
    assert_eq!(saved(&b), bytes);
    assert!(b.activate(64));
    assert!(b.start());
    assert_eq!(
        b.render(&[1.0; 2], &[1.0; 2], &mut Events::new(&[]), false)
            .1,
        [0.125; 2]
    );
    // If the restored command parameter had executed Record, this save would fail.
    assert!(!saved(&b).is_empty());
}
#[test]
fn saves_remain_coherent_while_an_audio_thread_plays_the_same_take() {
    use std::sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    };
    let h = recorded();
    let expected = saved(&h);
    let plugin = h.plugin as usize;
    let done = Arc::new(AtomicBool::new(false));
    let finished = Arc::clone(&done);
    let worker = std::thread::spawn(move || {
        let plugin = plugin as *const clap_sys::plugin::clap_plugin;
        let mut l = [0.0; 64];
        let mut r = [0.0; 64];
        let mut p = [l.as_mut_ptr(), r.as_mut_ptr()];
        let input = buffer(&mut p);
        let mut o = p;
        let mut output = buffer(&mut o);
        let mut events = Events::new(&[]);
        let list = events.list();
        let c = call(64, &input, &mut output, &list);
        while !finished.load(Ordering::Acquire) {
            assert_eq!(
                unsafe { (*plugin).process.unwrap()(plugin, &c) },
                CLAP_PROCESS_CONTINUE
            );
        }
    });
    for _ in 0..50 {
        assert_eq!(saved(&h), expected);
    }
    done.store(true, Ordering::Release);
    worker.join().unwrap();
}
#[test]
fn gain_automation_does_not_prevent_saving_unchanged_pcm() {
    use std::sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    };
    let h = recorded();
    let expected = saved(&h);
    let plugin = h.plugin as usize;
    let done = Arc::new(AtomicBool::new(false));
    let finished = Arc::clone(&done);
    let worker = std::thread::spawn(move || {
        let plugin = plugin as *const clap_sys::plugin::clap_plugin;
        let mut l = [0.0; 64];
        let mut r = [0.0; 64];
        let mut pointers = [l.as_mut_ptr(), r.as_mut_ptr()];
        let input = buffer(&mut pointers);
        let mut outputs = pointers;
        let mut output = buffer(&mut outputs);
        let mut events = Events::new(&[(0, 0, 0.5)]);
        while !finished.load(Ordering::Acquire) {
            events.0[0].value = if events.0[0].value == 0.5 { 0.25 } else { 0.5 };
            let list = events.list();
            let c = call(64, &input, &mut output, &list);
            assert_eq!(
                unsafe { (*plugin).process.unwrap()(plugin, &c) },
                CLAP_PROCESS_CONTINUE
            );
        }
    });
    for _ in 0..50 {
        let bytes = saved(&h);
        assert_eq!(
            &bytes[32..bytes.len() - 8],
            &expected[32..expected.len() - 8]
        );
        let gain = f64::from_le_bytes(bytes[24..32].try_into().unwrap());
        assert!([0.25, 0.5, 1.0].contains(&gain));
    }
    done.store(true, Ordering::Release);
    worker.join().unwrap();
}
