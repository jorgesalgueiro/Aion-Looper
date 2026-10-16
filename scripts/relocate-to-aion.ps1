# Owner-requested relocation. Copy and verify first; cleanup is a separate invocation.
param([switch]$RemoveVerifiedSources,[switch]$Resume,[switch]$VerifyOnly)
$ErrorActionPreference = 'Stop'
$destination = 'D:\Jorge\Documents\Codex\Aion-Looper'
$sourceParent = 'D:\Jorge\Dropbox\Codex'
$reportPath = Join-Path $destination 'verification\aion-relocation.json'
$comparison = [StringComparison]::OrdinalIgnoreCase
$roots = @(
    @{ Source=(Join-Path $sourceParent 'Rust-Looper'); Destination=$destination },
    @{ Source=(Join-Path $sourceParent 'Ascii-Looper'); Destination=(Join-Path $destination 'reference\Ascii-Looper') }
)
function Catalog([string]$root) {
    $files = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    $dirs = [Collections.Generic.List[string]]::new()
    $links = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    $pending = [Collections.Generic.Stack[IO.DirectoryInfo]]::new()
    $pending.Push([IO.DirectoryInfo]::new($root))
    while ($pending.Count) {
        foreach ($entry in $pending.Pop().GetFileSystemInfos()) {
            $relative = [IO.Path]::GetRelativePath($root,$entry.FullName)
            if ($entry.LinkTarget) { $links.Add($relative,$entry.LinkTarget) }
            elseif ($entry -is [IO.DirectoryInfo]) { $dirs.Add($relative); $pending.Push($entry) }
            else { $files.Add($relative,@{Length=$entry.Length;Modified=$entry.LastWriteTimeUtc.Ticks}) }
        }
    }
    return @{Files=$files;Directories=$dirs;Links=$links}
}
function Hash([string]$path) {
    $stream = [IO.FileStream]::new($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash($stream)) }
    finally { $sha.Dispose(); $stream.Dispose() }
}
function Assert-RealDirectory([string]$path) {
    $item = Get-Item -LiteralPath $path -Force
    if (!$item.PSIsContainer -or $item.LinkTarget -or !$item.FullName.TrimEnd('\').Equals($path,$comparison)) {
        throw "Unexpected or redirected directory: $path"
    }
}
if ($RemoveVerifiedSources) {
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if (!$report.verified -or $report.destination -ne $destination -or $report.roots.Count -ne 2) { throw 'Missing verified relocation report.' }
    # The manifest records project SHA-256 comparisons and cache metadata. Confirm every
    # source and destination is still unchanged before deleting either source.
    foreach ($root in $roots) {
        Assert-RealDirectory $root.Source
        Assert-RealDirectory $root.Destination
        $saved = @($report.roots | Where-Object { $_.source -eq $root.Source -and $_.destination -eq $root.Destination })
        if ($saved.Count -ne 1) { throw 'Unexpected manifest root.' }
        $current = Catalog $root.Source
        if ($current.Files.Count -ne $saved[0].files.Count -or $current.Directories.Count -ne $saved[0].directories.Count -or $current.Links.Count -ne $saved[0].links.Count) {
            throw "Source changed: $($root.Source)"
        }
        foreach ($file in $saved[0].files) {
            if (!$current.Files.ContainsKey($file.path)) { throw "Source changed: $($file.path)" }
            foreach ($base in @($root.Source,$root.Destination)) {
                $path = [IO.Path]::GetFullPath((Join-Path $base $file.path))
                if (!$path.StartsWith($base+'\',$comparison)) { throw "Unsafe file: $path" }
                $item = Get-Item -LiteralPath $path -Force
                if ($item.LinkTarget -or $item.Length -ne $file.length -or $item.LastWriteTimeUtc.Ticks -ne $file.modified) { throw "Verified file changed: $path" }
            }
        }
        foreach ($dir in $saved[0].directories) {
            if (!(Test-Path -LiteralPath (Join-Path $root.Destination $dir) -PathType Container)) { throw "Missing destination directory: $dir" }
        }
        foreach ($link in $saved[0].links) {
            foreach ($base in @($root.Source,$root.Destination)) {
                if ((Get-Item -LiteralPath (Join-Path $base $link.path) -Force).LinkTarget -ne $link.target) { throw 'Changed link.' }
            }
        }
        Write-Output "Reverified $($saved[0].files.Count) files: $($root.Source)"
    }
    # Hashing a large workspace takes time. Recheck both source catalogs just
    # before cleanup so edits made during verification cannot be discarded.
    foreach ($root in $roots) {
        $saved = @($report.roots | Where-Object source -EQ $root.Source)[0]
        $current = Catalog $root.Source
        if ($current.Files.Count -ne $saved.files.Count -or $current.Directories.Count -ne $saved.directories.Count -or $current.Links.Count -ne $saved.links.Count) { throw 'Source changed during verification.' }
        foreach ($file in $saved.files) {
            if (!$current.Files.ContainsKey($file.path) -or $current.Files[$file.path].Length -ne $file.length -or $current.Files[$file.path].Modified -ne $file.modified) { throw "Source changed during verification: $($file.path)" }
        }
    }
    Set-Location -LiteralPath $destination
    foreach ($root in $roots) {
        $resolved = (Get-Item -LiteralPath $root.Source -Force).FullName.TrimEnd('\')
        if ($resolved -notin @('D:\Jorge\Dropbox\Codex\Rust-Looper','D:\Jorge\Dropbox\Codex\Ascii-Looper') -or
            !$resolved.StartsWith($sourceParent+'\',$comparison)) { throw 'Unsafe deletion target.' }
        Assert-RealDirectory $resolved
        $saved = @($report.roots | Where-Object source -EQ $resolved)[0]
        foreach ($link in $saved.links) {
            $linkPath = [IO.Path]::GetFullPath((Join-Path $resolved $link.path))
            if (!$linkPath.StartsWith($resolved+'\',$comparison)) { throw 'Unsafe link path.' }
            $item = Get-Item -LiteralPath $linkPath -Force
            if (!$item.LinkTarget) { throw 'Expected link changed.' }
            if ($item.PSIsContainer) { [IO.Directory]::Delete($linkPath,$false) }
            else { Remove-Item -LiteralPath $linkPath -Force }
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
        if (Test-Path -LiteralPath $resolved) { throw "Source not fully removed: $resolved" }
        Write-Output "Removed verified source: $resolved"
    }
    $report.sources_removed = $true
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath
    exit 0
}
Assert-RealDirectory $sourceParent
if (Test-Path -LiteralPath $destination) {
    Assert-RealDirectory $destination
    if (!$Resume -and !$VerifyOnly -and @(Get-ChildItem -LiteralPath $destination -Force).Count) { throw 'Destination must be empty; use -Resume for a previously interrupted copy.' }
} else { New-Item -ItemType Directory -Path $destination | Out-Null }
$logRoot = Join-Path ([IO.Path]::GetTempPath()) ('Aion-Relocation-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $logRoot | Out-Null
$records = @()
$copyFailures = [Collections.Generic.List[string]]::new()
foreach ($root in $roots) {
    Assert-RealDirectory $root.Source
    $catalog = Catalog $root.Source
    $bytes = 0L; foreach ($file in $catalog.Files.Values) { $bytes += $file.Length }
    if ((Get-PSDrive D).Free -lt $bytes + 1GB) { throw 'Insufficient space for a verified copy.' }
    Write-Output "Copying $($catalog.Files.Count) files ($bytes bytes): $($root.Source)"
    if ($Resume -and !$VerifyOnly -and (Test-Path -LiteralPath $root.Destination)) {
        foreach ($pair in $catalog.Files.GetEnumerator()) {
            # Interrupted build/cache copies can be replaced. Preserve any newer
            # differing source, document or Git metadata for manual resolution.
            if ($pair.Key -match '^(\.toolchain|target|verification|dist|\.reference)[\\/]') { continue }
            $existingPath = Join-Path $root.Destination $pair.Key
            if (Test-Path -LiteralPath $existingPath -PathType Leaf) {
                $existing = Get-Item -LiteralPath $existingPath -Force
                if ($existing.LastWriteTimeUtc.Ticks -gt $pair.Value.Modified -and (Hash $existingPath) -ne (Hash (Join-Path $root.Source $pair.Key))) {
                    throw "Newer different destination file preserved; resolve before resuming: $existingPath"
                }
            }
        }
    }
    $copyLog = Join-Path $logRoot ((Split-Path $root.Source -Leaf)+'.log')
    if (!$VerifyOnly) {
        & robocopy.exe $root.Source $root.Destination /E /COPY:DAT /DCOPY:DAT /SJ /SL /NOOFFLOAD /MT:64 /R:0 /W:0 /NP /NFL /NDL "/LOG:$copyLog"
        if ($LASTEXITCODE -ge 8) {
            $copyFailures.Add($copyLog)
            Write-Output "Some files could not be copied; original retained. Log: $copyLog"
            continue
        }
    } else {
        $copyLog = $null
        Write-Output 'Verifying existing copy with SHA-256.'
    }
    $actual = Catalog $root.Destination
    # A resumed destination may also contain the separately preserved reference
    # repository or user-created files. Verify every source entry without removing extras.
    $verifiedFiles = [Collections.Generic.List[object]]::new(); $verifiedLinks = @(); $count = 0
    foreach ($pair in $catalog.Files.GetEnumerator()) {
        if (!$actual.Files.ContainsKey($pair.Key) -or $actual.Files[$pair.Key].Length -ne $pair.Value.Length) { throw "Missing or differently sized file: $($pair.Key)" }
        if ($actual.Files[$pair.Key].Modified -ne $pair.Value.Modified) { throw "Different copied timestamp: $($pair.Key)" }
    }
    $sourceBase = $root.Source; $destinationBase = $root.Destination
    $entries = foreach ($pair in $catalog.Files.GetEnumerator()) {
        $cache = $pair.Key -match '^(target|\.toolchain)[\\/]'
        $settings = $pair.Key -match '^\.toolchain[\\/](cargo[\\/](config(\.toml)?|credentials(\.toml)?)|rustup[\\/]settings\.toml|gnu-tools[\\/]cxx-packages\.json)$'
        if ($cache -and !$settings) {
            $verifiedFiles.Add(@{path=$pair.Key;length=$pair.Value.Length;modified=$pair.Value.Modified;verification='size+timestamp';sha256=$null})
            $count++
        } else {
            [pscustomobject]@{path=$pair.Key;length=$pair.Value.Length;modified=$pair.Value.Modified}
        }
    }
    Write-Output "Validated $count rebuildable cache files by size and timestamp; SHA-256 checking $($entries.Count) project/settings files."
    $entries | ForEach-Object -Parallel {
        $entry = $_
        function FileHash([string]$path) {
            $stream = [IO.FileStream]::new($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
            $sha = [Security.Cryptography.SHA256]::Create()
            try { return [Convert]::ToHexString($sha.ComputeHash($stream)) }
            finally { $sha.Dispose(); $stream.Dispose() }
        }
        try {
            $hash = FileHash (Join-Path $using:sourceBase $entry.path)
            if ((FileHash (Join-Path $using:destinationBase $entry.path)) -ne $hash) { throw "Hash mismatch: $($entry.path)" }
            [pscustomobject]@{ok=$true;record=@{path=$entry.path;length=$entry.length;modified=$entry.modified;verification='SHA-256';sha256=$hash}}
        } catch { [pscustomobject]@{ok=$false;error=$_.Exception.Message} }
    } -ThrottleLimit 8 | ForEach-Object {
        if (!$_.ok) { throw $_.error }
        $verifiedFiles.Add($_.record)
        $count++; if ($count % 2500 -eq 0) { Write-Output "Verified $count / $($catalog.Files.Count) files" }
    }
    foreach ($dir in $catalog.Directories) { if (!$actual.Directories.Contains($dir)) { throw "Missing directory: $dir" } }
    foreach ($pair in $catalog.Links.GetEnumerator()) {
        if (!$actual.Links.ContainsKey($pair.Key) -or $actual.Links[$pair.Key] -ne $pair.Value) { throw 'Link mismatch.' }
        $verifiedLinks += @{path=$pair.Key;target=$pair.Value}
    }
    & git -C $root.Destination rev-parse --show-toplevel
    if ($LASTEXITCODE -ne 0) { throw 'Copied Git repository is not readable.' }
    $records += @{source=$root.Source;destination=$root.Destination;files=$verifiedFiles;directories=@($catalog.Directories);links=$verifiedLinks;bytes=$bytes;copy_log=$copyLog}
    Write-Output "Verified all $count files: $($root.Destination)"
}
if ($copyFailures.Count) { throw "Relocation is incomplete; no originals removed. Copy logs: $($copyFailures -join ', ')" }
New-Item -ItemType Directory -Force -Path (Split-Path $reportPath -Parent) | Out-Null
@{verified=$true;sources_removed=$false;destination=$destination;roots=$records} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath
Write-Output "Verified copy complete. Originals retained for separate cleanup. Report: $reportPath"
