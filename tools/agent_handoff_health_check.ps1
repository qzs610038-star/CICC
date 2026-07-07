# Agent handoff health check (read-only, no side effects)
# Usage: powershell -ExecutionPolicy Bypass -File tools/agent_handoff_health_check.ps1 [-Handoff <path>]
# Forbidden here: robot motion commands, full Efinity build/P&R, full simulation reruns.
# Non-zero exit means the next agent should enter the Contradiction Report flow.

param(
    [string]$Handoff = ""
)

$ErrorActionPreference = "Continue"
$failItems = @()
$warnItems = @()

function Fail($msg) {
    $script:failItems += $msg
    Write-Host "FAIL: $msg" -ForegroundColor Red
}

function Warn($msg) {
    $script:warnItems += $msg
    Write-Host "WARN: $msg" -ForegroundColor Yellow
}

function Ok($msg) {
    Write-Host "OK  : $msg" -ForegroundColor Green
}

function Get-RepoRoot {
    try {
        $root = git rev-parse --show-toplevel 2>$null
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($root)) {
            return (Resolve-Path $root).Path
        }
    } catch {}
    return (Resolve-Path (Get-Location)).Path
}

$repo = Get-RepoRoot
if (-not (Test-Path $repo)) {
    Fail "repo root does not exist: $repo"
} else {
    Ok "repo root: $repo"
}

try {
    $branch = git -C $repo rev-parse --abbrev-ref HEAD 2>$null
    $head = git -C $repo rev-parse --short HEAD 2>$null
    $dirty = git -C $repo status --porcelain 2>$null
    $isDirty = [bool]$dirty
    Ok "git: branch=$branch head=$head dirty=$isDirty"
} catch {
    Fail "failed to read git status: $_"
}

$keyFiles = @(
    "AGENTS.md",
    "CLAUDE.md",
    "CURRENT_STATE.md",
    "final_project/fpga/efinity/mem_test.xml",
    "final_project/fpga/efinity/constrain.sdc",
    "final_project/fpga/rtl/top/top.v",
    "final_project/cpu/app/include/bsp.h"
)

foreach ($f in $keyFiles) {
    $p = Join-Path $repo $f
    if (-not (Test-Path $p)) {
        Fail "missing key file: $f"
    } else {
        Ok "exists: $f"
    }
}

foreach ($f in @("final_project/fpga/efinity/mem_test.xml", "AGENTS.md")) {
    $p = Join-Path $repo $f
    if (Test-Path $p) {
        $first = Get-Content $p -TotalCount 1 -ErrorAction SilentlyContinue
        if ([string]::IsNullOrWhiteSpace($first)) {
            Fail "file appears empty or unreadable: $f"
        } else {
            Ok "readable: $f"
        }
    }
}

if ($Handoff -ne "") {
    if (-not (Test-Path $Handoff)) {
        Fail "handoff file not found: $Handoff"
    } else {
        $content = Get-Content $Handoff -Raw -ErrorAction SilentlyContinue

        if ($content -match '"repo_root_declared"\s*:\s*"([^"]+)"') {
            $declaredRoot = $Matches[1].Trim()
            if ($declaredRoot -ne "" -and $declaredRoot -notmatch "^<.*>$") {
                if (Test-Path $declaredRoot) {
                    $declaredResolved = (Resolve-Path $declaredRoot).Path
                    if ($declaredResolved -ne $repo) {
                        Warn "declared repo root differs from current repo; will resolve repo-internal paths relative to current repo"
                    } else {
                        Ok "declared repo root matches current repo"
                    }
                } else {
                    Warn "declared repo root is not present on this machine; will resolve repo-internal paths relative to current repo"
                }
            }
        }

        $absolutePaths = [regex]::Matches($content, '[A-Za-z]:\\[^\s\)`"\]]+') | ForEach-Object { $_.Value }
        foreach ($p in $absolutePaths | Select-Object -Unique) {
            Warn "handoff contains an absolute local path for provenance only, not execution: $p"
        }

        $paths = [regex]::Matches($content, '(final_project[\\/][^\s\)`"\]]+|AGENTS\.md|CLAUDE\.md|CURRENT_STATE\.md|HANDOFF_TEMPLATE\.md)') | ForEach-Object { $_.Value }
        foreach ($p in $paths | Select-Object -Unique) {
            $rel = $p -replace '/', '\'
            $check = Join-Path $repo $rel
            if (-not (Test-Path $check)) {
                Fail "handoff references missing repo-relative path: $p"
            }
        }
        Ok "handoff path check complete"
    }
}

try {
    $pyCode = @'
import importlib.util
serial_spec = importlib.util.find_spec("serial")
print("serial", bool(serial_spec))
print("pymycobot", bool(importlib.util.find_spec("pymycobot")))
if serial_spec:
    import serial.tools.list_ports as p
    for x in p.comports():
        print(x.device, x.description)
'@
    $py = $pyCode | & python - 2>$null
    if ($LASTEXITCODE -ne 0) {
        Fail "python serial/pymycobot read-only check failed"
    } else {
        if ($py -match "serial False") {
            Warn "python serial module not available; serial port enumeration skipped"
        }
        if ($py -match "pymycobot False") {
            Warn "pymycobot not available; robot library import check is negative"
        }
        Ok "myCobot read-only check:`n$py"
    }
} catch {
    Fail "myCobot read-only check failed: $_"
}

$logDirs = @("debug_records", "final_project/docs/review_packets")
foreach ($d in $logDirs) {
    $p = Join-Path $repo $d
    if (-not (Test-Path $p)) {
        Fail "missing log/evidence directory: $d"
    } else {
        Ok "directory exists: $d"
    }
}

if ($warnItems.Count -gt 0) {
    Write-Host "`nHealth check warnings: $($warnItems.Count). If there are no FAIL items, continue and record warnings in the handoff." -ForegroundColor Yellow
}

if ($failItems.Count -gt 0) {
    Write-Host "`nHealth check failed: $($failItems.Count). Enter Contradiction Report flow." -ForegroundColor Yellow
    exit 1
}

Write-Host "`nHealth check passed. Continue with Next Immediate Action." -ForegroundColor Green
exit 0
