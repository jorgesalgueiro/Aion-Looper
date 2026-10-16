# Optional project-local assembler tools needed by windows-rs with GNU Rust.
# No system installation or persistent PATH changes.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolRoot = Join-Path $projectRoot '.toolchain\gnu-tools'
New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null
foreach ($packageName in @('binutils', 'gettext-runtime', 'libwinpthread', 'zlib', 'zstd', 'libiconv')) {
    $page = (Invoke-WebRequest -Uri "https://packages.msys2.org/packages/mingw-w64-x86_64-$packageName").Content
    $archiveUrl = [regex]::Match($page, "https://mirror\.msys2\.org/mingw/mingw64/mingw-w64-x86_64-$packageName-[^`"<>\s]+\.pkg\.tar\.zst").Value
    $expectedHash = [regex]::Match($page, '(?s)SHA256:.*?<code>\s*([a-f0-9]{64})\s*</code>').Groups[1].Value
    if (!$archiveUrl -or !$expectedHash) { throw "Missing package metadata for $packageName." }
    $archive = Join-Path $toolRoot "$packageName.pkg.tar.zst"
    Invoke-WebRequest -Uri $archiveUrl -OutFile $archive
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) { throw "Checksum mismatch: $packageName" }
    $entries = & tar -tf $archive
    if ($LASTEXITCODE -ne 0 -or @($entries | Where-Object { $_ -match '(^[/\\]|(^|[/\\])\.\.([/\\]|$)|^[A-Za-z]:)' }).Count -gt 0) {
        throw "Invalid archive paths: $packageName"
    }
    & tar -xf $archive -C $toolRoot
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $packageName" }
    Write-Output "Installed project-local $packageName (SHA-256 checked)."
}
