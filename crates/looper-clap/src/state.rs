use super::*;
use clap_sys::{
    ext::{params::*, state::*},
    stream::*,
};
use looper_core::{Command, MAX_TRACKS};
use std::io::{self, Read, Write};
pub(super) static STATE: clap_plugin_state = clap_plugin_state {
    save: Some(save),
    load: Some(load),
};
pub(super) struct Pending {
    rate: f64,
    prepared: Option<(Processor, Arc<looper_core::checkpoint::Checkpoint>)>,
    parameters: [f64; PARAM_COUNT],
}
// Main-thread only, after the host has stopped its audio callback.
pub(super) unsafe fn apply_pending(i: &Instance, a: &mut Audio) {
    unsafe {
        let main = &mut *i.main.get();
        let Some(pending) = main.pending.take() else {
            return;
        };
        match pending.prepared {
            Some((p, c)) => {
                a.processor = Some(p);
                main.view = Some((pending.rate, Arc::clone(&c)));
                a.checkpoint = Some(c);
            }
            None => {
                a.processor = None;
                a.checkpoint = None;
                main.view = None;
            }
        }
        for (to, v) in i.values.iter().zip(pending.parameters) {
            to.store(v.to_bits(), Ordering::Relaxed);
        }
        i.pending_state.store(false, Ordering::Release);
    }
}
struct Output<'a>(&'a clap_ostream);
impl Write for Output<'_> {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        let Some(write) = self.0.write else {
            return Err(io::ErrorKind::BrokenPipe.into());
        };
        let n = unsafe { write(self.0, data.as_ptr().cast(), data.len() as u64) };
        if n <= 0 || n as usize > data.len() {
            return Err(io::ErrorKind::BrokenPipe.into());
        }
        Ok(n as usize)
    }
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
struct InputStream<'a>(&'a clap_istream);
impl Read for InputStream<'_> {
    fn read(&mut self, data: &mut [u8]) -> io::Result<usize> {
        let Some(read) = self.0.read else {
            return Err(io::ErrorKind::UnexpectedEof.into());
        };
        let n = unsafe { read(self.0, data.as_mut_ptr().cast(), data.len() as u64) };
        if n < 0 || n as usize > data.len() {
            return Err(io::ErrorKind::UnexpectedEof.into());
        }
        Ok(n as usize)
    }
}
unsafe extern "C" fn save(plugin: *const clap_plugin, stream: *const clap_ostream) -> bool {
    guard(false, || unsafe {
        let Some(i) = instance(plugin) else {
            return false;
        };
        let Some(stream) = stream.as_ref() else {
            return false;
        };
        // CLAP main-thread only. Never touch Audio while the host may process it.
        let main = &*i.main.get();
        let parameters = || std::array::from_fn(|index| value(i, index));
        let session = if let Some(pending) = &main.pending {
            if let Some((_, checkpoint)) = &pending.prepared {
                let Ok((takes, parameters)) =
                    checkpoint.snapshot(state_codec::MAX_FRAMES, || pending.parameters)
                else {
                    return false;
                };
                state_codec::Session {
                    rate: pending.rate,
                    parameters,
                    takes,
                }
            } else {
                state_codec::Session::empty(pending.parameters)
            }
        } else if let Some((rate, checkpoint)) = &main.view {
            let Ok((takes, parameters)) = checkpoint.snapshot(state_codec::MAX_FRAMES, parameters)
            else {
                return false;
            };
            state_codec::Session {
                rate: *rate,
                parameters,
                takes,
            }
        } else {
            state_codec::Session::empty(parameters())
        };
        state_codec::encode(&session, Output(stream)).is_ok()
    })
}
unsafe extern "C" fn load(plugin: *const clap_plugin, stream: *const clap_istream) -> bool {
    guard(false, || unsafe {
        let Some(i) = instance(plugin) else {
            return false;
        };
        let Some(stream) = stream.as_ref() else {
            return false;
        };
        let active = i.active.load(Ordering::Acquire);
        if active && (*i.host).request_restart.is_none() {
            return false;
        }
        let Ok(session) = state_codec::decode(InputStream(stream)) else {
            return false;
        };
        let prepared = if session.rate == 0.0 {
            None
        } else {
            let capacity = (session.rate * 8.0) as usize;
            let Ok((mut p, c)) = Processor::prepare_shared(session.rate, 1, [capacity; MAX_TRACKS])
            else {
                return false;
            };
            let Ok(engine) = p.engine_mut() else {
                return false;
            };
            for (track, take) in session.takes.iter().enumerate() {
                if !take.audio.is_empty() && engine.load_stopped(track, &take.audio).is_err() {
                    return false;
                }
                for command in [
                    Command::Gain(take.gain),
                    Command::Feedback(take.feedback),
                    Command::Mute(take.muted),
                ] {
                    if engine.command(track, command).is_err() {
                        return false;
                    }
                }
                if take.playing && engine.command(track, Command::Play).is_err() {
                    return false;
                }
            }
            {
                let _transaction = c.begin();
                c.publish(p.engine());
            }
            Some((p, c))
        };
        // Commit only after complete validation/allocation. Old PCM is reclaimed
        // here on the inactive main thread, never by the processing callback.
        (*i.main.get()).pending = Some(Pending {
            rate: session.rate,
            prepared,
            parameters: session.parameters,
        });
        for (to, v) in i.pending_values.iter().zip(session.parameters) {
            to.store(v.to_bits(), Ordering::Relaxed);
        }
        i.pending_state.store(true, Ordering::Release);
        if active {
            // The host owns the stop/deactivate/reactivate cycle. The current
            // processor keeps running until that boundary; no callback frees PCM.
            (*i.host).request_restart.unwrap()(i.host);
        } else {
            apply_pending(i, audio(i));
        }
        if let Some(get) = (*i.host).get_extension {
            let host_params = get(i.host, CLAP_EXT_PARAMS.as_ptr()).cast::<clap_host_params>();
            if let Some(p) = host_params.as_ref() {
                if let Some(rescan) = p.rescan {
                    rescan(i.host, CLAP_PARAM_RESCAN_VALUES);
                }
            }
        }
        true
    })
}
