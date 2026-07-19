[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('wsc', 'libaoxun', 'qzs')]
    [string]$Role,

    [string]$BaseRef = 'HEAD',

    [string]$TargetRef = 'HEAD',

    [switch]$ListPolicy
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

& git -C $repoRoot rev-parse --verify --quiet $BaseRef *> $null
if ($LASTEXITCODE -ne 0) {
    throw "TEAM_SCOPE_FAIL: invalid base ref: $BaseRef"
}
& git -C $repoRoot rev-parse --verify --quiet $TargetRef *> $null
if ($LASTEXITCODE -ne 0) {
    throw "TEAM_SCOPE_FAIL: invalid target ref: $TargetRef"
}

$paths = New-Object 'System.Collections.Generic.List[string]'
@(& git -C $repoRoot diff --name-only --diff-filter=ACMR $BaseRef $TargetRef --) |
    ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $paths.Add(($_ -replace '\\', '/')) } }
if ($TargetRef -eq 'HEAD') {
    @(& git -C $repoRoot ls-files --others --exclude-standard) |
        ForEach-Object { if (-not [string]::IsNullOrWhiteSpace($_)) { $paths.Add(($_ -replace '\\', '/')) } }
}

$changedPaths = @($paths | Sort-Object -Unique)
$violations = New-Object 'System.Collections.Generic.List[string]'
foreach ($path in $changedPaths) {
    $isAllowed = $false
    foreach ($pattern in $allowed) {
        if ($path -match $pattern) {
            $isAllowed = $true
            break
        }
    }
    if (-not $isAllowed) { $violations.Add($path) }
}

if ($violations.Count -gt 0) {
    foreach ($path in $violations) { Write-Host "FAIL: out-of-scope path for $Role`: $path" }
    Write-Host "TEAM_SCOPE=FAIL role=$Role changed=$($changedPaths.Count) violations=$($violations.Count)"
    exit 1
}

Write-Host "TEAM_SCOPE=PASS role=$Role changed=$($changedPaths.Count) base=$BaseRef target=$TargetRef"
exit 0
