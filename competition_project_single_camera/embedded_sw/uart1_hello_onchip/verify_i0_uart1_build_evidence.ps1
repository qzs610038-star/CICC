[CmdletBinding()]
param(
    [string]$EvidenceRoot = 'D:\cicc_cbm_link\competition_project_single_camera\outflow_i0_uart1_20260719_final',
    [string]$WorkRoot = 'D:\cicc_cbm_link\competition_project_single_camera\work_i0_uart1_20260719_final'
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifestPath = Join-Path $PSScriptRoot 'I0_UART1_BUILD_MANIFEST.json'
$inputManifestPath = Join-Path $PSScriptRoot 'I0_UART1_BUILD_INPUTS.sha256'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json

if ((git -C $projectRoot rev-parse $manifest.design_commit).Trim() -ne $manifest.design_commit) {
    throw "Design commit is unavailable: $($manifest.design_commit)"
}

$verified = 0
foreach ($artifact in $manifest.artifacts) {
    $path = if ($artifact.name -eq 'work/mem_test.lbf') {
        Join-Path $WorkRoot 'mem_test.lbf'
    } elseif ($artifact.name -eq 'uart1_hello_onchip.elf') {
        Join-Path $projectRoot $artifact.path
    } else {
        Join-Path $EvidenceRoot $artifact.name
    }

    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing artifact: $path"
    }

    $item = Get-Item -LiteralPath $path
    if ($item.Length -ne [int64]$artifact.size) {
        throw "Size mismatch for $($artifact.name): expected $($artifact.size), got $($item.Length)"
    }

    $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($actualHash -ne $artifact.sha256) {
        throw "SHA-256 mismatch for $($artifact.name): expected $($artifact.sha256), got $actualHash"
    }

    $verified++
}

$inputLines = Get-Content -LiteralPath $inputManifestPath | Where-Object { $_.Trim() -ne '' }
foreach ($line in $inputLines) {
    if ($line -notmatch '^(?<hash>[0-9A-F]{64})  (?<path>.+)$') {
        throw "Invalid input-manifest line: $line"
    }

    $inputPath = Join-Path $projectRoot $Matches.path
    if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf)) {
        throw "Missing atomic input: $inputPath"
    }

    $actualHash = (Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash
    if ($actualHash -ne $Matches.hash) {
        throw "Atomic input SHA-256 mismatch: $($Matches.path)"
    }
}

"I0_UART1_BUILD_EVIDENCE=PASS batch=$($manifest.batch_id) commit=$($manifest.design_commit) artifacts=$verified atomic_inputs=$($inputLines.Count)"
