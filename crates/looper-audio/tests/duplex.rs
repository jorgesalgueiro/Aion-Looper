use looper_audio::{
    capture,
    duplex::{self, Control},
};
use looper_core::{Command, Engine, State, MAX_TRACKS};
fn setup(capacity: usize) -> (capture::InputWriter, duplex::Controller, duplex::Renderer) {
    let engine =
        Engine::with_capacities(std::array::from_fn(|i| if i == 0 { capacity } else { 0 }))
            .unwrap();
    let (writer, reader) = capture::connect(1024).unwrap();
    let (controller, renderer) = duplex::connect(engine, reader, writer.metrics(), 1.0).unwrap();
    (writer, controller, renderer)
}
fn control(c: &mut duplex::Controller, command: Command) {
    c.send(Control::Track { track: 0, command }).unwrap();
}
fn render(
    writer: &mut capture::InputWriter,
    renderer: &mut duplex::Renderer,
    input: &[f32],
) -> Vec<f32> {
    writer.capture(input, 2);
    let mut output = vec![0.0; input.len()];
    renderer.render(&mut output, 2);
    output
}
#[test]
fn native_record_play_overdub_substitute_and_mute_are_sample_exact() {
    let (mut w, mut c, mut r) = setup(64);
    let take = [0.125, -0.25, 0.375, -0.5, 0.25, -0.125];
    control(&mut c, Command::Record);
    assert_eq!(render(&mut w, &mut r, &take), [0.0; 6]);
    c.send(Control::FinishStopped { track: 0 }).unwrap();
    render(&mut w, &mut r, &[0.0; 2]);
    assert_eq!(c.status().tracks[0].state, State::Stopped);
    assert_eq!(c.status().tracks[0].length_frames, 3);
    control(&mut c, Command::Play);
    assert_eq!(render(&mut w, &mut r, &[0.0; 6]), take);
    control(&mut c, Command::Feedback(0.5));
    control(&mut c, Command::Overdub);
    let punch = [0.125, 0.125, -0.125, -0.125, 0.25, 0.25];
    assert_eq!(render(&mut w, &mut r, &punch), take);
    c.send(Control::FinishStopped { track: 0 }).unwrap();
    render(&mut w, &mut r, &[0.0; 2]);
    control(&mut c, Command::Play);
    let mixed: Vec<_> = take
        .iter()
        .zip(punch)
        .map(|(old, new)| old * 0.5 + new)
        .collect();
    assert_eq!(render(&mut w, &mut r, &[0.0; 6]), mixed);
    control(&mut c, Command::Substitute);
    let replacement = [0.5, 0.25, 0.125, -0.125, -0.25, -0.5];
    render(&mut w, &mut r, &replacement);
    c.send(Control::FinishStopped { track: 0 }).unwrap();
    render(&mut w, &mut r, &[0.0; 2]);
    control(&mut c, Command::Play);
    assert_eq!(render(&mut w, &mut r, &[0.0; 6]), replacement);
    control(&mut c, Command::Mute(true));
    assert_eq!(render(&mut w, &mut r, &[0.0; 6]), [0.0; 6]);
    control(&mut c, Command::Clear);
    render(&mut w, &mut r, &[0.0; 2]);
    assert_eq!(c.status().tracks[0].state, State::Empty);
    let status = c.status();
    assert_eq!(status.command_errors, 0);
    assert_eq!(status.underruns, 0);
    assert_eq!(status.discontinuities, 0);
}
#[test]
fn full_takes_finish_without_allocating_more_storage_and_input_gaps_are_reported() {
    let (mut w, mut c, mut r) = setup(4);
    control(&mut c, Command::Record);
    render(&mut w, &mut r, &[0.125; 8]);
    assert_eq!(c.status().tracks[0].state, State::Playing);
    assert_eq!(c.status().tracks[0].length_frames, 4);
    let mut out = [0.0; 8];
    r.render(&mut out, 2);
    assert_eq!(out, [0.125; 8]);
    assert_eq!(c.status().underruns, 4);
    assert!(c
        .send(Control::Track {
            track: MAX_TRACKS,
            command: Command::Record
        })
        .is_err());
    assert!(c.send(Control::Master(f32::NAN)).is_err());
    assert!(c.send(Control::InputGain(-1.0)).is_err());
    assert!(c
        .send(Control::Track {
            track: 0,
            command: Command::Feedback(1.01)
        })
        .is_err());
}
#[test]
fn duplex_control_work_is_bounded_and_malformed_output_is_silenced() {
    let (mut w, mut c, mut r) = setup(4);
    for _ in 0..256 {
        c.send(Control::Monitor(false)).unwrap();
    }
    assert!(c.send(Control::Monitor(false)).is_err());
    render(&mut w, &mut r, &[0.0; 2]);
    for _ in 0..64 {
        c.send(Control::Monitor(false)).unwrap();
    }
    assert!(c.send(Control::Monitor(false)).is_err());
    let mut bad = [1.0_f32; 3];
    r.render(&mut bad, 2);
    assert_eq!(bad, [0.0; 3]);
    assert_eq!(c.status().malformed, 1);
}
#[test]
fn native_monitor_starts_off_and_master_zero_is_silent_without_browser_nodes() {
    let (mut w, reader) = capture::connect(2048).unwrap();
    let engine = Engine::new(8).unwrap();
    let (mut c, mut r) = duplex::connect(engine, reader, w.metrics(), 0.0).unwrap();
    c.send(Control::Monitor(true)).unwrap();
    let input = [0.5_f32; 2048];
    w.capture(&input, 2);
    let mut output = [1.0_f32; 2048];
    r.render(&mut output, 2);
    assert!(output.iter().all(|v| *v == 0.0));
    assert_eq!(c.status().input_peak, 0.5);
    c.send(Control::Master(0.5)).unwrap();
    w.capture(&input, 2);
    r.render(&mut output, 2);
    assert!(output.last().unwrap() > &0.24);
    assert!(output.iter().all(|v| v.is_finite() && v.abs() <= 0.5));
}

#[test]
fn checkpoint_snapshots_remain_coherent_during_concurrent_playback_and_reject_capture() {
    use std::sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    };
    use std::time::{Duration, Instant};
    let (mut engine, checkpoint) =
        looper_core::checkpoint::Checkpoint::prepare(std::array::from_fn(|i| {
            if i < 2 {
                100_000
            } else {
                0
            }
        }))
        .unwrap();
    let take = vec![[0.125, -0.25]; 80_000];
    engine.load_stopped(0, &take).unwrap();
    checkpoint.publish(&engine);
    let (mut writer, reader) = capture::connect(1024).unwrap();
    let (mut controller, mut renderer) =
        duplex::connect_checkpoint(engine, reader, writer.metrics(), 0.0, Some(checkpoint))
            .unwrap();
    control(&mut controller, Command::Play);
    let done = Arc::new(AtomicBool::new(false));
    let stop = Arc::clone(&done);
    let audio = std::thread::spawn(move || {
        let mut output = [0.0; 256];
        for _ in 0..2000 {
            if stop.load(Ordering::Relaxed) {
                break;
            }
            writer.capture(&[0.125; 256], 2);
            renderer.render(&mut output, 2);
            std::thread::sleep(Duration::from_millis(1));
        }
    });
    let deadline = Instant::now() + Duration::from_secs(2);
    while controller.status().frames == 0 && Instant::now() < deadline {
        std::thread::yield_now();
    }
    let before = controller.status().frames;
    let mut coherent = 0;
    for _ in 0..10 {
        if let Ok(snapshot) = controller.snapshot(200_000) {
            assert_eq!(snapshot[0].audio, take);
            coherent += 1;
        }
    }
    controller
        .send(Control::Track {
            track: 1,
            command: Command::Record,
        })
        .unwrap();
    while controller.status().tracks[1].state != State::Recording && Instant::now() < deadline {
        std::thread::yield_now();
    }
    let rejected = controller.snapshot(200_000).is_err();
    let after = controller.status().frames;
    done.store(true, Ordering::Relaxed);
    audio.join().unwrap();
    assert!(coherent > 0);
    assert!(
        after > before,
        "Audio must advance while snapshotting on the worker"
    );
    assert!(
        rejected,
        "Active capture cannot be saved as a completed take"
    );
}

#[test]
fn imported_take_commits_without_restarting_other_loops_and_failed_imports_keep_them() {
    let (mut engine, checkpoint) =
        looper_core::checkpoint::Checkpoint::prepare([128; MAX_TRACKS]).unwrap();
    engine.load_stopped(0, &[[0.25, -0.25]; 100]).unwrap();
    checkpoint.publish(&engine);
    let old = std::sync::Arc::clone(&checkpoint);
    let (mut writer, reader) = capture::connect(1024).unwrap();
    let (mut controller, mut renderer) =
        duplex::connect_checkpoint(engine, reader, writer.metrics(), 1.0, Some(checkpoint))
            .unwrap();
    control(&mut controller, Command::Play);
    render(&mut writer, &mut renderer, &[0.0; 14]);
    let position = controller.status().tracks[0].position_frames;
    let clock = controller.status().frames;
    let take = [[0.125, -0.375]; 20];
    controller.begin_import(1, &take).unwrap();
    assert!(
        controller.snapshot(2560).is_err(),
        "No stale checkpoint can be saved during a pending import"
    );
    render(&mut writer, &mut renderer, &[0.0; 10]);
    assert!(controller.finish_import().unwrap().is_ok());
    assert_eq!(controller.status().frames, clock + 5);
    assert_eq!(controller.status().tracks[0].position_frames, position + 5);
    assert_eq!(controller.status().tracks[0].state, State::Playing);
    assert_eq!(controller.status().tracks[1].state, State::Stopped);
    assert_eq!(controller.snapshot(2560).unwrap()[1].audio, take);
    assert!(
        old.snapshot(2560, || ()).is_err(),
        "The retired checkpoint must reject reads"
    );
    controller.begin_import(0, &take).unwrap();
    render(&mut writer, &mut renderer, &[0.0; 2]);
    assert!(controller.finish_import().unwrap().is_err());
    assert_eq!(
        controller.snapshot(2560).unwrap()[0].audio,
        [[0.25, -0.25]; 100]
    );
    controller.begin_import(1, &[[0.9; 2]; 10]).unwrap();
    assert!(controller.cancel_import());
    render(&mut writer, &mut renderer, &[0.0; 2]);
    assert!(controller.finish_import().unwrap().is_err());
    assert_eq!(controller.snapshot(2560).unwrap()[1].audio, take);
    controller
        .send(Control::Track {
            track: 2,
            command: Command::Record,
        })
        .unwrap();
    render(&mut writer, &mut renderer, &[0.1, 0.2, 0.3, 0.4]);
    controller.begin_import(1, &take).unwrap();
    render(&mut writer, &mut renderer, &[0.5, 0.6]);
    assert!(controller.finish_import().unwrap().is_ok());
    assert!(
        controller.snapshot(2560).is_err(),
        "The new checkpoint must still reject an active recording on another track"
    );
    controller
        .send(Control::FinishStopped { track: 2 })
        .unwrap();
    render(&mut writer, &mut renderer, &[0.0; 2]);
    assert_eq!(
        controller.snapshot(2560).unwrap()[2].audio,
        [[0.1, 0.2], [0.3, 0.4], [0.5, 0.6]],
        "Import must preserve another track's captured samples across checkpoint replacement"
    );
    assert!(controller.begin_import(1, &[[f32::NAN, 0.0]]).is_err());
    assert!(controller.begin_import(1, &[[0.0; 2]; 129]).is_err());
}
