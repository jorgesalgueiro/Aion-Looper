# Isolated DSP/browser sound oracle. Does not enable product FX controls.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (Get-NetTCPConnection -LocalPort 9236 -State Listen -ErrorAction SilentlyContinue) { throw 'EQ CDP port 9236 is in use; close its prior owned test session first.' }
$savedArguments = $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$savedProfile = $env:WEBVIEW2_USER_DATA_FOLDER
$preview = $null
try {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = '--remote-debugging-address=127.0.0.1 --remote-debugging-port=9236 --disable-background-timer-throttling --disable-renderer-backgrounding'
    $env:WEBVIEW2_USER_DATA_FOLDER = Join-Path $repo 'build/eq-ui-profile'
    $preview = Start-Process -FilePath (Join-Path $repo 'build/windows/AionLooper_artefacts/Release/Standalone/Aion-Looper.exe') -WindowStyle Hidden -PassThru
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = $savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER = $savedProfile
    & node (Join-Path $repo 'tests/EqWebAudio.cjs')
    if ($LASTEXITCODE -ne 0) { throw 'Actual original WebAudio EQ comparison failed.' }
} finally {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = $savedArguments
    $env:WEBVIEW2_USER_DATA_FOLDER = $savedProfile
    if ($preview) {
        $preview.Refresh()
        if (!$preview.HasExited) {
            $preview.CloseMainWindow() | Out-Null
            if (!$preview.WaitForExit(10000)) { throw "Owned EQ test app did not shut down: PID $($preview.Id)." }
        }
    }
}
