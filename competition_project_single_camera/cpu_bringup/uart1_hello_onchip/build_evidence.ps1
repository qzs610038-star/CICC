param(
    [Parameter(Mandatory = $true)][string]$ToolchainRoot,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-f]{40}$')][string]$FirmwareGitSha,
    [string]$PythonExe,
    [string]$VerifierPath
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$evidence = Join-Path $root 'evidence'
$bin = Join-Path $ToolchainRoot 'toolchain\bin'
$make = Join-Path $ToolchainRoot 'build_tools\bin\make.exe'
$readelf = Join-Path $bin 'riscv-none-embed-readelf.exe'
$objdump = Join-Path $bin 'riscv-none-embed-objdump.exe'
$objcopy = Join-Path $bin 'riscv-none-embed-objcopy.exe'
$nm = Join-Path $bin 'riscv-none-embed-nm.exe'
$debugElf = Join-Path $root 'build\p0a_uart1_diag.elf'
$rawMap = Join-Path $root 'build\p0a_uart1_diag.map'
$diagnosticElf = Join-Path $evidence 'diagnostic.elf'

foreach ($tool in @($make,$readelf,$objdump,$objcopy,$nm)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "P0A_TOOL_MISSING: $tool" }
}

$inputs = @(
    'makefile','linker\p0a_diag.ld','src\main.c','src\p0a_diag.c','src\p0a_diag.h',
    '..\..\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h',
    '..\..\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\linker\default_i.ld',
    '..\..\embedded_sw\efx_hard_soc\software\standalone\common\start.S'
)
$inputLines = foreach ($path in $inputs) {
    $resolved = (Resolve-Path -LiteralPath (Join-Path $root $path)).Path
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolved).Hash
    "$hash  $path"
}
$normalizedInput = ($inputLines -join "`n") + "`n"
$sha = [Security.Cryptography.SHA256]::Create()
try { $inputHash = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($normalizedInput)))).Replace('-','') }
finally { $sha.Dispose() }
$buildIdHex = $inputHash.Substring(0,8)

New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$inputLines | Set-Content -Encoding ascii (Join-Path $evidence 'input_hashes.sha256')
$env:Path = "$bin;$(Join-Path $ToolchainRoot 'build_tools\bin');$env:Path"
$prefixFlags = "-DP0A_BUILD_ID=0x${buildIdHex}u -ffile-prefix-map=.=WSC_P0A_SOURCE -fdebug-prefix-map=.=WSC_P0A_SOURCE"
Push-Location $root
try {
    & $make clean 2>&1 | ForEach-Object { $_.ToString().TrimEnd() } |
        Set-Content -Encoding ascii (Join-Path $evidence 'clean.txt')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $make "CFLAGS_ARGS=$prefixFlags" all 2>&1 |
        ForEach-Object { $_.ToString().TrimEnd() } |
        Tee-Object (Join-Path $evidence 'strict-build.txt')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $objcopy --strip-debug $debugElf $diagnosticElf
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $readelf -h -l -S -s $diagnosticElf | ForEach-Object { $_.TrimEnd() } |
        Set-Content -Encoding ascii (Join-Path $evidence 'readelf-lW.txt')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $objdumpText = (& $objdump -d $diagnosticElf) -join "`n"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $objdumpText = $objdumpText -replace [regex]::Escape($diagnosticElf), 'diagnostic.elf'
    $objdumpText | Set-Content -Encoding ascii (Join-Path $evidence 'objdump-d.txt')
    & $nm -n $diagnosticElf | Set-Content -Encoding ascii (Join-Path $evidence 'symbols.txt')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $mapText = Get-Content -Raw -LiteralPath $rawMap
    $mapText = $mapText -replace [regex]::Escape($root), '<WORKTREE>'
    $toolchainPattern = [regex]::Escape($ToolchainRoot) -replace '\\\\','[\\/]'
    $mapText = $mapText -replace $toolchainPattern, '<TOOLCHAIN>'
    $mapLines = @($mapText -split "`r?`n")
    $debugStart = [Array]::FindIndex($mapLines, [Predicate[string]]{ param($line) $line -match '^\.debug_' })
    if ($debugStart -gt 0) { $mapLines = @($mapLines[0..($debugStart - 1)]) }
    (($mapLines | ForEach-Object { $_.TrimEnd() }) -join "`n") |
        Set-Content -NoNewline -Encoding ascii (Join-Path $evidence 'diagnostic-map.txt')

    $witness = Join-Path $evidence 'tx-never-ready.json'
    & (Join-Path $root 'run_host_tests.ps1') -WitnessPath $witness 2>&1 |
        ForEach-Object { $_.ToString().TrimEnd() } |
        Set-Content -Encoding ascii (Join-Path $evidence 'tx-never-ready.txt')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $symbols = Get-Content (Join-Path $evidence 'symbols.txt')
    function SymbolValue([string]$name) {
        $line = $symbols | Where-Object { $_ -match ("\s" + [regex]::Escape($name) + '$') } | Select-Object -First 1
        if ($null -eq $line) { throw "P0A_SYMBOL_MISSING: $name" }
        return [Convert]::ToUInt32(($line -split '\s+')[0],16)
    }
    $canaryStart = SymbolValue '__p0a_canary_start'
    $canaryEnd = SymbolValue '__p0a_canary_end'
    $stackStart = SymbolValue '__stack_bottom'
    $stackEnd = SymbolValue '__stack_top'
    $dataStart = SymbolValue '_start'
    $dataEnd = $canaryStart

    $allLines = $objdumpText -split "`n"
    $probeStart = [Array]::FindIndex($allLines, [Predicate[string]]{ param($line) $line -match '<p0a_run_uart_probe>:$' })
    if ($probeStart -lt 0) { throw 'P0A_PROBE_FUNCTION_MISSING' }
    $probeEnd = $allLines.Length
    for ($index = $probeStart + 1; $index -lt $allLines.Length; $index++) {
        if ($allLines[$index] -match '^[0-9a-f]+ <[^>]+>:$') { $probeEnd = $index; break }
    }
    $probeLines = @($allLines[$probeStart..($probeEnd - 1)])
    $markCalls = @()
    $waitCalls = @()
    $initCalls = @()
    foreach ($line in $probeLines) {
        if ($line -match '^\s*([0-9a-f]+):.*<p0a_canary_mark>') { $markCalls += ('0x' + $Matches[1]) }
        if ($line -match '^\s*([0-9a-f]+):.*<p0a_uart_write_bounded>') { $waitCalls += ('0x' + $Matches[1]) }
        if ($line -match '^\s*([0-9a-f]+):.*\bjalr\b') { $initCalls += ('0x' + $Matches[1]) }
    }
    if ($markCalls.Count -lt 4 -or $waitCalls.Count -lt 1 -or $initCalls.Count -lt 1) { throw 'P0A_ORDER_WITNESS_MISSING' }
    $stageCalls = @($markCalls | Select-Object -First 3)
    $uartCall = @($waitCalls | Select-Object -First 1)
    $orderDetails = [ordered]@{
        schema='p0-a-disassembly-order-v1'
        function='p0a_run_uart_probe'
        c003_mark=$stageCalls[0]
        uart_config_call=$initCalls[0]
        c004_mark=$stageCalls[1]
        c005_mark=$stageCalls[2]
        first_tx_call=$uartCall[0]
        relation='C003 < UART_CONFIG < C004 < C005 < FIRST_TX'
    }
    $orderDetails | ConvertTo-Json | Set-Content -Encoding ascii (Join-Path $evidence 'disassembly-order.json')

    $artifactPaths = [ordered]@{
        elf='diagnostic.elf'; map='diagnostic-map.txt'; readelf='readelf-lW.txt';
        objdump='objdump-d.txt'; build_log='strict-build.txt'; tx_never_ready='tx-never-ready.json'
    }
    $artifacts = [ordered]@{}
    foreach ($entry in $artifactPaths.GetEnumerator()) {
        $path = Join-Path $evidence $entry.Value
        $artifacts[$entry.Key] = [ordered]@{path=$entry.Value;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash}
    }
    $manifest = [ordered]@{
        schema='p0-a-evidence-v1'; status='AWAITING_QZS_REVIEW'
        firmware=[ordered]@{
            git_sha=$FirmwareGitSha; input_sha256=$inputHash; build_id_rule='uint32_be(SHA256(normalized input hash lines)[0:4])'
            build_id_hex=('0x' + $buildIdHex.ToUpper()); debug_elf_identity='NON_IDENTITY_LOCAL_BUILD_ONLY'
            identity_artifact='diagnostic.elf (strip-debug, prefix-mapped)'
            map_policy='loadable layout only; debug sections removed from text evidence'
        }
        artifacts=$artifacts
        memory=[ordered]@{base='0xF9000000';size_bytes=16384;regions=@(
            [ordered]@{name='text_data_bss';start=('0x{0:X8}' -f $dataStart);end=('0x{0:X8}' -f $dataEnd)},
            [ordered]@{name='canary';start=('0x{0:X8}' -f $canaryStart);end=('0x{0:X8}' -f $canaryEnd)},
            [ordered]@{name='stack';start=('0x{0:X8}' -f $stackStart);end=('0x{0:X8}' -f $stackEnd)}
        )}
        canary=[ordered]@{symbol='g_p0a_canary';size_bytes=($canaryEnd-$canaryStart);build_id=('0x' + $buildIdHex.ToUpper())}
        disassembly_order_witness=[ordered]@{artifact='objdump';canary_write_offsets=$stageCalls;uart_wait_offsets=$uartCall;semantic_order=@('C003','UART_CONFIG_WRITES','C004','C005','FIRST_TX_POLL')}
        stop_conditions=[ordered]@{same_failure_max_attempts=2;requires_new_evidence_per_attempt=$true;third_attempt_requires_review=$true}
        safety=[ordered]@{arm_enabled=0;board_verified=$false;forbidden=@('USER2','P0-B','UART2/J52','myCobot','board action')}
    }
    $manifestPath = Join-Path $evidence 'manifest.json'
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -Encoding ascii $manifestPath
    if ($PythonExe -and $VerifierPath) {
        & $PythonExe $VerifierPath $manifestPath | Set-Content -Encoding ascii (Join-Path $evidence 'verifier.txt')
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
finally { Pop-Location }
