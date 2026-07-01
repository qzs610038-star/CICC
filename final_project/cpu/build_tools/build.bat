@echo off
setlocal

set "APP_DIR=%~dp0..\app"
cd /d "%APP_DIR%"

where make >nul 2>nul
if errorlevel 1 (
    echo [ERROR] make not found in PATH.
    echo Install or expose a make-compatible tool, then run this script again.
    exit /b 1
)

make %*
