[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [string]$OutlineDirectory = 'ppt_doc_outlines'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$outlinePath = Join-Path $RepoRoot $OutlineDirectory
if (-not (Test-Path -LiteralPath $outlinePath -PathType Container)) {
    throw "Outline directory not found: $outlinePath"
}

$files = @(Get-ChildItem -LiteralPath $outlinePath -Recurse -File -Filter '*.md')
$pattern = '\]\((?<target>\.\./CICC技术文档与PPT收集整理/[^)#]+)(?:#[^)]*)?\)'
$missing = @()
$referenceCount = 0

foreach ($file in $files) {
    $content = Get-Content -LiteralPath $file.FullName -Raw
    foreach ($match in [regex]::Matches($content, $pattern)) {
        $referenceCount++
        $relativeTarget = [Uri]::UnescapeDataString($match.Groups['target'].Value)
        $targetPath = Join-Path $file.DirectoryName ($relativeTarget -replace '/', '\')
        if (-not (Test-Path -LiteralPath $targetPath)) {
            $missing += [pscustomobject]@{
                SourceFile = $file.FullName
                Target = $relativeTarget
                ResolvedPath = $targetPath
            }
        }
    }
}

if ($missing.Count -gt 0) {
    Write-Host "[FAIL] $($missing.Count) broken CICC reference link(s) found:" -ForegroundColor Red
    $missing | Format-Table -AutoSize | Out-String | Write-Host
    exit 1
}

Write-Host "[PASS] Checked $referenceCount CICC relative reference link(s) across $($files.Count) Markdown file(s)." -ForegroundColor Green
