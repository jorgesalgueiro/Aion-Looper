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
fn midi_callback_capture_and_overflow_have_no_heap_operations() {
    let (mut writer, mut reader) = looper_midi::transport::channel(std::time::Instant::now());
    ACTIVE.store(true, Ordering::SeqCst);
    for _ in 0..4096 {
        writer.capture(1, &[0x90, 60, 100]);
        writer.capture(2, &[0xF0, 0xF7]);
    }
    ACTIVE.store(false, Ordering::SeqCst);
    assert_eq!(OPS.load(Ordering::SeqCst), 0);
    assert_eq!(reader.dropped(), 3072);
    reader.discard();
    assert!(reader.pop().is_none());
}
