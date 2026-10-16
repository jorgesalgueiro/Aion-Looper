param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    New-Item -ItemType Directory -Force .toolchain | Out-Null
    if (!(Test-Path .toolchain/asio-sdk/ASIOSDK/common/asio.h)) {
        Invoke-WebRequest -Uri 'https://www.steinberg.net/asiosdk' -OutFile .toolchain/asio-sdk.zip
        [IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $projectRoot '.toolchain/asio-sdk.zip'),(Join-Path $projectRoot '.toolchain/asio-sdk'),$true)
        if (!(Test-Path .toolchain/asio-sdk/ASIOSDK/common/asio.h)) { throw 'Unexpected ASIO SDK archive layout. Set CPAL_ASIO_DIR to the SDK root.' }
    }
    if (!(Test-Path .toolchain/libclang/clang/native/libclang.dll)) {
        $metadata = Invoke-RestMethod -Uri 'https://pypi.org/pypi/libclang/18.1.1/json'
        $wheel = @($metadata.urls | Where-Object filename -eq 'libclang-18.1.1-py2.py3-none-win_amd64.whl')
        if ($wheel.Count -ne 1) { throw 'Pinned Windows libclang wheel not found.' }
        Invoke-WebRequest -Uri $wheel[0].url -OutFile .toolchain/libclang.zip
        if ((Get-FileHash .toolchain/libclang.zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $wheel[0].digests.sha256) { throw 'libclang checksum mismatch.' }
        [IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $projectRoot '.toolchain/libclang.zip'),(Join-Path $projectRoot '.toolchain/libclang'),$true)
    }
    if (!(Test-Path .toolchain/clang-headers/clang-18.1.8.src/lib/Headers/x86intrin.h)) {
        $sevenZip = Get-Command 7z -ErrorAction SilentlyContinue
        $extractor = if ($sevenZip) { $sevenZip.Source } else { 'C:\Program Files\7-Zip\7z.exe' }
        if (!(Test-Path -LiteralPath $extractor)) { throw 'Install 7-Zip or place 7z on PATH to extract official Clang headers.' }
        Invoke-WebRequest -Uri 'https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang-18.1.8.src.tar.xz' -OutFile .toolchain/clang-18.1.8.src.tar.xz
        & $extractor x .toolchain/clang-18.1.8.src.tar.xz '-o.toolchain/clang-headers' -y
        if ($LASTEXITCODE -ne 0) { throw 'Clang xz extraction failed.' }
        & $extractor x .toolchain/clang-headers/clang-18.1.8.src.tar 'clang-18.1.8.src/lib/Headers/*' '-o.toolchain/clang-headers' -y
        if ($LASTEXITCODE -ne 0) { throw 'Clang header extraction failed.' }
    }
    Write-Output 'Local ASIO dependencies ready. Build with scripts/build-desktop.ps1 -Asio.'
    Write-Output 'The downloaded ASIO SDK has separate license terms; read .toolchain/asio-sdk/ASIOSDK/LICENSE.txt before redistributing ASIO-linked binaries.'
} finally { Pop-Location }
