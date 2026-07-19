$ErrorActionPreference = 'Stop'
$cpuRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'host_test_compiler.ps1')
Invoke-StrictCHostTest -Name 'p1_host_model' `
    -IncludeDir (Join-Path $cpuRoot 'src') `
    -TestFile (Join-Path $PSScriptRoot 'test_p1_host_model.c') `
    -Sources @((Join-Path $cpuRoot 'src\p1_host_model.c'))
exit $HostTestExitCode
