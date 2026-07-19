[CmdletBinding()]
param(
    [string]$ManifestPath = 'competition_project_single_camera/integration/FINAL_STATIC_INTEGRATION_MANIFEST_20260719.json'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (& git rev-parse --show-toplevel).Trim()
if ([string]::IsNullOrWhiteSpace($repoRoot)) { throw 'FINAL_MANIFEST_FAIL: not inside a Git repository' }
$manifestFile = Join-Path $repoRoot $ManifestPath
if (-not (Test-Path -LiteralPath $manifestFile -PathType Leaf)) { throw "FINAL_MANIFEST_FAIL: missing manifest: $ManifestPath" }
$manifest = Get-Content -Raw -LiteralPath $manifestFile | ConvertFrom-Json
if ($manifest.schema_version -ne 2 -or -not $manifest.clean_checkout_verified) {
    throw 'FINAL_MANIFEST_FAIL: manifest schema or clean-checkout assertion is invalid'
}

function Get-RequiredEol {
    param([Parameter(Mandatory = $true)][string]$Path)

    $extension = [IO.Path]::GetExtension($Path).ToLowerInvariant()
    if ($extension -eq '.ps1') { return 'crlf' }
    if ($extension -in @('.gdb', '.cfg')) { return 'lf' }
    return $null
}

function Get-EolProfile {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [IO.File]::ReadAllBytes($Path)
    $crlfCount = 0
    $bareLfCount = 0
    $bareCrCount = 0
    for ($index = 0; $index -lt $bytes.Length; $index++) {
        if ($bytes[$index] -eq 13) {
            if ($index + 1 -ge $bytes.Length -or $bytes[$index + 1] -ne 10) { $bareCrCount++ }
        } elseif ($bytes[$index] -eq 10) {
            if ($index -gt 0 -and $bytes[$index - 1] -eq 13) { $crlfCount++ } else { $bareLfCount++ }
        }
    }
    $actual = if ($bareCrCount -gt 0 -or ($crlfCount -gt 0 -and $bareLfCount -gt 0)) {
        'mixed_or_invalid'
    } elseif ($crlfCount -gt 0) {
        'crlf'
    } elseif ($bareLfCount -gt 0) {
        'lf'
    } else {
        'none'
    }
    return [ordered]@{
        actual_eol = $actual
        crlf_count = $crlfCount
        bare_lf_count = $bareLfCount
        bare_cr_count = $bareCrCount
    }
}

$sourceCommit = [string]$manifest.source_commit
& git -C $repoRoot rev-parse --verify --quiet "$sourceCommit^{commit}" *> $null
if ($LASTEXITCODE -ne 0) { throw "FINAL_MANIFEST_FAIL: invalid source commit: $sourceCommit" }
& git -C $repoRoot merge-base --is-ancestor $sourceCommit HEAD
if ($LASTEXITCODE -ne 0) { throw "FINAL_MANIFEST_FAIL: source commit is not an ancestor of HEAD: $sourceCommit" }
if ((& git -C $repoRoot status --porcelain).Length -ne 0) { throw 'FINAL_MANIFEST_FAIL: verification requires a clean checkout' }

$failures = New-Object 'System.Collections.Generic.List[string]'
foreach ($entry in @($manifest.execution_files)) {
    $relativePath = [string]$entry.path
    if ([IO.Path]::IsPathRooted($relativePath) -or $relativePath -match '(^|[\\/])\.\.([\\/]|$)') {
        $failures.Add("unsafe path: $relativePath")
        continue
    }
    $absolutePath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        $failures.Add("missing file: $relativePath")
        continue
    }
    $checkoutHash = (Get-FileHash -LiteralPath $absolutePath -Algorithm SHA256).Hash
    if ($checkoutHash -cne [string]$entry.checkout_sha256) {
        $failures.Add("checkout SHA-256 mismatch: $relativePath")
    }
    $blob = (& git rev-parse "HEAD`:$relativePath").Trim()
    if ($LASTEXITCODE -ne 0 -or $blob -cne [string]$entry.git_blob_sha1) {
        $failures.Add("Git blob SHA mismatch: $relativePath")
    }
    $requiredEol = Get-RequiredEol -Path $relativePath
    if ($null -ne $requiredEol) {
        $eol = @(& git check-attr eol -- $relativePath)
        if ($LASTEXITCODE -ne 0 -or $eol.Count -ne 1 -or $eol[0] -notmatch (': eol: ' + [regex]::Escape($requiredEol) + '$')) {
            $failures.Add("EOL attribute mismatch: $relativePath expected=$requiredEol")
        }
        $profile = Get-EolProfile -Path $absolutePath
        if ($profile.actual_eol -cne $requiredEol -or $profile.bare_cr_count -ne 0) {
            $failures.Add("actual EOL bytes mismatch: $relativePath expected=$requiredEol actual=$($profile.actual_eol)")
        }
        foreach ($field in @('actual_eol', 'crlf_count', 'bare_lf_count', 'bare_cr_count')) {
            if ($entry.PSObject.Properties.Name -notcontains $field -or [string]$entry.$field -cne [string]$profile.$field) {
                $failures.Add("manifest EOL profile mismatch: $relativePath field=$field")
            }
        }
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) { Write-Host "FAIL: $failure" }
    Write-Host "FINAL_STATIC_INTEGRATION_MANIFEST=FAIL failures=$($failures.Count)"
    exit 1
}

$head = (& git rev-parse HEAD).Trim()
Write-Host "FINAL_STATIC_INTEGRATION_MANIFEST=PASS final_head=$head source_commit=$($manifest.source_commit) files=$(@($manifest.execution_files).Count)"
