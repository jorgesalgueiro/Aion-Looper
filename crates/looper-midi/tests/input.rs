use looper_midi::{transport, Hub, MAX_EVENTS};
use std::time::Instant;
#[test]
fn short_messages_keep_exact_bytes_and_filter_sysex_and_malformed_input() {
    let (mut writer, mut reader) = transport::channel(Instant::now());
    let messages = [
        vec![0x90, 60, 127],
        vec![0x91, 60, 0],
        vec![0xB0, 64, 127],
        vec![0xE0, 0, 64],
        vec![0xC0, 3],
        vec![0xD0, 40],
        vec![0xF2, 2, 3],
        vec![0xF8],
        vec![0xFA],
        vec![0xFB],
        vec![0xFC],
    ];
    for message in &messages {
        writer.capture(17, message);
    }
    for invalid in [
        &[][..],
        &[0xF0, 1, 0xF7],
        &[0xF7],
        &[0xF9],
        &[60, 2, 3],
        &[0x90, 60],
        &[0x90, 60, 128],
        &[0xFA, 0],
        &[0x90, 1, 2, 3],
    ] {
        writer.capture(18, invalid);
    }
    for expected in messages {
        let received = reader.pop().unwrap();
        assert_eq!(&received.data[..usize::from(received.length)], expected);
        assert_eq!(received.timestamp_us, 17);
    }
    assert!(reader.pop().is_none());
    assert_eq!(reader.dropped(), 0);
}
#[test]
fn callback_transport_keeps_fifo_across_threads_and_reports_capacity_without_growth() {
    let (mut writer, mut reader) = transport::channel(Instant::now());
    let writer = std::thread::spawn(move || {
        for i in 0..transport::CAPACITY + 3 {
            writer.capture(i as u64, &[0x90, (i % 128) as u8, 1]);
        }
        writer
    })
    .join()
    .unwrap();
    assert_eq!(reader.dropped(), 3);
    for i in 0..transport::CAPACITY {
        assert_eq!(reader.pop().unwrap().timestamp_us, i as u64);
    }
    assert!(reader.pop().is_none());
    let mut writer = writer;
    writer.capture(9, &[0x80, 60, 0]);
    assert_eq!(reader.pop().unwrap().data, [0x80, 60, 0]);
}
#[test]
fn synthetic_hotplug_discards_old_queues_and_reconnects_the_same_port_identity() {
    let mut hub = Hub::synthetic();
    let id = hub.initial_snapshot().ports[0].id.clone();
    let generation = hub.initial_snapshot().ports[0].generation;
    hub.inject(&[vec![0x90, 60, 100]]).unwrap();
    assert!(hub.initial_snapshot().events.is_empty()); // Does not drain startup notes.
    assert_eq!(hub.poll().unwrap().events[0].data, [0x90, 60, 100]);
    hub.inject(&[vec![0x90, 61, 100]]).unwrap();
    hub.set_connected(false).unwrap();
    let disconnected = hub.poll().unwrap();
    assert!(disconnected.ports.is_empty());
    assert!(disconnected.events.is_empty());
    assert!(hub.inject(&[vec![0x90, 60, 100]]).is_err());
    hub.set_connected(true).unwrap();
    let reconnected = hub.poll().unwrap();
    assert_eq!(reconnected.ports[0].id, id);
    assert_ne!(reconnected.ports[0].generation, generation);
    assert!(reconnected.events.is_empty());
    hub.inject(&[vec![0x90, 62, 100]]).unwrap();
    assert_eq!(hub.poll().unwrap().events[0].data, [0x90, 62, 100]);
}
#[test]
fn polling_is_bounded_and_overflow_flushes_stale_messages_with_an_explicit_reset() {
    let mut hub = Hub::synthetic();
    let batch = vec![vec![0x90, 60, 100]; MAX_EVENTS];
    hub.inject(&batch).unwrap();
    hub.inject(&[vec![0x80, 60, 0]]).unwrap();
    assert_eq!(hub.poll().unwrap().events.len(), MAX_EVENTS);
    assert_eq!(hub.poll().unwrap().events.len(), 1);
    for _ in 0..5 {
        hub.inject(&batch).unwrap();
    }
    let snapshot = hub.poll().unwrap();
    assert_eq!(snapshot.dropped, 256);
    assert_eq!(snapshot.reset, ["self-test-midi"]);
    assert!(snapshot.events.is_empty());
    hub.inject(&[vec![0xFC]]).unwrap();
    let fresh = hub.poll().unwrap();
    assert_eq!(fresh.events[0].data, [0xFC]);
    assert!(fresh.reset.is_empty());
    assert!(hub.inject(&vec![vec![0xFA]; MAX_EVENTS + 1]).is_err());
    assert!(hub.inject(&[vec![0xF0, 1, 0xF7]]).is_err());
}
