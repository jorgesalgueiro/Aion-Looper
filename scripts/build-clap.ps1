$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & (Join-Path $PSScriptRoot 'cargo.ps1') build --release -p looper-clap
    if ($LASTEXITCODE -ne 0) { throw 'CLAP build failed.' }
    $packagePath = Join-Path $projectRoot 'dist\Aion-Looper-CLAP-Windows'
    New-Item -ItemType Directory -Force -Path $packagePath | Out-Null
    $pluginPath = Join-Path $packagePath 'Aion-Looper-Prototype.clap'
    $cargoTargetRoot = if ($env:CARGO_TARGET_DIR) {
        if ([IO.Path]::IsPathRooted($env:CARGO_TARGET_DIR)) { $env:CARGO_TARGET_DIR }
        else { Join-Path $projectRoot $env:CARGO_TARGET_DIR }
    } else { Join-Path $projectRoot 'target' }
    Copy-Item -LiteralPath (Join-Path $cargoTargetRoot 'release\looper_clap.dll') -Destination $pluginPath
    Copy-Item -LiteralPath 'LICENSE' -Destination $packagePath
    Copy-Item -LiteralPath 'docs\CLAP.md' -Destination (Join-Path $packagePath 'README.md')
    Copy-Item -LiteralPath 'docs\PLUGIN_STATE.md' -Destination $packagePath
    $cargoRoot = if (Test-Path -LiteralPath '.toolchain\cargo') {
        Join-Path $projectRoot '.toolchain\cargo'
    } elseif ($env:CARGO_HOME) { $env:CARGO_HOME } else { Join-Path $env:USERPROFILE '.cargo' }
    $bindingLicense = Get-ChildItem -Path (Join-Path $cargoRoot 'registry\src\*\clap-sys-0.5.0\LICENSE-MIT') | Select-Object -First 1
    if (!$bindingLicense) { throw 'Pinned CLAP binding license not found.' }
    Copy-Item -LiteralPath $bindingLicense.FullName -Destination (Join-Path $packagePath 'clap-sys-LICENSE-MIT.txt')
    & (Join-Path $PSScriptRoot 'cargo.ps1') run --offline -p looper-clap --example host -- $pluginPath
    if ($LASTEXITCODE -ne 0) { throw 'Packaged CLAP ABI smoke test failed.' }
    Get-FileHash -LiteralPath $pluginPath -Algorithm SHA256
    Write-Output "CLAP prototype built and ABI checked at $pluginPath"
} finally { Pop-Location }
