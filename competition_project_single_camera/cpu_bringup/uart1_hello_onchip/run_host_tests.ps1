param([string]$WitnessPath)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$test = Join-Path $root 'tests\test_p0a_uart_timeout.c'
$source = Join-Path $root 'src\p0a_diag.c'
$include = Join-Path $root 'src'
$hostBuildId = '0xA17E5701u'
$cpuRoot = Split-Path -Parent (Split-Path -Parent $root)
. (Join-Path $cpuRoot 'cpu\tests\host_test_compiler.ps1')
# The shared helper has no Define parameter, so compile this test directly with
# the same strict policy and an explicit non-production Host-only build ID.
$buildRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-p0a-host-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
$hostExe = Join-Path $buildRoot 'p0a_uart_timeout.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'P0A_HOST_BLOCKED_COMPILER' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$compile = 'pushd "' + $buildRoot + '" && call "' + $vcvars + '" x64 >nul && cl.exe /nologo /utf-8 /std:c11 /W4 /WX /DP0A_BUILD_ID=' +
    $hostBuildId + ' /I"' + $include + '" /Fe"' + $hostExe + '" "' + $test + '" "' + $source + '" && popd'
cmd.exe /d /s /c $compile
$HostTestExitCode = $LASTEXITCODE
if ($HostTestExitCode -eq 0) { & $hostExe; $HostTestExitCode = $LASTEXITCODE }
if ($HostTestExitCode -eq 0 -and -not [string]::IsNullOrWhiteSpace($WitnessPath)) {
    $build = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-p0a-json-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $build | Out-Null
    $exe = Join-Path $build 'p0a_uart_timeout.exe'
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    if ($null -ne $gcc) {
        & $gcc.Source -std=c11 -Wall -Wextra -Werror "-DP0A_BUILD_ID=$hostBuildId" "-I$include" $test $source -o $exe
    } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
        cmd.exe /d /s /c ('pushd "' + $build + '" && call "' + $vcvars + '" x64 >nul && cl.exe /nologo /utf-8 /std:c11 /W4 /WX /DP0A_BUILD_ID=' + $hostBuildId + ' /I"' + $include + '" /Fe"' + $exe + '" "' + $test + '" "' + $source + '" && popd')
    }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $exe --json $WitnessPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit $HostTestExitCode
