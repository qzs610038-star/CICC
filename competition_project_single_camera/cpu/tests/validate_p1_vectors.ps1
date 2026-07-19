$ErrorActionPreference = 'Stop'
$vectorRoot = Join-Path $PSScriptRoot 'vectors\p1'
$vectors = Get-Content (Join-Path $vectorRoot 'p1_contract_vectors.jsonl') |
    ForEach-Object { $_ | ConvertFrom-Json }
$replay = Get-Content (Join-Path $vectorRoot 'twenty_round_arm0_replay.jsonl') |
    ForEach-Object { $_ | ConvertFrom-Json }
if ($vectors.Count -lt 10) { throw 'P1_VECTOR_COUNT_TOO_SMALL' }
if ($replay.Count -ne 20) { throw "P1_REPLAY_COUNT expected=20 actual=$($replay.Count)" }
if (($replay | Where-Object { $_.arm_enabled -ne 0 }).Count -ne 0) {
    throw 'P1_REPLAY_ARM_NOT_DISABLED'
}
foreach ($task in 1..4) {
    if (($replay | Where-Object { $_.task -eq $task }).Count -ne 5) {
        throw "P1_REPLAY_TASK_COUNT task=$task"
    }
}
if (($replay | Where-Object { $_.task -ge 3 -and $_.reason -ne 'SIZE_UNAVAILABLE' }).Count -ne 0) {
    throw 'P1_REPLAY_SIZE_EVIDENCE_OVERCLAIM'
}
Write-Host "p1_vectors: $($vectors.Count) contract vectors validated"
Write-Host 'p1_replay: 20/20 rounds validated, ARM=0'
