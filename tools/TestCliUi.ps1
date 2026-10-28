param([Parameter(Mandatory=$true)][string] $PythonPath)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$savedRoot = $env:AION_CLI_DIR
$savedPython = $env:AION_CLI_PYTHON
try {
    $env:AION_CLI_DIR = Join-Path $repo ('build/cli-ui-' + [guid]::NewGuid().ToString('N'))
    $env:AION_CLI_PYTHON = (Resolve-Path -LiteralPath $PythonPath).Path
    & (Join-Path $PSScriptRoot 'TestCountInUi.ps1')
} finally {
    $env:AION_CLI_DIR = $savedRoot
    $env:AION_CLI_PYTHON = $savedPython
}
