Param(
    [int]$TimeoutSeconds = 15
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$toolchain = 'D:\Efinity\efinity-riscv-ide-2025.2\toolchain'
$gcc = Join-Path $toolchain 'bin\riscv-none-embed-gcc.exe'
$qemu = 'D:\Efinity\efinity-riscv-ide-2025.2\qemu\qemu-system-riscv32.exe'
$specs = Join-Path $toolchain 'riscv-none-embed\lib\sim.specs'
foreach ($path in @($gcc, $qemu, $specs)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing QEMU test dependency: $path" }
}

$include = Join-Path $root 'final_project\cpu\app\include'
$params = Join-Path $root 'final_project\cpu\params'
$src = Join-Path $root 'final_project\cpu\app\src'
$test = Join-Path $root 'final_project\cpu\tests\test_arm_runtime.c'
$shim = Join-Path $root 'final_project\cpu\tests\qemu_test_shim.c'
$startup = Join-Path $root 'final_project\cpu\tests\qemu_test_startup.S'
$timeoutStartup = Join-Path $root 'final_project\cpu\tests\qemu_timeout_loop.S'
$linker = Join-Path $root 'final_project\cpu\tests\scratchpad.lds'
$build = Join-Path $env:TEMP ('codex-arm-runtime-qemu-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $build | Out-Null

function Invoke-RiscvTool([string]$Label, [string[]]$Arguments) {
    $output = @(& $gcc @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    $warningLines = @($output | ForEach-Object { [string]$_ } |
        Where-Object { $_ -match '(?i)\bwarning:' })
    $unexpectedWarnings = @($warningLines | Where-Object {
        $_ -notmatch 'warning: #warning "APB3 base address not provided by soc\.h'
    })

    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode.`n$($output -join "`n")"
    }
    if ($unexpectedWarnings.Count -ne 0) {
        throw "$Label emitted unexpected warning(s):`n$($unexpectedWarnings -join "`n")"
    }
    return ,$warningLines
}

function Invoke-QemuWithDeadline([string]$Elf, [int]$DeadlineSeconds) {
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $qemu
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    foreach ($argument in @('-M', 'spike', '-nographic', '-semihosting-config',
                             'enable=on,target=native', '-bios', 'none',
                             '-kernel', $Elf)) {
        [void]$psi.ArgumentList.Add($argument)
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $psi
    if (-not $process.Start()) { throw "Could not start QEMU: $qemu" }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($DeadlineSeconds * 1000)) {
        try { $process.Kill($true) } catch { }
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        throw "QEMU timed out after $DeadlineSeconds second(s): $stderr$stdout"
    }

    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 0) {
        throw "QEMU exited with code $($process.ExitCode): $stderr$stdout"
    }
    if (($stdout + $stderr) -match '(?i)\bwarning:') {
        throw "QEMU emitted a warning: $stderr$stdout"
    }
}

function Invoke-QemuProfile([string]$Backend, [string[]]$Sources) {
    $defs = @('-DAPP_PROFILE=ARM_PROFILE_ARM_BRINGUP', ("-DARM_BACKEND=ARM_BACKEND_" + $Backend.ToUpperInvariant()), '-DAPB_VISION_BASE_PLACEHOLDER=0xF0000000u')
    $flags = @('-march=rv32imac', '-mabi=ilp32', '-O0', '-g', '-Wall', '-Wextra',
               '-Werror', '-Wno-error=cpp', ("--specs=" + $specs),
               ("-I" + $include), ("-I" + $params)) + $defs
    $objects = @()
    $testObj = Join-Path $build ($Backend + '_test.o')
    Invoke-RiscvTool "$Backend QEMU test source compile" ($flags + @('-Dmain=arm_runtime_test_entry', '-c', $test, '-o', $testObj)) | Out-Null
    $objects += $testObj
    for ($i = 0; $i -lt $Sources.Count; ++$i) {
        $object = Join-Path $build ($Backend + '_' + $i + '.o')
        Invoke-RiscvTool "$Backend QEMU source compile: $($Sources[$i])" ($flags + @('-c', $Sources[$i], '-o', $object)) | Out-Null
        $objects += $object
    }
    $shimObj = Join-Path $build ($Backend + '_shim.o')
    $startupObj = Join-Path $build ($Backend + '_startup.o')
    Invoke-RiscvTool "$Backend QEMU shim compile" ($flags + @('-c', $shim, '-o', $shimObj)) | Out-Null
    Invoke-RiscvTool "$Backend QEMU startup compile" ($flags + @('-c', $startup, '-o', $startupObj)) | Out-Null
    $elf = Join-Path $build ($Backend + '.elf')
    Invoke-RiscvTool "$Backend QEMU link" ($flags + @('-nostartfiles', '-Wl,--fatal-warnings', $startupObj, $shimObj) + $objects + @('-T', $linker, '-o', $elf)) | Out-Null
    Invoke-QemuWithDeadline $elf $TimeoutSeconds
    Write-Output "QEMU PASS backend=$Backend"
}

function Invoke-QemuTimeoutProbe {
    $timeoutObject = Join-Path $build 'timeout_probe.o'
    $timeoutElf = Join-Path $build 'timeout_probe.elf'
    $flags = @('-march=rv32imac', '-mabi=ilp32', '-O0', '-g', '-Wall', '-Wextra',
               '-Werror', '-Wno-error=cpp', ("--specs=" + $specs))
    Invoke-RiscvTool 'QEMU timeout probe startup compile' ($flags + @('-c', $timeoutStartup, '-o', $timeoutObject)) | Out-Null
    Invoke-RiscvTool 'QEMU timeout probe link' ($flags + @('-nostartfiles', '-Wl,--fatal-warnings', $timeoutObject, '-T', $linker, '-o', $timeoutElf)) | Out-Null
    try {
        Invoke-QemuWithDeadline $timeoutElf 1
        throw 'QEMU timeout probe unexpectedly completed.'
    } catch {
        if ($_.Exception.Message -notmatch 'QEMU timed out after 1 second') {
            throw
        }
    }
    Write-Output 'QEMU TIMEOUT PASS seconds=1'
}

try {
    Invoke-QemuProfile 'disabled' @((Join-Path $src 'arm_runtime.c'), (Join-Path $src 'round_controller.c'))
    Invoke-QemuProfile 'simulated' @((Join-Path $src 'arm_runtime.c'),
        (Join-Path $src 'arm_sim_transport.c'), (Join-Path $src 'arm_controller.c'),
        (Join-Path $src 'round_controller.c'), (Join-Path $params 'arm_positions.c'))
    Invoke-QemuTimeoutProbe
} finally {
    Start-Sleep -Milliseconds 200
    Remove-Item -LiteralPath $build -Recurse -Force -ErrorAction SilentlyContinue
}
