[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $root 'tools\i0_uart1_execution_manifest.json'
$packetPath = Join-Path $root 'docs\review_packets\I0_UART1_CLEANLF_USER2_EXECUTION_CONFIG_REVIEW_20260719.md'
$designRoot = 'C:\cicc_i0_uart1_design_lf_20260719_v4\competition_project_single_camera'

function Get-UpperSha256 {
    param([Parameter(Mandatory)] [string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing file: $Path" }
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Assert-Text {
    param([string]$Text, [string]$Pattern, [string]$Description)
    if ($Text -notmatch $Pattern) { throw "Required proof missing: $Description" }
}

$requiredArtifacts = @(
    @{ Path = Join-Path $designRoot 'outflow_i0_uart1_20260719_cleanlf_v4\mem_test.bit'; Hash = 'D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544' },
    @{ Path = Join-Path $designRoot 'embedded_sw\uart1_hello_onchip\build\uart1_hello_onchip.elf'; Hash = '919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA' },
    @{ Path = Join-Path $designRoot 'embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h'; Hash = '25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B' }
)

foreach ($item in $requiredArtifacts) {
    $actual = Get-UpperSha256 -Path $item.Path
    if ($actual -ne $item.Hash) { throw "Fixed artifact hash mismatch: $($item.Path)" }
    "FIXED_ARTIFACT_SHA256 $actual $($item.Path)"
}

if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Missing manifest: $manifestPath" }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.batch -ne 'I0_UART1_20260719_CLEAN_LF_FINAL') { throw 'Manifest batch mismatch.' }
if ($manifest.files.Count -lt 8) { throw 'Manifest does not cover the execution configuration surface.' }

foreach ($entry in $manifest.files) {
    $path = if ([System.IO.Path]::IsPathRooted($entry.path)) { $entry.path } else { Join-Path $root $entry.path }
    $actual = Get-UpperSha256 -Path $path
    if ($actual -ne $entry.sha256.ToUpperInvariant()) {
        throw "Manifest hash mismatch: $($entry.path) expected=$($entry.sha256) actual=$actual"
    }
    "MANIFEST_SHA256_OK $actual $($entry.path)"
}

$cfgPath = Join-Path $root 'tools\i0_uart1_cleanlf_user2.cfg'
$gdbPath = Join-Path $root 'tools\i0_uart1_cleanlf_ram_halt.gdb'
$capturePath = Join-Path $root 'tools\capture_i0_uart1_raw.ps1'
$contractPath = Join-Path $root 'tools\wsc_i0_apb_probe_contract.json'
$gatePath = Join-Path $root 'tools\i0_uart1_wsc_probe_gate.ps1'
$wscGdbPath = Join-Path $root 'tools\i0_uart1_wsc_apb_probe.gdb'
$cfg = Get-Content -LiteralPath $cfgPath -Raw
$gdb = Get-Content -LiteralPath $gdbPath -Raw
$capture = Get-Content -LiteralPath $capturePath -Raw
$wscGdb = Get-Content -LiteralPath $wscGdbPath -Raw
$packet = Get-Content -LiteralPath $packetPath -Raw

if (Test-Path -LiteralPath (Join-Path $root 'tools\i0_uart1_cleanlf_apb_read.gdb')) {
    throw 'Direct APB GDB route must not exist.'
}
$toolText = Get-ChildItem -LiteralPath (Join-Path $root 'tools') -File |
    ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }
if ($toolText -match '(?mi)^\s*x/wx\b') {
    throw 'Debugger x/wx route found in tool files.'
}

Assert-Text $cfg 'set I0_USER2_OUTER_IR 0x09' 'USER2 outer IR declaration'
Assert-Text $cfg 'set I0_BSCAN_TUNNEL_USER \[expr \{\$I0_USER2_OUTER_IR == 0x09 \? 1 : 0\}\]' 'USER2 IR consumption into tunnel user selector'
Assert-Text $cfg 'riscv use_bscan_tunnel \$I0_BSCAN_TUNNEL_TYPE \$I0_BSCAN_TUNNEL_USER' 'USER2-derived tunnel selection command'
Assert-Text $cfg 'I0_USER2_SELECTION_CHAIN=outer_ir=0x09->tunnel_type=6,user=1->tunnel_ir=8' 'USER2 selection chain log marker'
Assert-Text $cfg '0x006A0A79' 'TJ375 CPU TAP ID'
Assert-Text $cfg 'set I0_BSCAN_TUNNEL_IR 8' 'BSCAN tunnel IR'
Assert-Text $gdb 'I0_PC_RANGE_PASS' 'PC gate pass marker'
Assert-Text $gdb 'I0_RESUME_CONTROLLED=AFTER_PC_RANGE_PASS' 'controlled resume marker'
Assert-Text $gdb '(?m)^continue$' 'resume after PC gate'
Assert-Text $capture 'Get-CimInstance -ClassName Win32_SerialPort' 'system PnP serial enumeration'
Assert-Text $capture 'DEVPKEY_Device_LocationPaths' 'PnP location binding'
Assert-Text $capture 'HELLO_COMPLETE lines=3' 'three-line Hello gate'
Assert-Text $capture 'ECHO_COMPLETE byte=' 'printable echo gate'
Assert-Text $capture '\^\[\\x20-\\x7E\]\$' 'printable-byte validation'
Assert-Text $capture 'COM17/CH340 is prohibited' 'CH340 rejection'
Assert-Text $capture 'rx_bytes=' 'continuous RX counter'
Assert-Text $capture 'tx_bytes=' 'continuous TX counter'
Assert-Text $wscGdb 'WSC_PROBE_ENTRY_PC_PASS' 'WSC probe entry PC gate'
Assert-Text $wscGdb 'break \*0xF90000C4' 'WSC deterministic halt breakpoint'
Assert-Text $wscGdb 'WSC_PROBE_RESUME_ONCE_TIMEOUT_MS=1000' 'WSC one-resume timeout marker'
Assert-Text $wscGdb 'WSC_RAM_g_apb_probe_expected=' 'WSC RAM evidence read'
foreach ($ramAddress in @('0xF90000D0', '0xF90000D4', '0xF90000E4', '0xF90000E8')) {
    Assert-Text $wscGdb $ramAddress "WSC fixed RAM evidence address $ramAddress"
}
if ([regex]::Matches($wscGdb, '(?m)^continue$').Count -ne 1) {
    throw 'WSC probe consumer must resume exactly once.'
}
if ($wscGdb -match '(?i)x/wx|\*\s*\([^\r\n]*\)\s*0xE8100') {
    throw 'WSC GDB consumer must not access the APB window.'
}

$forbidden = '(?i)\bUART0\b|\bSOFTTAP\b|\bFLASH\b|\bSPI\b|\bPROM\b|\bDDR\b|\bUSER1\b|prepare_m2|g1_user2|r0_'
foreach ($path in @($cfgPath, $gdbPath, $capturePath, $gatePath, $wscGdbPath)) {
    $executableText = (Get-Content -LiteralPath $path | Where-Object { $_ -notmatch '^\s*(#|$)' }) -join "`n"
    if ($executableText -match $forbidden) { throw "Prohibited route token in executable text: $path" }
}

$officialDebug = 'D:\Efinity\2025.2\ipm\ip\efx_hard_soc\fpga\Ti375C529_devkit\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\openocd\debug_ti.cfg'
$officialFtdi = 'D:\Efinity\2025.2\ipm\ip\efx_hard_soc\fpga\Ti375C529_devkit\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\openocd\ftdi_ti.cfg'
$officialJtag = 'D:\Efinity\2025.2\debugger\bin\efx_dbg\jtag.py'
$officialDebugText = Get-Content -LiteralPath $officialDebug -Raw
$officialJtagText = Get-Content -LiteralPath $officialJtag -Raw
Assert-Text $officialDebugText 'riscv use_bscan_tunnel 6 1' 'Efinity tunnel type/user source'
Assert-Text $officialDebugText 'riscv set_bscan_tunnel_ir 8' 'Efinity tunnel IR source'
Assert-Text $officialJtagText "'USER2': BitSequence\('01001', msb=True, length=5\)" 'Efinity Titanium USER2 outer IR source'

$contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if ($contract.owner -ne 'WSC' -or $contract.approved_address -ne '0xE8100000' -or
    $contract.expected_value -ne '0x375A0001' -or
    $contract.mode -ne 'CPU_READ_ONLY_SINGLE_ADDRESS_DEBUGGER_RAM_EVIDENCE') {
    throw 'WSC probe contract fixed fields are invalid.'
}
if ($contract.status -ne 'READY_STATIC' -or
    $contract.source_contract_commit -ne 'a840f0869c11bab0915757d64c56a167f6d4f917' -or
    $contract.source_probe_commit -ne '15908b32475f6ce80b645a728c25a5e7a2db749f' -or
    $contract.probe_elf_sha256 -ne '6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC') {
    throw 'WSC fixed probe binding is invalid.'
}

# Packet lists the exact expected hashes for all executable/configuration files.
foreach ($entry in $manifest.files | Where-Object { -not $_.path.EndsWith('I0_UART1_CLEANLF_USER2_EXECUTION_CONFIG_REVIEW_20260719.md') }) {
    $packetHashPattern = '\|\s*`?' + [regex]::Escape($entry.path) + '`?\s*\|\s*`?' +
        [regex]::Escape($entry.sha256) + '`?\s*\|'
    if ($packet -notmatch $packetHashPattern) {
        throw "Packet/manifest hash mismatch or missing line: $($entry.path)"
    }
}

"VERIFIER_SHA256 $(Get-UpperSha256 -Path $PSCommandPath)"
"MANIFEST_SHA256 $(Get-UpperSha256 -Path $manifestPath)"
"PACKET_SHA256 $(Get-UpperSha256 -Path $packetPath)"
'USER2_SELECTION_CHAIN=PASS'
'WSC_PROBE_GATE=PASS_STATIC_ONLY'
'I0_UART1_EXECUTION_CONFIG_STATIC=PASS'
