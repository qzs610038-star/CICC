[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$ContractPath
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Get-UpperSha256 {
    param([Parameter(Mandatory)] [string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing WSC probe input: $Path" }
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

if (-not (Test-Path -LiteralPath $ContractPath -PathType Leaf)) {
    throw "WSC probe binding is missing: $ContractPath"
}

$contract = Get-Content -LiteralPath $ContractPath -Raw | ConvertFrom-Json
if ($contract.owner -ne 'WSC' -or $contract.status -ne 'READY_STATIC') {
    throw 'WSC probe binding is not a static-ready WSC contract.'
}
if ($contract.source_contract_commit -ne 'a840f0869c11bab0915757d64c56a167f6d4f917' -or
    $contract.source_probe_commit -ne '15908b32475f6ce80b645a728c25a5e7a2db749f') {
    throw 'WSC probe binding source commit is not the fixed cloud contract.'
}
if ($contract.approved_address -ne '0xE8100000' -or $contract.expected_value -ne '0x375A0001' -or
    $contract.mode -ne 'CPU_READ_ONLY_SINGLE_ADDRESS_DEBUGGER_RAM_EVIDENCE' -or
    $contract.entry_pc -ne '0xF9000000' -or $contract.halt_pc -ne '0xF90000C4' -or
    $contract.timeout_ms -ne 1000) {
    throw 'WSC probe binding fixed values are invalid.'
}

$contractDocument = Join-Path $root $contract.wsc_contract_path
$evidence = Join-Path $root $contract.wsc_evidence_path
$probeElf = Join-Path $root $contract.probe_elf_path
foreach ($item in @(
    @{ Path = $contractDocument; Expected = $contract.wsc_contract_sha256 },
    @{ Path = $evidence; Expected = $contract.wsc_evidence_sha256 },
    @{ Path = $probeElf; Expected = $contract.probe_elf_sha256 }
)) {
    $actual = Get-UpperSha256 -Path $item.Path
    if ($actual -ne $item.Expected.ToUpperInvariant()) {
        throw "WSC probe binding hash mismatch: $($item.Path) expected=$($item.Expected) actual=$actual"
    }
}

$documentText = Get-Content -LiteralPath $contractDocument -Raw
$evidenceText = Get-Content -LiteralPath $evidence -Raw
foreach ($required in @(
    'debugger must not directly read, inspect, watch, dump, or poll `0xE8100000`',
    'APB_PROBE_CONTRACT=PASS',
    'apb_lw_count=1',
    'apb_store_count=0',
    'deterministic_halt_pc=0xF90000C4'
)) {
    if (($documentText + "`n" + $evidenceText) -notmatch [regex]::Escape($required)) {
        throw "WSC fixed probe proof missing: $required"
    }
}

@(
    "WSC_PROBE_CONTRACT_PATH=$ContractPath",
    "WSC_PROBE_ELF_SHA256=$($contract.probe_elf_sha256)",
    "WSC_PROBE_CONTRACT_SHA256=$($contract.wsc_contract_sha256)",
    'WSC_PROBE_GATE=PASS_STATIC_ONLY'
)
