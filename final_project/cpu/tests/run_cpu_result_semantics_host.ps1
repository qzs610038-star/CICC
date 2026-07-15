param(
    [string]$VcVars64 = "",
    [ValidateSet('auto', 'msvc', 'gcc')][string]$Compiler = 'auto'
)

$ErrorActionPreference = "Stop"

$testDir = $PSScriptRoot
$cpuDir = Split-Path -Parent $testDir
$repoRoot = Split-Path -Parent (Split-Path -Parent $cpuDir)
$includeDir = Join-Path $cpuDir "app\include"
$semanticsSource = Join-Path $cpuDir "app\src\cpu_result_semantics.c"
$adaptersSource = Join-Path $cpuDir "app\src\cpu_result_semantics_adapters.c"
$controllerSource = Join-Path $cpuDir "app\src\round_controller.c"
$testSource = Join-Path $testDir "test_cpu_result_semantics.c"
$pureTestSource = Join-Path $testDir "test_cpu_semantics_pure.c"
$buildDir = Join-Path $testDir "build\cpu_result_semantics_host"
$exePath = Join-Path $buildDir "test_cpu_result_semantics.exe"

function Resolve-VcVars64([string]$RequestedPath) {
    if ($RequestedPath) {
        if (-not (Test-Path -LiteralPath $RequestedPath)) {
            throw "Explicit vcvars64 path does not exist: $RequestedPath"
        }
        return (Resolve-Path -LiteralPath $RequestedPath).Path
    }
    if ($env:VCVARS64_PATH -and (Test-Path -LiteralPath $env:VCVARS64_PATH)) {
        return (Resolve-Path -LiteralPath $env:VCVARS64_PATH).Path
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $install = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        if ($LASTEXITCODE -eq 0 -and $install) {
            $candidate = Join-Path ($install | Select-Object -First 1) `
                'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }
    return $null
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# Primary: MSVC /W4 /WX (repo convention). Fallback: repo mingw64 gcc with
# -Wall -Wextra -Werror when MSVC is not installed on this machine.
$mingwGcc = Join-Path $repoRoot "tools\mingw64\bin\gcc.exe"
$resolvedVcVars64 = if ($Compiler -ne 'gcc') { Resolve-VcVars64 $VcVars64 } else { $null }
$useMsvc = ($Compiler -eq 'msvc') -or ($Compiler -eq 'auto' -and $resolvedVcVars64)
$useGcc = ($Compiler -eq 'gcc') -or ($Compiler -eq 'auto' -and -not $resolvedVcVars64)
if ($Compiler -eq 'msvc' -and -not $resolvedVcVars64) {
    throw 'MSVC requested but vcvars64.bat could not be resolved.'
}
if ($Compiler -eq 'gcc' -and -not (Test-Path -LiteralPath $mingwGcc)) {
    throw "GCC requested but repo compiler is missing: $mingwGcc"
}

Push-Location $buildDir
try {
    if ($useMsvc) {
        # Step 1 (proof): the pure semantic header/source must compile independently
        # with NO APB placeholder define. If it ever transitively pulled in board_io.h,
        # the APB base guard #error / placeholder #warning would break this strict build.
        Write-Output "[pure] MSVC compile-only pure semantic TUs (/W4 /WX, no APB placeholder define)"
        $pure = 'call "{0}" >nul && cl /nologo /std:c11 /utf-8 /W4 /WX /c /I"{1}" "{2}" "{3}"'
        $pure = $pure -f $resolvedVcVars64, $includeDir, $pureTestSource, $semanticsSource
        & cmd.exe /d /c $pure
        if ($LASTEXITCODE -ne 0) {
            throw "pure semantic header failed independent compile (MSVC), exit $LASTEXITCODE"
        }

        # Step 2: full adapters build (adapters TU legitimately pulls board_io.h placeholder)
        Write-Output "[compiler] MSVC cl.exe (/W4 /WX)"
        $compile = 'call "{0}" >nul && cl /nologo /std:c11 /utf-8 /W4 /WX ' +
                   '/DAPB_VISION_BASE_PLACEHOLDER=0x40000000u /I"{1}" ' +
                   '"{2}" "{3}" "{4}" "{5}" /Fe:"{6}"'
        $compile = $compile -f $resolvedVcVars64, $includeDir, $testSource,
            $semanticsSource, $adaptersSource, $controllerSource, $exePath
        & cmd.exe /d /c $compile
    }
    elseif ($useGcc -and (Test-Path -LiteralPath $mingwGcc)) {
        # Step 1 (proof): strict compile-only of the pure semantic TUs WITHOUT
        # -Wno-error=cpp and WITHOUT the APB placeholder define. Success proves the
        # pure header is independent of board_io.h / APB and free of the placeholder
        # #warning. (The full adapters build below still needs both flags.)
        Write-Output "[pure] repo mingw64 gcc compile-only pure semantic TUs (-Wall -Wextra -Wshadow -Werror, no cpp downgrade, no APB placeholder)"
        & $mingwGcc -std=c11 -Wall -Wextra -Wshadow -Werror -c "-I$includeDir" `
            $pureTestSource -o (Join-Path $buildDir "pure_test.o")
        if ($LASTEXITCODE -ne 0) {
            throw "pure header test TU failed independent compile (gcc), exit $LASTEXITCODE"
        }
        & $mingwGcc -std=c11 -Wall -Wextra -Wshadow -Werror -c "-I$includeDir" `
            $semanticsSource -o (Join-Path $buildDir "pure_semantics.o")
        if ($LASTEXITCODE -ne 0) {
            throw "pure semantics source failed independent compile (gcc), exit $LASTEXITCODE"
        }

        # Step 2: full build. -Wno-error=cpp only downgrades the single documented
        # board_io.h soc.h placeholder #warning (reached via the adapters TU);
        # every other warning stays fatal.
        Write-Output "[compiler] repo mingw64 gcc (-Wall -Wextra -Wshadow -Werror -Wno-error=cpp)"
        & $mingwGcc -std=c11 -Wall -Wextra -Wshadow -Werror -Wno-error=cpp `
            "-DAPB_VISION_BASE_PLACEHOLDER=0x40000000u" `
            "-I$includeDir" $testSource $semanticsSource $adaptersSource `
            $controllerSource -o $exePath
    }
    else {
        throw "No C compiler found: neither MSVC vcvars64 nor tools\mingw64 gcc."
    }
    if ($LASTEXITCODE -ne 0) {
        throw "cpu_result_semantics host compile failed with exit code $LASTEXITCODE"
    }

    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "cpu_result_semantics host tests failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}
