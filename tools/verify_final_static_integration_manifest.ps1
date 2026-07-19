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
if ($manifest.schema_version -ne 1 -or -not $manifest.clean_checkout_verified) {
    throw 'FINAL_MANIFEST_FAIL: manifest schema or clean-checkout assertion is invalid'
}

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
    if ($relativePath -like '*.ps1') {
        $eol = @(& git check-attr eol -- $relativePath)
        if ($LASTEXITCODE -ne 0 -or $eol.Count -ne 1 -or $eol[0] -notmatch ': eol: crlf$') {
            $failures.Add("PS1 EOL policy mismatch: $relativePath")
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
