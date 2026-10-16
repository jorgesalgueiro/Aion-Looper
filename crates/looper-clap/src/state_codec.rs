//! Versioned, bounded, float-exact prototype session stream. Control thread only.
use crate::{params, PARAM_COUNT};
use looper_core::{checkpoint::Take, MAX_TRACKS};
use std::io::{self, Read, Write};
pub const MAX_FRAMES: usize = MAX_TRACKS * 8 * 192_000;
const MAGIC: &[u8; 8] = b"RLCLAP01";
const INITIAL: u64 = 0xcbf29ce484222325;
fn hash(mut state: u64, data: &[u8]) -> u64 {
    for b in data {
        state ^= u64::from(*b);
        state = state.wrapping_mul(0x100000001b3);
    }
    state
}
fn invalid() -> io::Error {
    io::Error::new(
        io::ErrorKind::InvalidData,
        "Invalid Rust-Looper plugin state",
    )
}
pub struct Session {
    pub rate: f64,
    pub parameters: [f64; PARAM_COUNT],
    pub takes: Vec<Take>,
}
impl Session {
    pub fn empty(parameters: [f64; PARAM_COUNT]) -> Self {
        Self {
            rate: 0.0,
            parameters,
            takes: (0..MAX_TRACKS)
                .map(|_| Take {
                    audio: Vec::new(),
                    playing: false,
                    muted: false,
                    gain: 1.0,
                    feedback: 0.8,
                })
                .collect(),
        }
    }
    fn valid(&self) -> bool {
        (self.rate == 0.0 || (self.rate.is_finite() && (8000.0..=192000.0).contains(&self.rate)))
            && self
                .parameters
                .iter()
                .enumerate()
                .all(|(i, v)| params::valid(i, *v))
            && self.takes.len() == MAX_TRACKS
            && self.takes.iter().all(|t| {
                t.audio.len() <= (self.rate * 8.0) as usize
                    && (t.audio.is_empty() && !t.playing || !t.audio.is_empty())
                    && t.gain.is_finite()
                    && (0.0..=4.0).contains(&t.gain)
                    && t.feedback.is_finite()
                    && (0.0..=1.0).contains(&t.feedback)
                    && t.audio.iter().flatten().all(|v| v.is_finite())
            })
    }
}
pub fn encode(session: &Session, mut writer: impl Write) -> io::Result<()> {
    if !session.valid() {
        return Err(invalid());
    }
    let mut checksum = INITIAL;
    let mut put = |data: &[u8]| -> io::Result<()> {
        writer.write_all(data)?;
        checksum = hash(checksum, data);
        Ok(())
    };
    put(MAGIC)?;
    put(&1u32.to_le_bytes())?;
    put(&session.rate.to_le_bytes())?;
    put(&(PARAM_COUNT as u32).to_le_bytes())?;
    for value in session.parameters {
        put(&value.to_le_bytes())?;
    }
    put(&(MAX_TRACKS as u32).to_le_bytes())?;
    let mut bytes = [0u8; 8192];
    for take in &session.takes {
        put(&(take.audio.len() as u32).to_le_bytes())?;
        put(&(u32::from(take.playing) | u32::from(take.muted) << 1).to_le_bytes())?;
        put(&take.gain.to_le_bytes())?;
        put(&take.feedback.to_le_bytes())?;
        for frames in take.audio.chunks(1024) {
            for (frame, out) in frames.iter().zip(bytes.as_chunks_mut::<8>().0.iter_mut()) {
                out[..4].copy_from_slice(&frame[0].to_le_bytes());
                out[4..].copy_from_slice(&frame[1].to_le_bytes());
            }
            put(&bytes[..frames.len() * 8])?;
        }
    }
    writer.write_all(&checksum.to_le_bytes())
}
struct Decoder<R> {
    reader: R,
    checksum: u64,
}
impl<R: Read> Decoder<R> {
    fn bytes<const N: usize>(&mut self) -> io::Result<[u8; N]> {
        let mut b = [0; N];
        self.reader.read_exact(&mut b)?;
        self.checksum = hash(self.checksum, &b);
        Ok(b)
    }
    fn u32(&mut self) -> io::Result<u32> {
        Ok(u32::from_le_bytes(self.bytes()?))
    }
    fn f32(&mut self) -> io::Result<f32> {
        Ok(f32::from_le_bytes(self.bytes()?))
    }
}
pub fn decode(reader: impl Read) -> io::Result<Session> {
    let mut d = Decoder {
        reader,
        checksum: INITIAL,
    };
    if &d.bytes::<8>()? != MAGIC || d.u32()? != 1 {
        return Err(invalid());
    }
    let rate = f64::from_le_bytes(d.bytes()?);
    if (rate != 0.0 && (!rate.is_finite() || !(8000.0..=192000.0).contains(&rate)))
        || d.u32()? != PARAM_COUNT as u32
    {
        return Err(invalid());
    }
    let mut parameters = [0.0; PARAM_COUNT];
    for (index, v) in parameters.iter_mut().enumerate() {
        *v = f64::from_le_bytes(d.bytes()?);
        if !params::valid(index, *v) {
            return Err(invalid());
        }
    }
    if d.u32()? != MAX_TRACKS as u32 {
        return Err(invalid());
    }
    let mut takes = Vec::with_capacity(MAX_TRACKS);
    let mut total = 0usize;
    for _ in 0..MAX_TRACKS {
        let length = d.u32()? as usize;
        total = total.checked_add(length).ok_or_else(invalid)?;
        if length > (rate * 8.0) as usize || total > MAX_FRAMES {
            return Err(invalid());
        }
        let flags = d.u32()?;
        if flags & !3 != 0 || length == 0 && flags & 1 != 0 {
            return Err(invalid());
        }
        let gain = d.f32()?;
        let feedback = d.f32()?;
        if !gain.is_finite()
            || !(0.0..=4.0).contains(&gain)
            || !feedback.is_finite()
            || !(0.0..=1.0).contains(&feedback)
        {
            return Err(invalid());
        }
        let mut audio = Vec::new();
        audio.try_reserve_exact(length).map_err(|_| invalid())?;
        for _ in 0..length {
            let frame = [d.f32()?, d.f32()?];
            if frame.iter().any(|v| !v.is_finite()) {
                return Err(invalid());
            }
            audio.push(frame);
        }
        takes.push(Take {
            audio,
            playing: flags & 1 != 0,
            muted: flags & 2 != 0,
            gain,
            feedback,
        });
    }
    let mut checksum = [0; 8];
    d.reader.read_exact(&mut checksum)?;
    if u64::from_le_bytes(checksum) != d.checksum {
        return Err(invalid());
    }
    let mut extra = [0];
    if d.reader.read(&mut extra)? != 0 {
        return Err(invalid());
    }
    Ok(Session {
        rate,
        parameters,
        takes,
    })
}
