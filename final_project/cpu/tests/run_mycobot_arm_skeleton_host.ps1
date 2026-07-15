Param(
    [int]$QemuTimeoutSeconds = 8
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
    $scriptPath = $PSCommandPath
    if (-not $scriptPath -and $MyInvocation.MyCommand.Source) {
        $scriptPath = $MyInvocation.MyCommand.Source
    }
    if (-not $scriptPath) {
        $scriptPath = $MyInvocation.MyCommand.Path
    }
    $scriptDir = Split-Path -Parent $scriptPath
    return (Resolve-Path (Join-Path $scriptDir '..\..\..')).Path
}

function New-BuildArea {
    $repoRoot = Resolve-RepoRoot
    $buildDir = Join-Path $repoRoot 'final_project\cpu\build\mycobot_arm_skeleton_host'
    if (Test-Path $buildDir) {
        Get-ChildItem -Recurse $buildDir | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Force $buildDir | Out-Null
    return $buildDir
}

function Run-Native {
    param(
        [string]$Name,
        [string]$Exe
    )

    $repoRoot = Resolve-RepoRoot
    $testsDir = Join-Path $repoRoot 'final_project\cpu\tests'
    $buildDir = New-BuildArea
    $includeDir = Join-Path $repoRoot 'final_project\cpu\app\include'
    $paramsDir = Join-Path $repoRoot 'final_project\cpu\params'
    $testSrc = Join-Path $testsDir 'test_mycobot_arm_skeleton.c'
    $protoSrc = Join-Path $repoRoot 'final_project\cpu\app\src\mycobot_protocol.c'
    $transportSrc = Join-Path $repoRoot 'final_project\cpu\app\src\mycobot_transport.c'
    $transactionSrc = Join-Path $repoRoot 'final_project\cpu\app\src\mycobot_transaction.c'
    $ctrlSrc = Join-Path $repoRoot 'final_project\cpu\app\src\arm_controller.c'
    $positionsSrc = Join-Path $paramsDir 'arm_positions.c'
    $outExe = Join-Path $buildDir 'test_mycobot_arm_skeleton_host.exe'
    $runLog = Join-Path $buildDir 'native_run.log'

    $isCl = ($Name -ieq 'cl')
    $includeArg = "-I$includeDir"
    $paramsIncludeArg = "-I$paramsDir"
    if ($isCl) {
        $compileArgs = @(
            '/nologo', '/W4', '/O0', '/MD',
            '/EHsc', '/std:c11', ('/I' + $includeDir), ('/I' + $paramsDir),
            $testSrc, $protoSrc, $transportSrc, $transactionSrc, $ctrlSrc, $positionsSrc,
            ('/Fe:' + $outExe)
        )
    } else {
        $compileArgs = @(
            '-std=c11', '-O0', '-g', '-Wall', '-Wextra',
            $includeArg, $paramsIncludeArg, $testSrc, $protoSrc,
            $transportSrc, $transactionSrc, $ctrlSrc, $positionsSrc,
            '-o', $outExe
        )
    }

    Write-Host "[Native] compiler: $Name"
    Write-Host "Build output: $outExe"
    & $Exe @compileArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "BUILD_FAIL(native): $Name compile returned $LASTEXITCODE"
        return 1
    }

    Write-Host "Build OK. Running now for real assert execution."
    & $outExe *> $runLog
    $runCode = $LASTEXITCODE
    Get-Content $runLog
    if ($runCode -eq 0) {
        Write-Host "RESULT: PASS (native asserts executed)."
        return 0
    }

    Write-Host "RESULT: FAIL (asserts failed or runtime returned $runCode)."
    return $runCode
}

function Run-QemuFallback {
    $repoRoot = Resolve-RepoRoot
    $testsDir = Join-Path $repoRoot 'final_project\cpu\tests'
    $buildDir = New-BuildArea
    $includeDir = Join-Path $repoRoot 'final_project\cpu\app\include'
    $paramsDir = Join-Path $repoRoot 'final_project\cpu\params'
    $ldScript = Join-Path $testsDir 'scratchpad.lds'
    $testSrc = Join-Path $testsDir 'test_mycobot_arm_skeleton.c'
    $protoSrc = Join-Path $repoRoot 'final_project\cpu\app\src\mycobot_protocol.c'
    $transportSrc = Join-Path $repoRoot 'final_project\cpu\app\src\mycobot_transport.c'
    $transactionSrc = Join-Path $repoRoot 'final_project\cpu\app\src\mycobot_transaction.c'
    $ctrlSrc = Join-Path $repoRoot 'final_project\cpu\app\src\arm_controller.c'
    $positionsSrc = Join-Path $paramsDir 'arm_positions.c'

    $riscvGcc = 'D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-gcc.exe'
    $qemuExe = 'D:\Efinity\efinity-riscv-ide-2025.2\qemu\qemu-system-riscv32.exe'
    $simSpec = 'D:\Efinity\efinity-riscv-ide-2025.2\toolchain\riscv-none-embed\lib\sim.specs'

    if (-not (Test-Path $riscvGcc)) {
        Write-Host "RISC-V toolchain not found:"
        Write-Host "  gcc  : $riscvGcc"
        $fallbackLog = Join-Path $buildDir 'qemu_not_available.log'
        "ASSERTS_NOT_EXECUTED: toolchain/qemu not available, object compile only." | Set-Content -Path $fallbackLog -Encoding UTF8
        Get-Content $fallbackLog
        return 11
    }

    if (-not (Test-Path $qemuExe)) {
        Write-Host "RISC-V toolchain or QEMU path missing:"
        Write-Host "  qemu : $qemuExe"
        Write-Host "  gcc  : $riscvGcc"
    }

    $includeArg = "-I$includeDir"
    $paramsIncludeArg = "-I$paramsDir"
    $specArg = @()
    if (Test-Path $simSpec) {
        $specArg = @("--specs=$simSpec")
    }

    $ccArgs = @(
        '-march=rv32imac',
        '-mabi=ilp32',
        '-O0', '-g', '-Wall', '-Wextra',
        $specArg,
        $includeArg,
        $paramsIncludeArg
    )

    $testObj = Join-Path $buildDir 'test_mycobot_arm_skeleton_rv32.o'
    $protoObj = Join-Path $buildDir 'mycobot_protocol_rv32.o'
    $transportObj = Join-Path $buildDir 'mycobot_transport_rv32.o'
    $transactionObj = Join-Path $buildDir 'mycobot_transaction_rv32.o'
    $ctrlObj = Join-Path $buildDir 'arm_controller_rv32.o'
    $positionsObj = Join-Path $buildDir 'arm_positions_rv32.o'
    $shimC = Join-Path $buildDir 'qemu_exit_shim.c'
    $shimObj = Join-Path $buildDir 'qemu_exit_shim.o'
    $exitStartup = Join-Path $buildDir 'qemu_semihost_exit_startup.S'
    $elf = Join-Path $buildDir 'test_mycobot_arm_skeleton_rv32.elf'
    $compileLog = Join-Path $buildDir 'qemu_compile.log'
    $qemuStd = Join-Path $buildDir 'qemu_stdout.log'
    $qemuErr = Join-Path $buildDir 'qemu_stderr.log'
    $runLog = Join-Path $buildDir 'qemu_run.log'

    @'
static void qemu_semihost_exit(int code)
{
    volatile int block[2];
    register int a0 asm("a0") = 0x20;
    register void *a1 asm("a1") = (void *)block;

    block[0] = 0x20026;
    block[1] = code;
    asm volatile (
        ".option push\n"
        ".option norvc\n"
        "slli zero, zero, 0x1f\n"
        "ebreak\n"
        "srai zero, zero, 7\n"
        ".option pop\n"
        :
        : "r"(a0), "r"(a1)
        : "memory");
    for (;;) {
    }
}

void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    (void)file;
    (void)line;
    (void)func;
    (void)expr;
    qemu_semihost_exit(1);
}

int mycobot_test_entry(void);
int main(void) {
    return mycobot_test_entry();
}
'@ | Set-Content -Path $shimC -Encoding ASCII

    @'
    .section .init
    .globl _startup
    .globl _start
_start:
_startup:
    la sp, _sp
    call main
    mv s0, a0

    /* RISC-V semihosting SYS_EXIT_EXTENDED(reason, code). */
    la a1, semihost_exit_block
    li t0, 0x20026
    sw t0, 0(a1)
    sw s0, 4(a1)
    li a0, 0x20
    .option push
    .option norvc
    slli zero, zero, 0x1f
    ebreak
    srai zero, zero, 7
    .option pop
1:
    j 1b

    .section .bss
    .align 4
semihost_exit_block:
    .space 8
'@ | Set-Content -Path $exitStartup -Encoding ASCII

    $buildFailed = $null
    & $riscvGcc @ccArgs -Dmain=mycobot_test_entry -c $testSrc -o $testObj
    if ($LASTEXITCODE -ne 0) { $buildFailed = 'RISC-V test source compile failed.' }
    if (-not $buildFailed) {
        & $riscvGcc @ccArgs -c $protoSrc -o $protoObj
        if ($LASTEXITCODE -ne 0) { $buildFailed = 'RISC-V protocol compile failed.' }
    }
    if (-not $buildFailed) {
        & $riscvGcc @ccArgs -c $transportSrc -o $transportObj
        if ($LASTEXITCODE -ne 0) { $buildFailed = 'RISC-V transport compile failed.' }
    }
    if (-not $buildFailed) {
        & $riscvGcc @ccArgs -c $transactionSrc -o $transactionObj
        if ($LASTEXITCODE -ne 0) { $buildFailed = 'RISC-V transaction compile failed.' }
    }
    if (-not $buildFailed) {
        & $riscvGcc @ccArgs -c $ctrlSrc -o $ctrlObj
        if ($LASTEXITCODE -ne 0) { $buildFailed = 'RISC-V controller compile failed.' }
    }
    if (-not $buildFailed) {
        & $riscvGcc @ccArgs -c $positionsSrc -o $positionsObj
        if ($LASTEXITCODE -ne 0) { $buildFailed = 'RISC-V arm positions compile failed.' }
    }
    if (-not $buildFailed) {
        & $riscvGcc @ccArgs -c $shimC -o $shimObj
        if ($LASTEXITCODE -ne 0) { $buildFailed = 'RISC-V shim compile failed.' }
    }
    if (-not $buildFailed) {
        & $riscvGcc @ccArgs -nostartfiles $exitStartup $testObj $protoObj $transportObj $transactionObj $ctrlObj $positionsObj $shimObj -T $ldScript -o $elf
        if ($LASTEXITCODE -ne 0) { $buildFailed = 'RISC-V link failed.' }
    }
    if ($buildFailed) {
        "ASSERTS_NOT_EXECUTED: object compile step failed: $buildFailed" | Set-Content -Path $compileLog -Encoding UTF8
        "ASSERTS_NOT_EXECUTED: object compile step failed: $buildFailed" | Add-Content $runLog -Encoding UTF8
        Write-Host "ASSERTS_NOT_EXECUTED: object compile step failed: $buildFailed"
        if (Test-Path $compileLog) { Get-Content $compileLog }
        return 11
    }

    "RISC-V object compile OK." | Set-Content -Path $compileLog -Encoding UTF8

    Write-Host "QEMU object compile OK: $elf"
    Write-Host "Running QEMU semihosting assertion check."

    if (-not (Test-Path $qemuExe)) {
        "ASSERTS_NOT_EXECUTED: object compile done, QEMU execution path unavailable." | Set-Content -Path $runLog -Encoding UTF8
        Write-Host "ASSERTS_NOT_EXECUTED: object compile done, QEMU execution path unavailable."
        return 11
    }

    $tempElf = Join-Path $env:TEMP ('mycobot_mycobot_skeleton_' + [guid]::NewGuid().ToString('N') + '.elf')
    Copy-Item -Path $elf -Destination $tempElf -Force

    try {
        $qemuArgs = @(
            '-M', 'spike',
            '-nographic',
            '-semihosting-config', 'enable=on,target=native',
            '-bios', 'none',
            '-kernel', $tempElf
        )
        & $qemuExe @qemuArgs > $qemuStd 2> $qemuErr
        $qemuExitCode = $LASTEXITCODE
        "QEMU exit code: $qemuExitCode" | Out-File -FilePath $compileLog -Append -Encoding UTF8
        "`nQEMU stdout:" | Add-Content -Path $runLog -Encoding UTF8
        if (Test-Path $qemuStd) { Get-Content $qemuStd | Add-Content -Path $runLog -Encoding UTF8 }
        "`nQEMU stderr:" | Add-Content -Path $runLog -Encoding UTF8
        if (Test-Path $qemuErr) { Get-Content $qemuErr | Add-Content -Path $runLog -Encoding UTF8 }

        if ($null -eq $qemuExitCode) {
            "ASSERTS_NOT_EXECUTED: QEMU exited but PowerShell did not expose an exit code." | Add-Content -Path $runLog -Encoding UTF8
            Write-Host "ASSERTS_NOT_EXECUTED: QEMU exited but PowerShell did not expose an exit code."
            Get-Content $runLog
            return 11
        }

        if ($qemuExitCode -eq 0) {
            "RESULT: PASS (QEMU asserts executed)." | Add-Content -Path $runLog -Encoding UTF8
            Write-Host "RESULT: PASS (QEMU asserts executed)."
            Get-Content $runLog
            return 0
        }

        "RESULT: FAIL (QEMU asserts executed; exit code $qemuExitCode)." | Add-Content -Path $runLog -Encoding UTF8
        Write-Host "RESULT: FAIL (QEMU asserts executed; exit code $qemuExitCode)."
        Get-Content $runLog
        return $qemuExitCode
    } finally {
        Remove-Item $tempElf -Force -ErrorAction SilentlyContinue
        if (Test-Path $qemuStd) { Remove-Item $qemuStd -Force -ErrorAction SilentlyContinue }
        if (Test-Path $qemuErr) { Remove-Item $qemuErr -Force -ErrorAction SilentlyContinue }
    }
}

$nativeCompiler = $null
foreach ($candidate in @('gcc', 'clang', 'cl', 'tcc')) {
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd) {
        $nativeCompiler = [PSCustomObject]@{
            Name = $candidate
            Exe  = $cmd.Source
        }
        break
    }
}

if ($nativeCompiler) {
    $code = Run-Native -Name $nativeCompiler.Name -Exe $nativeCompiler.Exe
    if ($code -eq 0) { exit 0 }
    Write-Host "Native branch failed with code $code."
    exit $code
}

Write-Host "No native compiler found in PATH (gcc/clang/cl/tcc)."
Write-Host "Falling back to Efinity RISC-V QEMU shim attempt..."
$code = Run-QemuFallback
exit $code
