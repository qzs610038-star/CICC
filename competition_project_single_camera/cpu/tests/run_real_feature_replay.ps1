param([Parameter(Mandatory=$true)][string]$Batch)

$ErrorActionPreference = 'Stop'
$required = @('capture_profile.json','sample_manifest.jsonl','feature_rows.jsonl','data_quality_summary.json','sha256sums.json')
if (-not (Test-Path -LiteralPath $Batch -PathType Container)) {
    Write-Output "WAITING_FOR_QZS_BATCH: directory not found: $Batch"
    exit 2
}
foreach ($name in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $Batch $name) -PathType Leaf)) {
        Write-Output "QW_CALIBRATION_BATCH_INCOMPLETE: $name"
        exit 2
    }
}
Write-Output 'QW_CALIBRATION_REPLAY_BLOCKED: fixed qzs batch parser is not activated until a published batch/hash is available'
exit 2
