[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('wsc', 'libaoxun', 'qzs')]
    [string]$Role,

    [string]$BaseRef = 'HEAD',

    [string]$TargetRef = 'HEAD',

    [switch]$ListPolicy,

    [string]$ProvenanceFile
)

$ErrorActionPreference = 'Stop'

$policies = @{
    wsc = @(
        '^competition_project_single_camera/cpu/src/',
        '^competition_project_single_camera/cpu/tests/',
        '^competition_project_single_camera/cpu/README\.md$',
        '^competition_project_single_camera/cpu_bringup/uart1_hello_onchip/'
    )
    libaoxun = @(
        '^competition_project_single_camera/src/',
        '^competition_project_single_camera/tests/rtl/',
        '^competition_project_single_camera/tests/apb_reg_magic/',
        '^competition_project_single_camera/mem_test\.xml$',
        '^competition_project_single_camera/mem_test\.peri\.xml$',
        '^competition_project_single_camera/constrain\.sdc$',
        '^competition_project_single_camera/debug_[^/]+\.v$',
        '^competition_project_single_camera/ip/',
        '^competition_project_single_camera/embedded_sw/'
    )
    qzs = @(
        '^\.gitattributes$',
        '^\.gitignore$',
        '^AGENTS\.md$',
        '^CLAUDE\.md$',
        '^CURRENT_STATE\.md$',
        '^SESSION_HANDOFF\.md$',
        '^\.agents/skills/',
        '^docs/',
        '^final_project/docs/',
        '^learning_guides/',
        '^ppt_doc_outlines/',
        '^tools/',
        '^competition_project_single_camera/README\.md$',
        '^competition_project_single_camera/docs/',
        '^competition_project_single_camera/integration/',
        '^competition_project_single_camera/tools/',
        '^competition_project_single_camera/cpu_bringup/uart1_hello_onchip/README\.md$'
    )
}

$repoRoot = (& git rev-parse --show-toplevel).Trim()
if ([string]::IsNullOrWhiteSpace($repoRoot)) {
    throw 'TEAM_SCOPE_FAIL: not inside a Git repository'
}

$allowed = @($policies[$Role])
if ($ListPolicy) {
    Write-Host "TEAM_SCOPE_POLICY role=$Role"
    $allowed | ForEach-Object { Write-Host $_ }
    exit 0
}

function Test-RolePathAllowed {
    param([Parameter(Mandatory = $true)][string]$Path)
    foreach ($pattern in $allowed) {
        if ($Path -match $pattern) { return $true }
    }
    return $false
}

& git -C $repoRoot rev-parse --verify --quiet $BaseRef *> $null
if ($LASTEXITCODE -ne 0) {
    throw "TEAM_SCOPE_FAIL: invalid base ref: $BaseRef"
}
& git -C $repoRoot rev-parse --verify --quiet $TargetRef *> $null
if ($LASTEXITCODE -ne 0) {
    throw "TEAM_SCOPE_FAIL: invalid target ref: $TargetRef"
}

if (-not [string]::IsNullOrWhiteSpace($ProvenanceFile)) {
    if ($Role -ne 'qzs') {
        throw 'TEAM_SCOPE_FAIL: merge provenance mode is defined only for qzs candidate integration review'
    }
    if (-not (Test-Path -LiteralPath $ProvenanceFile -PathType Leaf)) {
        throw "TEAM_SCOPE_FAIL: provenance file not found: $ProvenanceFile"
    }

    try {
        $provenance = Get-Content -LiteralPath $ProvenanceFile -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    catch {
        throw "TEAM_SCOPE_FAIL: invalid provenance JSON: $($_.Exception.Message)"
    }
    if ($provenance.schema -ne 'cpu-hello-merge-provenance-v1') {
        throw 'TEAM_SCOPE_FAIL: unsupported provenance schema'
    }

    $anchor = [string]$provenance.merge_anchor
    $uart1Parent = [string]$provenance.uart1_parent
    $candidateParent = [string]$provenance.candidate_parent
    foreach ($ref in @($anchor, $uart1Parent, $candidateParent)) {
        & git -C $repoRoot rev-parse --verify --quiet $ref *> $null
        if ($LASTEXITCODE -ne 0) { throw "TEAM_SCOPE_FAIL: invalid provenance ref: $ref" }
    }
    $anchorParents = @((& git -C $repoRoot show -s --format=%P $anchor).Trim().Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries))
    if ($anchorParents -notcontains $uart1Parent -or $anchorParents -notcontains $candidateParent) {
        throw 'TEAM_SCOPE_FAIL: merge anchor parents do not match provenance parents'
    }
    & git -C $repoRoot merge-base --is-ancestor $anchor $TargetRef
    if ($LASTEXITCODE -ne 0) { throw 'TEAM_SCOPE_FAIL: target is not descended from merge anchor' }

    $preserved = @($provenance.preserved_candidate_p1_paths | ForEach-Object { [string]$_ })
    $resolutionPaths = @($provenance.qzs_merge_resolution_paths | ForEach-Object { [string]$_ })
    $postAnchorExceptions = @($provenance.post_anchor_qzs_exception_paths | ForEach-Object { [string]$_ })
    $anchorPaths = @(& git -C $repoRoot diff --name-only --diff-filter=ACMR $uart1Parent $anchor -- |
        ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $_ -replace '\\', '/' } } |
        Sort-Object -Unique)
    $violations = New-Object 'System.Collections.Generic.List[string]'
    foreach ($path in $anchorPaths) {
        if (Test-RolePathAllowed -Path $path) { continue }
        if ($resolutionPaths -contains $path) { continue }
        if ($preserved -contains $path) {
            $anchorBlob = (& git -C $repoRoot rev-parse "$anchor`:$path" 2>$null).Trim()
            $candidateBlob = (& git -C $repoRoot rev-parse "$candidateParent`:$path" 2>$null).Trim()
            if (-not [string]::IsNullOrWhiteSpace($anchorBlob) -and $anchorBlob -eq $candidateBlob) { continue }
            $violations.Add("anchor preserved-path blob mismatch: $path")
            continue
        }
        $violations.Add("anchor unassigned path: $path")
    }

    $postPaths = New-Object 'System.Collections.Generic.List[string]'
    @(& git -C $repoRoot diff --name-only --diff-filter=ACMR $anchor $TargetRef --) |
        ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $postPaths.Add(($_ -replace '\\', '/')) } }
    if ($TargetRef -eq 'HEAD') {
        @(& git -C $repoRoot diff --name-only --diff-filter=ACMR $TargetRef --) |
            ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $postPaths.Add(($_ -replace '\\', '/')) } }
        @(& git -C $repoRoot diff --cached --name-only --diff-filter=ACMR $TargetRef --) |
            ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $postPaths.Add(($_ -replace '\\', '/')) } }
        @(& git -C $repoRoot ls-files --others --exclude-standard) |
            ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $postPaths.Add(($_ -replace '\\', '/')) } }
    }
    $postPaths = @($postPaths | Sort-Object -Unique)
    foreach ($path in $postPaths) {
        if ((Test-RolePathAllowed -Path $path) -or $postAnchorExceptions -contains $path) { continue }
        $violations.Add("post-anchor out-of-scope path: $path")
    }
    if ($violations.Count -gt 0) {
        foreach ($violation in $violations) { Write-Host "FAIL: $violation" }
        Write-Host "TEAM_SCOPE=FAIL role=$Role mode=merge-provenance anchor_paths=$($anchorPaths.Count) post_anchor_paths=$($postPaths.Count) violations=$($violations.Count)"
        exit 1
    }
    Write-Host "TEAM_SCOPE=PASS role=$Role mode=merge-provenance anchor=$anchor anchor_paths=$($anchorPaths.Count) post_anchor_paths=$($postPaths.Count)"
    exit 0
}

$paths = New-Object 'System.Collections.Generic.List[string]'
@(& git -C $repoRoot diff --name-only --diff-filter=ACMR $BaseRef $TargetRef --) |
    ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $paths.Add(($_ -replace '\\', '/')) } }
if ($TargetRef -eq 'HEAD') {
    @(& git -C $repoRoot diff --name-only --diff-filter=ACMR $TargetRef --) |
        ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $paths.Add(($_ -replace '\\', '/')) } }
    @(& git -C $repoRoot diff --cached --name-only --diff-filter=ACMR $TargetRef --) |
        ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $paths.Add(($_ -replace '\\', '/')) } }
    @(& git -C $repoRoot ls-files --others --exclude-standard) |
        ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $paths.Add(($_ -replace '\\', '/')) } }
}

$changedPaths = @($paths | Sort-Object -Unique)
$violations = New-Object 'System.Collections.Generic.List[string]'
foreach ($path in $changedPaths) {
    if (-not (Test-RolePathAllowed -Path $path)) { $violations.Add($path) }
}

if ($violations.Count -gt 0) {
    foreach ($path in $violations) { Write-Host "FAIL: out-of-scope path for $Role`: $path" }
    Write-Host "TEAM_SCOPE=FAIL role=$Role changed=$($changedPaths.Count) violations=$($violations.Count)"
    exit 1
}

Write-Host "TEAM_SCOPE=PASS role=$Role changed=$($changedPaths.Count) base=$BaseRef target=$TargetRef"
exit 0
