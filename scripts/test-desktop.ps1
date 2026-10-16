param([switch]$PhysicalInput)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$packagePath = Join-Path $projectRoot 'dist\Aion-Looper-Windows'
$reportDirectory = Join-Path $projectRoot 'verification'
New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
$testMode = if ($PhysicalInput) { '--input-smoke-test' } else { '--self-test' }
$reportName = if ($PhysicalInput) { 'physical-input-standalone' } else { 'desktop-ui-standalone' }
$reportPath = Join-Path $reportDirectory ($reportName + '.json')
if (Test-Path -LiteralPath $reportPath) { Remove-Item -LiteralPath $reportPath }
if ($PhysicalInput) { Write-Output 'Testing physical microphone briefly: output muted, captured audio discarded.' }
$desktopProcess = Start-Process -FilePath (Join-Path $packagePath 'Aion-Looper.exe') `
    -ArgumentList @($testMode,('"' + $reportPath + '"')) `
    -WorkingDirectory $packagePath -WindowStyle Hidden -Wait -PassThru `
    -RedirectStandardOutput (Join-Path $reportDirectory ($reportName + '-stdout.log')) `
    -RedirectStandardError (Join-Path $reportDirectory ($reportName + '-stderr.log'))
if (!(Test-Path -LiteralPath $reportPath)) { throw 'Desktop exited without a test report. Inspect its stderr log.' }
Get-Content -LiteralPath $reportPath
if ($desktopProcess.ExitCode -ne 0) { throw "Desktop test failed; inspect $reportPath and its stderr log." }
