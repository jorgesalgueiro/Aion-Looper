use looper_audio::realtime::{connect, Control, COMMANDS_PER_CALLBACK, CONTROL_CAPACITY};
use looper_core::schedule::ScheduledCommand;
use looper_core::{Command, Engine, MAX_TRACKS};

fn loaded() -> (
    looper_audio::realtime::Controller,
    looper_audio::realtime::Renderer,
) {
    let mut engine = Engine::new(16).unwrap();
    engine
        .load_stopped(0, &[[0.5, -0.25], [0.25, -0.5]])
        .unwrap();
    let (mut controller, renderer) = connect(engine);
    controller
        .send(Control::Now {
            track: 0,
            command: Command::Play,
        })
        .unwrap();
    (controller, renderer)
}

#[test]
fn master_ramp_is_sample_accurate_across_callback_boundaries() {
    let (mut a, mut ar) = loaded();
    let (mut b, mut br) = loaded();
    for controller in [&mut a, &mut b] {
        controller
            .send(Control::MasterGain {
                gain: 1.0,
                ramp_frames: 4,
            })
            .unwrap();
    }
    let mut whole = [0.0_f32; 8];
    ar.render(&[], &mut whole, 2);
    let mut split = [0.0_f32; 8];
    br.render(&[], &mut split[..4], 2);
    br.render(&[], &mut split[4..], 2);
    assert_eq!(whole, split);
    assert_eq!(whole[0], 0.125);
    assert_eq!(whole[6], 0.25);
    assert_eq!(a.status().frames, 4);
}

#[test]
fn bounded_control_work_leaves_excess_for_later_callbacks() {
    let (mut controller, mut renderer) = connect(Engine::new(8).unwrap());
    for _ in 0..CONTROL_CAPACITY {
        controller
            .send(Control::Now {
                track: MAX_TRACKS,
                command: Command::Clear,
            })
            .unwrap();
    }
    assert!(controller.send(Control::Cancel { track: 0 }).is_err());
    for callback in 1..=CONTROL_CAPACITY / COMMANDS_PER_CALLBACK {
        renderer.render(&[], &mut [0.0_f32; 2], 2);
        assert_eq!(
            controller.status().command_errors,
            (callback * COMMANDS_PER_CALLBACK) as u64
        );
    }
    assert!(controller.send(Control::Cancel { track: 0 }).is_ok());
}

#[test]
fn scheduled_and_immediate_rejections_are_counted() {
    let (mut controller, mut renderer) = connect(Engine::new(16).unwrap());
    controller
        .send(Control::At(ScheduledCommand {
            frame: 2,
            track: 0,
            command: Command::Play,
        }))
        .unwrap();
    renderer.render(&[], &mut [0.0_f32; 8], 2);
    assert_eq!(controller.status().command_errors, 1);
    controller
        .send(Control::At(ScheduledCommand {
            frame: 0,
            track: 0,
            command: Command::Record,
        }))
        .unwrap();
    renderer.render(&[], &mut [0.0_f32; 2], 2);
    assert_eq!(controller.status().command_errors, 2);
}

#[test]
fn callback_routing_supports_mono_and_integer_device_samples() {
    let (mut controller, mut renderer) = loaded();
    controller
        .send(Control::MasterGain {
            gain: 1.0,
            ramp_frames: 0,
        })
        .unwrap();
    let mut mono = [0.0_f32; 2];
    renderer.render(&[], &mut mono, 1);
    assert_eq!(mono, [0.125, -0.125]);
    let mut integer = [0_i16; 4];
    renderer.render(&[], &mut integer, 2);
    assert_eq!(integer, [16384, -8192, 8192, -16384]);
}

#[test]
fn malformed_blocks_are_silent_and_do_not_advance_the_engine() {
    let (mut controller, mut renderer) = loaded();
    controller
        .send(Control::MasterGain {
            gain: 1.0,
            ramp_frames: 0,
        })
        .unwrap();
    let mut data = [1.0_f32; 3];
    renderer.render(&[], &mut data, 2);
    assert_eq!(data, [0.0; 3]);
    renderer.render(&[], &mut data, 0);
    renderer.render(&[[0.0; 2]; 2], &mut [1.0_f32; 2], 2);
    assert_eq!(controller.status().malformed_blocks, 3);
    assert_eq!(controller.status().frames, 0);
}

#[test]
fn safety_curve_bounds_large_levels_and_invalid_gain_is_rejected() {
    let mut engine = Engine::new(8).unwrap();
    engine.load_stopped(0, &[[f32::MAX, -f32::MAX]]).unwrap();
    engine.command(0, Command::Play).unwrap();
    let (mut controller, mut renderer) = connect(engine);
    controller
        .send(Control::MasterGain {
            gain: 1.0,
            ramp_frames: 0,
        })
        .unwrap();
    controller
        .send(Control::MasterGain {
            gain: f32::NAN,
            ramp_frames: 1,
        })
        .unwrap();
    let mut data = [0.0_f32; 2];
    renderer.render(&[], &mut data, 2);
    assert_eq!(data, [0.9375, -0.9375]);
    assert_eq!(controller.status().command_errors, 1);
}

#[test]
fn commands_cross_threads_without_sharing_the_engine() {
    let (mut controller, mut renderer) = loaded();
    let sender = std::thread::spawn(move || {
        controller
            .send(Control::MasterGain {
                gain: 1.0,
                ramp_frames: 0,
            })
            .unwrap();
        controller
    });
    let controller = sender.join().unwrap();
    let mut data = [0.0_f32; 2];
    renderer.render(&[], &mut data, 2);
    assert_eq!(data, [0.5, -0.25]);
    assert_eq!(controller.status().command_errors, 0);
}

#[test]
fn sparse_track_storage_rejects_disabled_capture_and_overflow() {
    let mut capacities = [0; MAX_TRACKS];
    capacities[0] = 8;
    let mut engine = Engine::with_capacities(capacities).unwrap();
    assert!(engine.command(1, Command::Record).is_err());
    assert!(engine.load_stopped(1, &[[0.0; 2]]).is_err());
    assert!(Engine::with_capacities([usize::MAX; MAX_TRACKS]).is_err());
    assert!(Engine::with_capacities([0; MAX_TRACKS]).is_err());
}
