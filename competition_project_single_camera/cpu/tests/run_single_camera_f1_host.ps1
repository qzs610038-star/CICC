param()

$ErrorActionPreference = 'Stop'
$cpuRoot = Split-Path -Parent $PSScriptRoot
$include = Join-Path $cpuRoot 'include'
$source = Join-Path $cpuRoot 'src\single_camera_f1.c'
$test = Join-Path $PSScriptRoot 'test_single_camera_f1.c'
. (Join-Path $PSScriptRoot 'host_test_compiler.ps1')
Invoke-StrictCHostTest -Name 'test_single_camera_f1' -IncludeDir $include -TestFile $test -Sources @($source)
exit $script:HostTestExitCode
