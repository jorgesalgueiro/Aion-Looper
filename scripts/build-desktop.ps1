param([switch]$Jack,[switch]$Asio)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    $buildArguments = @('build','--release','-p','looper-desktop')
    $audioFeatures = @()
    if ($Jack) { $audioFeatures += 'jack' }
    if ($Asio) { $audioFeatures += 'asio' }
    if ($audioFeatures.Count) { $buildArguments += @('--features',($audioFeatures -join ',')) }
    & (Join-Path $PSScriptRoot 'cargo.ps1') @buildArguments
    if ($LASTEXITCODE -ne 0) { throw 'Desktop build failed.' }
    $packagePath = Join-Path $projectRoot 'dist\Aion-Looper-Windows'
    New-Item -ItemType Directory -Force -Path $packagePath | Out-Null
    $cargoTargetRoot = if ($env:CARGO_TARGET_DIR) {
        if ([IO.Path]::IsPathRooted($env:CARGO_TARGET_DIR)) { $env:CARGO_TARGET_DIR }
        else { Join-Path $projectRoot $env:CARGO_TARGET_DIR }
    } else { Join-Path $projectRoot 'target' }
    Copy-Item -LiteralPath (Join-Path $cargoTargetRoot 'release\Aion-Looper.exe') -Destination $packagePath
    # Remove only obsolete files produced by earlier versions of this packager.
    foreach ($obsoleteFile in @('rust_looper_desktop.dll','RustLooperHost.cs','Try-Aion-Looper.cmd')) {
        $obsoletePath = Join-Path $packagePath $obsoleteFile
        if (Test-Path -LiteralPath $obsoletePath) { Remove-Item -LiteralPath $obsoletePath }
    }
    $cargoRootPath = if (Test-Path -LiteralPath '.toolchain\cargo') {
        Join-Path $projectRoot '.toolchain\cargo'
    } elseif ($env:CARGO_HOME) { $env:CARGO_HOME } else { Join-Path $env:USERPROFILE '.cargo' }
    $loader = Get-ChildItem -Path (Join-Path $cargoRootPath 'registry\src\*\webview2-com-sys-0.39.1\x64\WebView2Loader.dll') | Select-Object -First 1
    if (!$loader) { throw 'The locked WebView2 loader was not found in the Cargo registry.' }
    Copy-Item -LiteralPath $loader.FullName -Destination $packagePath
    $cxxRuntime = Join-Path $projectRoot '.toolchain\gnu-tools\mingw64\bin'
    foreach ($runtime in @('libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')) {
        $runtimePath=Join-Path $cxxRuntime $runtime
        if (!(Test-Path -LiteralPath $runtimePath)) { throw "Missing native C++ runtime: run scripts/setup-native-cxx.ps1 ($runtime)" }
        Copy-Item -LiteralPath $runtimePath -Destination $packagePath
    }
    Copy-Item -LiteralPath 'crates\looper-stretch\vendor\rubberband-4.0.0\COPYING' -Destination (Join-Path $packagePath 'RubberBand-LICENSE.txt')
    $runtimeLicenses=Join-Path $projectRoot '.toolchain\gnu-tools\mingw64\share\licenses'
    Copy-Item -LiteralPath (Join-Path $runtimeLicenses 'libstdc++\COPYING.RUNTIME') -Destination (Join-Path $packagePath 'GCC-Runtime-LICENSE.txt')
    Copy-Item -LiteralPath (Join-Path $runtimeLicenses 'libstdc++\COPYING3') -Destination (Join-Path $packagePath 'GCC-GPL3.txt')
    Copy-Item -LiteralPath (Join-Path $runtimeLicenses 'libwinpthread\COPYING') -Destination (Join-Path $packagePath 'Winpthreads-LICENSE.txt')
    Copy-Item -LiteralPath 'LICENSE' -Destination $packagePath
    $midiLicense=Get-ChildItem -Path (Join-Path $cargoRootPath 'registry\src\*\midir-0.11.1\LICENSE') | Select-Object -First 1
    if (!$midiLicense) { throw 'Pinned midir license was not found.' }
    Copy-Item -LiteralPath $midiLicense.FullName -Destination (Join-Path $packagePath 'midir-LICENSE.txt')
    Copy-Item -LiteralPath 'crates\looper-desktop\assets\vendor\SoundTouch-LICENSE.txt' -Destination $packagePath
    Copy-Item -LiteralPath 'docs\DESKTOP_PREVIEW.txt' -Destination (Join-Path $packagePath 'README.txt')
    @("ASIO=$($Asio.IsPresent)","JACK=$($Jack.IsPresent)") | Set-Content -LiteralPath (Join-Path $packagePath 'BUILD_FEATURES.txt') -Encoding utf8NoBOM
    Write-Output "Desktop app built at $packagePath\Aion-Looper.exe"
} finally { Pop-Location }
