# Package only the already-built Windows Standalone. No build, installer or SDKs.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build/windows/AionLooper_artefacts/Release/Standalone/Aion-Looper.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Build AionLooper_Standalone Release first.' }
$revision = (& git -C $repo rev-parse --short=12 HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the source revision.' }
$version = (Get-Item -LiteralPath $exe).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Unexpected Standalone product version.' }
$dist = Join-Path $repo 'dist'
$stage = Join-Path $repo ('build/standalone-preview-' + [guid]::NewGuid().ToString('N'))
$app = New-Item -ItemType Directory -Path (Join-Path $stage 'Standalone') -Force
$notices = New-Item -ItemType Directory -Path (Join-Path $stage 'licenses') -Force
$docs = New-Item -ItemType Directory -Path (Join-Path $stage 'docs') -Force
Copy-Item -LiteralPath $exe -Destination $app.FullName
foreach ($name in @('BUILDING.md','PARITY.md','VALIDATION.md','LICENSING.md')) {
    Copy-Item -LiteralPath (Join-Path $repo "docs/$name") -Destination $docs.FullName
}
# Use the same pinned dependency notices as CMake, including bundled ASIO headers.
$licenses = @{
    'resources/ui/LICENSE' = 'Ascii-Looper-GPL-2.0.txt'
    '.deps/JUCE/LICENSE.md' = 'JUCE-LICENSE.md'
    '.deps/clap-juce-extensions/LICENSE.md' = 'CLAP-extension-LICENSE.txt'
    '.deps/clap-juce-extensions/clap-libs/clap/LICENSE' = 'CLAP-SDK-LICENSE.txt'
    '.deps/clap-juce-extensions/clap-libs/clap-helpers/LICENSE' = 'CLAP-helpers-LICENSE.txt'
    '.deps/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt' = 'VST3-SDK-LICENSE.txt'
    '.deps/JUCE/modules/juce_audio_devices/native/asio/LICENSE.txt' = 'ASIO-headers-LICENSE.txt'
    '.deps/nuget/Microsoft.Web.WebView2.1.0.3485.44/LICENSE.txt' = 'WebView2-LICENSE.txt'
    '.deps/nuget/Microsoft.Web.WebView2.1.0.3485.44/NOTICE.txt' = 'WebView2-NOTICE.txt'
}
foreach ($entry in $licenses.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $repo $entry.Key) -Destination (Join-Path $notices.FullName $entry.Value)
}
$changes = (& git -C $repo diff --name-only HEAD) -join ', '
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
@"
Aion-Looper $version - Windows x64 Standalone test preview
Source commit: $revision
Tracked local overrides: $changes
Executable SHA256: $hash

Extract the entire ZIP, then open Standalone/Aion-Looper.exe.
The Microsoft Evergreen WebView2 Runtime must be installed on the test PC.
Start with MASTER at zero for MIDI/control checks. Existing app settings apply.
Defaults: MIDI notes 46 OD, 47 SUB, 48 SUS. Every positive mode NoteOn toggles;
NoteOff is not required for these three mode keys. Loop keys retain hold behavior.
See docs/VALIDATION.md and docs/PARITY.md for tested scope and remaining migration.
This is a test preview; other platforms, drivers and plugin hosts require validation.
"@ | Set-Content -LiteralPath (Join-Path $stage 'TEST-BUILD.txt') -Encoding utf8
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$archive = Join-Path $dist "Aion-Looper-$version-$revision-win64-Standalone.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
Get-FileHash -LiteralPath $archive -Algorithm SHA256 | Format-List
Write-Output $archive
