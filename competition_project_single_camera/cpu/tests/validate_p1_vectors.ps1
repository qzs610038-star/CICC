param(
    [Parameter(Mandatory = $true)][string]$SchemaPath,
    [Parameter(Mandatory = $true)][string]$ReplayPath
)

$ErrorActionPreference = 'Stop'
$vectorPath = Join-Path $PSScriptRoot 'vectors\p1\p1_contract_vectors.jsonl'
if (-not (Test-Path -LiteralPath $SchemaPath -PathType Leaf)) { throw 'P1_SCHEMA_MISSING' }
$schema = Get-Content -Raw -LiteralPath $SchemaPath | ConvertFrom-Json
$vectors = @(Get-Content -LiteralPath $vectorPath | ForEach-Object { $_ | ConvertFrom-Json })
$allowed = @($schema.properties.category.enum)
$levels = @($schema.properties.evidence_level.enum)
$topLevelAllowed = @($schema.properties.psobject.Properties.Name)
$ids = @{}
foreach ($v in $vectors) {
    foreach ($property in $v.psobject.Properties.Name) {
        if ($property -notin $topLevelAllowed) { throw "P1_ADDITIONAL_PROPERTY case=$($v.case_id) field=$property" }
    }
    foreach ($field in @('schema','case_id','category','input','expected','evidence_level')) {
        if ($null -eq $v.$field) { throw "P1_SCHEMA_REQUIRED case=$($v.case_id) field=$field" }
    }
    if ($v.schema -ne 'p1-feature-vector-v1') { throw "P1_SCHEMA_NAME case=$($v.case_id)" }
    if ($ids.ContainsKey($v.case_id)) { throw "P1_CASE_ID_DUPLICATE case=$($v.case_id)" }
    $ids[$v.case_id] = $true
    if ($v.category -notin $allowed) { throw "P1_CATEGORY case=$($v.case_id)" }
    if ($v.evidence_level -notin $levels) { throw "P1_EVIDENCE_LEVEL case=$($v.case_id)" }
    foreach ($field in @('frame_id','config_seq','source_flags')) {
        if ($null -eq $v.input.$field) { throw "P1_INPUT_REQUIRED case=$($v.case_id) field=$field" }
    }
    foreach ($field in @('accept','ack','decision','reason')) {
        if ($null -eq $v.expected.$field) { throw "P1_EXPECTED_REQUIRED case=$($v.case_id) field=$field" }
    }
    if ($v.input.frame_id -lt 0 -or $v.input.frame_id -gt 65535 -or
        $v.input.config_seq -lt 0 -or $v.input.config_seq -gt 65535 -or
        $v.input.source_flags -lt 0 -or $v.input.source_flags -gt 255) {
        throw "P1_BOUNDARY case=$($v.case_id)"
    }
    if (($v.input.source_flags -band 0x80) -ne 0 -and ($v.expected.accept -or $v.expected.ack)) {
        throw "P1_RESERVED_BIT_NOT_FAIL_CLOSED case=$($v.case_id)"
    }
}
foreach ($requiredCase in @('snapshot_reserved_bit_001','frame_wrap_001','event_jitter_001',
                            'result_commit_001','result_stale_001','size_unavailable_001',
                            'non_cube_provisional_001')) {
    if (-not $ids.ContainsKey($requiredCase)) { throw "P1_REQUIRED_CASE_MISSING case=$requiredCase" }
}
$reserved = $vectors | Where-Object case_id -eq 'snapshot_reserved_bit_001'
if ($reserved.input.source_flags -ne 0xC7 -or $reserved.expected.accept -or $reserved.expected.ack) {
    throw 'P1_RESERVED_CASE_SEMANTICS'
}

$replay = @(Get-Content -LiteralPath $ReplayPath | ForEach-Object { $_ | ConvertFrom-Json })
if ($replay.Count -ne 20) { throw "P1_REPLAY_COUNT actual=$($replay.Count)" }
foreach ($r in $replay) {
    foreach ($field in @('round_id','task','event_seq','snapshot_hash','ack_sequence','decision','reason',
                         'result_commit_count','elapsed_ms','terminal_release','second_result_count','arm_enabled')) {
        if ($null -eq $r.$field) { throw "P1_REPLAY_REQUIRED round=$($r.round_id) field=$field" }
    }
    if ($r.snapshot_hash -notmatch '^[0-9A-F]{64}$') { throw "P1_SNAPSHOT_HASH round=$($r.round_id)" }
    if ($r.arm_enabled -ne 0 -or $r.result_commit_count -ne 1 -or $r.second_result_count -ne 0) {
        throw "P1_REPLAY_SAFETY round=$($r.round_id)"
    }
}
foreach ($task in 1..4) {
    if (($replay | Where-Object task -eq $task).Count -ne 5) { throw "P1_TASK_COUNT task=$task" }
}
foreach ($required in @('ABANDON','TIMEOUT','RESET')) {
    if (($replay | Where-Object terminal_release -eq $required).Count -eq 0) { throw "P1_NEGATIVE_MISSING $required" }
}
if (($replay | Where-Object { $_.task -ge 3 -and $_.reason -ne 'SIZE_UNAVAILABLE' }).Count -ne 0) {
    throw 'P1_SIZE_OVERCLAIM'
}
Write-Host "P1_VECTOR_SCHEMA=PASS count=$($vectors.Count)"
Write-Host 'P1_REPLAY_SCHEMA=PASS rounds=20 ARM=0'
