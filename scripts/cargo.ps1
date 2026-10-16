# Use the project-local Windows toolchain when present; otherwise use normal Cargo.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$previousCargoHome = $env:CARGO_HOME
$previousRustupHome = $env:RUSTUP_HOME
$previousPath = $env:PATH
$previousAsioDir = $env:CPAL_ASIO_DIR
$previousClang = $env:LIBCLANG_PATH
$previousBindgen = $env:BINDGEN_EXTRA_CLANG_ARGS
$toolExit = 1
Push-Location $projectRoot
try {
    # Keep optional ASIO build dependencies local; do not change user settings.
    $asioSdk = Join-Path $projectRoot '.toolchain/asio-sdk/ASIOSDK'
    $clangLibrary = Join-Path $projectRoot '.toolchain/libclang/clang/native'
    $clangHeaders = Join-Path $projectRoot '.toolchain/clang-headers/clang-18.1.8.src/lib/Headers'
    $gnuHeaders = Join-Path $projectRoot '.toolchain/gnu-tools/mingw64/include'
    if (!$env:CPAL_ASIO_DIR -and (Test-Path -LiteralPath $asioSdk)) { $env:CPAL_ASIO_DIR = $asioSdk }
    if (!$env:LIBCLANG_PATH -and (Test-Path -LiteralPath $clangLibrary)) { $env:LIBCLANG_PATH = $clangLibrary }
    if (!$env:BINDGEN_EXTRA_CLANG_ARGS -and (Test-Path -LiteralPath $clangHeaders) -and (Test-Path -LiteralPath $gnuHeaders)) {
        $env:BINDGEN_EXTRA_CLANG_ARGS = '--target=x86_64-w64-windows-gnu -I"' + $gnuHeaders.Replace('\','/') + '" -I"' + $clangHeaders.Replace('\','/') + '"'
    }
    if (Test-Path -LiteralPath '.toolchain\cargo\bin\cargo.exe') {
        $env:CARGO_HOME = Join-Path $projectRoot '.toolchain\cargo'
        $env:RUSTUP_HOME = Join-Path $projectRoot '.toolchain\rustup'
        $env:PATH = (Join-Path $projectRoot '.toolchain\gnu-tools\mingw64\bin') + ';' + (Join-Path $env:RUSTUP_HOME 'toolchains\stable-x86_64-pc-windows-gnu\lib\rustlib\x86_64-pc-windows-gnu\bin\self-contained') + ';' + $previousPath
        & '.\.toolchain\cargo\bin\cargo.exe' +stable-x86_64-pc-windows-gnu @args
    } else {
        cargo @args
    }
    $toolExit = $LASTEXITCODE
} finally {
    $env:CARGO_HOME = $previousCargoHome
    $env:RUSTUP_HOME = $previousRustupHome
    $env:PATH = $previousPath
    $env:CPAL_ASIO_DIR = $previousAsioDir
    $env:LIBCLANG_PATH = $previousClang
    $env:BINDGEN_EXTRA_CLANG_ARGS = $previousBindgen
    Pop-Location
}
exit $toolExit
