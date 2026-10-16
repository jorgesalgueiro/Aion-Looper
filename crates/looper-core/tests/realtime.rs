use looper_core::schedule::ScheduledCommand;
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
fn twenty_tracks_capture_mix_and_clear_without_heap_operations() {
    let mut engine = Engine::new(256).unwrap();
    let input = [[0.01; 2]; 128];
    let mut output = [[0.0; 2]; 128];
    ENABLED.store(true, Ordering::SeqCst);
    let result = (|| {
        for track in 0..MAX_TRACKS {
            engine.command(track, Command::Record)?;
        }
        engine.process(&input, &mut output)?;
        for track in 0..MAX_TRACKS {
            engine.command(track, Command::Finish)?;
        }
        for track in 0..MAX_TRACKS {
            engine.schedule(ScheduledCommand {
                frame: engine.clock_frames() + 32,
                track,
                command: Command::Mute(false),
            })?;
        }
        engine.process(&input, &mut output)?;
        engine.process(&input, &mut output)?;
        for track in 0..MAX_TRACKS {
            engine.command(track, Command::Overdub)?;
        }
        for _ in 0..100 {
            engine.process(&input, &mut output)?;
        }
        for track in 0..MAX_TRACKS {
            engine.command(track, Command::Finish)?;
            engine.command(track, Command::Substitute)?;
        }
        engine.process(&input, &mut output)?;
        for track in 0..MAX_TRACKS {
            engine.command(track, Command::Mute(true))?;
            engine.command(track, Command::Stop)?;
            engine.command(track, Command::Clear)?;
        }
        engine.process(&input, &mut output)
    })();
    ENABLED.store(false, Ordering::SeqCst);
    assert!(result.is_ok());
    assert_eq!(OPERATIONS.load(Ordering::SeqCst), 0);
}
