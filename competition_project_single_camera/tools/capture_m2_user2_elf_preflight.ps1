[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$MirrorRoot,
    [string]$EvidenceDir,
    [string]$Label = 'm2_user2_elf_preflight',
    [switch]$RequireMirror
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $PSScriptRoot '..\docs\debug_sessions\evidence'
}

$expectedElfSha256 = 'C99FD39DB437409A63A6061CD29698B5B60099B9E24A77B155B871E169BF5DA5'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$canonicalElf = Join-Path $repoRoot 'cpu_bringup\uart_hello_onchip\build\uart_hello_onchip.elf'
$mirrorElf = Join-Path $MirrorRoot 'm2_cpuhello_20260716_1730\uart_hello_onchip.elf'

function Get-FileIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedSha256
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{
            path = $Path
            exists = $false
            bytes = $null
            sha256 = $null
            matches_expected = $false
        }
    }

    $item = Get-Item -LiteralPath $Path
    $sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    return [ordered]@{
        path = $item.FullName
        exists = $true
        bytes = $item.Length
        sha256 = $sha256
        matches_expected = ($sha256 -eq $ExpectedSha256)
    }
}

$canonical = Get-FileIdentity -Path $canonicalElf -ExpectedSha256 $expectedElfSha256
if (-not $canonical.exists) {
    throw "Canonical M2 Hello ELF is missing: $canonicalElf"
}

$mirror = Get-FileIdentity -Path $mirrorElf -ExpectedSha256 $expectedElfSha256
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "git rev-parse HEAD failed with exit code $LASTEXITCODE"
}

$result = if (-not $canonical.matches_expected) {
    'HOLD_CANONICAL_ELF_HASH_MISMATCH'
} elseif (-not $mirror.exists) {
    'HOLD_ELF_MIRROR_MISSING'
} elseif (-not $mirror.matches_expected) {
    'HOLD_ELF_MIRROR_HASH_MISMATCH'
} else {
    'ELF_MIRROR_READY_FOR_USER2_RAM_LOAD'
}

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$evidencePath = Join-Path (Resolve-Path $EvidenceDir).Path ("{0}_{1}.json" -f $Label, $timestamp)

$evidence = [ordered]@{
    schema = 'm2_user2_elf_preflight_v2'
    timestamp_local = (Get-Date).ToString('o')
    repo_head = $head
    read_only_actions = @(
        'Get-FileHash canonical Hello ELF',
        'Get-FileHash ASCII-mirrored Hello ELF when present',
        'git rev-parse HEAD'
    )
    serial_port_opened = $false
    uart_bytes_sent = 0
    programmer_invoked = $false
    openocd_invoked = $false
    gdb_invoked = $false
    flash_operation_invoked = $false
    user_tap_selected = $null
    expected_elf_sha256 = $expectedElfSha256
    canonical_elf = $canonical
    mirror_elf = $mirror
    result = $result
    next_action = if ($result -eq 'ELF_MIRROR_READY_FOR_USER2_RAM_LOAD') {
        'Return this JSON with the USER2/Target/Debug-in-RAM configuration screenshot. Do not resume or open UART.'
    } else {
        'Keep USER2 load HOLD. Correct only the Hello ELF mirror identity, then rerun with -RequireMirror; do not start IDE/OpenOCD.'
    }
}

$evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $evidencePath -Encoding utf8
[PSCustomObject]@{
    result = $result
    canonical_sha256 = $canonical.sha256
    mirror_sha256 = $mirror.sha256
    evidence_path = $evidencePath
} | ConvertTo-Json -Depth 4

if ($RequireMirror -and $result -ne 'ELF_MIRROR_READY_FOR_USER2_RAM_LOAD') {
    exit 2
}
