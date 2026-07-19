[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$ToolchainRoot
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $projectRoot '..\..\..')).Path
$toolchainBin = Join-Path $ToolchainRoot 'toolchain\bin'
$make = Join-Path $ToolchainRoot 'build_tools\bin\make.exe'
$readelf = Join-Path $toolchainBin 'riscv-none-embed-readelf.exe'
$objdump = Join-Path $toolchainBin 'riscv-none-embed-objdump.exe'
$nm = Join-Path $toolchainBin 'riscv-none-embed-nm.exe'
$artifactRoot = Join-Path $projectRoot 'artifacts'
$buildRoot = Join-Path $projectRoot 'build'
$elf = Join-Path $buildRoot 'apb_magic_onchip.elf'
$artifactElf = Join-Path $artifactRoot 'apb_magic_onchip.elf'
$readelfOutput = Join-Path $artifactRoot 'apb_magic_onchip.readelf.txt'
$disassemblyOutput = Join-Path $artifactRoot 'apb_magic_onchip.objdump.txt'
$undefinedOutput = Join-Path $artifactRoot 'apb_magic_onchip.undefined.txt'
$symbolsOutput = Join-Path $artifactRoot 'apb_magic_onchip.symbols.txt'
$hashOutput = Join-Path $artifactRoot 'apb_magic_onchip.sha256.txt'

$resolvedProjectRoot = [IO.Path]::GetFullPath($projectRoot).TrimEnd('\')
$resolvedBuildRoot = [IO.Path]::GetFullPath($buildRoot)
if (-not $resolvedBuildRoot.StartsWith($resolvedProjectRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Build root escapes the APB probe directory.'
}

foreach ($tool in @($make, $readelf, $objdump, $nm)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Missing tool: $tool"
    }
}

if ((git -C $repoRoot status --porcelain --untracked-files=no) -match 'CPU_MODULE_PLAN.txt') {
    throw 'Refusing to run with a tracked CPU_MODULE_PLAN.txt modification in this worktree.'
}

$savedPath = $env:PATH
try {
    $env:PATH = "$toolchainBin;$(Split-Path -Parent $make);$savedPath"
    if (Test-Path -LiteralPath $buildRoot) {
        Remove-Item -LiteralPath $buildRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path (Join-Path $buildRoot 'obj_files') -Force | Out-Null
    Push-Location $projectRoot
    & $make
    if ($LASTEXITCODE -ne 0) { throw "make failed: $LASTEXITCODE" }
    Pop-Location
} finally {
    if ((Get-Location).Path -eq $projectRoot) { Pop-Location }
    $env:PATH = $savedPath
}

New-Item -ItemType Directory -Force -Path $artifactRoot | Out-Null
Copy-Item -LiteralPath $elf -Destination $artifactElf -Force
& $readelf -h -l $artifactElf |
    ForEach-Object { $_.TrimEnd() } |
    Set-Content -LiteralPath $readelfOutput -Encoding ascii
if ($LASTEXITCODE -ne 0) { throw "readelf failed: $LASTEXITCODE" }
& $objdump -d -S $artifactElf | Set-Content -LiteralPath $disassemblyOutput -Encoding ascii
if ($LASTEXITCODE -ne 0) { throw "objdump failed: $LASTEXITCODE" }
@(& $nm -u $artifactElf) | Set-Content -LiteralPath $undefinedOutput -Encoding ascii
if ($LASTEXITCODE -ne 0) { throw "nm undefined scan failed: $LASTEXITCODE" }
if (-not (Test-Path -LiteralPath $undefinedOutput)) {
    New-Item -ItemType File -Path $undefinedOutput | Out-Null
}
& $nm -n $artifactElf | Set-Content -LiteralPath $symbolsOutput -Encoding ascii
if ($LASTEXITCODE -ne 0) { throw "nm symbol scan failed: $LASTEXITCODE" }

$readelfText = Get-Content -LiteralPath $readelfOutput -Raw
if ($readelfText -notmatch 'Entry point address:\s+0xf9000000') {
    throw 'ELF entry is not 0xF9000000.'
}
$loadLines = @($readelfText -split "`r?`n" | Where-Object { $_ -match '^\s*LOAD\s+' })
if ($loadLines.Count -ne 1) { throw "Expected one LOAD segment, found $($loadLines.Count)." }
if ($loadLines[0] -notmatch '0xf9000000\s+0xf9000000') {
    throw 'LOAD segment does not start at 0xF9000000.'
}
$loadFields = @($loadLines[0] -split '\s+' | Where-Object { $_ })
$fileSize = [Convert]::ToUInt32($loadFields[4].Substring(2), 16)
$memorySize = [Convert]::ToUInt32($loadFields[5].Substring(2), 16)
$loadEnd = 0xF9000000L + [Math]::Max($fileSize, $memorySize)
if ($loadEnd -gt 0xF9004000L) { throw ('LOAD exceeds on-chip RAM: 0x{0:X8}' -f $loadEnd) }
if ((Get-Item -LiteralPath $undefinedOutput).Length -ne 0) { throw 'ELF contains undefined symbols.' }

$disassembly = Get-Content -LiteralPath $disassemblyOutput -Raw
$mainMatch = [regex]::Match($disassembly, '(?ms)^([0-9a-f]+) <main>:\r?\n(?<body>.*?)(?=^[0-9a-f]+ <|\z)')
if (-not $mainMatch.Success) { throw 'Unable to isolate main disassembly.' }
$mainBody = $mainMatch.Groups['body'].Value
if ($mainBody -notmatch '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+lw\s+[^\r\n]*\(.*\)') {
    throw 'main does not contain the expected 32-bit load.'
}
if (([regex]::Matches($mainBody, '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+lw\s+')).Count -ne 1) {
    throw 'main must contain exactly one 32-bit load instruction.'
}
if ($mainBody -notmatch '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+lui\s+[^,]+,0xe8100\s*$') {
    throw 'main does not materialize the soc.h APB base 0xE8100000.'
}
if ($mainBody -notmatch '(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+lw\s+[^,]+,0\([^\)]+\)\s*$') {
    throw 'main does not read offset 0 with a 32-bit load.'
}
$symbols = Get-Content -LiteralPath $symbolsOutput -Raw
foreach ($symbol in @('g_apb_probe_address','g_apb_probe_expected','g_apb_probe_observed','g_apb_probe_status')) {
    $pattern = '(?m)^(?<address>[0-9a-fA-F]{8})\s+\S\s+' +
        [regex]::Escape($symbol) + '\s*$'
    $match = [regex]::Match($symbols, $pattern)
    if (-not $match.Success) { throw "Missing probe symbol: $symbol" }
    $address = [Convert]::ToUInt32($match.Groups['address'].Value, 16)
    if ($address -lt 0xF9000000L -or $address -ge 0xF9004000L) {
        throw "Probe symbol outside on-chip RAM: $symbol"
    }
}

@(
    "apb_magic_onchip.elf  $((Get-FileHash -LiteralPath $artifactElf -Algorithm SHA256).Hash)",
    "soc.h  $((Get-FileHash -LiteralPath (Join-Path $repoRoot 'competition_project_single_camera\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h') -Algorithm SHA256).Hash)",
    "source_main.c  $((Get-FileHash -LiteralPath (Join-Path $projectRoot 'src\main.c') -Algorithm SHA256).Hash)",
    "makefile  $((Get-FileHash -LiteralPath (Join-Path $projectRoot 'makefile') -Algorithm SHA256).Hash)",
    "evidence_script  $((Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash)",
    ('entry  0xF9000000'),
    ('load_end  0x{0:X8}' -f $loadEnd),
    ('apb_base_from_soc_h  0xE8100000'),
    ('expected_magic  0x375A0001'),
    ('main_lw_count  1'),
    ('apb_write_count  0')
) | Set-Content -LiteralPath $hashOutput -Encoding ascii

"APB_PROBE_EVIDENCE=PASS elf_sha256=$((Get-FileHash -LiteralPath $artifactElf -Algorithm SHA256).Hash) load_end=$('0x{0:X8}' -f $loadEnd)"
