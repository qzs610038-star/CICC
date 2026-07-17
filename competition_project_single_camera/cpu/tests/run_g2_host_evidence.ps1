param(
    [Parameter(Mandatory = $true)]
    [string]$RunDir
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
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: vswhere.exe not found' }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: VS2022 C++ tools not found' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw 'HOST_RUNTIME_BLOCKED_COMPILER: vcvarsall.bat not found' }

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
$build = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-g2-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $build | Out-Null
$exe = Join-Path $build 'test_single_camera_runtime.exe'
$rawLog = Join-Path $RunDir 'raw.log'
$compileArgs = @('/nologo', '/std:c11', '/W4', '/WX', "/I$include", "/Fe$exe", $test) + $sources
$compileLine = 'pushd "' + $build + '" && call "' + $vcvars + '" x64 >nul && cl.exe ' + (($compileArgs | ForEach-Object { '"' + $_ + '"' }) -join ' ') + ' && popd'
cmd.exe /d /s /c $compileLine
if ($LASTEXITCODE -ne 0) { throw "HOST_RUNTIME_BLOCKED_COMPILER: cl.exe failed with $LASTEXITCODE" }
& $exe --raw-log $rawLog
$testExit = $LASTEXITCODE
$compilerVersion = (cmd.exe /d /s /c ('call "' + $vcvars + '" x64 >nul && cl.exe 2>&1') | Select-Object -First 1)
$bundleTool = Join-Path $repoRoot 'final_project\tools\board_observability\g2_run_bundle.py'
$testCommand = $compileLine + ' && ' + $exe + ' --raw-log ' + $rawLog
$testCommandBase64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($testCommand))
python $bundleTool create --run-dir $RunDir --raw-log $rawLog --repo-root $repoRoot --compiler $compilerVersion --test-command-base64 $testCommandBase64 --exit-code $testExit
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
python $bundleTool validate --run-dir $RunDir
exit $LASTEXITCODE
