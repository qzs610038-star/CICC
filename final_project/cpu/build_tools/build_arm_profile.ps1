Param(
    [ValidateSet('competition', 'arm_bringup')][string]$Profile = 'competition',
    [ValidateSet('disabled', 'simulated', 'readonly', 'real')][string]$Backend = 'disabled',
    [string]$OutputRoot,
    [switch]$BoardBuild,
    [string]$SocHPath,
    [string]$LinkerPath,
    [string]$StartupPath,
    [string]$ReviewPacketPath,
    [string]$ToolchainPath = $env:EFINITY_RISCV_TOOLCHAIN
)

$ErrorActionPreference = 'Stop'
$cpuRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $cpuRoot '..\..')).Path
$appDir = Join-Path $cpuRoot 'app'
$srcDir = Join-Path $appDir 'src'
$paramsDir = Join-Path $cpuRoot 'params'

if (-not $ToolchainPath -and $env:EFINITY_HOME) {
    $candidate = Join-Path $env:EFINITY_HOME 'toolchain'
    if (Test-Path -LiteralPath $candidate) { $ToolchainPath = $candidate }
}
if (-not $ToolchainPath) {
    $compiler = Get-Command 'riscv-none-embed-gcc.exe' -ErrorAction SilentlyContinue
    if ($compiler) {
        $ToolchainPath = Split-Path -Parent (Split-Path -Parent $compiler.Source)
    }
}
if (-not $ToolchainPath) {
    throw 'RISC-V toolchain not found. Pass -ToolchainPath or set EFINITY_RISCV_TOOLCHAIN.'
}
$ToolchainPath = [IO.Path]::GetFullPath($ToolchainPath)

# G4 owns generated SoC inputs and the separate board-policy verifier.  This
# G0-G3 builder fails before creating any directory or artifact on BoardBuild.
if ($BoardBuild) {
    throw 'BoardBuild is blocked before G4: no ELF/HEX/BIN is permitted from this G0-G3 builder.'
}
if ($Backend -in @('readonly', 'real')) {
    if ($Backend -eq 'real' -and
        (-not $ReviewPacketPath -or -not (Test-Path -LiteralPath $ReviewPacketPath))) {
        throw 'real backend requires an existing GO Review Packet; G0-G3 has none.'
    }
    throw "$Backend backend is intentionally unavailable before its safety gate."
}

$gcc = Join-Path $ToolchainPath 'bin\riscv-none-embed-gcc.exe'
$objcopy = Join-Path $ToolchainPath 'bin\riscv-none-embed-objcopy.exe'
$objdump = Join-Path $ToolchainPath 'bin\riscv-none-embed-objdump.exe'
$nm = Join-Path $ToolchainPath 'bin\riscv-none-embed-nm.exe'
foreach ($tool in @($gcc, $objcopy, $objdump, $nm)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "RISC-V tool missing: $tool"
    }
}

$allowedWarningPattern = 'warning: #warning "APB3 base address not provided by soc\.h'
$observedAllowedWarnings = [System.Collections.Generic.List[string]]::new()
function Invoke-StrictNative([string]$Label, [string]$Tool, [string[]]$Arguments) {
    # Windows PowerShell may turn native stderr records into terminating
    # NativeCommandError objects when the script-wide preference is Stop.
    # Capture the complete compiler diagnostic first, then enforce the exit
    # code and warning allowlist ourselves below.
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $Tool @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $outputText = @($output | ForEach-Object { [string]$_ })
    $warningLines = @($outputText | Where-Object { $_ -match '(?i)\bwarning:' })
    $unexpectedWarnings = @($warningLines | Where-Object {
        $_ -notmatch $allowedWarningPattern
    })

    foreach ($line in $warningLines) {
        if ($line -match $allowedWarningPattern -and
            -not $observedAllowedWarnings.Contains($line)) {
            $observedAllowedWarnings.Add($line)
        }
    }
    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode.`n$($outputText -join "`n")"
    }
    if ($unexpectedWarnings.Count -ne 0) {
        throw "$Label emitted unexpected warning(s):`n$($unexpectedWarnings -join "`n")"
    }
    return ,$outputText
}

function Get-HashedFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "Cannot hash missing file: $Path" }
    return [ordered]@{
        path = [IO.Path]::GetFullPath($Path)
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
    }
}

$notForFlash = $true
$LinkerPath = Join-Path $appDir 'linker\linker.ld'
$StartupPath = Join-Path $appDir 'startup\startup_qcrv32.S'
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $cpuRoot 'build\arm_profiles'
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)

$profileDefine = if ($Profile -eq 'competition') {
    'ARM_PROFILE_COMPETITION'
} else {
    'ARM_PROFILE_ARM_BRINGUP'
}
$backendDefine = if ($Backend -eq 'disabled') {
    'ARM_BACKEND_DISABLED'
} else {
    'ARM_BACKEND_SIMULATED'
}
$sources = if ($Profile -eq 'competition') {
    @(
        (Join-Path $srcDir 'main.c'),
        (Join-Path $srcDir 'board_io.c'),
        (Join-Path $srcDir 'vision_classifier.c'),
        (Join-Path $srcDir 'param_table.c'),
        (Join-Path $srcDir 'task_matcher.c'),
        (Join-Path $srcDir 'round_controller.c'),
        (Join-Path $srcDir 'cpu_result_semantics.c'),
        (Join-Path $srcDir 'cpu_result_semantics_adapters.c'),
        (Join-Path $srcDir 'main_loop_adapter.c'),
        (Join-Path $srcDir 'arm_runtime.c')
    )
} else {
    @(
        (Join-Path $appDir 'profiles\arm_bringup_main.c'),
        (Join-Path $srcDir 'round_controller.c'),
        (Join-Path $srcDir 'arm_runtime.c')
    )
}
if ($Backend -eq 'simulated') {
    $sources += @(
        (Join-Path $srcDir 'arm_controller.c'),
        (Join-Path $srcDir 'arm_sim_transport.c'),
        (Join-Path $paramsDir 'arm_positions.c')
    )
}
foreach ($source in @($sources + @($StartupPath, $LinkerPath))) {
    if (-not (Test-Path -LiteralPath $source)) { throw "Required build input is missing: $source" }
}

$commit = (& git -C $repoRoot rev-parse --short HEAD).Trim()
$dirty = [bool]((& git -C $repoRoot status --porcelain).Length)
$buildId = (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + $commit
$artifactBase = "${Profile}_${Backend}_${buildId}"
$buildDir = Join-Path $OutputRoot $artifactBase
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$elf = Join-Path $buildDir ($artifactBase + '.elf')
$map = Join-Path $buildDir ($artifactBase + '.elf.map')
$hex = Join-Path $buildDir ($artifactBase + '.hex')
$bin = Join-Path $buildDir ($artifactBase + '.bin')
$lst = Join-Path $buildDir ($artifactBase + '.lst')
$manifest = Join-Path $buildDir ($artifactBase + '.manifest.json')

$cflags = @(
    '-march=rv32imac', '-mabi=ilp32', '-mcmodel=medlow', '-O2', '-g',
    '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra',
    '-Werror', '-Wno-error=cpp',
    ("-I" + (Join-Path $appDir 'include')), ("-I" + $paramsDir),
    ("-DAPP_PROFILE=" + $profileDefine), ("-DARM_BACKEND=" + $backendDefine),
    ('-DARM_BUILD_ID=\"' + $buildId + '\"'),
    '-DAPB_VISION_BASE_PLACEHOLDER=0xF0000000u'
)
$ldflags = @(
    '-march=rv32imac', '-mabi=ilp32', '-mcmodel=medlow', '-nostartfiles',
    '-Wl,--gc-sections', '-Wl,--fatal-warnings', ("-Wl,-Map=" + $map),
    '-T', $LinkerPath
)

$objects = @()
for ($i = 0; $i -lt $sources.Count; ++$i) {
    $source = $sources[$i]
    $object = Join-Path $buildDir (("{0:D2}_" -f $i) +
        [IO.Path]::GetFileNameWithoutExtension($source) + '.o')
    Invoke-StrictNative "Compile $source" $gcc ($cflags + @('-c', $source, '-o', $object)) | Out-Null
    $objects += $object
}
$startupObject = Join-Path $buildDir 'startup_qcrv32.o'
Invoke-StrictNative "Compile $StartupPath" $gcc ($cflags + @('-c', $StartupPath, '-o', $startupObject)) | Out-Null
$objects += $startupObject

Invoke-StrictNative 'RISC-V link' $gcc ($ldflags + @('-o', $elf) + $objects) | Out-Null
Invoke-StrictNative 'HEX conversion' $objcopy @('-O', 'ihex', $elf, $hex) | Out-Null
Invoke-StrictNative 'BIN conversion' $objcopy @('-O', 'binary', $elf, $bin) | Out-Null
$listing = Invoke-StrictNative 'Disassembly' $objdump @('-d', '-S', $elf)
$listing | Set-Content -LiteralPath $lst -Encoding utf8

$sourceManifest = @()
foreach ($source in ($sources + @($StartupPath, $LinkerPath) | Select-Object -Unique)) {
    $sourceManifest += Get-HashedFile $source
}
$artifactHashes = [ordered]@{
    elf = Get-HashedFile $elf
    map = Get-HashedFile $map
    hex = Get-HashedFile $hex
    bin = Get-HashedFile $bin
    lst = Get-HashedFile $lst
}
$manifestData = [ordered]@{
    schema = 'mycobot-g1-g3-manifest-v2'
    canonical_builder = 'final_project/cpu/build_tools/build_arm_profile.ps1'
    build_id = $buildId
    timestamp = (Get-Date).ToString('o')
    commit = $commit
    dirty = $dirty
    profile = $Profile
    backend = $Backend
    not_for_flash = $notForFlash
    board_build = $false
    entry_symbol = '_start'
    soc_truth_source = 'placeholder-standalone-test'
    compile_flags = $cflags
    link_flags = $ldflags
    source_files = $sourceManifest
    startup = Get-HashedFile $StartupPath
    linker = Get-HashedFile $LinkerPath
    toolchain = (& $gcc '--version' | Select-Object -First 1)
    march = 'rv32imac'
    mabi = 'ilp32'
    warning_policy = [ordered]@{
        allowed_patterns = @($allowedWarningPattern)
        observed_warning_lines = @($observedAllowedWarnings | Select-Object -Unique)
    }
    artifact_sha256 = $artifactHashes
}
$manifestData | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifest -Encoding utf8

& (Join-Path $PSScriptRoot 'verify_arm_elf.ps1') -Elf $elf -Profile $Profile `
    -Backend $Backend -Manifest $manifest -NmPath $nm -Map $map -Listing $lst
if ($LASTEXITCODE -ne 0) { throw 'ELF verification failed.' }

Write-Output "BUILD PASS: $elf"
Write-Output "MANIFEST: $manifest"
