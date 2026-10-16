# Use the project-local Windows toolchain when present; otherwise use normal Cargo.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$previousCargoHome = $env:CARGO_HOME
$previousRustupHome = $env:RUSTUP_HOME
$previousPath = $env:PATH
$toolExit = 1
Push-Location $projectRoot
try {
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
    Pop-Location
}
exit $toolExit
