param([int[]] $Blocks = @(64, 128), [switch] $ExportOnly, [switch] $MergeOnly, [switch] $MidiOnly, [switch] $SkipMidi, [switch] $QueuedNormOnly, [switch] $StoppingNormOnly, [switch] $QueuedHistoryOnly, [switch] $QueuedMultiplyOnly, [switch] $PostbirthMultiplyOnly, [switch] $FinalizingControlsOnly)
$ErrorActionPreference = 'Stop'
if ($SkipMidi -and $MidiOnly) { throw 'SkipMidi cannot be combined with MidiOnly.' }
$repo = Split-Path -Parent $PSScriptRoot
$hostPath = Join-Path $repo 'build/windows/Release/aion_mouse_host.exe'
if (!(Test-Path -LiteralPath $hostPath)) { throw 'Build target aion_mouse_host first.' }
if (Get-NetTCPConnection -LocalPort 9235 -State Listen -ErrorAction SilentlyContinue) { throw 'CDP port 9235 is already in use. Close the previous test host first.' }
$savedArguments = $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS
$savedProfile = $env:WEBVIEW2_USER_DATA_FOLDER
$savedTestRun = $env:AION_MOUSE_TEST_RUN
if (Get-Process -Name aion_mouse_host -ErrorAction SilentlyContinue) { throw 'An existing mouse test host is running. Close it before starting another workflow.' }
foreach ($block in $Blocks) {
    if ($block -notin @(64,128)) { throw 'Supported test block sizes: 64, 128.' }
    $preview = $null
    try {
        $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = '--remote-debugging-address=127.0.0.1 --remote-debugging-port=9235 --disable-background-timer-throttling --disable-renderer-backgrounding'
        $env:AION_MOUSE_TEST_RUN = [guid]::NewGuid().ToString('N')
        $env:WEBVIEW2_USER_DATA_FOLDER = Join-Path $repo "build/mouse-profile-$block-$($env:AION_MOUSE_TEST_RUN)"
        $preview = Start-Process -FilePath $hostPath -ArgumentList "$block" -WindowStyle Hidden -PassThru
        $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = $savedArguments
        $env:WEBVIEW2_USER_DATA_FOLDER = $savedProfile
        if ($FinalizingControlsOnly) {
            & node (Join-Path $repo 'tests/MouseWorkflow.cjs') $block --finalizing-controls-only
            if ($LASTEXITCODE -ne 0) { throw "Finalizing control workflow failed at $block samples" }
        } elseif ($PostbirthMultiplyOnly) {
            & node (Join-Path $repo 'tests/MouseWorkflow.cjs') $block --postbirth-multiply-only
            if ($LASTEXITCODE -ne 0) { throw "Post-birth Multiply workflow failed at $block samples" }
        } elseif ($QueuedMultiplyOnly) {
            & node (Join-Path $repo 'tests/MouseWorkflow.cjs') $block --queued-multiply-only
            if ($LASTEXITCODE -ne 0) { throw "Queued Multiply mouse workflow failed at $block samples" }
        } elseif ($QueuedHistoryOnly) {
            & node (Join-Path $repo 'tests/MouseWorkflow.cjs') $block --queued-history-only
            if ($LASTEXITCODE -ne 0) { throw "Queued history UI failed at $block samples." }
        } elseif ($StoppingNormOnly) {
            & node (Join-Path $repo 'tests/MouseWorkflow.cjs') $block --stopping-norm-only
            if ($LASTEXITCODE -ne 0) { throw "Stopping NORM UI failed at $block samples." }
        } elseif ($QueuedNormOnly) {
            & node (Join-Path $repo 'tests/MouseWorkflow.cjs') $block --queued-norm-only
            if ($LASTEXITCODE -ne 0) { throw "Queued NORM UI failed at $block samples." }
        } elseif (!$ExportOnly -and !$MergeOnly -and !$MidiOnly) {
            & node (Join-Path $repo 'tests/MouseWorkflow.cjs') $block
            if ($LASTEXITCODE -ne 0) { throw "Mouse workflow failed at $block samples." }
        }
        if (!$FinalizingControlsOnly -and !$PostbirthMultiplyOnly -and !$QueuedMultiplyOnly -and !$QueuedHistoryOnly -and !$QueuedNormOnly -and !$StoppingNormOnly -and !$MergeOnly -and !$MidiOnly) {
            & node (Join-Path $repo 'tests/ExportWorkflow.cjs') $block
            if ($LASTEXITCODE -ne 0) { throw "WAV export workflow failed at $block samples." }
        }
        if (!$FinalizingControlsOnly -and !$PostbirthMultiplyOnly -and !$QueuedMultiplyOnly -and !$QueuedHistoryOnly -and !$QueuedNormOnly -and !$StoppingNormOnly -and !$ExportOnly -and !$MidiOnly) {
            & node (Join-Path $repo 'tests/MergeWorkflow.cjs') $block
            if ($LASTEXITCODE -ne 0) { throw "Merge workflow failed at $block samples." }
        }
        if (!$FinalizingControlsOnly -and !$PostbirthMultiplyOnly -and !$QueuedMultiplyOnly -and !$QueuedHistoryOnly -and !$QueuedNormOnly -and !$StoppingNormOnly -and !$SkipMidi -and !$ExportOnly -and !$MergeOnly) {
            & node (Join-Path $repo 'tests/MidiWorkflow.cjs') $block
            if ($LASTEXITCODE -ne 0) { throw "MIDI workflow failed at $block samples." }
        }
        if ($FinalizingControlsOnly) { Write-Output "PASS: finalizing source controls at $block samples." }
        elseif ($PostbirthMultiplyOnly) { Write-Output "PASS: focused post-birth input/no-input MULT workflow at $block samples." }
        elseif ($QueuedMultiplyOnly) { Write-Output "PASS: focused queued-MULT processor/editor mouse workflow at $block samples." }
        elseif ($QueuedHistoryOnly) { Write-Output "PASS: focused queued-history processor/editor mouse workflow at $block samples." }
        elseif ($StoppingNormOnly) { Write-Output "PASS: focused stopping-NORM processor/editor mouse workflow at $block samples." }
        elseif ($QueuedNormOnly) { Write-Output "PASS: focused queued-NORM processor/editor mouse workflow at $block samples." }
        else { Write-Output "PASS: production processor/editor mouse workflow at $block samples." }
    } finally {
        $env:AION_MOUSE_TEST_RUN = $savedTestRun
        $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = $savedArguments
        $env:WEBVIEW2_USER_DATA_FOLDER = $savedProfile
        if ($preview -and !$preview.HasExited) {
            $preview.CloseMainWindow() | Out-Null
            if (!$preview.WaitForExit(10000)) { throw "Test host did not shut down: PID $($preview.Id)." }
        }
    }
}
