$ErrorActionPreference = 'Stop'
$root = 'D:\Jorge\Documents\Codex\Aion-Looper'
$source = 'D:\Jorge\Dropbox\Codex\Rust-Looper'
$parent = 'D:\Jorge\Dropbox\Codex'
$reportPath = Join-Path $root 'verification\aion-relocation.json'
for ($attempt = 0; $attempt -lt 600; $attempt++) {
    if (Test-Path -LiteralPath $source) {
        $item = Get-Item -LiteralPath $source -Force
        if ($item.LinkTarget -or $item.FullName.TrimEnd('\') -ne $source) { throw 'Unexpected old folder.' }
        if (@(Get-ChildItem -LiteralPath $source -Force).Count) { throw 'Old folder is no longer empty; preserving it.' }
        try { [IO.Directory]::Delete($source,$false) } catch [IO.IOException] { Start-Sleep -Seconds 2; continue }
    }
    if (Test-Path -LiteralPath 'D:\Jorge\Dropbox\Codex\Ascii-Looper') { throw 'Unexpected original repository remnant.' }
    if ((Test-Path -LiteralPath $parent) -and !@(Get-ChildItem -LiteralPath $parent -Force).Count) {
        $item = Get-Item -LiteralPath $parent -Force
        if ($item.LinkTarget -or $item.FullName -ne $parent) { throw 'Unexpected parent folder.' }
        try { [IO.Directory]::Delete($parent,$false) } catch [IO.IOException] { Start-Sleep -Seconds 2; continue }
    }
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if (!$report.verified -or $report.destination -ne $root) { throw 'Missing verified move.' }
    $report.sources_removed = $true
    $report | Add-Member -NotePropertyName files_moved -NotePropertyValue $true -Force
    $report | Add-Member -NotePropertyName cleanup_completed_utc -NotePropertyValue ([DateTime]::UtcNow.ToString('o')) -Force
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath
    Write-Output 'Old empty folders removed; relocation complete.'
    exit 0
}
throw 'Windows still holds the empty old folder. Run this script after closing the old workspace.'