[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$probeRoot = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $probeRoot '..\..\..')).Path
$socPath = Join-Path $repoRoot 'competition_project_single_camera\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h'
$elfPath = Join-Path $probeRoot 'artifacts\apb_magic_onchip.elf'
$objdumpPath = Join-Path $probeRoot 'artifacts\apb_magic_onchip.objdump.txt'
$readelfPath = Join-Path $probeRoot 'artifacts\apb_magic_onchip.readelf.txt'
$symbolsPath = Join-Path $probeRoot 'artifacts\apb_magic_onchip.symbols.txt'
$contractPath = Join-Path $probeRoot 'APB_PROBE_DEBUGGER_CONTRACT.md'
$outputPath = Join-Path $probeRoot 'artifacts\apb_magic_onchip.contract.txt'

$expectedSocHash = '25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B'
$expectedElfHash = '6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC'
$expectedEntry = '0xf9000000'
$expectedLoadEnd = 0xF90008F0L
$expectedApbBase = '0xe8100000'
$expectedHaltPc = '0xf90000c4'
$expectedTimeoutMs = 1000

foreach ($path in @($socPath, $elfPath, $objdumpPath, $readelfPath, $symbolsPath, $contractPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing input: $path" }
}

$socHash = (Get-FileHash -LiteralPath $socPath -Algorithm SHA256).Hash
$elfHash = (Get-FileHash -LiteralPath $elfPath -Algorithm SHA256).Hash
if ($socHash -ne $expectedSocHash) { throw "soc.h SHA-256 mismatch: $socHash" }
if ($elfHash -ne $expectedElfHash) { throw "ELF SHA-256 mismatch: $elfHash" }

$socText = Get-Content -LiteralPath $socPath -Raw
$baseMatch = [regex]::Match($socText, '(?m)^#define\s+IO_APB_SLAVE_0_INPUT\s+(?<base>0x[0-9a-fA-F]+)\s*$')
if (-not $baseMatch.Success) { throw 'Missing IO_APB_SLAVE_0_INPUT in soc.h.' }
if ($baseMatch.Groups['base'].Value.ToLowerInvariant() -ne $expectedApbBase) {
    throw "Unexpected APB base: $($baseMatch.Groups['base'].Value)"
}

$readelf = Get-Content -LiteralPath $readelfPath -Raw
$entryMatch = [regex]::Match($readelf, 'Entry point address:\s+(?<entry>0x[0-9a-fA-F]+)')
if (-not $entryMatch.Success -or $entryMatch.Groups['entry'].Value.ToLowerInvariant() -ne $expectedEntry) {
    throw 'ELF entry is not 0xF9000000.'
}
$loadLines = @($readelf -split "`r?`n" | Where-Object { $_ -match '^\s*LOAD\s+' })
if ($loadLines.Count -ne 1) { throw "Expected one LOAD segment, found $($loadLines.Count)." }
$loadFields = @($loadLines[0] -split '\s+' | Where-Object { $_ })
if ($loadFields[2].ToLowerInvariant() -ne $expectedEntry -or
    $loadFields[3].ToLowerInvariant() -ne $expectedEntry) {
    throw 'LOAD does not start at the fixed entry address.'
}
$fileSize = [Convert]::ToUInt32($loadFields[4].Substring(2), 16)
$memorySize = [Convert]::ToUInt32($loadFields[5].Substring(2), 16)
$loadEnd = 0xF9000000L + [Math]::Max($fileSize, $memorySize)
if ($loadEnd -ne $expectedLoadEnd) { throw ('Unexpected LOAD end: 0x{0:X8}' -f $loadEnd) }

$objdump = Get-Content -LiteralPath $objdumpPath -Raw
$mainMatch = [regex]::Match($objdump, '(?ms)^(?<address>[0-9a-f]+) <main>:\r?\n(?<body>.*?)(?=^[0-9a-f]+ <|\z)')
if (-not $mainMatch.Success) { throw 'Unable to isolate main disassembly.' }
$main = $mainMatch.Groups['body'].Value
$apbBaseLoads = @([regex]::Matches($objdump, '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+lui\s+[^,]+,0xe8100\s*\r?$'))
$lwInstructions = @([regex]::Matches($main, '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+lw\s+[^\r\n]+\r?$'))
$apbOffsetZeroLoads = @([regex]::Matches($main, '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+lw\s+[^,]+,0\([^\)]+\)\s*\r?$'))
if ($apbBaseLoads.Count -ne 1) { throw "APB base materialization count is $($apbBaseLoads.Count), expected 1." }
if ($lwInstructions.Count -ne 1) { throw "APB lw count is $($lwInstructions.Count), expected 1." }
if ($apbOffsetZeroLoads.Count -ne 1) { throw 'The unique APB lw is not offset 0.' }

$apbStores = @([regex]::Matches($main, '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+s[bhw]\s+[^\r\n]*(?:e810|\([^\)]*a5\))[^\r\n]*\r?$'))
$ramStores = @([regex]::Matches($main, '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+sw\s+[^\r\n]+#\s+f900[0-9a-f]+\s+<g_apb_probe_(?:observed|status)>\s*\r?$'))
if ($apbStores.Count -ne 0) { throw "APB store count is $($apbStores.Count), expected 0." }
if ($ramStores.Count -ne 2) { throw "RAM result store count is $($ramStores.Count), expected 2." }

$haltLoops = @([regex]::Matches($main, '(?m)^\s*(?<pc>f90000c4):\s+[0-9a-f]+\s+j\s+f90000c4\s+<main\+0x20>\s*\r?$'))
if ($haltLoops.Count -ne 1) { throw 'Deterministic halt self-loop is not exactly 0xF90000C4.' }

$symbols = Get-Content -LiteralPath $symbolsPath -Raw
$expectedSymbols = [ordered]@{
    g_apb_probe_expected = 'f90000d0'
    g_apb_probe_address = 'f90000d4'
    g_apb_probe_status = 'f90000e4'
    g_apb_probe_observed = 'f90000e8'
}
foreach ($entry in $expectedSymbols.GetEnumerator()) {
    $pattern = '(?m)^' + $entry.Value + '\s+\S\s+' + [regex]::Escape($entry.Key) + '\s*$'
    if (-not [regex]::IsMatch($symbols, $pattern)) { throw "Missing fixed RAM symbol: $($entry.Key)" }
}

$contract = Get-Content -LiteralPath $contractPath -Raw
$contractPlain = $contract.Replace('`', '')
foreach ($required in @(
    'must not directly read, inspect, watch, dump, or poll 0xE8100000',
    'PC == 0xF9000000',
    'PC == 0xF90000C4',
    '1000 ms',
    'do not read the four RAM symbols'
)) {
    if (-not $contractPlain.Contains($required)) { throw "Contract missing required clause: $required" }
}

@(
    'APB_PROBE_CONTRACT=PASS',
    "soc_h_sha256=$socHash",
    "elf_sha256=$elfHash",
    'entry=0xF9000000',
    'load_segments=1',
    ('load_end=0x{0:X8}' -f $loadEnd),
    'apb_address=0xE8100000',
    'apb_offset=0x000',
    'apb_lw_count=1',
    'apb_store_count=0',
    'ram_result_store_count=2',
    'deterministic_halt_pc=0xF90000C4',
    "run_timeout_ms=$expectedTimeoutMs",
    'debugger_direct_apb_read=PROHIBITED',
    'ram_symbols=0xF90000D0,0xF90000D4,0xF90000E4,0xF90000E8'
) | Set-Content -LiteralPath $outputPath -Encoding ascii

"APB_PROBE_CONTRACT=PASS verifier_sha256=$((Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash)"
