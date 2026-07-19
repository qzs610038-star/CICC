[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$root = Split-Path -Parent $projectRoot
$manifestPath = Join-Path $PSScriptRoot 'i0_uart1_execution_manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
function Assert-Contains([string]$Text, [string]$Value) { if (-not $Text.Contains($Value)) { throw "Missing static clause: $Value" } }
function Get-Sha([string]$Path) { if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing required file: $Path" }; (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant() }
if ($manifest.batch -ne 'I0_UART1_20260719_CLEAN_LF_FINAL') { throw 'Batch mismatch.' }
foreach ($entry in $manifest.files) { $actual = Get-Sha (Join-Path $root $entry.path); if ($actual -ne $entry.sha256) { throw "Manifest hash mismatch: $($entry.path)" } }
$cfg = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'i0_uart1_cleanlf_user2.cfg') -Raw
$ram = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'i0_uart1_cleanlf_ram_halt.gdb') -Raw
$capture = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'capture_i0_uart1_raw.ps1') -Raw
$apb = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'i0_uart1_wsc_apb_probe.gdb') -Raw
foreach ($value in @('CPUTAPID 0x006A0A79','I0_TITANIUM_USER2_OUTER_IR 0x09','I0_BSCAN_TUNNEL_TYPE 6','I0_BSCAN_TUNNEL_USER 1','I0_BSCAN_TUNNEL_IR_WIDTH 8','I0_RAM_WORK_AREA 0xF9000000','init','halt')) { Assert-Contains $cfg $value }
foreach ($value in @('monitor halt','load','0xF9000000','0xF9003FFF','CAPTURE_READY','I0_RESUME_COUNT_LIMIT=1','continue')) { Assert-Contains $ram $value }
foreach ($value in @('PnPAllowlist','ResumeMarkerPath','COM17 is prohibited','CH340 is prohibited','AUTO_CRLF=DISABLED','CAPTURE_READY=','RESUME_ONCE=','TX_COUNT=','RX_COUNT=','SILENT_WINDOW_END=','I0 UART1 HELLO','UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100','[\x20-\x7E]')) { Assert-Contains $capture $value }
foreach ($value in @('0xF90000C4','WSC_PROBE_RESUME_ONCE_TIMEOUT_MS=1000','WSC_RAM_READ_COUNT=4','0xF90000D0','0xF90000D4','0xF90000E4','0xF90000E8')) { Assert-Contains $apb $value }
if ($apb -match '(?i)0xE810000[1-9A-F]|x/wx|monitor.*E810') { throw 'Direct APB debugger route detected.' }
$scan = @($cfg, $ram, $capture, $apb) -join "`n"
if ($scan -match '(?i)\b(prepare_m2|softtap|flash|spi|prom|ddr|uart0)\b|\b(g1_|r0_)|com17.*allow') { throw 'Prohibited route token detected.' }
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'i0_uart1_cleanlf_apb_read.gdb')) { 'DIRECT_APB_READ_ROUTE=BASELINE_PRESERVED' } else { 'DIRECT_APB_READ_ROUTE=ABSENT_IN_BASELINE' }
& (Join-Path $PSScriptRoot 'i0_uart1_wsc_probe_gate.ps1')
if ($LASTEXITCODE -ne 0) { throw 'WSC gate failed.' }
'USER2_SELECTION_CHAIN=BLOCKED'
'UART0_DEPENDENCY_COUNT=0'
'DIRECT_APB_READ_COUNT=0'
'APB_WRITE_COUNT=0'
'CAPTURE_BEFORE_RESUME=PASS'
'PNP_ALLOWLIST=PASS'
'SINGLE_PRINTABLE_BYTE_NO_CRLF=PASS'
'STATIC_EXECUTION_VERIFIER=BLOCKED'
'HARDWARE_ACTIONS=NONE'
exit 2
