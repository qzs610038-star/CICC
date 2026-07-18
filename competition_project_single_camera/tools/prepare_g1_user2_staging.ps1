[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ManifestPath,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$BitPath,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ElfPath,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$StagingRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-AsciiPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ($Path -match '[^\x00-\x7F]') {
        throw "StagingRoot must be an ASCII path: $Path"
    }
}

function Get-ArtifactIdentity {
    param([Parameter(Mandatory = $true)][string]$Path)

    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    [ordered]@{
        path = $item.FullName
        size_bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
}

function Copy-VerifiedArtifact {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][string]$ExpectedSha256,
        [Parameter(Mandatory = $true)][long]$ExpectedSize
    )

    if (Test-Path -LiteralPath $DestinationPath -PathType Leaf) {
        $existing = Get-ArtifactIdentity -Path $DestinationPath
        if ($existing.size_bytes -ne $ExpectedSize -or $existing.sha256 -ne $ExpectedSha256) {
            throw "Staging conflict; existing target differs and will not be overwritten: $DestinationPath"
        }
        return [ordered]@{ action = 'retained_matching_existing_file'; identity = $existing }
    }

    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -ErrorAction Stop
    $copied = Get-ArtifactIdentity -Path $DestinationPath
    if ($copied.size_bytes -ne $ExpectedSize -or $copied.sha256 -ne $ExpectedSha256) {
        throw "Post-copy identity mismatch: $DestinationPath"
    }
    return [ordered]@{ action = 'copied'; identity = $copied }
}

Assert-AsciiPath -Path $StagingRoot
$preflight = Join-Path $PSScriptRoot 'capture_g1_user2_artifact_preflight.ps1'
& $preflight -ManifestPath $ManifestPath -BitPath $BitPath -ElfPath $ElfPath

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json -ErrorAction Stop
$stagingRootFull = [System.IO.Path]::GetFullPath($StagingRoot)
Assert-AsciiPath -Path $stagingRootFull
New-Item -ItemType Directory -Path $stagingRootFull -Force | Out-Null

$bitTarget = Join-Path $stagingRootFull $manifest.bitstream.logical_name
$elfTarget = Join-Path $stagingRootFull $manifest.hello_elf.logical_name
$bitResult = Copy-VerifiedArtifact -SourcePath $BitPath -DestinationPath $bitTarget -ExpectedSha256 $manifest.bitstream.sha256 -ExpectedSize ([long]$manifest.bitstream.size_bytes)
$elfResult = Copy-VerifiedArtifact -SourcePath $ElfPath -DestinationPath $elfTarget -ExpectedSha256 $manifest.hello_elf.sha256 -ExpectedSize ([long]$manifest.hello_elf.size_bytes)

[pscustomobject]@{
    result = 'G1_CURRENT_BATCH_STAGING_READY_NO_HARDWARE_ACTION'
    batch_id = $manifest.batch_id
    staging_root = $stagingRootFull
    bitstream = $bitResult
    hello_elf = $elfResult
    hardware_actions_performed = $false
} | ConvertTo-Json -Depth 6
