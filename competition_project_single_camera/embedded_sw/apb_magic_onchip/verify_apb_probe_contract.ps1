[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$contractPath = Join-Path $PSScriptRoot 'APB_PROBE_DEBUGGER_CONTRACT.md'
$summaryPath = Join-Path $PSScriptRoot 'artifacts\apb_magic_onchip.contract.txt'

function Assert-Equal {
    param([string]$Name, $Actual, $Expected)
    if ($Actual -cne $Expected) { throw "${Name}: expected '$Expected', got '$Actual'" }
}

function ConvertFrom-Summary {
    param([string]$Text)
    $values = [ordered]@{}
    foreach ($line in ($Text -split "`r?`n")) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line -notmatch '^(?<key>[a-zA-Z0-9_]+)=(?<value>.*)$') {
            throw "Malformed summary line: $line"
        }
        if ($values.Contains($Matches.key)) { throw "Duplicate key: $($Matches.key)" }
        $values[$Matches.key] = $Matches.value
    }
    $values
}

function Invoke-HostGateModel {
    param([ValidateSet('success','timeout','trap','wrong_pc','wrong_reason')]$Scenario)
    $result = [ordered]@{
        state = 'RUNNING'; resume = 1; ram_reads = 0; host_halts = 0
        elapsed = 25; pc = '0xF90000C4'; reason = 'BREAKPOINT'
        retries = 0; debugger_apb_reads = 0; apb_writes = 0
    }
    switch ($Scenario) {
        timeout { $result.elapsed = 1000; $result.pc = '0xF90000B0'; $result.reason = 'HOST_HALT_TIMEOUT'; $result.host_halts = 1 }
        trap { $result.pc = '0xF9000010'; $result.reason = 'TRAP' }
        wrong_pc { $result.pc = '0xF90000C0' }
        wrong_reason { $result.reason = 'STEP' }
    }
    $gate = $result.elapsed -lt 1000 -and $result.pc -ceq '0xF90000C4' -and $result.reason -ceq 'BREAKPOINT'
    if ($gate) { $result.state = 'EVIDENCE_READ'; $result.ram_reads = 4 }
    else { $result.state = 'FAILED_CLOSED' }
    [pscustomobject]$result
}

foreach ($path in @($contractPath, $summaryPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing input: $path" }
}
$contract = (Get-Content -LiteralPath $contractPath -Raw).Replace('`', '')
$summaryText = Get-Content -LiteralPath $summaryPath -Raw

foreach ($clause in @(
    'STATIC CONTRACT ONLY / BOARD NOT AUTHORIZED / BOARD NOT VERIFIED',
    'I0_UART1_20260719_CLEAN_LF_FINAL', 'PC == 0xF9000000',
    'Resume exactly once', 'PC == 0xF90000C4', 'RAM_READ_COUNT=0',
    'UART_CAPTURE_READY', 'printable ASCII byte',
    'automatic CR/LF append', 'zero direct APB reads and zero APB writes'
)) {
    if (-not $contract.Contains($clause)) { throw "Missing contract clause: $clause" }
}

$actual = ConvertFrom-Summary $summaryText
$expected = [ordered]@{
    APB_PROBE_CONTRACT='PASS'; patch_baseline='182fd6f5c4d628379760d6f4fc74e3b342e30083'
    handoff_sha='fd3fc0881d4e71338f1aa34f361cd498b7cd2d4c'; design_input='a840f0869c11bab0915757d64c56a167f6d4f917'
    batch='I0_UART1_20260719_CLEAN_LF_FINAL'; evidence_mode='STATIC_CONTRACT_AND_HOST_STATE_MACHINE'
    board_authorized='false'; board_verified='false'
    soc_h_sha256='25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B'
    elf_sha256='6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC'
    entry='0xF9000000'; load_segments='1'; load_end='0xF90008F0'
    ram_start='0xF9000000'; ram_end='0xF9003FFF'; apb_address='0xE8100000'; apb_offset='0x000'
    main_lw_count='1'; apb_write_count='0'; halt_pc='0xF90000C4'; timeout_ms='1000'
    resume_count='1'; success_ram_read_count='4'; failure_ram_read_count='0'
    debugger_direct_apb_read='PROHIBITED'; retry='PROHIBITED'
    raw_artifacts_committed='false'; hardware_actions='NONE'
}
Assert-Equal 'summary key count' $actual.Count $expected.Count
foreach ($item in $expected.GetEnumerator()) {
    if (-not $actual.Contains($item.Key)) { throw "Missing summary key: $($item.Key)" }
    Assert-Equal $item.Key $actual[$item.Key] $item.Value
}

$results = @{}
foreach ($scenario in @('success','timeout','trap','wrong_pc','wrong_reason')) {
    $results[$scenario] = Invoke-HostGateModel $scenario
    Assert-Equal "$scenario resume" $results[$scenario].resume 1
    Assert-Equal "$scenario retry" $results[$scenario].retries 0
    Assert-Equal "$scenario debugger APB read" $results[$scenario].debugger_apb_reads 0
    Assert-Equal "$scenario APB write" $results[$scenario].apb_writes 0
}
Assert-Equal 'success state' $results.success.state 'EVIDENCE_READ'
Assert-Equal 'success RAM reads' $results.success.ram_reads 4
Assert-Equal 'success halt PC' $results.success.pc '0xF90000C4'
Assert-Equal 'success halt reason' $results.success.reason 'BREAKPOINT'
Assert-Equal 'timeout elapsed' $results.timeout.elapsed 1000
Assert-Equal 'timeout host halt' $results.timeout.host_halts 1
foreach ($scenario in @('timeout','trap','wrong_pc','wrong_reason')) {
    Assert-Equal "$scenario state" $results[$scenario].state 'FAILED_CLOSED'
    Assert-Equal "$scenario RAM reads" $results[$scenario].ram_reads 0
}

$hash = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
'APB_PROBE_CONTRACT=PASS'
'NEGATIVE_SUCCESS=PASS RAM_READ_COUNT=4'
'NEGATIVE_TIMEOUT=PASS RAM_READ_COUNT=0 host_halt_count=1'
'NEGATIVE_TRAP=PASS RAM_READ_COUNT=0'
'NEGATIVE_WRONG_PC=PASS RAM_READ_COUNT=0'
'NEGATIVE_WRONG_HALT_REASON=PASS RAM_READ_COUNT=0'
'STATIC_APB=PASS main_lw_count=1 apb_write_count=0'
'HALT_CONTRACT=PASS halt_pc=0xF90000C4 timeout_ms=1000'
"HARDWARE_ACTIONS=NONE verifier_sha256=$hash"
