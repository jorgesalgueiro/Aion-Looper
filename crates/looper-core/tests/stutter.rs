use looper_core::{Command, Engine, Error, State};
fn engine() -> Engine {
    let mut e = Engine::new(16).unwrap();
    e.load_stopped(
        0,
        &(0..8).map(|i| [i as f32, -(i as f32)]).collect::<Vec<_>>(),
    )
    .unwrap();
    e
}
#[test]
fn held_fragment_is_sample_exact_clipped_at_end_and_release_keeps_phase() {
    let mut e = engine();
    e.command(0, Command::Play).unwrap();
    for i in 0..3 {
        assert_eq!(e.process_frame([0.0; 2])[0], i as f32);
    }
    e.command(0, Command::StutterStart(2)).unwrap();
    assert!(e.is_stuttering(0).unwrap());
    for expected in [3.0, 4.0, 3.0] {
        assert_eq!(e.process_frame([0.0; 2])[0], expected);
    }
    e.command(0, Command::StutterStart(7)).unwrap(); // repeated press retains the anchor
    assert_eq!(e.process_frame([0.0; 2])[0], 4.0);
    e.command(0, Command::StutterStop).unwrap();
    assert!(!e.is_stuttering(0).unwrap());
    for expected in [3.0, 4.0, 5.0, 6.0, 7.0, 0.0] {
        assert_eq!(e.process_frame([0.0; 2])[0], expected);
    }
    e.command(0, Command::Retrigger(7)).unwrap();
    e.command(0, Command::StutterStart(usize::MAX)).unwrap();
    for _ in 0..10 {
        assert_eq!(e.process_frame([0.0; 2])[0], 7.0);
    }
    e.command(0, Command::StutterStop).unwrap();
    assert_eq!(e.process_frame([0.0; 2])[0], 7.0);
    assert_eq!(e.process_frame([0.0; 2])[0], 0.0);
    assert_eq!(
        e.audio(0).unwrap(),
        &(0..8).map(|i| [i as f32, -(i as f32)]).collect::<Vec<_>>()
    );
}
#[test]
fn transport_changes_cancel_holds_and_retrigger_wraps_without_touching_other_tracks() {
    for command in [
        Command::Stop,
        Command::Clear,
        Command::Retrigger(0),
        Command::Overdub,
        Command::Substitute,
    ] {
        let mut e = engine();
        e.load_stopped(1, &[[0.0; 2]; 7]).unwrap();
        e.command(1, Command::Play).unwrap();
        e.command(0, Command::Play).unwrap();
        e.process_frame([0.0; 2]);
        e.command(0, Command::StutterStart(2)).unwrap();
        e.command(0, command).unwrap();
        assert!(!e.is_stuttering(0).unwrap());
        assert_eq!(e.status(1).unwrap().position_frames, 1);
        e.process_frame([0.0; 2]);
        assert_eq!(e.status(1).unwrap().position_frames, 2);
        assert_eq!(e.clock_frames(), 2);
    }
    let mut e = engine();
    assert_eq!(
        e.command(0, Command::StutterStart(2)),
        Err(Error::InvalidState)
    );
    assert_eq!(
        e.command(0, Command::StutterStart(0)),
        Err(Error::InvalidParameter)
    );
    e.command(0, Command::Retrigger(17)).unwrap();
    assert_eq!(e.status(0).unwrap().state, State::Playing);
    assert_eq!(e.process_frame([0.0; 2])[0], 1.0);
    e.command(0, Command::Clear).unwrap();
    assert!(e.command(0, Command::StutterStop).is_ok());
    assert_eq!(
        e.command(0, Command::Retrigger(0)),
        Err(Error::InvalidState)
    );
    e.load_stopped(0, &[[0.25, -0.5]]).unwrap();
    e.command(0, Command::Play).unwrap();
    e.command(0, Command::StutterStart(1)).unwrap();
    for _ in 0..20 {
        assert_eq!(e.process_frame([0.0; 2]), [0.25, -0.5]);
    }
}
