param([string]$ReaperExecutable = 'C:\Program Files\REAPER (x64)\reaper.exe',[switch]$StateRoundTrip)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$plugin = Join-Path $projectRoot 'dist\Aion-Looper-CLAP-Windows\Aion-Looper-Prototype.clap'
if (!(Test-Path -LiteralPath $plugin)) { throw 'Build the CLAP prototype first.' }
if (!(Test-Path -LiteralPath $ReaperExecutable)) { throw 'REAPER executable not found.' }
$testRoot = Join-Path $projectRoot 'verification\reaper-clap-portable'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
Set-Content -LiteralPath (Join-Path $testRoot 'state-mode.txt') -Value ([int]$StateRoundTrip.IsPresent)
Set-Content -LiteralPath (Join-Path $testRoot 'reaper-clap-progress.log') -Value ''
# Explicit -cfgfile selects the copied executable's test resource directory.
# Lua checks that directory before exercising the plugin. Normal REAPER
# installation, preferences and plugin paths are not modified by this test.
Copy-Item -LiteralPath $ReaperExecutable -Destination (Join-Path $testRoot 'reaper.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'reaper-clap-smoke.lua') -Destination $testRoot
$testFxPath = Join-Path $testRoot 'UserPlugins\FX'
New-Item -ItemType Directory -Force -Path $testFxPath | Out-Null
Copy-Item -LiteralPath $plugin -Destination $testFxPath
$testPluginsPath = Join-Path $testRoot 'Plugins'
New-Item -ItemType Directory -Force -Path $testPluginsPath | Out-Null
$hostHelper = Join-Path (Split-Path -Parent $ReaperExecutable) 'Plugins\reaper_host64.exe'
if (Test-Path -LiteralPath $hostHelper) { Copy-Item -LiteralPath $hostHelper -Destination $testPluginsPath }
$pluginDirectory = Split-Path -Parent $plugin
$iniPath = Join-Path $testRoot 'reaper.ini'
$reportPath = Join-Path $testRoot 'reaper-clap-smoke.json'
if (Test-Path -LiteralPath $reportPath) { Remove-Item -LiteralPath $reportPath }
@"
[REAPER]
clap_path_win64=$pluginDirectory
vstpath64=
lv2path=
loadlastproj=0
showlastundo=0
saveundostatesproj=0
audio_close_stop=1
audio_close_inactive=1
[clap]
clap_path_win64=$pluginDirectory
"@ | Set-Content -LiteralPath $iniPath
$process = Start-Process -FilePath (Join-Path $testRoot 'reaper.exe') -ArgumentList @('-newinst','-nosplash','-noactivate','-cfgfile',('"'+$iniPath+'"'),('"'+(Join-Path $testRoot 'reaper-clap-smoke.lua')+'"')) -Environment @{ CLAP_PATH=$pluginDirectory } -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds(45)
while (!$process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 250
    $process.Refresh()
}
if (!$process.HasExited) {
    Stop-Process -Id $process.Id
    throw 'Isolated REAPER test timed out; only the test process was stopped.'
}
if (!(Test-Path -LiteralPath $reportPath)) { throw 'REAPER produced no smoke-test report.' }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$report | Add-Member -NotePropertyName plugin_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $plugin -Algorithm SHA256).Hash -Force
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $projectRoot 'verification\reaper-clap.json')
$report | ConvertTo-Json -Depth 5
if (!$report.ok) { throw 'REAPER CLAP scan/load test failed.' }
