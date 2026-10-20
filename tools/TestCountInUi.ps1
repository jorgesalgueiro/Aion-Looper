$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (Get-NetTCPConnection -LocalPort 9234 -State Listen -ErrorAction SilentlyContinue) { throw 'CDP port 9234 is already in use. Close the previous test session first.' }
$savedArguments = $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$savedProfile = $env:WEBVIEW2_USER_DATA_FOLDER
$preview = $null
try {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = '--remote-debugging-address=127.0.0.1 --remote-debugging-port=9234 --disable-background-timer-throttling --disable-renderer-backgrounding'
    $env:WEBVIEW2_USER_DATA_FOLDER = Join-Path $repo 'build/countin-ui-profile'
    $preview = Start-Process -FilePath (Join-Path $repo 'build/windows/AionLooper_artefacts/Release/Standalone/Aion-Looper.exe') -WindowStyle Hidden -PassThru
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = $savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER = $savedProfile
    & node (Join-Path $repo 'tests/CountInWebAudio.cjs')
    if ($LASTEXITCODE -ne 0) { throw 'Original WebAudio count-in sound comparison failed.' }
    & node (Join-Path $repo 'tests/NudgeWebAudio.cjs')
    if ($LASTEXITCODE -ne 0) { throw 'Tiny-clip WebAudio nudge regression failed.' }
    & node (Join-Path $repo 'tests/LiveInputWebAudio.cjs')
    if ($LASTEXITCODE -ne 0) { throw 'Live input WebAudio downmix comparison failed.' }
    & node (Join-Path $repo 'tests/NativeUiSmoke.cjs')
    if ($LASTEXITCODE -ne 0) { throw 'Native UI smoke failed.' }
} finally {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = $savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER = $savedProfile
    if ($preview -and !$preview.HasExited) {
        $preview.CloseMainWindow() | Out-Null
        if (!$preview.WaitForExit(10000)) { throw "Test app did not shut down: PID $($preview.Id)." }
    }
}
