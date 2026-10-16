mod common;
use clap_sys::{
    events::*,
    ext::{audio_ports::*, params::*},
    process::*,
};
use common::*;
use std::{ffi::CStr, mem::size_of, ptr};

fn plugin() -> Harness {
    Harness::new(&looper_clap::clap_entry)
}
fn running() -> Harness {
    let h = plugin();
    assert!(h.activate(128));
    assert!(h.start());
    h
}

#[test]
fn discovery_ports_and_parameter_metadata_are_consistent() {
    let h = plugin();
    unsafe {
        assert_eq!((*h.factory).get_plugin_count.unwrap()(h.factory), 1);
        assert!((*h.factory).get_plugin_descriptor.unwrap()(h.factory, 1).is_null());
        assert!(h.extension(c"unknown.extension").is_null());
        assert!(h.extension(c"clap.gui").is_null());
        assert!(h.extension(c"clap.note-ports").is_null());
        let ports = &*h
            .extension(CLAP_EXT_AUDIO_PORTS)
            .cast::<clap_plugin_audio_ports>();
        for input in [false, true] {
            assert_eq!(ports.count.unwrap()(h.plugin, input), 1);
            let mut info = std::mem::zeroed();
            assert!(ports.get.unwrap()(h.plugin, 0, input, &mut info));
            assert_eq!(info.channel_count, 2);
            assert_eq!(CStr::from_ptr(info.port_type), CLAP_PORT_STEREO);
            assert!(!ports.get.unwrap()(h.plugin, 1, input, &mut info));
        }
        let params = h.params();
        assert_eq!(params.count.unwrap()(h.plugin), 22);
        for index in 0..22 {
            let mut info: clap_param_info = std::mem::zeroed();
            assert!(params.get_info.unwrap()(h.plugin, index, &mut info));
            assert_eq!(h.value(info.id), info.default_value);
            let mut text = [0; 128];
            assert!(params.value_to_text.unwrap()(
                h.plugin,
                info.id,
                info.default_value,
                text.as_mut_ptr(),
                128
            ));
            let mut parsed = -1.0;
            assert!(params.text_to_value.unwrap()(
                h.plugin,
                info.id,
                text.as_ptr(),
                &mut parsed
            ));
            assert_eq!(parsed, info.default_value);
        }
    }
}
#[test]
fn lifecycle_rejects_bad_transitions_and_preserves_audio_across_reactivation() {
    let h = plugin();
    assert!(!h.start());
    assert!(h.activate(8));
    assert!(!h.activate(8));
    assert!(h.start());
    assert!(!h.start());
    let (result, l, r) = h.render(
        &[0.25, 0.5, 0.0, 0.0],
        &[-0.5, -0.25, 0.0, 0.0],
        &mut Events::new(&[(0, 100, 1.0), (2, 100, 2.0)]),
        false,
    );
    assert_eq!(result, CLAP_PROCESS_CONTINUE);
    assert_eq!(l, [0.0, 0.0, 0.25, 0.5]);
    assert_eq!(r, [0.0, 0.0, -0.5, -0.25]);
    h.stop();
    h.deactivate();
    assert!(h.activate(128));
    assert!(h.start());
    assert_eq!(
        h.render(&[0.0; 2], &[0.0; 2], &mut Events::new(&[]), false)
            .1,
        [0.25, 0.5]
    );
    h.reset();
    assert_eq!(
        h.render(&[0.0; 2], &[0.0; 2], &mut Events::new(&[]), false)
            .1,
        [0.0; 2]
    );
    assert_eq!(
        h.render(
            &[0.0; 2],
            &[0.0; 2],
            &mut Events::new(&[(0, 100, 3.0)]),
            false
        )
        .1,
        [0.25, 0.5]
    );
}
#[test]
fn sample_offsets_record_only_the_requested_interval() {
    let h = running();
    let (result, l, r) = h.render(
        &[9.0, 0.2, 0.3, 0.4, 9.0, 9.0, 9.0, 9.0],
        &[9.0, -0.2, -0.3, -0.4, 9.0, 9.0, 9.0, 9.0],
        &mut Events::new(&[(1, 100, 1.0), (4, 100, 2.0)]),
        false,
    );
    assert_eq!(result, CLAP_PROCESS_CONTINUE);
    assert_eq!(l, [0.0, 0.0, 0.0, 0.0, 0.2, 0.3, 0.4, 0.2]);
    assert_eq!(r, l.iter().map(|v| -v).collect::<Vec<_>>());
    assert_eq!(h.value(100), 2.0);
}
#[test]
fn input_is_snapshotted_before_writing_in_place_host_buffers() {
    let h = plugin();
    h.flush(&mut Events::new(&[(0, 1, 1.0)]));
    assert!(h.activate(128));
    assert!(h.start());
    let (result, l, r) = h.render(
        &[2.0, 1.0, 0.5, -0.1],
        &[-0.1, 0.5, 1.0, 2.0],
        &mut Events::new(&[]),
        true,
    );
    assert_eq!(result, CLAP_PROCESS_CONTINUE);
    assert_eq!(l, [2.0, 1.0, 0.5, -0.1]);
    assert_eq!(r, [-0.1, 0.5, 1.0, 2.0]);
}
#[test]
fn malformed_event_batches_cannot_apply_partial_recording_or_parameters() {
    let h = running();
    for values in [
        &[(0, 100, 1.0), (3, 0, f64::NAN)][..],
        &[(0, 100, 1.0), (4, 100, 2.0)],
        &[(2, 100, 1.0), (1, 0, 0.5)],
    ] {
        let (status, l, r) = h.render(&[0.5; 4], &[0.5; 4], &mut Events::new(values), false);
        assert_eq!(status, CLAP_PROCESS_ERROR);
        assert_eq!(l, [0.0; 4]);
        assert_eq!(r, [0.0; 4]);
        assert_eq!(h.value(100), 0.0);
        assert_eq!(h.value(0), 1.0);
    }
    let mut small = Events::new(&[(0, 100, 1.0)]);
    small.0[0].header.size = size_of::<clap_event_header>() as u32;
    assert_eq!(
        h.render(&[0.0; 4], &[0.0; 4], &mut small, false).0,
        CLAP_PROCESS_ERROR
    );
    assert_eq!(
        h.render(
            &[0.25; 4],
            &[0.25; 4],
            &mut Events::new(&[(0, 100, 1.0), (2, 100, 2.0)]),
            false
        )
        .1,
        [0.0, 0.0, 0.25, 0.25]
    );
}
#[test]
fn parameter_flush_works_inactive_and_active_and_zero_frames_are_valid() {
    let h = plugin();
    h.flush(&mut Events::new(&[(0, 0, 0.5)]));
    assert_eq!(h.value(0), 0.5);
    assert!(h.activate(128));
    h.flush(&mut Events::new(&[(0, 1, 1.0)]));
    assert!(h.start());
    let (status, l, _) = h.render(&[1.0; 64], &[1.0; 64], &mut Events::new(&[]), false);
    assert_eq!(status, CLAP_PROCESS_CONTINUE);
    assert_eq!(l[63], 0.5);
    assert_eq!(
        h.render(&[], &[], &mut Events::new(&[(0, 100, 1.0)]), false)
            .0,
        CLAP_PROCESS_CONTINUE
    );
    h.flush(&mut Events::new(&[(55, 100, 5.0)]));
    assert_eq!(h.value(100), 5.0);
}
#[test]
fn instances_have_independent_audio_and_parameter_values() {
    let a = running();
    let b = running();
    a.render(
        &[0.5; 4],
        &[-0.25; 4],
        &mut Events::new(&[(0, 100, 1.0), (2, 100, 2.0)]),
        false,
    );
    assert_eq!(
        a.render(&[0.0; 2], &[0.0; 2], &mut Events::new(&[]), false)
            .1,
        [0.5; 2]
    );
    assert_eq!(
        b.render(&[0.0; 2], &[0.0; 2], &mut Events::new(&[]), false)
            .1,
        [0.0; 2]
    );
    assert_eq!(b.value(100), 0.0);
}
#[test]
fn malformed_audio_layout_and_transport_are_rejected_without_consuming_commands() {
    let h = running();
    let mut l = [1.0; 4];
    let mut r = [1.0; 4];
    let mut pointers = [l.as_mut_ptr(), r.as_mut_ptr()];
    let input = buffer(&mut pointers);
    let mut output_pointers = pointers;
    let mut output = buffer(&mut output_pointers);
    let mut events = Events::new(&[(0, 100, 1.0)]);
    let list = events.list();
    let mut c = call(4, &input, &mut output, &list);
    let mut transport: clap_event_transport = unsafe { std::mem::zeroed() };
    transport.header = clap_event_header {
        size: size_of::<clap_event_transport>() as u32,
        time: 0,
        space_id: 0,
        type_: CLAP_EVENT_TRANSPORT,
        flags: 0,
    };
    transport.flags = CLAP_TRANSPORT_HAS_TEMPO;
    transport.tempo = f64::NAN;
    c.transport = &transport;
    assert_eq!(
        unsafe { (*h.plugin).process.unwrap()(h.plugin, &c) },
        CLAP_PROCESS_ERROR
    );
    assert_eq!(h.value(100), 0.0);
    c.transport = ptr::null();
    output.channel_count = 1;
    std::hint::black_box(&output);
    assert_eq!(
        unsafe { (*h.plugin).process.unwrap()(h.plugin, &c) },
        CLAP_PROCESS_ERROR
    );
}
