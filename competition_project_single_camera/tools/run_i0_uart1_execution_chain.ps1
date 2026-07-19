[CmdletBinding()]
param(
    [ValidateSet('Mock', 'Live')]
    [string]$Mode = 'Mock',
    [ValidateSet('run', 'success', 'timeout', 'trap', 'wrong_pc', 'wrong_reason', 'halt_unconfirmed', 'all')]
    [string]$Scenario = 'all',
    [Parameter(Mandatory)]
    [string]$RunDir,
    [string]$UartPort,
    [string[]]$Uart1PnPAllowlist = @(),
    [ValidatePattern('^[\x20-\x7E]$')]
    [string]$EchoByte = 'U',
    [string]$OpenOcdExe,
    [string]$FtdiConfig,
    [string]$TargetConfig,
    [string]$GdbExe,
    [string]$ProbeElf,
    [string]$OpenOcdHost = '127.0.0.1',
    [ValidateRange(1, 65535)]
    [int]$OpenOcdPort = 3333,
    [string]$ApprovalToken
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($TargetConfig)) {
    $TargetConfig = Join-Path $PSScriptRoot 'i0_uart1_cleanlf_user2.cfg'
}
$BaseSha = '2d713b80a41185e472837abaec3a10c01383c70f'
$QzsAuthorizationSha = 'a222ea64653a2232945342faacfb53a06ce50e42'
$WscContractSha = '48548f47dfa5964b13aed7edf3b3e9da6f6583a2'
$DesignSha = '6effdc3685d696cb4d33f3fbb1c449729ed72e33'
$ExpectedEntryPc = 0xF9000000L
$ExpectedHaltPc = 0xF90000C4L
$TimeoutMs = 1000
$ExpectedRam = [ordered]@{
    'g_apb_probe_expected' = @{ Address = 0xF90000D0L; Value = 0x375A0001L }
    'g_apb_probe_address' = @{ Address = 0xF90000D4L; Value = 0xE8100000L }
    'g_apb_probe_status' = @{ Address = 0xF90000E4L; Value = 0x50415353L }
    'g_apb_probe_observed' = @{ Address = 0xF90000E8L; Value = 0x375A0001L }
}

function Get-IsoTime {
    (Get-Date).ToUniversalTime().ToString('o')
}

function Write-RunLog {
    param([string]$Path, [string]$Message)
    $line = '{0} {1}' -f (Get-IsoTime), $Message
    $line | Add-Content -LiteralPath $Path -Encoding ascii
    $line
}

function Write-AtomicMarker {
    param([string]$Path, [string]$Content)
    $temporary = "$Path.$([guid]::NewGuid().ToString('N')).tmp"
    [IO.File]::WriteAllText($temporary, $Content + "`r`n", [Text.Encoding]::ASCII)
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

function Get-RspChecksum {
    param([string]$Payload)
    $sum = 0
    foreach ($byte in [Text.Encoding]::ASCII.GetBytes($Payload)) {
        $sum = ($sum + $byte) -band 0xff
    }
    $sum
}

function New-RspFrame {
    param([string]$Payload)
    '${0}#{1:x2}' -f $Payload, (Get-RspChecksum $Payload)
}

function New-RspState {
    @{
        Buffer = [Collections.Generic.List[byte]]::new()
        AckQueue = [Collections.Generic.Queue[char]]::new()
        PacketQueue = [Collections.Generic.Queue[string]]::new()
        OutboundAcks = [Collections.Generic.Queue[char]]::new()
    }
}

function Add-RspBytes {
    param($State, [byte[]]$Bytes)
    foreach ($byte in $Bytes) { [void]$State.Buffer.Add($byte) }
    while ($State.Buffer.Count -gt 0) {
        $first = [char]$State.Buffer[0]
        if ($first -eq '+' -or $first -eq '-') {
            $State.AckQueue.Enqueue($first)
            $State.Buffer.RemoveAt(0)
            continue
        }
        if ($first -ne '$') {
            $State.Buffer.RemoveAt(0)
            continue
        }
        $hashIndex = -1
        for ($index = 1; $index -lt $State.Buffer.Count; $index++) {
            if ([char]$State.Buffer[$index] -eq '#') { $hashIndex = $index; break }
        }
        if ($hashIndex -lt 0 -or $State.Buffer.Count -lt ($hashIndex + 3)) { break }
        $payloadBytes = $State.Buffer.GetRange(1, $hashIndex - 1).ToArray()
        $payload = [Text.Encoding]::ASCII.GetString($payloadBytes)
        $checksumText = [Text.Encoding]::ASCII.GetString($State.Buffer.GetRange($hashIndex + 1, 2).ToArray())
        $receivedChecksum = 0
        $validChecksumText = [int]::TryParse($checksumText, [Globalization.NumberStyles]::HexNumber, [Globalization.CultureInfo]::InvariantCulture, [ref]$receivedChecksum)
        $frameLength = $hashIndex + 3
        $State.Buffer.RemoveRange(0, $frameLength)
        if (-not $validChecksumText -or $receivedChecksum -ne (Get-RspChecksum $payload)) {
            $State.OutboundAcks.Enqueue('-')
            continue
        }
        $State.OutboundAcks.Enqueue('+')
        $State.PacketQueue.Enqueue($payload)
    }
}

function Send-PendingRspAcks {
    param([Net.Sockets.NetworkStream]$Stream, $State)
    while ($State.OutboundAcks.Count -gt 0) {
        $ack = [byte][char]$State.OutboundAcks.Dequeue()
        $Stream.WriteByte($ack)
        $Stream.Flush()
    }
}

function Receive-RspData {
    param([Net.Sockets.NetworkStream]$Stream, $State)
    if (-not $Stream.DataAvailable) { return }
    $buffer = [byte[]]::new(4096)
    $read = $Stream.Read($buffer, 0, $buffer.Length)
    if ($read -le 0) { throw 'OpenOCD RSP connection closed.' }
    $chunk = [byte[]]::new($read)
    [Array]::Copy($buffer, $chunk, $read)
    Add-RspBytes -State $State -Bytes $chunk
    Send-PendingRspAcks -Stream $Stream -State $State
}

function Wait-RspAck {
    param([Net.Sockets.NetworkStream]$Stream, $State, [int]$TimeoutMilliseconds)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.ElapsedMilliseconds -lt $TimeoutMilliseconds) {
        if ($State.AckQueue.Count -gt 0) {
            $ack = $State.AckQueue.Dequeue()
            if ($ack -eq '-') { throw 'OpenOCD RSP returned NACK; retry is prohibited.' }
            return
        }
        Receive-RspData -Stream $Stream -State $State
        Start-Sleep -Milliseconds 2
    }
    throw 'OpenOCD RSP ACK timeout; retry is prohibited.'
}

function Wait-RspPacket {
    param([Net.Sockets.NetworkStream]$Stream, $State, [int]$TimeoutMilliseconds)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.ElapsedMilliseconds -lt $TimeoutMilliseconds) {
        if ($State.PacketQueue.Count -gt 0) { return $State.PacketQueue.Dequeue() }
        Receive-RspData -Stream $Stream -State $State
        Start-Sleep -Milliseconds 2
    }
    $null
}

function Send-RspCommand {
    param([Net.Sockets.NetworkStream]$Stream, $State, [string]$Payload, [switch]$NoReply)
    $bytes = [Text.Encoding]::ASCII.GetBytes((New-RspFrame $Payload))
    $Stream.Write($bytes, 0, $bytes.Length)
    $Stream.Flush()
    Wait-RspAck -Stream $Stream -State $State -TimeoutMilliseconds 1000
    if ($NoReply) { return $null }
    $reply = Wait-RspPacket -Stream $Stream -State $State -TimeoutMilliseconds 1000
    if ($null -eq $reply -or $reply -match '^E[0-9A-Fa-f]{2}$') {
        throw "OpenOCD RSP command failed: $Payload reply=$reply"
    }
    $reply
}

function ConvertFrom-RspLittleEndianHex {
    param([string]$Hex)
    if ($Hex -notmatch '^(?:[0-9A-Fa-f]{2})+$') { throw "Invalid RSP hex value: $Hex" }
    $pairs = @([regex]::Matches($Hex, '..') | ForEach-Object { $_.Value })
    [array]::Reverse($pairs)
    [Convert]::ToInt64(($pairs -join ''), 16)
}

function ConvertFrom-StopReply {
    param([string]$Reply)
    if ($Reply -notmatch '^[TS](?<signal>[0-9A-Fa-f]{2})') { return $null }
    $signal = $Matches.signal.ToUpperInvariant()
    $reason = if ($Reply -match '(?:^|;)reason:(?:swbreak|breakpoint);|(?:^|;)swbreak:') { 'BREAKPOINT' } else { 'SIGNAL_' + $signal }
    $pc = $null
    if ($Reply -match '(?:^|;)20:(?<pc>[0-9A-Fa-f]+);') { $pc = ConvertFrom-RspLittleEndianHex $Matches.pc }
    [pscustomobject]@{ Reason = $reason; Pc = $pc; Raw = $Reply }
}

function Wait-AsyncStopReply {
    param([Net.Sockets.NetworkStream]$Stream, $State, [int]$TimeoutMilliseconds)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.ElapsedMilliseconds -lt $TimeoutMilliseconds) {
        $remaining = [Math]::Max(1, $TimeoutMilliseconds - [int]$timer.ElapsedMilliseconds)
        $reply = Wait-RspPacket -Stream $Stream -State $State -TimeoutMilliseconds ([Math]::Min(25, $remaining))
        if ($null -eq $reply) { continue }
        $stop = ConvertFrom-StopReply $reply
        if ($null -ne $stop) {
            if ($null -eq $stop.Pc) {
                $stop.Pc = ConvertFrom-RspLittleEndianHex (Send-RspCommand -Stream $Stream -State $State -Payload 'p20')
            }
            return $stop
        }
    }
    $null
}

function Invoke-RspFixtures {
    $payload = 'T05thread:1;20:c40000f9;reason:swbreak;'
    $frame = New-RspFrame $payload
    $state = New-RspState
    $bytes = [Text.Encoding]::ASCII.GetBytes($frame)
    Add-RspBytes $state $bytes[0..6]
    if ($state.PacketQueue.Count -ne 0) { throw 'RSP half-packet fixture emitted early.' }
    Add-RspBytes $state $bytes[7..($bytes.Length - 1)]
    if ($state.PacketQueue.Dequeue() -cne $payload -or $state.OutboundAcks.Dequeue() -ne '+') { throw 'RSP split-packet fixture failed.' }

    $state = New-RspState
    $sticky = '+' + (New-RspFrame 'OK') + (New-RspFrame $payload)
    Add-RspBytes $state ([Text.Encoding]::ASCII.GetBytes($sticky))
    if ($state.AckQueue.Dequeue() -ne '+' -or $state.PacketQueue.Count -ne 2) { throw 'RSP sticky-packet fixture failed.' }

    $state = New-RspState
    Add-RspBytes $state ([Text.Encoding]::ASCII.GetBytes('$OK#00'))
    if ($state.PacketQueue.Count -ne 0 -or $state.OutboundAcks.Dequeue() -ne '-') { throw 'RSP checksum NACK fixture failed.' }

    $state = New-RspState
    Add-RspBytes $state ([Text.Encoding]::ASCII.GetBytes('-'))
    if ($state.AckQueue.Dequeue() -ne '-') { throw 'RSP command NACK fixture failed.' }

    $stop = ConvertFrom-StopReply $payload
    if ($stop.Reason -cne 'BREAKPOINT' -or $stop.Pc -ne $ExpectedHaltPc) { throw 'RSP async stop fixture failed.' }
    'RSP_FIXTURES=PASS split=PASS sticky=PASS checksum=PASS ack_nack=PASS async_stop=PASS'
}

function Get-UartIdentityKey {
    param($SerialPort, $PnpEntity)
    $instance = [string]$SerialPort.PNPDeviceID
    $match = [regex]::Match($instance, '(?i)VID_([0-9A-F]{4})&PID_([0-9A-F]{4})')
    if (-not $match.Success) { throw "PnP identity has no VID/PID: $instance" }
    $serial = ($instance -split '\\')[-1]
    [pscustomobject]@{
        Port = [string]$SerialPort.DeviceID
        VID = $match.Groups[1].Value.ToUpperInvariant()
        PID = $match.Groups[2].Value.ToUpperInvariant()
        Serial = $serial
        Instance = $instance
        FriendlyName = [string]$PnpEntity.Name
        Key = "VID=$($match.Groups[1].Value.ToUpperInvariant());PID=$($match.Groups[2].Value.ToUpperInvariant());SERIAL=$serial;INSTANCE=$instance"
    }
}

function Select-UniqueUart1Identity {
    param([array]$Identities, [string]$Port, [string[]]$Allowlist)
    if ($Port -eq 'COM17') { throw 'COM17 is prohibited for I0 UART1.' }
    if ($Allowlist.Count -eq 0) { throw 'Live UART1 requires an exact VID/PID/serial/instance allowlist.' }
    $matches = @($Identities | Where-Object { $Allowlist -ccontains $_.Key })
    if ($matches.Count -ne 1) { throw "UART1 allowlist must uniquely match one live device; matches=$($matches.Count)." }
    $identity = $matches[0]
    if ($identity.Port -cne $Port) { throw "Allowlisted UART1 identity is on $($identity.Port), not requested $Port." }
    if ($identity.VID -eq '1A86' -and $identity.PID -eq '7523') { throw 'CH340 is prohibited for I0 UART1.' }
    if ($identity.FriendlyName -match '(?i)CH340|J44|UART0|programmer|downloader|burner') { throw 'J44/UART0 programmer identity is prohibited for I0 UART1.' }
    $identity
}

function Resolve-UniqueUart1Identity {
    param([string]$Port, [string[]]$Allowlist)
    $identities = @()
    foreach ($serialPort in @(Get-CimInstance Win32_SerialPort)) {
        $pnp = Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -eq $serialPort.PNPDeviceID } | Select-Object -First 1
        try { $identities += Get-UartIdentityKey $serialPort $pnp } catch { }
    }
    Select-UniqueUart1Identity -Identities $identities -Port $Port -Allowlist $Allowlist
}

function Invoke-Uart1PnPFixtures {
    $uart1 = [pscustomobject]@{ Port = 'COM10'; VID = '10C4'; PID = 'EA60'; Serial = 'UART1_FIXED'; Instance = 'USB\\VID_10C4&PID_EA60\\UART1_FIXED'; FriendlyName = 'USB Serial Port'; Key = 'VID=10C4;PID=EA60;SERIAL=UART1_FIXED;INSTANCE=USB\\VID_10C4&PID_EA60\\UART1_FIXED' }
    $programmer = [pscustomobject]@{ Port = 'COM13'; VID = '0403'; PID = '6010'; Serial = 'J44_PROGRAMMER'; Instance = 'USB\\VID_0403&PID_6010\\J44_PROGRAMMER'; FriendlyName = 'J44 UART0 Programmer'; Key = 'VID=0403;PID=6010;SERIAL=J44_PROGRAMMER;INSTANCE=USB\\VID_0403&PID_6010\\J44_PROGRAMMER' }
    $ch340 = [pscustomobject]@{ Port = 'COM17'; VID = '1A86'; PID = '7523'; Serial = 'CH340'; Instance = 'USB\\VID_1A86&PID_7523\\CH340'; FriendlyName = 'USB-SERIAL CH340'; Key = 'VID=1A86;PID=7523;SERIAL=CH340;INSTANCE=USB\\VID_1A86&PID_7523\\CH340' }
    $selected = Select-UniqueUart1Identity -Identities @($uart1, $programmer, $ch340) -Port 'COM10' -Allowlist @($uart1.Key)
    if ($selected.Key -cne $uart1.Key) { throw 'UART1 exact allowlist fixture selected the wrong identity.' }
    foreach ($fixture in @(
        { Select-UniqueUart1Identity -Identities @($uart1, $programmer) -Port 'COM10' -Allowlist @($uart1.Key, $programmer.Key) },
        { Select-UniqueUart1Identity -Identities @($programmer) -Port 'COM13' -Allowlist @($programmer.Key) },
        { Select-UniqueUart1Identity -Identities @($ch340) -Port 'COM17' -Allowlist @($ch340.Key) }
    )) {
        $failedClosed = $false
        try { & $fixture | Out-Null } catch { $failedClosed = $true }
        if (-not $failedClosed) { throw 'UART1 prohibited/ambiguous PnP fixture did not fail closed.' }
    }
    'UART1_PNP_ALLOWLIST=PASS exact=PASS ambiguous=FAIL_CLOSED j44_uart0=FAIL_CLOSED ch340_com17=FAIL_CLOSED'
}

function Start-UartCapture {
    param($Identity, [string]$ReadyPath, [string]$StopPath, [string]$ByteLogPath)
    Start-Job -ScriptBlock {
        param($JobIdentity, $JobReadyPath, $JobStopPath, $JobByteLogPath)
        $serial = [IO.Ports.SerialPort]::new($JobIdentity.Port, 115200, [IO.Ports.Parity]::None, 8, [IO.Ports.StopBits]::One)
        $serial.ReadTimeout = 100
        $rx = 0
        try {
            $serial.Open()
            $readyTime = (Get-Date).ToUniversalTime().ToString('o')
            "CAPTURE_READY_TIME=$readyTime PNP=$($JobIdentity.Key)" | Set-Content -LiteralPath $JobReadyPath -Encoding ascii
            while (-not (Test-Path -LiteralPath $JobStopPath)) {
                try {
                    [byte]$value = $serial.ReadByte()
                    $rx++
                    "$( (Get-Date).ToUniversalTime().ToString('o') ) RX 0x$($value.ToString('X2')) RX_COUNT=$rx TX_COUNT=0" | Add-Content -LiteralPath $JobByteLogPath -Encoding ascii
                } catch [TimeoutException] { }
            }
        } finally {
            if ($serial.IsOpen) { $serial.Close() }
            $serial.Dispose()
            "BYTE_COUNTS RX=$rx TX=0" | Add-Content -LiteralPath $JobByteLogPath -Encoding ascii
        }
    } -ArgumentList $Identity, $ReadyPath, $StopPath, $ByteLogPath
}

function Get-OpenOcdArguments {
    param([string]$FtdiPath, [string]$TargetPath, [switch]$RequireFiles)
    if ($RequireFiles) {
        foreach ($path in @($FtdiPath, $TargetPath)) {
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing OpenOCD config: $path" }
        }
    }
    $targetText = Get-Content -LiteralPath $TargetPath -Raw
    if ($targetText -notmatch '(?m)^riscv use_bscan_tunnel 6 1\s*$' -or
        $targetText -notmatch '(?m)^riscv set_bscan_tunnel_ir 0x09\s*$') {
        throw 'OpenOCD USER2 argument flow is not literal 6 1 / 0x09.'
    }
    @('-f', $FtdiPath, '-f', $TargetPath)
}

function Invoke-OpenOcdArgumentFlowFixture {
    $arguments = Get-OpenOcdArguments -FtdiPath 'OFFLINE_FTDI_FIXTURE.cfg' -TargetPath $TargetConfig
    if (($arguments -join '|') -cne "-f|OFFLINE_FTDI_FIXTURE.cfg|-f|$TargetConfig") { throw 'OpenOCD -f argument order fixture failed.' }
    'OPENOCD_ARGUMENT_FLOW=PASS width=6 type=1 outer_ir=0x09 board_user2=NOT_VERIFIED'
}

function Invoke-GdbRamOnlyLoad {
    param([string]$Path, [string]$Elf, [string]$HostName, [int]$Port)
    foreach ($file in @($Path, $Elf)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing live prerequisite: $file" }
    }
    $output = & $Path --batch --nx --quiet -ex 'set pagination off' -ex "file $Elf" -ex "target extended-remote $HostName`:$Port" -ex 'monitor halt' -ex 'load' -ex 'monitor halt' -ex 'p/x $pc' -ex 'quit' 2>&1
    if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -notmatch '0xf9000000') { throw "RAM-only load or entry PC gate failed: $output" }
}

function Invoke-MockScenario {
    param([string]$Name)
    $runId = 'mock-{0}-{1}' -f $Name, ([guid]::NewGuid().ToString('N'))
    $scenarioDir = Join-Path $RunDir $runId
    New-Item -ItemType Directory -Path $scenarioDir -Force | Out-Null
    $logPath = Join-Path $scenarioDir 'execution.log'
    $markerPath = Join-Path $scenarioDir 'RESUME_ONCE.marker'
    $captureReady = [DateTimeOffset]::UtcNow
    Write-RunLog $logPath "RUN_ID=$runId MODE=Mock BASE_SHA=$BaseSha QZS_SHA=$QzsAuthorizationSha WSC_SHA=$WscContractSha DESIGN_SHA=$DesignSha"
    Write-RunLog $logPath 'PNP=VID=10C4;PID=EA60;SERIAL=MOCK_UART1;INSTANCE=USB\\VID_10C4&PID_EA60\\MOCK_UART1 ROUTE=TYPEC_UART1'
    Write-RunLog $logPath "CAPTURE_READY_TIME=$($captureReady.ToString('o'))"
    $resumeTime = [DateTimeOffset]::UtcNow.AddMilliseconds(1)
    Write-AtomicMarker $markerPath "RUN_ID=$runId`r`nRESUME_ONCE_TIME=$($resumeTime.ToString('o'))`r`nRESUME_COUNT=1"
    Write-RunLog $logPath "RESUME_ONCE_TIME=$($resumeTime.ToString('o')) RESUME_COUNT=1"
    $mockBytes = [Text.Encoding]::ASCII.GetBytes("I0 UART1 HELLO`n")
    $rx = 0
    foreach ($byte in $mockBytes) { $rx++; Write-RunLog $logPath "RX 0x$($byte.ToString('X2')) RX_COUNT=$rx TX_COUNT=0" | Out-Null }
    Write-RunLog $logPath "TX 0x$(([byte][char]$EchoByte).ToString('X2')) RX_COUNT=$rx TX_COUNT=1" | Out-Null
    $halt = switch ($Name) {
        'success' { [pscustomobject]@{ Confirmed = $true; Reason = 'BREAKPOINT'; Pc = $ExpectedHaltPc; TimedOut = $false; ActiveHaltCount = 0 } }
        'timeout' { [pscustomobject]@{ Confirmed = $true; Reason = 'HOST_INTERRUPT'; Pc = 0xF90000B0L; TimedOut = $true; ActiveHaltCount = 1 } }
        'trap' { [pscustomobject]@{ Confirmed = $true; Reason = 'SIGNAL_05'; Pc = 0xF9000010L; TimedOut = $false; ActiveHaltCount = 0 } }
        'wrong_pc' { [pscustomobject]@{ Confirmed = $true; Reason = 'BREAKPOINT'; Pc = 0xF90000C0L; TimedOut = $false; ActiveHaltCount = 0 } }
        'wrong_reason' { [pscustomobject]@{ Confirmed = $true; Reason = 'SIGNAL_02'; Pc = $ExpectedHaltPc; TimedOut = $false; ActiveHaltCount = 0 } }
        'halt_unconfirmed' { [pscustomobject]@{ Confirmed = $false; Reason = 'TIMEOUT_HALT_UNCONFIRMED'; Pc = $null; TimedOut = $true; ActiveHaltCount = 1 } }
    }
    $ramReadCount = 0
    $result = 'FAILED_CLOSED'
    if ($halt.Confirmed -and -not $halt.TimedOut -and $halt.Reason -ceq 'BREAKPOINT' -and $halt.Pc -eq $ExpectedHaltPc) {
        foreach ($entry in $ExpectedRam.GetEnumerator()) {
            $ramReadCount++
            Write-RunLog $logPath "RAM_$($entry.Key)=0x$($entry.Value.Value.ToString('X8'))" | Out-Null
        }
        $result = 'SUCCESS'
    }
    if ($halt.ActiveHaltCount -eq 1) { Write-RunLog $logPath 'WATCHDOG_TIMEOUT_MS=1000 ACTIVE_HALT_COUNT=1' | Out-Null }
    if (-not $halt.Confirmed) { Write-RunLog $logPath 'TIMEOUT_HALT_UNCONFIRMED RAM_READ_COUNT=0 RETRY_COUNT=0' | Out-Null }
    Write-RunLog $logPath "BYTE_COUNTS RX=$rx TX=1" | Out-Null
    Write-RunLog $logPath "FINAL_STATE=$result HALT_REASON=$($halt.Reason) RAM_READ_COUNT=$ramReadCount RESUME_COUNT=1 RETRY_COUNT=0" | Out-Null
    [pscustomobject]@{ Scenario = $Name; Result = $result; RamReadCount = $ramReadCount; ResumeCount = 1; MarkerPath = $markerPath; LogPath = $logPath }
}

if ($Mode -eq 'Live') {
    if ($Scenario -eq 'all') { throw 'Live mode rejects Scenario=all before any external action.' }
    if ($Scenario -ne 'run') { throw 'Live mode accepts Scenario=run only; fixture outcome labels are prohibited.' }
    if ($ApprovalToken -cne 'I0_EXECUTION_WINDOW_APPROVED') { throw 'Live mode requires the separately approved hardware-window token.' }
    foreach ($value in @($UartPort, $OpenOcdExe, $FtdiConfig, $TargetConfig, $GdbExe, $ProbeElf)) {
        if ([string]::IsNullOrWhiteSpace($value)) { throw 'Live mode is missing an explicit fixed path or UART port.' }
    }
}

New-Item -ItemType Directory -Path $RunDir -Force | Out-Null
Invoke-RspFixtures
Invoke-Uart1PnPFixtures
Invoke-OpenOcdArgumentFlowFixture

if ($Mode -eq 'Mock') {
    if ($Scenario -eq 'run') { throw 'Mock mode requires an explicit fixture outcome or Scenario=all.' }
    $scenarios = if ($Scenario -eq 'all') { @('success', 'timeout', 'trap', 'wrong_pc', 'wrong_reason', 'halt_unconfirmed') } else { @($Scenario) }
    foreach ($item in $scenarios) {
        $outcome = Invoke-MockScenario $item
        if ($outcome.ResumeCount -ne 1) { throw "$item RESUME_COUNT must be 1." }
        if ($item -eq 'success') {
            if ($outcome.Result -ne 'SUCCESS' -or $outcome.RamReadCount -ne 4) { throw 'Success fixture must read four RAM words.' }
        } elseif ($outcome.Result -ne 'FAILED_CLOSED' -or $outcome.RamReadCount -ne 0) {
            throw "$item must fail closed with RAM_READ_COUNT=0."
        }
        "MOCK_$($item.ToUpperInvariant())=PASS RESULT=$($outcome.Result) RAM_READ_COUNT=$($outcome.RamReadCount) RESUME_COUNT=1"
    }
    'HARDWARE_ACTIONS=NONE'
    exit 0
}

$runId = 'live-{0}' -f ([guid]::NewGuid().ToString('N'))
$liveDir = Join-Path $RunDir $runId
New-Item -ItemType Directory -Path $liveDir -Force | Out-Null
$executionLog = Join-Path $liveDir 'execution.log'
$byteLog = Join-Path $liveDir 'uart_bytes.log'
$readyPath = Join-Path $liveDir 'CAPTURE_READY.marker'
$resumeMarker = Join-Path $liveDir 'RESUME_ONCE.marker'
$captureStop = Join-Path $liveDir 'capture.stop'
$openOcdStdout = Join-Path $liveDir 'openocd.stdout.log'
$openOcdStderr = Join-Path $liveDir 'openocd.stderr.log'
$resumeCount = 0
$ramReadCount = 0
$retryCount = 0
$finalLogged = $false
$captureJob = $null
$openOcdProcess = $null
$client = $null
$stream = $null
$rspState = New-RspState
try {
    Write-RunLog $executionLog "RUN_ID=$runId MODE=Live BASE_SHA=$BaseSha QZS_SHA=$QzsAuthorizationSha WSC_SHA=$WscContractSha DESIGN_SHA=$DesignSha"
    $identity = Resolve-UniqueUart1Identity -Port $UartPort -Allowlist $Uart1PnPAllowlist
    Write-RunLog $executionLog "PNP=$($identity.Key) FRIENDLY_NAME=$($identity.FriendlyName) ROUTE=TYPEC_UART1"
    $openOcdArguments = Get-OpenOcdArguments -FtdiPath $FtdiConfig -TargetPath $TargetConfig -RequireFiles
    Write-RunLog $executionLog "OPENOCD_ARGUMENT_FLOW=-f $FtdiConfig -f $TargetConfig"
    $openOcdProcess = Start-Process -FilePath $OpenOcdExe -ArgumentList $openOcdArguments -RedirectStandardOutput $openOcdStdout -RedirectStandardError $openOcdStderr -WindowStyle Hidden -PassThru
    Invoke-GdbRamOnlyLoad -Path $GdbExe -Elf $ProbeElf -HostName $OpenOcdHost -Port $OpenOcdPort
    $captureJob = Start-UartCapture -Identity $identity -ReadyPath $readyPath -StopPath $captureStop -ByteLogPath $byteLog
    $readyTimer = [Diagnostics.Stopwatch]::StartNew()
    while (-not (Test-Path -LiteralPath $readyPath) -and $readyTimer.ElapsedMilliseconds -lt 5000) { Start-Sleep -Milliseconds 10 }
    if (-not (Test-Path -LiteralPath $readyPath)) { throw 'UART1 capture did not produce CAPTURE_READY.' }
    $readyContent = (Get-Content -LiteralPath $readyPath -Raw).Trim()
    Write-RunLog $executionLog $readyContent
    $captureReadyTime = [DateTimeOffset]::Parse(($readyContent -split '[ =]')[1])
    $client = [Net.Sockets.TcpClient]::new($OpenOcdHost, $OpenOcdPort)
    $stream = $client.GetStream()
    $supported = Send-RspCommand -Stream $stream -State $rspState -Payload 'qSupported:multiprocess+;swbreak+;hwbreak+'
    if ($supported -notmatch 'PacketSize=') { throw 'OpenOCD RSP qSupported negotiation failed; retry is prohibited.' }
    [void](Send-RspCommand -Stream $stream -State $rspState -Payload ('Z0,{0:x},4' -f $ExpectedHaltPc))
    $resumeTime = [DateTimeOffset]::UtcNow
    if ($resumeTime -le $captureReadyTime) { throw 'CAPTURE_READY_TIME must be earlier than RESUME_ONCE_TIME.' }
    $resumeCount++
    if ($resumeCount -ne 1) { throw 'Global resume count invariant violated.' }
    Write-AtomicMarker $resumeMarker "RUN_ID=$runId`r`nCAPTURE_READY_TIME=$($captureReadyTime.ToString('o'))`r`nRESUME_ONCE_TIME=$($resumeTime.ToString('o'))`r`nRESUME_COUNT=1"
    Write-RunLog $executionLog "RESUME_ONCE_TIME=$($resumeTime.ToString('o')) RESUME_COUNT=1 MARKER=$resumeMarker"
    Send-RspCommand -Stream $stream -State $rspState -Payload 'c' -NoReply | Out-Null
    $stop = Wait-AsyncStopReply -Stream $stream -State $rspState -TimeoutMilliseconds $TimeoutMs
    $timedOut = $null -eq $stop
    if ($timedOut) {
        $stream.WriteByte(3)
        $stream.Flush()
        Write-RunLog $executionLog "WATCHDOG_TIMEOUT_MS=$TimeoutMs ACTIVE_HALT=CTRL_C"
        $stop = Wait-AsyncStopReply -Stream $stream -State $rspState -TimeoutMilliseconds 1000
        if ($null -eq $stop) {
            Write-RunLog $executionLog 'TIMEOUT_HALT_UNCONFIRMED RAM_READ_COUNT=0 RETRY_COUNT=0'
            Write-RunLog $executionLog 'FINAL_STATE=FAILED_CLOSED RESUME_COUNT=1 RAM_READ_COUNT=0 RETRY_COUNT=0'
            $finalLogged = $true
            throw 'TIMEOUT_HALT_UNCONFIRMED'
        }
    }
    Write-RunLog $executionLog ('HALT_CONFIRMED=true HALT_REASON={0} HALT_PC=0x{1:X8} TIMED_OUT={2}' -f $stop.Reason, $stop.Pc, $timedOut)
    if ($timedOut -or $stop.Reason -cne 'BREAKPOINT' -or $stop.Pc -ne $ExpectedHaltPc) {
        Write-RunLog $executionLog 'FINAL_STATE=FAILED_CLOSED RESUME_COUNT=1 RAM_READ_COUNT=0 RETRY_COUNT=0'
        $finalLogged = $true
        throw 'Halt reason/PC gate failed; RAM reads are prohibited.'
    }
    foreach ($entry in $ExpectedRam.GetEnumerator()) {
        $value = ConvertFrom-RspLittleEndianHex (Send-RspCommand -Stream $stream -State $rspState -Payload ('m{0:x},4' -f $entry.Value.Address))
        if ($value -ne $entry.Value.Value) { throw "RAM evidence mismatch: $($entry.Key)" }
        $ramReadCount++
        Write-RunLog $executionLog "RAM_$($entry.Key)=0x$($value.ToString('X8'))"
    }
    Write-RunLog $executionLog 'FINAL_STATE=SUCCESS RESUME_COUNT=1 RAM_READ_COUNT=4 RETRY_COUNT=0'
    $finalLogged = $true
} catch {
    if (Test-Path -LiteralPath $executionLog) {
        if (-not $finalLogged) {
            Write-RunLog $executionLog "FINAL_STATE=FAILED_CLOSED ERROR=$($_.Exception.Message) RESUME_COUNT=$resumeCount RAM_READ_COUNT=$ramReadCount RETRY_COUNT=$retryCount"
        }
    }
    throw
} finally {
    if ($null -ne $captureJob) {
        New-Item -ItemType File -Path $captureStop -Force | Out-Null
        Wait-Job -Job $captureJob -Timeout 5 | Out-Null
        Remove-Job -Job $captureJob -Force
    }
    if (Test-Path -LiteralPath $byteLog) {
        Get-Content -LiteralPath $byteLog | Add-Content -LiteralPath $executionLog -Encoding ascii
    } elseif (Test-Path -LiteralPath $executionLog) {
        Write-RunLog $executionLog 'BYTE_COUNTS RX=0 TX=0'
    }
    if ($null -ne $stream) { $stream.Dispose() }
    if ($null -ne $client) { $client.Dispose() }
    if ($null -ne $openOcdProcess -and -not $openOcdProcess.HasExited) { Stop-Process -Id $openOcdProcess.Id -Force }
}
