param(
    [Parameter(Mandatory = $true)][string]$ManifestA,
    [Parameter(Mandatory = $true)][string]$ManifestB,
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [string]$BundleLabelA = 'fresh_bundle_a',
    [string]$BundleLabelB = 'fresh_bundle_b'
)

$ErrorActionPreference = 'Stop'

function Read-Manifest([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "P0A_REPRO_MANIFEST_MISSING: $Path" }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $value = Get-Content -Raw -LiteralPath $resolved | ConvertFrom-Json
    if ($value.schema -ne 'p0-a-evidence-v1') { throw "P0A_REPRO_SCHEMA: $resolved" }
    return [pscustomobject]@{ Path=$resolved; Root=(Split-Path -Parent $resolved); Value=$value }
}

function Assert-ArtifactHashes($Manifest) {
    foreach ($property in $Manifest.Value.artifacts.psobject.Properties) {
        $artifact = $property.Value
        $path = Join-Path $Manifest.Root $artifact.path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "P0A_REPRO_ARTIFACT_MISSING bundle=$($Manifest.Root) artifact=$($property.Name)"
        }
        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
        if ($actual -ne $artifact.sha256) {
            throw "P0A_REPRO_ARTIFACT_HASH bundle=$($Manifest.Root) artifact=$($property.Name) expected=$($artifact.sha256) actual=$actual"
        }
    }
}

function Get-ComparedFields($Manifest) {
    $v = $Manifest.Value
    return [ordered]@{
        implementation_git_sha = $v.firmware.git_sha
        normalized_input_sha256 = $v.firmware.input_sha256
        build_id_hex = $v.firmware.build_id_hex
        stripped_diagnostic_elf_sha256 = $v.artifacts.elf.sha256
        loadable_layout_map_sha256 = $v.artifacts.map.sha256
        readelf_sha256 = $v.artifacts.readelf.sha256
        objdump_sha256 = $v.artifacts.objdump.sha256
        strict_build_log_sha256 = $v.artifacts.build_log.sha256
        tx_never_ready_sha256 = $v.artifacts.tx_never_ready.sha256
        memory_json = ($v.memory | ConvertTo-Json -Depth 10 -Compress)
        canary_json = ($v.canary | ConvertTo-Json -Depth 10 -Compress)
        disassembly_order_json = ($v.disassembly_order_witness | ConvertTo-Json -Depth 10 -Compress)
    }
}

$a = Read-Manifest $ManifestA
$b = Read-Manifest $ManifestB
if ($a.Path -eq $b.Path) { throw 'P0A_REPRO_REQUIRES_TWO_DISTINCT_MANIFESTS' }
Assert-ArtifactHashes $a
Assert-ArtifactHashes $b
$fieldsA = Get-ComparedFields $a
$fieldsB = Get-ComparedFields $b
$comparisons = @()
$mismatch = $false
foreach ($name in $fieldsA.Keys) {
    $match = $fieldsA[$name] -eq $fieldsB[$name]
    if (-not $match) { $mismatch = $true }
    $comparisons += [ordered]@{field=$name;value_a=$fieldsA[$name];value_b=$fieldsB[$name];match=$match}
}
if ($mismatch) {
    $failed = ($comparisons | Where-Object { -not $_.match } | ForEach-Object { $_.field }) -join ','
    throw "P0A_REPRO_MISMATCH fields=$failed"
}

$output = [ordered]@{
    schema = 'p0-a-reproducibility-v2'
    status = 'PASS_AWAITING_QZS_REVIEW'
    generated_by = 'compare_reproducibility.ps1'
    manifest_a = [ordered]@{label=$BundleLabelA;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $a.Path).Hash}
    manifest_b = [ordered]@{label=$BundleLabelB;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $b.Path).Hash}
    artifact_hashes_validated = $true
    compared_fields = $comparisons
    all_compared_fields_match = ($comparisons.Count -gt 0 -and @($comparisons | Where-Object { -not $_.match }).Count -eq 0)
    debug_elf_policy = 'local non-identity artifact; never committed'
}
$parent = Split-Path -Parent $OutputPath
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$output | ConvertTo-Json -Depth 10 | Set-Content -Encoding ascii -LiteralPath $OutputPath

$roundTrip = Get-Content -Raw -LiteralPath $OutputPath | ConvertFrom-Json
if ($roundTrip.schema -ne 'p0-a-reproducibility-v2' -or
    -not $roundTrip.artifact_hashes_validated -or -not $roundTrip.all_compared_fields_match -or
    @($roundTrip.compared_fields).Count -ne $comparisons.Count -or
    @($roundTrip.compared_fields | Where-Object { -not $_.match }).Count -ne 0) {
    throw 'P0A_REPRO_OUTPUT_VALIDATION_FAILED'
}
Write-Host "P0_A_REPRODUCIBILITY=PASS compared=$($comparisons.Count) output=$OutputPath"
