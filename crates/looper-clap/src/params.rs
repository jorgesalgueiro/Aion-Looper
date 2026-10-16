use super::*;
use clap_sys::{events::*, ext::params::*};
use looper_core::Command;
use looper_host::{Action, Event, Input};

pub(super) static PARAMS: clap_plugin_params = clap_plugin_params {
    count: Some(count),
    get_info: Some(info),
    get_value: Some(get_value),
    value_to_text: Some(to_text),
    text_to_value: Some(from_text),
    flush: Some(flush),
};
pub(super) fn param_index(id: u32) -> Option<usize> {
    match id {
        0..=1 => Some(id as usize),
        100..=119 => Some(id as usize - 98),
        _ => None,
    }
}
fn param_id(index: usize) -> u32 {
    if index < 2 {
        index as u32
    } else {
        index as u32 + 98
    }
}
pub(super) fn valid(index: usize, v: f64) -> bool {
    v.is_finite()
        && v >= 0.0
        && v <= if index == 0 {
            4.0
        } else if index == 1 {
            1.0
        } else {
            7.0
        }
        && (index < 2 || v.fract() == 0.0)
}
pub(super) fn action(index: usize, v: f64) -> Option<Action> {
    match index {
        0 => Some(Action::MasterGain {
            gain: v as f32,
            ramp_frames: 64,
        }),
        1 => Some(Action::MonitorGain {
            gain: v as f32,
            ramp_frames: 64,
        }),
        _ => Some(Action::Track {
            track: index - 2,
            command: match v as u8 {
                1 => Command::Record,
                2 => Command::Finish,
                3 => Command::Play,
                4 => Command::Stop,
                5 => Command::Clear,
                6 => Command::Overdub,
                7 => Command::Substitute,
                _ => return None,
            },
        }),
    }
}
pub(super) fn gain_events(i: &Instance) -> [Event; 2] {
    [
        Event {
            offset: 0,
            action: Action::MasterGain {
                gain: value(i, 0) as f32,
                ramp_frames: 0,
            },
        },
        Event {
            offset: 0,
            action: Action::MonitorGain {
                gain: value(i, 1) as f32,
                ramp_frames: 0,
            },
        },
    ]
}
pub(super) fn text<const N: usize>(out: &mut [c_char; N], value: &str) {
    for (to, from) in out.iter_mut().take(N.saturating_sub(1)).zip(value.bytes()) {
        *to = from as c_char;
    }
}
unsafe extern "C" fn count(_p: *const clap_plugin) -> u32 {
    PARAM_COUNT as u32
}
unsafe extern "C" fn info(_p: *const clap_plugin, index: u32, out: *mut clap_param_info) -> bool {
    guard(false, || unsafe {
        if index as usize >= PARAM_COUNT || out.is_null() {
            return false;
        }
        let mut p: clap_param_info = std::mem::zeroed();
        p.id = param_id(index as usize);
        p.flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_REQUIRES_PROCESS;
        p.max_value = if index == 0 {
            4.0
        } else if index == 1 {
            1.0
        } else {
            7.0
        };
        p.default_value = if index == 0 { 1.0 } else { 0.0 };
        if index < 2 {
            text(
                &mut p.name,
                if index == 0 {
                    "Master gain"
                } else {
                    "Input monitor"
                },
            );
        } else {
            p.flags |= CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM;
            text(&mut p.name, &format!("Loop {} command", index - 1));
            text(&mut p.module, "Prototype loop commands");
        }
        out.write(p);
        true
    })
}
unsafe extern "C" fn get_value(plugin: *const clap_plugin, id: u32, out: *mut f64) -> bool {
    guard(false, || unsafe {
        let (Some(i), Some(index)) = (instance(plugin), param_index(id)) else {
            return false;
        };
        if out.is_null() {
            return false;
        }
        out.write(value(i, index));
        true
    })
}
unsafe extern "C" fn to_text(
    _p: *const clap_plugin,
    id: u32,
    v: f64,
    out: *mut c_char,
    capacity: u32,
) -> bool {
    guard(false, || unsafe {
        let Some(index) = param_index(id) else {
            return false;
        };
        if out.is_null() || capacity == 0 || !valid(index, v) {
            return false;
        }
        let s = if index >= 2 {
            [
                "Idle",
                "Record",
                "Finish",
                "Play",
                "Stop",
                "Clear",
                "Overdub",
                "Substitute",
            ][v as usize]
                .to_string()
        } else {
            format!("{v:.3}")
        };
        if s.len() + 1 > capacity as usize {
            return false;
        }
        ptr::copy_nonoverlapping(s.as_ptr().cast::<c_char>(), out, s.len());
        out.add(s.len()).write(0);
        true
    })
}
unsafe extern "C" fn from_text(
    _p: *const clap_plugin,
    id: u32,
    input: *const c_char,
    out: *mut f64,
) -> bool {
    guard(false, || unsafe {
        let Some(index) = param_index(id) else {
            return false;
        };
        if input.is_null() || out.is_null() {
            return false;
        }
        let Ok(s) = CStr::from_ptr(input).to_str() else {
            return false;
        };
        let parsed = if index >= 2 {
            [
                "Idle",
                "Record",
                "Finish",
                "Play",
                "Stop",
                "Clear",
                "Overdub",
                "Substitute",
            ]
            .iter()
            .position(|v| v.eq_ignore_ascii_case(s))
            .map(|v| v as f64)
        } else {
            s.parse::<f64>().ok()
        };
        let Some(v) = parsed.filter(|v| valid(index, *v)) else {
            return false;
        };
        out.write(v);
        true
    })
}
unsafe extern "C" fn flush(
    plugin: *const clap_plugin,
    input: *const clap_input_events,
    _output: *const clap_output_events,
) {
    guard((), || unsafe {
        let Some(i) = instance(plugin) else {
            return;
        };
        let Ok(batch) = super::processing::Batch::read(input, 0) else {
            return;
        };
        let a = audio(i);
        let changed = a.activated
            && batch.events[..batch.length]
                .iter()
                .any(|e| matches!(e.action, Action::Track { .. }));
        let transaction = if changed {
            a.checkpoint.as_ref().map(|c| c.begin())
        } else {
            None
        };
        if a.activated {
            let Some(p) = a.processor.as_mut() else {
                return;
            };
            if p.process(
                Input::Silent,
                &mut [],
                &mut [],
                &batch.events[..batch.length],
                None,
            )
            .is_err()
            {
                return;
            }
        }
        // Inactive flush saves values, but does not execute destructive loop commands.
        batch.commit(i);
        if changed {
            if let (Some(c), Some(p)) = (&a.checkpoint, &a.processor) {
                c.publish(p.engine());
            }
        }
        drop(transaction);
    })
}
