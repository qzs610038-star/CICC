param(
    [Parameter(Mandatory = $true)][string]$BundleDir
)

$ErrorActionPreference = 'Stop'
$verifier = Join-Path $PSScriptRoot 'verify_p1_replay_manifest.ps1'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('cicc-p1-manifest-negative-' + [guid]::NewGuid().ToString('N'))
$tempBundle = Join-Path $tempRoot 'bundle'

try {
    New-Item -ItemType Directory -Force -Path $tempBundle | Out-Null
    foreach ($name in @('manifest.json','rounds.jsonl','runner.txt','compile.txt','tamper.jsonl')) {
        Copy-Item -LiteralPath (Join-Path $BundleDir $name) -Destination $tempBundle -Force
    }
    & $verifier -BundleDir $tempBundle

    $tamperPath = Join-Path $tempBundle 'rounds.jsonl'
    [IO.File]::AppendAllText($tamperPath, 'TAMPER', [Text.Encoding]::ASCII)
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $negativeOutput = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $verifier -BundleDir $tempBundle 2>&1)
        $negativeExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedPreference
    }
    $negativeText = ($negativeOutput | ForEach-Object { $_.ToString() }) -join "`n"
    if ($negativeExit -ne 1 -or $negativeText -notlike '*P1_MANIFEST_FILE_HASH file=rounds.jsonl*') {
        throw "P1_MANIFEST_TAMPER_NOT_DETECTED exit=$negativeExit output=$negativeText"
    }
    Write-Host 'P1_MANIFEST_TAMPER_NEGATIVE=PASS file=rounds.jsonl expected_exit=1'
} finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
