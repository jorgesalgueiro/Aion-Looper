"""Independent standard-library inspection of the native float WAV example."""
import array
import math
from pathlib import Path
import struct
import sys

raw = Path(sys.argv[1]).read_bytes()
assert raw[:4] == b"RIFF" and raw[8:12] == b"WAVE"
assert struct.unpack_from("<I", raw, 4)[0] + 8 == len(raw)
offset = 12
chunks = {}
while offset < len(raw):
    name, length = struct.unpack_from("<4sI", raw, offset)
    start = offset + 8
    chunks[name] = raw[start:start + length]
    assert len(chunks[name]) == length
    offset = start + length + (length % 2)
tag, channels, rate, byte_rate, align, bits = struct.unpack_from("<HHIIHH", chunks[b"fmt "])
assert (tag, channels, rate, byte_rate, align, bits) == (3, 2, 48000, 384000, 8, 32)
samples = array.array("f", chunks[b"data"])
if sys.byteorder != "little":
    samples.byteswap()
assert len(samples) == 5 * rate * channels
assert struct.unpack("<I", chunks[b"fact"])[0] == 5 * rate
assert all(math.isfinite(sample) for sample in samples)
assert max(abs(sample) for sample in samples[:2 * rate * channels]) > 0.1
assert all(sample == 0.0 for sample in samples[2 * rate * channels:3 * rate * channels])
assert samples[:rate * channels] == samples[rate * channels:2 * rate * channels]
assert samples[:rate * channels] == samples[4 * rate * channels:5 * rate * channels]
assert any(samples[i] != samples[i + 1] for i in range(0, len(samples), 2))
print("Independent WAV inspection passed: 5 s, 48 kHz stereo float32, exact mute interval, repeated takes and finite audio.")
