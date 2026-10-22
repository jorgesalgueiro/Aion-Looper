# Validate the built Windows VST3 through its public plugin interface.
# This is opt-in: no app/plugin installation, packaging or product changes.
param(
    [string] $ValidatorPath = '',
    [string] $PluginPath = '',
    [string] $OutputDirectory = 'build/pluginval',
    [ValidateRange(5,10)] [int] $Strictness = 9,
    [ValidateRange(0,2147483647)] [int] $Seed = 1337,
    [switch] $DownloadTool
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path -Parent $PSScriptRoot
$toolDirectory = Join-Path $repo '.toolchain/pluginval/1.0.4'
$expectedArchiveHash = 'C08E61CE3B96DB41636F8EC7E76F4C7E2C13EBDAC7FA1B5A1F52B4F32EC715AB'
$expectedExecutableHash = 'F4AC5C31C5544A434F73E686B816861D92C22AE849D69AD693681CBC9522FCD8'
if ($DownloadTool) {
    if ($ValidatorPath) { throw 'DownloadTool uses the pinned project-local validator; omit ValidatorPath.' }
    [IO.Directory]::CreateDirectory($toolDirectory) | Out-Null
    $archive = Join-Path $toolDirectory 'pluginval_Windows.zip'
    if (!(Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri 'https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip' -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedArchiveHash) {
        throw 'The pluginval archive does not match the pinned official v1.0.4 asset.'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $toolDirectory -Force
}
if (!$ValidatorPath) { $ValidatorPath = Join-Path $toolDirectory 'pluginval.exe' }
if (!$PluginPath) { $PluginPath = Join-Path $repo 'build/windows/AionLooper_artefacts/Release/VST3/Aion-Looper.vst3' }
$ValidatorPath = (Resolve-Path -LiteralPath $ValidatorPath).Path
$PluginPath = (Resolve-Path -LiteralPath $PluginPath).Path.TrimEnd('\','/')
if ((Get-FileHash -LiteralPath $ValidatorPath -Algorithm SHA256).Hash -ne $expectedExecutableHash) {
    throw 'Use the pinned pluginval Windows v1.0.4 executable (DownloadTool installs it locally).'
}
$binaryPath = Join-Path $PluginPath 'Contents/x86_64-win/Aion-Looper.vst3'
$binaryHash = (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash
if (![IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory = Join-Path $repo $OutputDirectory }
$runName = '{0}-level{1}-seed{2}-{3}' -f [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'),$Strictness,$Seed,[Guid]::NewGuid().ToString('N').Substring(0,8)
$runDirectory = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) $runName
[IO.Directory]::CreateDirectory($runDirectory) | Out-Null
$arguments = @('--validate-in-process','--strictness-level',"$Strictness",'--random-seed',"$Seed",'--repeat','1','--timeout-ms','30000',
    '--sample-rates','44100,48000,96000','--block-sizes','64,128,256,512,1024','--output-dir',$runDirectory,'--validate',$PluginPath)
# Paths are leaf names (no trailing backslash); quote each argument for Windows.
foreach ($argument in $arguments) {
    if ($argument.Contains('"') -or $argument.Contains("`r") -or $argument.Contains("`n")) { throw 'Invalid validator argument.' }
}
$quotedArguments = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
$stdoutPath = Join-Path $runDirectory 'stdout.log'
$stderrPath = Join-Path $runDirectory 'stderr.log'
$head = & git -C $repo rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the source checkout.' }
$dirty = @(& git -C $repo status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify local source changes.' }
$result = [ordered]@{
    schemaVersion = 1; startedUtc = [DateTime]::UtcNow.ToString('o'); completedUtc = $null
    sourceCommit = "$head"; trackedChanges = $dirty
    harnessSha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
    validatorVersion = '1.0.4'; validatorSha256 = $expectedExecutableHash
    pluginPath = $PluginPath; binarySha256 = $binaryHash; binarySha256After = $null
    strictness = $Strictness; seed = $Seed; sampleRates = @(44100,48000,96000); blockSizes = @(64,128,256,512,1024)
    arguments = $arguments; guiTestsRequired = $true; requiredTests = @(); missingTests = @()
    allocationTestExecuted = $false; externalSteinbergValidatorExecuted = $false
    exitCode = $null; passed = $false; error = $null; elapsedSeconds = 0
}
# Sanitize only this process's child options, then restore them. Inherited tool
# preferences must not silently skip GUI tests or alter the recorded matrix.
$optionNames = @('STRICTNESS_LEVEL','RANDOM_SEED','TIMEOUT_MS','VERBOSE','SKIP_GUI_TESTS','DATA_FILE','OUTPUT_DIR','OUTPUT_FILENAME','REPEAT','RANDOMISE','SAMPLE_RATES','BLOCK_SIZES','VST3VALIDATOR','DISABLED_TESTS')
$savedOptions = @{}
$clock = [Diagnostics.Stopwatch]::StartNew()
try {
    foreach ($name in $optionNames) {
        $savedOptions[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
        [Environment]::SetEnvironmentVariable($name,$null,'Process')
    }
    $process = Start-Process -FilePath $ValidatorPath -ArgumentList $quotedArguments -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    $result.exitCode = $process.ExitCode
    $text = [IO.File]::ReadAllText($stdoutPath)
    $required = @('Editor','Open editor whilst processing','Audio processing','Plugin state','Automation','Editor Automation','Automatable Parameters','Restoring default layout')
    if ($Strictness -ge 6) { $required += @('Non-releasing audio processing','Plugin state restoration','Fuzz parameters') }
    if ($Strictness -ge 7) { $required += @('Parameters','Background thread state','Parameter thread safety') }
    $result.requiredTests = $required
    $result.missingTests = @($required | Where-Object { ![regex]::IsMatch($text,('(?m)^Completed tests in pluginval / ' + [regex]::Escape($_) + '\r?$')) })
    $result.allocationTestExecuted = $text.Contains('Completed tests in pluginval / Allocations during process')
    $result.externalSteinbergValidatorExecuted = $text.Contains('Completed tests in pluginval / vst3 validator') -and !$text.Contains('Skipping vst3 validator')
    $result.binarySha256After = (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash
    $matrixComplete = $true
    foreach ($rate in $result.sampleRates) { foreach ($block in $result.blockSizes) {
        if (!$text.Contains("Testing with sample rate [$rate] and block size [$block]")) { $matrixComplete = $false }
    } }
    $result.passed = $process.ExitCode -eq 0 -and [regex]::IsMatch($text,'(?m)^SUCCESS\r?$') -and $text.Contains('Num plugins found: 1') -and $result.missingTests.Count -eq 0 -and $matrixComplete -and $result.binarySha256After -eq $binaryHash
    if (!$result.passed) { throw "VST3 validation failed or coverage was incomplete; see $runDirectory" }
    Write-Output "PASS: actual Windows VST3, pluginval 1.0.4 level $Strictness, GUI enabled, seed $Seed."
    Write-Output "Evidence: $(Join-Path $runDirectory 'result.json')"
} catch {
    $result.error = $_.Exception.Message
    throw
} finally {
    $clock.Stop()
    foreach ($name in $savedOptions.Keys) { [Environment]::SetEnvironmentVariable($name,$savedOptions[$name],'Process') }
    $result.completedUtc = [DateTime]::UtcNow.ToString('o')
    $result.elapsedSeconds = $clock.Elapsed.TotalSeconds
    [IO.File]::WriteAllText((Join-Path $runDirectory 'result.json'),($result | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
}
