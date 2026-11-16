# Optional Faust interpreter backend

The owner authorized native Faust implementations on 16 November 2026. The
development backend embeds **Faust 2.88.0's portable bytecode interpreter**; LLVM,
JIT, external compiler backends, HTTP/OSC and the Faust executable are disabled.
It is opt-in (`AION_ENABLE_FAUST_INTERPRETER=OFF` by default), separate from
`aion_core`, and currently **not linked/routed into the Standalone application**.
There is no product patch loader, GUI/CLI command or completed Faust effect yet.

`FaustInterpreter::compile` builds a prepared instance off audio. Its immutable
control metadata uses Faust paths. A single audio owner applies finite, bounded
control values and processes 0..4096 frames using prepared scratch. Supported DSP
has 0..2 inputs, 1..2 outputs and at most 64 input controls. Soundfiles are
rejected. Matching in-place buffers work; distinct output channels must not
overlap. Controls are not automatically smoothed: each effect must implement
appropriate smoothing/crossfades before product integration.

Compile, factory/cache operations, allocation, clear and destruction belong off
audio. They must not overlap processing of the same instance. The API is for
**trusted local DSP**, not a sandbox. Source-size/UI/I/O bounds do not establish
bounded compilation time, DSP memory or CPU cost. A future loader needs explicit
resource admission, bounded ownership transfer/retirement, cancellation, validated
state and shared GUI/CLI access. Fault silence is not memory isolation and cannot
recover arbitrary invalid integer arithmetic or unsafe foreign functions. Never
destroy an instance from the audio callback.

Nonfinite inputs become zero. Nonfinite outputs silence the whole block and latch
a fault until an explicit off-audio clear. Invalid buffer/control arguments leave
DSP state untouched. The pinned dependency adaptation removes compute exception
diagnostic allocation/file output, disables environment-selected tracing and
removes an unused signed cycle counter. A further pinned VM adaptation replaces
pointer-punned 32-bit scalar bitcasts with representation copies, value-initializes
the iterator return stacks in both dispatch implementations, and explicitly
rejects unsupported non-single bitcasts. The runtime marker requires this patch.
An upstream exception may still allocate
while being constructed; this is not a claim that arbitrary invalid Faust DSP is
real-time safe. Tests instrument C++ new/delete; Linux additionally wraps direct
malloc/calloc/realloc/free references in linked objects. This does not instrument
every shared-library internal allocation or lock API.

Faust documents the scalar interpreter as generally slower than compiled code.
Measure each actual effect at relevant rates/block sizes before promotion. The
gain/nonlinear benchmark is a backend smoke measurement, not a full-chain performance or
hardware-latency result. If an effect misses its budget, the same DSP source can
be evaluated for ahead-of-time C++ generation; that pipeline is not implemented.

## Reproducible source and build

Official archive:
`https://github.com/grame-cncm/faust/releases/download/2.88.0/faust-2.88.0.tar.gz`

SHA256: `e4e175cf236924b5b7d4784cbb8c50cc01e211159e169655dd9e6d8f92b871d9`.
`Prepare.cmake` checks the archive and extracts only required sources, headers,
build files, library definitions and notices, avoiding unrelated formalisation
symlinks unsupported on Windows. It preserves an existing source directory;
use a fresh private cache to recover from an interrupted extraction. The wrapper
patch refuses unexpected original file hashes rather than overwrite local work.
Keep separate dependency builds/source caches per OS. Never install globally.

Linux, from the native WSL checkout (existing compiler/CMake required):

```bash
cache="$PWD/build/faust-source"
cmake -DAION_FAUST_CACHE="$cache" -P cmake/faust/Prepare.cmake
cmake -S cmake/faust -B build/faust-dependency -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAION_FAUST_SOURCE_DIR="$cache/faust-2.88.0"
cmake --build build/faust-dependency --parallel 2
cmake -S . -B build/faust-tests -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAION_BUILD_PLUGIN=OFF -DAION_ENABLE_FAUST_INTERPRETER=ON \
  -DAION_FAUST_SOURCE_DIR="$cache/faust-2.88.0" \
  -DAION_FAUST_BUILD_DIR="$PWD/build/faust-dependency" \
  -DAION_FAUST_LIBRARY="$PWD/build/faust-dependency/lib/Release/libfaust.a"
cmake --build build/faust-tests --target aion_faust_tests aion_faust_instruction_tests --parallel 2
ctest --test-dir build/faust-tests -R '^faust_(instruction|interpreter)_contract$' --output-on-failure
```

Windows development verification, from the checkout in PowerShell:

```powershell
$cache = "$PWD/build/faust-source"
cmake "-DAION_FAUST_CACHE=$cache" -P cmake/faust/Prepare.cmake
cmake -S cmake/faust -B build/faust-dependency -G 'Visual Studio 17 2022' -A x64 `
  "-DAION_FAUST_SOURCE_DIR=$cache/faust-2.88.0"
cmake --build build/faust-dependency --config Release --parallel 2
cmake -S . -B build/faust-tests -G 'Visual Studio 17 2022' -A x64 `
  -DAION_BUILD_PLUGIN=OFF -DAION_ENABLE_FAUST_INTERPRETER=ON `
  "-DAION_FAUST_SOURCE_DIR=$cache/faust-2.88.0" `
  "-DAION_FAUST_BUILD_DIR=$PWD/build/faust-dependency" `
  "-DAION_FAUST_LIBRARY=$PWD/build/faust-dependency/lib/Release/libfaust.lib"
cmake --build build/faust-tests --config Release --target aion_faust_tests aion_faust_instruction_tests --parallel 2
ctest --test-dir build/faust-tests -C Release -R '^faust_(instruction|interpreter)_contract$' --output-on-failure
```

Use the same toolchain/runtime/configuration for dependency and consumer. With
the repository's private VS installation add
`-DCMAKE_GENERATOR_INSTANCE=D:/Jorge/Documents/Codex/Aion-Looper/.toolchain/vs2022`
to both configure commands and use its bundled cmake/ctest if absent from PATH.
Do not link a Debug consumer to this Release dependency. Build the dependency to
completion before linking the consumer; neither normal builds nor packages fetch
Faust implicitly.

For dependency-wide Linux sanitizer verification, use separate caches:

```bash
cmake -S cmake/faust -B build/faust-dependency-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DAION_FAUST_SANITIZERS=ON \
  -DAION_FAUST_SOURCE_DIR="$PWD/build/faust-source/faust-2.88.0"
cmake --build build/faust-dependency-sanitized --parallel 2
cmake -S . -B build/faust-sanitized-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DAION_BUILD_PLUGIN=OFF -DAION_ENABLE_FAUST_INTERPRETER=ON \
  -DAION_FAUST_SANITIZERS=ON \
  -DAION_FAUST_SOURCE_DIR="$PWD/build/faust-source/faust-2.88.0" \
  -DAION_FAUST_BUILD_DIR="$PWD/build/faust-dependency-sanitized" \
  -DAION_FAUST_LIBRARY="$PWD/build/faust-dependency-sanitized/lib/Debug/libfaust.a"
cmake --build build/faust-sanitized-tests --target aion_faust_tests aion_faust_instruction_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/faust-sanitized-tests \
  -R '^faust_(instruction|interpreter)_contract$' --output-on-failure
```

Both the dependency and host/test targets must be instrumented. This is a focused
runtime/ownership gate, not blanket approval of arbitrary Faust programs. Keep
the normal C++ new/delete and Linux direct-heap-call guards as separate evidence.

## Licensing and delivery limits

The compiler/interpreter is **LGPL-2.1-or-later**, separate from project-owned
AGPL-3.0-only code. See the retained full
[Faust license](../third_party/notices/Faust-LGPL-2.1.txt). Upstream architecture
headers also carry their LGPL exception. Any redistribution of an enabled static
backend needs corresponding dependency source, these local adaptation scripts,
notices and the applicable relinking/source obligations. No Faust-enabled
distributable is produced by this work. Library functions and third-party DSP
patches can carry additional licenses; audit each selected implementation before
shipping it. No third-party effect model is copied into product code here.

Primary references: [embedding/interpreter documentation](https://faustdoc.grame.fr/manual/embedding/),
[pinned release](https://github.com/grame-cncm/faust/releases/tag/2.88.0),
[upstream license](https://github.com/grame-cncm/faust/blob/2.88.0/COPYING.txt),
[antialiased nonlinear library](https://faustlibraries.grame.fr/libs/aanl/).
