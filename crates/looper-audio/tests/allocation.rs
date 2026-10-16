use looper_audio::realtime::{connect, Control};
use looper_core::{Command, Engine, MAX_TRACKS};
use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};

struct CountingAllocator;
static ENABLED: AtomicBool = AtomicBool::new(false);
static OPERATIONS: AtomicUsize = AtomicUsize::new(0);

fn count() {
    if ENABLED.load(Ordering::Relaxed) {
        OPERATIONS.fetch_add(1, Ordering::Relaxed);
    }
}

// Delegates every operation to System with the unchanged pointer and layout.
unsafe impl GlobalAlloc for CountingAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        count();
        unsafe { System.alloc(layout) }
    }
    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        count();
        unsafe { System.alloc_zeroed(layout) }
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, size: usize) -> *mut u8 {
        count();
        unsafe { System.realloc(ptr, layout, size) }
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        count();
        unsafe { System.dealloc(ptr, layout) }
    }
}

#[global_allocator]
static ALLOCATOR: CountingAllocator = CountingAllocator;

#[test]
fn native_callback_and_control_queue_have_no_heap_operations() {
    let mut engine = Engine::new(128).unwrap();
    for track in 0..MAX_TRACKS {
        engine.load_stopped(track, &[[0.01; 2]; 64]).unwrap();
    }
    let (mut controller, mut renderer) = connect(engine);
    let (mut input_writer, mut input_reader) = looper_audio::capture::connect(128).unwrap();
    let mut output = [0.0_f32; 256];
    ENABLED.store(true, Ordering::SeqCst);
    let result = (|| {
        controller.send(Control::MasterGain {
            gain: 0.5,
            ramp_frames: 240,
        })?;
        for track in 0..MAX_TRACKS {
            controller.send(Control::Now {
                track,
                command: Command::Play,
            })?;
        }
        for _ in 0..100 {
            input_writer.capture(&[0.125_f32; 128], 2);
            for _ in 0..64 {
                let _ = input_reader.pop();
            }
            renderer.render(&[], &mut output, 2);
        }
        // Both overflow and malformed input paths must also stay allocation-free.
        input_writer.capture(&[0.1_f32; 256], 1);
        input_writer.capture(&[0.1_f32], 2);
        for track in 0..MAX_TRACKS {
            controller.send(Control::Now {
                track,
                command: Command::Mute(true),
            })?;
        }
        renderer.render(&[], &mut output, 2);
        Ok::<_, Control>(())
    })();
    ENABLED.store(false, Ordering::SeqCst);
    assert!(result.is_ok());
    assert_eq!(OPERATIONS.load(Ordering::SeqCst), 0);
    assert_eq!(controller.status().frames, 12928);
    assert_eq!(controller.status().command_errors, 0);
    assert_eq!(input_reader.status().frames, 6656);
    assert_eq!(input_reader.status().dropped_frames, 128);
    assert_eq!(input_reader.status().malformed_blocks, 1);

    // Native duplex capture, mixing, monitoring and controls share the same
    // no-allocation requirement, including malformed/error paths.
    let mut engine = Engine::new(128).unwrap();
    for track in 0..MAX_TRACKS {
        engine.load_stopped(track, &[[0.01; 2]; 64]).unwrap();
    }
    let (mut writer, reader) = looper_audio::capture::connect(128).unwrap();
    let (mut control, mut duplex) =
        looper_audio::duplex::connect(engine, reader, writer.metrics(), 0.5).unwrap();
    let mut data = [0.0_f32; 128];
    OPERATIONS.store(0, Ordering::SeqCst);
    ENABLED.store(true, Ordering::SeqCst);
    for track in 0..MAX_TRACKS {
        control
            .send(looper_audio::duplex::Control::Track {
                track,
                command: Command::Play,
            })
            .unwrap();
    }
    control
        .send(looper_audio::duplex::Control::Monitor(true))
        .unwrap();
    for _ in 0..100 {
        writer.capture(&[0.125_f32; 128], 2);
        duplex.render(&mut data, 2);
    }
    control
        .send(looper_audio::duplex::Control::Track {
            track: 0,
            command: Command::Record,
        })
        .unwrap();
    writer.capture(&[0.125_f32; 128], 2);
    duplex.render(&mut data, 2);
    duplex.render(&mut data[..3], 2);
    ENABLED.store(false, Ordering::SeqCst);
    assert_eq!(OPERATIONS.load(Ordering::SeqCst), 0);
    assert_eq!(control.status().underruns, 0);
    assert_eq!(control.status().command_errors, 1);
    assert_eq!(control.status().malformed, 1);
}
