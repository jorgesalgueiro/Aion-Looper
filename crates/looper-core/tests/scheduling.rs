use looper_core::schedule::{ScheduledCommand, SCHEDULE_CAPACITY};
use looper_core::{Command, Engine, Error, State};

fn queue(engine: &mut Engine, frame: u64, track: usize, command: Command) {
    engine
        .schedule(ScheduledCommand {
            frame,
            track,
            command,
        })
        .unwrap();
}

fn scheduled_take() -> Engine {
    let mut engine = Engine::new(32).unwrap();
    queue(&mut engine, 2, 0, Command::Record);
    queue(&mut engine, 5, 0, Command::Finish);
    queue(&mut engine, 7, 0, Command::Mute(true));
    queue(&mut engine, 9, 0, Command::Mute(false));
    queue(&mut engine, 10, 0, Command::Retrigger(1));
    queue(&mut engine, 12, 0, Command::Stop);
    engine
}

#[test]
fn commands_apply_at_the_exact_frame_across_block_partitions() {
    let input: Vec<_> = (0..16).map(|i| [i as f32 / 16.0; 2]).collect();
    let mut a = scheduled_take();
    let mut b = scheduled_take();
    let mut whole = [[0.0; 2]; 16];
    a.process(&input, &mut whole).unwrap();
    let mut split = [[0.0; 2]; 16];
    let mut start = 0;
    for size in [1, 3, 1, 2, 5, 4] {
        b.process(&input[start..start + size], &mut split[start..start + size])
            .unwrap();
        start += size;
    }
    assert_eq!(whole, split);
    assert_eq!(whole[5], [2.0 / 16.0; 2]);
    assert_eq!(whole[6], [3.0 / 16.0; 2]);
    assert_eq!(whole[7], [0.0; 2]);
    assert_eq!(whole[9], [3.0 / 16.0; 2]);
    assert_eq!(whole[10], [3.0 / 16.0; 2]);
    assert_eq!(whole[12], [0.0; 2]);
    assert_eq!(a.schedule_status().applied, 6);
    assert_eq!(a.status(0).unwrap().state, State::Stopped);
}

#[test]
fn same_frame_insertion_order_and_failed_commands_are_observable() {
    let mut engine = Engine::new(16).unwrap();
    queue(&mut engine, 0, 0, Command::Play); // Empty: invalid at execution.
    queue(&mut engine, 0, 0, Command::Record);
    queue(&mut engine, 2, 0, Command::Finish);
    queue(&mut engine, 2, 0, Command::Mute(true));
    let mut output = [[1.0; 2]; 4];
    engine.process(&[[0.5; 2]; 4], &mut output).unwrap();
    assert_eq!(output, [[0.0; 2]; 4]);
    assert_eq!(engine.schedule_status().applied, 3);
    assert_eq!(engine.schedule_status().rejected, 1);
    assert_eq!(
        engine.schedule_status().last_error,
        Some(Error::InvalidState)
    );
}

#[test]
fn queue_backpressure_order_and_past_timestamps_are_explicit() {
    let mut engine = Engine::new(16).unwrap();
    queue(&mut engine, 10, 0, Command::Mute(true));
    assert_eq!(
        engine.schedule(ScheduledCommand {
            frame: 9,
            track: 0,
            command: Command::Clear
        }),
        Err(Error::ScheduleOrder)
    );
    for _ in 1..SCHEDULE_CAPACITY {
        queue(&mut engine, 10, 0, Command::Mute(false));
    }
    assert_eq!(
        engine.schedule(ScheduledCommand {
            frame: 10,
            track: 0,
            command: Command::Clear
        }),
        Err(Error::ScheduleFull)
    );
    engine
        .process(&[[0.0; 2]; 11], &mut [[0.0; 2]; 11])
        .unwrap();
    assert_eq!(engine.schedule_status().applied, SCHEDULE_CAPACITY as u64);
    assert_eq!(
        engine.schedule(ScheduledCommand {
            frame: 0,
            track: 0,
            command: Command::Clear
        }),
        Err(Error::ScheduleInPast)
    );
}

#[test]
fn cancellation_preserves_other_tracks_and_ring_wrap() {
    let mut engine = Engine::new(16).unwrap();
    for _ in 0..200 {
        queue(&mut engine, 0, 0, Command::Mute(false));
    }
    engine.process(&[[0.0; 2]], &mut [[0.0; 2]]).unwrap();
    for frame in 1..200 {
        queue(
            &mut engine,
            frame,
            (frame % 2) as usize,
            Command::Mute(true),
        );
    }
    engine.cancel_scheduled(0).unwrap();
    assert_eq!(engine.schedule_status().pending, 100);
    engine
        .process(&[[0.0; 2]; 200], &mut [[0.0; 2]; 200])
        .unwrap();
    assert_eq!(engine.schedule_status().pending, 0);
    assert!(!engine.status(0).unwrap().muted);
    assert!(engine.status(1).unwrap().muted);
}

#[test]
fn pcm_import_is_validated_and_does_not_modify_a_failed_destination() {
    let mut engine = Engine::new(4).unwrap();
    engine
        .load_stopped(0, &[[0.25, -0.5], [0.5, -0.25]])
        .unwrap();
    assert_eq!(engine.status(0).unwrap().state, State::Stopped);
    assert_eq!(
        engine.load_stopped(0, &[[f32::NAN; 2]]),
        Err(Error::InvalidParameter)
    );
    assert_eq!(engine.audio(0).unwrap(), &[[0.25, -0.5], [0.5, -0.25]]);
    engine.command(0, Command::Play).unwrap();
    assert_eq!(
        engine.load_stopped(0, &[[0.1; 2]]),
        Err(Error::InvalidState)
    );
}
