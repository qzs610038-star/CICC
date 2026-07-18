param(
    [Parameter(Mandatory = $true)]
    [string]$RunDir,
    [string]$PythonExe
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
$fixedGcc = Join-Path $repoRoot 'tools\mingw64\bin\gcc.exe'
$gccPath = $null
if (Test-Path -LiteralPath $fixedGcc -PathType Leaf) {
    $gccPath = $fixedGcc
} else {
    $gccCommand = Get-Command gcc -ErrorAction SilentlyContinue
    if ($null -ne $gccCommand) {
        $gccPath = $gccCommand.Source
    }
}
if ($null -eq $gccPath) {
    $workspaceRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $repoRoot))
    $linkedWorktreeGcc = Join-Path $workspaceRoot 'tools\mingw64\bin\gcc.exe'
    if (Test-Path -LiteralPath $linkedWorktreeGcc -PathType Leaf) {
        $gccPath = $linkedWorktreeGcc
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
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if ($null -ne $gccPath) {
    New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
    $build = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-g2-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $build | Out-Null
    $exe = Join-Path $build 'test_single_camera_runtime.exe'
    $rawLog = Join-Path $RunDir 'raw.log'
    $compileArgs = @('-std=c11', '-Wall', '-Wextra', '-Werror', "-I$include", $test) + $sources + @('-o', $exe)
    & $gccPath @compileArgs
    if ($LASTEXITCODE -ne 0) { throw "HOST_RUNTIME_BLOCKED_COMPILER: gcc failed with $LASTEXITCODE" }
    & $exe --raw-log $rawLog
    $testExit = $LASTEXITCODE
    $compilerVersion = (& $gccPath --version | Select-Object -First 1)
    $compileLine = $gccPath + ' ' + (($compileArgs | ForEach-Object { '"' + $_ + '"' }) -join ' ')
    $bundleTool = Join-Path $repoRoot 'final_project\tools\board_observability\g2_run_bundle.py'
    $testCommand = $compileLine + ' && ' + $exe + ' --raw-log ' + $rawLog
    $testCommandBase64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($testCommand))
    $createArgs = @($bundleTool, 'create', '--run-dir', $RunDir, '--raw-log', $rawLog, '--repo-root', $repoRoot, '--compiler', $compilerVersion, '--test-command-base64', $testCommandBase64, '--exit-code', $testExit)
    & $pythonPath @createArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $validateArgs = @($bundleTool, 'validate', '--run-dir', $RunDir)
    & $pythonPath @validateArgs
    $validateExit = $LASTEXITCODE
    if ($validateExit -ne 0) { exit $validateExit }
    if ($testExit -ne 0) { exit $testExit }
    exit 0
}
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: neither gcc nor vswhere.exe is available' }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: VS2022 C++ tools not found' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: vcvarsall.bat not found' }

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
$build = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-g2-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $build | Out-Null
$exe = Join-Path $build 'test_single_camera_runtime.exe'
$rawLog = Join-Path $RunDir 'raw.log'
$compileArgs = @('/nologo', '/utf-8', '/std:c11', '/W4', '/WX', "/I$include", "/Fe$exe", $test) + $sources
$compileLine = 'pushd "' + $build + '" && call "' + $vcvars + '" x64 >nul && cl.exe ' + (($compileArgs | ForEach-Object { '"' + $_ + '"' }) -join ' ') + ' && popd'
cmd.exe /d /s /c $compileLine
if ($LASTEXITCODE -ne 0) { throw "HOST_RUNTIME_BLOCKED_COMPILER: cl.exe failed with $LASTEXITCODE" }
& $exe --raw-log $rawLog
$testExit = $LASTEXITCODE
$compilerVersion = (cmd.exe /d /s /c ('call "' + $vcvars + '" x64 >nul && cl.exe 2>&1') | Select-Object -First 1)
$bundleTool = Join-Path $repoRoot 'final_project\tools\board_observability\g2_run_bundle.py'
$testCommand = $compileLine + ' && ' + $exe + ' --raw-log ' + $rawLog
$testCommandBase64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($testCommand))
$createArgs = @($bundleTool, 'create', '--run-dir', $RunDir, '--raw-log', $rawLog, '--repo-root', $repoRoot, '--compiler', $compilerVersion, '--test-command-base64', $testCommandBase64, '--exit-code', $testExit)
& $pythonPath @createArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$validateArgs = @($bundleTool, 'validate', '--run-dir', $RunDir)
& $pythonPath @validateArgs
$validateExit = $LASTEXITCODE
if ($validateExit -ne 0) { exit $validateExit }
if ($testExit -ne 0) { exit $testExit }
exit 0
