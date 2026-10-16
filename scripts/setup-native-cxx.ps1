# Project-local GNU C++ build tools for the vendored native stretcher.
# Downloads official MSYS2 packages and verifies their published SHA-256 values.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot '.toolchain\gnu-tools'
New-Item -ItemType Directory -Force -Path $toolsRoot | Out-Null
$packages = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'native-cxx-packages.json') -Raw | ConvertFrom-Json
$manifest = @()
foreach ($package in $packages) {
    $name=$package.name; $url=$package.url; $hash=$package.sha256
    if ($url -notmatch '^https://mirror\.msys2\.org/mingw/mingw64/[a-zA-Z0-9._+-]+\.pkg\.tar\.zst$' -or $hash -notmatch '^[a-f0-9]{64}$') { throw "Invalid pinned package metadata: $name" }
    $archive = Join-Path $toolsRoot ([IO.Path]::GetFileName($url))
    if (!(Test-Path -LiteralPath $archive) -or (Get-FileHash -LiteralPath $archive).Hash -ne $hash) {
        Invoke-WebRequest -Uri $url -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive).Hash -ne $hash) { throw "Checksum mismatch: $name" }
    & tar -xf $archive -C $toolsRoot
    if ($LASTEXITCODE -ne 0) { throw "Cannot extract $name" }
    $manifest += [pscustomobject]@{name=$name;url=$url;sha256=$hash}
    Write-Output "Installed project-local $name"
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $toolsRoot 'cxx-packages.json')
& (Join-Path $toolsRoot 'mingw64\bin\g++.exe') --version
exit $LASTEXITCODE
