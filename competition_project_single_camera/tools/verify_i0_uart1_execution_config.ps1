[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$RunDir,
    [Parameter(Mandatory)]
    [string]$WscContractRoot
)

$ErrorActionPreference = 'Stop'
$BaseSha = '69a1030e1e72854a857fab147aa2c9cc8f0e6800'
$QzsAuthorizationSha = 'a222ea64653a2232945342faacfb53a06ce50e42'
$WscContractSha = '48548f47dfa5964b13aed7edf3b3e9da6f6583a2'
$projectRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent $projectRoot
$manifestPath = Join-Path $PSScriptRoot 'i0_uart1_execution_manifest.json'
$runnerPath = Join-Path $PSScriptRoot 'run_i0_uart1_execution_chain.ps1'
$cfgPath = Join-Path $PSScriptRoot 'i0_uart1_cleanlf_user2.cfg'
$helloSourcePath = Join-Path $projectRoot 'embedded_sw\uart1_hello_onchip\src\main.c'
$packetPath = Join-Path $projectRoot 'docs\review_packets\I0_UART1_CLEANLF_USER2_EXECUTION_CONFIG_REVIEW_20260719.md'
$operationCardPath = Join-Path $projectRoot 'docs\debug_sessions\I0_UART1_CLEANLF_USER2_OPERATION_CARD_20260719.md'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json

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

function Assert-Ordered([string]$Text, [string]$Earlier, [string]$Later, [string]$Label) {
    $earlierIndex = $Text.IndexOf($Earlier, [StringComparison]::Ordinal)
    $laterIndex = $Text.IndexOf($Later, [StringComparison]::Ordinal)
    if ($earlierIndex -lt 0 -or $laterIndex -lt 0 -or $earlierIndex -ge $laterIndex) { throw "Invalid order for ${Label}: '$Earlier' must precede '$Later'." }
}

if ($manifest.schema_version -ne 3 -or $manifest.batch -ne 'I0_UART1_20260719_CLEAN_LF_FINAL') { throw 'Manifest schema or batch mismatch.' }
if ($manifest.base_sha -cne $BaseSha -or $manifest.qzs_authorization_sha -cne $QzsAuthorizationSha -or $manifest.wsc_contract_sha -cne $WscContractSha) { throw 'Manifest provenance SHA mismatch.' }
if ($manifest.PSObject.Properties.Name -contains 'packet_sha256' -or $manifest.PSObject.Properties.Name -contains 'manifest_sha256') { throw 'Packet/manifest circular hash dependency is prohibited.' }

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
Assert-Contains $cfg 'set CPUTAPID 0x006A0EF3' 'TJ375N529 outer TAP identity'
if ($cfg -match '(?m)^set CPUTAPID 0x006A0A79\s*$') { throw 'Ti375 default TAP ID is prohibited for the TJ375N529 target cfg.' }
if ($cfg -match '(?m)^riscv use_bscan_tunnel \$|(?m)^riscv set_bscan_tunnel_ir \$') { throw 'Variable-only OpenOCD flow is prohibited.' }

$helloSource = Get-Content -LiteralPath $helloSourcePath -Raw
foreach ($line in @('I0 UART1 HELLO', 'UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100', 'Type characters to verify echo.')) { Assert-Contains $helloSource $line 'fixed Hello source line' }
Assert-Contains $helloSource 'uart1_puts("\r\nI0 UART1 HELLO\r\n");' 'fixed leading CRLF transcript'

$runner = Get-Content -LiteralPath $runnerPath -Raw
foreach ($clause in @(
    "if (`$Scenario -eq 'all') { throw 'Live mode rejects Scenario=all before any external action.' }",
    'ApprovalRecordPath', 'approved_commit', 'board_id', 'uart1_pnp_key', 'window_start_utc', 'window_end_utc',
    'FIRST_FAILURE_STOP_NO_RETRY_CTRL_C_TIMEOUT', 'TIMEOUT_HALT_UNCONFIRMED',
    'RSP_FIXTURES=PASS', 'UART1_PNP_ALLOWLIST=PASS', 'WSC_CONTRACT_CONSUMED=PASS',
    'HELLO_THREE_LINES_FIXTURE=PASS', 'UART_TX_ECHO_FIXTURE=PASS', 'UART_ECHO_MISMATCH_NEGATIVE=PASS',
    'WRONG_POST_LOAD_PC_NEGATIVE=PASS', 'BAD_ARTIFACT_HASH_NEGATIVE=PASS', 'BAD_APPROVAL_TUPLE_NEGATIVE=PASS',
    '"$Phase`_RESUME_ONCE.marker"', "Write-PhaseResumeMarker `$directory `$runId 'HELLO'", "Write-PhaseResumeMarker `$directory `$runId 'APB'",
    'HELLO_RESUME_COUNT=1', 'APB_RESUME_COUNT=1', 'RETRY_COUNT=0',
    '$Serial.Write([byte[]]@($TxByte), 0, 1)', 'Assert-EchoByte -Expected $TxByte -Actual $echoValue',
    'Wait-OpenOcdReady -Process $openOcdProcess', 'OpenOCD RSP qSupported negotiation failed; retry is prohibited.',
    'APB_PHASE_STARTED_AFTER_HELLO_PASS=true', '$wsc.HaltPc', '$wsc.TimeoutMs', '$wsc.Ram.GetEnumerator()',
    'Start-Process -FilePath $OpenOcdExe -ArgumentList $openOcdArguments'
)) { Assert-Contains $runner $clause 'runner fail-closed clause' }
if ($runner -match 'ApprovalToken|I0_EXECUTION_WINDOW_APPROVED|WriteLine\(|(?i)prepare_m2|softtap|\bUSER1\b|\bUART0\b.*fallback|tap scan|cable scan|retry_count=[1-9]') { throw 'Generic authorization, automatic CR/LF, or dangerous fallback route detected.' }

$startProcessIndex = $runner.IndexOf('Start-Process -FilePath $OpenOcdExe', [StringComparison]::Ordinal)
foreach ($preflight in @(
    "Assert-FileHash `$Bitstream `$Manifest.fixed_artifacts.bitstream_sha256",
    "Assert-FileHash `$HelloElf `$Manifest.fixed_artifacts.hello_elf_sha256",
    "Assert-FileHash `$ProbeElf `$wsc.ProbeElfSha256",
    "Assert-FileHash `$SocH `$wsc.SocHSha256",
    "Assert-FileHash `$FtdiConfig `$Manifest.fixed_artifacts.ftdi_cfg_sha256",
    "Assert-FileHash `$TargetConfig `$Manifest.fixed_artifacts.target_cfg_sha256",
    'Read-WscContract $WscContractRoot'
)) {
    $index = $runner.IndexOf($preflight, [StringComparison]::Ordinal)
    if ($index -lt 0 -or $index -ge $startProcessIndex) { throw "Preflight does not precede OpenOCD start: $preflight" }
}
Assert-Ordered $runner 'Invoke-GdbRamOnlyLoad $GdbExe $HelloElf' "Write-RunLog `$executionLog 'APB_PHASE_STARTED_AFTER_HELLO_PASS=true'" 'Hello load before APB phase'
Assert-Ordered $runner 'Complete-HelloEcho $serial' "Write-RunLog `$executionLog 'APB_PHASE_STARTED_AFTER_HELLO_PASS=true'" 'Hello/echo PASS before APB phase'
Assert-Ordered $runner '$serial.Open()' 'Invoke-GdbRamOnlyLoad $GdbExe $HelloElf' 'UART capture before Hello load'

$wscHead = (& git -C $WscContractRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $wscHead -cne $WscContractSha -or @(& git -C $WscContractRoot status --porcelain).Count -ne 0) { throw 'WSC contract checkout must be clean at the fixed SHA.' }
$wscRelative = 'competition_project_single_camera\embedded_sw\apb_magic_onchip'
$wscFiles = @{
    document_sha256 = Join-Path $WscContractRoot "$wscRelative\APB_PROBE_DEBUGGER_CONTRACT.md"
    summary_sha256 = Join-Path $WscContractRoot "$wscRelative\artifacts\apb_magic_onchip.contract.txt"
    verifier_sha256 = Join-Path $WscContractRoot "$wscRelative\verify_apb_probe_contract.ps1"
}
foreach ($entry in $wscFiles.GetEnumerator()) {
    if ((Get-Sha256 $entry.Value) -cne $manifest.wsc_contract_files.($entry.Key)) { throw "WSC file hash mismatch: $($entry.Key)" }
}

New-Item -ItemType Directory -Path $RunDir -Force | Out-Null
$mockRunDir = Join-Path $RunDir 'runner-fixtures'
$mockOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runnerPath -Mode Mock -Scenario all -RunDir $mockRunDir -WscContractRoot $WscContractRoot 2>&1
if ($LASTEXITCODE -ne 0) { throw "Runner fixtures failed: $mockOutput" }
$mockText = $mockOutput -join "`n"
foreach ($marker in @(
    'RSP_FIXTURES=PASS', 'UART1_PNP_ALLOWLIST=PASS', 'OPENOCD_ARGUMENT_FLOW=PASS', 'WSC_CONTRACT_CONSUMED=PASS',
    'HELLO_THREE_LINES_FIXTURE=PASS', 'UART_TX_ECHO_FIXTURE=PASS printable=true crlf=false same_byte=true',
    'UART_ECHO_MISMATCH_NEGATIVE=PASS', 'HELLO_BAD_LINE_NEGATIVE=PASS', 'WRONG_POST_LOAD_PC_NEGATIVE=PASS',
    'BAD_ARTIFACT_HASH_NEGATIVE=PASS', 'BAD_APPROVAL_TUPLE_NEGATIVE=PASS',
    'MOCK_SUCCESS=PASS RESULT=SUCCESS RAM_READ_COUNT=4 HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=1 RETRY_COUNT=0',
    'MOCK_TIMEOUT=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=1 RETRY_COUNT=0',
    'MOCK_TRAP=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=1 RETRY_COUNT=0',
    'MOCK_WRONG_PC=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=1 RETRY_COUNT=0',
    'MOCK_WRONG_REASON=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=1 RETRY_COUNT=0',
    'MOCK_HALT_UNCONFIRMED=PASS RESULT=FAILED_CLOSED RAM_READ_COUNT=0 HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=1 RETRY_COUNT=0',
    'PHASE_ORDER_FIXTURE=PASS hello_then_apb=true hello_resume=1 apb_resume=1 retry=0', 'HARDWARE_ACTIONS=NONE'
)) { Assert-Contains $mockText $marker 'runner fixture result' }

$savedErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$liveRejectOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runnerPath -Mode Live -Scenario all -RunDir (Join-Path $RunDir 'live-all-reject') 2>&1
$liveRejectExitCode = $LASTEXITCODE
$ErrorActionPreference = $savedErrorActionPreference
if ($liveRejectExitCode -eq 0 -or ($liveRejectOutput -join "`n") -notmatch 'Live mode rejects Scenario=all before any external action') { throw 'Live Scenario=all did not fail closed before external action.' }

$scenarioDirs = @(Get-ChildItem -LiteralPath $mockRunDir -Directory | Where-Object { $_.Name -match '^mock-(success|timeout|trap|wrong_pc|wrong_reason|halt_unconfirmed)-' })
if ($scenarioDirs.Count -ne 6) { throw "Expected six APB scenario directories, got $($scenarioDirs.Count)." }
foreach ($directory in $scenarioDirs) {
    $log = Get-Content -LiteralPath (Join-Path $directory.FullName 'execution.log') -Raw
    $helloMarker = Get-Content -LiteralPath (Join-Path $directory.FullName 'HELLO_RESUME_ONCE.marker') -Raw
    $apbMarker = Get-Content -LiteralPath (Join-Path $directory.FullName 'APB_RESUME_ONCE.marker') -Raw
    foreach ($required in @('RUN_ID=', 'BASE_SHA=', 'QZS_SHA=', 'WSC_SHA=', 'DESIGN_SHA=', 'PNP=', 'HELLO_POST_LOAD_PC=', 'APB_POST_LOAD_PC=', 'BYTE_COUNTS RX=', 'FINAL_STATE=', 'RETRY_COUNT=0')) { Assert-Contains $log $required 'persistent execution log field' }
    Assert-Ordered $log 'HELLO_ECHO=PASS' 'APB_PHASE_STARTED_AFTER_HELLO_PASS=true' 'Hello PASS before APB log'
    foreach ($marker in @($helloMarker, $apbMarker)) {
        Assert-Contains $marker 'RESUME_COUNT=1' 'phase resume marker count'
        Assert-Contains $marker 'RETRY_COUNT=0' 'phase retry count'
        $ready = [regex]::Match($marker, 'PHASE_READY_TIME=(?<value>[^\s]+)').Groups['value'].Value
        $resume = [regex]::Match($marker, 'RESUME_ONCE_TIME=(?<value>[^\s]+)').Groups['value'].Value
        if ([DateTimeOffset]::Parse($ready) -ge [DateTimeOffset]::Parse($resume)) { throw 'Phase ready time must precede resume time.' }
    }
    if ($directory.Name -notmatch '^mock-success-' -and $log -match '(?m)^.* RAM_g_apb_probe_') { throw "Failure fixture read RAM: $($directory.Name)" }
}
$timeoutLog = Get-Content -LiteralPath (Join-Path (@($scenarioDirs | Where-Object Name -Like 'mock-timeout-*')[0].FullName) 'execution.log') -Raw
$unconfirmedLog = Get-Content -LiteralPath (Join-Path (@($scenarioDirs | Where-Object Name -Like 'mock-halt_unconfirmed-*')[0].FullName) 'execution.log') -Raw
Assert-Contains $timeoutLog 'WATCHDOG_TIMEOUT_MS=1000 ACTIVE_HALT_COUNT=1' 'timeout active halt fixture'
Assert-Contains $unconfirmedLog 'TIMEOUT_HALT_UNCONFIRMED RAM_READ_COUNT=0 RETRY_COUNT=0' 'unconfirmed halt fixture'

foreach ($entry in $manifest.files) {
    $path = Join-Path $repoRoot $entry.path
    if ((Get-Sha256 $path) -cne $entry.sha256) { throw "Manifest hash mismatch: $($entry.path)" }
}
$manifestPaths = @($manifest.files.path)
foreach ($path in $allowedPaths | Where-Object { $_ -notmatch 'i0_uart1_execution_manifest\.json$' }) {
    if ($manifestPaths -cnotcontains $path) { throw "Modified runtime file is not manifest-bound: $path" }
}

foreach ($path in @($runnerPath, $PSCommandPath)) { if ((Get-EolProfile $path) -ne 'crlf') { throw "PS1 EOL mismatch: $path" } }
if ((Get-EolProfile $cfgPath) -ne 'lf') { throw 'CFG EOL mismatch.' }
foreach ($path in Get-ChildItem -LiteralPath $PSScriptRoot -File -Filter '*.gdb') { if ((Get-EolProfile $path.FullName) -ne 'lf') { throw "GDB EOL mismatch: $($path.FullName)" } }

$packet = Get-Content -LiteralPath $packetPath -Raw
$operationCard = Get-Content -LiteralPath $operationCardPath -Raw
foreach ($text in @($packet, $operationCard)) {
    foreach ($clause in @($BaseSha, $QzsAuthorizationSha, $WscContractSha, 'HARDWARE_ACTIONS=NONE', 'USER2=NOT_VERIFIED', 'HELLO_RESUME_COUNT=1', 'APB_RESUME_COUNT=1', 'RETRY_COUNT=0')) { Assert-Contains $text $clause 'review-document clause' }
}
foreach ($hash in @($manifest.raw_build_evidence.restored_evidence_files.sha256)) { Assert-Contains $packet $hash 'restored evidence hash' }
Assert-Contains $packet $manifest.raw_build_evidence.verification_command 'raw evidence command'
Assert-Contains $packet $manifest.raw_build_evidence.key_output 'raw evidence output'
Assert-Contains $packet 'exit_code=0' 'raw evidence exit code'

'RUNNER_MANIFEST_BOUND=PASS'
'OPENOCD_ARGUMENT_FLOW=PASS cputapid=0x006A0EF3 width=6 type=1 outer_ir=0x09 USER2=NOT_VERIFIED'
'LIVE_SCENARIO_ALL_REJECTED=PASS'
'RSP_FIXTURES=PASS'
'HELLO_ECHO_CHAIN=PASS three_lines=true printable_tx=true same_byte_echo=true apb_after_hello=true'
'NEGATIVE_FIXTURES=PASS wrong_pc=true artifact_hash=true approval_tuple=true echo_mismatch=true'
'TIMEOUT_FAIL_CLOSED=PASS TIMEOUT_HALT_UNCONFIRMED=RAM_READ_COUNT_0'
'UART1_PNP_ALLOWLIST=PASS exact_vid_pid_serial_instance=true'
'RESUME_COUNT_MODEL=HELLO_1_APB_1_RETRY_0'
'WSC_CONTRACT_CONSUMPTION=PASS fixed_sha=true hash_bound=true constants_source_parsed=true'
'TEMP_SCOPE_DANGER_ROUTE=PASS'
'EOL_PROFILE=PASS ps1=CRLF gdb_cfg=LF'
'I0_UART1_EXECUTION_CONFIG_STATIC=PASS'
'HARDWARE_ACTIONS=NONE'
