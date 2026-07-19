[CmdletBinding()]
param(
    [string]$SourceRef = 'HEAD',
    [string]$OutputPath = 'competition_project_single_camera/integration/FINAL_STATIC_INTEGRATION_MANIFEST_20260719.json'
)

$ErrorActionPreference = 'Stop'

function Get-AttributeValue {
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)][string]$Name)

    $line = @(& git check-attr $Name -- $Path)
    if ($LASTEXITCODE -ne 0 -or $line.Count -ne 1) { throw "MANIFEST_FAIL: cannot read $Name attribute for $Path" }
    if ($line[0] -notmatch ('^' + [regex]::Escape($Path) + ': ' + [regex]::Escape($Name) + ': (?<value>.+)$')) {
        throw "MANIFEST_FAIL: malformed attribute output for $Path"
    }
    return $Matches.value
}

function Get-RequiredEol {
    param([Parameter(Mandatory = $true)][string]$Path)

    $extension = [IO.Path]::GetExtension($Path).ToLowerInvariant()
    if ($extension -eq '.ps1') { return 'crlf' }
    if ($extension -in @('.gdb', '.cfg')) { return 'lf' }
    return $null
}

$repoRoot = (& git rev-parse --show-toplevel).Trim()
if ([string]::IsNullOrWhiteSpace($repoRoot)) { throw 'MANIFEST_FAIL: not inside a Git repository' }
Set-Location -LiteralPath $repoRoot

$commit = (& git rev-parse --verify "$SourceRef^{commit}").Trim()
if ($LASTEXITCODE -ne 0) { throw "MANIFEST_FAIL: invalid source ref: $SourceRef" }
if ((& git rev-parse HEAD).Trim() -ne $commit) { throw 'MANIFEST_FAIL: SourceRef must equal checked-out HEAD' }
if ((& git status --porcelain).Length -ne 0) { throw 'MANIFEST_FAIL: manifest must be generated from a clean checkout' }

$files = @(
    'competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_MANIFEST.json',
    'competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_INPUTS.sha256',
    'competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_EVIDENCE.md',
    'competition_project_single_camera/embedded_sw/uart1_hello_onchip/rebuild_i0_uart1_clean.ps1',
    'competition_project_single_camera/embedded_sw/uart1_hello_onchip/verify_i0_uart1_build_evidence.ps1',
    'competition_project_single_camera/cpu/tests/run_g2_host_evidence.ps1',
    'competition_project_single_camera/cpu/tests/test_single_camera_classifier.c',
    'tools/interface_freeze_check.ps1',
    'tools/team_scope_check.ps1',
    'competition_project_single_camera/docs/debug_sessions/I0_SMOKE_OPERATION_CARD_DRAFT_20260718.md',
    'docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md'
)

$entries = foreach ($relativePath in $files) {
    $absolutePath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) { throw "MANIFEST_FAIL: missing execution file: $relativePath" }
    $blob = (& git rev-parse "$commit`:$relativePath").Trim()
    if ($LASTEXITCODE -ne 0) { throw "MANIFEST_FAIL: $relativePath is absent from $commit" }
    $requiredEol = Get-RequiredEol -Path $relativePath
    $attributeEol = Get-AttributeValue -Path $relativePath -Name 'eol'
    if ($null -ne $requiredEol -and $attributeEol -cne $requiredEol) {
        throw "MANIFEST_FAIL: EOL policy mismatch for $relativePath expected=$requiredEol actual=$attributeEol"
    }
    [ordered]@{
        path = $relativePath
        checkout_sha256 = (Get-FileHash -LiteralPath $absolutePath -Algorithm SHA256).Hash
        git_blob_sha1 = $blob
        eol_attribute = $attributeEol
        required_eol = if ($null -eq $requiredEol) { 'repository_existing_rule' } else { $requiredEol }
    }
}

$manifest = [ordered]@{
    schema_version = 1
    purpose = 'final static integration identity; checkout bytes are authoritative for execution'
    source_commit = $commit
    clean_checkout_verified = $true
    eol_policy = [ordered]@{
        ps1 = 'CRLF'
        gdb = 'LF'
        cfg = 'LF'
        docs_json_source = 'repository existing .gitattributes rule'
    }
    ownership_input_audit = [ordered]@{
        claimed_exception_count = 7
        actual_acmr_cross_owner_input_count = 11
        interpretation = 'Eleven fixed upstream inputs are audited by their source role; they are not qzs write exceptions.'
        libaoxun_inputs = 5
        wsc_inputs = 2
        qzs_gate_and_governance_inputs = 4
    }
    execution_files = @($entries)
}

$target = Join-Path $repoRoot $OutputPath
$targetDirectory = Split-Path -Parent $target
if (-not (Test-Path -LiteralPath $targetDirectory -PathType Container)) { throw "MANIFEST_FAIL: output directory missing: $targetDirectory" }
$json = $manifest | ConvertTo-Json -Depth 6
[IO.File]::WriteAllText($target, ($json + "`n"), [Text.UTF8Encoding]::new($false))
Write-Host "FINAL_STATIC_INTEGRATION_MANIFEST=PASS commit=$commit files=$($entries.Count) output=$OutputPath"
