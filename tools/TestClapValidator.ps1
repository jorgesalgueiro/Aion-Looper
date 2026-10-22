# Opt-in complete Windows CLAP wrapper validation, without physical devices.
param([string] $ValidatorPath='', [string] $PluginPath='', [string] $OutputDirectory='build/clap-validator', [switch] $DownloadTool)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=Split-Path -Parent $PSScriptRoot
$toolDirectory=Join-Path $repo '.toolchain/clap-validator/0.4.1'
$archiveHash='D935C3AF0A45C3911EA2E900F4AA5D6709DAC82BB485F0C4CE28648AB2CD0C10'
$executableHash='825439AD3FF98770A24656013EEB11591407687B97058E1DB9549C4F7D53E38A'
if($DownloadTool){
    if($ValidatorPath){throw 'Omit ValidatorPath with DownloadTool.'}
    [IO.Directory]::CreateDirectory($toolDirectory) | Out-Null
    $archive=Join-Path $toolDirectory 'clap-validator-windows.zip'
    if(!(Test-Path -LiteralPath $archive)){
        Invoke-WebRequest 'https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-windows.zip' -OutFile $archive
    }
    if((Get-FileHash -LiteralPath $archive).Hash -ne $archiveHash){throw 'CLAP validator archive digest mismatch.'}
    Expand-Archive -LiteralPath $archive -DestinationPath $toolDirectory -Force
}
if(!$ValidatorPath){$ValidatorPath=Join-Path $toolDirectory 'clap-validator.exe'}
if(!$PluginPath){$PluginPath=Join-Path $repo 'build/windows/AionLooper_artefacts/Release/CLAP/Aion-Looper.clap'}
$ValidatorPath=(Resolve-Path -LiteralPath $ValidatorPath).Path
$PluginPath=(Resolve-Path -LiteralPath $PluginPath).Path
if((Get-FileHash -LiteralPath $ValidatorPath).Hash -ne $executableHash){throw 'Use the pinned official Windows CLAP validator 0.4.1.'}
$binaryHash=(Get-FileHash -LiteralPath $PluginPath).Hash
if(![IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory=Join-Path $repo $OutputDirectory}
$runDirectory=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($runDirectory) | Out-Null
# Validator reads test filters from cwd/ancestors. Refuse rather than silently skip.
for($directory=[IO.DirectoryInfo]$runDirectory;$null -ne $directory;$directory=$directory.Parent){
    if(Test-Path -LiteralPath (Join-Path $directory.FullName 'clap-validator.toml')){throw "Remove external test filtering configuration from $($directory.FullName) before validation."}
}
$expected=@('plugin-library:query-nonexistent-factory',
    'plugin-library:create-id-with-trailing-garbage',
    'plugin-library:scan-rtld-now',
    'plugin-library:scan-time',
    'plugin-library:preset-discovery-crawl',
    'plugin-library:preset-discovery-descriptor-consistency',
    'plugin-library:preset-discovery-load',
    'plugin-instance:descriptor-consistency',
    'plugin-instance:features-categories',
    'plugin-instance:features-duplicates',
    'plugin-instance:process-audio-basic-out-of-place',
    'plugin-instance:process-audio-basic-in-place',
    'plugin-instance:process-audio-double-out-of-place',
    'plugin-instance:process-audio-double-in-place',
    'plugin-instance:process-audio-denormals',
    'plugin-instance:process-sleep-constant-mask',
    'plugin-instance:process-sleep-process-status',
    'plugin-instance:process-note-out-of-place-basic',
    'plugin-instance:process-note-inconsistent',
    'plugin-instance:process-note-wildcard',
    'plugin-instance:process-varying-sample-rates',
    'plugin-instance:process-varying-block-sizes',
    'plugin-instance:process-random-block-sizes',
    'plugin-instance:process-reset-reactivate',
    'plugin-instance:layout-audio-ports-activation',
    'plugin-instance:layout-audio-ports-config',
    'plugin-instance:layout-configurable-audio-ports',
    'plugin-instance:param-set-events',
    'plugin-instance:param-set-no-cookies',
    'plugin-instance:param-set-wrong-namespace',
    'plugin-instance:param-fuzz-basic',
    'plugin-instance:param-fuzz-bounds',
    'plugin-instance:param-fuzz-sample-accurate',
    'plugin-instance:param-fuzz-modulation',
    'plugin-instance:param-conversions',
    'plugin-instance:param-default-values',
    'plugin-instance:state-invalid-empty',
    'plugin-instance:state-invalid-random',
    'plugin-instance:state-reproducibility-basic',
    'plugin-instance:state-reproducibility-binary',
    'plugin-instance:state-reproducibility-buffered',
    'plugin-instance:transport-null',
    'plugin-instance:transport-fuzz',
    'plugin-instance:transport-fuzz-sample-accurate')
$allowedSkips=@('scan-rtld-now','preset-discovery-crawl','preset-discovery-descriptor-consistency','preset-discovery-load',
    'process-audio-double-out-of-place','process-audio-double-in-place','process-note-wildcard',
    'layout-audio-ports-activation','layout-audio-ports-config','layout-configurable-audio-ports')
$arguments=@('validate','--json','--hide-output','--jobs','1',$PluginPath)
foreach($argument in $arguments){if($argument.Contains('"') -or $argument.Contains("`r") -or $argument.Contains("`n")){throw 'Invalid validator argument.'}}
$quotedArguments=($arguments | ForEach-Object {'"'+$_+'"'}) -join ' '
$head=& git -C $repo rev-parse HEAD
if($LASTEXITCODE -ne 0){throw 'Cannot identify source revision.'}
$dirty=@(& git -C $repo status --porcelain --untracked-files=no)
if($LASTEXITCODE -ne 0){throw 'Cannot identify local edits.'}
$result=[ordered]@{
    schemaVersion=1;sourceCommit="$head";trackedChanges=$dirty;startedUtc=[DateTime]::UtcNow.ToString('o');completedUtc=$null
    harnessSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash
    validatorVersion='0.4.1-127-g152b982';validatorSha256=$executableHash;archiveSha256=$archiveHash
    pluginPath=$PluginPath;binarySha256=$binaryHash;binarySha256After=$null;arguments=$arguments
    testCount=0;passedCount=0;skipped=@();warnings=@();failures=@();passed=$false;exitCode=$null;error=$null
    guiTested=$false;allocationTested=$false;hardwareTested=$false
}
try{
    $process=Start-Process -FilePath $ValidatorPath -ArgumentList $quotedArguments -WorkingDirectory $runDirectory -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput (Join-Path $runDirectory 'stdout.json') -RedirectStandardError (Join-Path $runDirectory 'stderr.log')
    $result.exitCode=$process.ExitCode
    $validationReport=[IO.File]::ReadAllText((Join-Path $runDirectory 'stdout.json')) | ConvertFrom-Json
    $seen=@{}
    foreach($entry in $validationReport.results){
        $kind=@($entry.test.PSObject.Properties)[0];$test=$kind.Value
        $key=$kind.Name+':'+$test.test
        if($seen.ContainsKey($key) -or $key -notin $expected){throw "Unexpected/duplicate test: $key"}
        if($test.path -ne $PluginPath){throw 'Validator tested a different module.'}
        if($kind.Name -eq 'plugin-instance' -and $test.plugin_id -ne 'com.jorgesalgueiro.aionlooper'){throw 'Unexpected plugin ID.'}
        $seen[$key]=$true
        switch($entry.status.code){
            'success' {$result.passedCount++}
            'skipped' {$result.skipped+=@{test=$key;reason=$entry.status.details};if($test.test -notin $allowedSkips){$result.failures+=$key}}
            'warning' {
                $result.warnings+=@{test=$key;reason=$entry.status.details}
                # JUCE setStateInformation returns void: wrapper cannot report rejection.
                if($test.test -ne 'state-invalid-random' -or $entry.status.details -ne 'The plugin loaded random bytes successfully, which is unexpected, but the plugin did not crash.'){$result.failures+=$key}
            }
            default {$result.failures+=@{test=$key;status=$entry.status}}
        }
    }
    $result.testCount=$seen.Count
    $result.binarySha256After=(Get-FileHash -LiteralPath $PluginPath).Hash
    $missing=@($expected | Where-Object {!$seen.ContainsKey($_)})
    $result.passed=$process.ExitCode -eq 0 -and $result.failures.Count -eq 0 -and $missing.Count -eq 0 -and $result.binarySha256After -eq $binaryHash
    if(!$result.passed){throw "CLAP validation failed or incomplete; see $runDirectory"}
    Write-Output "PASS: all $($result.testCount) CLAP tests accounted for: $($result.passedCount) passed, $($result.skipped.Count) unsupported/platform skips, $($result.warnings.Count) retained warnings."
    Write-Output "Evidence: $(Join-Path $runDirectory 'result.json')"
}catch{$result.error=$_.Exception.Message;throw}
finally{
    $result.completedUtc=[DateTime]::UtcNow.ToString('o')
    [IO.File]::WriteAllText((Join-Path $runDirectory 'result.json'),($result | ConvertTo-Json -Depth 9),[Text.UTF8Encoding]::new($false))
}
