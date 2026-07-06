@echo off
REM ==========================================================================
REM  run_qemu_test.bat — QEMU + GDB 自动化运行 vision_classifier 测试
REM
REM  步骤:
REM   1. 启动 QEMU GDB server (后台)
REM   2. GDB 连接, 加载 ELF, continue 执行
REM   3. 等待测试完成后 GDB 断点, 读取 g_results 内存
REM   4. 打印 PASS/FAIL, 清理
REM ==========================================================================

setlocal enabledelayedexpansion

set TOOLCHAIN=D:\Efinity\efinity-riscv-ide-2025.2\toolchain
set QEMU=D:\Efinity\efinity-riscv-ide-2025.2\qemu
set PATH=%TOOLCHAIN%\bin;%QEMU%;%PATH%

set ELF=test_noprint.elf
set GDB_PORT=12345

echo.
echo === Vision Classifier QEMU Test ===
echo.

REM -- 1. 先杀掉可能残留的 QEMU 进程 --
taskkill /F /IM qemu-system-riscv32.exe >nul 2>&1

REM -- 2. 启动 QEMU GDB server (后台) --
start "QEMU-Test" /B "%QEMU%\qemu-system-riscv32.exe" -M spike -gdb tcp::%GDB_PORT% -bios none -kernel %ELF% -S
echo [1/3] QEMU GDB server started on port %GDB_PORT%

REM 等 QEMU 启动
timeout /t 2 /nobreak >nul

REM -- 3. GDB 自动执行 --
echo [2/3] Running test via GDB...
(
echo target remote localhost:%GDB_PORT%
echo load
echo continue
echo ^C
echo x/4xw &g_results
echo quit
) > gdb_cmds.tmp

"%TOOLCHAIN%\bin\riscv-none-embed-gdb.exe" -batch -x gdb_cmds.tmp %ELF% > gdb_output.txt 2>&1

REM -- 4. 解析结果 --
echo [3/3] Parsing results...

REM 从 GDB 输出提取 g_results
for /f "tokens=*" %%L in ('findstr "0x" gdb_output.txt ^| findstr "g_results"') do (
    echo GDB output: %%L
)

REM 直接显示关键行
echo.
echo --- Raw GDB memory dump ---
findstr /C:"0x" gdb_output.txt | findstr /V "inaccessible"
echo.

REM 清理
del gdb_cmds.tmp 2>nul
REM taskkill /F /IM qemu-system-riscv32.exe >nul 2>&1

echo === Test complete. See gdb_output.txt for full log ===
echo.
echo   Expected PASS: g_results[0] = 0xBEEF0015 (21 tests run)
echo                   g_results[1] = 21 (all passed)
echo                   g_results[2] = 0  (no failures)
echo                   g_results[3] = 0  (no first-fail index)
echo.

endlocal
