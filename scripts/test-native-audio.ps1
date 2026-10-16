param([switch]$PhysicalInput,[string]$AsioDriver='TOPPING Pro USB Audio Device',[ValidateRange(1,60)][double]$Seconds=1.5,[ValidateRange(0,35)][double]$IdleSeconds=0)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$packagePath=Join-Path $projectRoot 'dist/Aion-Looper-Windows'
$reportDirectory=Join-Path $projectRoot 'verification'
New-Item -ItemType Directory -Force $reportDirectory | Out-Null
$name=if($PhysicalInput){'native-audio-asio'}else{'native-audio-standalone'}
$report=Join-Path $reportDirectory ($name+'.json')
if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
$arguments=@('--native-self-test',('"'+$report+'"'),'--input-test-seconds',$Seconds.ToString([Globalization.CultureInfo]::InvariantCulture),'--native-idle-seconds',$IdleSeconds.ToString([Globalization.CultureInfo]::InvariantCulture))
if($PhysicalInput){$arguments+=@('--native-test-physical','--input-host','ASIO','--input-device',('"'+$AsioDriver+'"'))}
$process=Start-Process -FilePath (Join-Path $packagePath 'Aion-Looper.exe') -ArgumentList $arguments -WorkingDirectory $packagePath -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $reportDirectory ($name+'-stdout.log')) -RedirectStandardError (Join-Path $reportDirectory ($name+'-stderr.log'))
if(!(Test-Path -LiteralPath $report)){throw 'Native audio test exited without a report.'}
Get-Content -LiteralPath $report
$result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
if($process.ExitCode -ne 0 -or !$result.ok){throw "Native audio test failed: $report"}
exit 0
