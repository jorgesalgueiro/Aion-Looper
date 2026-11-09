# Actual browser sound oracle in an owned hardware-free development host.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(Get-NetTCPConnection -LocalPort 9237 -State Listen -ErrorAction SilentlyContinue){throw 'Compressor test port 9237 is in use.'}
if(Get-Process -Name aion_mouse_host -ErrorAction SilentlyContinue){throw 'Close the prior owned mouse host first.'}
$savedArguments=$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$savedProfile=$env:WEBVIEW2_USER_DATA_FOLDER
$preview=$null
try {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS='--remote-debugging-address=127.0.0.1 --remote-debugging-port=9237 --disable-background-timer-throttling --disable-renderer-backgrounding'
    $env:WEBVIEW2_USER_DATA_FOLDER=Join-Path $repo ('build/compressor-oracle-profile-'+[guid]::NewGuid().ToString('N'))
    $preview=Start-Process -FilePath (Join-Path $repo 'build/windows/Release/aion_mouse_host.exe') -ArgumentList '64' -WindowStyle Hidden -PassThru
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
    & node (Join-Path $repo 'tests/CompressorWebAudio.cjs')
    if($LASTEXITCODE -ne 0){throw 'Original compressor browser sound comparison failed.'}
} finally {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
    if($preview){$preview.Refresh();if(!$preview.HasExited){$preview.CloseMainWindow() | Out-Null;if(!$preview.WaitForExit(10000)){throw "Owned compressor test app did not shut down: PID $($preview.Id)."}}}
}
