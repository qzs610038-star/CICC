[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$DesignRoot,
    [Parameter(Mandatory)] [string]$EvidenceRoot,
    [Parameter(Mandatory)] [string]$OutflowRoot,
    [Parameter(Mandatory)] [string]$WorkRoot,
    [Parameter(Mandatory)] [string]$EfinityRoot,
    [Parameter(Mandatory)] [string]$RiscvToolchainBin,
    [Parameter(Mandatory)] [string]$MakePath
)

$ErrorActionPreference = 'Stop'
$scriptRoot = $PSScriptRoot
$evidenceProjectRoot = (Resolve-Path (Join-Path $scriptRoot '..\..')).Path
$projectRoot = Join-Path (Resolve-Path -LiteralPath $DesignRoot).Path 'competition_project_single_camera'
$designRoot = (Resolve-Path -LiteralPath $DesignRoot).Path
$manifest = Get-Content -LiteralPath (Join-Path $scriptRoot 'I0_UART1_BUILD_MANIFEST.json') -Raw | ConvertFrom-Json
$inputFile = Join-Path $scriptRoot 'I0_UART1_BUILD_INPUTS.sha256'

function Assert-SafeRelativePath([string]$Path, [string]$Label) {
    if ([string]::IsNullOrWhiteSpace($Path) -or [IO.Path]::IsPathRooted($Path) -or $Path -match '(^|[\\/])\.\.([\\/]|$)' -or $Path -match '^[A-Za-z]:') { throw "Unsafe $Label path: $Path" }
}

function Join-CheckedPath([string]$Root, [string]$Relative, [string]$Label) {
    Assert-SafeRelativePath $Relative $Label
    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
    $candidate = [IO.Path]::GetFullPath((Join-Path $resolvedRoot $Relative))
    if (-not $candidate.StartsWith($resolvedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Escaping $Label path: $Relative" }
    return $candidate
}

function Assert-HashAndSize([string]$Path, [string]$ExpectedHash, [Int64]$ExpectedSize, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing ${Label}: $Path" }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -ne $ExpectedSize) { throw "Size mismatch $Label" }
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $ExpectedHash) { throw "SHA-256 mismatch $Label" }
}

function Get-ToolVersion([string]$Name, [string]$Path) {
    switch ($Name) {
        'efx_map.exe' { return (Get-Item -LiteralPath $Path).VersionInfo.ProductVersion }
        'riscv-none-embed-gcc.exe' { return ((& $Path --version | Select-Object -First 1) -replace '^.*\)\s*', '') }
        'make.exe' { return ((& $Path --version | Select-Object -First 1) -replace '^GNU Make\s+', '') }
        default { throw "No version probe for tool: $Name" }
    }
}

if ((git -C $designRoot rev-parse HEAD).Trim() -ne $manifest.design_sha) { throw 'HEAD does not equal design_sha.' }
if ((git -C $designRoot status --porcelain).Length -ne 0) { throw 'Design worktree is dirty.' }
& git -C $designRoot diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed.' }

$seenInputs = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
$inputLines = @(Get-Content -LiteralPath $inputFile | Where-Object { $_.Trim() -ne '' })
foreach ($line in $inputLines) {
    if ($line -notmatch '^(?<hash>[0-9A-F]{64})  (?<path>.+)$') { throw "Invalid input line: $line" }
    $relative = $Matches.path
    Assert-SafeRelativePath $relative 'input'
    if (-not $seenInputs.Add($relative)) { throw "Duplicate input: $relative" }
    $path = Join-CheckedPath $projectRoot $relative 'input'
    if ((git -C $projectRoot ls-files --error-unmatch -- $relative 2>$null).Trim() -ne $relative) { throw "Untracked input: $relative" }
    Assert-HashAndSize $path $Matches.hash (Get-Item -LiteralPath $path).Length "input $relative"
}

# Exact source identity is checked independently of the evidence commit.
$inputPaths = @($seenInputs | ForEach-Object { $_ })
& git -C $projectRoot diff --quiet $manifest.design_sha -- $inputPaths
if ($LASTEXITCODE -ne 0) { throw 'Atomic design inputs differ from design_sha.' }

$seenTools = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
foreach ($tool in $manifest.toolchain_files) {
    Assert-SafeRelativePath $tool.relative_path 'toolchain'
    $key = "$($tool.root)/$($tool.relative_path)"
    if (-not $seenTools.Add($key)) { throw "Duplicate toolchain entry: $key" }
    $root = switch ($tool.root) { 'efinity' { $EfinityRoot } 'riscv' { $RiscvToolchainBin } 'make' { Split-Path -Parent $MakePath } default { throw "Unknown tool root: $($tool.root)" } }
    $path = if ($tool.root -eq 'make') { $MakePath } else { Join-CheckedPath $root $tool.relative_path 'toolchain' }
    Assert-HashAndSize $path $tool.sha256 ([Int64]$tool.size) "toolchain $key"
    if ($tool.PSObject.Properties.Name -contains 'version' -and (Get-ToolVersion $tool.name $path) -ne $tool.version) { throw "Tool version mismatch: $key" }
}

$seenArtifacts = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
foreach ($artifact in $manifest.artifacts) {
    Assert-SafeRelativePath $artifact.path 'artifact'
    $key = "$($artifact.root)/$($artifact.path)"
    if (-not $seenArtifacts.Add($key)) { throw "Duplicate artifact: $key" }
    $root = switch ($artifact.root) { 'evidence' { $EvidenceRoot } 'outflow' { $OutflowRoot } 'work' { $WorkRoot } 'project' { $projectRoot } default { throw "Unknown artifact root: $($artifact.root)" } }
    Assert-HashAndSize (Join-CheckedPath $root $artifact.path 'artifact') $artifact.sha256 ([Int64]$artifact.size) "artifact $key"
}

"I0_UART1_BUILD_EVIDENCE=PASS batch=$($manifest.batch_id) design_sha=$($manifest.design_sha) inputs=$($inputLines.Count) artifacts=$($manifest.artifacts.Count)"
