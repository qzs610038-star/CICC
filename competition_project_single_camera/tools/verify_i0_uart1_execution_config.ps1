[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$designRoot = 'C:\cicc_i0_uart1_design_lf_20260719_v4\competition_project_single_camera'
$required = @(
    @{ Path = Join-Path $designRoot 'outflow_i0_uart1_20260719_cleanlf_v4\mem_test.bit'; Hash = 'D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544' },
    @{ Path = Join-Path $designRoot 'embedded_sw\uart1_hello_onchip\build\uart1_hello_onchip.elf'; Hash = '919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA' },
    @{ Path = Join-Path $designRoot 'embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h'; Hash = '25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B' }
)

foreach ($item in $required) {
    if (-not (Test-Path -LiteralPath $item.Path -PathType Leaf)) { throw "Missing fixed artifact: $($item.Path)" }
    if ((Get-FileHash -LiteralPath $item.Path -Algorithm SHA256).Hash -ne $item.Hash) { throw "Hash mismatch: $($item.Path)" }
}

$files = @(
    (Join-Path $root 'tools\i0_uart1_cleanlf_user2.cfg'),
    (Join-Path $root 'tools\i0_uart1_cleanlf_ram_halt.gdb'),
    (Join-Path $root 'tools\i0_uart1_cleanlf_apb_read.gdb')
)
$prohibited = '(?i)\bUART0\b|\bSOFTTAP\b|\bFLASH\b|\bSPI\b|\bPROM\b|\bDDR\b|\bUSER1\b|prepare_m2|g1_user2|r0_'
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing execution file: $file" }
    # Prohibited-route documentation is required. Inspect only executable lines.
    $content = (Get-Content -LiteralPath $file | Where-Object { $_ -notmatch '^\s*(#|$)' }) -join "`n"
    if ($content -match $prohibited) { throw "Prohibited route token in: $file" }
}

$capture = Join-Path $root 'tools\capture_i0_uart1_raw.ps1'
if (-not (Test-Path -LiteralPath $capture -PathType Leaf)) { throw "Missing capture script: $capture" }
$captureText = Get-Content -LiteralPath $capture -Raw
foreach ($token in @('115200', 'StopBits]::One', 'rx_bytes', 'tx_bytes', "Port -eq 'COM17'", 'CH340')) {
    if ($captureText -notmatch [regex]::Escape($token)) { throw "Capture guard missing: $token" }
}

$cfg = Get-Content -LiteralPath (Join-Path $root 'tools\i0_uart1_cleanlf_user2.cfg') -Raw
foreach ($token in @('0x006A0A79', '0x09', 'set I0_BSCAN_TUNNEL_TYPE 6', 'set I0_BSCAN_TUNNEL_USER 1', 'set I0_BSCAN_TUNNEL_IR 8', '0xF9000000', 'halt')) {
    if ($cfg -notmatch [regex]::Escape($token)) { throw "Missing required USER2 configuration: $token" }
}

$apb = Get-Content -LiteralPath (Join-Path $root 'tools\i0_uart1_cleanlf_apb_read.gdb') -Raw
if ($apb -notmatch 'x/wx 0xE8100000' -or $apb -match '(?i)\bset\s*\{|\bset\s+\*|\brestore\b') { throw 'APB script is not restricted to the single read.' }

@($files + $capture) | ForEach-Object { Get-FileHash -LiteralPath $_ -Algorithm SHA256 | Select-Object Path, Hash }
'I0_UART1_EXECUTION_CONFIG=PASS'
