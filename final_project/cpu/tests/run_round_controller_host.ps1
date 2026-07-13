param(
    [string]$VcVars64 = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $VcVars64)) {
    throw "MSVC environment script not found: $VcVars64"
}

$testDir = $PSScriptRoot
$cpuDir = Split-Path -Parent $testDir
$includeDir = Join-Path $cpuDir "app\include"
$controllerSource = Join-Path $cpuDir "app\src\round_controller.c"
$testSource = Join-Path $testDir "test_round_controller.c"
$buildDir = Join-Path $testDir "build\round_controller_host"
$exePath = Join-Path $buildDir "test_round_controller.exe"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$compile = 'call "{0}" >nul && cl /nologo /std:c11 /utf-8 /W4 /WX ' +
           '/DAPB_VISION_BASE_PLACEHOLDER=0x40000000u /I"{1}" ' +
           '"{2}" "{3}" /Fe:"{4}"'
$compile = $compile -f $VcVars64, $includeDir, $testSource, $controllerSource, $exePath

Push-Location $buildDir
try {
    & cmd.exe /d /c $compile
    if ($LASTEXITCODE -ne 0) {
        throw "round_controller host compile failed with exit code $LASTEXITCODE"
    }

    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "round_controller host tests failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}
