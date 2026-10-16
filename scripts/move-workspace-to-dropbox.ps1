# One-time relocation requested by the project owner. Fixed paths are intentional.
param([switch]$DeleteOriginal,[switch]$Resume)
$ErrorActionPreference = 'Stop'
$sourceRoot = 'C:\Users\jorge.salgueiro\Documents\Codex\Rust-Looper'
$destinationRoot = 'C:\Users\jorge.salgueiro\Dropbox\Codex\Rust-Looper'
$comparison = [StringComparison]::OrdinalIgnoreCase

function Get-WorkspaceCatalog([string]$Root) {
    $files = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    $directories = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $links = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    $pending = [Collections.Generic.Stack[IO.DirectoryInfo]]::new()
    $pending.Push([IO.DirectoryInfo]::new($Root))
    while ($pending.Count) {
        $directory = $pending.Pop()
        foreach ($entry in $directory.GetFileSystemInfos()) {
            $relative = [IO.Path]::GetRelativePath($Root, $entry.FullName)
            if ($entry.LinkTarget) {
                # Preserve links without traversing their external target.
                $links.Add($relative, $entry.LinkTarget)
            } elseif ($entry -is [IO.DirectoryInfo]) {
                [void]$directories.Add($relative)
                $pending.Push($entry)
            } else {
                $files.Add($relative, @{ Length=$entry.Length; Modified=$entry.LastWriteTimeUtc.Ticks })
            }
        }
    }
    return @{ Files=$files; Directories=$directories; Links=$links }
}
function Assert-CatalogMatches($Expected, $Actual, [switch]$CheckModified) {
    if ($Expected.Files.Count -ne $Actual.Files.Count -or
        $Expected.Directories.Count -ne $Actual.Directories.Count -or
        $Expected.Links.Count -ne $Actual.Links.Count) {
        throw 'Workspace entry counts differ; the original will not be deleted.'
    }
    foreach ($path in $Expected.Directories) {
        if (!$Actual.Directories.Contains($path)) { throw "Missing directory: $path" }
    }
    foreach ($pair in $Expected.Files.GetEnumerator()) {
        if (!$Actual.Files.ContainsKey($pair.Key) -or $Actual.Files[$pair.Key].Length -ne $pair.Value.Length) {
            throw "Missing or differently sized file: $($pair.Key)"
        }
        if ($CheckModified -and $Actual.Files[$pair.Key].Modified -ne $pair.Value.Modified) {
            throw "File changed during relocation: $($pair.Key)"
        }
    }
    foreach ($pair in $Expected.Links.GetEnumerator()) {
        if (!$Actual.Links.ContainsKey($pair.Key) -or $Actual.Links[$pair.Key] -ne $pair.Value) {
            throw "Link target differs: $($pair.Key)"
        }
    }
}
function Get-SharedFileHash([string]$Path) {
    $stream=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $hasher=[Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($hasher.ComputeHash($stream)) }
    finally { $hasher.Dispose();$stream.Dispose() }
}

$sourceRoot = [IO.Path]::GetFullPath($sourceRoot).TrimEnd('\')
$destinationRoot = [IO.Path]::GetFullPath($destinationRoot).TrimEnd('\')
if ($sourceRoot -ne 'C:\Users\jorge.salgueiro\Documents\Codex\Rust-Looper' -or
    $destinationRoot -ne 'C:\Users\jorge.salgueiro\Dropbox\Codex\Rust-Looper') {
    throw 'Unexpected relocation paths.'
}
$sourceItem = Get-Item -LiteralPath $sourceRoot -Force
if (!$sourceItem.PSIsContainer -or ($sourceItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'The source must be the actual project directory, not a reparse point.'
}
if ([IO.DirectoryInfo]::new($destinationRoot).LinkTarget) { throw 'Destination must not redirect to another directory.' }
if (!$Resume -and @(Get-ChildItem -LiteralPath $destinationRoot -Force).Count) { throw 'Destination is not empty; refusing to overwrite existing files.' }
Set-Location -LiteralPath (Split-Path -Parent $sourceRoot)
$catalog = Get-WorkspaceCatalog $sourceRoot
if ($Resume) {
    # The owner saved/closed this workbook after the initial copy. Its copied
    # Excel ownership file is stale and is not project content.
    $staleLock='outputs\alenquer-estimativa\~$4candy_estimativa_Alenquer_20pct.xlsx'
    if (!(Test-Path -LiteralPath (Join-Path $sourceRoot $staleLock)) -and (Test-Path -LiteralPath (Join-Path $destinationRoot $staleLock))) {
        Remove-Item -LiteralPath (Join-Path $destinationRoot $staleLock) -Force
    }
    $existing=Get-WorkspaceCatalog $destinationRoot
    foreach ($pair in $existing.Files.GetEnumerator()) {
        if (!$catalog.Files.ContainsKey($pair.Key)) { throw "Unexpected destination-only file: $($pair.Key)" }
        if ($pair.Value.Modified -gt $catalog.Files[$pair.Key].Modified -and
            (Get-SharedFileHash (Join-Path $sourceRoot $pair.Key)) -ne (Get-SharedFileHash (Join-Path $destinationRoot $pair.Key))) {
            throw "Destination has a newer different file; preserving both copies: $($pair.Key)"
        }
    }
}
$bytes = 0L
foreach ($file in $catalog.Files.Values) { $bytes += $file.Length }
if ((Get-PSDrive -Name C).Free -lt $bytes + 1GB) { throw 'Insufficient free space for verified copy before deletion.' }
$logRoot = Join-Path ([IO.Path]::GetTempPath()) ('RustLooper-Relocation-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $logRoot | Out-Null
$copyLog = Join-Path $logRoot 'robocopy.log'
Write-Output "Copying $($catalog.Files.Count) files ($bytes bytes), $($catalog.Links.Count) links. Log: $copyLog"
& robocopy.exe $sourceRoot $destinationRoot /E /COPY:DAT /DCOPY:DAT /SJ /SL /XO /R:1 /W:1 /NP /NFL /NDL "/LOG:$copyLog"
$copyExit = $LASTEXITCODE
if ($copyExit -ge 8) { Get-Content -LiteralPath $copyLog -Tail 25; throw "Copy failed ($copyExit); original retained." }
Assert-CatalogMatches $catalog (Get-WorkspaceCatalog $destinationRoot)
Write-Output 'File/directory counts and link targets match. Verifying every regular file with SHA-256.'
$verified = 0
foreach ($relative in $catalog.Files.Keys) {
    $sourceHash = Get-SharedFileHash (Join-Path $sourceRoot $relative)
    $destinationHash = Get-SharedFileHash (Join-Path $destinationRoot $relative)
    if ($sourceHash -ne $destinationHash) { throw "Hash mismatch: $relative. Original retained." }
    $verified++
    if ($verified % 2500 -eq 0) { Write-Output "SHA-256 verified $verified / $($catalog.Files.Count) files." }
}
Assert-CatalogMatches $catalog (Get-WorkspaceCatalog $sourceRoot) -CheckModified
Assert-CatalogMatches $catalog (Get-WorkspaceCatalog $destinationRoot)
$gitRoot = & git -C $destinationRoot rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or ![IO.Path]::GetFullPath($gitRoot).Equals($destinationRoot, $comparison)) {
    throw 'Copied Git repository validation failed; original retained.'
}
$remote = & git -C $destinationRoot remote get-url origin
if ($LASTEXITCODE -ne 0) { throw 'Copied Git remote validation failed; original retained.' }
Write-Output "Verified $verified files. Git origin: $remote"
$report = [ordered]@{
    source=$sourceRoot; destination=$destinationRoot; verified_files=$verified
    bytes=$bytes; preserved_links=$catalog.Links.Count; sha256_verified=$true
    git_origin=$remote; original_deleted=$false; copy_log=$copyLog
}
if ($DeleteOriginal) {
    # Revalidate the exact named directory immediately before recursive deletion.
    $deleteTarget = (Get-Item -LiteralPath $sourceRoot -Force).FullName.TrimEnd('\')
    if (!$deleteTarget.Equals('C:\Users\jorge.salgueiro\Documents\Codex\Rust-Looper', $comparison) -or
        !$deleteTarget.StartsWith('C:\Users\jorge.salgueiro\Documents\Codex\', $comparison) -or
        (Get-Item -LiteralPath $deleteTarget -Force).LinkTarget) { throw 'Unsafe deletion target.' }
    # Remove junction objects non-recursively, never their cache targets.
    foreach ($relative in $catalog.Links.Keys) {
        $linkPath = [IO.Path]::GetFullPath((Join-Path $deleteTarget $relative))
        if (!$linkPath.StartsWith($deleteTarget+'\', $comparison)) { throw 'Link outside source root.' }
        $link = Get-Item -LiteralPath $linkPath -Force
        if (!$link.LinkTarget) { throw "Expected link changed: $relative" }
        if ($link.PSIsContainer) { [IO.Directory]::Delete($linkPath, $false) }
        else { Remove-Item -LiteralPath $linkPath -Force }
    }
    Remove-Item -LiteralPath $deleteTarget -Recurse -Force
    if (Test-Path -LiteralPath $sourceRoot) { throw 'Original directory still exists.' }
    $report.original_deleted=$true
    Write-Output 'Original directory removed after complete verification.'
}
$reportPath = Join-Path $destinationRoot 'verification\workspace-relocation.json'
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath
$report | ConvertTo-Json -Depth 4
