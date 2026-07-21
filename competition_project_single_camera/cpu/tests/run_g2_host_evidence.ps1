param(
    [Parameter(Mandatory = $true)]
    [string]$RunDir,
    [string]$PythonExe,
    [ValidateSet('Auto', 'Gcc', 'Msvc')]
    [string]$CompilerPreference = 'Auto'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$cpuRoot = Split-Path -Parent $PSScriptRoot
$include = Join-Path $cpuRoot 'include'
$test = Join-Path $PSScriptRoot 'test_single_camera_runtime.c'
$sources = @(
    (Join-Path $cpuRoot 'src\single_camera_runtime.c'),
    (Join-Path $cpuRoot 'src\single_camera_fake_transport.c'),
    (Join-Path $cpuRoot 'src\single_camera_mmio_transport.c'),
    (Join-Path $cpuRoot 'src\single_camera_feature_adapter.c'),
    (Join-Path $cpuRoot 'src\single_camera_classifier.c'),
    (Join-Path $cpuRoot 'src\single_camera_f1.c')
)
$commonGitDir = (& git rev-parse --path-format=absolute --git-common-dir).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commonGitDir)) {
    throw 'HOST_RUNTIME_BLOCKED_COMPILER: unable to resolve Git common directory'
}
$sharedRoot = Split-Path -Parent $commonGitDir
$fixedGccCandidates = @(
    (Join-Path $repoRoot 'tools\mingw64\bin\gcc.exe'),
    (Join-Path $sharedRoot 'tools\mingw64\bin\gcc.exe')
) | Select-Object -Unique
$gccPath = $null
if ($CompilerPreference -ne 'Msvc') {
    foreach ($candidate in $fixedGccCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $gccPath = (Resolve-Path -LiteralPath $candidate).Path
            break
        }
    }
    if ($null -eq $gccPath) {
        $gccCommand = Get-Command gcc -ErrorAction SilentlyContinue
        if ($null -ne $gccCommand) {
            $gccPath = $gccCommand.Source
        }
    }
}
$pythonPath = $null
if ($PSBoundParameters.ContainsKey('PythonExe')) {
    if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf)) {
        throw "HOST_RUNTIME_BLOCKED_PYTHON: specified Python executable not found: $PythonExe"
    }
    $pythonPath = (Resolve-Path -LiteralPath $PythonExe).Path
} else {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($null -ne $pythonCommand) {
        $pythonPath = $pythonCommand.Source
    } else {
        $bundledPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
        if (Test-Path -LiteralPath $bundledPython -PathType Leaf) {
            $pythonPath = $bundledPython
        }
    }
}
if ($null -eq $pythonPath) {
    throw 'HOST_RUNTIME_BLOCKED_PYTHON: no usable Python executable found'
}

function New-ArgvStep {
    param([Parameter(Mandatory = $true)][string]$Exe, [Parameter(Mandatory = $true)][object[]]$Args)
    return [pscustomobject]@{ exe = $Exe; args = @($Args) }
}

function Convert-StepsToBase64 {
    param([Parameter(Mandatory = $true)][object[]]$Steps)
    $record = [pscustomobject]@{ format = 'argv-json-v1'; steps = @($Steps) }
    $json = $record | ConvertTo-Json -Compress -Depth 8
    return [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($json))
}

function Invoke-Bundle {
    param(
        [Parameter(Mandatory = $true)][string]$BundleTool,
        [Parameter(Mandatory = $true)][string]$RawLog,
        [Parameter(Mandatory = $true)][string]$CompilerVersion,
        [Parameter(Mandatory = $true)][string]$TestCommandBase64,
        [Parameter(Mandatory = $true)][int]$TestExit
    )
    $createArgs = @($BundleTool, 'create', '--run-dir', $RunDir, '--raw-log', $RawLog,
        '--repo-root', $repoRoot, '--compiler', $CompilerVersion,
        '--test-command-base64', $TestCommandBase64, '--exit-code', $TestExit)
    & $pythonPath @createArgs
    $createExit = $LASTEXITCODE
    if ($createExit -ne 0) { exit $createExit }
    $validateArgs = @($BundleTool, 'validate', '--run-dir', $RunDir)
    & $pythonPath @validateArgs
    $validateExit = $LASTEXITCODE
    if ($validateExit -ne 0) { exit $validateExit }
    if ($TestExit -ne 0) { exit $TestExit }
    exit 0
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if ($CompilerPreference -ne 'Msvc' -and $null -ne $gccPath) {
    New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
    $build = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-g2-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $build | Out-Null
    $exe = Join-Path $build 'test_single_camera_runtime.exe'
    $rawLog = Join-Path $RunDir 'raw.log'
    $compileArgs = @('-std=c11', '-Wall', '-Wextra', '-Werror', "-I$include", $test) + $sources + @('-o', $exe)
    & $gccPath @compileArgs
    $compileExit = $LASTEXITCODE
    if ($compileExit -ne 0) {
        Write-Host "HOST_RUNTIME_BLOCKED_COMPILER: gcc failed with $compileExit"
        exit $compileExit
    }
    $testArgs = @('--raw-log', $rawLog)
    & $exe @testArgs
    $testExit = $LASTEXITCODE
    $compilerVersion = (& $gccPath --version | Select-Object -First 1)
    $steps = @((New-ArgvStep -Exe $gccPath -Args $compileArgs), (New-ArgvStep -Exe $exe -Args $testArgs))
    $testCommandBase64 = Convert-StepsToBase64 -Steps $steps
    $bundleTool = Join-Path $repoRoot 'final_project\tools\board_observability\g2_run_bundle.py'
    Invoke-Bundle -BundleTool $bundleTool -RawLog $rawLog -CompilerVersion $compilerVersion -TestCommandBase64 $testCommandBase64 -TestExit $testExit
}
if ($CompilerPreference -eq 'Gcc' -and $null -eq $gccPath) {
    throw 'HOST_RUNTIME_BLOCKED_COMPILER: GCC requested but gcc.exe is unavailable'
}
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: neither gcc nor vswhere.exe is available' }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: VS2022 C++ tools not found' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: vcvarsall.bat not found' }

$envCommand = 'call "' + $vcvars + '" x64 >nul && set'
$envArgs = @('/d', '/s', '/c', $envCommand)
$envOutput = & $env:ComSpec @envArgs
if ($LASTEXITCODE -ne 0) { throw "HOST_RUNTIME_BLOCKED_COMPILER: vcvarsall failed with $LASTEXITCODE" }

$vcvarsEnvironment = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([System.StringComparer]::OrdinalIgnoreCase)
$vcvarsPathCandidates = New-Object 'System.Collections.Generic.List[string]'
foreach ($line in $envOutput) {
    if ($line -match '^([^=]+)=(.*)$') {
        $name = $Matches[1]
        $value = $Matches[2]
        if ($name -ieq 'PATH') {
            $vcvarsPathCandidates.Add($value)
        }
        if (-not $vcvarsEnvironment.ContainsKey($name)) {
            $vcvarsEnvironment.Add($name, $value)
        }
    }
}

if ($vcvarsPathCandidates.Count -gt 0) {
    $selectedPath = $vcvarsPathCandidates[0]
    foreach ($pathCandidate in $vcvarsPathCandidates) {
        foreach ($pathEntry in ($pathCandidate -split ';')) {
            if (-not [string]::IsNullOrWhiteSpace($pathEntry)) {
                $clCandidate = Join-Path $pathEntry 'cl.exe'
                if (Test-Path -LiteralPath $clCandidate -PathType Leaf) {
                    $selectedPath = $pathCandidate
                    break
                }
            }
        }
        if ($selectedPath -eq $pathCandidate -and ($pathCandidate -split ';' | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_) -and
            (Test-Path -LiteralPath (Join-Path $_ 'cl.exe') -PathType Leaf)
        })) {
            break
        }
    }
    $vcvarsEnvironment['Path'] = $selectedPath
}

foreach ($entry in $vcvarsEnvironment.GetEnumerator()) {
    $environmentName = if ($entry.Key -ieq 'PATH') { 'Path' } else { $entry.Key }
    [Environment]::SetEnvironmentVariable($environmentName, $entry.Value, 'Process')
}
$clCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($null -eq $clCommand) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: cl.exe not found after vcvarsall import' }
$clPath = $clCommand.Source

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
$build = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-g2-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $build | Out-Null
$exe = Join-Path $build 'test_single_camera_runtime.exe'
$rawLog = Join-Path $RunDir 'raw.log'
$compileArgs = @('/nologo', '/utf-8', '/std:c11', '/W4', '/WX', "/I$include", "/Fe$exe", $test) + $sources
Push-Location $build
try {
    & $clPath @compileArgs
    $compileExit = $LASTEXITCODE
}
finally {
    Pop-Location
}
if ($compileExit -ne 0) {
    Write-Host "HOST_RUNTIME_BLOCKED_COMPILER: cl.exe failed with $compileExit"
    exit $compileExit
}
$testArgs = @('--raw-log', $rawLog)
& $exe @testArgs
$testExit = $LASTEXITCODE
$savedErrorActionPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    $compilerVersion = (& $clPath 2>&1 | ForEach-Object { [string]$_ } | Select-Object -First 1)
}
finally {
    $ErrorActionPreference = $savedErrorActionPreference
}
$steps = @((New-ArgvStep -Exe $clPath -Args $compileArgs), (New-ArgvStep -Exe $exe -Args $testArgs))
$testCommandBase64 = Convert-StepsToBase64 -Steps $steps
$bundleTool = Join-Path $repoRoot 'final_project\tools\board_observability\g2_run_bundle.py'
Invoke-Bundle -BundleTool $bundleTool -RawLog $rawLog -CompilerVersion $compilerVersion -TestCommandBase64 $testCommandBase64 -TestExit $testExit
