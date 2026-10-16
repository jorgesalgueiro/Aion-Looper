use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
static ACTIVE: AtomicBool = AtomicBool::new(false);
static OPS: AtomicUsize = AtomicUsize::new(0);
struct Measured;
unsafe impl GlobalAlloc for Measured {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if ACTIVE.load(Ordering::Relaxed) {
            OPS.fetch_add(1, Ordering::Relaxed);
        }
        unsafe { System.alloc(layout) }
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        if ACTIVE.load(Ordering::Relaxed) {
            OPS.fetch_add(1, Ordering::Relaxed);
        }
        unsafe { System.dealloc(ptr, layout) }
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, size: usize) -> *mut u8 {
        if ACTIVE.load(Ordering::Relaxed) {
            OPS.fetch_add(1, Ordering::Relaxed);
        }
        unsafe { System.realloc(ptr, layout, size) }
    }
}
#[global_allocator]
static ALLOC: Measured = Measured;
#[test]
fn host_processing_automation_capture_and_reactivation_have_no_heap_operations() {
    use looper_core::{Command, MAX_TRACKS};
    use looper_host::{Action, Event, Input, Processor, Transport};
    let mut processor = Processor::prepare(48_000.0, 64, [1024; MAX_TRACKS]).unwrap();
    processor.activate().unwrap();
    let mut left = [0.0; 64];
    let mut right = [0.0; 64];
    let mut events = [Event {
        offset: 0,
        action: Action::Track {
            track: 0,
            command: Command::Record,
        },
    }; MAX_TRACKS + 1];
    for (track, event) in events[..MAX_TRACKS].iter_mut().enumerate() {
        event.action = Action::Track {
            track,
            command: Command::Record,
        };
    }
    events[MAX_TRACKS].action = Action::MasterGain {
        gain: 0.5,
        ramp_frames: 128,
    };
    ACTIVE.store(true, Ordering::SeqCst);
    let first = processor.process(
        Input::Mono(&[0.1; 64]),
        &mut left,
        &mut right,
        &events,
        None,
    );
    for block in 0..64 {
        let result = processor.process(
            Input::Silent,
            &mut left,
            &mut right,
            &[],
            Some(Transport {
                sample_position: Some(block * 64),
                playing: true,
                ..Transport::default()
            }),
        );
        std::hint::black_box(result).unwrap();
    }
    processor.deactivate();
    processor.activate().unwrap();
    let flush = processor.process(Input::Silent, &mut [], &mut [], &[], None);
    ACTIVE.store(false, Ordering::SeqCst);
    assert_eq!(OPS.load(Ordering::SeqCst), 0);
    assert_eq!(first.unwrap().rejected_commands, 0);
    assert_eq!(flush.unwrap().frames, 0);
    assert_eq!(processor.engine().clock_frames(), 4160);
}
