[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$bindingPath = Join-Path $PSScriptRoot 'wsc_i0_apb_probe_contract.json'
if (-not (Test-Path -LiteralPath $bindingPath -PathType Leaf)) { throw 'Missing WSC binding summary.' }
$binding = Get-Content -LiteralPath $bindingPath -Raw | ConvertFrom-Json
if ($binding.owner -ne 'WSC' -or $binding.status -ne 'READY_STATIC') { throw 'WSC ownership/status mismatch.' }
if ($binding.source_commit -ne '48548f47dfa5964b13aed7edf3b3e9da6f6583a2') { throw 'WSC source SHA mismatch.' }
if ($binding.cpu_lw_count -ne 1 -or $binding.apb_write_count -ne 0 -or $binding.timeout_ms -ne 1000 -or $binding.failure_ram_read_count -ne 0) { throw 'WSC fixed probe limits mismatch.' }
foreach ($name in @('document', 'summary', 'verifier')) {
    $path = $binding.contract_paths.$name
    $actual = (git -C $root rev-parse "$($binding.source_commit):$path").Trim()
    if ($actual -ne $binding.source_blob_sha1.$name) { throw "WSC source blob mismatch: $name" }
}
'WSC_PROBE_GATE=PASS_STATIC_ONLY'
"WSC_FINAL_SHA=$($binding.source_commit)"
"PROBE_ELF_SHA256=$($binding.probe_elf_sha256)"
'DIRECT_APB_READ_COUNT=0'
'APB_WRITE_COUNT=0'
'HARDWARE_ACTIONS=NONE'
