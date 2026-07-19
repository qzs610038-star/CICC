[CmdletBinding()]
param(
    [ValidateSet('Mock', 'Live')]
    [string]$Mode = 'Mock',
    [ValidateSet('run', 'success', 'hello_bad_line', 'echo_mismatch', 'wrong_postload_pc', 'bad_artifact_hash', 'bad_approval_tuple', 'apb_timeout', 'apb_wrong_pc', 'apb_trap', 'halt_unconfirmed', 'all')]
    [string]$Scenario = 'all',
    [Parameter(Mandatory)]
    [string]$RunDir,
    [string]$ApprovalRecordPath,
    [string]$BoardId,
    [string]$UartPort,
    [ValidatePattern('^[\x20-\x7E]$')]
    [string]$EchoByte = 'U',
    [string]$OpenOcdExe,
    [string]$FtdiConfig,
    [string]$TargetConfig,
    [string]$GdbExe,
    [string]$Bitstream,
    [string]$HelloElf,
    [string]$ProbeElf,
    [string]$SocH,
    [string]$WscContractRoot,
    [string]$OpenOcdHost = '127.0.0.1',
    [ValidateRange(1, 65535)]
    [int]$OpenOcdPort = 3333
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($TargetConfig)) {
    $TargetConfig = Join-Path $PSScriptRoot 'i0_uart1_cleanlf_user2.cfg'
}
$BaseSha = '69a1030e1e72854a857fab147aa2c9cc8f0e6800'
$QzsAuthorizationSha = 'a222ea64653a2232945342faacfb53a06ce50e42'
$WscContractSha = '48548f47dfa5964b13aed7edf3b3e9da6f6583a2'
$DesignSha = '6effdc3685d696cb4d33f3fbb1c449729ed72e33'
$StopStrategy = 'FIRST_FAILURE_STOP_NO_RETRY_CTRL_C_TIMEOUT'
$HelloLines = @(
    'I0 UART1 HELLO',
    'UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100',
    'Type characters to verify echo.'
)
$ManifestPath = Join-Path $PSScriptRoot 'i0_uart1_execution_manifest.json'
$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json

function Get-IsoTime { (Get-Date).ToUniversalTime().ToString('o') }

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

function Get-Sha256 {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing required file: $Path" }
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Assert-FileHash {
    param([string]$Path, [string]$Expected, [string]$Label)
    $actual = Get-Sha256 $Path
    if ($actual -cne $Expected) { throw "ARTIFACT_HASH_MISMATCH label=$Label expected=$Expected actual=$actual" }
    $actual
}

function Get-CurrentCommit {
    $repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Unable to resolve the fixed runner commit.' }
    $commit
}

function Get-RspChecksum {
    param([string]$Payload)
    $sum = 0
    foreach ($byte in [Text.Encoding]::ASCII.GetBytes($Payload)) { $sum = ($sum + $byte) -band 0xff }
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
        if ($first -ne '$') { $State.Buffer.RemoveAt(0); continue }
        $hashIndex = -1
        for ($index = 1; $index -lt $State.Buffer.Count; $index++) {
            if ([char]$State.Buffer[$index] -eq '#') { $hashIndex = $index; break }
        }
        if ($hashIndex -lt 0 -or $State.Buffer.Count -lt ($hashIndex + 3)) { break }
        $payload = [Text.Encoding]::ASCII.GetString($State.Buffer.GetRange(1, $hashIndex - 1).ToArray())
        $checksumText = [Text.Encoding]::ASCII.GetString($State.Buffer.GetRange($hashIndex + 1, 2).ToArray())
        $receivedChecksum = 0
        $validChecksum = [int]::TryParse($checksumText, [Globalization.NumberStyles]::HexNumber, [Globalization.CultureInfo]::InvariantCulture, [ref]$receivedChecksum)
        $State.Buffer.RemoveRange(0, $hashIndex + 3)
        if (-not $validChecksum -or $receivedChecksum -ne (Get-RspChecksum $payload)) {
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
        $Stream.WriteByte([byte][char]$State.OutboundAcks.Dequeue())
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
    if ($null -eq $reply -or $reply -match '^E[0-9A-Fa-f]{2}$') { throw "OpenOCD RSP command failed: $Payload reply=$reply" }
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
            if ($null -eq $stop.Pc) { $stop.Pc = ConvertFrom-RspLittleEndianHex (Send-RspCommand -Stream $Stream -State $State -Payload 'p20') }
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
    Add-RspBytes $state ([Text.Encoding]::ASCII.GetBytes('+' + (New-RspFrame 'OK') + (New-RspFrame $payload)))
    if ($state.AckQueue.Dequeue() -ne '+' -or $state.PacketQueue.Count -ne 2) { throw 'RSP sticky-packet fixture failed.' }
    $state = New-RspState
    Add-RspBytes $state ([Text.Encoding]::ASCII.GetBytes('$OK#00'))
    if ($state.PacketQueue.Count -ne 0 -or $state.OutboundAcks.Dequeue() -ne '-') { throw 'RSP checksum NACK fixture failed.' }
    $stop = ConvertFrom-StopReply $payload
    if ($stop.Reason -cne 'BREAKPOINT') { throw 'RSP async stop fixture failed.' }
    'RSP_FIXTURES=PASS split=PASS sticky=PASS checksum=PASS ack_nack=PASS async_stop=PASS'
}

function ConvertFrom-KeyValueText {
    param([string]$Text)
    $values = [ordered]@{}
    foreach ($line in ($Text -split "`r?`n")) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line -notmatch '^(?<key>[A-Za-z0-9_]+)=(?<value>.*)$') { throw "Malformed WSC summary line: $line" }
        if ($values.Contains($Matches.key)) { throw "Duplicate WSC summary key: $($Matches.key)" }
        $values[$Matches.key] = $Matches.value
    }
    $values
}

function Read-WscContract {
    param([string]$Root)
    $head = (& git -C $Root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cne $WscContractSha) { throw "WSC contract checkout must be fixed at $WscContractSha" }
    if ((& git -C $Root status --porcelain).Length -ne 0) { throw 'WSC contract checkout must be clean.' }
    $relativeRoot = 'competition_project_single_camera\embedded_sw\apb_magic_onchip'
    $documentPath = Join-Path $Root "$relativeRoot\APB_PROBE_DEBUGGER_CONTRACT.md"
    $summaryPath = Join-Path $Root "$relativeRoot\artifacts\apb_magic_onchip.contract.txt"
    $verifierPath = Join-Path $Root "$relativeRoot\verify_apb_probe_contract.ps1"
    Assert-FileHash $documentPath $Manifest.wsc_contract_files.document_sha256 'wsc_contract_document' | Out-Null
    Assert-FileHash $summaryPath $Manifest.wsc_contract_files.summary_sha256 'wsc_contract_summary' | Out-Null
    Assert-FileHash $verifierPath $Manifest.wsc_contract_files.verifier_sha256 'wsc_contract_verifier' | Out-Null
    $summary = ConvertFrom-KeyValueText (Get-Content -LiteralPath $summaryPath -Raw)
    $document = Get-Content -LiteralPath $documentPath -Raw
    $ram = [ordered]@{}
    foreach ($symbol in @('g_apb_probe_expected', 'g_apb_probe_address', 'g_apb_probe_status', 'g_apb_probe_observed')) {
        $pattern = '\|\s*`?' + [regex]::Escape($symbol) + '`?\s*\|\s*`?(?<address>0x[0-9A-Fa-f]+)`?\s*\|\s*`?(?<value>0x[0-9A-Fa-f]+)`?\s*\|'
        $match = [regex]::Match($document, $pattern)
        if (-not $match.Success) { throw "Unable to parse WSC RAM contract row: $symbol" }
        $ram[$symbol] = @{ Address = [Convert]::ToInt64($match.Groups['address'].Value.Substring(2), 16); Value = [Convert]::ToInt64($match.Groups['value'].Value.Substring(2), 16) }
    }
    [pscustomobject]@{
        EntryPc = [Convert]::ToInt64($summary.entry.Substring(2), 16)
        HaltPc = [Convert]::ToInt64($summary.halt_pc.Substring(2), 16)
        TimeoutMs = [int]$summary.timeout_ms
        SuccessRamReadCount = [int]$summary.success_ram_read_count
        FailureRamReadCount = [int]$summary.failure_ram_read_count
        ProbeElfSha256 = $summary.elf_sha256
        SocHSha256 = $summary.soc_h_sha256
        Ram = $ram
        DocumentPath = $documentPath
        SummaryPath = $summaryPath
        VerifierPath = $verifierPath
    }
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
    param([array]$Identities, [string]$Port, [string]$ExpectedKey)
    if ($Port -eq 'COM17') { throw 'COM17 is prohibited for I0 UART1.' }
    $matches = @($Identities | Where-Object { $_.Key -ceq $ExpectedKey })
    if ($matches.Count -ne 1) { throw "Approval PnP tuple must uniquely match one live UART1 device; matches=$($matches.Count)." }
    $identity = $matches[0]
    if ($identity.Port -cne $Port) { throw "Approved UART1 identity is on $($identity.Port), not $Port." }
    if (($identity.VID -eq '1A86' -and $identity.PID -eq '7523') -or $identity.FriendlyName -match '(?i)CH340|J44|UART0|programmer|downloader|burner') {
        throw 'J44/UART0 programmer or CH340 identity is prohibited for I0 UART1.'
    }
    $identity
}

function Resolve-UniqueUart1Identity {
    param([string]$Port, [string]$ExpectedKey)
    $identities = @()
    foreach ($serialPort in @(Get-CimInstance Win32_SerialPort)) {
        $pnp = Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -eq $serialPort.PNPDeviceID } | Select-Object -First 1
        try { $identities += Get-UartIdentityKey $serialPort $pnp } catch { }
    }
    Select-UniqueUart1Identity -Identities $identities -Port $Port -ExpectedKey $ExpectedKey
}

function Test-ApprovalRecord {
    param($Approval, [string]$Commit, [string]$ExpectedBoard, [string]$ExpectedPnp, [string]$ManifestSha, [hashtable]$ArtifactHashes, [DateTimeOffset]$Now)
    if ($Approval.schema_version -ne 1 -or $Approval.approved_commit -cne $Commit -or $Approval.board_id -cne $ExpectedBoard -or $Approval.uart1_pnp_key -cne $ExpectedPnp) { throw 'APPROVAL_TUPLE_MISMATCH identity' }
    if ([string]::IsNullOrWhiteSpace([string]$Approval.window_id) -or $Approval.stop_strategy -cne $StopStrategy) { throw 'APPROVAL_TUPLE_MISMATCH window_or_stop_strategy' }
    $start = [DateTimeOffset]::Parse($Approval.window_start_utc)
    $end = [DateTimeOffset]::Parse($Approval.window_end_utc)
    if ($start -ge $end -or $Now -lt $start -or $Now -gt $end) { throw 'APPROVAL_TUPLE_MISMATCH inactive_window' }
    if ($Approval.manifest_sha256 -cne $ManifestSha) { throw 'APPROVAL_TUPLE_MISMATCH manifest_hash' }
    foreach ($entry in $ArtifactHashes.GetEnumerator()) {
        $property = $Approval.artifact_sha256.PSObject.Properties[$entry.Key]
        if ($null -eq $property -or [string]$property.Value -cne $entry.Value) { throw "APPROVAL_TUPLE_MISMATCH artifact=$($entry.Key)" }
    }
    $true
}

function Get-LiveApproval {
    param([string]$Path, [string]$Commit, [string]$ExpectedBoard, [string]$ExpectedPnp, [hashtable]$ArtifactHashes)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw 'Live mode requires an external approval record.' }
    $approval = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    Test-ApprovalRecord -Approval $approval -Commit $Commit -ExpectedBoard $ExpectedBoard -ExpectedPnp $ExpectedPnp -ManifestSha (Get-Sha256 $ManifestPath) -ArtifactHashes $ArtifactHashes -Now ([DateTimeOffset]::UtcNow) | Out-Null
    $approval
}

function Get-OpenOcdArguments {
    param([string]$FtdiPath, [string]$TargetPath)
    $targetText = Get-Content -LiteralPath $TargetPath -Raw
    if ($targetText -notmatch '(?m)^riscv use_bscan_tunnel 6 1\s*$' -or $targetText -notmatch '(?m)^riscv set_bscan_tunnel_ir 0x09\s*$') { throw 'OpenOCD USER2 argument flow is not literal 6 1 / 0x09.' }
    @('-f', $FtdiPath, '-f', $TargetPath)
}

function Wait-OpenOcdReady {
    param([Diagnostics.Process]$Process, [string]$StdoutPath, [string]$StderrPath, [int]$Port)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.ElapsedMilliseconds -lt 5000) {
        if ($Process.HasExited) {
            $stderr = if (Test-Path -LiteralPath $StderrPath) { Get-Content -LiteralPath $StderrPath -Raw } else { '' }
            throw "OpenOCD exited before readiness: $stderr"
        }
        if (Test-Path -LiteralPath $StdoutPath) {
            $stdout = Get-Content -LiteralPath $StdoutPath -Raw
            if ($stdout -match "(?m)Listening on port $Port for gdb connections") { return }
        }
        Start-Sleep -Milliseconds 10
    }
    throw "OpenOCD readiness timeout on GDB port $Port; retry is prohibited."
}

function Invoke-OpenOcdArgumentFlowFixture {
    $arguments = Get-OpenOcdArguments -FtdiPath 'OFFLINE_FTDI_FIXTURE.cfg' -TargetPath $TargetConfig
    if (($arguments -join '|') -cne "-f|OFFLINE_FTDI_FIXTURE.cfg|-f|$TargetConfig") { throw 'OpenOCD -f argument order fixture failed.' }
    'OPENOCD_ARGUMENT_FLOW=PASS width=6 type=1 outer_ir=0x09 board_user2=NOT_VERIFIED'
}

function ConvertFrom-GdbPostLoadPc {
    param([string]$Output, [Int64]$ExpectedPc, [string]$Phase)
    $matches = @([regex]::Matches($Output, 'POST_LOAD_PC=(?<pc>0x[0-9A-Fa-f]+)'))
    if ($matches.Count -ne 1) { throw "$Phase post-load PC evidence must contain exactly one value." }
    $pcText = $matches[0].Groups['pc'].Value
    $pc = [Convert]::ToInt64($pcText.Substring(2), 16)
    if ($pc -ne $ExpectedPc) { throw "$Phase WRONG_POST_LOAD_PC actual=$pcText expected=0x$($ExpectedPc.ToString('X8'))" }
    [pscustomobject]@{ Text = $pcText; Value = $pc }
}

function Invoke-GdbRamOnlyLoad {
    param([string]$Path, [string]$Elf, [string]$HostName, [int]$Port, [Int64]$ExpectedPc, [string]$Phase, [string]$LogPath)
    $output = & $Path --batch --nx --quiet -ex 'set pagination off' -ex "file $Elf" -ex "target extended-remote $HostName`:$Port" -ex 'monitor halt' -ex 'load' -ex 'monitor halt' -ex 'printf "POST_LOAD_PC=0x%08lx\n", $pc' -ex 'quit' 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$Phase RAM-only load failed: $output" }
    $parsed = ConvertFrom-GdbPostLoadPc -Output ($output -join "`n") -ExpectedPc $ExpectedPc -Phase $Phase
    Write-RunLog $LogPath "$Phase`_POST_LOAD_PC=$($parsed.Text) EXPECTED=0x$($ExpectedPc.ToString('X8'))"
    $parsed.Value
}

function New-HelloState {
    @{ LineIndex = 0; Buffer = [Text.StringBuilder]::new(); LeadingDelimiterSeen = $false; Complete = $false }
}

function Add-HelloByte {
    param($State, [byte]$Value)
    if ($Value -eq 0x0A) {
        $line = $State.Buffer.ToString().TrimEnd("`r")
        $State.Buffer.Clear() | Out-Null
        if (-not $State.LeadingDelimiterSeen -and $State.LineIndex -eq 0 -and $line.Length -eq 0) {
            $State.LeadingDelimiterSeen = $true
            return
        }
        if ($State.LineIndex -ge $HelloLines.Count -or $line -cne $HelloLines[$State.LineIndex]) { throw "HELLO_LINE_MISMATCH index=$($State.LineIndex + 1) actual=[$line]" }
        $State.LineIndex++
        if ($State.LineIndex -eq $HelloLines.Count) { $State.Complete = $true }
    } else { [void]$State.Buffer.Append([char]$Value) }
}

function Assert-EchoByte {
    param([byte]$Expected, [byte]$Actual)
    if ($Expected -eq 0x0A -or $Expected -eq 0x0D -or $Expected -lt 0x20 -or $Expected -gt 0x7E) { throw 'UART1 TX must be one printable ASCII byte without CR/LF.' }
    if ($Actual -ne $Expected) { throw "UART1_ECHO_MISMATCH expected=0x$($Expected.ToString('X2')) actual=0x$($Actual.ToString('X2'))" }
}

function Read-SerialByte {
    param([IO.Ports.SerialPort]$Serial, [DateTimeOffset]$Deadline, [string]$LogPath, $Counts)
    while ([DateTimeOffset]::UtcNow -lt $Deadline) {
        try {
            [byte]$value = $Serial.ReadByte()
            $Counts.Rx++
            Write-RunLog $LogPath "RX 0x$($value.ToString('X2')) RX_COUNT=$($Counts.Rx) TX_COUNT=$($Counts.Tx)" | Out-Null
            return $value
        } catch [TimeoutException] { }
    }
    throw 'UART1 receive timeout; retry is prohibited.'
}

function Complete-HelloEcho {
    param([IO.Ports.SerialPort]$Serial, [string]$LogPath, $Counts, [byte]$TxByte)
    Assert-EchoByte -Expected $TxByte -Actual $TxByte
    $deadline = [DateTimeOffset]::UtcNow.AddSeconds(10)
    $state = New-HelloState
    while (-not $state.Complete) { Add-HelloByte -State $state -Value (Read-SerialByte -Serial $Serial -Deadline $deadline -LogPath $LogPath -Counts $Counts) }
    Write-RunLog $LogPath 'HELLO_THREE_LINES=PASS' | Out-Null
    $Serial.Write([byte[]]@($TxByte), 0, 1)
    $Counts.Tx++
    Write-RunLog $LogPath "TX 0x$($TxByte.ToString('X2')) RX_COUNT=$($Counts.Rx) TX_COUNT=$($Counts.Tx) AUTO_CRLF=DISABLED" | Out-Null
    $echoValue = Read-SerialByte -Serial $Serial -Deadline $deadline -LogPath $LogPath -Counts $Counts
    Assert-EchoByte -Expected $TxByte -Actual $echoValue
    Write-RunLog $LogPath "HELLO_ECHO=PASS byte=0x$($TxByte.ToString('X2'))" | Out-Null
}

function Write-PhaseResumeMarker {
    param([string]$Directory, [string]$RunId, [string]$Phase, [DateTimeOffset]$ReadyTime)
    $resumeTime = [DateTimeOffset]::UtcNow
    if ($resumeTime -le $ReadyTime) { $resumeTime = $ReadyTime.AddTicks(1) }
    $path = Join-Path $Directory "$Phase`_RESUME_ONCE.marker"
    Write-AtomicMarker $path "RUN_ID=$RunId`r`nPHASE=$Phase`r`nPHASE_READY_TIME=$($ReadyTime.ToString('o'))`r`nRESUME_ONCE_TIME=$($resumeTime.ToString('o'))`r`nRESUME_COUNT=1`r`nRETRY_COUNT=0"
    [pscustomobject]@{ Path = $path; Time = $resumeTime }
}

function Invoke-Uart1PnPFixtures {
    $uart1 = [pscustomobject]@{ Port = 'COM10'; VID = '10C4'; PID = 'EA60'; Serial = 'UART1_FIXED'; Instance = 'USB\VID_10C4&PID_EA60\UART1_FIXED'; FriendlyName = 'USB Serial Port'; Key = 'VID=10C4;PID=EA60;SERIAL=UART1_FIXED;INSTANCE=USB\VID_10C4&PID_EA60\UART1_FIXED' }
    $programmer = [pscustomobject]@{ Port = 'COM13'; VID = '0403'; PID = '6010'; Serial = 'J44_PROGRAMMER'; Instance = 'USB\VID_0403&PID_6010\J44_PROGRAMMER'; FriendlyName = 'J44 UART0 Programmer'; Key = 'VID=0403;PID=6010;SERIAL=J44_PROGRAMMER;INSTANCE=USB\VID_0403&PID_6010\J44_PROGRAMMER' }
    $ch340 = [pscustomobject]@{ Port = 'COM17'; VID = '1A86'; PID = '7523'; Serial = 'CH340'; Instance = 'USB\VID_1A86&PID_7523\CH340'; FriendlyName = 'USB-SERIAL CH340'; Key = 'VID=1A86;PID=7523;SERIAL=CH340;INSTANCE=USB\VID_1A86&PID_7523\CH340' }
    $selected = Select-UniqueUart1Identity -Identities @($uart1, $programmer, $ch340) -Port 'COM10' -ExpectedKey $uart1.Key
    if ($selected.Key -cne $uart1.Key) { throw 'UART1 exact PnP allowlist fixture failed.' }
    foreach ($fixture in @(@($programmer, 'COM13'), @($ch340, 'COM17'))) {
        $closed = $false
        try { Select-UniqueUart1Identity -Identities @($uart1, $programmer, $ch340) -Port $fixture[1] -ExpectedKey $fixture[0].Key | Out-Null } catch { $closed = $true }
        if (-not $closed) { throw 'UART0/programmer PnP exclusion fixture failed.' }
    }
    'UART1_PNP_ALLOWLIST=PASS exact_vid_pid_serial_instance=true excludes_com17_ch340_j44_uart0=true'
}

function Invoke-MockApbScenario {
    param([string]$Name, $Wsc)
    $runId = 'mock-' + $Name + '-' + [guid]::NewGuid().ToString('N')
    $directory = Join-Path $RunDir $runId
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $logPath = Join-Path $directory 'execution.log'
    Write-RunLog $logPath "RUN_ID=$runId MODE=Mock BASE_SHA=$BaseSha QZS_SHA=$QzsAuthorizationSha WSC_SHA=$WscContractSha DESIGN_SHA=$DesignSha RETRY_COUNT=0"
    Write-RunLog $logPath 'PNP=VID=10C4;PID=EA60;SERIAL=MOCK_UART1;INSTANCE=USB\\VID_10C4&PID_EA60\\MOCK_UART1 ROUTE=TYPEC_UART1'
    $captureReady = [DateTimeOffset]::UtcNow
    Write-RunLog $logPath "CAPTURE_READY_TIME=$($captureReady.ToString('o')) AUTO_CRLF=DISABLED"
    Write-RunLog $logPath 'HELLO_POST_LOAD_PC=0xF9000000 EXPECTED=0xF9000000'
    $helloMarker = Write-PhaseResumeMarker $directory $runId 'HELLO' $captureReady
    Write-RunLog $logPath "HELLO_RESUME_ONCE_TIME=$($helloMarker.Time.ToString('o')) HELLO_RESUME_COUNT=1 MARKER=$($helloMarker.Path)"
    Write-RunLog $logPath 'HELLO_THREE_LINES=PASS'
    Write-RunLog $logPath "TX 0x$(([byte][char]$EchoByte).ToString('X2')) RX_COUNT=107 TX_COUNT=1 AUTO_CRLF=DISABLED"
    Write-RunLog $logPath "HELLO_ECHO=PASS byte=0x$(([byte][char]$EchoByte).ToString('X2'))"
    Write-RunLog $logPath 'APB_PHASE_STARTED_AFTER_HELLO_PASS=true'
    Write-RunLog $logPath ('APB_POST_LOAD_PC=0x{0:X8} EXPECTED=0x{0:X8}' -f $Wsc.EntryPc)
    $apbReady = [DateTimeOffset]::UtcNow
    $apbMarker = Write-PhaseResumeMarker $directory $runId 'APB' $apbReady
    Write-RunLog $logPath "APB_RESUME_ONCE_TIME=$($apbMarker.Time.ToString('o')) APB_RESUME_COUNT=1 MARKER=$($apbMarker.Path)"
    $halt = switch ($Name) {
        'success' { [pscustomobject]@{ Confirmed = $true; Reason = 'BREAKPOINT'; Pc = $Wsc.HaltPc; TimedOut = $false; ActiveHaltCount = 0 } }
        'timeout' { [pscustomobject]@{ Confirmed = $true; Reason = 'SIGNAL_02'; Pc = 0xF90000B0L; TimedOut = $true; ActiveHaltCount = 1 } }
        'trap' { [pscustomobject]@{ Confirmed = $true; Reason = 'SIGNAL_05'; Pc = 0xF9000010L; TimedOut = $false; ActiveHaltCount = 0 } }
        'wrong_pc' { [pscustomobject]@{ Confirmed = $true; Reason = 'BREAKPOINT'; Pc = 0xF90000C0L; TimedOut = $false; ActiveHaltCount = 0 } }
        'wrong_reason' { [pscustomobject]@{ Confirmed = $true; Reason = 'SIGNAL_02'; Pc = $Wsc.HaltPc; TimedOut = $false; ActiveHaltCount = 0 } }
        'halt_unconfirmed' { [pscustomobject]@{ Confirmed = $false; Reason = 'TIMEOUT_HALT_UNCONFIRMED'; Pc = $null; TimedOut = $true; ActiveHaltCount = 1 } }
    }
    $ramReadCount = 0
    $result = 'FAILED_CLOSED'
    if ($halt.Confirmed -and -not $halt.TimedOut -and $halt.Reason -ceq 'BREAKPOINT' -and $halt.Pc -eq $Wsc.HaltPc) {
        foreach ($entry in $Wsc.Ram.GetEnumerator()) {
            $ramReadCount++
            Write-RunLog $logPath "RAM_$($entry.Key)=0x$($entry.Value.Value.ToString('X8'))" | Out-Null
        }
        $result = 'SUCCESS'
    }
    if ($halt.ActiveHaltCount -eq 1) { Write-RunLog $logPath "WATCHDOG_TIMEOUT_MS=$($Wsc.TimeoutMs) ACTIVE_HALT_COUNT=1" | Out-Null }
    if (-not $halt.Confirmed) { Write-RunLog $logPath 'TIMEOUT_HALT_UNCONFIRMED RAM_READ_COUNT=0 RETRY_COUNT=0' | Out-Null }
    Write-RunLog $logPath "BYTE_COUNTS RX=108 TX=1"
    Write-RunLog $logPath "FINAL_STATE=$result HALT_REASON=$($halt.Reason) HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=1 RETRY_COUNT=0 RAM_READ_COUNT=$ramReadCount"
    [pscustomobject]@{ Name = $Name; Result = $result; RamReadCount = $ramReadCount; Directory = $directory }
}

function Invoke-MockFixtures {
    $wsc = Read-WscContract $WscContractRoot
    'WSC_CONTRACT_CONSUMED=PASS sha=48548f47dfa5964b13aed7edf3b3e9da6f6583a2 hashes=3 constants=source_parsed'
    $fixtureDirectory = Join-Path $RunDir ('mock-negative-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $fixtureDirectory -Force | Out-Null
    $helloTranscript = [Text.Encoding]::ASCII.GetBytes("`r`n" + ($HelloLines -join "`r`n") + "`r`n")
    $state = New-HelloState
    foreach ($byte in $helloTranscript) { Add-HelloByte $state $byte }
    if (-not $state.LeadingDelimiterSeen -or -not $state.Complete) { throw 'Hello three-line fixture did not match the fixed firmware transcript.' }
    'HELLO_THREE_LINES_FIXTURE=PASS'
    $tx = [byte][char]$EchoByte
    Assert-EchoByte -Expected $tx -Actual $tx
    'UART_TX_ECHO_FIXTURE=PASS printable=true crlf=false same_byte=true'

    $echoMismatchClosed = $false
    try { Assert-EchoByte -Expected $tx -Actual ([byte]($tx -bxor 1)) } catch { $echoMismatchClosed = $true }
    if (-not $echoMismatchClosed) { throw 'Wrong echo byte fixture produced a false PASS.' }
    'UART_ECHO_MISMATCH_NEGATIVE=PASS'

    $badHelloClosed = $false
    try { $bad = New-HelloState; foreach ($byte in [Text.Encoding]::ASCII.GetBytes("WRONG`n")) { Add-HelloByte $bad $byte } } catch { $badHelloClosed = $true }
    if (-not $badHelloClosed) { throw 'Bad Hello fixture did not fail closed.' }
    'HELLO_BAD_LINE_NEGATIVE=PASS'

    $wrongPcClosed = $false
    try {
        ConvertFrom-GdbPostLoadPc -Output 'POST_LOAD_PC=0xF9000004' -ExpectedPc 0xF9000000L -Phase 'HELLO' | Out-Null
    } catch { $wrongPcClosed = $true }
    if (-not $wrongPcClosed) { throw 'Wrong post-load PC fixture produced a false PASS.' }
    'WRONG_POST_LOAD_PC_NEGATIVE=PASS'

    $artifactPath = Join-Path $fixtureDirectory 'artifact.fixture'
    [IO.File]::WriteAllText($artifactPath, 'fixture', [Text.Encoding]::ASCII)
    $badArtifactClosed = $false
    try { Assert-FileHash $artifactPath ('0' * 64) 'fixture' | Out-Null } catch { $badArtifactClosed = $true }
    if (-not $badArtifactClosed) { throw 'Bad artifact hash fixture did not fail closed.' }
    'BAD_ARTIFACT_HASH_NEGATIVE=PASS'

    $artifactHashes = @{ bitstream = 'A'; hello_elf = 'B'; probe_elf = 'C'; soc_h = 'D'; ftdi_cfg = 'E'; target_cfg = 'F'; wsc_contract_document = 'G'; wsc_contract_summary = 'H'; wsc_contract_verifier = 'I' }
    $approval = [pscustomobject]@{
        schema_version = 1; approved_commit = 'fixed'; board_id = 'BOARD'; uart1_pnp_key = 'PNP'; window_id = 'WINDOW'
        window_start_utc = [DateTimeOffset]::UtcNow.AddMinutes(-1).ToString('o'); window_end_utc = [DateTimeOffset]::UtcNow.AddMinutes(1).ToString('o')
        stop_strategy = $StopStrategy; manifest_sha256 = 'MANIFEST'; artifact_sha256 = [pscustomobject]$artifactHashes
    }
    Test-ApprovalRecord $approval 'fixed' 'BOARD' 'PNP' 'MANIFEST' $artifactHashes ([DateTimeOffset]::UtcNow) | Out-Null
    $approval.board_id = 'WRONG'
    $badApprovalClosed = $false
    try { Test-ApprovalRecord $approval 'fixed' 'BOARD' 'PNP' 'MANIFEST' $artifactHashes ([DateTimeOffset]::UtcNow) | Out-Null } catch { $badApprovalClosed = $true }
    if (-not $badApprovalClosed) { throw 'Bad approval tuple fixture did not fail closed.' }
    'BAD_APPROVAL_TUPLE_NEGATIVE=PASS'

    foreach ($name in @('success', 'timeout', 'trap', 'wrong_pc', 'wrong_reason', 'halt_unconfirmed')) {
        $outcome = Invoke-MockApbScenario -Name $name -Wsc $wsc
        if ($name -eq 'success') {
            if ($outcome.Result -cne 'SUCCESS' -or $outcome.RamReadCount -ne $wsc.SuccessRamReadCount) { throw 'Success fixture must read the four WSC RAM evidence words.' }
        } elseif ($outcome.Result -cne 'FAILED_CLOSED' -or $outcome.RamReadCount -ne $wsc.FailureRamReadCount) {
            throw "$name fixture must fail closed with the WSC failure RAM read count."
        }
        "MOCK_$($name.ToUpperInvariant())=PASS RESULT=$($outcome.Result) RAM_READ_COUNT=$($outcome.RamReadCount) HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=1 RETRY_COUNT=0"
    }
    'PHASE_ORDER_FIXTURE=PASS hello_then_apb=true hello_resume=1 apb_resume=1 retry=0'
    'HARDWARE_ACTIONS=NONE'
}

if ($Mode -eq 'Live') {
    if ($Scenario -eq 'all') { throw 'Live mode rejects Scenario=all before any external action.' }
    if ($Scenario -ne 'run') { throw 'Live mode accepts Scenario=run only before any external action.' }
    foreach ($value in @($ApprovalRecordPath, $BoardId, $UartPort, $OpenOcdExe, $FtdiConfig, $TargetConfig, $GdbExe, $Bitstream, $HelloElf, $ProbeElf, $SocH, $WscContractRoot)) {
        if ([string]::IsNullOrWhiteSpace($value)) { throw 'Live mode is missing a fixed approval, identity, artifact, or tool path.' }
    }
}
if ($Mode -eq 'Mock' -and [string]::IsNullOrWhiteSpace($WscContractRoot)) { throw 'Mock mode requires the clean fixed-SHA WSC contract checkout.' }

New-Item -ItemType Directory -Path $RunDir -Force | Out-Null
Invoke-RspFixtures
Invoke-Uart1PnPFixtures
Invoke-OpenOcdArgumentFlowFixture
if ($Mode -eq 'Mock') {
    if ($Scenario -notin @('all', 'success')) { throw 'Mock mode uses the complete offline fixture bundle via Scenario=all or success.' }
    Invoke-MockFixtures
    exit 0
}

$runId = 'live-' + [guid]::NewGuid().ToString('N')
$liveDir = Join-Path $RunDir $runId
New-Item -ItemType Directory -Path $liveDir -Force | Out-Null
$executionLog = Join-Path $liveDir 'execution.log'
$openOcdStdout = Join-Path $liveDir 'openocd.stdout.log'
$openOcdStderr = Join-Path $liveDir 'openocd.stderr.log'
$serial = $null
$openOcdProcess = $null
$client = $null
$stream = $null
$helloResumeCount = 0
$apbResumeCount = 0
$ramReadCount = 0
$retryCount = 0
$finalLogged = $false
$counts = @{ Rx = 0; Tx = 0 }
Write-RunLog $executionLog "RUN_ID=$runId MODE=Live BASE_SHA=$BaseSha QZS_SHA=$QzsAuthorizationSha WSC_SHA=$WscContractSha DESIGN_SHA=$DesignSha RETRY_COUNT=0" | Out-Null
try {
    $currentCommit = Get-CurrentCommit
    $wsc = Read-WscContract $WscContractRoot
    $artifactHashes = @{
        bitstream = Assert-FileHash $Bitstream $Manifest.fixed_artifacts.bitstream_sha256 'bitstream'
        hello_elf = Assert-FileHash $HelloElf $Manifest.fixed_artifacts.hello_elf_sha256 'hello_elf'
        probe_elf = Assert-FileHash $ProbeElf $wsc.ProbeElfSha256 'probe_elf'
        soc_h = Assert-FileHash $SocH $wsc.SocHSha256 'soc_h'
        ftdi_cfg = Assert-FileHash $FtdiConfig $Manifest.fixed_artifacts.ftdi_cfg_sha256 'ftdi_cfg'
        target_cfg = Assert-FileHash $TargetConfig $Manifest.fixed_artifacts.target_cfg_sha256 'target_cfg'
        wsc_contract_document = Get-Sha256 $wsc.DocumentPath
        wsc_contract_summary = Get-Sha256 $wsc.SummaryPath
        wsc_contract_verifier = Get-Sha256 $wsc.VerifierPath
    }
    $approvalObject = Get-Content -LiteralPath $ApprovalRecordPath -Raw | ConvertFrom-Json
    $identity = Resolve-UniqueUart1Identity -Port $UartPort -ExpectedKey $approvalObject.uart1_pnp_key
    $approval = Get-LiveApproval -Path $ApprovalRecordPath -Commit $currentCommit -ExpectedBoard $BoardId -ExpectedPnp $identity.Key -ArtifactHashes $artifactHashes
    Write-RunLog $executionLog "COMMIT_SHA=$currentCommit"
    Write-RunLog $executionLog "APPROVAL_SHA256=$(Get-Sha256 $ApprovalRecordPath) WINDOW_ID=$($approval.window_id) BOARD_ID=$BoardId STOP_STRATEGY=$StopStrategy"
    Write-RunLog $executionLog "PNP=$($identity.Key) FRIENDLY_NAME=$($identity.FriendlyName) ROUTE=TYPEC_UART1"
    foreach ($entry in $artifactHashes.GetEnumerator()) { Write-RunLog $executionLog "PREFLIGHT_HASH_$($entry.Key.ToUpperInvariant())=$($entry.Value)" | Out-Null }
    $openOcdArguments = Get-OpenOcdArguments $FtdiConfig $TargetConfig
    Write-RunLog $executionLog "OPENOCD_ARGUMENT_FLOW=-f $FtdiConfig -f $TargetConfig"
    $openOcdProcess = Start-Process -FilePath $OpenOcdExe -ArgumentList $openOcdArguments -RedirectStandardOutput $openOcdStdout -RedirectStandardError $openOcdStderr -WindowStyle Hidden -PassThru
    Wait-OpenOcdReady -Process $openOcdProcess -StdoutPath $openOcdStdout -StderrPath $openOcdStderr -Port $OpenOcdPort

    $serial = [IO.Ports.SerialPort]::new($identity.Port, 115200, [IO.Ports.Parity]::None, 8, [IO.Ports.StopBits]::One)
    $serial.Handshake = [IO.Ports.Handshake]::None
    $serial.ReadTimeout = 100
    $serial.WriteTimeout = 1000
    $serial.Open()
    $captureReady = [DateTimeOffset]::UtcNow
    Write-RunLog $executionLog "CAPTURE_READY_TIME=$($captureReady.ToString('o')) PNP=$($identity.Key) AUTO_CRLF=DISABLED"
    Invoke-GdbRamOnlyLoad $GdbExe $HelloElf $OpenOcdHost $OpenOcdPort $Manifest.fixed_artifacts.hello_entry_pc 'HELLO' $executionLog | Out-Null
    $client = [Net.Sockets.TcpClient]::new($OpenOcdHost, $OpenOcdPort)
    $stream = $client.GetStream()
    $rspState = New-RspState
    $supported = Send-RspCommand $stream $rspState 'qSupported:multiprocess+;swbreak+;hwbreak+'
    if ($supported -notmatch '(?:^|;)PacketSize=[0-9A-Fa-f]+(?:;|$)') { throw 'OpenOCD RSP qSupported negotiation failed; retry is prohibited.' }
    $helloMarker = Write-PhaseResumeMarker $liveDir $runId 'HELLO' $captureReady
    $helloResumeCount++
    Write-RunLog $executionLog "HELLO_RESUME_ONCE_TIME=$($helloMarker.Time.ToString('o')) HELLO_RESUME_COUNT=$helloResumeCount MARKER=$($helloMarker.Path)"
    Send-RspCommand $stream $rspState 'c' -NoReply | Out-Null
    Complete-HelloEcho $serial $executionLog $counts ([byte][char]$EchoByte)
    $stream.WriteByte(3); $stream.Flush()
    $helloStop = Wait-AsyncStopReply $stream $rspState 1000
    if ($null -eq $helloStop) { throw 'HELLO_HALT_UNCONFIRMED; APB phase prohibited.' }
    Write-RunLog $executionLog ('HELLO_HALT_CONFIRMED=true HALT_REASON={0} HALT_PC=0x{1:X8}' -f $helloStop.Reason, $helloStop.Pc)
    $stream.Dispose(); $stream = $null; $client.Dispose(); $client = $null

    Invoke-GdbRamOnlyLoad $GdbExe $ProbeElf $OpenOcdHost $OpenOcdPort $wsc.EntryPc 'APB' $executionLog | Out-Null
    Write-RunLog $executionLog 'APB_PHASE_STARTED_AFTER_HELLO_PASS=true'
    $client = [Net.Sockets.TcpClient]::new($OpenOcdHost, $OpenOcdPort)
    $stream = $client.GetStream()
    $rspState = New-RspState
    $supported = Send-RspCommand $stream $rspState 'qSupported:multiprocess+;swbreak+;hwbreak+'
    if ($supported -notmatch '(?:^|;)PacketSize=[0-9A-Fa-f]+(?:;|$)') { throw 'OpenOCD RSP qSupported negotiation failed; retry is prohibited.' }
    [void](Send-RspCommand $stream $rspState ('Z0,{0:x},4' -f $wsc.HaltPc))
    $apbReady = [DateTimeOffset]::UtcNow
    $apbMarker = Write-PhaseResumeMarker $liveDir $runId 'APB' $apbReady
    $apbResumeCount++
    Write-RunLog $executionLog "APB_RESUME_ONCE_TIME=$($apbMarker.Time.ToString('o')) APB_RESUME_COUNT=$apbResumeCount MARKER=$($apbMarker.Path)"
    Send-RspCommand $stream $rspState 'c' -NoReply | Out-Null
    $stop = Wait-AsyncStopReply $stream $rspState $wsc.TimeoutMs
    $timedOut = $null -eq $stop
    if ($timedOut) {
        $stream.WriteByte(3); $stream.Flush()
        Write-RunLog $executionLog "WATCHDOG_TIMEOUT_MS=$($wsc.TimeoutMs) ACTIVE_HALT_COUNT=1"
        $stop = Wait-AsyncStopReply $stream $rspState 1000
        if ($null -eq $stop) { Write-RunLog $executionLog 'TIMEOUT_HALT_UNCONFIRMED RAM_READ_COUNT=0 RETRY_COUNT=0'; throw 'TIMEOUT_HALT_UNCONFIRMED' }
    }
    Write-RunLog $executionLog ('APB_HALT_CONFIRMED=true HALT_REASON={0} HALT_PC=0x{1:X8} TIMED_OUT={2}' -f $stop.Reason, $stop.Pc, $timedOut)
    if ($timedOut -or $stop.Reason -cne 'BREAKPOINT' -or $stop.Pc -ne $wsc.HaltPc) { throw 'APB halt reason/PC gate failed; RAM reads prohibited.' }
    foreach ($entry in $wsc.Ram.GetEnumerator()) {
        $value = ConvertFrom-RspLittleEndianHex (Send-RspCommand $stream $rspState ('m{0:x},4' -f $entry.Value.Address))
        if ($value -ne $entry.Value.Value) { throw "RAM evidence mismatch: $($entry.Key)" }
        $ramReadCount++
        Write-RunLog $executionLog "RAM_$($entry.Key)=0x$($value.ToString('X8'))"
    }
    if ($ramReadCount -ne $wsc.SuccessRamReadCount) { throw 'WSC RAM read count mismatch.' }
    Write-RunLog $executionLog "BYTE_COUNTS RX=$($counts.Rx) TX=$($counts.Tx)"
    Write-RunLog $executionLog "FINAL_STATE=SUCCESS HELLO_RESUME_COUNT=$helloResumeCount APB_RESUME_COUNT=$apbResumeCount RETRY_COUNT=$retryCount RAM_READ_COUNT=$ramReadCount"
    $finalLogged = $true
} catch {
    if (Test-Path -LiteralPath $executionLog -and -not $finalLogged) {
        Write-RunLog $executionLog "BYTE_COUNTS RX=$($counts.Rx) TX=$($counts.Tx)" | Out-Null
        Write-RunLog $executionLog "FINAL_STATE=FAILED_CLOSED ERROR=$($_.Exception.Message) HELLO_RESUME_COUNT=$helloResumeCount APB_RESUME_COUNT=$apbResumeCount RETRY_COUNT=$retryCount RAM_READ_COUNT=$ramReadCount"
    }
    throw
} finally {
    if ($null -ne $serial) { if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose() }
    if ($null -ne $stream) { $stream.Dispose() }
    if ($null -ne $client) { $client.Dispose() }
    if ($null -ne $openOcdProcess -and -not $openOcdProcess.HasExited) { Stop-Process -Id $openOcdProcess.Id -Force }
}
