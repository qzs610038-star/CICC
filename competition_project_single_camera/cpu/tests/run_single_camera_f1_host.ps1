param()

$ErrorActionPreference = 'Stop'
$cpuRoot = Split-Path -Parent $PSScriptRoot
$include = Join-Path $cpuRoot 'include'
$source = Join-Path $cpuRoot 'src\single_camera_f1.c'
$test = Join-Path $PSScriptRoot 'test_single_camera_f1.c'
$build = Join-Path $PSScriptRoot 'build'
$exe = Join-Path $build 'test_single_camera_f1.exe'

$gcc = (Get-Command gcc -ErrorAction Stop).Source
New-Item -ItemType Directory -Force -Path $build | Out-Null
& $gcc -std=c11 -Wall -Wextra -Werror "-I$include" $test $source -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
