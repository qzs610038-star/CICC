<#
.SYNOPSIS
  Sync final_project to the standalone burn/programming directory.

.DESCRIPTION
  Efinity programming currently requires the project to live in a separate
  D: directory. Run this script before burning to copy the current
  final_project tree to that directory.

  Teammate adaptation:
    Change only the $DefaultTargetPath line below if your local burn directory
    is not D:\final_project_shaolu.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\final_project\tools\sync_to_burn_dir.ps1

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\final_project\tools\sync_to_burn_dir.ps1 -DryRun

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\final_project\tools\sync_to_burn_dir.ps1 -TargetPath "E:\final_project_shaolu"

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\final_project\tools\sync_to_burn_dir.ps1 -Mirror
#>

[CmdletBinding()]
param(
    [string]$TargetPath,
    [switch]$Mirror,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

# Teammates only need to change this line for their own machine.
$DefaultTargetPath = "D:\final_project_shaolu"

if ([string]::IsNullOrWhiteSpace($TargetPath)) {
    $TargetPath = $DefaultTargetPath
}

$ScriptPath = $PSCommandPath
if ([string]::IsNullOrWhiteSpace($ScriptPath)) {
    $ScriptPath = $MyInvocation.MyCommand.Path
}

$ToolsDir = Split-Path -Parent $ScriptPath
$FinalProjectPath = Resolve-Path (Join-Path $ToolsDir "..")
$SourcePath = $FinalProjectPath.Path.TrimEnd("\")

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
    throw "Source final_project directory not found: $SourceFullPath"
}

if (Test-UnsafeTargetPath -Path $TargetFullPath) {
    throw "Refusing to sync to an unsafe target path: $TargetFullPath"
}

if ($TargetFullPath -eq $SourceFullPath) {
    throw "Target path is the same as source path: $TargetFullPath"
}

if ($TargetFullPath.StartsWith($SourceFullPath + "\", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Target path must not be inside source final_project: $TargetFullPath"
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
    "__pycache__",
    ".pytest_cache",
    "/XF",
    "*.tmp",
    "*.bak",
    "*.log"
)

if ($DryRun) {
    $robocopyArgs += "/L"
}

Write-Host "Source : $SourceFullPath"
Write-Host "Target : $TargetFullPath"
Write-Host "Mode   : $(if ($Mirror) { 'mirror, deletes stale target files' } else { 'copy/update only, keeps target build outputs' })"
if ($DryRun) {
    Write-Host "DryRun : enabled, no files will be changed"
}

& robocopy @robocopyArgs
$exitCode = $LASTEXITCODE

# Robocopy exit codes 0-7 are success/informational. 8+ means failure.
if ($exitCode -ge 8) {
    throw "robocopy failed with exit code $exitCode"
}

Write-Host "Sync completed. robocopy exit code: $exitCode"
