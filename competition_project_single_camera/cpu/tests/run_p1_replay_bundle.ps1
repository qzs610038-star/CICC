param(
    [Parameter(Mandatory = $true)][string]$RunDir,
    [Parameter(Mandatory = $true)][string]$SchemaPath,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-f]{40}$')][string]$ImplementationGitSha,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-f]{40}$')][string]$GovernanceGitSha
)

$ErrorActionPreference = 'Stop'
$cpuRoot = Split-Path -Parent $PSScriptRoot
$include = Join-Path $cpuRoot 'src'
$test = Join-Path $PSScriptRoot 'p1_replay_runner.c'
$source = Join-Path $cpuRoot 'src\p1_host_model.c'
$records = Join-Path $RunDir 'rounds.jsonl'
$rawLog = Join-Path $RunDir 'runner.txt'
$build = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-p1-replay-' + [guid]::NewGuid().ToString('N'))
$exe = Join-Path $build 'p1_replay_runner.exe'

New-Item -ItemType Directory -Force -Path $RunDir,$build | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'P1_REPLAY_BLOCKED_COMPILER' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$compileLog = Join-Path $RunDir 'compile.txt'
$command = 'pushd "' + $build + '" && call "' + $vcvars + '" x64 >nul && cl.exe /nologo /utf-8 /std:c11 /W4 /WX /I"' +
    $include + '" /Fe"' + $exe + '" "' + $test + '" "' + $source + '" && popd'
cmd.exe /d /s /c $command 2>&1 | Tee-Object $compileLog
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe $records $rawLog
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot 'validate_p1_vectors.ps1') -SchemaPath $SchemaPath -ReplayPath $records
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$files = @('rounds.jsonl','runner.txt','compile.txt')
$hashes = [ordered]@{}
foreach ($name in $files) {
    $hashes[$name] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $RunDir $name)).Hash
}
function Get-NormalizedTextSha256([string]$Path) {
    $text = (Get-Content -Raw -LiteralPath $Path) -replace "`r`n", "`n"
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($text)))).Replace('-','') }
    finally { $sha.Dispose() }
}
$hashes['test_source_lf'] = Get-NormalizedTextSha256 $test
$hashes['model_source_lf'] = Get-NormalizedTextSha256 $source
$hashes['vector_file_lf'] = Get-NormalizedTextSha256 (Join-Path $PSScriptRoot 'vectors\p1\p1_contract_vectors.jsonl')
$hashes['schema_file_lf'] = Get-NormalizedTextSha256 $SchemaPath
$manifest = [ordered]@{
    schema = 'p1-host-replay-bundle-v1'
    status = 'AWAITING_QZS_REVIEW'
    implementation_git_sha = $ImplementationGitSha
    governance_git_sha = $GovernanceGitSha
    compiler_policy = 'MSVC C11 /W4 /WX'
    text_hash_policy = 'UTF-8 with CRLF normalized to LF'
    round_count = 20
    task_counts = [ordered]@{'1'=5;'2'=5;'3'=5;'4'=5}
    required_negative_cases = @('ABANDON','TIMEOUT','RESET','NO_SECOND_RESULT')
    arm_enabled = 0
    board_verified = $false
    files_sha256 = $hashes
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -Encoding ascii (Join-Path $RunDir 'manifest.json')
Write-Host 'P1_REPLAY_BUNDLE=PASS rounds=20 ARM=0 status=AWAITING_QZS_REVIEW'
