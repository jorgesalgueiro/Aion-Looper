//! Native CLAP prototype. See docs/CLAP.md for current limitations.
// Raw pointers are confined to this ABI boundary. The core/host crates forbid
// unsafe code. Hosts must obey CLAP pointer lifetime and callback threading rules.
#![deny(unsafe_op_in_unsafe_fn)]

mod params;
mod processing;
mod state;
mod state_codec;

use clap_sys::{entry::*, factory::plugin_factory::*, host::*, plugin::*, version::*};
use looper_host::Processor;
use std::{
    cell::UnsafeCell,
    ffi::{c_char, c_void, CStr},
    panic::{catch_unwind, AssertUnwindSafe},
    ptr,
    sync::{
        atomic::{AtomicBool, AtomicU64, Ordering},
        Arc,
    },
};

pub const PLUGIN_ID: &CStr = c"org.jorgesalgueiro.rust-looper.prototype";
pub const PARAM_COUNT: usize = 22;
const FEATURES: [*const c_char; 4] = [
    c"audio-effect".as_ptr(),
    c"utility".as_ptr(),
    c"stereo".as_ptr(),
    ptr::null(),
];
static DESCRIPTOR: clap_plugin_descriptor = clap_plugin_descriptor {
    clap_version: CLAP_VERSION,
    id: PLUGIN_ID.as_ptr(),
    name: c"Aion Looper Prototype".as_ptr(),
    vendor: c"Jorge Salgueiro".as_ptr(),
    url: c"https://github.com/jorgesalgueiro/Aion-Looper".as_ptr(),
    manual_url: c"".as_ptr(),
    support_url: c"".as_ptr(),
    version: c"0.1.0".as_ptr(),
    description: c"Native looper prototype; editor and MIDI pending".as_ptr(),
    features: FEATURES.as_ptr(),
};

struct Audio {
    processor: Option<Processor>,
    checkpoint: Option<Arc<looper_core::checkpoint::Checkpoint>>,
    scratch_left: Vec<f32>,
    scratch_right: Vec<f32>,
    initialized: bool,
    activated: bool,
    processing: bool,
}
struct Instance {
    plugin: clap_plugin,
    values: [AtomicU64; PARAM_COUNT],
    audio: UnsafeCell<Audio>,
    main: UnsafeCell<Main>,
    active: AtomicBool,
    pending_state: AtomicBool,
    pending_values: [AtomicU64; PARAM_COUNT],
    host: *const clap_host,
}
// Main-thread callbacks alone access this view; audio never changes it.
struct Main {
    view: Option<(f64, Arc<looper_core::checkpoint::Checkpoint>)>,
    pending: Option<state::Pending>,
}
// CLAP serializes activation/process/flush/deactivation; only the atomic parameter
// values are read concurrently by get_value. No &mut Instance is ever created.
unsafe fn instance<'a>(plugin: *const clap_plugin) -> Option<&'a Instance> {
    if plugin.is_null() {
        return None;
    }
    unsafe { ((*plugin).plugin_data as *const Instance).as_ref() }
}
#[allow(clippy::mut_from_ref)] // UnsafeCell + the caller's exclusive CLAP callback role.
unsafe fn audio(instance: &Instance) -> &mut Audio {
    // Caller must hold the exclusive CLAP lifecycle/audio callback role.
    unsafe { &mut *instance.audio.get() }
}
fn guard<T>(fallback: T, f: impl FnOnce() -> T) -> T {
    catch_unwind(AssertUnwindSafe(f)).unwrap_or(fallback)
}

unsafe extern "C" fn entry_init(_path: *const c_char) -> bool {
    true
}
unsafe extern "C" fn entry_deinit() {}
unsafe extern "C" fn get_factory(id: *const c_char) -> *const c_void {
    guard(ptr::null(), || unsafe {
        if !id.is_null() && CStr::from_ptr(id) == CLAP_PLUGIN_FACTORY_ID {
            (&FACTORY as *const clap_plugin_factory).cast()
        } else {
            ptr::null()
        }
    })
}
#[no_mangle]
pub static clap_entry: clap_plugin_entry = clap_plugin_entry {
    clap_version: CLAP_VERSION,
    init: Some(entry_init),
    deinit: Some(entry_deinit),
    get_factory: Some(get_factory),
};
static FACTORY: clap_plugin_factory = clap_plugin_factory {
    get_plugin_count: Some(count),
    get_plugin_descriptor: Some(descriptor),
    create_plugin: Some(create),
};
unsafe extern "C" fn count(_factory: *const clap_plugin_factory) -> u32 {
    1
}
unsafe extern "C" fn descriptor(
    _factory: *const clap_plugin_factory,
    index: u32,
) -> *const clap_plugin_descriptor {
    if index == 0 {
        &DESCRIPTOR
    } else {
        ptr::null()
    }
}
unsafe extern "C" fn create(
    _factory: *const clap_plugin_factory,
    host: *const clap_host,
    id: *const c_char,
) -> *const clap_plugin {
    guard(ptr::null(), || unsafe {
        if host.is_null()
            || id.is_null()
            || !clap_version_is_compatible((*host).clap_version)
            || CStr::from_ptr(id) != PLUGIN_ID
        {
            return ptr::null();
        }
        let mut boxed = Box::new(Instance {
            plugin: clap_plugin {
                desc: &DESCRIPTOR,
                plugin_data: ptr::null_mut(),
                init: Some(init),
                destroy: Some(destroy),
                activate: Some(activate),
                deactivate: Some(deactivate),
                start_processing: Some(start),
                stop_processing: Some(stop),
                reset: Some(reset),
                process: Some(processing::process),
                get_extension: Some(extension),
                on_main_thread: Some(on_main),
            },
            values: std::array::from_fn(|i| {
                AtomicU64::new(if i == 0 {
                    1.0_f64.to_bits()
                } else {
                    0.0_f64.to_bits()
                })
            }),
            audio: UnsafeCell::new(Audio {
                processor: None,
                checkpoint: None,
                scratch_left: Vec::new(),
                scratch_right: Vec::new(),
                initialized: false,
                activated: false,
                processing: false,
            }),
            main: UnsafeCell::new(Main {
                view: None,
                pending: None,
            }),
            active: AtomicBool::new(false),
            pending_state: AtomicBool::new(false),
            pending_values: std::array::from_fn(|_| AtomicU64::new(0)),
            host,
        });
        boxed.plugin.plugin_data = (&mut *boxed as *mut Instance).cast();
        let instance = Box::into_raw(boxed);
        &(*instance).plugin
    })
}
unsafe extern "C" fn init(plugin: *const clap_plugin) -> bool {
    guard(false, || unsafe {
        let Some(i) = instance(plugin) else {
            return false;
        };
        let a = audio(i);
        if a.initialized {
            return false;
        }
        a.initialized = true;
        true
    })
}
unsafe extern "C" fn destroy(plugin: *const clap_plugin) {
    guard((), || unsafe {
        if let Some(i) = instance(plugin) {
            drop(Box::from_raw(i as *const Instance as *mut Instance));
        }
    })
}
unsafe extern "C" fn activate(plugin: *const clap_plugin, rate: f64, min: u32, max: u32) -> bool {
    guard(false, || unsafe {
        let Some(i) = instance(plugin) else {
            return false;
        };
        let a = audio(i);
        if !a.initialized || a.activated || min == 0 || min > max {
            return false;
        }
        // Eight seconds per loop, allocated before processing. No resizing in callbacks.
        let capacity = (rate * 8.0) as usize;
        if let Some(p) = a.processor.as_mut() {
            if p.sample_rate() != rate || p.set_max_block_frames(max as usize).is_err() {
                return false;
            }
        } else {
            let Ok((p, checkpoint)) =
                Processor::prepare_shared(rate, max as usize, [capacity; looper_core::MAX_TRACKS])
            else {
                return false;
            };
            a.processor = Some(p);
            (*i.main.get()).view = Some((rate, Arc::clone(&checkpoint)));
            a.checkpoint = Some(checkpoint);
        }
        let p = a.processor.as_mut().unwrap();
        if p.activate().is_err() {
            return false;
        }
        let controls = params::gain_events(i);
        if p.process(
            looper_host::Input::Silent,
            &mut [],
            &mut [],
            &controls,
            None,
        )
        .is_err()
        {
            return false;
        }
        a.scratch_left = vec![0.0; max as usize];
        a.scratch_right = vec![0.0; max as usize];
        a.activated = true;
        i.active.store(true, Ordering::Release);
        true
    })
}
unsafe extern "C" fn deactivate(plugin: *const clap_plugin) {
    guard((), || unsafe {
        if let Some(i) = instance(plugin) {
            let a = audio(i);
            a.processing = false;
            a.activated = false;
            if let Some(p) = a.processor.as_mut() {
                p.deactivate();
            }
            state::apply_pending(i, a);
            a.scratch_left.clear();
            a.scratch_right.clear();
            i.active.store(false, Ordering::Release);
        }
    })
}
unsafe extern "C" fn start(plugin: *const clap_plugin) -> bool {
    guard(false, || unsafe {
        let Some(i) = instance(plugin) else {
            return false;
        };
        let a = audio(i);
        if a.processing || !a.activated {
            return false;
        }
        a.processing = true;
        true
    })
}
unsafe extern "C" fn stop(plugin: *const clap_plugin) {
    guard((), || unsafe {
        if let Some(i) = instance(plugin) {
            audio(i).processing = false;
        }
    })
}
unsafe extern "C" fn reset(plugin: *const clap_plugin) {
    guard((), || unsafe {
        if let Some(i) = instance(plugin) {
            let a = audio(i);
            if let Some(p) = a.processor.as_mut() {
                let transaction = a.checkpoint.as_ref().map(|c| c.begin());
                p.reset();
                if let Some(c) = &a.checkpoint {
                    c.publish(p.engine());
                }
                drop(transaction);
            }
        }
    })
}
unsafe extern "C" fn on_main(_plugin: *const clap_plugin) {}
unsafe extern "C" fn extension(_plugin: *const clap_plugin, id: *const c_char) -> *const c_void {
    guard(ptr::null(), || unsafe {
        if id.is_null() {
            return ptr::null();
        }
        let id = CStr::from_ptr(id);
        if id == clap_sys::ext::audio_ports::CLAP_EXT_AUDIO_PORTS {
            (&processing::PORTS as *const clap_sys::ext::audio_ports::clap_plugin_audio_ports)
                .cast()
        } else if id == clap_sys::ext::params::CLAP_EXT_PARAMS {
            (&params::PARAMS as *const clap_sys::ext::params::clap_plugin_params).cast()
        } else if id == clap_sys::ext::state::CLAP_EXT_STATE {
            (&state::STATE as *const clap_sys::ext::state::clap_plugin_state).cast()
        } else {
            ptr::null()
        }
    })
}

fn value(instance: &Instance, index: usize) -> f64 {
    let values = if instance.pending_state.load(Ordering::Acquire) {
        &instance.pending_values
    } else {
        &instance.values
    };
    f64::from_bits(values[index].load(Ordering::Relaxed))
}
