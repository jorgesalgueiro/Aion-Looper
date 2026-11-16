param([int[]]$Blocks=@(64,128))
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if((Get-NetTCPConnection -LocalPort 9235 -State Listen -ErrorAction SilentlyContinue) -or (Get-Process -Name aion_mouse_host -ErrorAction SilentlyContinue)){throw 'Previous test host/port is in use.'}
$savedArguments=$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS;$savedProfile=$env:WEBVIEW2_USER_DATA_FOLDER;$savedRun=$env:AION_MOUSE_TEST_RUN
foreach($block in $Blocks){
 if($block -notin @(64,128)){throw 'Use 64/128.'};$preview=$null
 try{
  $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS='--remote-debugging-address=127.0.0.1 --remote-debugging-port=9235 --disable-background-timer-throttling --disable-renderer-backgrounding'
  $env:AION_MOUSE_TEST_RUN=[guid]::NewGuid().ToString('N');$env:WEBVIEW2_USER_DATA_FOLDER=Join-Path $repo "build/visibility-profile-$block-$($env:AION_MOUSE_TEST_RUN)"
  $preview=Start-Process -FilePath (Join-Path $repo 'build/windows/Release/aion_mouse_host.exe') -ArgumentList "$block --steady-start" -WindowStyle Hidden -PassThru
  $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments;$env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile
  & 'C:/Program Files/nodejs/node.exe' (Join-Path $repo 'tests/EditorVisibilityUi.cjs') $block
  if($LASTEXITCODE -ne 0){throw "Visibility acceptance failed at $block."}
 }finally{
  $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$savedArguments;$env:WEBVIEW2_USER_DATA_FOLDER=$savedProfile;$env:AION_MOUSE_TEST_RUN=$savedRun
  if($preview -and !$preview.HasExited){$preview.CloseMainWindow()|Out-Null;if(!$preview.WaitForExit(10000)){throw "Owned visibility host failed to close: $($preview.Id)"}}
 }
}
