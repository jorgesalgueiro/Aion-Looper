# Native stretch dependency

Rubber Band Library 4.0.0, unmodified source from the official upstream tag:
https://github.com/breakfastquay/rubberband/archive/refs/tags/v4.0.0.tar.gz

Archive SHA-256:
`24300f48a8014b7c863b573a9647e61b1b19b37875e2cdd92005e64c6424d266`

Copyright 2007–2024 Particular Programs Ltd. GPL-2.0-or-later; COPYING and
the original notices are retained under vendor/rubberband-4.0.0.
The application continues to use GPL-2.0-only.

Build uses upstream's `single/RubberBandSingle.cpp`, built-in FFT/resampler,
and Accelerate on Apple targets. No external FFT or resampling dependency.
Our native shim and Rust wrapper run offline on the control worker. C++ owns
and destroys its engine/vectors and catches exceptions before returning to Rust.
Neither side retains borrowed PCM pointers. Channels are processed together;
the offline engine handles startup delay and flushes the final block.

Windows GNU packages include the compiler's libstdc++, libgcc and winpthreads
runtime DLLs with their notices. `scripts/setup-native-cxx.ps1` installs verified
official MSYS2 build dependencies into the ignored project-local toolchain.

Integration reference: https://breakfastquay.com/rubberband/integration.html
