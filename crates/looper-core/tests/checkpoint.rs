use looper_core::{
    checkpoint::{Checkpoint, SnapshotError},
    Command, MAX_TRACKS,
};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
#[test]
fn an_unwound_write_transaction_cannot_be_saved_as_a_complete_take() {
    let (_, c) = Checkpoint::prepare([4; MAX_TRACKS]).unwrap();
    let result = std::panic::catch_unwind(|| {
        let _transaction = c.begin();
        panic!("interrupted checkpoint update");
    });
    assert!(result.is_err());
    assert_eq!(c.snapshot(80, || ()).unwrap_err(), SnapshotError::Busy);
}
#[test]
fn checkpoint_pcm_matches_local_mix_without_a_duplicate_pcm_buffer() {
    let (mut engine, c) = Checkpoint::prepare([8; MAX_TRACKS]).unwrap();
    {
        let _t = c.begin();
        engine.command(0, Command::Record).unwrap();
        engine.process_frame([2.0, -0.0]);
        engine.process_frame([-0.5, 0.25]);
        engine.command(0, Command::Finish).unwrap();
        c.publish(&engine);
    }
    let (takes, extra) = c.snapshot(160, || 7).unwrap();
    assert_eq!(extra, 7);
    assert_eq!(takes[0].audio, [[2.0, -0.0], [-0.5, 0.25]]);
    assert_eq!(takes[0].audio[0][1].to_bits(), (-0.0f32).to_bits());
    assert!(takes[0].playing);
    assert_eq!(engine.process_frame([0.0; 2]), [2.0, -0.0]);
    assert_eq!(std::mem::size_of::<[std::sync::atomic::AtomicU32; 2]>(), 8);
}
#[test]
fn checkpoint_rejects_capture_open_transactions_and_frame_overflow() {
    let (mut e, c) = Checkpoint::prepare([8; MAX_TRACKS]).unwrap();
    {
        let _t = c.begin();
        e.command(0, Command::Record).unwrap();
        e.process_frame([1.0; 2]);
        c.publish(&e);
        assert_eq!(c.snapshot(160, || ()).unwrap_err(), SnapshotError::Busy);
    }
    assert_eq!(c.snapshot(160, || ()).unwrap_err(), SnapshotError::Busy);
    {
        let _t = c.begin();
        e.command(0, Command::Finish).unwrap();
        c.publish(&e);
    }
    assert_eq!(c.snapshot(0, || ()).unwrap_err(), SnapshotError::Capacity);
    {
        let _t = c.begin();
        e.command(0, Command::Overdub).unwrap();
        c.publish(&e);
    }
    assert_eq!(c.snapshot(160, || ()).unwrap_err(), SnapshotError::Busy);
}
#[test]
fn concurrent_checkpoint_reads_never_return_a_mixed_take() {
    let (mut e, c) = Checkpoint::prepare([128; MAX_TRACKS]).unwrap();
    {
        let _t = c.begin();
        e.load_stopped(0, &[[0.25; 2]; 128]).unwrap();
        c.publish(&e);
    }
    let done = Arc::new(AtomicBool::new(false));
    let writer_done = Arc::clone(&done);
    let writer_c = Arc::clone(&c);
    let writer = std::thread::spawn(move || {
        let mut value = 0.5;
        while !writer_done.load(Ordering::Acquire) {
            let _t = writer_c.begin();
            e.load_stopped(0, &[[value; 2]; 128]).unwrap();
            writer_c.publish(&e);
            value = if value == 0.5 { 0.25 } else { 0.5 };
        }
    });
    for _ in 0..500 {
        if let Ok((takes, _)) = c.snapshot(2560, || ()) {
            let first = takes[0].audio[0];
            assert!(takes[0].audio.iter().all(|f| *f == first));
        }
    }
    done.store(true, Ordering::Release);
    writer.join().unwrap();
    assert!(c.snapshot(2560, || ()).is_ok());
}
