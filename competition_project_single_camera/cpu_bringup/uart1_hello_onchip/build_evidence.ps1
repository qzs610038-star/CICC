param(
    [Parameter(Mandatory = $true)]
    [string]$ToolchainRoot
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$evidence = Join-Path $root 'evidence'
$bin = Join-Path $ToolchainRoot 'toolchain\bin'
$make = Join-Path $ToolchainRoot 'build_tools\bin\make.exe'
$gcc = Join-Path $bin 'riscv-none-embed-gcc.exe'
$readelf = Join-Path $bin 'riscv-none-embed-readelf.exe'
$objdump = Join-Path $bin 'riscv-none-embed-objdump.exe'
$nm = Join-Path $bin 'riscv-none-embed-nm.exe'
$elf = Join-Path $root 'build\p0a_uart1_diag.elf'
$map = Join-Path $root 'build\p0a_uart1_diag.map'

foreach ($tool in @($make, $gcc, $readelf, $objdump, $nm)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "P0A_TOOL_MISSING: $tool"
    }
}

New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$env:Path = "$bin;$(Join-Path $ToolchainRoot 'build_tools\bin');$env:Path"
Push-Location $root
try {
    & $make clean 2>&1 | Tee-Object (Join-Path $evidence 'clean.log')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $make all 2>&1 | Tee-Object (Join-Path $evidence 'build.log')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $readelf -h -l -S -s $elf | ForEach-Object { $_.TrimEnd() } |
        Set-Content -Encoding ascii (Join-Path $evidence 'readelf.txt')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $objdumpText = (& $objdump -d -S $elf) -join "`n"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    ($objdumpText -replace [regex]::Escape($root), '<WORKTREE>') |
        Set-Content -Encoding ascii (Join-Path $evidence 'disassembly.txt')
    & $nm -n $elf | Set-Content -Encoding ascii (Join-Path $evidence 'symbols.txt')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $mapText = Get-Content -Raw -LiteralPath $map
    $mapText = $mapText -replace [regex]::Escape($root), '<WORKTREE>'
    $toolchainPattern = [regex]::Escape($ToolchainRoot) -replace '\\\\', '[\\/]'
    $mapText = $mapText -replace $toolchainPattern, '<TOOLCHAIN>'
    $mapText | Set-Content -Encoding ascii (Join-Path $evidence 'p0a_uart1_diag.map')
    $evidenceElf = Join-Path $evidence 'p0a_uart1_diag.elf'
    Copy-Item -LiteralPath $elf -Destination $evidenceElf -Force

    $inputs = @(
        'makefile', 'linker\p0a_diag.ld', 'src\main.c', 'src\p0a_diag.c',
        'src\p0a_diag.h',
        '..\..\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h',
        '..\..\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\linker\default_i.ld',
        '..\..\embedded_sw\efx_hard_soc\software\standalone\common\start.S'
    )
    $hashLines = foreach ($path in $inputs) {
        $resolved = (Resolve-Path -LiteralPath (Join-Path $root $path)).Path
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolved).Hash
        "$hash  $path"
    }
    $hashLines | Set-Content -Encoding ascii (Join-Path $evidence 'input_hashes.sha256')
    Get-FileHash -Algorithm SHA256 -LiteralPath $evidenceElf |
        ForEach-Object { "$($_.Hash)  evidence\p0a_uart1_diag.elf" } |
        Set-Content -Encoding ascii (Join-Path $evidence 'artifact_hashes.sha256')

    $symbols = Get-Content (Join-Path $evidence 'symbols.txt')
    $required = @('_start', 'main', 'g_p0a_canary', '__p0a_canary_start',
                  '__p0a_canary_end', '__stack_bottom', '__stack_top')
    foreach ($name in $required) {
        if (-not ($symbols | Select-String -SimpleMatch $name)) {
            throw "P0A_SYMBOL_MISSING: $name"
        }
    }
    $summary = @(
        'P0-A-READY CANDIDATE / OFFLINE ONLY / BOARD NOT VERIFIED',
        'entry=0xF9000000',
        'ram=0xF9000000..0xF9004000 (16 KiB)',
        'canary=resolved from current ELF/map; see symbols.txt',
        'stack=resolved from current ELF/map; see symbols.txt',
        'uart_poll=bounded; TX timeout records E101 and continues memory heartbeat',
        'ARM_ENABLED=0',
        'non_claims=RISC-V execution, MMIO, APB, UART1 terminal, OSD, USER2, board PASS'
    )
    $summary | Set-Content -Encoding ascii (Join-Path $evidence 'P0A_READY_CANDIDATE.txt')
}
finally {
    Pop-Location
}
