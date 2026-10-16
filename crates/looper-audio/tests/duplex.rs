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
