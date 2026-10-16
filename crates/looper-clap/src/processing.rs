use super::*;
use clap_sys::{
    audio_buffer::*, events::*, ext::audio_ports::*, fixedpoint::*, id::CLAP_INVALID_ID, process::*,
};
use looper_host::{Action, Event, Input, Transport, MAX_EVENTS};
use std::mem::{align_of, size_of};

pub(super) static PORTS: clap_plugin_audio_ports = clap_plugin_audio_ports {
    count: Some(port_count),
    get: Some(port_get),
};
unsafe extern "C" fn port_count(_plugin: *const clap_plugin, _input: bool) -> u32 {
    1
}
unsafe extern "C" fn port_get(
    _plugin: *const clap_plugin,
    index: u32,
    input: bool,
    out: *mut clap_audio_port_info,
) -> bool {
    guard(false, || unsafe {
        if index != 0 || out.is_null() {
            return false;
        }
        let mut info: clap_audio_port_info = std::mem::zeroed();
        info.id = if input { 0 } else { 1 };
        params::text(
            &mut info.name,
            if input {
                "Stereo input"
            } else {
                "Stereo output"
            },
        );
        info.flags = CLAP_AUDIO_PORT_IS_MAIN;
        info.channel_count = 2;
        info.port_type = CLAP_PORT_STEREO.as_ptr();
        info.in_place_pair = CLAP_INVALID_ID;
        out.write(info);
        true
    })
}

pub(super) struct Batch {
    pub events: [Event; MAX_EVENTS],
    pub length: usize,
    updates: [Option<f64>; PARAM_COUNT],
}
impl Batch {
    pub unsafe fn read(list: *const clap_input_events, frames: usize) -> Result<Self, ()> {
        let mut batch = Self {
            events: [Event {
                offset: 0,
                action: Action::MasterGain {
                    gain: 1.0,
                    ramp_frames: 0,
                },
            }; MAX_EVENTS],
            length: 0,
            updates: [None; PARAM_COUNT],
        };
        if list.is_null() {
            return Ok(batch);
        }
        // The host guarantees each callback result points at a readable event.
        let list = unsafe { &*list };
        let (Some(size), Some(get)) = (list.size, list.get) else {
            return Err(());
        };
        let count = unsafe { size(list) } as usize;
        if count > MAX_EVENTS {
            return Err(());
        }
        let mut prior = 0;
        for index in 0..count {
            let raw = unsafe { get(list, index as u32) };
            if raw.is_null() {
                return Err(());
            }
            let header = unsafe { raw.read_unaligned() };
            let offset = if frames == 0 { 0 } else { header.time as usize };
            if header.size < size_of::<clap_event_header>() as u32
                || (frames > 0 && (offset >= frames || offset < prior))
            {
                return Err(());
            }
            prior = offset;
            if header.space_id != CLAP_CORE_EVENT_SPACE_ID || header.type_ != CLAP_EVENT_PARAM_VALUE
            {
                continue;
            }
            if header.size < size_of::<clap_event_param_value>() as u32 {
                return Err(());
            }
            let event = unsafe { raw.cast::<clap_event_param_value>().read_unaligned() };
            let Some(index) = params::param_index(event.param_id) else {
                continue;
            };
            if event.note_id != -1
                || event.port_index != -1
                || event.channel != -1
                || event.key != -1
            {
                continue;
            }
            if !params::valid(index, event.value) {
                return Err(());
            }
            batch.updates[index] = Some(event.value);
            if let Some(action) = params::action(index, event.value) {
                batch.events[batch.length] = Event { offset, action };
                batch.length += 1;
            }
        }
        Ok(batch)
    }
    pub fn commit(&self, i: &Instance) {
        for (index, v) in self.updates.iter().enumerate() {
            if let Some(v) = v {
                i.values[index].store(v.to_bits(), Ordering::Relaxed);
            }
        }
    }
}

unsafe fn channels(buffer: *const clap_audio_buffer, frames: usize) -> Result<[*mut f32; 2], ()> {
    if buffer.is_null() {
        return Err(());
    }
    let b = unsafe { &*buffer };
    if b.channel_count != 2 || b.data32.is_null() || !b.data64.is_null() {
        return Err(());
    }
    let pointers = unsafe { [b.data32.read(), b.data32.add(1).read()] };
    let bytes = frames.checked_mul(size_of::<f32>()).ok_or(())?;
    for p in pointers {
        if p.is_null()
            || !(p as usize).is_multiple_of(align_of::<f32>())
            || (p as usize).checked_add(bytes).is_none()
        {
            return Err(());
        }
    }
    Ok(pointers)
}
fn overlapping(p: [*mut f32; 2], frames: usize) -> bool {
    let a = p[0] as usize;
    let b = p[1] as usize;
    let bytes = frames * size_of::<f32>();
    a < b + bytes && b < a + bytes
}
unsafe fn transport(raw: *const clap_event_transport, rate: f64) -> Result<Option<Transport>, ()> {
    if raw.is_null() {
        return Ok(None);
    }
    let t = unsafe { raw.read_unaligned() };
    if t.header.size < size_of::<clap_event_transport>() as u32
        || t.header.space_id != CLAP_CORE_EVENT_SPACE_ID
        || t.header.type_ != CLAP_EVENT_TRANSPORT
    {
        return Err(());
    }
    let sample_position = if t.flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE != 0 {
        let frames = t.song_pos_seconds as f64 / CLAP_SECTIME_FACTOR as f64 * rate;
        if frames < i64::MIN as f64 || frames >= i64::MAX as f64 {
            return Err(());
        }
        Some(frames.round() as i64)
    } else {
        None
    };
    Ok(Some(Transport {
        sample_position,
        quarter_position: (t.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE != 0)
            .then_some(t.song_pos_beats as f64 / CLAP_BEATTIME_FACTOR as f64),
        tempo: (t.flags & CLAP_TRANSPORT_HAS_TEMPO != 0).then_some(t.tempo),
        signature: (t.flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE != 0)
            .then_some((t.tsig_num, t.tsig_denom)),
        playing: t.flags & CLAP_TRANSPORT_IS_PLAYING != 0,
    }))
}

pub(super) unsafe extern "C" fn process(
    plugin: *const clap_plugin,
    raw: *const clap_process,
) -> clap_process_status {
    guard(CLAP_PROCESS_ERROR, || unsafe {
        let Some(i) = instance(plugin) else {
            return CLAP_PROCESS_ERROR;
        };
        if raw.is_null() {
            return CLAP_PROCESS_ERROR;
        }
        let a = audio(i);
        let Some(p) = a.processor.as_mut() else {
            return CLAP_PROCESS_ERROR;
        };
        if !a.processing || !a.activated {
            return CLAP_PROCESS_ERROR;
        }
        let call = &*raw;
        let frames = call.frames_count as usize;
        if frames > a.scratch_left.len() || call.audio_outputs_count != 1 {
            return CLAP_PROCESS_ERROR;
        }
        let Ok(out) = channels(call.audio_outputs, frames) else {
            return CLAP_PROCESS_ERROR;
        };
        if overlapping(out, frames) {
            return CLAP_PROCESS_ERROR;
        }
        let result = (|| {
            if call.audio_inputs_count != 1 {
                return Err(());
            }
            let input = channels(call.audio_inputs, frames)?;
            // Snapshot input before borrowing output mutably: tolerates hosts
            // reusing input memory despite our declared separate-buffer layout.
            a.scratch_left[..frames].copy_from_slice(std::slice::from_raw_parts(input[0], frames));
            a.scratch_right[..frames].copy_from_slice(std::slice::from_raw_parts(input[1], frames));
            let batch = Batch::read(call.in_events, frames)?;
            let t = transport(call.transport, p.sample_rate())?;
            let left = std::slice::from_raw_parts_mut(out[0], frames);
            let right = std::slice::from_raw_parts_mut(out[1], frames);
            let changed = batch.events[..batch.length]
                .iter()
                .any(|e| matches!(e.action, Action::Track { .. }))
                || p.is_capturing()
                || p.engine().schedule_status().pending > 0;
            let transaction = if changed {
                a.checkpoint.as_ref().map(|c| c.begin())
            } else {
                None
            };
            p.process(
                Input::Stereo {
                    left: &a.scratch_left[..frames],
                    right: &a.scratch_right[..frames],
                },
                left,
                right,
                &batch.events[..batch.length],
                t,
            )
            .map_err(|_| ())?;
            batch.commit(i);
            if changed {
                if let Some(c) = &a.checkpoint {
                    c.publish(p.engine());
                }
            }
            drop(transaction);
            (*call.audio_outputs).constant_mask = 0;
            Ok(())
        })();
        if result.is_err() {
            std::slice::from_raw_parts_mut(out[0], frames).fill(0.0);
            std::slice::from_raw_parts_mut(out[1], frames).fill(0.0);
            CLAP_PROCESS_ERROR
        } else {
            CLAP_PROCESS_CONTINUE
        }
    })
}
