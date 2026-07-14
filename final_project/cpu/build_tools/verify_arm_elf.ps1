Param(
    [Parameter(Mandatory = $true)][string]$Elf,
    [Parameter(Mandatory = $true)][ValidateSet('competition', 'arm_bringup')][string]$Profile,
    [Parameter(Mandatory = $true)][ValidateSet('disabled', 'simulated')][string]$Backend,
    [Parameter(Mandatory = $true)][string]$Manifest,
    [Parameter(Mandatory = $true)][string]$NmPath,
    [Parameter(Mandatory = $true)][string]$Map,
    [Parameter(Mandatory = $true)][string]$Listing
)

$ErrorActionPreference = 'Stop'
foreach ($path in @($Elf, $Manifest, $NmPath, $Map, $Listing)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing verification input: $path"
    }
}

$manifestData = Get-Content -Raw -LiteralPath $Manifest | ConvertFrom-Json
function Require-ManifestProperty([string]$Name) {
    if (-not $manifestData.PSObject.Properties.Name.Contains($Name) -or
        $null -eq $manifestData.$Name) {
        throw "Manifest is missing required property: $Name"
    }
}
foreach ($name in @('schema', 'canonical_builder', 'profile', 'backend',
                     'not_for_flash', 'board_build', 'entry_symbol',
                     'compile_flags', 'link_flags', 'source_files', 'startup',
                     'linker', 'warning_policy', 'artifact_sha256')) {
    Require-ManifestProperty $name
}
if ($manifestData.schema -ne 'mycobot-g1-g3-manifest-v2') {
    throw 'Manifest schema is not the strict G1-G3 schema.'
}
if ($manifestData.canonical_builder -ne
    'final_project/cpu/build_tools/build_arm_profile.ps1') {
    throw 'Manifest did not come from the canonical G1-G3 builder.'
}
if ($manifestData.profile -ne $Profile -or $manifestData.backend -ne $Backend) {
    throw 'Manifest profile/backend does not match the ELF verification request.'
}
if ($manifestData.not_for_flash -ne $true -or $manifestData.board_build -ne $false) {
    throw 'G0-G3 validation artifacts must be NOT_FOR_FLASH and must not be BoardBuild artifacts.'
}
if ($manifestData.entry_symbol -ne '_start') {
    throw 'G1-G3 artifact does not declare the reviewed _start entry symbol.'
}

function Assert-HashedFile($Entry, [string]$Role) {
    if ($null -eq $Entry -or -not $Entry.path -or -not $Entry.sha256) {
        throw "Manifest $Role entry lacks path or SHA-256."
    }
    if (-not (Test-Path -LiteralPath $Entry.path)) {
        throw "Manifest $Role path does not exist: $($Entry.path)"
    }
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Entry.path).Hash
    if ($actualHash -ne $Entry.sha256) {
        throw "Manifest $Role SHA-256 mismatch: $($Entry.path)"
    }
}

$forbiddenPattern = '(?i)(mycobot_uart|mycobot_protocol|mycobot_transport|uart2|system_uart_2)'
$sourceEntries = @($manifestData.source_files)
if ($sourceEntries.Count -eq 0) { throw 'Manifest source_files is empty.' }
foreach ($entry in $sourceEntries) {
    Assert-HashedFile $entry 'source_files'
    if ($entry.path -match $forbiddenPattern -or
        (Get-Content -Raw -LiteralPath $entry.path) -match $forbiddenPattern) {
        throw "Forbidden UART2/real-transport evidence in manifest source: $($entry.path)"
    }
}
Assert-HashedFile $manifestData.startup 'startup'
Assert-HashedFile $manifestData.linker 'linker'
foreach ($artifactName in @('elf', 'map', 'hex', 'bin', 'lst')) {
    if (-not $manifestData.artifact_sha256.PSObject.Properties.Name.Contains($artifactName)) {
        throw "Manifest is missing artifact SHA-256: $artifactName"
    }
    Assert-HashedFile $manifestData.artifact_sha256.$artifactName "artifact_sha256.$artifactName"
}

$compileFlags = @($manifestData.compile_flags) -join "`n"
$linkFlags = @($manifestData.link_flags) -join "`n"
if ($compileFlags -match $forbiddenPattern -or $linkFlags -match $forbiddenPattern) {
    throw 'Forbidden UART2/real-transport token appears in build flags.'
}
if ($compileFlags -notmatch '-Werror' -or $linkFlags -notmatch '--fatal-warnings') {
    throw 'Strict compiler/linker warning policy is missing from the manifest.'
}
if (@($manifestData.warning_policy.allowed_patterns).Count -ne 1 -or
    @($manifestData.warning_policy.allowed_patterns)[0] -notmatch 'APB3 base address') {
    throw 'Manifest warning policy does not contain the single allowed standalone warning.'
}

$nmOutput = @(& $NmPath '--defined-only' '--format=posix' $Elf 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "nm failed: $($nmOutput -join "`n")"
}
$symbolNames = @($nmOutput | ForEach-Object {
    $fields = ([string]$_).Trim() -split '\s+'
    if ($fields.Count -ge 1 -and $fields[0]) { $fields[0] }
})
function Require-ExactSymbol([string]$Name) {
    if ($symbolNames -notcontains $Name) {
        throw "Required linked symbol is missing: $Name"
    }
}
function Forbid-SymbolPattern([string]$Pattern) {
    $matches = @($symbolNames | Where-Object { $_ -match $Pattern })
    if ($matches.Count -ne 0) {
        throw "Forbidden G0-G3 path is linked: $($matches -join ', ')"
    }
}

Require-ExactSymbol '_start'
Require-ExactSymbol 'round_controller_tick'
Require-ExactSymbol 'arm_runtime_tick'
Forbid-SymbolPattern $forbiddenPattern

if ($Backend -eq 'simulated') {
    Require-ExactSymbol 'arm_controller_tick'
    Require-ExactSymbol 'arm_sim_transport_init'
} else {
    Forbid-SymbolPattern '(?i)(arm_controller_tick|arm_sim_transport|mycobot_protocol|mycobot_transport)'
}

foreach ($evidencePath in @($Map, $Listing)) {
    if ((Get-Content -Raw -LiteralPath $evidencePath) -match $forbiddenPattern) {
        throw "Forbidden UART2/real-transport evidence appears in $evidencePath"
    }
}

Write-Output "ELF PASS profile=$Profile backend=$Backend entry=_start not_for_flash=$($manifestData.not_for_flash) uart2=excluded"
