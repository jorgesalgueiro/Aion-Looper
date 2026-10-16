use looper_core::{Command, State, MAX_TRACKS};
use looper_host::{Action, Error, Event, Input, Processor, Transport, MAX_EVENTS};

fn prepared() -> Processor {
    Processor::prepare(48_000.0, 64, [64; MAX_TRACKS]).unwrap()
}
fn running() -> Processor {
    let mut p = prepared();
    p.activate().unwrap();
    p
}
fn track(offset: usize, command: Command) -> Event {
    Event {
        offset,
        action: Action::Track { track: 0, command },
    }
}
fn render(p: &mut Processor, events: &[Event], frames: usize) -> Vec<[f32; 2]> {
    let mut left = vec![0.0; frames];
    let mut right = vec![0.0; frames];
    p.process(Input::Silent, &mut left, &mut right, events, None)
        .unwrap();
    left.into_iter().zip(right).map(|(l, r)| [l, r]).collect()
}

#[test]
fn host_stereo_record_finish_and_retrigger_obey_sample_offsets() {
    let mut p = running();
    let left = [1.0, 0.25, 0.5, 0.75, 9.0, 9.0, 9.0, 9.0];
    let right = left.map(|x| -x);
    let mut out_l = [0.0; 8];
    let mut out_r = [0.0; 8];
    p.process(
        Input::Stereo {
            left: &left,
            right: &right,
        },
        &mut out_l,
        &mut out_r,
        &[
            track(1, Command::Record),
            track(4, Command::Finish),
            track(6, Command::Retrigger(1)),
        ],
        None,
    )
    .unwrap();
    assert_eq!(out_l, [0.0, 0.0, 0.0, 0.0, 0.25, 0.5, 0.5, 0.75]);
    assert_eq!(out_r, out_l.map(|x| -x));
    assert_eq!(
        p.engine().audio(0).unwrap(),
        &[[0.25, -0.25], [0.5, -0.5], [0.75, -0.75]]
    );
    assert_eq!(p.engine().clock_frames(), 8);
}

#[test]
fn automation_ramps_are_independent_of_host_block_partitioning() {
    let mut a = prepared();
    let mut b = prepared();
    for p in [&mut a, &mut b] {
        p.engine_mut()
            .unwrap()
            .load_stopped(0, &[[0.5, -0.25], [1.0, -0.5]])
            .unwrap();
        p.activate().unwrap();
    }
    let gain = Event {
        offset: 2,
        action: Action::MasterGain {
            gain: 0.0,
            ramp_frames: 5,
        },
    };
    let whole = render(
        &mut a,
        &[track(0, Command::Play), gain, track(7, Command::Mute(true))],
        10,
    );
    let mut parts = render(&mut b, &[track(0, Command::Play)], 2);
    parts.extend(render(&mut b, &[Event { offset: 0, ..gain }], 3));
    parts.extend(render(&mut b, &[track(2, Command::Mute(true))], 5));
    assert_eq!(parts, whole);
    assert_eq!(whole[2], [0.4, -0.2]);
    assert_eq!(whole[6], [0.0, 0.0]);
}

#[test]
fn malformed_blocks_are_silent_and_cannot_partially_change_engine_or_transport() {
    let mut p = running();
    let transport = Some(Transport {
        tempo: Some(120.0),
        sample_position: Some(0),
        ..Transport::default()
    });
    let invalid = [
        vec![track(0, Command::Record), track(4, Command::Finish)],
        vec![track(2, Command::Record), track(1, Command::Stop)],
        vec![track(0, Command::Gain(f32::NAN))],
        vec![Event {
            offset: 0,
            action: Action::Track {
                track: MAX_TRACKS,
                command: Command::Record,
            },
        }],
        vec![track(0, Command::Record); MAX_EVENTS + 1],
    ];
    for events in invalid {
        let mut l = [9.0; 4];
        let mut r = [9.0; 4];
        assert_eq!(
            p.process(Input::Silent, &mut l, &mut r, &events, transport),
            Err(Error::Events)
        );
        assert_eq!(l, [0.0; 4]);
        assert_eq!(r, [0.0; 4]);
        assert_eq!(p.engine().clock_frames(), 0);
        assert_eq!(p.engine().status(0).unwrap().state, State::Empty);
        assert_eq!(p.transport(), None);
    }
    let mut l = [9.0; 4];
    let mut r = [9.0; 3];
    assert_eq!(
        p.process(Input::Silent, &mut l, &mut r, &[], transport),
        Err(Error::BufferLayout)
    );
    assert_eq!(r, [0.0; 3]);
    assert_eq!(
        p.process(Input::Mono(&[1.0; 3]), &mut l, &mut [0.0; 4], &[], None),
        Err(Error::BufferLayout)
    );
    assert_eq!(
        p.process(Input::Silent, &mut [0.0; 65], &mut [0.0; 65], &[], None),
        Err(Error::BufferLayout)
    );
}

#[test]
fn zero_frame_flush_preserves_order_and_counts_state_errors_without_advancing() {
    let mut p = running();
    let result = p
        .process(
            Input::Silent,
            &mut [],
            &mut [],
            &[
                track(0, Command::Play),
                track(0, Command::Record),
                track(0, Command::Gain(0.5)),
            ],
            None,
        )
        .unwrap();
    assert_eq!(result.rejected_commands, 1);
    assert_eq!(result.last_error, Some(looper_core::Error::InvalidState));
    assert_eq!(p.engine().status(0).unwrap().state, State::Recording);
    assert_eq!(p.engine().clock_frames(), 0);
    assert_eq!(
        p.process(
            Input::Silent,
            &mut [],
            &mut [],
            &[track(1, Command::Stop)],
            None
        ),
        Err(Error::Events)
    );
}

#[test]
fn monitor_is_off_by_default_and_preserves_finite_host_headroom() {
    let mut p = running();
    let mut l = [9.0; 4];
    let mut r = [9.0; 4];
    p.process(Input::Mono(&[2.0; 4]), &mut l, &mut r, &[], None)
        .unwrap();
    assert_eq!(l, [0.0; 4]);
    let enable = Event {
        offset: 0,
        action: Action::MonitorGain {
            gain: 1.0,
            ramp_frames: 0,
        },
    };
    p.process(
        Input::Mono(&[2.0, f32::NAN, f32::INFINITY, -3.0]),
        &mut l,
        &mut r,
        &[enable, track(0, Command::Record)],
        None,
    )
    .unwrap();
    assert_eq!(l, [2.0, 0.0, 0.0, -3.0]);
    assert_eq!(r, l);
    assert_eq!(
        p.engine().audio(0).unwrap(),
        &[[2.0; 2], [0.0; 2], [0.0; 2], [-3.0; 2]]
    );
    let master = Event {
        offset: 0,
        action: Action::MasterGain {
            gain: 4.0,
            ramp_frames: 0,
        },
    };
    p.process(Input::Mono(&[f32::MAX; 4]), &mut l, &mut r, &[master], None)
        .unwrap();
    assert!(l.iter().all(|&v| v == f32::MAX));
}

#[test]
fn transport_seeks_and_stop_do_not_rewind_the_engine_sample_clock() {
    let mut p = running();
    let mut t = Transport {
        sample_position: Some(-8),
        quarter_position: Some(-0.1),
        tempo: Some(123.0),
        signature: Some((7, 8)),
        playing: true,
    };
    let call = |p: &mut Processor, t| {
        p.process(Input::Silent, &mut [0.0; 4], &mut [0.0; 4], &[], Some(t))
            .unwrap()
    };
    assert!(!call(&mut p, t).discontinuity);
    t.sample_position = Some(-4);
    assert!(!call(&mut p, t).discontinuity);
    t.sample_position = Some(100);
    assert!(call(&mut p, t).discontinuity);
    t.sample_position = Some(104);
    t.playing = false;
    assert!(!call(&mut p, t).discontinuity);
    assert!(!call(&mut p, t).discontinuity);
    assert_eq!(p.engine().clock_frames(), 20);
    assert_eq!(p.transport(), Some(t));
    t.tempo = Some(f64::NAN);
    assert_eq!(
        p.process(Input::Silent, &mut [1.0; 4], &mut [1.0; 4], &[], Some(t)),
        Err(Error::Transport)
    );
    assert_eq!(p.engine().clock_frames(), 20);
}

#[test]
fn lifecycle_and_independent_instances_keep_loop_state_isolated() {
    let mut a = prepared();
    let mut b = running();
    assert_eq!(
        a.process(Input::Silent, &mut [1.0; 2], &mut [1.0; 2], &[], None),
        Err(Error::Inactive)
    );
    a.engine_mut()
        .unwrap()
        .load_stopped(0, &[[0.7, -0.2]])
        .unwrap();
    a.activate().unwrap();
    assert_eq!(a.activate(), Err(Error::Active));
    assert!(matches!(a.engine_mut(), Err(Error::Active)));
    assert_eq!(
        render(&mut a, &[track(0, Command::Play)], 2),
        vec![[0.7, -0.2]; 2]
    );
    assert_eq!(render(&mut b, &[], 2), vec![[0.0; 2]; 2]);
    a.deactivate();
    assert!(a.engine_mut().is_ok());
    a.activate().unwrap();
    assert_eq!(render(&mut a, &[], 1), vec![[0.7, -0.2]]);
    assert_eq!(a.sample_rate(), 48_000.0);
}

#[test]
fn prepare_and_transport_reject_invalid_configuration() {
    for (rate, block) in [(f64::NAN, 64), (0.0, 64), (48_000.0, 0), (48_000.0, 65_537)] {
        assert!(matches!(
            Processor::prepare(rate, block, [4; MAX_TRACKS]),
            Err(Error::Configuration)
        ));
    }
    assert!(matches!(
        Processor::prepare(48_000.0, 64, [0; MAX_TRACKS]),
        Err(Error::Core(_))
    ));
    let mut p = running();
    for t in [
        Transport {
            signature: Some((0, 4)),
            ..Transport::default()
        },
        Transport {
            signature: Some((4, 3)),
            ..Transport::default()
        },
        Transport {
            quarter_position: Some(f64::INFINITY),
            ..Transport::default()
        },
        Transport {
            tempo: Some(0.0),
            ..Transport::default()
        },
    ] {
        assert_eq!(
            p.process(Input::Silent, &mut [1.0; 4], &mut [1.0; 4], &[], Some(t)),
            Err(Error::Transport)
        );
    }
}
