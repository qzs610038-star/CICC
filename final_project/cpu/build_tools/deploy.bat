@echo off
setlocal

echo [ERROR] CPU deployment is intentionally blocked.
echo A verified SoC batch, PNR/STA evidence, target-board confirmation, and a
echo separately reviewed dry-run Programmer command are required before deploy.
exit /b 1
