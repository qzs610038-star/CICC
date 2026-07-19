param(
    [Parameter(Mandatory = $true)][string]$BundleDir,
    [string]$ManifestPath
)

$ErrorActionPreference = 'Stop'

function Get-NormalizedTextSha256([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $normalized = New-Object IO.MemoryStream
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        if ($bytes[$i] -eq 13 -and ($i + 1) -lt $bytes.Length -and $bytes[$i + 1] -eq 10) {
            $normalized.WriteByte(10)
            $i++
        } else {
            $normalized.WriteByte($bytes[$i])
        }
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($normalized.ToArray()))).Replace('-','') }
    finally { $sha.Dispose(); $normalized.Dispose() }
}

$bundle = (Resolve-Path -LiteralPath $BundleDir).Path
if (-not $ManifestPath) { $ManifestPath = Join-Path $bundle 'manifest.json' }
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "P1_MANIFEST_MISSING path=$ManifestPath"
}
$manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
if ($manifest.schema -ne 'p1-host-replay-bundle-v1') {
    throw "P1_MANIFEST_SCHEMA expected=p1-host-replay-bundle-v1 actual=$($manifest.schema)"
}
$expectedPolicy = 'SHA-256 of file bytes after CRLF byte pairs are normalized to LF; lone CR bytes are preserved'
if ($manifest.text_hash_policy -ne $expectedPolicy) {
    throw "P1_MANIFEST_HASH_POLICY expected='$expectedPolicy' actual='$($manifest.text_hash_policy)'"
}

$files = @('rounds.jsonl','runner.txt','compile.txt','tamper.jsonl')
foreach ($name in $files) {
    $path = Join-Path $bundle $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "P1_MANIFEST_FILE_MISSING file=$name"
    }
    $property = $manifest.files_sha256.psobject.Properties[$name]
    if (-not $property) { throw "P1_MANIFEST_HASH_MISSING file=$name" }
    $actual = Get-NormalizedTextSha256 $path
    if ($actual -ne $property.Value) {
        throw "P1_MANIFEST_FILE_HASH file=$name expected=$($property.Value) actual=$actual"
    }
}

Write-Host "P1_MANIFEST_HASHES=PASS files=$($files.Count) policy=LF_NORMALIZED"
