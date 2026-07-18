[CmdletBinding()]
param(
    [switch]$PrintCurrent
)

$ErrorActionPreference = 'Stop'

function Get-TextSha256 {
    param([Parameter(Mandatory = $true)][string]$Text)

    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($Text)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '')
    } finally {
        $sha.Dispose()
    }
}

function Get-FeatureTapSurfaceSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $content = [System.IO.File]::ReadAllText($Path)
    $header = [regex]::Match(
        $content,
        '(?ms)^module\s+feature_stats_tap\b.*?^\s*\);'
    )
    if (-not $header.Success) {
        throw "INTERFACE_FREEZE_FAIL: feature_stats_tap module header not found: $Path"
    }

    $flagPattern = '(?m)^\s*localparam\s+\[7:0\]\s+FLAG_[A-Z0-9_]+\s*=\s*8' +
        [char]39 + 'h[0-9A-Fa-f]+;\s*$'
    $flags = [regex]::Matches($content, $flagPattern)
    if ($flags.Count -ne 7) {
        throw "INTERFACE_FREEZE_FAIL: expected 7 source flag constants, found $($flags.Count): $Path"
    }

    $normalizedHeader = $header.Value -replace "`r`n", "`n"
    $normalizedFlags = @($flags | ForEach-Object { $_.Value.Trim() }) -join "`n"
    return Get-TextSha256 -Text ($normalizedHeader + "`n" + $normalizedFlags + "`n")
}

$repoRoot = (& git rev-parse --show-toplevel).Trim()
if ([string]::IsNullOrWhiteSpace($repoRoot)) {
    throw 'INTERFACE_FREEZE_FAIL: not inside a Git repository'
}

$manifestPath = Join-Path $repoRoot 'competition_project_single_camera/integration/interface_freeze_manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "INTERFACE_FREEZE_FAIL: manifest missing: $manifestPath"
}

$manifestJson = [System.IO.File]::ReadAllText($manifestPath, [System.Text.UTF8Encoding]::new($false))
$manifest = $manifestJson | ConvertFrom-Json
$approvalBytes = [System.Text.UTF8Encoding]::new($false).GetBytes([string]$manifest.approval_phrase)
$approvalBase64 = [System.Convert]::ToBase64String($approvalBytes)
$requiredApprovalBase64 = '56Gu6K6k5o6l5Y+j5paH5Lu25L+u5pS577yM5bey57uP5ZKMd3Nj44CBbGliYW94dW7jgIFxenPmsp/pgJrjgII='
if ($approvalBase64 -cne $requiredApprovalBase64) {
    throw 'INTERFACE_FREEZE_FAIL: approval phrase in manifest is missing or changed'
}
if ($manifest.i0.soc_uart -cne 'UART1' -or
    $manifest.i0.board_route -cne 'Type-C UART1' -or
    $manifest.i0.rx -cne 'GPIOR_96/B12' -or
    $manifest.i0.tx -cne 'GPIOR_100/D12' -or
    [int]$manifest.i0.baud -ne 115200) {
    throw 'INTERFACE_FREEZE_FAIL: frozen I0 route does not match the confirmed UART1 contract'
}

$failures = New-Object 'System.Collections.Generic.List[string]'
$current = New-Object 'System.Collections.Generic.List[object]'

foreach ($entry in @($manifest.files)) {
    $relativePath = [string]$entry.path
    $absolutePath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        $failures.Add("missing file: $relativePath")
        continue
    }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $absolutePath).Hash
    $current.Add([pscustomobject]@{ path = $relativePath; sha256 = $actual })
    if ($actual -cne ([string]$entry.sha256).ToUpperInvariant()) {
        $failures.Add("hash mismatch: $relativePath expected=$($entry.sha256) actual=$actual")
    }
}

foreach ($entry in @($manifest.surfaces)) {
    $relativePath = [string]$entry.path
    $absolutePath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        $failures.Add("missing interface surface: $relativePath")
        continue
    }
    if ($entry.kind -cne 'verilog_module_header_and_flag_constants') {
        $failures.Add("unsupported surface kind: $($entry.kind)")
        continue
    }
    $actual = Get-FeatureTapSurfaceSha256 -Path $absolutePath
    $current.Add([pscustomobject]@{ path = $relativePath; kind = $entry.kind; sha256 = $actual })
    if ($actual -cne ([string]$entry.sha256).ToUpperInvariant()) {
        $failures.Add("surface hash mismatch: $relativePath expected=$($entry.sha256) actual=$actual")
    }
}

if ($PrintCurrent) {
    $current | ConvertTo-Json -Depth 4
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) { Write-Host "FAIL: $failure" }
    Write-Host "INTERFACE_FREEZE=FAIL failures=$($failures.Count)"
    exit 1
}

Write-Host "INTERFACE_FREEZE=PASS files=$(@($manifest.files).Count) surfaces=$(@($manifest.surfaces).Count) route=UART1_TYPEC"
exit 0
