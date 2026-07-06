@echo off
REM ==========================================================================
REM  build_qemu.bat — 用 Efinity RISC-V 工具链编译 vision_classifier 测试
REM  输出: test.elf (QEMU 可直接加载)
REM ==========================================================================

set TOOLCHAIN=D:\Efinity\efinity-riscv-ide-2025.2\toolchain
set QEMU=D:\Efinity\efinity-riscv-ide-2025.2\qemu

set PATH=%TOOLCHAIN%\bin;%PATH%
set GCC=riscv-none-embed-gcc

set CFLAGS=-march=rv32imac -mabi=ilp32 -O0 -g ^
    -Wall -Wextra ^
    -specs=sim.specs ^
    -DIO_APB_SLAVE_0_BASE=0x00000000 ^
    -I..\app\include

set SRC=main.c startup.S ..\app\src\vision_classifier.c
set LDFLAGS=-T scratchpad.lds

echo.
echo === Building vision_classifier test for QEMU ===
echo.

%GCC% %CFLAGS% %SRC% %LDFLAGS% -o test.elf
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo *** BUILD FAILED ***
    pause
    exit /b 1
)

echo.
echo === Build OK: test.elf ===
echo.
echo Run with:
echo   %QEMU%\qemu-system-riscv32 -M spike -nographic -semihosting -kernel test.elf
echo.
echo (Ctrl+A then X to quit QEMU)
echo.

REM Uncomment the next line to auto-run:
REM %QEMU%\qemu-system-riscv32 -M spike -nographic -semihosting -kernel test.elf
