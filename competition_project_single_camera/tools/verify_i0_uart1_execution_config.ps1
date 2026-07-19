[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$RunDir
)

$ErrorActionPreference = 'Stop'
$BaseSha = '2d713b80a41185e472837abaec3a10c01383c70f'
$QzsAuthorizationSha = 'a222ea64653a2232945342faacfb53a06ce50e42'
$WscContractSha = '48548f47dfa5964b13aed7edf3b3e9da6f6583a2'
$projectRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent $projectRoot
$manifestPath = Join-Path $PSScriptRoot 'i0_uart1_execution_manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$runnerPath = Join-Path $PSScriptRoot 'run_i0_uart1_execution_chain.ps1'
$cfgPath = Join-Path $PSScriptRoot 'i0_uart1_cleanlf_user2.cfg'
$packetPath = Join-Path $projectRoot 'docs\review_packets\I0_UART1_CLEANLF_USER2_EXECUTION_CONFIG_REVIEW_20260719.md'
$operationCardPath = Join-Path $projectRoot 'docs\debug_sessions\I0_UART1_CLEANLF_USER2_OPERATION_CARD_20260719.md'

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing required file: $Path" }
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Assert-Contains([string]$Text, [string]$Value, [string]$Label) {
    if (-not $Text.Contains($Value)) { throw "Missing ${Label}: $Value" }
}

function Get-EolProfile([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $crlf = 0; $lf = 0; $bareCr = 0
    for ($index = 0; $index -lt $bytes.Length; $index++) {
        if ($bytes[$index] -eq 13) {
            if ($index + 1 -ge $bytes.Length -or $bytes[$index + 1] -ne 10) { $bareCr++ }
        } elseif ($bytes[$index] -eq 10) {
            if ($index -gt 0 -and $bytes[$index - 1] -eq 13) { $crlf++ } else { $lf++ }
        }
    }
    if ($bareCr -gt 0 -or ($crlf -gt 0 -and $lf -gt 0)) { return 'mixed_or_invalid' }
    if ($crlf -gt 0) { return 'crlf' }
    if ($lf -gt 0) { return 'lf' }
    'none'
}

if ($manifest.batch -ne 'I0_UART1_20260719_CLEAN_LF_FINAL') { throw 'Batch mismatch.' }
if ($manifest.base_sha -ne $BaseSha -or $manifest.qzs_authorization_sha -ne $QzsAuthorizationSha -or $manifest.wsc_contract_sha -ne $WscContractSha) {
    throw 'Manifest provenance SHA mismatch.'
}
if ($manifest.PSObject.Properties.Name -contains 'packet_sha256') { throw 'Packet-to-manifest circular hash dependency is prohibited.' }

$allowedPaths = @(
    'competition_project_single_camera/docs/debug_sessions/I0_UART1_CLEANLF_USER2_OPERATION_CARD_20260719.md',
    'competition_project_single_camera/docs/review_packets/I0_UART1_CLEANLF_USER2_EXECUTION_CONFIG_REVIEW_20260719.md',
    'competition_project_single_camera/tools/i0_uart1_cleanlf_user2.cfg',
    'competition_project_single_camera/tools/i0_uart1_execution_manifest.json',
    'competition_project_single_camera/tools/run_i0_uart1_execution_chain.ps1',
    'competition_project_single_camera/tools/verify_i0_uart1_execution_config.ps1'
)
$changed = @(
    @(& git -C $repoRoot diff --name-only $BaseSha --),
    @(& git -C $repoRoot ls-files --others --exclude-standard)
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_ -replace '\\', '/' } | Sort-Object -Unique
$scopeViolations = @($changed | Where-Object { $allowedPaths -cnotcontains $_ })
if ($scopeViolations.Count -gt 0) { throw "Temporary scope violation: $($scopeViolations -join ', ')" }
if ($changed -contains '.gitignore') { throw 'User .gitignore modification must not be included.' }

$cfg = Get-Content -LiteralPath $cfgPath -Raw
Assert-Contains $cfg 'riscv use_bscan_tunnel 6 1' 'literal BSCAN width/type flow'
Assert-Contains $cfg 'riscv set_bscan_tunnel_ir 0x09' 'literal USER2 outer IR flow'
if ($cfg -match '(?m)^riscv use_bscan_tunnel \$|(?m)^riscv set_bscan_tunnel_ir \$') { throw 'Variable-only OpenOCD flow is prohibited.' }

$runner = Get-Content -LiteralPath $runnerPath -Raw
foreach ($clause in @(
    "if (`$Scenario -eq 'all') { throw 'Live mode rejects Scenario=all before any external action.' }",
    "if (`$Scenario -ne 'run') { throw 'Live mode accepts Scenario=run only; fixture outcome labels are prohibited.' }",
    'TIMEOUT_HALT_UNCONFIRMED', 'RSP_FIXTURES=PASS', 'UART1_PNP_ALLOWLIST=PASS',
    'CAPTURE_READY_TIME=', 'RESUME_ONCE_TIME=', 'RESUME_COUNT=1', 'BYTE_COUNTS RX=',
    'RUN_ID=', 'PNP=', 'RETRY_COUNT=0', 'RAM_READ_COUNT=0', 'RAM_READ_COUNT=4',
    'OpenOCD RSP returned NACK; retry is prohibited.', 'J44/UART0 programmer identity is prohibited'
)) { Assert-Contains $runner $clause 'runner fail-closed clause' }
foreach ($flowClause in @(
    '$openOcdArguments = Get-OpenOcdArguments -FtdiPath $FtdiConfig -TargetPath $TargetConfig -RequireFiles',
    'Start-Process -FilePath $OpenOcdExe -ArgumentList $openOcdArguments'
)) { Assert-Contains $runner $flowClause 'actual OpenOCD process argument flow' }
if ($runner -match '(?i)prepare_m2|softtap|\bUSER1\b|\bUART0\b.*fallback|tap scan|cable scan|retry_count=[1-9]') { throw 'Dangerous or fallback route detected in runner.' }

New-Item -ItemType Directory -Path $RunDir -Force | Out-Null
$mockRunDir = Join-Path $RunDir 'runner-fixtures'
$mockOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runnerPath -Mode Mock -Scenario all -RunDir $mockRunDir 2>&1
if ($LASTEXITCODE -ne 0) { throw "Runner fixtures failed: $mockOutput" }
$mockText = $mockOutput -join "`n"
foreach ($marker in @(
    'RSP_FIXTURES=PASS', 'UART1_PNP_ALLOWLIST=PASS', 'OPENOCD_ARGUMENT_FLOW=PASS',
    'MOCK_SUCCESS=PASS RESULT=SUCCESS RAM_READ_COUNT=4 RESUME_COUNT=1',
    'MOCK_TIMEOUT=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 RESUME_COUNT=1',
    'MOCK_TRAP=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 RESUME_COUNT=1',
    'MOCK_WRONG_PC=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 RESUME_COUNT=1',
    'MOCK_WRONG_REASON=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 RESUME_COUNT=1',
    'MOCK_HALT_UNCONFIRMED=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 RESUME_COUNT=1',
    'HARDWARE_ACTIONS=NONE'
)) { Assert-Contains $mockText $marker 'runner fixture result' }

$savedErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$liveRejectOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runnerPath -Mode Live -Scenario all -RunDir (Join-Path $RunDir 'live-all-reject') 2>&1
$liveRejectExitCode = $LASTEXITCODE
$ErrorActionPreference = $savedErrorActionPreference
if ($liveRejectExitCode -eq 0 -or ($liveRejectOutput -join "`n") -notmatch 'Live mode rejects Scenario=all before any external action') {
    throw 'Live Scenario=all did not fail closed before external action.'
}

$fixtureDirs = @(Get-ChildItem -LiteralPath $mockRunDir -Directory)
if ($fixtureDirs.Count -ne 6) { throw "Expected six runner fixture directories, got $($fixtureDirs.Count)." }
foreach ($directory in $fixtureDirs) {
    $logPath = Join-Path $directory.FullName 'execution.log'
    $markerPath = Join-Path $directory.FullName 'RESUME_ONCE.marker'
    $log = Get-Content -LiteralPath $logPath -Raw
    $marker = Get-Content -LiteralPath $markerPath -Raw
    foreach ($required in @('RUN_ID=', 'BASE_SHA=', 'QZS_SHA=', 'WSC_SHA=', 'DESIGN_SHA=', 'PNP=', 'CAPTURE_READY_TIME=', 'RESUME_ONCE_TIME=', 'BYTE_COUNTS RX=', 'FINAL_STATE=', 'RESUME_COUNT=1')) {
        Assert-Contains $log $required 'persistent execution log field'
    }
    Assert-Contains $marker 'RESUME_COUNT=1' 'persistent resume marker count'
    $readyMatch = [regex]::Match($log, 'CAPTURE_READY_TIME=(?<value>[^\s]+)')
    $resumeMatch = [regex]::Match($log, 'RESUME_ONCE_TIME=(?<value>[^\s]+)')
    if (-not $readyMatch.Success -or -not $resumeMatch.Success) { throw 'Missing timestamp ordering evidence.' }
    if ([DateTimeOffset]::Parse($readyMatch.Groups['value'].Value) -ge [DateTimeOffset]::Parse($resumeMatch.Groups['value'].Value)) {
        throw 'CAPTURE_READY_TIME must be earlier than RESUME_ONCE_TIME.'
    }
}
$timeoutLog = Get-Content -LiteralPath (Join-Path (@($fixtureDirs | Where-Object { $_.Name -like 'mock-timeout-*' })[0].FullName) 'execution.log') -Raw
$unconfirmedLog = Get-Content -LiteralPath (Join-Path (@($fixtureDirs | Where-Object { $_.Name -like 'mock-halt_unconfirmed-*' })[0].FullName) 'execution.log') -Raw
Assert-Contains $timeoutLog 'WATCHDOG_TIMEOUT_MS=1000 ACTIVE_HALT_COUNT=1' 'timeout active halt fixture'
Assert-Contains $unconfirmedLog 'TIMEOUT_HALT_UNCONFIRMED RAM_READ_COUNT=0 RETRY_COUNT=0' 'unconfirmed halt fixture'

foreach ($entry in $manifest.files) {
    $path = Join-Path $repoRoot $entry.path
    if ((Get-Sha256 $path) -ne $entry.sha256) { throw "Manifest hash mismatch: $($entry.path)" }
}
$manifestPaths = @($manifest.files.path)
foreach ($path in $allowedPaths | Where-Object { $_ -notmatch 'i0_uart1_execution_manifest\.json$' }) {
    if ($manifestPaths -cnotcontains $path) { throw "Modified runtime file is not manifest-bound: $path" }
}

foreach ($path in @($runnerPath, $PSCommandPath)) {
    if ((Get-EolProfile $path) -ne 'crlf') { throw "PS1 EOL mismatch: $path" }
}
if ((Get-EolProfile $cfgPath) -ne 'lf') { throw 'CFG EOL mismatch.' }
foreach ($path in Get-ChildItem -LiteralPath $PSScriptRoot -File -Filter '*.gdb') {
    if ((Get-EolProfile $path.FullName) -ne 'lf') { throw "GDB EOL mismatch: $($path.FullName)" }
}

$packet = Get-Content -LiteralPath $packetPath -Raw
$operationCard = Get-Content -LiteralPath $operationCardPath -Raw
foreach ($text in @($packet, $operationCard)) {
    Assert-Contains $text $QzsAuthorizationSha 'qzs authorization SHA'
    Assert-Contains $text $WscContractSha 'WSC contract SHA'
    Assert-Contains $text 'HARDWARE_ACTIONS=NONE' 'hardware safety boundary'
    Assert-Contains $text 'USER2=NOT_VERIFIED' 'board USER2 boundary'
}

'RUNNER_MANIFEST_BOUND=PASS'
'OPENOCD_ARGUMENT_FLOW=PASS width=6 type=1 outer_ir=0x09 USER2=NOT_VERIFIED'
'LIVE_SCENARIO_ALL_REJECTED=PASS'
'RSP_FIXTURES=PASS'
'TIMEOUT_FAIL_CLOSED=PASS TIMEOUT_HALT_UNCONFIRMED=RAM_READ_COUNT_0'
'UART1_PNP_ALLOWLIST=PASS exact_vid_pid_serial_instance=true'
'RESUME_COUNT_MODEL=1'
'TEMP_SCOPE_DANGER_ROUTE=PASS'
'EOL_PROFILE=PASS ps1=CRLF gdb_cfg=LF'
'I0_UART1_EXECUTION_CONFIG_STATIC=PASS'
'HARDWARE_ACTIONS=NONE'
