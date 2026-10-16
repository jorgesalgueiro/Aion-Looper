//! Preallocated SPSC input transport. The device callback never waits for a reader.
use cpal::{FromSample, Sample};
use looper_core::Frame;
use rtrb::{Consumer, Producer, RingBuffer};
use std::sync::atomic::{AtomicU64, AtomicUsize, Ordering};
use std::sync::Arc;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CapturedFrame {
    /// Input-device frame clock, including frames dropped when the ring was full.
    pub position: u64,
    pub samples: Frame,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Status {
    pub frames: u64,
    pub dropped_frames: u64,
    pub nonfinite_samples: u64,
    pub malformed_blocks: u64,
    pub stream_errors: u64,
    pub max_callback_frames: usize,
}

#[derive(Default)]
pub struct Metrics {
    frames: AtomicU64,
    dropped_frames: AtomicU64,
    nonfinite_samples: AtomicU64,
    malformed_blocks: AtomicU64,
    stream_errors: AtomicU64,
    max_callback_frames: AtomicUsize,
}
impl Metrics {
    pub fn status(&self) -> Status {
        Status {
            frames: self.frames.load(Ordering::Relaxed),
            dropped_frames: self.dropped_frames.load(Ordering::Relaxed),
            nonfinite_samples: self.nonfinite_samples.load(Ordering::Relaxed),
            malformed_blocks: self.malformed_blocks.load(Ordering::Relaxed),
            stream_errors: self.stream_errors.load(Ordering::Relaxed),
            max_callback_frames: self.max_callback_frames.load(Ordering::Relaxed),
        }
    }
    pub fn stream_error(&self) {
        self.stream_errors.fetch_add(1, Ordering::Relaxed);
    }
}

pub struct InputWriter {
    producer: Producer<CapturedFrame>,
    metrics: Arc<Metrics>,
    position: u64,
}
pub struct InputReader {
    consumer: Consumer<CapturedFrame>,
    metrics: Arc<Metrics>,
}

/// Allocate the entire ring before device startup. The caller sets its memory bound.
pub fn connect(capacity_frames: usize) -> Result<(InputWriter, InputReader), &'static str> {
    if capacity_frames == 0 || capacity_frames > 1_000_000 {
        return Err("Input ring capacity must be between 1 and 1,000,000 frames.");
    }
    let (producer, consumer) = RingBuffer::new(capacity_frames);
    let metrics = Arc::new(Metrics::default());
    Ok((
        InputWriter {
            producer,
            metrics: Arc::clone(&metrics),
            position: 0,
        },
        InputReader { consumer, metrics },
    ))
}

impl InputWriter {
    pub fn metrics(&self) -> Arc<Metrics> {
        Arc::clone(&self.metrics)
    }

    /// Preserve finite float headroom. Mono duplicates to stereo; wider interfaces
    /// capture their first input pair. Invalid samples
    /// become silence and are counted. Overflow drops new frames, never old audio.
    pub fn capture<T: Sample>(&mut self, data: &[T], channels: usize)
    where
        f32: FromSample<T>,
    {
        if !(1..=128).contains(&channels) || !data.len().is_multiple_of(channels) {
            self.metrics
                .malformed_blocks
                .fetch_add(1, Ordering::Relaxed);
            return;
        }
        let frames = data.len() / channels;
        let mut dropped = 0;
        let mut nonfinite = 0;
        for source in data.chunks_exact(channels) {
            let mut convert = |value| {
                let value = f32::from_sample(value);
                if value.is_finite() {
                    value
                } else {
                    nonfinite += 1;
                    0.0
                }
            };
            let left = convert(source[0]);
            let right = if channels == 1 {
                left
            } else {
                convert(source[1])
            };
            if self
                .producer
                .push(CapturedFrame {
                    position: self.position,
                    samples: [left, right],
                })
                .is_err()
            {
                dropped += 1;
            }
            self.position += 1;
        }
        self.metrics
            .frames
            .fetch_add(frames as u64, Ordering::Relaxed);
        self.metrics
            .dropped_frames
            .fetch_add(dropped, Ordering::Relaxed);
        self.metrics
            .nonfinite_samples
            .fetch_add(nonfinite, Ordering::Relaxed);
        self.metrics
            .max_callback_frames
            .fetch_max(frames, Ordering::Relaxed);
    }
}
impl InputReader {
    pub fn pop(&mut self) -> Option<CapturedFrame> {
        self.consumer.pop().ok()
    }
    pub fn status(&self) -> Status {
        self.metrics.status()
    }
}
