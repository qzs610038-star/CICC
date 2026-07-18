[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Test-PathWithinDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$Child,
        [Parameter(Mandatory = $true)][string]$Parent
    )

    $childPath = [System.IO.Path]::GetFullPath($Child).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $parentPath = [System.IO.Path]::GetFullPath($Parent).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $prefix = $parentPath + [System.IO.Path]::DirectorySeparatorChar
    return $childPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-OfflineBoundary {
    param(
        [Parameter(Mandatory = $true)][string]$RunDirectory,
        [Parameter(Mandatory = $true)][string]$TemporaryRoot
    )

    if (-not (Test-PathWithinDirectory -Child $RunDirectory -Parent $TemporaryRoot)) {
        throw "OFFLINE_PRESUBMIT_BLOCKED: run directory must be inside TEMP: $RunDirectory"
    }

    $source = Get-Content -Raw -LiteralPath $PSCommandPath
    $guardBegin = $source.LastIndexOf('# OFFLINE_FORBIDDEN_BEGIN', [System.StringComparison]::Ordinal)
    $guardEnd = $source.LastIndexOf('# OFFLINE_FORBIDDEN_END', [System.StringComparison]::Ordinal)
    if ($guardBegin -lt 0 -or $guardEnd -le $guardBegin) {
        throw 'OFFLINE_PRESUBMIT_BLOCKED: forbidden-token guard is malformed'
    }
    $sourceOutsideGuard = $source.Remove($guardBegin, ($guardEnd + '# OFFLINE_FORBIDDEN_END'.Length) - $guardBegin)

    # OFFLINE_FORBIDDEN_BEGIN
    $forbiddenTokens = @(
        'serial', 'COM', 'Efinity', 'Programmer', 'JTAG', 'Flash', 'USER1', 'DDR',
        'UART2', 'J52', 'myCobot', 'send_coords', 'send_angles', 'set_coords',
        'set_angles', 'release_all_servos', 'power_on', 'power_off'
    )
    $script:offlineTestScriptRelativePath = 'final_project/cpu/tests/run_mycobot_arm_skeleton_host.ps1'
    $script:offlineTestCheckName = 'mycobot_arm_skeleton_qemu'
    $script:offlineTestRequiredOutput = 'RESULT: PASS (QEMU asserts executed).'
    $script:offlineTestForbiddenOutput = 'ASSERTS_NOT_EXECUTED'
    # OFFLINE_FORBIDDEN_END

    foreach ($token in $forbiddenTokens) {
        $pattern = '(?<![A-Za-z0-9_])' + [regex]::Escape($token) + '(?![A-Za-z0-9_])'
        if ([regex]::IsMatch($sourceOutsideGuard, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
            throw "OFFLINE_PRESUBMIT_BLOCKED: forbidden token outside policy guard: $token"
        }
    }
}

function Test-UntrackedMarkdownTrailingWhitespace {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot
    )

    $markdownFiles = @(& git -C $RepositoryRoot ls-files --others --exclude-standard -- '*.md')
    if ($LASTEXITCODE -ne 0) {
        throw 'FAIL: unable to enumerate untracked Markdown files'
    }

    $issues = New-Object 'System.Collections.Generic.List[string]'
    foreach ($relativePath in $markdownFiles) {
        $absolutePath = Join-Path $RepositoryRoot $relativePath
        $lines = [System.IO.File]::ReadAllLines($absolutePath)
        for ($index = 0; $index -lt $lines.Length; $index++) {
            if ($lines[$index] -match '[\x20\x09]+$') {
                $issues.Add("${relativePath}:$($index + 1)")
            }
        }
    }

    if ($issues.Count -gt 0) {
        throw ("FAIL: untracked Markdown trailing horizontal whitespace:`n" + ($issues -join [Environment]::NewLine))
    }

    Write-Host "UNTRACKED_MARKDOWN_TRAILING_WHITESPACE=PASS files=$($markdownFiles.Count)"
}

$repoRoot = (& git rev-parse --show-toplevel).Trim()
if ([string]::IsNullOrWhiteSpace($repoRoot)) {
    throw 'OFFLINE_PRESUBMIT_BLOCKED: not inside a Git repository'
}
Set-Location -LiteralPath $repoRoot

if ([string]::IsNullOrWhiteSpace($env:TEMP)) {
    throw 'OFFLINE_PRESUBMIT_BLOCKED: TEMP is not set'
}
$tempRoot = [System.IO.Path]::GetFullPath($env:TEMP)
$runDirectory = Join-Path $tempRoot ('cicc-offline-presubmit-' + [guid]::NewGuid().ToString('N'))
Assert-OfflineBoundary -RunDirectory $runDirectory -TemporaryRoot $tempRoot

if ([string]::IsNullOrWhiteSpace($offlineTestScriptRelativePath) -or
    [string]::IsNullOrWhiteSpace($offlineTestCheckName) -or
    [string]::IsNullOrWhiteSpace($offlineTestRequiredOutput) -or
    [string]::IsNullOrWhiteSpace($offlineTestForbiddenOutput)) {
    throw 'OFFLINE_PRESUBMIT_BLOCKED: approved offline test route is missing'
}
$offlineTestScript = Join-Path $repoRoot $offlineTestScriptRelativePath
if (-not (Test-Path -LiteralPath $offlineTestScript)) {
    throw "OFFLINE_PRESUBMIT_BLOCKED: approved offline test script is missing: $offlineTestScriptRelativePath"
}

$checks = @(
    [pscustomobject]@{
        Name = 'agent_handoff_health_check'
        Run = { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'tools/agent_handoff_health_check.ps1') }
    },
    [pscustomobject]@{
        Name = 'project_freshness_check'
        Run = { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'tools/project_freshness_check.ps1') }
    },
    [pscustomobject]@{
        Name = 'interface_freeze_check'
        Run = { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'tools/interface_freeze_check.ps1') }
    },
    [pscustomobject]@{
        Name = 'agent_context_budget'
        Run = { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'tools/agent_context_budget.ps1') }
    },
    [pscustomobject]@{
        Name = 'g2_run_bundle_unittest'
        Run = { & python -m unittest 'final_project/tools/board_observability/tests/test_g2_run_bundle.py' }
    },
    [pscustomobject]@{
        Name = 'g2_host_evidence'
        Run = { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'competition_project_single_camera/cpu/tests/run_g2_host_evidence.ps1') -RunDir $runDirectory }
    },
    [pscustomobject]@{
        Name = $offlineTestCheckName
        Run = { & powershell -NoProfile -ExecutionPolicy Bypass -File $offlineTestScript }
        RequiredOutput = @($offlineTestRequiredOutput)
        ForbiddenOutput = @($offlineTestForbiddenOutput)
    },
    [pscustomobject]@{
        Name = 'untracked_markdown_trailing_whitespace'
        Run = { Test-UntrackedMarkdownTrailingWhitespace -RepositoryRoot $repoRoot }
    },
    [pscustomobject]@{
        Name = 'git_diff_check'
        Run = { & git diff --check }
    }
)

$results = New-Object 'System.Collections.Generic.List[object]'
$failureExit = 0

try {
    foreach ($check in $checks) {
        Write-Host "`n=== OFFLINE_PRESUBMIT: $($check.Name) ==="
        $output = @()
        $exitCode = 1
        $savedErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $output = @(& $check.Run 2>&1)
            $exitCode = $LASTEXITCODE
            if ($null -eq $exitCode) { $exitCode = 0 }
        } catch {
            $output += $_
        } finally {
            $ErrorActionPreference = $savedErrorActionPreference
        }

        foreach ($line in $output) { Write-Host ([string]$line) }
        $outputText = [string]::Join([Environment]::NewLine, @($output | ForEach-Object { [string]$_ }))
        $requiredOutputs = @($check.RequiredOutput | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        $forbiddenOutputs = @($check.ForbiddenOutput | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        foreach ($requiredOutput in $requiredOutputs) {
            if (-not $outputText.Contains($requiredOutput)) {
                Write-Host "FAIL: required output missing: $requiredOutput"
                $exitCode = 1
            }
        }
        foreach ($forbiddenOutput in $forbiddenOutputs) {
            if ($outputText.Contains($forbiddenOutput)) {
                Write-Host "FAIL: forbidden output present: $forbiddenOutput"
                $exitCode = 1
            }
        }
        $notices = @($output | ForEach-Object { [string]$_ } | Where-Object { $_ -match '^\s*(WARN|FAIL):' })
        $status = if ($exitCode -ne 0) { 'FAIL' } elseif ($notices.Count -gt 0) { 'WARN' } else { 'PASS' }
        $results.Add([pscustomobject]@{ Name = $check.Name; Status = $status; ExitCode = $exitCode; Notices = $notices.Count })
        Write-Host "OFFLINE_PRESUBMIT_CHECK name=$($check.Name) status=$status exit=$exitCode notices=$($notices.Count)"

        if ($exitCode -ne 0) {
            $failureExit = $exitCode
            break
        }
    }
} finally {
    if ((Test-Path -LiteralPath $runDirectory) -and (Test-PathWithinDirectory -Child $runDirectory -Parent $tempRoot)) {
        Remove-Item -LiteralPath $runDirectory -Recurse -Force
    }
}

Write-Host "`n=== OFFLINE_PRESUBMIT SUMMARY ==="
foreach ($result in $results) {
    Write-Host "name=$($result.Name) status=$($result.Status) exit=$($result.ExitCode) notices=$($result.Notices)"
}

if ($failureExit -ne 0) {
    Write-Host "OFFLINE_PRESUBMIT=FAIL exit=$failureExit"
    exit $failureExit
}

$warningCount = @($results | Where-Object { $_.Status -eq 'WARN' }).Count
if ($warningCount -gt 0) {
    Write-Host "OFFLINE_PRESUBMIT=PASS_WITH_WARNINGS exit=0 warnings=$warningCount"
} else {
    Write-Host 'OFFLINE_PRESUBMIT=PASS exit=0'
}
exit 0
