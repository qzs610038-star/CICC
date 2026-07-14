Param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw "Missing MSVC environment: $vcvars" }

$include = Join-Path $root 'final_project\cpu\app\include'
$params = Join-Path $root 'final_project\cpu\params'
$src = Join-Path $root 'final_project\cpu\app\src'
$test = Join-Path $root 'final_project\cpu\tests\test_arm_runtime.c'
$build = Join-Path $env:TEMP ('codex-arm-runtime-host-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $build | Out-Null

function Invoke-Profile([string]$Backend, [string[]]$Sources) {
    $exe = Join-Path $build ($Backend + '.exe')
    $objects = Join-Path $build ($Backend + '_objs\\')
    New-Item -ItemType Directory -Force -Path $objects | Out-Null
    $sourceText = ($Sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $compile = 'call "{0}" >nul && cl /nologo /std:c11 /utf-8 /W4 /WX /Fo"{1}" /DAPB_VISION_BASE_PLACEHOLDER=0xF0000000u /DAPP_PROFILE=ARM_PROFILE_ARM_BRINGUP /DARM_BACKEND=ARM_BACKEND_{2} /I"{3}" /I"{4}" {5} /Fe:"{6}"' -f $vcvars, $objects, $Backend.ToUpperInvariant(), $include, $params, $sourceText, $exe
    & cmd.exe /d /c $compile
    if ($LASTEXITCODE -ne 0) { throw "$Backend Host compile failed." }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$Backend Host assertions failed." }
    Write-Output "HOST PASS backend=$Backend"
}

try {
    Invoke-Profile 'disabled' @($test, (Join-Path $src 'arm_runtime.c'), (Join-Path $src 'round_controller.c'))
    Invoke-Profile 'simulated' @($test, (Join-Path $src 'arm_runtime.c'),
        (Join-Path $src 'arm_sim_transport.c'), (Join-Path $src 'arm_controller.c'),
        (Join-Path $src 'round_controller.c'), (Join-Path $params 'arm_positions.c'))
} finally {
    Start-Sleep -Milliseconds 200
    Remove-Item -LiteralPath $build -Recurse -Force -ErrorAction SilentlyContinue
}
