@echo off
REM ==========================================================================
REM  deploy.bat - Deploy RISC-V firmware to SapphireSoC via JTAG
REM
REM  This script programs the compiled shape recognition code to the QCRV32
REM  SoC using Efinity Programmer.
REM ==========================================================================

setlocal enabledelayedexpansion

echo ============================================
echo  RISC-V Firmware Deployer
echo  Target: SapphireSoC QCRV32 (TJ375N529)
echo ============================================
echo.

REM Check if hex file exists
set "HEX_FILE=shape_detect.hex"
if not exist "%HEX_FILE%" (
    echo [ERROR] %HEX_FILE% not found.
    echo Please run build.bat first.
    pause
    exit /b 1
)

echo Firmware file: %HEX_FILE%
dir %HEX_FILE% 2>nul | find "shape_detect.hex"
echo.

REM Check if FPGA bitstream exists
set "FPGA_BIT=..\outflow\mem_test.bit"
if exist "%FPGA_BIT%" (
    echo FPGA bitstream: %FPGA_BIT%
) else (
    echo [WARNING] FPGA bitstream not found at %FPGA_BIT%
    echo Make sure FPGA is already programmed.
)

echo.
echo ============================================
echo  Deployment Instructions
echo ============================================
echo.
echo Option 1: Using Efinity Programmer (GUI)
echo -----------------------------------------
echo 1. Open Efinity Programmer
echo 2. Load FPGA bitstream: outflow\mem_test.bit
echo 3. Load RISC-V firmware: sw\shape_detect.hex
echo 4. Click "Program" to download both
echo.
echo Option 2: Using Command Line (if available)
echo --------------------------------------------
echo If your Efinity version supports command-line programming:
echo   efx_programmer -fpga outflow\mem_test.bit -riscv sw\shape_detect.hex
echo.
echo Option 3: Using OpenOCD (if JTAG adapter supported)
echo ----------------------------------------------------
echo   openocd -f interface/your_jtag.cfg ^
echo            -f target/qcrv32.cfg ^
echo            -c "program sw\shape_detect.hex verify reset exit"
echo.
echo ============================================
echo.
echo Please connect the JTAG adapter to the development board now.
echo.
pause

echo.
echo Launching deployment instructions...
echo.

REM Try to find and launch Efinity Programmer
set "PROGRAMMER=%EFINDIR%\ispFPGA\bin\programmer.exe"
if exist "%PROGRAMMER%" (
    echo Launching Efinity Programmer...
    start "" "%PROGRAMMER%"
    echo.
    echo Programmer launched. Please:
    echo 1. Load the FPGA bitstream
    echo 2. Load %HEX_FILE% as RISC-V firmware
    echo 3. Click Program
) else (
    echo Efinity Programmer not found at: %PROGRAMMER%
    echo Please launch it manually from Start Menu.
)

echo.
echo ============================================
echo  Deploy script complete
echo ============================================

pause
