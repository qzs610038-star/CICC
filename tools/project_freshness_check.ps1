# Project freshness check (read-only, no side effects)
# Usage: powershell -ExecutionPolicy Bypass -File tools/project_freshness_check.ps1 [-CheckRemote]

[CmdletBinding()]
param(
    [switch]$CheckRemote
)

$ErrorActionPreference = 'Stop'
$failItems = [System.Collections.Generic.List[string]]::new()
$warnItems = [System.Collections.Generic.List[string]]::new()

function Pass([string]$Message) { Write-Host "PASS: $Message" -ForegroundColor Green }
function Warn([string]$Message) { $warnItems.Add($Message); Write-Host "WARN: $Message" -ForegroundColor Yellow }
function Fail([string]$Message) { $failItems.Add($Message); Write-Host "FAIL: $Message" -ForegroundColor Red }

function Normalize-RepoPath([string]$Path) {
    return ($Path -replace '\\', '/').TrimStart('./')
}

function Test-PathMatchesTrigger([string]$Path, [string]$Trigger) {
    $normalizedPath = Normalize-RepoPath $Path
    $normalizedTrigger = Normalize-RepoPath $Trigger
    if ($normalizedTrigger.EndsWith('/')) {
        return $normalizedPath.StartsWith($normalizedTrigger, [System.StringComparison]::OrdinalIgnoreCase)
    }
    return $normalizedPath.Equals($normalizedTrigger, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-LatestPathCommit([string[]]$Paths) {
    if ($null -eq $Paths -or $Paths.Count -eq 0) { return '' }
    $gitArgs = @('log', '-1', '--format=%H', 'HEAD', '--') + @($Paths)
    $result = @(& git @gitArgs 2>$null)
    if ($LASTEXITCODE -ne 0 -or $result.Count -eq 0) { return '' }
    return ([string]$result[0]).Trim()
}

try {
    $root = (& git rev-parse --show-toplevel).Trim()
    if ([string]::IsNullOrWhiteSpace($root)) { throw 'git root is empty' }
    Set-Location -LiteralPath $root
    Pass "repo root: $root"
} catch {
    Write-Error "Not inside a Git repository: $($_.Exception.Message)"
    exit 2
}

$manifestPath = Join-Path $root 'maintenance_manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    Fail 'maintenance_manifest.json is missing'
    $manifest = $null
} else {
    try {
        $manifest = Get-Content -Raw -Encoding UTF8 -LiteralPath $manifestPath | ConvertFrom-Json
        Pass "maintenance manifest: schema $($manifest.schema_version)"
    } catch {
        Fail "maintenance manifest is not valid JSON: $($_.Exception.Message)"
        $manifest = $null
    }
}

$dirty = & git status --porcelain
if ([string]::IsNullOrWhiteSpace(($dirty -join ''))) { Pass 'working tree is clean' } else { Warn 'working tree has uncommitted changes' }

if ($CheckRemote) {
    try {
        $head = (& git rev-parse HEAD).Trim()
        $remote = (& git ls-remote origin refs/heads/main).Trim().Split("`t")[0]
        if ($head -eq $remote) { Pass 'HEAD matches remote main' } else { Warn "HEAD ($head) differs from remote main ($remote)" }
    } catch {
        Warn "remote main could not be checked: $($_.Exception.Message)"
    }
}

if ($null -ne $manifest) {
    foreach ($document in $manifest.managed_documents) {
        $path = Join-Path $root $document.file
        if (Test-Path -LiteralPath $path) { Pass "managed document exists: $($document.file)" } else { Fail "managed document missing: $($document.file)" }
    }

    $verifiedCommit = [string]$manifest.last_verified_commit
    $verifiedCommitUsable = $false
    if ([string]::IsNullOrWhiteSpace($verifiedCommit)) {
        Fail 'maintenance manifest has no last_verified_commit'
    } else {
        & git cat-file -e "$verifiedCommit^{commit}" 2>$null
        if ($LASTEXITCODE -ne 0) {
            Fail "maintenance manifest baseline commit is unavailable: $verifiedCommit"
        } else {
            & git merge-base --is-ancestor $verifiedCommit HEAD
            if ($LASTEXITCODE -ne 0) {
                Warn "maintenance manifest baseline is not an ancestor of HEAD: $verifiedCommit"
            } else {
                $verifiedCommitUsable = $true
                Pass "maintenance manifest baseline: $verifiedCommit"
            }
        }
    }

    $worktreeChanged = @(
        @(& git diff --name-only)
        @(& git diff --cached --name-only)
        @(& git ls-files --others --exclude-standard)
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { Normalize-RepoPath $_ } | Sort-Object -Unique

    foreach ($document in $manifest.managed_documents) {
        $documentPath = Normalize-RepoPath ([string]$document.file)
        $refreshOn = @($document.refresh_on | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { Normalize-RepoPath ([string]$_) })
        if ($refreshOn.Count -eq 0) {
            Warn "managed document has no refresh_on rule: $documentPath"
            continue
        }

        $worktreeTriggers = @($worktreeChanged | Where-Object {
            $changedPath = $_
            @($refreshOn | Where-Object { Test-PathMatchesTrigger $changedPath $_ }).Count -gt 0
        })
        $documentChanged = $worktreeChanged -contains $documentPath
        $triggerCommit = Get-LatestPathCommit $refreshOn
        $committedTriggerAfterBaseline = $false
        if ($verifiedCommitUsable -and -not [string]::IsNullOrWhiteSpace($triggerCommit) -and $triggerCommit -ne $verifiedCommit) {
            & git merge-base --is-ancestor $verifiedCommit $triggerCommit
            $committedTriggerAfterBaseline = ($LASTEXITCODE -eq 0)
        }

        if ($worktreeTriggers.Count -gt 0) {
            if ($documentChanged) {
                Pass "managed document refreshed with worktree trigger: $documentPath"
            } else {
                Warn "managed document may be stale after worktree trigger: $documentPath"
            }
            continue
        }

        if ($committedTriggerAfterBaseline) {
            if ($documentChanged) {
                Pass "managed document refreshed after committed trigger: $documentPath"
                continue
            }
            $documentCommit = Get-LatestPathCommit @($documentPath)
            if ([string]::IsNullOrWhiteSpace($documentCommit)) {
                Warn "managed document has no Git history after trigger: $documentPath"
                continue
            }
            & git merge-base --is-ancestor $triggerCommit $documentCommit
            if ($LASTEXITCODE -eq 0) {
                Pass "managed document commit covers latest trigger: $documentPath"
            } else {
                Warn "managed document may be stale after committed trigger: $documentPath"
            }
        }
    }
}

$statePath = Join-Path $root 'CURRENT_STATE.md'
if (Test-Path -LiteralPath $statePath) {
    $stateLines = Get-Content -Encoding UTF8 -LiteralPath $statePath
    $evidenceLines = @($stateLines | Where-Object { $_ -match '^\s*-\s*\u8BC1\u636E(?:\u8DEF\u5F84|\u7D22\u5F15)\uFF1A' })
    if ($evidenceLines.Count -eq 0) {
        Fail 'CURRENT_STATE evidence scan found no evidence lines'
    }
    foreach ($line in $evidenceLines) {
        foreach ($match in [regex]::Matches($line, '\x60([^\x60]+)\x60')) {
            $reference = $match.Groups[1].Value
            if ($reference -match '[\u300C\u300D]' -or $reference -match '^0x') { continue }
            if ($reference -match '^[A-Za-z]:\\') { Warn "CURRENT_STATE uses absolute path as provenance: $reference"; continue }
            if ($reference -notmatch '[\\/]' -and $reference -notmatch '\.[A-Za-z0-9]+$') { continue }
            $candidate = Join-Path $root $reference
            if (-not (Test-Path -LiteralPath $candidate)) { Fail "CURRENT_STATE evidence is missing: $reference" }
        }
    }
    Pass 'CURRENT_STATE evidence-path scan completed'
} else {
    Fail 'CURRENT_STATE.md is missing'
}

$linkDocs = @(
    'AGENTS.md',
    'CLAUDE.md',
    'CURRENT_STATE.md',
    'final_project/README.md',
    'final_project/cpu/README.md',
    'final_project/docs/README.md',
    'final_project/docs/technical_plans/README.md',
    'final_project/integration/register_map.md',
    'learning_guides/README.md',
    'final_project/docs/review_packets/README.md',
    'docs/agent_context/migration_map.md',
    'docs/agent_context/operations_runbook.md',
    'debug_records/state_history/archive_manifest.md'
)
if ($null -ne $manifest) {
    $linkDocs += @($manifest.managed_documents | ForEach-Object { $_.file })
}
$linkDocs = @($linkDocs | Sort-Object -Unique)
foreach ($relativeDoc in $linkDocs) {
    $docPath = Join-Path $root $relativeDoc
    if (-not (Test-Path -LiteralPath $docPath)) { continue }
    $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $docPath
    if ($text -match 'file:///') { Warn "portable-link scan found file URI: $relativeDoc" }
    $docDir = Split-Path -Parent $docPath
    foreach ($match in [regex]::Matches($text, '\[[^\]]+\]\(([^)]+)\)')) {
        $target = $match.Groups[1].Value.Trim('<>')
        if ($target -match '^(https?://|file:|mailto:|#)') { continue }
        $target = ($target -split '#')[0]
        if ([string]::IsNullOrWhiteSpace($target)) { continue }
        if (-not (Test-Path -LiteralPath (Join-Path $docDir $target))) { Fail "broken relative Markdown link: $relativeDoc -> $target" }
    }
}
Pass 'managed Markdown link scan completed'

$artifactPath = Join-Path $root '.codebase-memory/artifact.json'
if (Test-Path -LiteralPath $artifactPath) {
    try {
        $artifact = Get-Content -Raw -Encoding UTF8 -LiteralPath $artifactPath | ConvertFrom-Json
        $artifactCommit = [string]$artifact.commit
        & git cat-file -e "$artifactCommit^{commit}" 2>$null
        if ($LASTEXITCODE -ne 0) {
            Warn "CBM artifact commit is unavailable locally: $artifactCommit"
        } else {
            & git merge-base --is-ancestor $artifactCommit HEAD
            if ($LASTEXITCODE -eq 0) {
                $changed = & git diff --name-only "$artifactCommit..HEAD"
                $structural = @($changed | Where-Object { $_ -match '^final_project/(fpga/rtl|cpu/app|cpu/tests)/' })
                if ($structural.Count -gt 0) {
                    Warn "CBM artifact predates $($structural.Count) structural CPU/FPGA file(s); refresh required"
                } else {
                    Pass "CBM artifact covers current structural code (commit $artifactCommit)"
                }
            } else {
                Warn "CBM artifact commit is not an ancestor of HEAD: $artifactCommit"
            }
        }
    } catch {
        Warn "CBM artifact could not be parsed: $($_.Exception.Message)"
    }
} else {
    Warn 'CBM artifact metadata is missing'
}

# G3 context budgets, required markers, archive integrity and anti-bloat checks.
if ($null -ne $manifest -and [int]$manifest.schema_version -ge 2) {
    foreach ($entry in @($manifest.context_files)) {
        $entryPath = Join-Path $root ([string]$entry.file)
        if (-not (Test-Path -LiteralPath $entryPath)) { Fail "context entry missing: $($entry.file)"; continue }
        $entryText = Get-Content -Raw -Encoding UTF8 -LiteralPath $entryPath
        $entryBytes = (Get-Item -LiteralPath $entryPath).Length
        if ($entryBytes -gt [int64]$entry.max_bytes) { Fail "context entry exceeds max_bytes: $($entry.file)" }
        foreach ($marker in @($entry.required_markers)) {
            if ($entryText.IndexOf([string]$marker, [System.StringComparison]::Ordinal) -lt 0) { Fail "required marker missing: $($entry.file) -> $marker" }
        }
    }

    foreach ($relative in @($manifest.stable_entrypoints)) {
        $stablePath = Join-Path $root ([string]$relative)
        if (-not (Test-Path -LiteralPath $stablePath)) { Fail "stable entrypoint missing: $relative"; continue }
        $stableText = Get-Content -Raw -Encoding UTF8 -LiteralPath $stablePath
        foreach ($pattern in @($manifest.forbidden_dynamic_patterns)) {
            if ([regex]::IsMatch($stableText, [string]$pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
                Fail "dynamic artifact fact found in stable entrypoint: $relative pattern=$pattern"
            }
        }
    }

    $archivePath = Join-Path $root ([string]$manifest.archive.file)
    $archiveManifestPath = Join-Path $root ([string]$manifest.archive.manifest)
    if (-not (Test-Path -LiteralPath $archivePath)) {
        Fail 'CURRENT_STATE history archive is missing'
    } else {
        $archiveHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash
        $archiveLines = (Get-Content -Encoding UTF8 -LiteralPath $archivePath).Count
        if ($archiveHash -ne [string]$manifest.archive.sha256) { Fail "CURRENT_STATE history archive hash mismatch: $archiveHash" }
        if ($archiveLines -ne [int]$manifest.archive.lines) { Fail "CURRENT_STATE history archive line mismatch: $archiveLines" }
        if ($archiveHash -eq [string]$manifest.archive.sha256 -and $archiveLines -eq [int]$manifest.archive.lines) { Pass 'CURRENT_STATE history archive hash/line mapping verified' }
    }
    if (-not (Test-Path -LiteralPath $archiveManifestPath)) { Fail 'CURRENT_STATE archive manifest is missing' }

    $budgetScript = Join-Path $root 'tools/agent_context_budget.ps1'
    if (-not (Test-Path -LiteralPath $budgetScript)) {
        Fail 'agent_context_budget.ps1 is missing'
    } else {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $budgetScript
        if ($LASTEXITCODE -ne 0) { Fail "agent context budget failed: exit $LASTEXITCODE" } else { Pass 'agent context budget completed' }
    }
} else {
    Fail 'maintenance manifest schema 2 context gates are missing'
}

Write-Host "`nFreshness summary: PASS checks completed; WARN=$($warnItems.Count); FAIL=$($failItems.Count)."
if ($failItems.Count -gt 0) { exit 1 }
