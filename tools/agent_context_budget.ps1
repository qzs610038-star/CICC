# Agent context budget check. PowerShell 5.1 compatible; read-only.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (& git rev-parse --show-toplevel).Trim()
if (-not $root) { Write-Error 'Not inside a Git repository'; exit 2 }
Set-Location -LiteralPath $root
$manifest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'maintenance_manifest.json') | ConvertFrom-Json
$fail = New-Object 'System.Collections.Generic.List[string]'
$warn = New-Object 'System.Collections.Generic.List[string]'

function Get-TokenCount([string]$Path) {
    $code = "import sys,tiktoken;print(len(tiktoken.get_encoding('o200k_base').encode(open(sys.argv[1],encoding='utf-8-sig').read())))"
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $value = & python -c $code $Path 2>$null
    $pythonExit = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($pythonExit -eq 0 -and "$value" -match '^\d+$') { return [int64]$value }
    return $null
}

$rows = @()
foreach ($entry in $manifest.context_files) {
    $path = Join-Path $root ([string]$entry.file)
    if (-not (Test-Path -LiteralPath $path)) { $fail.Add("missing context file: $($entry.file)"); continue }
    $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
    $bytes = (Get-Item -LiteralPath $path).Length
    $lines = (Get-Content -Encoding UTF8 -LiteralPath $path).Count
    $tokens = Get-TokenCount $path
    $metric = if ($null -eq $tokens) { [math]::Ceiling($bytes / 3.0) } else { $tokens }
    if ($null -eq $tokens) { $warn.Add("tiktoken unavailable; byte/3 estimate used: $($entry.file)") }
    if ($bytes -gt [int64]$entry.max_bytes) { $fail.Add("bytes over budget: $($entry.file) $bytes > $($entry.max_bytes)") }
    if ($metric -gt [int64]$entry.max_tokens) { $fail.Add("tokens over budget: $($entry.file) $metric > $($entry.max_tokens)") }
    foreach ($marker in @($entry.required_markers)) {
        if ($text.IndexOf([string]$marker, [System.StringComparison]::Ordinal) -lt 0) { $fail.Add("required marker missing: $($entry.file) -> $marker") }
    }
    $rows += [pscustomobject]@{File=$entry.file;Bytes=$bytes;Lines=$lines;Tokens=$metric;MaxBytes=$entry.max_bytes;MaxTokens=$entry.max_tokens}
}

$rows | Format-Table -AutoSize
$threeTokens = ($rows | Measure-Object Tokens -Sum).Sum
$chain = @($manifest.required_reading_chain)
$fiveTokens = 0
foreach ($relative in $chain) {
    $path = Join-Path $root $relative
    $count = Get-TokenCount $path
    if ($null -eq $count) { $count = [math]::Ceiling((Get-Item -LiteralPath $path).Length / 3.0) }
    $fiveTokens += $count
}
$baseline = [double]$manifest.context_baseline_tokens
$reduction = if ($baseline -gt 0) { [math]::Round((1.0 - ($fiveTokens / $baseline)) * 100.0, 2) } else { 0 }
Write-Host "THREE_ENTRY_TOKENS=$threeTokens"
Write-Host "FIVE_FILE_CHAIN_TOKENS=$fiveTokens"
Write-Host "BASELINE_TOKENS=$([int64]$baseline)"
Write-Host "REDUCTION_PERCENT=$reduction"
if ($threeTokens -gt 11500) { $fail.Add("three-entry total over budget: $threeTokens > 11500") }
if ($fiveTokens -gt 27500) { $fail.Add("five-file chain over budget: $fiveTokens > 27500") }
if ($reduction -lt 55) { $fail.Add("reduction below target: $reduction% < 55%") }

foreach ($item in $warn) { Write-Host "WARN: $item" -ForegroundColor Yellow }
foreach ($item in $fail) { Write-Host "FAIL: $item" -ForegroundColor Red }
if ($fail.Count -gt 0) { Write-Host "CONTEXT_BUDGET=FAIL"; exit 1 }
if ($warn.Count -gt 0) { Write-Host "CONTEXT_BUDGET=WARN"; exit 0 }
Write-Host "CONTEXT_BUDGET=PASS" -ForegroundColor Green
exit 0
