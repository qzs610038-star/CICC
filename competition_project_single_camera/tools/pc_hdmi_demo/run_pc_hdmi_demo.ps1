param(
    [ValidateSet('synthetic','camera','file')][string]$Mode = 'synthetic',
    [int]$Device = 0,
    [string]$File = '',
    [string]$SourceLabel = '',
    [int]$Width = 1280,
    [int]$Height = 720,
    [int]$Frames = 0,
    [switch]$Headless,
    [string]$OutputDir = '',
    [switch]$ListDevices
)

$ErrorActionPreference = 'Stop'
$entry = Join-Path $PSScriptRoot 'pc_hdmi_demo.py'

python -c "import cv2, numpy" 2>$null
if ($LASTEXITCODE -ne 0) {
    throw 'PC_DEMO_BLOCKED_DEPENDENCIES: install requirements.txt'
}

$arguments = @($entry)
if ($ListDevices) {
    $arguments += '--list-devices'
} else {
    $arguments += @('--source', $Mode, '--device', $Device, '--width', $Width, '--height', $Height)
    if ($File -ne '') { $arguments += @('--file', $File) }
    if ($SourceLabel -ne '') { $arguments += @('--source-label', $SourceLabel) }
    if ($Frames -gt 0) { $arguments += @('--frames', $Frames) }
    if ($Headless) { $arguments += '--headless' }
    if ($OutputDir -ne '') { $arguments += @('--output-dir', $OutputDir) }
}

& python @arguments
exit $LASTEXITCODE
