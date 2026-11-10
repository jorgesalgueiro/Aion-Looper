# Actual original worklet in an owned hardware-free development host.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(Get-NetTCPConnection -LocalPort 9239 -State Listen -ErrorAction SilentlyContinue){throw 'Arp Delay test port 9239 is in use.'}
if(Get-Process -Name aion_effects_browser_host -ErrorAction SilentlyContinue){throw 'Close the prior owned effects browser host first.'}
$savedArguments=$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$savedProfile=$env:WEBVIEW2_USER_DATA_FOLDER
$preview=$null
try {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS='--remote-debugging-address=127.0.0.1 --remote-debugging-port=9239 --disable-background-timer-throttling --disable-renderer-backgrounding'
    $env:WEBVIEW2_USER_DATA_FOLDER=Join-Path $repo ('build/arp-oracle-profile-'+[guid]::NewGuid().ToString('N'))
    $preview=Start-Process -FilePath (Join-Path $repo 'build/windows/Release/aion_effects_browser_host.exe') -WindowStyle Hidden -PassThru
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
    & node (Join-Path $repo 'tests/ArpDelayWebAudio.cjs')
    if($LASTEXITCODE -ne 0){throw 'Original Arp Delay browser sound comparison failed.'}
} finally {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
    if($preview){$preview.Refresh();if(!$preview.HasExited){$preview.CloseMainWindow() | Out-Null;if(!$preview.WaitForExit(10000)){throw "Owned Arp Delay test app did not shut down: PID $($preview.Id)."}}}
}
