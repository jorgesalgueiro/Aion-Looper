#![allow(dead_code)]
use clap_sys::{
    audio_buffer::*, entry::*, events::*, ext::params::*, ext::state::*,
    factory::plugin_factory::*, host::*, plugin::*, process::*, stream::*, version::*,
};
use std::{
    ffi::{c_void, CStr},
    mem::size_of,
    ptr,
};

unsafe extern "C" fn no_extension(
    _host: *const clap_host,
    _id: *const std::ffi::c_char,
) -> *const c_void {
    ptr::null()
}
unsafe extern "C" fn no_request(_host: *const clap_host) {}
pub struct Harness {
    pub plugin: *const clap_plugin,
    pub factory: *const clap_plugin_factory,
    _host: Box<clap_host>,
}
impl Harness {
    pub fn state(&self) -> &clap_plugin_state {
        unsafe { &*self.extension(CLAP_EXT_STATE).cast::<clap_plugin_state>() }
    }
    pub fn save(&self, bytes: &mut StateBytes) -> bool {
        let stream = clap_ostream {
            ctx: (bytes as *mut StateBytes).cast(),
            write: Some(state_write),
        };
        unsafe { self.state().save.unwrap()(self.plugin, &stream) }
    }
    pub fn load(&self, bytes: &mut StateBytes) -> bool {
        let stream = clap_istream {
            ctx: (bytes as *mut StateBytes).cast(),
            read: Some(state_read),
        };
        unsafe { self.state().load.unwrap()(self.plugin, &stream) }
    }
    pub fn new(entry: &clap_plugin_entry) -> Self {
        unsafe {
            assert!(entry.init.unwrap()(c"test-host".as_ptr()));
            let factory = entry.get_factory.unwrap()(CLAP_PLUGIN_FACTORY_ID.as_ptr())
                .cast::<clap_plugin_factory>();
            assert!(!factory.is_null());
            let desc = (*factory).get_plugin_descriptor.unwrap()(factory, 0);
            assert!(!desc.is_null());
            let host = Box::new(clap_host {
                clap_version: CLAP_VERSION,
                host_data: ptr::null_mut(),
                name: c"Rust-Looper test host".as_ptr(),
                vendor: c"Rust-Looper".as_ptr(),
                url: c"".as_ptr(),
                version: c"1".as_ptr(),
                get_extension: Some(no_extension),
                request_restart: Some(no_request),
                request_process: Some(no_request),
                request_callback: Some(no_request),
            });
            let plugin = (*factory).create_plugin.unwrap()(factory, &*host, (*desc).id);
            assert!(!plugin.is_null());
            assert!((*plugin).init.unwrap()(plugin));
            Self {
                plugin,
                factory,
                _host: host,
            }
        }
    }
    pub fn activate(&self, max: u32) -> bool {
        unsafe { (*self.plugin).activate.unwrap()(self.plugin, 8_000.0, 1, max) }
    }
    pub fn start(&self) -> bool {
        unsafe { (*self.plugin).start_processing.unwrap()(self.plugin) }
    }
    pub fn stop(&self) {
        unsafe { (*self.plugin).stop_processing.unwrap()(self.plugin) }
    }
    pub fn deactivate(&self) {
        unsafe { (*self.plugin).deactivate.unwrap()(self.plugin) }
    }
    pub fn reset(&self) {
        unsafe { (*self.plugin).reset.unwrap()(self.plugin) }
    }
    pub fn params(&self) -> &clap_plugin_params {
        unsafe {
            &*(*self.plugin).get_extension.unwrap()(self.plugin, CLAP_EXT_PARAMS.as_ptr())
                .cast::<clap_plugin_params>()
        }
    }
    pub fn value(&self, id: u32) -> f64 {
        let mut value = 0.0;
        assert!(unsafe { self.params().get_value.unwrap()(self.plugin, id, &mut value) });
        value
    }
    pub fn extension(&self, id: &CStr) -> *const c_void {
        unsafe { (*self.plugin).get_extension.unwrap()(self.plugin, id.as_ptr()) }
    }
    pub fn flush(&self, events: &mut Events) {
        let list = events.list();
        unsafe { self.params().flush.unwrap()(self.plugin, &list, ptr::null()) }
    }
    pub fn render(
        &self,
        left: &[f32],
        right: &[f32],
        events: &mut Events,
        in_place: bool,
    ) -> (i32, Vec<f32>, Vec<f32>) {
        assert_eq!(left.len(), right.len());
        let mut in_l = left.to_vec();
        let mut in_r = right.to_vec();
        let mut out_l = vec![99.0; left.len()];
        let mut out_r = vec![99.0; left.len()];
        let mut input = [in_l.as_mut_ptr(), in_r.as_mut_ptr()];
        let mut output = if in_place {
            input
        } else {
            [out_l.as_mut_ptr(), out_r.as_mut_ptr()]
        };
        let input_buffer = buffer(&mut input);
        let mut output_buffer = buffer(&mut output);
        let list = events.list();
        let process = call(left.len(), &input_buffer, &mut output_buffer, &list);
        let result = unsafe { (*self.plugin).process.unwrap()(self.plugin, &process) };
        if in_place {
            (result, in_l, in_r)
        } else {
            (result, out_l, out_r)
        }
    }
}
pub struct StateBytes {
    pub data: Vec<u8>,
    pub position: usize,
    pub chunk: usize,
    pub fail_at: usize,
    pub invalid_count: bool,
}
impl StateBytes {
    pub fn new(data: Vec<u8>) -> Self {
        Self {
            data,
            position: 0,
            chunk: 7,
            fail_at: usize::MAX,
            invalid_count: false,
        }
    }
}
unsafe extern "C" fn state_write(
    stream: *const clap_ostream,
    data: *const c_void,
    count: u64,
) -> i64 {
    unsafe {
        let bytes = &mut *((*stream).ctx as *mut StateBytes);
        if bytes.invalid_count {
            return count as i64 + 1;
        }
        if bytes.position >= bytes.fail_at {
            return -1;
        }
        let count = (count as usize)
            .min(bytes.chunk)
            .min(bytes.fail_at - bytes.position);
        bytes
            .data
            .extend_from_slice(std::slice::from_raw_parts(data.cast::<u8>(), count));
        bytes.position += count;
        count as i64
    }
}
unsafe extern "C" fn state_read(stream: *const clap_istream, data: *mut c_void, count: u64) -> i64 {
    unsafe {
        let bytes = &mut *((*stream).ctx as *mut StateBytes);
        if bytes.invalid_count {
            return count as i64 + 1;
        }
        if bytes.position >= bytes.fail_at {
            return -1;
        }
        let count = (count as usize)
            .min(bytes.chunk)
            .min(bytes.data.len() - bytes.position)
            .min(bytes.fail_at - bytes.position);
        std::ptr::copy_nonoverlapping(
            bytes.data.as_ptr().add(bytes.position),
            data.cast::<u8>(),
            count,
        );
        bytes.position += count;
        count as i64
    }
}
impl Drop for Harness {
    fn drop(&mut self) {
        unsafe {
            (*self.plugin).stop_processing.unwrap()(self.plugin);
            (*self.plugin).deactivate.unwrap()(self.plugin);
            (*self.plugin).destroy.unwrap()(self.plugin);
        }
    }
}
pub fn buffer(pointers: &mut [*mut f32; 2]) -> clap_audio_buffer {
    clap_audio_buffer {
        data32: pointers.as_mut_ptr(),
        data64: ptr::null_mut(),
        channel_count: 2,
        latency: 0,
        constant_mask: 0,
    }
}
pub fn call(
    frames: usize,
    input: &clap_audio_buffer,
    output: &mut clap_audio_buffer,
    events: &clap_input_events,
) -> clap_process {
    clap_process {
        steady_time: -1,
        frames_count: frames as u32,
        transport: ptr::null(),
        audio_inputs: input,
        audio_outputs: output,
        audio_inputs_count: 1,
        audio_outputs_count: 1,
        in_events: events,
        out_events: ptr::null(),
    }
}
pub struct Events(pub Vec<clap_event_param_value>);
impl Events {
    pub fn new(values: &[(u32, u32, f64)]) -> Self {
        Self(
            values
                .iter()
                .map(|&(time, param_id, value)| clap_event_param_value {
                    header: clap_event_header {
                        size: size_of::<clap_event_param_value>() as u32,
                        time,
                        space_id: CLAP_CORE_EVENT_SPACE_ID,
                        type_: CLAP_EVENT_PARAM_VALUE,
                        flags: 0,
                    },
                    param_id,
                    cookie: ptr::null_mut(),
                    note_id: -1,
                    port_index: -1,
                    channel: -1,
                    key: -1,
                    value,
                })
                .collect(),
        )
    }
    pub fn list(&mut self) -> clap_input_events {
        clap_input_events {
            ctx: (self as *mut Self).cast(),
            size: Some(event_count),
            get: Some(event_get),
        }
    }
}
unsafe extern "C" fn event_count(list: *const clap_input_events) -> u32 {
    unsafe { (&*((*list).ctx as *const Events)).0.len() as u32 }
}
unsafe extern "C" fn event_get(
    list: *const clap_input_events,
    index: u32,
) -> *const clap_event_header {
    unsafe {
        (&*((*list).ctx as *const Events))
            .0
            .get(index as usize)
            .map_or(ptr::null(), |e| &e.header)
    }
}
