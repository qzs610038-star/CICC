param([string]$ToolchainBin, [string]$OutputDir)

$ErrorActionPreference = 'Stop'
$tests = $PSScriptRoot
$cpu = Split-Path -Parent $tests
$repo = Split-Path -Parent (Split-Path -Parent $cpu)
$profile = Join-Path $tests 'f1_board_selftest'
if (-not $OutputDir) { $OutputDir = Join-Path ([IO.Path]::GetTempPath()) 'cicc-f1-riscv-profile' }
$build = $OutputDir

if (-not $ToolchainBin) {
    $gccCommand = Get-Command riscv-none-embed-gcc -ErrorAction SilentlyContinue
    if ($gccCommand) { $ToolchainBin = Split-Path -Parent $gccCommand.Source }
    elseif (Test-Path -LiteralPath 'D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-gcc.exe') {
        $ToolchainBin = 'D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin'
    }
}
if (-not $ToolchainBin) { Write-Error 'TOOLCHAIN_BLOCKED: riscv-none-embed toolchain not found'; exit 2 }

$gcc = Join-Path $ToolchainBin 'riscv-none-embed-gcc.exe'
$readelf = Join-Path $ToolchainBin 'riscv-none-embed-readelf.exe'
$objdump = Join-Path $ToolchainBin 'riscv-none-embed-objdump.exe'
$nm = Join-Path $ToolchainBin 'riscv-none-embed-nm.exe'
foreach ($tool in @($gcc,$readelf,$objdump,$nm)) {
    if (-not (Test-Path -LiteralPath $tool)) { Write-Error "TOOLCHAIN_BLOCKED: $tool"; exit 2 }
}

New-Item -ItemType Directory -Force -Path $build | Out-Null
$elf = Join-Path $build 'f1_selftest_profile.elf'
$map = Join-Path $build 'f1_selftest_profile.map'
$normalizedMap = Join-Path $build 'map.normalized.txt'
$readelfOut = Join-Path $build 'readelf.txt'
$normalizedReadelf = Join-Path $build 'readelf.normalized.txt'
$objdumpOut = Join-Path $build 'objdump.txt'
$normalizedObjdump = Join-Path $build 'objdump.normalized.txt'
$symbolsOut = Join-Path $build 'symbols.txt'
$identityOut = Join-Path $build 'identity.json'
$include = Join-Path $cpu 'include'
$src = Join-Path $cpu 'src'
$sources = @(
    (Join-Path $profile 'profile_start.c'), (Join-Path $profile 'freestanding_support.c'),
    (Join-Path $src 'f1_board_selftest.c'), (Join-Path $src 'single_camera_classifier.c'),
    (Join-Path $src 'single_camera_feature_adapter.c'), (Join-Path $src 'single_camera_f1.c'),
    (Join-Path $src 'single_camera_runtime.c')
)
$flags = @('-march=rv32im','-mabi=ilp32','-std=c11','-Os','-ffreestanding','-fno-builtin',
    '-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror',"-I$include",("-I" + $src),
    '-nostartfiles','-nostdlib',('-T' + (Join-Path $profile 'profile.ld')),
    ('-Wl,-Map=' + $map),'-Wl,--gc-sections','-Wl,--build-id=none')
& $gcc @flags @sources '-lgcc' '-o' $elf
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $readelf -h -l -S -s $elf | Set-Content -Encoding ascii $readelfOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $objdump -d $elf | Set-Content -Encoding ascii $objdumpOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $nm -n $elf | Set-Content -Encoding ascii $symbolsOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$mapText = Get-Content -Raw -LiteralPath $map
$buildPattern = [regex]::Escape($build)
$mapText = $mapText -replace $buildPattern,'<OUTPUT_DIR>'
$mapText = $mapText -replace '(?i)[A-Z]:\\[^\r\n ]*\\Temp\\cc[A-Za-z0-9]+\.o','<TMP_OBJECT>'
$mapText = $mapText -replace '(?i)[A-Z]:/[^\r\n ]*/Temp/cc[A-Za-z0-9]+\.o','<TMP_OBJECT>'
$mapText | Set-Content -NoNewline -Encoding ascii $normalizedMap
$readelfText = (Get-Content -Raw -LiteralPath $readelfOut) -replace $buildPattern,'<OUTPUT_DIR>'
$readelfText | Set-Content -NoNewline -Encoding ascii $normalizedReadelf
$objdumpText = (Get-Content -Raw -LiteralPath $objdumpOut) -replace $buildPattern,'<OUTPUT_DIR>'
$objdumpText | Set-Content -NoNewline -Encoding ascii $normalizedObjdump

function Symbol([string]$name) {
    $line = Get-Content $symbolsOut | Where-Object { $_ -match ("\s" + [regex]::Escape($name) + '$') } | Select-Object -First 1
    if (-not $line) { throw "PROFILE_SYMBOL_MISSING: $name" }
    [Convert]::ToUInt32(($line -split '\s+')[0],16)
}
$entry = Symbol '_start'
$bssStart = Symbol '__bss_start'
$bssEnd = Symbol '__bss_end'
$stackBottom = Symbol '__stack_bottom'
$stackTop = Symbol '__stack_top'
$imageEnd = Symbol '__image_end'
if ($entry -ne 0 -or $imageEnd -gt 16384 -or ($stackTop - $stackBottom) -ne 2048) {
    throw 'PROFILE_BUDGET_OR_LAYOUT_FAILED'
}
$objdumpText = Get-Content -Raw $normalizedObjdump
foreach ($forbidden in @('e801','f900','uart','mycobot','soc.h')) {
    if ($objdumpText -match [regex]::Escape($forbidden)) { throw "PROFILE_FORBIDDEN_REFERENCE: $forbidden" }
}
$inputHashes = [ordered]@{}
foreach ($path in $sources + @((Join-Path $profile 'profile.ld'))) {
    $resolved = (Resolve-Path -LiteralPath $path).Path
    $repoPrefix = (Resolve-Path -LiteralPath $repo).Path.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "PROFILE_INPUT_OUTSIDE_REPO: $resolved"
    }
    $relative = $resolved.Substring($repoPrefix.Length).Replace('\','/')
    $inputHashes[$relative] = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
}
$identity = [ordered]@{
    schema='QW-F1-RISCV-PROFILE-v1'; profile_only=$true; board_elf=$false
    architecture='rv32im/ilp32'; entry=('0x{0:X8}' -f $entry); load_origin='0x00000000'
    image_bytes=$imageEnd; bss_bytes=($bssEnd-$bssStart); stack_bytes=($stackTop-$stackBottom)
    budget_bytes=16384; budget_pass=($imageEnd -le 16384); arm_enabled=0
    mmio_addresses=@(); uart_binding=$null; bsp_binding=$null
    elf_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $elf).Hash
    raw_map_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $map).Hash
    map_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $normalizedMap).Hash
    map_identity='map.normalized.txt; compiler temporary object names replaced with <TMP_OBJECT>'
    readelf_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $normalizedReadelf).Hash
    readelf_identity='readelf.normalized.txt; output directory replaced with <OUTPUT_DIR>'
    objdump_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $normalizedObjdump).Hash
    objdump_identity='objdump.normalized.txt; output directory replaced with <OUTPUT_DIR>'
    inputs=$inputHashes
}
$identity | ConvertTo-Json -Depth 6 | Set-Content -Encoding ascii $identityOut
Write-Output ("QW-F1-RISCV-PROFILE-v1 PASS image={0}/16384 stack={1} elf={2}" -f $imageEnd,($stackTop-$stackBottom),$identity.elf_sha256)
