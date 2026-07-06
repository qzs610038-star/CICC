@echo off
REM ==========================================================================
REM  build.bat - Build RISC-V Shape Recognition for SapphireSoC (QCRV32)
REM
REM  Usage:
REM    build.bat                    - Build all
REM    build.bat clean              - Clean build artifacts
REM
REM  Prerequisites:
REM    - Efinity 2025.2 installed
REM    - EFINDIR environment variable set (or Efinity in default path)
REM ==========================================================================

setlocal enabledelayedexpansion

echo ============================================
echo  RISC-V Shape Recognition Builder
echo  Target: SapphireSoC QCRV32 (TJ375N529)
echo ============================================
echo.

REM Find Efinity installation
if "%EFINDIR%"=="" (
    REM Try common installation paths (check for versioned subdirectory)
    if exist "D:\Efinity\2025.2\bin" (
        set "EFINDIR=D:\Efinity"
    ) else if exist "C:\Efinity\2025.2\bin" (
        set "EFINDIR=C:\Efinity"
    ) else if exist "C:\Program Files\Efinity\2025.2\bin" (
        set "EFINDIR=C:\Program Files\Efinity"
    ) else (
        echo [ERROR] Efinity installation not found.
        echo Please set EFINDIR environment variable or install Efinity to default path.
        pause
        exit /b 1
    )
    echo Auto-detected Efinity: %EFINDIR%
) else (
    echo Using EFINDIR: %EFINDIR%
)

REM Set toolchain path (use RISC-V IDE toolchain bundled with Efinity)
set "RISCV_TOOLCHAIN=%EFINDIR%\efinity-riscv-ide-2025.2\toolchain\bin"
set "RISCV_PREFIX=riscv-none-embed-"
set "RISCV_CC=%RISCV_TOOLCHAIN%\%RISCV_PREFIX%gcc.exe"
set "RISCV_OBJCOPY=%RISCV_TOOLCHAIN%\%RISCV_PREFIX%objcopy.exe"
set "RISCV_OBJDUMP=%RISCV_TOOLCHAIN%\%RISCV_PREFIX%objdump.exe"
set "RISCV_SIZE=%RISCV_TOOLCHAIN%\%RISCV_PREFIX%size.exe"

REM Verify toolchain
if not exist "%RISCV_CC%" (
    echo [ERROR] RISC-V GCC not found: %RISCV_CC%
    echo Please check Efinity installation.
    pause
    exit /b 1
)

echo Toolchain: %RISCV_TOOLCHAIN%
echo.

REM Check source files
set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

if not exist "main.c" (
    echo [ERROR] main.c not found in %SRC_DIR%
    pause
    exit /b 1
)
if not exist "shape_detect.c" (
    echo [ERROR] shape_detect.c not found
    pause
    exit /b 1
)
if not exist "startup_qcrv32.S" (
    echo [ERROR] startup_qcrv32.S not found
    pause
    exit /b 1
)
if not exist "linker.ld" (
    echo [ERROR] linker.ld not found
    pause
    exit /b 1
)

echo Source files verified.
echo.

REM Handle clean command
if "%1"=="clean" (
    echo Cleaning build artifacts...
    del /Q *.o *.elf *.hex *.bin *.lst *.map 2>nul
    echo Clean complete.
    exit /b 0
)

REM ==========================================================================
REM  Compilation
REM ==========================================================================

set "CFLAGS=-march=rv32imac -mabi=ilp32 -mcmodel=medlow -O2 -ffunction-sections -fdata-sections -Wall -g -I."
set "LDFLAGS=-march=rv32imac -mabi=ilp32 -nostartfiles -Wl,--gc-sections -Wl,-Map=shape_detect.elf.map -T linker.ld"

echo [1/5] Compiling startup_qcrv32.S...
"%RISCV_CC%" %CFLAGS% -c -o startup_qcrv32.o startup_qcrv32.S
if errorlevel 1 (
    echo [ERROR] Failed to compile startup_qcrv32.S
    pause
    exit /b 1
)

echo [2/5] Compiling main.c...
"%RISCV_CC%" %CFLAGS% -c -o main.o main.c
if errorlevel 1 (
    echo [ERROR] Failed to compile main.c
    pause
    exit /b 1
)

echo [3/5] Compiling shape_detect.c...
"%RISCV_CC%" %CFLAGS% -c -o shape_detect.o shape_detect.c
if errorlevel 1 (
    echo [ERROR] Failed to compile shape_detect.c
    pause
    exit /b 1
)

REM ==========================================================================
REM  Linking
REM ==========================================================================

echo [4/5] Linking...
"%RISCV_CC%" %LDFLAGS% -o shape_detect.elf startup_qcrv32.o main.o shape_detect.o
if errorlevel 1 (
    echo [ERROR] Linking failed
    pause
    exit /b 1
)

REM ==========================================================================
REM  Generate output files
REM ==========================================================================

echo [5/5] Generating output files...

if exist "%RISCV_OBJCOPY%" (
    "%RISCV_OBJCOPY%" -O ihex shape_detect.elf shape_detect.hex
    "%RISCV_OBJCOPY%" -O binary shape_detect.elf shape_detect.bin
)

if exist "%RISCV_OBJDUMP%" (
    "%RISCV_OBJDUMP%" -d -S shape_detect.elf > shape_detect.lst
)

echo.
echo ============================================
echo  Build Summary
echo ============================================

if exist "%RISCV_SIZE%" (
    "%RISCV_SIZE%" shape_detect.elf
)

echo.
echo Output files:
echo   shape_detect.elf  - ELF executable
dir shape_detect.elf 2>nul | find "shape_detect.elf"

if exist "shape_detect.hex" (
    echo   shape_detect.hex  - Intel HEX (for JTAG download)
    dir shape_detect.hex 2>nul | find "shape_detect.hex"
)

if exist "shape_detect.bin" (
    echo   shape_detect.bin  - Binary image
    dir shape_detect.bin 2>nul | find "shape_detect.bin"
)

echo.
echo ============================================
echo  Build SUCCESSFUL
echo ============================================
echo.
echo Next steps:
echo 1. Download shape_detect.hex to QCRV32 SoC via JTAG
echo 2. Use Efinity Programmer or openocd
echo 3. Monitor UART output at 115200 baud
echo.

pause
