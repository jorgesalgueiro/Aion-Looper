use std::alloc::{GlobalAlloc, Layout, System};
mod common;
use common::*;
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
fn clap_processing_flush_reset_and_twenty_tracks_have_no_heap_operations() {
    let h = Harness::new(&looper_clap::clap_entry);
    assert!(h.activate(64));
    assert!(h.start());
    let mut left = [0.25; 64];
    let mut right = [-0.5; 64];
    let mut pointers = [left.as_mut_ptr(), right.as_mut_ptr()];
    let input = buffer(&mut pointers);
    let mut output_pointers = pointers;
    let mut output = buffer(&mut output_pointers);
    let mut events = Events::new(&(100..120).map(|id| (0, id, 1.0)).collect::<Vec<_>>());
    let list = events.list();
    let c = call(64, &input, &mut output, &list);
    let mut gains = Events::new(&[(0, 0, 0.5), (0, 1, 0.25)]);
    let gains_list = gains.list();
    let callback = unsafe { (*h.plugin).process.unwrap() };
    let flush = h.params().flush.unwrap();
    ACTIVE.store(true, Ordering::SeqCst);
    let result = unsafe { callback(h.plugin, &c) };
    unsafe { flush(h.plugin, &gains_list, std::ptr::null()) };
    h.reset();
    ACTIVE.store(false, Ordering::SeqCst);
    assert_eq!(OPS.load(Ordering::SeqCst), 0);
    assert_eq!(result, clap_sys::process::CLAP_PROCESS_CONTINUE);
    assert_eq!(h.value(0), 0.5);
}
