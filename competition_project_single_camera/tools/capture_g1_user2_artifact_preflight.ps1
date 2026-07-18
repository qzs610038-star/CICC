[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ManifestPath,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$BitPath,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ElfPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expected = [ordered]@{
    schema = 'g1_current_batch_manifest'
    version = 1
    batch_id = 'G1-20260717-A897-E5BC'
    input_baseline_sha = '489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f'
    bit_sha256 = 'A897E33514A1079BB1B46C02C464B0BD679AF551CEFCB67C4A0EBD5B8FCD1ACD'
    bit_size_bytes = 11847132
    elf_sha256 = 'E5BC80A2F18A7E2951D53DA539BE2FC61AAECFA90C5CDADB29E65FFC6141928A'
    elf_size_bytes = 31116
    elf_load_range = '0xF9000000..0xF9000A30'
    elf_entry_point = '0xF9000000'
    valid_pc_range = '0xF9000000..0xF9003FFF'
    user_tap = 'USER2'
    uart0_baud = 115200
}

function Get-ArtifactIdentity {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required artifact is missing: $Path"
    }

    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    [ordered]@{
        path = $item.FullName
        size_bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
}

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Manifest is missing: $ManifestPath"
}

try {
    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json -ErrorAction Stop
}
catch {
    throw "Manifest is not valid JSON: $ManifestPath"
}

$manifestChecks = [ordered]@{
    schema = ($manifest.schema -eq $expected.schema)
    version = ($manifest.version -eq $expected.version)
    batch_id = ($manifest.batch_id -eq $expected.batch_id)
    input_baseline_sha = ($manifest.input_baseline_sha -eq $expected.input_baseline_sha)
    bit_sha256 = ($manifest.bitstream.sha256 -eq $expected.bit_sha256)
    bit_size_bytes = ($manifest.bitstream.size_bytes -eq $expected.bit_size_bytes)
    elf_sha256 = ($manifest.hello_elf.sha256 -eq $expected.elf_sha256)
    elf_size_bytes = ($manifest.hello_elf.size_bytes -eq $expected.elf_size_bytes)
    elf_load_range = ($manifest.hello_elf.load_range -eq $expected.elf_load_range)
    elf_entry_point = ($manifest.hello_elf.entry_point -eq $expected.elf_entry_point)
    valid_pc_range = ($manifest.hello_elf.valid_pc_range -eq $expected.valid_pc_range)
    user_tap = ($manifest.debug.required_user_tap -eq $expected.user_tap)
    uart0_baud = ($manifest.uart0.baud -eq $expected.uart0_baud)
}

$failedManifestChecks = @($manifestChecks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)
if ($failedManifestChecks.Count -gt 0) {
    throw ('Manifest does not bind the approved G1 batch: ' + ($failedManifestChecks -join ', '))
}

$bit = Get-ArtifactIdentity -Path $BitPath
$elf = Get-ArtifactIdentity -Path $ElfPath
$artifactChecks = [ordered]@{
    bit_size_bytes = ($bit.size_bytes -eq $expected.bit_size_bytes)
    bit_sha256 = ($bit.sha256 -eq $expected.bit_sha256)
    elf_size_bytes = ($elf.size_bytes -eq $expected.elf_size_bytes)
    elf_sha256 = ($elf.sha256 -eq $expected.elf_sha256)
}
$failedArtifactChecks = @($artifactChecks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)
if ($failedArtifactChecks.Count -gt 0) {
    throw ('Artifact identity mismatch: ' + ($failedArtifactChecks -join ', '))
}

[pscustomobject]@{
    result = 'G1_CURRENT_BATCH_ARTIFACT_PREFLIGHT_PASS'
    batch_id = $expected.batch_id
    input_baseline_sha = $expected.input_baseline_sha
    bitstream = $bit
    hello_elf = $elf
    user_tap = $expected.user_tap
    uart0_baud = $expected.uart0_baud
    hardware_actions_performed = $false
} | ConvertTo-Json -Depth 5
