Param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$include = Join-Path $root 'final_project\cpu\app\include'
$test = Join-Path $root 'final_project\cpu\tests\test_competition_rounds.c'
$tasks = Join-Path $root 'final_project\cpu\app\src\competition_tasks.c'
$rounds = Join-Path $root 'final_project\cpu\app\src\competition_round_transaction.c'
$build = Join-Path $root 'final_project\cpu\build\competition_rounds_host'
$exe = Join-Path $build 'test_competition_rounds.exe'

New-Item -ItemType Directory -Force $build | Out-Null
& gcc -std=c99 -Wall -Wextra -Werror -Wno-error=cpp -DAPB_VISION_BASE_PLACEHOLDER=0xF0000000u "-I$include" $test $tasks $rounds -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
