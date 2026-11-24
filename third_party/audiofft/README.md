# Pinned AudioFFT subset

Unmodified `AudioFFT.h`, `AudioFFT.cpp`, `COPYING.txt` from
[HiFi-LoFi/FFTConvolver](https://github.com/HiFi-LoFi/FFTConvolver/tree/f2cdeb04c42141d2caec19ca4f137398b2a76b85),
commit `f2cdeb04c42141d2caec19ca4f137398b2a76b85` (`non-uniform` branch).
These files are vendored so normal/offline core builds do not fetch a new SDK.

SHA-256 of the downloaded bytes:

| File | SHA-256 |
| --- | --- |
| AudioFFT.cpp | B00FCB12A200241113579655EC3B5D0E9A8D3693D28E3A607C52A2AA3DB4087C |
| AudioFFT.h | 9A4551DFBD2887B3EA5872C883C83D521319CCFD6A82794FBF8E4A8073D9CC65 |
| COPYING.txt | 1537D3165D802602DE45A40CE5CB335D33CD801DA8F25060D010E691C548AE65 |

Keep these source files unchanged and retain their MIT text. The selected Ooura
path has its own attribution/permission in `../notices/AudioFFT-Ooura.txt`.
No FFTConvolver engine, FFTW, Accelerate, plugin, UI or preset bank is included.
No claim of active upstream maintenance or fastest FFT is made.

Source audit for this build: constructor/init allocate the implementation,
integer work array, double twiddle table and double transform buffer. `init(256)`
fully builds tables before audio. `fft`/`ifft` use those arrays and radix-4
arithmetic only: no resizing, allocation, lock, thread, logging or I/O. Destroy
the object off audio. Optional FFTW/Accelerate code is not the audited path.
