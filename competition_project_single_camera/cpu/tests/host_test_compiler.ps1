Set-StrictMode -Version Latest

function Invoke-StrictCHostTest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$IncludeDir,
        [Parameter(Mandatory = $true)]
        [string]$TestFile,
        [Parameter(Mandatory = $true)]
        [string[]]$Sources,
        [string]$AdditionalIncludeDir
    )

    $script:HostTestExitCode = 1
    $build = Join-Path ([System.IO.Path]::GetTempPath()) ('cicc-' + $Name + '-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $build | Out-Null
    $exe = Join-Path $build ($Name + '.exe')
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue

    if ($null -ne $gcc) {
        $includeArgs = @("-I$IncludeDir")
        if ($AdditionalIncludeDir) { $includeArgs += "-I$AdditionalIncludeDir" }
        & $gcc.Source -std=c11 -Wall -Wextra -Werror @includeArgs $TestFile @Sources -o $exe
        if ($LASTEXITCODE -ne 0) {
            $script:HostTestExitCode = $LASTEXITCODE
            return
        }
    }
    else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) {
            throw 'HOST_TEST_BLOCKED_COMPILER: neither gcc nor vswhere.exe is available'
        }
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $vs) { throw 'HOST_TEST_BLOCKED_COMPILER: VS2022 C++ tools not found' }
        $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
        if (-not (Test-Path -LiteralPath $vcvars)) { throw 'HOST_TEST_BLOCKED_COMPILER: vcvarsall.bat not found' }
        $compileArgs = @('/nologo', '/utf-8', '/std:c11', '/W4', '/WX', "/I$IncludeDir")
        if ($AdditionalIncludeDir) { $compileArgs += "/I$AdditionalIncludeDir" }
        $compileArgs += @("/Fe$exe", $TestFile) + $Sources
        $compileLine = 'pushd "' + $build + '" && call "' + $vcvars + '" x64 >nul && cl.exe ' +
            (($compileArgs | ForEach-Object { '"' + $_ + '"' }) -join ' ') + ' && popd'
        cmd.exe /d /s /c $compileLine
        if ($LASTEXITCODE -ne 0) {
            $script:HostTestExitCode = $LASTEXITCODE
            return
        }
    }

    & $exe
    $script:HostTestExitCode = $LASTEXITCODE
}
