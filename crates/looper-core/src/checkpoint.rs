//! Optional atomic PCM storage for coherent control-thread plugin snapshots.
//! Local/standalone storage remains ordinary float32. No duplicate PCM mirror.
use crate::{Engine, Error, Frame, State, MAX_TRACKS};
use std::sync::{
    atomic::{AtomicBool, AtomicU32, AtomicU64, AtomicUsize, Ordering::SeqCst},
    Arc,
};

pub(crate) struct AtomicPcm(Box<[[AtomicU32; 2]]>);
impl AtomicPcm {
    fn new(capacity: usize) -> Result<Self, Error> {
        let mut data = Vec::new();
        data.try_reserve_exact(capacity)
            .map_err(|_| Error::InvalidCapacity)?;
        data.resize_with(capacity, || [AtomicU32::new(0), AtomicU32::new(0)]);
        Ok(Self(data.into_boxed_slice()))
    }
    fn read(&self, index: usize) -> Frame {
        self.0[index]
            .each_ref()
            .map(|v| f32::from_bits(v.load(SeqCst)))
    }
    fn write(&self, index: usize, frame: Frame) {
        for (to, from) in self.0[index].iter().zip(frame) {
            to.store(from.to_bits(), SeqCst);
        }
    }
}
pub(crate) enum Storage {
    Local(Box<[Frame]>),
    Shared(Arc<AtomicPcm>),
}
impl Storage {
    pub fn len(&self) -> usize {
        match self {
            Self::Local(a) => a.len(),
            Self::Shared(a) => a.0.len(),
        }
    }
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
    pub fn read(&self, index: usize) -> Frame {
        match self {
            Self::Local(a) => a[index],
            Self::Shared(a) => a.read(index),
        }
    }
    pub fn write(&mut self, index: usize, frame: Frame) {
        match self {
            Self::Local(a) => a[index] = frame,
            Self::Shared(a) => a.write(index, frame),
        }
    }
}
struct Metadata {
    length: AtomicUsize,
    state: AtomicU32,
    gain: AtomicU32,
    feedback: AtomicU32,
    muted: AtomicU32,
}
pub struct Checkpoint {
    pcm: [Arc<AtomicPcm>; MAX_TRACKS],
    metadata: [Metadata; MAX_TRACKS],
    epoch: AtomicU64,
    poisoned: AtomicBool,
}
#[derive(Debug, PartialEq)]
pub struct Take {
    pub audio: Vec<Frame>,
    pub playing: bool,
    pub gain: f32,
    pub feedback: f32,
    pub muted: bool,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SnapshotError {
    Busy,
    Capacity,
}
/// The audio owner holds this across commands, rendering and parameter commits.
/// Dropping it publishes the transaction boundary even when unwinding.
pub struct Transaction<'a>(&'a Checkpoint);
impl Drop for Transaction<'_> {
    fn drop(&mut self) {
        if std::thread::panicking() {
            self.0.poisoned.store(true, SeqCst);
        }
        self.0.epoch.fetch_add(1, SeqCst);
    }
}
/// Worker-prepared PCM and checkpoint references. Applying swaps ownership only;
/// the caller must retire this object on the worker, never drop it in a callback.
pub struct Replacement {
    track: usize,
    audio: Storage,
    length: usize,
    parent: Arc<Checkpoint>,
    next: Arc<Checkpoint>,
    applied: bool,
    reset_levels: bool,
}
impl Replacement {
    pub fn checkpoint(&self) -> Arc<Checkpoint> {
        Arc::clone(&self.next)
    }
    pub fn apply(&mut self, engine: &mut Engine) -> Result<(), Error> {
        if self.applied
            || !engine
                .tracks
                .iter()
                .zip(&self.parent.pcm)
                .all(|(t, p)| matches!(&t.audio,Storage::Shared(a) if Arc::ptr_eq(a,p)))
        {
            return Err(Error::InvalidState);
        }
        let track = &mut engine.tracks[self.track];
        if !matches!(track.state, State::Empty | State::Stopped) {
            return Err(Error::InvalidState);
        }
        // Retire the previous view before shared PCM can change under a new epoch.
        self.parent.poisoned.store(true, SeqCst);
        self.parent.epoch.fetch_add(2, SeqCst);
        std::mem::swap(&mut track.audio, &mut self.audio);
        track.length = self.length;
        if self.reset_levels {
            track.gain = 1.0;
            track.feedback = 0.8;
        }
        track.position = 0;
        track.state = State::Stopped;
        engine.schedule.cancel(self.track);
        self.applied = true;
        Ok(())
    }
}
impl Checkpoint {
    /// Control-thread allocation; atomic PCM occupies the same eight bytes/frame.
    pub fn prepare(capacities: [usize; MAX_TRACKS]) -> Result<(Engine, Arc<Self>), Error> {
        // Validate aggregate capacity through the existing constructor, one track
        // at a time below; avoid first allocating a duplicate full PCM engine.
        let frames = capacities
            .iter()
            .try_fold(0usize, |n, v| n.checked_add(*v))
            .ok_or(Error::InvalidCapacity)?;
        if frames == 0
            || frames
                .checked_mul(8)
                .is_none_or(|v| v > isize::MAX as usize)
        {
            return Err(Error::InvalidCapacity);
        }
        let mut pcm = Vec::with_capacity(MAX_TRACKS);
        let mut tracks = Vec::with_capacity(MAX_TRACKS);
        for capacity in capacities {
            let data = Arc::new(AtomicPcm::new(capacity)?);
            let mut track = super::Track::new(0)?;
            track.audio = Storage::Shared(Arc::clone(&data));
            tracks.push(track);
            pcm.push(data);
        }
        let checkpoint = Arc::new(Self {
            pcm: pcm.try_into().map_err(|_| Error::InvalidCapacity)?,
            metadata: std::array::from_fn(|_| Metadata {
                length: AtomicUsize::new(0),
                state: AtomicU32::new(0),
                gain: AtomicU32::new(1f32.to_bits()),
                feedback: AtomicU32::new(0.8f32.to_bits()),
                muted: AtomicU32::new(0),
            }),
            epoch: AtomicU64::new(0),
            poisoned: AtomicBool::new(false),
        });
        Ok((
            Engine {
                tracks: tracks.try_into().map_err(|_| Error::InvalidCapacity)?,
                clock: 0,
                schedule: super::Schedule::new(),
            },
            checkpoint,
        ))
    }
    /// Allocate/decode on the worker before committing to an empty/stopped track.
    pub fn prepare_replacement(
        self: &Arc<Self>,
        track: usize,
        audio: &[Frame],
    ) -> Result<Replacement, Error> {
        let old = self.pcm.get(track).ok_or(Error::InvalidTrack)?;
        if audio.is_empty() || audio.len() > old.0.len() {
            return Err(Error::InvalidCapacity);
        }
        if audio.iter().flatten().any(|v| !v.is_finite()) {
            return Err(Error::InvalidParameter);
        }
        let pcm = Arc::new(AtomicPcm::new(old.0.len())?);
        for (i, frame) in audio.iter().enumerate() {
            pcm.write(i, *frame);
        }
        let next = Arc::new(Self {
            pcm: std::array::from_fn(|i| {
                if i == track {
                    Arc::clone(&pcm)
                } else {
                    Arc::clone(&self.pcm[i])
                }
            }),
            metadata: std::array::from_fn(|_| Metadata {
                length: AtomicUsize::new(0),
                state: AtomicU32::new(0),
                gain: AtomicU32::new(1f32.to_bits()),
                feedback: AtomicU32::new(0.8f32.to_bits()),
                muted: AtomicU32::new(0),
            }),
            epoch: AtomicU64::new(0),
            poisoned: AtomicBool::new(false),
        });
        Ok(Replacement {
            track,
            audio: Storage::Shared(pcm),
            length: audio.len(),
            parent: Arc::clone(self),
            next,
            applied: false,
            reset_levels: true,
        })
    }
    /// Worker-prepared edit of a stopped take; retain its current mix settings.
    pub fn prepare_edit(
        self: &Arc<Self>,
        track: usize,
        audio: &[Frame],
    ) -> Result<Replacement, Error> {
        let mut replacement = self.prepare_replacement(track, audio)?;
        replacement.reset_levels = false;
        Ok(replacement)
    }
    /// Single audio/control owner only; transactions must not nest.
    pub fn begin(&self) -> Transaction<'_> {
        self.epoch.fetch_add(1, SeqCst);
        Transaction(self)
    }
    /// Publish metadata inside the audio owner's transaction.
    pub fn publish(&self, engine: &Engine) {
        for (meta, track) in self.metadata.iter().zip(&engine.tracks) {
            meta.length.store(track.length, SeqCst);
            meta.state.store(
                match track.state {
                    State::Empty => 0,
                    State::Stopped => 1,
                    State::Playing => 2,
                    _ => 3,
                },
                SeqCst,
            );
            meta.gain.store(track.gain.to_bits(), SeqCst);
            meta.feedback.store(track.feedback.to_bits(), SeqCst);
            meta.muted.store(u32::from(track.muted), SeqCst);
        }
    }
    /// Allocates on the saving thread. Active capture/punch or concurrent edits
    /// reject the entire snapshot. Playback phase is deliberately not persisted.
    pub fn snapshot<T>(
        &self,
        max_frames: usize,
        extra: impl FnOnce() -> T,
    ) -> Result<(Vec<Take>, T), SnapshotError> {
        let epoch = self.epoch.load(SeqCst);
        if !epoch.is_multiple_of(2) || self.poisoned.load(SeqCst) {
            return Err(SnapshotError::Busy);
        }
        if self.metadata.iter().any(|m| m.state.load(SeqCst) == 3) {
            return Err(SnapshotError::Busy);
        }
        let mut takes = Vec::with_capacity(MAX_TRACKS);
        let mut total = 0usize;
        for (meta, pcm) in self.metadata.iter().zip(&self.pcm) {
            let length = meta.length.load(SeqCst);
            total = total.checked_add(length).ok_or(SnapshotError::Capacity)?;
            if length > pcm.0.len() || total > max_frames {
                return Err(SnapshotError::Capacity);
            }
            let mut audio = Vec::new();
            audio
                .try_reserve_exact(length)
                .map_err(|_| SnapshotError::Capacity)?;
            for index in 0..length {
                audio.push(pcm.read(index));
            }
            takes.push(Take {
                audio,
                playing: meta.state.load(SeqCst) == 2,
                gain: f32::from_bits(meta.gain.load(SeqCst)),
                feedback: f32::from_bits(meta.feedback.load(SeqCst)),
                muted: meta.muted.load(SeqCst) != 0,
            });
        }
        let extra = extra();
        if self.epoch.load(SeqCst) != epoch {
            return Err(SnapshotError::Busy);
        }
        Ok((takes, extra))
    }
}
