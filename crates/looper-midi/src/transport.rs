//! Fixed-size MIDI 1.0 short messages. SysEx is deliberately not exposed.
use rtrb::{Consumer, Producer, RingBuffer};
use std::sync::{
    atomic::{AtomicU64, Ordering},
    Arc,
};
use std::time::Instant;
pub const CAPACITY: usize = 1024;
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Message {
    pub data: [u8; 3],
    pub length: u8,
    pub timestamp_us: u64,
    pub received_us: u64,
}
pub struct Writer {
    queue: Producer<Message>,
    dropped: Arc<AtomicU64>,
    epoch: Instant,
}
pub struct Reader {
    queue: Consumer<Message>,
    dropped: Arc<AtomicU64>,
}
pub fn channel(epoch: Instant) -> (Writer, Reader) {
    let (producer, consumer) = RingBuffer::new(CAPACITY);
    let dropped = Arc::new(AtomicU64::new(0));
    (
        Writer {
            queue: producer,
            dropped: Arc::clone(&dropped),
            epoch,
        },
        Reader {
            queue: consumer,
            dropped,
        },
    )
}
/// Running status must already have been resolved by the native backend.
pub fn valid(data: &[u8]) -> bool {
    let Some(&status) = data.first() else {
        return false;
    };
    let length = match status {
        0x80..=0xBF | 0xE0..=0xEF | 0xF2 => 3,
        0xC0..=0xDF | 0xF1 | 0xF3 => 2,
        0xF6 | 0xF8 | 0xFA..=0xFC | 0xFE..=0xFF => 1,
        _ => return false,
    };
    data.len() == length && data[1..].iter().all(|byte| *byte < 128)
}
impl Writer {
    /// Callback path: bounded copy, monotonic timestamp and atomics only.
    pub fn capture(&mut self, timestamp_us: u64, data: &[u8]) {
        if !valid(data) {
            return;
        }
        let mut bytes = [0; 3];
        bytes[..data.len()].copy_from_slice(data);
        let message = Message {
            data: bytes,
            length: data.len() as u8,
            timestamp_us,
            received_us: self.epoch.elapsed().as_micros().min(u128::from(u64::MAX)) as u64,
        };
        if self.queue.push(message).is_err() {
            self.dropped.fetch_add(1, Ordering::Relaxed);
        }
    }
}
impl Reader {
    pub fn has_messages(&self) -> bool {
        !self.queue.is_empty()
    }
    pub fn pop(&mut self) -> Option<Message> {
        self.queue.pop().ok()
    }
    pub fn dropped(&self) -> u64 {
        self.dropped.load(Ordering::Relaxed)
    }
    pub fn discard(&mut self) {
        for _ in 0..CAPACITY {
            if self.pop().is_none() {
                break;
            }
        }
    }
}
