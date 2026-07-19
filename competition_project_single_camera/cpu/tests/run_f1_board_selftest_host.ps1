$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$include = Join-Path $root 'include'
$src = Join-Path $root 'src'
$test = Join-Path $PSScriptRoot 'f1_board_selftest\test_f1_board_selftest.c'
. (Join-Path $PSScriptRoot 'host_test_compiler.ps1')
Invoke-StrictCHostTest -Name 'f1-board-selftest' -IncludeDir $include -TestFile $test -Sources @(
    (Join-Path $src 'f1_board_selftest.c'),
    (Join-Path $src 'single_camera_classifier.c'),
    (Join-Path $src 'single_camera_feature_adapter.c'),
    (Join-Path $src 'single_camera_f1.c'),
    (Join-Path $src 'single_camera_runtime.c')
) -AdditionalIncludeDir $src
exit $script:HostTestExitCode
