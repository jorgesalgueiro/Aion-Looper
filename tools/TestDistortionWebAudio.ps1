# Original WaveShaper branch in the existing owned offline effects browser host.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if((Get-NetTCPConnection -LocalPort 9239 -State Listen -ErrorAction SilentlyContinue) -or (Get-Process -Name aion_effects_browser_host -ErrorAction SilentlyContinue)){throw 'Close the prior owned effects browser host first.'}
$savedArguments=$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$savedProfile=$env:WEBVIEW2_USER_DATA_FOLDER
$preview=$null
try{
 $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS='--remote-debugging-address=127.0.0.1 --remote-debugging-port=9239 --disable-background-timer-throttling --disable-renderer-backgrounding'
 $env:WEBVIEW2_USER_DATA_FOLDER=Join-Path $repo ('build/distortion-oracle-profile-'+[guid]::NewGuid().ToString('N'))
 $preview=Start-Process -FilePath (Join-Path $repo 'build/windows/Release/aion_effects_browser_host.exe') -WindowStyle Hidden -PassThru
 $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments;$env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
 & node (Join-Path $repo 'tests/DistortionWaveShaperWebAudio.cjs')
 if($LASTEXITCODE -ne 0){throw 'Original WaveShaper browser comparison failed.'}
}finally{
 $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments;$env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
 if($preview){$preview.Refresh();if(!$preview.HasExited){$preview.CloseMainWindow()|Out-Null;if(!$preview.WaitForExit(10000)){throw "Owned effects browser failed to close: PID $($preview.Id)"}}}
}
