//! Native desktop projects. Float-exact, bounded, versioned, worker-side only.
use looper_core::{checkpoint::Take, MAX_TRACKS};
use std::io::{self, Read, Write};
pub const MAX_FRAMES: usize = 32_000_000;
const MAGIC: &[u8; 8] = b"AIONPR01";
const INITIAL: u64 = 0xcbf29ce484222325;
fn invalid() -> io::Error {
    io::Error::new(
        io::ErrorKind::InvalidData,
        "Invalid or unsupported Aion native project",
    )
}
fn hash(h: &mut u64, data: &[u8]) {
    for b in data {
        *h ^= u64::from(*b);
        *h = h.wrapping_mul(0x100000001b3);
    }
}
pub struct Project {
    pub rate: u32,
    pub capacity: usize,
    pub master: f32,
    pub input_gain: f32,
    pub names: Vec<String>,
    pub takes: Vec<Take>,
}
impl Project {
    fn valid(&self) -> bool {
        (8000..=192000).contains(&self.rate)
            && (1..=MAX_TRACKS).contains(&self.takes.len())
            && self.capacity > 0
            && self.capacity <= self.rate as usize * 120
            && self
                .capacity
                .checked_mul(self.takes.len())
                .is_some_and(|v| v <= MAX_FRAMES)
            && self.names.len() == self.takes.len()
            && self
                .names
                .iter()
                .all(|n| n.len() <= 256 && !n.contains('\0'))
            && [self.master, self.input_gain]
                .iter()
                .all(|v| v.is_finite() && (0.0..=4.0).contains(v))
            && self.takes.iter().all(|t| {
                t.audio.len() <= self.capacity
                    && t.audio.iter().flatten().all(|v| v.is_finite())
                    && t.gain.is_finite()
                    && (0.0..=4.0).contains(&t.gain)
                    && t.feedback.is_finite()
                    && (0.0..=1.0).contains(&t.feedback)
            })
    }
}
struct Output<W> {
    writer: W,
    hash: u64,
}
impl<W: Write> Output<W> {
    fn put(&mut self, data: &[u8]) -> io::Result<()> {
        self.writer.write_all(data)?;
        hash(&mut self.hash, data);
        Ok(())
    }
}
struct Input<R> {
    reader: R,
    hash: u64,
}
impl<R: Read> Input<R> {
    fn get<const N: usize>(&mut self) -> io::Result<[u8; N]> {
        let mut bytes = [0; N];
        self.reader.read_exact(&mut bytes)?;
        hash(&mut self.hash, &bytes);
        Ok(bytes)
    }
    fn u32(&mut self) -> io::Result<u32> {
        Ok(u32::from_le_bytes(self.get()?))
    }
    fn float(&mut self) -> io::Result<f32> {
        Ok(f32::from_bits(self.u32()?))
    }
}
pub fn encode(project: &Project, writer: impl Write) -> io::Result<()> {
    if !project.valid() {
        return Err(invalid());
    }
    let mut out = Output {
        writer,
        hash: INITIAL,
    };
    out.put(MAGIC)?;
    for v in [
        project.rate,
        project.capacity as u32,
        project.takes.len() as u32,
    ] {
        out.put(&v.to_le_bytes())?;
    }
    for v in [project.master, project.input_gain] {
        out.put(&v.to_bits().to_le_bytes())?;
    }
    for (take, name) in project.takes.iter().zip(&project.names) {
        out.put(&(name.len() as u32).to_le_bytes())?;
        out.put(name.as_bytes())?;
        for v in [take.gain, take.feedback] {
            out.put(&v.to_bits().to_le_bytes())?;
        }
        out.put(&[u8::from(take.muted)])?;
        out.put(&(take.audio.len() as u32).to_le_bytes())?;
        for frame in &take.audio {
            for sample in frame {
                out.put(&sample.to_bits().to_le_bytes())?;
            }
        }
    }
    out.writer.write_all(&out.hash.to_le_bytes())
}
pub fn decode(reader: impl Read) -> io::Result<Project> {
    let mut input = Input {
        reader,
        hash: INITIAL,
    };
    if &input.get::<8>()? != MAGIC {
        return Err(invalid());
    }
    let rate = input.u32()?;
    let capacity = input.u32()? as usize;
    let count = input.u32()? as usize;
    if !(8000..=192000).contains(&rate)
        || !(1..=MAX_TRACKS).contains(&count)
        || capacity == 0
        || capacity > rate as usize * 120
        || capacity.checked_mul(count).is_none_or(|v| v > MAX_FRAMES)
    {
        return Err(invalid());
    }
    let master = input.float()?;
    let input_gain = input.float()?;
    let mut takes = Vec::with_capacity(count);
    let mut names = Vec::with_capacity(count);
    for _ in 0..count {
        let length = input.u32()? as usize;
        if length > 256 {
            return Err(invalid());
        }
        let mut name = Vec::with_capacity(length);
        for _ in 0..length {
            name.push(input.get::<1>()?[0]);
        }
        names.push(String::from_utf8(name).map_err(|_| invalid())?);
        let gain = input.float()?;
        let feedback = input.float()?;
        let muted = match input.get::<1>()?[0] {
            0 => false,
            1 => true,
            _ => return Err(invalid()),
        };
        let length = input.u32()? as usize;
        if length > capacity {
            return Err(invalid());
        }
        let mut audio = Vec::new();
        audio.try_reserve_exact(length).map_err(|_| invalid())?;
        for _ in 0..length {
            audio.push([input.float()?, input.float()?]);
        }
        takes.push(Take {
            audio,
            playing: false,
            gain,
            feedback,
            muted,
        });
    }
    let checksum = input.hash;
    let mut stored = [0; 8];
    input.reader.read_exact(&mut stored)?;
    let mut trailing = [0];
    if u64::from_le_bytes(stored) != checksum || input.reader.read(&mut trailing)? != 0 {
        return Err(invalid());
    }
    let project = Project {
        rate,
        capacity,
        master,
        input_gain,
        names,
        takes,
    };
    if !project.valid() {
        return Err(invalid());
    }
    Ok(project)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn native_project_roundtrip_is_float_exact_and_rejects_corruption_and_unbounded_headers() {
        let p = Project {
            rate: 48000,
            capacity: 48000,
            master: 0.5,
            input_gain: 1.25,
            names: vec!["Voz 🎵".into()],
            takes: vec![Take {
                audio: vec![[-0.0, 1.25], [0.125, -2.0]],
                playing: true,
                gain: 0.4,
                feedback: 0.7,
                muted: true,
            }],
        };
        let mut bytes = Vec::new();
        encode(&p, &mut bytes).unwrap();
        let loaded = decode(bytes.as_slice()).unwrap();
        assert_eq!(loaded.takes[0].audio[0][0].to_bits(), (-0.0f32).to_bits());
        assert_eq!(loaded.takes[0].audio, p.takes[0].audio);
        assert!(!loaded.takes[0].playing);
        assert_eq!(loaded.names, p.names);
        assert_eq!(loaded.master, p.master);
        assert_eq!(loaded.takes[0].feedback, 0.7);
        assert!(loaded.takes[0].muted);
        for end in 0..bytes.len() {
            assert!(decode(&bytes[..end]).is_err());
        }
        let mut broken = bytes.clone();
        broken[35] ^= 1;
        assert!(decode(broken.as_slice()).is_err());
        let mut huge = bytes.clone();
        huge[12..16].copy_from_slice(&u32::MAX.to_le_bytes());
        assert!(decode(huge.as_slice()).is_err());
        let mut trailing = bytes;
        trailing.push(1);
        assert!(decode(trailing.as_slice()).is_err());
    }
}
