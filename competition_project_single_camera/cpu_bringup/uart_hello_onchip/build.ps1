param(
    [switch]$Clean,
    [string]$ToolchainRoot,
    [string]$BuildId
)

$ErrorActionPreference = "Stop"

# --- Publish artifact paths driven from PSScriptRoot ---
$buildDir = Join-Path $PSScriptRoot "build"
$elf = Join-Path $buildDir "uart_hello_onchip.elf"
$hex = Join-Path $buildDir "uart_hello_onchip.hex"
$bin = Join-Path $buildDir "uart_hello_onchip.bin"
$asm = Join-Path $buildDir "uart_hello_onchip.asm"
$map = Join-Path $buildDir "uart_hello_onchip.map"
$publishArtifacts = @($elf, $hex, $bin, $asm, $map)

function Clear-PublishArtifacts {
    foreach ($artifact in $publishArtifacts) {
        if (Test-Path -LiteralPath $artifact) {
            Remove-Item -LiteralPath $artifact -Force
        }
    }
}

# --- Invalidate old publish artifacts BEFORE any validation that may throw ---
Clear-PublishArtifacts

# --- BuildId validation ---
if (-not $BuildId) {
    throw "BUILD_ID is required. Pass -BuildId with a value matching [A-Za-z0-9._-]+"
}
if ($BuildId -notmatch '^[A-Za-z0-9._-]+$') {
    throw "BUILD_ID contains invalid characters: '$BuildId'. Only [A-Za-z0-9._-]+ allowed."
}
if ($BuildId.Length -gt 64) {
    throw "BUILD_ID too long: $($BuildId.Length) characters (max 64)."
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$bspRoot = Join-Path $projectRoot "embedded_sw\efx_hard_soc"

$bspFiles = @(
    "software\standalone\common\bsp.mk",
    "software\standalone\common\riscv64-unknown-elf.mk",
    "software\standalone\common\standalone.mk",
    "software\standalone\common\start.S",
    "bsp\efinix\EfxSapphireSoc\include\soc.mk",
    "bsp\efinix\EfxSapphireSoc\include\soc.h",
    "bsp\efinix\EfxSapphireSoc\linker\default_i.ld"
)

foreach ($relativePath in $bspFiles) {
    $requiredBspFile = Join-Path $bspRoot $relativePath
    if (-not (Test-Path -LiteralPath $requiredBspFile)) {
        throw "Required generated BSP file not found: $requiredBspFile. Regenerate the BSP from ip/EfxSapphireHpSoc_slb/settings.json with Efinity 2025.2.288.4.15 or restore the reviewed minimal BSP set."
    }
}

if (-not $ToolchainRoot) {
    $ToolchainRoot = $env:EFINITY_RISCV_IDE
}

$toolchainBin = if ($ToolchainRoot) {
    Join-Path $ToolchainRoot "toolchain\bin"
} else {
    $null
}
$buildToolsBin = if ($ToolchainRoot) {
    Join-Path $ToolchainRoot "build_tools\bin"
} else {
    $null
}

function Resolve-RequiredTool {
    param(
        [string]$ToolName,
        [string]$PreferredDirectory
    )

    if ($PreferredDirectory) {
        $preferredPath = Join-Path $PreferredDirectory $ToolName
        if (Test-Path -LiteralPath $preferredPath) {
            return $preferredPath
        }
    }

    $command = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw "Required Efinity RISC-V tool not found: $ToolName. Pass -ToolchainRoot, set EFINITY_RISCV_IDE, or add the Efinity RISC-V toolchain to PATH."
}

$make = Resolve-RequiredTool "make.exe" $buildToolsBin
$readelf = Resolve-RequiredTool "riscv-none-embed-readelf.exe" $toolchainBin
$size = Resolve-RequiredTool "riscv-none-embed-size.exe" $toolchainBin
$nm = Resolve-RequiredTool "riscv-none-embed-nm.exe" $toolchainBin

$resolvedToolDirectories = @($make, $readelf, $size, $nm) |
    ForEach-Object { Split-Path -Parent $_ } |
    Select-Object -Unique
$env:PATH = (($resolvedToolDirectories + $env:PATH) -join [IO.Path]::PathSeparator)

Push-Location $PSScriptRoot
try {
    if ($Clean) {
        & $make clean "BUILD_ID=$BuildId"
        if ($LASTEXITCODE -ne 0) {
            throw "make clean failed with exit code $LASTEXITCODE"
        }
    }

    & $make all "BUILD_ID=$BuildId"
    if ($LASTEXITCODE -ne 0) {
        throw "make all failed with exit code $LASTEXITCODE"
    }

    # --- ELF entry audit (readelf -h) ---
    $elfReport = & $readelf -h -l -S $elf
    if ($LASTEXITCODE -ne 0) {
        throw "readelf failed with exit code $LASTEXITCODE"
    }
    $elfReport

    $entryLine = $elfReport | Where-Object { $_ -match 'Entry point address:' }
    if (@($entryLine).Count -ne 1) {
        throw "Unable to locate ELF entry point address in readelf output"
    }
    if ($entryLine -notmatch 'Entry point address:\s+0x(?<entry>[0-9a-fA-F]+)') {
        throw "Unable to parse ELF entry point address: $entryLine"
    }
    $entryAddr = [Convert]::ToUInt64($Matches.entry, 16)
    if ($entryAddr -ne 0xF9000000L) {
        throw ('ELF entry mismatch: expected 0xF9000000, got 0x{0:X8}' -f $entryAddr)
    }
    'ELF_ENTRY_AUDIT=PASS'

    # --- _start symbol audit (nm) ---
    $nmOutput = & $nm $elf
    if ($LASTEXITCODE -ne 0) {
        throw "nm failed with exit code $LASTEXITCODE"
    }
    $startLine = $nmOutput | Where-Object { $_ -match '\s+T\s+_start$' }
    if (-not $startLine) {
        throw "_start symbol not found in ELF symbol table"
    }
    if ($startLine -notmatch '^\s*(?<addr>[0-9a-fA-F]+)\s+T\s+_start') {
        throw "Unable to parse _start address: $startLine"
    }
    $startAddr = [Convert]::ToUInt64($Matches.addr, 16)
    if ($startAddr -ne 0xF9000000L) {
        throw ('_start address mismatch: expected 0xF9000000, got 0x{0:X8}' -f $startAddr)
    }

    # --- LOAD segment audit ---
    $loadLine = $elfReport | Where-Object { $_ -match '^\s*LOAD\s+' }
    if (@($loadLine).Count -ne 1) {
        throw "Expected exactly one ELF LOAD segment, found $(@($loadLine).Count)"
    }

    if ($loadLine -notmatch '^\s*LOAD\s+0x[0-9a-f]+\s+0x(?<start>[0-9a-f]+)\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+0x(?<mem>[0-9a-f]+)') {
        throw "Unable to parse ELF LOAD segment: $loadLine"
    }

    $loadStart = [Convert]::ToUInt64($Matches.start, 16)
    $loadSize = [Convert]::ToUInt64($Matches.mem, 16)
    $loadEnd = $loadStart + $loadSize
    if ($loadStart -ne 0xF9000000L -or $loadEnd -gt 0xF9004000L) {
        throw ('ELF escapes on-chip RAM: start=0x{0:X8} end=0x{1:X8}' -f $loadStart, $loadEnd)
    }

    $undefined = & $nm -u $elf
    if ($LASTEXITCODE -ne 0 -or $undefined) {
        throw "ELF contains undefined symbols: $undefined"
    }
    'ELF_LOAD_AUDIT=PASS start=0x{0:X8} end=0x{1:X8}' -f $loadStart, $loadEnd

    & $size -A -x $elf
    if ($LASTEXITCODE -ne 0) {
        throw "size audit failed with exit code $LASTEXITCODE"
    }
} catch {
    Clear-PublishArtifacts
    throw
} finally {
    Pop-Location
}
