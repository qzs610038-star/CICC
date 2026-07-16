[CmdletBinding()]
param(
    [ValidatePattern('^COM(?:3|7|8)$')]
    [string]$PortName = 'COM3',
    [ValidateRange(3, 10)]
    [int]$ListenSeconds = 5,
    [string]$PcGateApproval,
    [string]$EvidenceDir,
    [string]$Label = 'm2_uart0_banner_capture',
    [switch]$Listen
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $PSScriptRoot '..\docs\debug_sessions\evidence'
}

$expectedBitSha256 = '2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347'
$expectedElfSha256 = 'C99FD39DB437409A63A6061CD29698B5B60099B9E24A77B155B871E169BF5DA5'
$requiredApprovalResult = 'M2_USER2_RAM_PC_GATE_APPROVED'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Read-PcGateApproval {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{ exists = $false; valid = $false; reason = 'approval_json_missing'; path = $Path }
    }

    try {
        $approval = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    }
    catch {
        return [ordered]@{ exists = $true; valid = $false; reason = 'approval_json_unparseable'; path = $Path }
    }

    $result = if ($null -ne $approval.PSObject.Properties['result']) { "$($approval.result)" } else { '' }
    $bitHash = if ($null -ne $approval.PSObject.Properties['bitstream_sha256']) { "$($approval.bitstream_sha256)" } else { '' }
    $elfHash = if ($null -ne $approval.PSObject.Properties['elf_sha256']) { "$($approval.elf_sha256)" } else { '' }
    $pcRange = if ($null -ne $approval.PSObject.Properties['pc_range']) { "$($approval.pc_range)" } else { '' }
    $valid = $result -eq $requiredApprovalResult -and
        $bitHash -eq $expectedBitSha256 -and
        $elfHash -eq $expectedElfSha256 -and
        $pcRange -eq '0xF9000000..0xF9003FFF'

    return [ordered]@{
        exists = $true
        valid = $valid
        reason = if ($valid) { 'approved' } else { 'approval_fields_do_not_match_current_m2_batch' }
        path = (Resolve-Path -LiteralPath $Path).Path
        result = $result
        bitstream_sha256 = $bitHash
        elf_sha256 = $elfHash
        pc_range = $pcRange
    }
}

function Get-ConnectedPortNames {
    try {
        Add-Type -AssemblyName System.IO.Ports -ErrorAction Stop
        return @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
    }
    catch {
        return @()
    }
}

$approval = Read-PcGateApproval -Path $PcGateApproval
$connectedPortNames = Get-ConnectedPortNames
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "git rev-parse HEAD failed with exit code $LASTEXITCODE"
}

$serialPortOpened = $false
$rawBytes = [System.Collections.Generic.List[byte]]::new()
$listenException = $null
$openAttempted = $false

if ($Listen -and -not $approval.valid) {
    $result = 'HOLD_PC_GATE_APPROVAL_REQUIRED_NO_SERIAL_OPEN'
}
elseif (-not $Listen) {
    $result = 'DRY_RUN_NO_SERIAL_OPEN'
}
elseif ($connectedPortNames -notcontains $PortName) {
    $result = 'HOLD_SELECTED_PORT_NOT_ENUMERATED_NO_SERIAL_OPEN'
}
else {
    $port = $null
    try {
        Add-Type -AssemblyName System.IO.Ports -ErrorAction Stop
        $port = [System.IO.Ports.SerialPort]::new($PortName, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
        $port.Handshake = [System.IO.Ports.Handshake]::None
        $port.DtrEnable = $false
        $port.RtsEnable = $false
        $port.ReadTimeout = 100
        $port.WriteTimeout = 100
        $openAttempted = $true
        $port.Open()
        $serialPortOpened = $true

        $deadline = [DateTime]::UtcNow.AddSeconds($ListenSeconds)
        while ([DateTime]::UtcNow -lt $deadline) {
            if ($port.BytesToRead -gt 0) {
                $buffer = [byte[]]::new($port.BytesToRead)
                $read = $port.Read($buffer, 0, $buffer.Length)
                if ($read -gt 0) {
                    for ($index = 0; $index -lt $read; $index++) {
                        [void]$rawBytes.Add($buffer[$index])
                    }
                }
            }
            Start-Sleep -Milliseconds 50
        }
    }
    catch {
        $listenException = $_.Exception.Message
    }
    finally {
        if ($null -ne $port -and $port.IsOpen) {
            $port.Close()
        }
        if ($null -ne $port) {
            $port.Dispose()
        }
    }

    if ($null -ne $listenException) {
        $result = 'HOLD_SERIAL_LISTEN_FAILED_NO_UART_BYTES_SENT'
    }
    else {
        $asciiText = [System.Text.Encoding]::ASCII.GetString($rawBytes.ToArray())
        $hasFullBanner = $asciiText -match '(?s)TJ375 CPU\+VIDEO UART0 HELLO\r?\nONCHIP_RAM=0xF9000000 UART0=115200 8N1\r?\nType characters to verify echo\.'
        $result = if ($hasFullBanner) { 'UART0_LISTEN_FULL_BANNER_CAPTURED_NO_TX' } else { 'UART0_LISTEN_COMPLETE_NO_FULL_BANNER_NO_TX' }
    }
}

$asciiText = [System.Text.Encoding]::ASCII.GetString($rawBytes.ToArray())
$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$evidencePath = Join-Path (Resolve-Path $EvidenceDir).Path ("{0}_{1}.json" -f $Label, $timestamp)

$evidence = [ordered]@{
    schema = 'm2_uart0_banner_capture_v1'
    timestamp_local = (Get-Date).ToString('o')
    repo_head = $head
    requested_port = $PortName
    listen_requested = [bool]$Listen
    listen_seconds = $ListenSeconds
    approval = $approval
    connected_port_names = $connectedPortNames
    serial_open_attempted = $openAttempted
    serial_port_opened = $serialPortOpened
    uart_bytes_sent = 0
    dtr_enabled = $false
    rts_enabled = $false
    flow_control = 'none'
    baud = 115200
    data_bits = 8
    parity = 'none'
    stop_bits = 1
    raw_rx_byte_count = $rawBytes.Count
    raw_rx_base64 = [Convert]::ToBase64String($rawBytes.ToArray())
    raw_rx_ascii = $asciiText
    listener_exception = $listenException
    programmer_invoked = $false
    openocd_invoked = $false
    gdb_invoked = $false
    flash_operation_invoked = $false
    result = $result
    next_action = if ($result -eq 'UART0_LISTEN_FULL_BANNER_CAPTURED_NO_TX') {
        'Return this JSON and terminal-equivalent raw text to Codex. Do not send a character until Codex issues a separate single-echo operation card.'
    }
    elseif ($result -eq 'DRY_RUN_NO_SERIAL_OPEN') {
        'No serial action occurred. After Codex creates a matching PC-gate approval JSON, rerun with -Listen for one read-only capture.'
    }
    else {
        'Stop at the current layer. Do not send bytes, try another UART class, use COM11, or touch UART2/J52/myCobot.'
    }
}

$evidence | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $evidencePath -Encoding utf8
[PSCustomObject]@{
    result = $result
    serial_port_opened = $serialPortOpened
    uart_bytes_sent = 0
    raw_rx_byte_count = $rawBytes.Count
    evidence_path = $evidencePath
} | ConvertTo-Json -Depth 4

if ($Listen -and $result -ne 'UART0_LISTEN_FULL_BANNER_CAPTURED_NO_TX' -and $result -ne 'UART0_LISTEN_COMPLETE_NO_FULL_BANNER_NO_TX') {
    exit 2
}
