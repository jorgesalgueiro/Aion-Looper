//! Dedicated MIDI owner thread. Offline stretching cannot delay MIDI polling.
use looper_midi::Hub;
use serde::Deserialize;
use serde_json::{json, Value};
use std::time::{Duration, Instant};
#[derive(Default, Deserialize)]
pub struct Request {
    pub id: u64,
    pub session: String,
    pub operation: String,
    #[serde(default)]
    pub token: u64,
    #[serde(default)]
    pub sequence: u64,
    #[serde(default)]
    pub synthetic: bool,
    #[serde(default)]
    pub messages: Vec<Vec<u8>>,
    #[serde(default)]
    pub connected: Option<bool>,
}

struct Active {
    token: u64,
    session: String,
    hub: Hub,
    touched: Instant,
    sent: Instant,
    sequence: u64,
    waiting: bool,
}
pub struct Service {
    active: Option<Active>,
    sequence: u64,
    allow_synthetic: bool,
}
impl Service {
    pub fn new(allow_synthetic: bool) -> Self {
        Self {
            active: None,
            sequence: 0,
            allow_synthetic,
        }
    }
    pub fn expire(&mut self) {
        if self
            .active
            .as_ref()
            .is_some_and(|a| a.touched.elapsed() > Duration::from_secs(30))
        {
            self.active = None;
        }
    }
    pub fn wait_duration(&self) -> Duration {
        if self.active.is_some() {
            Duration::from_millis(2)
        } else {
            Duration::from_secs(1)
        }
    }
    /// At most one unacknowledged batch can enter the window event queue.
    pub fn pump(&mut self) -> Option<Value> {
        self.expire();
        let active = self.active.as_mut()?;
        if active.waiting
            || active.sent.elapsed() < Duration::from_millis(2)
            || (!active.hub.has_activity() && active.sent.elapsed() < Duration::from_secs(1))
        {
            return None;
        }
        active.sequence = active.sequence.checked_add(1)?;
        active.waiting = true;
        active.sent = Instant::now();
        Some(match active.hub.poll() {
            Ok(snapshot) => {
                json!({"kind":"native_midi","session":active.session,"token":active.token,"sequence":active.sequence,"snapshot":snapshot})
            }
            Err(error) => {
                json!({"kind":"native_midi","session":active.session,"token":active.token,"sequence":active.sequence,"error":error})
            }
        })
    }
    pub fn execute(&mut self, request: Request) -> Value {
        self.expire();
        let result = self.perform(&request);
        match result {
            Ok(value) => json!({"id":request.id,"session":request.session,"ok":true,"value":value}),
            Err(error) => {
                json!({"id":request.id,"session":request.session,"ok":false,"error":error})
            }
        }
    }
    fn perform(&mut self, request: &Request) -> Result<Value, String> {
        if request.session.is_empty() || request.session.len() > 128 {
            return Err("Invalid MIDI session.".into());
        }
        if request.operation == "midi_open" {
            if request.synthetic && !self.allow_synthetic {
                return Err("Synthetic MIDI requires self-test mode.".into());
            }
            // Release the prior document before opening the same hardware again.
            self.active = None;
            let hub = if request.synthetic {
                Hub::synthetic()
            } else {
                Hub::native()?
            };
            let snapshot = hub.initial_snapshot();
            self.sequence = self
                .sequence
                .checked_add(1)
                .ok_or("MIDI token exhausted.")?;
            self.active = Some(Active {
                token: self.sequence,
                session: request.session.clone(),
                hub,
                touched: Instant::now(),
                sent: Instant::now(),
                sequence: 0,
                waiting: false,
            });
            return Ok(json!({"token":self.sequence,"snapshot":snapshot,"streaming":true}));
        }
        let active = self
            .active
            .as_mut()
            .filter(|a| a.token == request.token && a.session == request.session)
            .ok_or("MIDI session is not active.")?;
        active.touched = Instant::now();
        match request.operation.as_str() {
            "midi_ack" => {
                if !active.waiting || active.sequence != request.sequence {
                    return Err("Invalid MIDI acknowledgement.".into());
                }
                active.waiting = false;
                Ok(json!({}))
            }
            "midi_poll" => Ok(json!(active.hub.poll()?)),
            "midi_close" => {
                self.active = None;
                Ok(json!({}))
            }
            "midi_test" if self.allow_synthetic => {
                if let Some(connected) = request.connected {
                    active.hub.set_connected(connected)?;
                }
                if !request.messages.is_empty() {
                    active.hub.inject(&request.messages)?;
                }
                Ok(json!({}))
            }
            _ => Err("Unknown or unavailable MIDI operation.".into()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn request(operation: &str, token: u64) -> Request {
        Request {
            id: 1,
            session: "owner".into(),
            operation: operation.into(),
            token,
            synthetic: true,
            ..Request::default()
        }
    }
    fn open(service: &mut Service) -> u64 {
        let result = service.execute(request("midi_open", 0));
        assert_eq!(result["ok"], true);
        result["value"]["token"].as_u64().unwrap()
    }
    #[test]
    fn pushed_batches_wait_for_matching_ack_and_idle_heartbeats_keep_the_lease() {
        let mut service = Service::new(true);
        let token = open(&mut service);
        service.active.as_mut().unwrap().sent = Instant::now() - Duration::from_secs(2);
        let first = service.pump().unwrap();
        assert_eq!(first["token"], token);
        assert!(service.pump().is_none());
        let mut ack = request("midi_ack", token);
        ack.sequence = 99;
        assert_eq!(service.execute(ack)["ok"], false);
        assert!(service.pump().is_none());
        let mut ack = request("midi_ack", token);
        ack.sequence = first["sequence"].as_u64().unwrap();
        assert_eq!(service.execute(ack)["ok"], true);
        service.active.as_mut().unwrap().sent = Instant::now() - Duration::from_millis(3);
        assert!(service.pump().is_none());
        service.active.as_mut().unwrap().sent = Instant::now() - Duration::from_secs(2);
        let heartbeat = service.pump().unwrap();
        assert_eq!(heartbeat["sequence"], 2);
        assert_eq!(heartbeat["snapshot"]["events"], json!([]));
        service.active.as_mut().unwrap().touched = Instant::now() - Duration::from_secs(31);
        assert!(service.pump().is_none());
        assert!(service.active.is_none());
    }
    #[test]
    fn synthetic_midi_is_test_only_and_messages_require_the_document_token() {
        assert_eq!(
            Service::new(false).execute(request("midi_open", 0))["ok"],
            false
        );
        let mut service = Service::new(true);
        let token = open(&mut service);
        let mut foreign = request("midi_close", token);
        foreign.session = "other".into();
        assert_eq!(service.execute(foreign)["ok"], false);
        let mut inject = request("midi_test", token);
        inject.messages = vec![vec![0x90, 60, 100], vec![0xC0, 2], vec![0xFA]];
        assert_eq!(service.execute(inject)["ok"], true);
        let polled = service.execute(request("midi_poll", token));
        assert_eq!(polled["value"]["events"].as_array().unwrap().len(), 3);
        assert_eq!(
            service.execute(request("midi_poll", token))["value"]["events"],
            json!([])
        );
        let replacement = open(&mut service);
        assert_ne!(replacement, token);
        assert_eq!(service.execute(request("midi_close", token))["ok"], false);
        assert_eq!(
            service.execute(request("midi_poll", replacement))["ok"],
            true
        );
    }
    #[test]
    fn midi_hotplug_and_lease_cleanup_release_the_active_owner() {
        let mut service = Service::new(true);
        let token = open(&mut service);
        let mut disconnected = request("midi_test", token);
        disconnected.connected = Some(false);
        service.execute(disconnected);
        assert_eq!(
            service.execute(request("midi_poll", token))["value"]["ports"],
            json!([])
        );
        let mut reconnected = request("midi_test", token);
        reconnected.connected = Some(true);
        service.execute(reconnected);
        assert_eq!(
            service.execute(request("midi_poll", token))["value"]["ports"][0]["id"],
            "self-test-midi"
        );
        service.active.as_mut().unwrap().touched = Instant::now() - Duration::from_secs(31);
        service.expire();
        assert!(service.active.is_none());
        assert_eq!(service.execute(request("midi_poll", token))["ok"], false);
        let token = open(&mut service);
        assert_eq!(service.execute(request("midi_close", token))["ok"], true);
        assert_eq!(service.execute(request("midi_poll", token))["ok"], false);
    }
    #[test]
    fn invalid_midi_test_payloads_cannot_inject_partial_batches() {
        let mut service = Service::new(true);
        let token = open(&mut service);
        let mut invalid = request("midi_test", token);
        invalid.messages = vec![vec![0x90, 60, 100], vec![0xF0, 1, 0xF7]];
        assert_eq!(service.execute(invalid)["ok"], false);
        assert_eq!(
            service.execute(request("midi_poll", token))["value"]["events"],
            json!([])
        );
        let mut invalid = request("midi_open", 0);
        invalid.session.clear();
        assert_eq!(service.execute(invalid)["ok"], false);
        assert_eq!(service.execute(request("unknown", token))["ok"], false);
    }
}
