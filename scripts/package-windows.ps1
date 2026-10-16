param([switch]$SkipBuild,[switch]$Asio)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    if (!$SkipBuild) {
        & (Join-Path $PSScriptRoot 'build-desktop.ps1') -Asio:$Asio
        if ($LASTEXITCODE -ne 0) { throw 'Windows build failed.' }
    }
    & (Join-Path $PSScriptRoot 'test-desktop.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Windows graphical verification failed.' }
    & (Join-Path $PSScriptRoot 'test-native-audio.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Native audio graphical verification failed.' }
    $packagePath = Join-Path $projectRoot 'dist\Aion-Looper-Windows'
    if ($Asio) {
        if (!(Select-String -LiteralPath (Join-Path $packagePath 'BUILD_FEATURES.txt') -SimpleMatch 'ASIO=True' -Quiet)) { throw 'Rebuild with -Asio before packaging an ASIO archive.' }
        Copy-Item -LiteralPath 'vendor/asio-sys/LICENSE' -Destination (Join-Path $packagePath 'asio-sys-LICENSE.txt')
        $sdkRoot = if ($env:CPAL_ASIO_DIR) { $env:CPAL_ASIO_DIR } else { Join-Path $projectRoot '.toolchain/asio-sdk/ASIOSDK' }
        Copy-Item -LiteralPath (Join-Path $sdkRoot 'LICENSE.txt') -Destination (Join-Path $packagePath 'ASIO-SDK-LICENSE.txt')
        Copy-Item -LiteralPath 'docs/ASIO.md' -Destination $packagePath
    }
    Copy-Item -LiteralPath 'docs/NATIVE_MODE.md','docs/NATIVE_PROJECTS.md','docs/NATIVE_PARITY.md' -Destination $packagePath
    Copy-Item -LiteralPath 'docs\INPUT_TESTING.md' -Destination $packagePath
    Copy-Item -LiteralPath 'docs\RECORDING_TRANSFER.md' -Destination $packagePath
    $revision = & git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Commit the source before packaging.' }
    @("Source: https://github.com/jorgesalgueiro/Aion-Looper/tree/$revision", "Revision: $revision", 'License: GPL-2.0-only', 'Windows x64 preview. Extract the entire ZIP and run Aion-Looper.exe.', 'Microsoft Edge WebView2 Runtime is required. See README.txt and INPUT_TESTING.md.') |
        Set-Content -LiteralPath (Join-Path $packagePath 'SOURCE.txt') -Encoding utf8NoBOM
    Get-ChildItem -LiteralPath $packagePath -File |
        Where-Object Name -ne 'SHA256SUMS.txt' |
        Sort-Object Name |
        ForEach-Object { "{0}  {1}" -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name } |
        Set-Content -LiteralPath (Join-Path $packagePath 'SHA256SUMS.txt') -Encoding utf8NoBOM
    $archiveName = if ($Asio) { 'Aion-Looper-Windows-x64-ASIO-local.zip' } else { 'Aion-Looper-Windows-x64-preview.zip' }
    $archive = Join-Path $projectRoot ('dist\'+$archiveName)
    # Ship program files only, never a portable browser profile or user sessions.
    $packageFiles = Get-ChildItem -LiteralPath $packagePath -File
    Compress-Archive -LiteralPath $packageFiles.FullName -DestinationPath $archive -Force
    $checksum = Get-FileHash -LiteralPath $archive -Algorithm SHA256
    "{0}  {1}" -f $checksum.Hash.ToLowerInvariant(), (Split-Path -Leaf $archive) |
        Set-Content -LiteralPath ($archive + '.sha256') -Encoding ascii
    $checksum
} finally { Pop-Location }
