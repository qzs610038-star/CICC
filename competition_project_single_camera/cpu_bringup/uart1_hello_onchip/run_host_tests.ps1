$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$test = Join-Path $root 'tests\test_p0a_uart_timeout.c'
$source = Join-Path $root 'src\p0a_diag.c'
$include = Join-Path $root 'src'
$cpuRoot = Split-Path -Parent (Split-Path -Parent $root)
. (Join-Path $cpuRoot 'cpu\tests\host_test_compiler.ps1')
Invoke-StrictCHostTest -Name 'p0a_uart_timeout' -IncludeDir $include -TestFile $test -Sources @($source)
exit $HostTestExitCode
