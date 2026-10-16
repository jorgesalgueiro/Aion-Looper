$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & (Join-Path $PSScriptRoot 'cargo.ps1') test --workspace
    if ($LASTEXITCODE -ne 0) { throw 'Rust core verification failed.' }
    node --test 'crates/looper-desktop/tests/*.test.cjs'
    if ($LASTEXITCODE -ne 0) { throw 'Desktop bridge verification failed.' }
    Push-Location ui
    try {
        node --test 'tests/*.test.cjs'
        if ($LASTEXITCODE -ne 0) { throw 'Original behavior verification failed.' }
    } finally { Pop-Location }
} finally {
    Pop-Location
}
