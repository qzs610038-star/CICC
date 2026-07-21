param()

$ErrorActionPreference = 'Stop'
$cpuRoot = Split-Path -Parent $PSScriptRoot
$include = Join-Path $cpuRoot 'include'
$classifier = Join-Path $cpuRoot 'src\single_camera_classifier.c'
$f1 = Join-Path $cpuRoot 'src\single_camera_f1.c'
$test = Join-Path $PSScriptRoot 'test_single_camera_classifier.c'
. (Join-Path $PSScriptRoot 'host_test_compiler.ps1')
Invoke-StrictCHostTest -Name 'test_single_camera_classifier' -IncludeDir $include -TestFile $test -Sources @($classifier, $f1)
exit $script:HostTestExitCode
