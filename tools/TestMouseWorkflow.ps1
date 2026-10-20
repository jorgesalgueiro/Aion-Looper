param([int[]] $Blocks = @(64, 128), [switch] $ExportOnly, [switch] $MergeOnly)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$hostPath = Join-Path $repo 'build/windows/Release/aion_mouse_host.exe'
if (!(Test-Path -LiteralPath $hostPath)) { throw 'Build target aion_mouse_host first.' }
if (Get-NetTCPConnection -LocalPort 9235 -State Listen -ErrorAction SilentlyContinue) { throw 'CDP port 9235 is already in use. Close the previous test host first.' }
$savedArguments = $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$savedProfile = $env:WEBVIEW2_USER_DATA_FOLDER
foreach ($block in $Blocks) {
    if ($block -notin @(64,128)) { throw 'Supported test block sizes: 64, 128.' }
    $preview = $null
    try {
        $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = '--remote-debugging-address=127.0.0.1 --remote-debugging-port=9235 --disable-background-timer-throttling --disable-renderer-backgrounding'
        $env:WEBVIEW2_USER_DATA_FOLDER = Join-Path $repo "build/mouse-profile-$block"
        $preview = Start-Process -FilePath $hostPath -ArgumentList "$block" -WindowStyle Hidden -PassThru
        $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = $savedArguments
        $env:WEBVIEW2_USER_DATA_FOLDER = $savedProfile
        if (!$ExportOnly -and !$MergeOnly) {
            & node (Join-Path $repo 'tests/MouseWorkflow.cjs') $block
            if ($LASTEXITCODE -ne 0) { throw "Mouse workflow failed at $block samples." }
        }
        if (!$MergeOnly) {
            & node (Join-Path $repo 'tests/ExportWorkflow.cjs') $block
            if ($LASTEXITCODE -ne 0) { throw "WAV export workflow failed at $block samples." }
        }
        if (!$ExportOnly) {
            & node (Join-Path $repo 'tests/MergeWorkflow.cjs') $block
            if ($LASTEXITCODE -ne 0) { throw "Merge workflow failed at $block samples." }
        }
        Write-Output "PASS: production processor/editor mouse workflow at $block samples."
    } finally {
        $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = $savedArguments
        $env:WEBVIEW2_USER_DATA_FOLDER = $savedProfile
        if ($preview -and !$preview.HasExited) {
            $preview.CloseMainWindow() | Out-Null
            if (!$preview.WaitForExit(10000)) { throw "Test host did not shut down: PID $($preview.Id)." }
        }
    }
}
