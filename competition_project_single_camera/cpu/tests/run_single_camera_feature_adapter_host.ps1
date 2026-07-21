param()

$ErrorActionPreference = 'Stop'
$cpuRoot = Split-Path -Parent $PSScriptRoot
$include = Join-Path $cpuRoot 'include'
$adapter = Join-Path $cpuRoot 'src\single_camera_feature_adapter.c'
$classifier = Join-Path $cpuRoot 'src\single_camera_classifier.c'
$f1 = Join-Path $cpuRoot 'src\single_camera_f1.c'
$test = Join-Path $PSScriptRoot 'test_single_camera_feature_adapter.c'

. (Join-Path $PSScriptRoot 'host_test_compiler.ps1')
Invoke-StrictCHostTest -Name 'single_camera_feature_adapter' -IncludeDir $include -TestFile $test -Sources @($adapter, $classifier, $f1)
exit $script:HostTestExitCode
