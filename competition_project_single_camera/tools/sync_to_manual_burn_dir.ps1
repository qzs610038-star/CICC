<#
.SYNOPSIS
  Sync the single-camera candidate project to its dedicated manual burn directory.

.DESCRIPTION
  Copies the current workspace source of `competition_project_single_camera/`
  to an operator-selected ASCII staging directory for manual Efinity
  synthesis, place-and-route, and volatile programming.

  This is intentionally separate from the final-project staging workflow.
  The destination must be supplied explicitly and must not overlap the source.

  Generated Efinity output/work directories are excluded so the manual burn
  directory always rebuilds from the copied source. By default, existing
  local generated output in the target is preserved.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\competition_project_single_camera\tools\sync_to_manual_burn_dir.ps1

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\competition_project_single_camera\tools\sync_to_manual_burn_dir.ps1 -DryRun

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\competition_project_single_camera\tools\sync_to_manual_burn_dir.ps1 -TargetPath "<absolute ASCII staging directory>"

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\competition_project_single_camera\tools\sync_to_manual_burn_dir.ps1 -Mirror
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$TargetPath,
    [switch]$Mirror,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$ScriptPath = $PSCommandPath
if ([string]::IsNullOrWhiteSpace($ScriptPath)) {
    $ScriptPath = $MyInvocation.MyCommand.Path
}

$ToolsDir = Split-Path -Parent $ScriptPath
$SingleCameraProjectPath = Resolve-Path (Join-Path $ToolsDir "..")
$SourcePath = $SingleCameraProjectPath.Path.TrimEnd("\")

$TargetFullPath = [System.IO.Path]::GetFullPath($TargetPath).TrimEnd("\")
$SourceFullPath = [System.IO.Path]::GetFullPath($SourcePath).TrimEnd("\")

function Test-UnsafeTargetPath {
    param([string]$Path)

    $root = [System.IO.Path]::GetPathRoot($Path).TrimEnd("\")
    $trimmed = $Path.TrimEnd("\")

    if ($trimmed -eq $root) {
        return $true
    }

    if ($trimmed -match '^[A-Za-z]:\\?$') {
        return $true
    }

    return $false
}

if (-not (Test-Path $SourceFullPath -PathType Container)) {
    throw "Source competition_project_single_camera directory not found: $SourceFullPath"
}

if (Test-UnsafeTargetPath -Path $TargetFullPath) {
    throw "Refusing to sync to an unsafe target path: $TargetFullPath"
}

if ($TargetFullPath -eq $SourceFullPath) {
    throw "Target path is the same as source path: $TargetFullPath"
}

if ($TargetFullPath.StartsWith($SourceFullPath + "\", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Target path must not be inside source competition_project_single_camera: $TargetFullPath"
}

if (-not (Test-Path $TargetFullPath -PathType Container)) {
    New-Item -ItemType Directory -Path $TargetFullPath | Out-Null
}

$robocopyMode = if ($Mirror) { "/MIR" } else { "/E" }
$robocopyArgs = @(
    $SourceFullPath,
    $TargetFullPath,
    $robocopyMode,
    "/COPY:DAT",
    "/DCOPY:DAT",
    "/R:2",
    "/W:2",
    "/FFT",
    "/NP",
    "/TEE",
    "/XD",
    ".git",
    ".metadata",
    "outflow",
    "outflow_*",
    "work",
    "work_*",
    "__pycache__",
    ".pytest_cache",
    "/XF",
    "*.tmp",
    "*.bak",
    "*.log",
    "*.vdb",
    "*.qdb",
    "*.db",
    "*.lbf",
    "*.lpf",
    "*.rs",
    "*.map.out",
    "*.place.out",
    "*.route.out",
    "*.tcl.out",
    "*.pgm.out",
    "*.raminfo.pb",
    "*.troutingtraces"
)

if ($DryRun) {
    $robocopyArgs += "/L"
}

Write-Host "Route  : competition_project_single_camera (single-camera manual burn)"
Write-Host "Source : $SourceFullPath"
Write-Host "Target : $TargetFullPath"
Write-Host "Mode   : $(if ($Mirror) { 'mirror source files, preserves excluded generated outputs' } else { 'copy/update only, preserves target-only files and generated outputs' })"
if ($DryRun) {
    Write-Host "DryRun : enabled, no source files will be copied"
}

& robocopy @robocopyArgs
$exitCode = $LASTEXITCODE

# Robocopy exit codes 0-7 are success/informational. 8+ means failure.
if ($exitCode -ge 8) {
    throw "robocopy failed with exit code $exitCode"
}

Write-Host "Single-camera sync completed. robocopy exit code: $exitCode"
