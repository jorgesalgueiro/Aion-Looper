$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (Get-NetTCPConnection -LocalPort 9235 -State Listen -ErrorAction SilentlyContinue) { throw 'CDP port 9235 is already in use.' }
if (Get-Process -Name aion_mouse_host -ErrorAction SilentlyContinue) { throw 'An existing mouse test host is running.' }
$savedArguments=$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$savedProfile=$env:WEBVIEW2_USER_DATA_FOLDER
$preview=$null
try {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS='--remote-debugging-address=127.0.0.1 --remote-debugging-port=9235'
    $env:WEBVIEW2_USER_DATA_FOLDER=Join-Path $repo ('build/meter-window-profile-'+[guid]::NewGuid().ToString('N'))
    $preview=Start-Process -FilePath (Join-Path $repo 'build/windows/Release/aion_mouse_host.exe') -ArgumentList '64' -WindowStyle Hidden -PassThru
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
    & node (Join-Path $repo 'tests/MeterWindowWebAudio.cjs')
    if ($LASTEXITCODE -ne 0) { throw 'Original browser meter PCM comparison failed.' }
} finally {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
    if ($preview -and !$preview.HasExited) {
        $preview.CloseMainWindow() | Out-Null
        if (!$preview.WaitForExit(10000)) { throw "Owned meter test host did not shut down: PID $($preview.Id)." }
    }
}
