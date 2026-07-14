param(
    [string]$VcVars64 = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"

$testDir = $PSScriptRoot
$cpuDir = Split-Path -Parent $testDir
$repoRoot = Split-Path -Parent (Split-Path -Parent $cpuDir)
$includeDir = Join-Path $cpuDir "app\include"
$srcDir = Join-Path $cpuDir "app\src"
$testSource = Join-Path $testDir "test_main_loop_arm_disabled.c"
# 与 main.c 链接同一份 adapter 实现（P1-1 关键：测试真实覆盖 main.c 适配代码）
$adapterSource = Join-Path $srcDir "main_loop_adapter.c"
$semanticsSource = Join-Path $srcDir "cpu_result_semantics.c"
$adaptersSource = Join-Path $srcDir "cpu_result_semantics_adapters.c"
$controllerSource = Join-Path $srcDir "round_controller.c"
$matcherSource = Join-Path $srcDir "task_matcher.c"
$buildDir = Join-Path $testDir "build\main_loop_arm_disabled_host"
$exePath = Join-Path $buildDir "test_main_loop_arm_disabled.exe"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$mingwGcc = Join-Path $repoRoot "tools\mingw64\bin\gcc.exe"

Push-Location $buildDir
try {
    if (Test-Path -LiteralPath $VcVars64) {
        Write-Output "[compiler] MSVC cl.exe (/W4 /WX)"
        $compile = 'call "{0}" >nul && cl /nologo /std:c11 /utf-8 /W4 /WX ' +
                   '/DAPB_VISION_BASE_PLACEHOLDER=0x40000000u /I"{1}" ' +
                   '"{2}" "{3}" "{4}" "{5}" "{6}" "{7}" /Fe:"{8}"'
        $compile = $compile -f $VcVars64, $includeDir, $testSource,
            $adapterSource, $semanticsSource, $adaptersSource,
            $controllerSource, $matcherSource, $exePath
        & cmd.exe /d /c $compile
    }
    elseif (Test-Path -LiteralPath $mingwGcc) {
        Write-Output "[compiler] repo mingw64 gcc (-Wall -Wextra -Werror -Wno-error=cpp)"
        Push-Location $repoRoot
        $relInc = "final_project\cpu\app\include"
        $relSrc = "final_project\cpu\app\src"
        $relTest = "final_project\cpu\tests"
        & $mingwGcc -std=c11 -Wall -Wextra -Werror -Wno-error=cpp "-DAPB_VISION_BASE_PLACEHOLDER=0x40000000u" "-I$relInc" "$relTest\test_main_loop_arm_disabled.c" "$relSrc\main_loop_adapter.c" "$relSrc\cpu_result_semantics.c" "$relSrc\cpu_result_semantics_adapters.c" "$relSrc\round_controller.c" "$relSrc\task_matcher.c" -o $exePath
        Pop-Location
    }
    else {
        throw "No C compiler found: neither MSVC vcvars64 nor tools\mingw64 gcc."
    }
    if ($LASTEXITCODE -ne 0) {
        throw "main_loop_arm_disabled host compile failed with exit code $LASTEXITCODE"
    }

    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "main_loop_arm_disabled host tests failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}
