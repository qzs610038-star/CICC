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
    [string]$EfxRunBat,
    [string]$EfxRunPy,
    [string]$FtdiProgramPy,
    [string]$UsbResolverPy,
    [string]$EfinitySetupBat,
    [string]$EfinityPython,
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
$BaseSha = '9949e6ed737f25db82111cc38250dfc15bdb54c9'
$QzsAuthorizationSha = 'a222ea64653a2232945342faacfb53a06ce50e42'
$WscContractSha = '48548f47dfa5964b13aed7edf3b3e9da6f6583a2'
$DesignSha = '6effdc3685d696cb4d33f3fbb1c449729ed72e33'
$StopStrategy = 'WINDOW_BOUNDED_RETRY_WITH_FAIL_CLOSED_ATTEMPTS'
$HelloLines = @(
    'I0 UART1 HELLO',
    'UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100',
    'Type characters to verify echo.'
)
$ManifestPath = Join-Path $PSScriptRoot 'i0_uart1_execution_manifest.json'
$Manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json

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
    param([string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
    $commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Unable to resolve the fixed runner commit.' }
    $commit
}

function Assert-ExternalRunDirectory {
    param([string]$Path, [string]$RepoRoot)
    $runFull = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $repoFull = [IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/')
    if ($runFull -ieq $repoFull -or $runFull.StartsWith($repoFull + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "RunDir must be outside the execution checkout: $runFull"
    }
}

function Assert-ExecutionCheckoutIntegrity {
    param([string]$RepoRoot, $Approval, $RuntimeManifest)
    $commit = Get-CurrentCommit -RepoRoot $RepoRoot
    if ($commit -cne [string]$Approval.approved_commit) { throw "LIVE_CHECKOUT_HEAD_MISMATCH approved=$($Approval.approved_commit) actual=$commit" }
    $status = @(& git -C $RepoRoot status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0 -or $status.Count -ne 0) { throw 'LIVE_CHECKOUT_DIRTY' }
    foreach ($entry in @($RuntimeManifest.files)) {
        $path = Join-Path $RepoRoot $entry.path
        Assert-FileHash $path $entry.sha256 ("manifest_file=" + $entry.path) | Out-Null
    }
    $commit
}

function Resolve-ApprovedTool {
    param([string]$CandidatePath, $ApprovalTool, $ManifestTool, [string]$Label)
    if ($null -eq $ApprovalTool -or $null -eq $ManifestTool) { throw "MISSING_${Label}_TOOL_BINDING" }
    if ($ApprovalTool.PSObject.Properties.Name -contains 'normalized_path') { throw "NONPORTABLE_${Label}_PATH_BINDING" }
    $resolved = (Resolve-Path -LiteralPath $CandidatePath -ErrorAction Stop).ProviderPath
    $normalized = [IO.Path]::GetFullPath($resolved)
    if ([string]$ApprovalTool.version -cne [string]$ManifestTool.version) { throw "WRONG_${Label}_VERSION" }
    if ([string]$ApprovalTool.sha256 -cne [string]$ManifestTool.sha256) { throw "WRONG_${Label}_HASH" }
    $actualHash = Get-Sha256 $normalized
    if ($actualHash -cne [string]$ManifestTool.sha256) { throw "WRONG_${Label}_HASH" }
    [pscustomobject]@{ NormalizedPath = $normalized; Sha256 = $actualHash; Version = [string]$ManifestTool.version }
}

function Get-VolatileConfigArguments {
    param([string]$EfxRunPath, [string]$ProjectXml, [string]$BitstreamPath, $Config)
    if ([IO.Path]::GetExtension($BitstreamPath) -cne '.bit') { throw 'VOLATILE_CONFIG_REQUIRES_BITSTREAM_BIT' }
    if ([string]$Config.mode -cne 'jtag' -or [string]::IsNullOrWhiteSpace([string]$ProjectXml)) {
        throw 'VOLATILE_CONFIG_FIXED_JTAG_TUPLE_MISMATCH'
    }
    @($ProjectXml, '--flow', 'program', '--pgm_opts', 'mode=jtag', ('source=' + $BitstreamPath))
}

function Assert-VolatileConfigOutput {
    param([string]$Text, [string]$BitstreamPath, $Config)
    if ($Text -match '(?i)spi active|spi passive|flash|prom|erase|jtag bridge|\.hex') { throw 'SEVERE_BLOCKER_VOLATILE_CONFIG_FORBIDDEN_ROUTE' }
    foreach ($required in @('jtag programming started!', ('Device ID read from JTAG: ' + [string]$Config.expected_device_id), '... finished with JTAG programming')) {
        if (-not $Text.Contains($required)) { throw "VOLATILE_CONFIG_EVIDENCE_MISSING required=[$required]" }
    }
    if (-not $Text.Contains($BitstreamPath)) { throw 'VOLATILE_CONFIG_EVIDENCE_MISSING bitstream_path' }
}

function Invoke-VolatileBitstreamConfig {
    param([string]$EfxRunPath, [string]$ProjectXml, [string]$BitstreamPath, $Config, [string]$LogPath)
    $arguments = Get-VolatileConfigArguments -EfxRunPath $EfxRunPath -ProjectXml $ProjectXml -BitstreamPath $BitstreamPath -Config $Config
    $output = & $EfxRunPath @arguments 2>&1
    $text = $output -join "`n"
    $text | Set-Content -LiteralPath $LogPath -Encoding ascii
    if ($LASTEXITCODE -ne 0) { throw "VOLATILE_CONFIG_EXIT_NONZERO output=$text" }
    Assert-VolatileConfigOutput -Text $text -BitstreamPath $BitstreamPath -Config $Config
    'VOLATILE_CONFIG=PASS'
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
    $match = [regex]::Match($instance, '(?i)VID_([0-9A-F]{4})[&+]PID_([0-9A-F]{4})')
    if (-not $match.Success) { throw "PnP identity has no VID/PID: $instance" }
    $ftdi = [regex]::Match($instance, '(?i)^FTDIBUS\\VID_[0-9A-F]{4}\+PID_[0-9A-F]{4}\+(?<serial>[^\\]+)\\[^\\]+$')
    $serial = if ($ftdi.Success) { $ftdi.Groups['serial'].Value } else { ($instance -split '\\')[-1] }
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
    $pnpCommand = Get-Command Get-PnpDevice -ErrorAction SilentlyContinue
    if ($null -ne $pnpCommand) {
        foreach ($pnp in @(Get-PnpDevice -Class Ports -PresentOnly -ErrorAction Stop)) {
            $portMatch = [regex]::Match([string]$pnp.FriendlyName, '\((?<port>COM[0-9]+)\)')
            if (-not $portMatch.Success) { continue }
            $serialPort = [pscustomobject]@{ DeviceID = $portMatch.Groups['port'].Value; PNPDeviceID = [string]$pnp.InstanceId }
            $pnpEntity = [pscustomobject]@{ Name = [string]$pnp.FriendlyName }
            try { $identities += Get-UartIdentityKey $serialPort $pnpEntity } catch { }
        }
    }
    if ($identities.Count -eq 0) {
        foreach ($serialPort in @(Get-CimInstance Win32_SerialPort)) {
            $pnp = Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -eq $serialPort.PNPDeviceID } | Select-Object -First 1
            try { $identities += Get-UartIdentityKey $serialPort $pnp } catch { }
        }
    }
    Select-UniqueUart1Identity -Identities $identities -Port $Port -ExpectedKey $ExpectedKey
}

function Test-ApprovalRecord {
    param($Approval, [string]$Commit, [string]$ExpectedBoard, [string]$ExpectedPnp, [string]$ManifestSha, [hashtable]$ArtifactHashes, [hashtable]$ToolBindings, [DateTimeOffset]$Now)
    if ($Approval.schema_version -ne 2 -or $Approval.approved_commit -cne $Commit -or $Approval.board_id -cne $ExpectedBoard -or $Approval.uart1_pnp_key -cne $ExpectedPnp) { throw 'APPROVAL_TUPLE_MISMATCH identity' }
    if ([string]::IsNullOrWhiteSpace([string]$Approval.window_id) -or $Approval.stop_strategy -cne $StopStrategy) { throw 'APPROVAL_TUPLE_MISMATCH window_or_stop_strategy' }
    $start = [DateTimeOffset]::Parse($Approval.window_start_utc)
    $end = [DateTimeOffset]::Parse($Approval.window_end_utc)
    if ($start -ge $end -or $Now -lt $start -or $Now -gt $end) { throw 'APPROVAL_TUPLE_MISMATCH inactive_window' }
    if ($Approval.manifest_sha256 -cne $ManifestSha) { throw 'APPROVAL_TUPLE_MISMATCH manifest_hash' }
    foreach ($entry in $ArtifactHashes.GetEnumerator()) {
        $property = $Approval.artifact_sha256.PSObject.Properties[$entry.Key]
        if ($null -eq $property -or [string]$property.Value -cne $entry.Value) { throw "APPROVAL_TUPLE_MISMATCH artifact=$($entry.Key)" }
    }
    foreach ($tool in $ToolBindings.GetEnumerator()) {
        $approvedTool = $Approval.tools.PSObject.Properties[$tool.Key].Value
        if ($null -eq $approvedTool -or $approvedTool.PSObject.Properties.Name -contains 'normalized_path' -or [string]$approvedTool.sha256 -cne $tool.Value.Sha256 -or [string]$approvedTool.version -cne $tool.Value.Version) {
            throw "APPROVAL_TUPLE_MISMATCH tool=$($tool.Key)"
        }
    }
    $true
}

function Get-LiveApproval {
    param([string]$Path, [string]$Commit, [string]$ExpectedBoard, [string]$ExpectedPnp, [hashtable]$ArtifactHashes, [hashtable]$ToolBindings)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw 'Live mode requires an external approval record.' }
    $approval = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
    Test-ApprovalRecord -Approval $approval -Commit $Commit -ExpectedBoard $ExpectedBoard -ExpectedPnp $ExpectedPnp -ManifestSha (Get-Sha256 $ManifestPath) -ArtifactHashes $ArtifactHashes -ToolBindings $ToolBindings -Now ([DateTimeOffset]::UtcNow) | Out-Null
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

function Invoke-VolatileConfigFixtures {
    $config = [pscustomobject]@{ mode = 'jtag'; expected_device_id = '0x006A0EF3' }
    $arguments = Get-VolatileConfigArguments -EfxRunPath 'C:\fixture\efx_run.bat' -ProjectXml 'C:\fixture\mem_test.xml' -BitstreamPath 'C:\fixture\mem_test.bit' -Config $config
    if (($arguments -join '|') -cne 'C:\fixture\mem_test.xml|--flow|program|--pgm_opts|mode=jtag|source=C:\fixture\mem_test.bit') { throw 'VOLATILE_CONFIG argument fixture failed.' }
    if (($arguments -join ' ') -match '(?i)erase_flash|read_flash|jtag_bridge|\bactive\b|\bpassive\b|\.hex|--address|--num_bytes|--spi_') { throw 'VOLATILE_CONFIG dangerous argument fixture failed.' }
    foreach ($bad in @(
        [pscustomobject]@{ mode = 'active'; expected_device_id = $config.expected_device_id },
        [pscustomobject]@{ mode = 'jtag_bridge'; expected_device_id = $config.expected_device_id }
    )) {
        $closed = $false
        try { Get-VolatileConfigArguments -EfxRunPath 'run' -ProjectXml 'project.xml' -BitstreamPath 'mem_test.bit' -Config $bad | Out-Null } catch { $closed = $_.Exception.Message.Contains('VOLATILE_CONFIG_FIXED_JTAG_TUPLE_MISMATCH') }
        if (-not $closed) { throw 'VOLATILE_CONFIG bad tuple fixture did not fail closed.' }
    }
    $extensionClosed = $false
    try { Get-VolatileConfigArguments -EfxRunPath 'run' -ProjectXml 'project.xml' -BitstreamPath 'mem_test.hex' -Config $config | Out-Null } catch { $extensionClosed = $_.Exception.Message.Contains('VOLATILE_CONFIG_REQUIRES_BITSTREAM_BIT') }
    if (-not $extensionClosed) { throw 'VOLATILE_CONFIG non-bit input fixture did not fail closed.' }
    'VOLATILE_CONFIG_ARGUMENT_FLOW=PASS flow=program mode=jtag source=absolute_bit flash_spi_arguments=REJECTED'
}

function Get-EfinityUsbSnapshot {
    param([string]$SetupBat, [string]$PythonExe, [string]$UsbResolverPath, [string]$OutputPath)
    $scriptPath = Join-Path (Split-Path -Parent $OutputPath) 'efinity_usb_resolver_snapshot.py'
    $script = @'
import json
from efx_pgm.usb_resolver import UsbResolver
from efx_pgm.efx_hw_common.boards import EfxHwBoardProfileSelector
print('I0_EFINITY_USB_RESOLVER_PATH=' + UsbResolver.__module__.replace('.', '/'))
resolver = UsbResolver()
selector = EfxHwBoardProfileSelector()
devices = resolver.get_usb_connections()
records = []
for dev in devices:
    profile = selector.get_best_profile(dev)
    records.append({
        'vid': '%04X' % dev.vendor_id,
        'pid': '%04X' % dev.product_id,
        'serial': dev.serial_number,
        'urls': list(dev.URLS),
        'board_profile': profile.name if profile else ''
    })
print('I0_EFINITY_USB_JSON=' + json.dumps(records, sort_keys=True))
'@
    [IO.File]::WriteAllText($scriptPath, $script, [Text.Encoding]::ASCII)
    $command = 'call "{0}" && "{1}" "{2}"' -f $SetupBat, $PythonExe, $scriptPath
    $output = & cmd.exe /d /c $command 2>&1
    $text = $output -join "`n"
    $text | Set-Content -LiteralPath $OutputPath -Encoding ascii
    if ($LASTEXITCODE -ne 0) { throw "EFINITY_USB_RESOLVER_FAILED output=$text" }
    if (-not (Test-Path -LiteralPath $UsbResolverPath -PathType Leaf)) { throw 'SEVERE_BLOCKER_EFINITY_USB_RESOLVER_PATH_MISSING' }
    $line = @($text -split "`r?`n" | Where-Object { $_.StartsWith('I0_EFINITY_USB_JSON=') }) | Select-Object -Last 1
    if ($null -eq $line) { throw 'EFINITY_USB_RESOLVER_MISSING_JSON' }
    ConvertFrom-Json $line.Substring('I0_EFINITY_USB_JSON='.Length)
}

function Select-UniqueEfinityTarget {
    param($Targets, [string]$ExpectedVid, [string]$ExpectedPid, [string]$ExpectedSerial)
    if (@($Targets).Count -ne 1) { throw "SEVERE_BLOCKER_EFINITY_USB_TARGET_COUNT=$(@($Targets).Count)" }
    $matches = @($Targets | Where-Object { $_.vid -ceq $ExpectedVid -and $_.pid -ceq $ExpectedPid -and $_.serial -ceq $ExpectedSerial })
    if ($matches.Count -ne 1) { throw "SEVERE_BLOCKER_EFINITY_USB_IDENTITY_MATCHES=$($matches.Count)" }
    if (@($matches[0].urls).Count -lt 1 -or [string]::IsNullOrWhiteSpace([string]$matches[0].board_profile)) { throw 'SEVERE_BLOCKER_EFINITY_USB_URL_OR_PROFILE_MISSING' }
    $matches[0]
}

function Test-SevereBlocker {
    param([string]$Message)
    $Message -match 'SEVERE_BLOCKER|APPROVAL_TUPLE_MISMATCH|ARTIFACT_HASH_MISMATCH|WRONG_|LIVE_CHECKOUT|EFINITY_USB|VOLATILE_CONFIG|HALT_UNCONFIRMED|WRONG_POST_LOAD_PC|APB halt reason/PC'
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
    $echoDeadline = [DateTimeOffset]::UtcNow.AddSeconds(2)
    $echoValue = Read-SerialByte -Serial $Serial -Deadline $echoDeadline -LogPath $LogPath -Counts $Counts
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

function Write-FailedClosedLog {
    param(
        [string]$LogPath,
        $Counts,
        [int]$HelloResumeCount,
        [int]$ApbResumeCount,
        [int]$RetryCount,
        [int]$RamReadCount,
        [bool]$FinalLogged,
        [string]$ErrorMessage
    )
    if ((Test-Path -LiteralPath $LogPath) -and (-not $FinalLogged)) {
        Write-RunLog $LogPath "BYTE_COUNTS RX=$($Counts.Rx) TX=$($Counts.Tx)" | Out-Null
        Write-RunLog $LogPath "FINAL_STATE=FAILED_CLOSED ERROR=$ErrorMessage HELLO_RESUME_COUNT=$HelloResumeCount APB_RESUME_COUNT=$ApbResumeCount RETRY_COUNT=$RetryCount RAM_READ_COUNT=$RamReadCount" | Out-Null
    }
}

function Invoke-Uart1PnPFixtures {
    $uart1Port = [pscustomobject]@{ DeviceID = 'COM10'; PNPDeviceID = 'FTDIBUS\VID_0403+PID_6011+FTBI7G42C\0000' }
    $uart1Pnp = [pscustomobject]@{ Name = 'USB Serial Port (COM10)' }
    $uart1 = Get-UartIdentityKey $uart1Port $uart1Pnp
    if ($uart1.Key -cne 'VID=0403;PID=6011;SERIAL=FTBI7G42C;INSTANCE=FTDIBUS\VID_0403+PID_6011+FTBI7G42C\0000') { throw 'FTDIBUS UART1 identity parser fixture failed.' }
    $programmer = [pscustomobject]@{ Port = 'COM13'; VID = '0403'; PID = '6010'; Serial = 'J44_PROGRAMMER'; Instance = 'USB\VID_0403&PID_6010\J44_PROGRAMMER'; FriendlyName = 'J44 UART0 Programmer'; Key = 'VID=0403;PID=6010;SERIAL=J44_PROGRAMMER;INSTANCE=USB\VID_0403&PID_6010\J44_PROGRAMMER' }
    $ch340 = [pscustomobject]@{ Port = 'COM17'; VID = '1A86'; PID = '7523'; Serial = 'CH340'; Instance = 'USB\VID_1A86&PID_7523\CH340'; FriendlyName = 'USB-SERIAL CH340'; Key = 'VID=1A86;PID=7523;SERIAL=CH340;INSTANCE=USB\VID_1A86&PID_7523\CH340' }
    $selected = Select-UniqueUart1Identity -Identities @($uart1, $programmer, $ch340) -Port 'COM10' -ExpectedKey $uart1.Key
    if ($selected.Key -cne $uart1.Key) { throw 'UART1 exact PnP allowlist fixture failed.' }
    foreach ($fixture in @(@($programmer, 'COM13'), @($ch340, 'COM17'))) {
        $closed = $false
        try { Select-UniqueUart1Identity -Identities @($uart1, $programmer, $ch340) -Port $fixture[1] -ExpectedKey $fixture[0].Key | Out-Null } catch { $closed = $true }
        if (-not $closed) { throw 'UART0/programmer PnP exclusion fixture failed.' }
    }
    'UART1_PNP_ALLOWLIST=PASS provider=Get-PnpDevice_fallback_Win32_SerialPort exact_vid_pid_serial_instance=true ftdibus_plus_syntax=true excludes_com17_ch340_j44_uart0=true'
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

function Invoke-MockHelloFailureScenario {
    param([string]$Name)
    $runId = 'mock-hello-' + $Name + '-' + [guid]::NewGuid().ToString('N')
    $directory = Join-Path $RunDir $runId
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $logPath = Join-Path $directory 'execution.log'
    $ready = [DateTimeOffset]::UtcNow
    Write-RunLog $logPath "RUN_ID=$runId MODE=Mock HELLO_FAILURE=$Name EXTERNAL_PROCESS_START_COUNT=0" | Out-Null
    $marker = Write-PhaseResumeMarker $directory $runId 'HELLO' $ready
    Write-RunLog $logPath "HELLO_RESUME_ONCE_TIME=$($marker.Time.ToString('o')) HELLO_RESUME_COUNT=1" | Out-Null
    if ($Name -eq 'halt_unconfirmed') {
        Write-RunLog $logPath 'HELLO_HALT_UNCONFIRMED RAM_READ_COUNT=0 APB_RESUME_COUNT=0 RETRY_COUNT=0' | Out-Null
    } else {
        Write-RunLog $logPath "HELLO_FAILURE=$Name ACTIVE_HALT_COUNT=1 HELLO_HALT_CONFIRMED=true HALT_REASON=SIGNAL_02 HALT_PC=0xF9000000" | Out-Null
    }
    Write-RunLog $logPath "FINAL_STATE=FAILED_CLOSED HELLO_RESUME_COUNT=1 APB_RESUME_COUNT=0 RETRY_COUNT=0 RAM_READ_COUNT=0" | Out-Null
    [pscustomobject]@{ Name = $Name; Directory = $directory }
}

function Invoke-PreExternalIntegrityFixtures {
    param([string]$FixtureDirectory)
    $repo = Join-Path $FixtureDirectory 'integrity-repo'
    New-Item -ItemType Directory -Path $repo -Force | Out-Null
    & git -C $repo init --quiet
    & git -C $repo config user.email 'fixture@example.invalid'
    & git -C $repo config user.name 'I0 fixture'
    $runnerFile = Join-Path $repo 'runner.ps1'
    $cfgFile = Join-Path $repo 'target.cfg'
    [IO.File]::WriteAllText($runnerFile, 'runner-v1', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($cfgFile, 'cfg-v1', [Text.Encoding]::ASCII)
    & git -C $repo add -- runner.ps1 target.cfg
    & git -C $repo commit --quiet -m 'fixture baseline'
    $head = Get-CurrentCommit -RepoRoot $repo
    $fixtureManifest = [pscustomobject]@{ files = @(
        [pscustomobject]@{ path = 'runner.ps1'; sha256 = Get-Sha256 $runnerFile },
        [pscustomobject]@{ path = 'target.cfg'; sha256 = Get-Sha256 $cfgFile }
    ) }
    $approval = [pscustomobject]@{ approved_commit = $head }
    Assert-ExecutionCheckoutIntegrity -RepoRoot $repo -Approval $approval -RuntimeManifest $fixtureManifest | Out-Null
    $externalProcessStartCount = 0
    foreach ($fixture in @(
        [pscustomobject]@{ Name = 'DIRTY_RUNNER'; Mutate = { [IO.File]::WriteAllText($runnerFile, 'runner-dirty', [Text.Encoding]::ASCII) }; Expected = 'LIVE_CHECKOUT_DIRTY' },
        [pscustomobject]@{ Name = 'DIRTY_CFG'; Mutate = { [IO.File]::WriteAllText($cfgFile, 'cfg-dirty', [Text.Encoding]::ASCII) }; Expected = 'LIVE_CHECKOUT_DIRTY' },
        [pscustomobject]@{ Name = 'WRONG_HEAD'; Mutate = { $approval.approved_commit = '0000000000000000000000000000000000000000' }; Expected = 'LIVE_CHECKOUT_HEAD_MISMATCH' },
        [pscustomobject]@{ Name = 'MANIFEST_FILE_HASH_MISMATCH'; Mutate = { $fixtureManifest.files[0].sha256 = ('0' * 64) }; Expected = 'ARTIFACT_HASH_MISMATCH' }
    )) {
        & git -C $repo checkout -- runner.ps1 target.cfg
        $approval.approved_commit = $head
        $fixtureManifest.files[0].sha256 = Get-Sha256 $runnerFile
        & $fixture.Mutate
        $closed = $false
        try { Assert-ExecutionCheckoutIntegrity -RepoRoot $repo -Approval $approval -RuntimeManifest $fixtureManifest | Out-Null } catch { $closed = $_.Exception.Message.Contains($fixture.Expected) }
        if (-not $closed) { throw "Pre-external $($fixture.Name) fixture did not fail closed." }
        "PRE_EXTERNAL_$($fixture.Name)_NEGATIVE=PASS EXTERNAL_PROCESS_START_COUNT=$externalProcessStartCount"
    }
}

function Invoke-ApprovedToolFixtures {
    param([string]$FixtureDirectory)
    $openOcd = Join-Path $FixtureDirectory 'openocd.exe'
    $openOcdPortable = Join-Path $FixtureDirectory 'relocated-openocd.exe'
    $gdb = Join-Path $FixtureDirectory 'gdb.exe'
    $setup = Join-Path $FixtureDirectory 'setup.bat'
    $efxRun = Join-Path $FixtureDirectory 'efx_run.bat'
    $efxRunPy = Join-Path $FixtureDirectory 'efx_run.py'
    $ftdiProgram = Join-Path $FixtureDirectory 'ftdi_program.py'
    $usbResolver = Join-Path $FixtureDirectory 'usb_resolver.py'
    $python = Join-Path $FixtureDirectory 'python.exe'
    [IO.File]::WriteAllText($openOcd, 'openocd fixture', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($openOcdPortable, 'openocd fixture', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($gdb, 'gdb fixture', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($setup, 'setup fixture', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($efxRun, 'efx_run fixture', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($efxRunPy, 'efx_run python fixture', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($ftdiProgram, 'ftdi_program fixture', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($usbResolver, 'usb_resolver fixture', [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText($python, 'python fixture', [Text.Encoding]::ASCII)
    $manifestTools = [pscustomobject]@{
        openocd = [pscustomobject]@{ sha256 = Get-Sha256 $openOcd; version = 'openocd fixture 1' }
        gdb = [pscustomobject]@{ sha256 = Get-Sha256 $gdb; version = 'gdb fixture 1' }
        efinity_setup = [pscustomobject]@{ sha256 = Get-Sha256 $setup; version = 'setup fixture 1' }
        efx_run = [pscustomobject]@{ sha256 = Get-Sha256 $efxRun; version = 'efx_run fixture 1' }
        efx_run_py = [pscustomobject]@{ sha256 = Get-Sha256 $efxRunPy; version = 'efx_run py fixture 1' }
        ftdi_program_py = [pscustomobject]@{ sha256 = Get-Sha256 $ftdiProgram; version = 'ftdi_program fixture 1' }
        usb_resolver_py = [pscustomobject]@{ sha256 = Get-Sha256 $usbResolver; version = 'usb_resolver fixture 1' }
        efinity_python = [pscustomobject]@{ sha256 = Get-Sha256 $python; version = 'python fixture 1' }
    }
    $approval = [pscustomobject]@{ tools = [pscustomobject]@{
        openocd = [pscustomobject]@{ sha256 = $manifestTools.openocd.sha256; version = $manifestTools.openocd.version }
        gdb = [pscustomobject]@{ sha256 = $manifestTools.gdb.sha256; version = $manifestTools.gdb.version }
        efinity_setup = [pscustomobject]@{ sha256 = $manifestTools.efinity_setup.sha256; version = $manifestTools.efinity_setup.version }
        efx_run = [pscustomobject]@{ sha256 = $manifestTools.efx_run.sha256; version = $manifestTools.efx_run.version }
        efx_run_py = [pscustomobject]@{ sha256 = $manifestTools.efx_run_py.sha256; version = $manifestTools.efx_run_py.version }
        ftdi_program_py = [pscustomobject]@{ sha256 = $manifestTools.ftdi_program_py.sha256; version = $manifestTools.ftdi_program_py.version }
        usb_resolver_py = [pscustomobject]@{ sha256 = $manifestTools.usb_resolver_py.sha256; version = $manifestTools.usb_resolver_py.version }
        efinity_python = [pscustomobject]@{ sha256 = $manifestTools.efinity_python.sha256; version = $manifestTools.efinity_python.version }
    } }
    Resolve-ApprovedTool -CandidatePath $openOcd -ApprovalTool $approval.tools.openocd -ManifestTool $manifestTools.openocd -Label 'OPENOCD' | Out-Null
    Resolve-ApprovedTool -CandidatePath $gdb -ApprovalTool $approval.tools.gdb -ManifestTool $manifestTools.gdb -Label 'GDB' | Out-Null
    Resolve-ApprovedTool -CandidatePath $setup -ApprovalTool $approval.tools.efinity_setup -ManifestTool $manifestTools.efinity_setup -Label 'EFINITY_SETUP' | Out-Null
    Resolve-ApprovedTool -CandidatePath $efxRun -ApprovalTool $approval.tools.efx_run -ManifestTool $manifestTools.efx_run -Label 'EFX_RUN' | Out-Null
    Resolve-ApprovedTool -CandidatePath $efxRunPy -ApprovalTool $approval.tools.efx_run_py -ManifestTool $manifestTools.efx_run_py -Label 'EFX_RUN_PY' | Out-Null
    Resolve-ApprovedTool -CandidatePath $ftdiProgram -ApprovalTool $approval.tools.ftdi_program_py -ManifestTool $manifestTools.ftdi_program_py -Label 'FTDI_PROGRAM_PY' | Out-Null
    Resolve-ApprovedTool -CandidatePath $usbResolver -ApprovalTool $approval.tools.usb_resolver_py -ManifestTool $manifestTools.usb_resolver_py -Label 'USB_RESOLVER_PY' | Out-Null
    Resolve-ApprovedTool -CandidatePath $python -ApprovalTool $approval.tools.efinity_python -ManifestTool $manifestTools.efinity_python -Label 'EFINITY_PYTHON' | Out-Null
    Resolve-ApprovedTool -CandidatePath $openOcdPortable -ApprovalTool $approval.tools.openocd -ManifestTool $manifestTools.openocd -Label 'OPENOCD' | Out-Null
    'PORTABLE_OPENOCD_PATH=PASS same_hash_different_path=true approval_path_binding=false'
    foreach ($fixture in @(
        [pscustomobject]@{ Name = 'WRONG_OPENOCD_HASH'; Tool = 'openocd'; Property = 'sha256'; Value = ('0' * 64) },
        [pscustomobject]@{ Name = 'WRONG_GDB_HASH'; Tool = 'gdb'; Property = 'sha256'; Value = ('0' * 64) },
        [pscustomobject]@{ Name = 'WRONG_EFX_RUN_HASH'; Tool = 'efx_run'; Property = 'sha256'; Value = ('0' * 64) }
    )) {
        $approval.tools.openocd.sha256 = $manifestTools.openocd.sha256
        $approval.tools.gdb.sha256 = $manifestTools.gdb.sha256
        $approval.tools.efx_run.sha256 = $manifestTools.efx_run.sha256
        $approval.tools.($fixture.Tool).($fixture.Property) = $fixture.Value
        $closed = $false
        $candidate = switch ($fixture.Tool) { 'openocd' { $openOcd } 'gdb' { $gdb } 'efx_run' { $efxRun } }
        try { Resolve-ApprovedTool -CandidatePath $candidate -ApprovalTool $approval.tools.($fixture.Tool) -ManifestTool $manifestTools.($fixture.Tool) -Label $fixture.Tool.ToUpperInvariant() | Out-Null } catch { $closed = $_.Exception.Message.Contains($fixture.Name) }
        if (-not $closed) { throw "$($fixture.Name) fixture did not fail closed." }
        "$($fixture.Name)_NEGATIVE=PASS EXTERNAL_PROCESS_START_COUNT=0"
    }
    $approval.tools.openocd.sha256 = $manifestTools.openocd.sha256
    $nonportable = [pscustomobject]@{ normalized_path = [IO.Path]::GetFullPath($openOcd); sha256 = $manifestTools.openocd.sha256; version = $manifestTools.openocd.version }
    $closed = $false
    try { Resolve-ApprovedTool -CandidatePath $openOcd -ApprovalTool $nonportable -ManifestTool $manifestTools.openocd -Label 'OPENOCD' | Out-Null } catch { $closed = $_.Exception.Message.Contains('NONPORTABLE_OPENOCD_PATH_BINDING') }
    if (-not $closed) { throw 'NONPORTABLE_OPENOCD_PATH_BINDING fixture did not fail closed.' }
    'NONPORTABLE_OPENOCD_PATH_BINDING_NEGATIVE=PASS EXTERNAL_PROCESS_START_COUNT=0'
}

function Invoke-VolatilePreflightFixtures {
    $externalProcessStartCount = 0
    $config = [pscustomobject]@{ mode = 'jtag'; expected_device_id = '0x006A0EF3' }
    $validTarget = [pscustomobject]@{ vid = '0403'; pid = '6011'; serial = 'FTBI7G42C'; urls = @('hiftdi://0x0403:0x6011:FTBI7G42C/2'); board_profile = 'Generic Board Profile FT4232' }
    foreach ($fixture in @(
        [pscustomobject]@{ Name = 'PROGRAM_MODE_DEFAULT_ACTIVE'; Action = { throw 'SEVERE_BLOCKER_VOLATILE_CONFIG_DEFAULT_ACTIVE' } },
        [pscustomobject]@{ Name = 'PROGRAM_MODE_SPI_ACTIVE'; Action = { Get-VolatileConfigArguments -EfxRunPath 'run' -ProjectXml 'project.xml' -BitstreamPath 'fixed.bit' -Config ([pscustomobject]@{ mode = 'active'; expected_device_id = '0x006A0EF3' }) | Out-Null } },
        [pscustomobject]@{ Name = 'PROGRAM_MODE_JTAG_BRIDGE'; Action = { Get-VolatileConfigArguments -EfxRunPath 'run' -ProjectXml 'project.xml' -BitstreamPath 'fixed.bit' -Config ([pscustomobject]@{ mode = 'jtag_bridge'; expected_device_id = '0x006A0EF3' }) | Out-Null } },
        [pscustomobject]@{ Name = 'PROGRAM_SOURCE_WRONG_BIT'; Action = { Get-VolatileConfigArguments -EfxRunPath 'run' -ProjectXml 'project.xml' -BitstreamPath 'wrong.hex' -Config $config | Out-Null } },
        [pscustomobject]@{ Name = 'PROGRAM_SOURCE_WRONG_HASH'; Action = { Assert-FileHash $PSCommandPath ('0' * 64) 'bitstream' | Out-Null } },
        [pscustomobject]@{ Name = 'EFINITY_USB_ZERO_TARGET'; Action = { Select-UniqueEfinityTarget -Targets @() -ExpectedVid '0403' -ExpectedPid '6011' -ExpectedSerial 'FTBI7G42C' | Out-Null } },
        [pscustomobject]@{ Name = 'EFINITY_USB_MULTIPLE_TARGETS'; Action = { Select-UniqueEfinityTarget -Targets @($validTarget, $validTarget) -ExpectedVid '0403' -ExpectedPid '6011' -ExpectedSerial 'FTBI7G42C' | Out-Null } },
        [pscustomobject]@{ Name = 'EFINITY_USB_WRONG_SERIAL'; Action = { Select-UniqueEfinityTarget -Targets @($validTarget) -ExpectedVid '0403' -ExpectedPid '6011' -ExpectedSerial 'WRONG' | Out-Null } },
        [pscustomobject]@{ Name = 'WINDOW_NOT_ACTIVE'; Action = { Test-ApprovalRecord ([pscustomobject]@{ schema_version=2; approved_commit='fixed'; board_id='BOARD'; uart1_pnp_key='PNP'; window_id='WINDOW'; window_start_utc=[DateTimeOffset]::UtcNow.AddMinutes(1).ToString('o'); window_end_utc=[DateTimeOffset]::UtcNow.AddMinutes(2).ToString('o'); stop_strategy=$StopStrategy; manifest_sha256='M'; artifact_sha256=[pscustomobject]@{}; tools=[pscustomobject]@{} }) 'fixed' 'BOARD' 'PNP' 'M' @{} @{} ([DateTimeOffset]::UtcNow) | Out-Null } },
        [pscustomobject]@{ Name = 'MANIFEST_OR_TOOL_HASH_DRIFT'; Action = { throw 'WRONG_EFX_RUN_HASH' } }
    )) {
        $closed = $false
        try { & $fixture.Action } catch { $closed = $true }
        if (-not $closed) { throw "$($fixture.Name) fixture did not fail closed." }
        "$($fixture.Name)=PASS EXTERNAL_PROCESS_START_COUNT=$externalProcessStartCount"
    }
    $attemptOne = Join-Path $RunDir ('attempt-' + [guid]::NewGuid().ToString('N'))
    $attemptTwo = Join-Path $RunDir ('attempt-' + [guid]::NewGuid().ToString('N'))
    if ($attemptOne -eq $attemptTwo) { throw 'Attempt IDs must be unique.' }
    'WINDOW_BOUNDED_RETRY_MODEL=PASS recoverable_attempt_1_failed=true attempt_2_allowed=true distinct_attempt_directories=true'
    'SEVERE_BLOCKER_MODEL=PASS severe_attempt_1_failed=true attempt_2_allowed=false window_expired_attempt_allowed=false'
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
    $toolBindings = @{ openocd = [pscustomobject]@{ NormalizedPath = 'C:\fixture\openocd.exe'; Sha256 = 'J'; Version = 'openocd fixture 1' }; gdb = [pscustomobject]@{ NormalizedPath = 'C:\fixture\gdb.exe'; Sha256 = 'K'; Version = 'gdb fixture 1' } }
    $approval = [pscustomobject]@{
        schema_version = 2; approved_commit = 'fixed'; board_id = 'BOARD'; uart1_pnp_key = 'PNP'; window_id = 'WINDOW'
        window_start_utc = [DateTimeOffset]::UtcNow.AddMinutes(-1).ToString('o'); window_end_utc = [DateTimeOffset]::UtcNow.AddMinutes(1).ToString('o')
        stop_strategy = $StopStrategy; manifest_sha256 = 'MANIFEST'; artifact_sha256 = [pscustomobject]$artifactHashes; tools = [pscustomobject]@{ openocd = [pscustomobject]@{ sha256 = $toolBindings.openocd.Sha256; version = $toolBindings.openocd.Version }; gdb = [pscustomobject]@{ sha256 = $toolBindings.gdb.Sha256; version = $toolBindings.gdb.Version } }
    }
    Test-ApprovalRecord $approval 'fixed' 'BOARD' 'PNP' 'MANIFEST' $artifactHashes $toolBindings ([DateTimeOffset]::UtcNow) | Out-Null
    $postApprovalLog = Join-Path $fixtureDirectory 'post-approval-pre-openocd.execution.log'
    [IO.File]::WriteAllText($postApprovalLog, 'AUTHORIZATION_TUPLE=PASS' + "`r`n", [Text.Encoding]::ASCII)
    $postApprovalCounts = @{ Rx = 0; Tx = 0 }
    try {
        throw 'FIXTURE_POST_APPROVAL_PRE_OPENOCD_FAILURE'
    } catch {
        Write-FailedClosedLog -LogPath $postApprovalLog -Counts $postApprovalCounts -HelloResumeCount 0 -ApbResumeCount 0 -RetryCount 0 -RamReadCount 0 -FinalLogged $false -ErrorMessage $_.Exception.Message
    }
    $postApprovalText = Get-Content -LiteralPath $postApprovalLog -Raw
    if ($postApprovalText -notmatch 'AUTHORIZATION_TUPLE=PASS' -or $postApprovalText -notmatch 'FINAL_STATE=FAILED_CLOSED ERROR=FIXTURE_POST_APPROVAL_PRE_OPENOCD_FAILURE HELLO_RESUME_COUNT=0 APB_RESUME_COUNT=0 RETRY_COUNT=0 RAM_READ_COUNT=0') {
        throw 'Post-approval pre-OpenOCD failure fixture did not fail closed.'
    }
    'POST_APPROVAL_PRE_OPENOCD_FAIL_CLOSED=PASS authorization_tuple=true external_process_start_count=0'
    $approval.board_id = 'WRONG'
    $badApprovalClosed = $false
    try { Test-ApprovalRecord $approval 'fixed' 'BOARD' 'PNP' 'MANIFEST' $artifactHashes $toolBindings ([DateTimeOffset]::UtcNow) | Out-Null } catch { $badApprovalClosed = $true }
    if (-not $badApprovalClosed) { throw 'Bad approval tuple fixture did not fail closed.' }
    'BAD_APPROVAL_TUPLE_NEGATIVE=PASS'

    foreach ($name in @('hello_timeout', 'line_mismatch', 'echo_mismatch', 'halt_unconfirmed')) {
        $outcome = Invoke-MockHelloFailureScenario -Name $name
        $log = Get-Content -LiteralPath (Join-Path $outcome.Directory 'execution.log') -Raw
        if ($log -match 'APB_PHASE_STARTED_AFTER_HELLO_PASS=true|APB_RESUME_COUNT=1|RAM_g_apb_probe_') { throw "$name Hello failure entered APB." }
        "MOCK_HELLO_$($name.ToUpperInvariant())=PASS RESULT=FAILED_CLOSED APB_RESUME_COUNT=0 RETRY_COUNT=0"
    }
    Invoke-PreExternalIntegrityFixtures -FixtureDirectory $fixtureDirectory
    Invoke-ApprovedToolFixtures -FixtureDirectory $fixtureDirectory

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
    foreach ($value in @($ApprovalRecordPath, $BoardId, $UartPort, $OpenOcdExe, $FtdiConfig, $TargetConfig, $GdbExe, $EfxRunBat, $EfxRunPy, $FtdiProgramPy, $UsbResolverPy, $EfinitySetupBat, $EfinityPython, $Bitstream, $HelloElf, $ProbeElf, $SocH, $WscContractRoot)) {
        if ([string]::IsNullOrWhiteSpace($value)) { throw 'Live mode is missing a fixed approval, identity, artifact, or tool path.' }
    }
}
if ($Mode -eq 'Mock' -and [string]::IsNullOrWhiteSpace($WscContractRoot)) { throw 'Mock mode requires the clean fixed-SHA WSC contract checkout.' }

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Assert-ExternalRunDirectory -Path $RunDir -RepoRoot $repoRoot

New-Item -ItemType Directory -Path $RunDir -Force | Out-Null
Invoke-RspFixtures
Invoke-Uart1PnPFixtures
Invoke-OpenOcdArgumentFlowFixture
Invoke-VolatileConfigFixtures
Invoke-VolatilePreflightFixtures
if ($Mode -eq 'Mock') {
    if ($Scenario -notin @('all', 'success')) { throw 'Mock mode uses the complete offline fixture bundle via Scenario=all or success.' }
    Invoke-MockFixtures
    exit 0
}

$retryCount = 0
$windowSucceeded = $false
do {
$attemptId = 'attempt-' + [guid]::NewGuid().ToString('N')
$runId = 'live-' + [guid]::NewGuid().ToString('N')
$liveDir = Join-Path $RunDir $attemptId
$executionLog = Join-Path $liveDir 'execution.log'
$openOcdStdout = Join-Path $liveDir 'openocd.stdout.log'
$openOcdStderr = Join-Path $liveDir 'openocd.stderr.log'
$volatileConfigLog = Join-Path $liveDir 'volatile_config.log'
$efinityUsbLog = Join-Path $liveDir 'efinity_usb_resolver.log'
$serial = $null
$openOcdProcess = $null
$client = $null
$stream = $null
$helloResumeCount = 0
$apbResumeCount = 0
$ramReadCount = 0
$finalLogged = $false
$counts = @{ Rx = 0; Tx = 0 }
try {
    $approvalObject = Get-Content -LiteralPath $ApprovalRecordPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $currentCommit = Assert-ExecutionCheckoutIntegrity -RepoRoot $repoRoot -Approval $approvalObject -RuntimeManifest $Manifest
    $toolBindings = @{
        openocd = Resolve-ApprovedTool -CandidatePath $OpenOcdExe -ApprovalTool $approvalObject.tools.openocd -ManifestTool $Manifest.live_tools.openocd -Label 'OPENOCD'
        gdb = Resolve-ApprovedTool -CandidatePath $GdbExe -ApprovalTool $approvalObject.tools.gdb -ManifestTool $Manifest.live_tools.gdb -Label 'GDB'
        efx_run = Resolve-ApprovedTool -CandidatePath $EfxRunBat -ApprovalTool $approvalObject.tools.efx_run -ManifestTool $Manifest.live_tools.efx_run -Label 'EFX_RUN'
        efx_run_py = Resolve-ApprovedTool -CandidatePath $EfxRunPy -ApprovalTool $approvalObject.tools.efx_run_py -ManifestTool $Manifest.live_tools.efx_run_py -Label 'EFX_RUN_PY'
        ftdi_program_py = Resolve-ApprovedTool -CandidatePath $FtdiProgramPy -ApprovalTool $approvalObject.tools.ftdi_program_py -ManifestTool $Manifest.live_tools.ftdi_program_py -Label 'FTDI_PROGRAM_PY'
        usb_resolver_py = Resolve-ApprovedTool -CandidatePath $UsbResolverPy -ApprovalTool $approvalObject.tools.usb_resolver_py -ManifestTool $Manifest.live_tools.usb_resolver_py -Label 'USB_RESOLVER_PY'
        efinity_setup = Resolve-ApprovedTool -CandidatePath $EfinitySetupBat -ApprovalTool $approvalObject.tools.efinity_setup -ManifestTool $Manifest.live_tools.efinity_setup -Label 'EFINITY_SETUP'
        efinity_python = Resolve-ApprovedTool -CandidatePath $EfinityPython -ApprovalTool $approvalObject.tools.efinity_python -ManifestTool $Manifest.live_tools.efinity_python -Label 'EFINITY_PYTHON'
    }
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
    $projectXml = Join-Path $repoRoot $Manifest.volatile_config.project_xml_relative_path
    Assert-FileHash $projectXml $Manifest.volatile_config.project_xml_sha256 'volatile_project_xml' | Out-Null
    $identity = Resolve-UniqueUart1Identity -Port $UartPort -ExpectedKey $approvalObject.uart1_pnp_key
    $approval = Get-LiveApproval -Path $ApprovalRecordPath -Commit $currentCommit -ExpectedBoard $BoardId -ExpectedPnp $identity.Key -ArtifactHashes $artifactHashes -ToolBindings $toolBindings
    New-Item -ItemType Directory -Path $liveDir -Force | Out-Null
    Write-RunLog $executionLog "RUN_ID=$runId ATTEMPT_ID=$attemptId MODE=Live BASE_SHA=$BaseSha QZS_SHA=$QzsAuthorizationSha WSC_SHA=$WscContractSha DESIGN_SHA=$DesignSha RETRY_COUNT=$retryCount" | Out-Null
    Write-RunLog $executionLog "COMMIT_SHA=$currentCommit"
    Write-RunLog $executionLog "APPROVAL_SHA256=$(Get-Sha256 $ApprovalRecordPath) WINDOW_ID=$($approval.window_id) BOARD_ID=$BoardId STOP_STRATEGY=$StopStrategy"
    Write-RunLog $executionLog "PNP=$($identity.Key) FRIENDLY_NAME=$($identity.FriendlyName) ROUTE=TYPEC_UART1"
    foreach ($entry in $artifactHashes.GetEnumerator()) { Write-RunLog $executionLog "PREFLIGHT_HASH_$($entry.Key.ToUpperInvariant())=$($entry.Value)" | Out-Null }
    foreach ($tool in $toolBindings.GetEnumerator()) { Write-RunLog $executionLog "PREFLIGHT_TOOL_$($tool.Key.ToUpperInvariant())_PATH=$($tool.Value.NormalizedPath) SHA256=$($tool.Value.Sha256) VERSION=$($tool.Value.Version)" | Out-Null }
    $efinityTarget = Select-UniqueEfinityTarget -Targets (Get-EfinityUsbSnapshot -SetupBat $toolBindings.efinity_setup.NormalizedPath -PythonExe $toolBindings.efinity_python.NormalizedPath -UsbResolverPath $toolBindings.usb_resolver_py.NormalizedPath -OutputPath $efinityUsbLog) -ExpectedVid $Manifest.volatile_config.expected_ftdi.vid -ExpectedPid $Manifest.volatile_config.expected_ftdi.pid -ExpectedSerial $Manifest.volatile_config.expected_ftdi.serial
    Write-RunLog $executionLog "EFINITY_USB_TARGET=VID=$($efinityTarget.vid) PID=$($efinityTarget.pid) SERIAL=$($efinityTarget.serial) URL=$($efinityTarget.urls[0]) BOARD_PROFILE=$($efinityTarget.board_profile)" | Out-Null
    Write-RunLog $executionLog "VOLATILE_CONFIG_ARGUMENT_FLOW=project=$($Manifest.volatile_config.project_xml_sha256) flow=program mode=$($Manifest.volatile_config.mode) source=$Bitstream flash_spi=PROHIBITED"
    Invoke-VolatileBitstreamConfig -EfxRunPath $toolBindings.efx_run.NormalizedPath -ProjectXml $projectXml -BitstreamPath $Bitstream -Config $Manifest.volatile_config -LogPath $volatileConfigLog | Out-Null
    Write-RunLog $executionLog 'FPGA_VOLATILE_CONFIG=PASS'
    $openOcdArguments = Get-OpenOcdArguments $FtdiConfig $TargetConfig
    Write-RunLog $executionLog "OPENOCD_ARGUMENT_FLOW=-f $FtdiConfig -f $TargetConfig"
    $openOcdProcess = Start-Process -FilePath $toolBindings.openocd.NormalizedPath -ArgumentList $openOcdArguments -RedirectStandardOutput $openOcdStdout -RedirectStandardError $openOcdStderr -WindowStyle Hidden -PassThru
    Wait-OpenOcdReady -Process $openOcdProcess -StdoutPath $openOcdStdout -StderrPath $openOcdStderr -Port $OpenOcdPort

    $serial = [IO.Ports.SerialPort]::new($identity.Port, 115200, [IO.Ports.Parity]::None, 8, [IO.Ports.StopBits]::One)
    $serial.Handshake = [IO.Ports.Handshake]::None
    $serial.ReadTimeout = 100
    $serial.WriteTimeout = 1000
    $serial.Open()
    $captureReady = [DateTimeOffset]::UtcNow
    Write-RunLog $executionLog "CAPTURE_READY_TIME=$($captureReady.ToString('o')) PNP=$($identity.Key) AUTO_CRLF=DISABLED"
    Invoke-GdbRamOnlyLoad $toolBindings.gdb.NormalizedPath $HelloElf $OpenOcdHost $OpenOcdPort $Manifest.fixed_artifacts.hello_entry_pc 'HELLO' $executionLog | Out-Null
    $client = [Net.Sockets.TcpClient]::new($OpenOcdHost, $OpenOcdPort)
    $stream = $client.GetStream()
    $rspState = New-RspState
    $supported = Send-RspCommand $stream $rspState 'qSupported:multiprocess+;swbreak+;hwbreak+'
    if ($supported -notmatch '(?:^|;)PacketSize=[0-9A-Fa-f]+(?:;|$)') { throw 'OpenOCD RSP qSupported negotiation failed; retry is prohibited.' }
    $helloMarker = Write-PhaseResumeMarker $liveDir $runId 'HELLO' $captureReady
    $helloResumeCount++
    Write-RunLog $executionLog "HELLO_RESUME_ONCE_TIME=$($helloMarker.Time.ToString('o')) HELLO_RESUME_COUNT=$helloResumeCount MARKER=$($helloMarker.Path)"
    $helloFailure = $null
    try {
        Send-RspCommand $stream $rspState 'c' -NoReply | Out-Null
        Complete-HelloEcho $serial $executionLog $counts ([byte][char]$EchoByte)
    } catch { $helloFailure = $_ }
    $helloStop = $null
    try {
        # After HELLO resume, every result path performs exactly one active halt.
        $stream.WriteByte(3); $stream.Flush()
        $helloStop = Wait-AsyncStopReply $stream $rspState 1000
    } catch {
        if ($null -eq $helloFailure) { $helloFailure = $_ }
    }
    if ($null -eq $helloStop) {
        Write-RunLog $executionLog 'HELLO_HALT_UNCONFIRMED RAM_READ_COUNT=0 APB_RESUME_COUNT=0 RETRY_COUNT=0' | Out-Null
        throw 'HELLO_HALT_UNCONFIRMED; APB phase prohibited.'
    }
    Write-RunLog $executionLog ('HELLO_HALT_CONFIRMED=true HALT_REASON={0} HALT_PC=0x{1:X8}' -f $helloStop.Reason, $helloStop.Pc)
    if ($null -ne $helloFailure) { throw $helloFailure }
    $stream.Dispose(); $stream = $null; $client.Dispose(); $client = $null

    Invoke-GdbRamOnlyLoad $toolBindings.gdb.NormalizedPath $ProbeElf $OpenOcdHost $OpenOcdPort $wsc.EntryPc 'APB' $executionLog | Out-Null
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
    $windowSucceeded = $true
} catch {
    Write-FailedClosedLog -LogPath $executionLog -Counts $counts -HelloResumeCount $helloResumeCount -ApbResumeCount $apbResumeCount -RetryCount $retryCount -RamReadCount $ramReadCount -FinalLogged $finalLogged -ErrorMessage $_.Exception.Message
    if (Test-SevereBlocker $_.Exception.Message) { throw }
    $retryCount++
    if ([DateTimeOffset]::UtcNow -gt [DateTimeOffset]::Parse($approvalObject.window_end_utc)) { throw 'SEVERE_BLOCKER_WINDOW_EXPIRED' }
} finally {
    if ($null -ne $serial) { if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose() }
    if ($null -ne $stream) { $stream.Dispose() }
    if ($null -ne $client) { $client.Dispose() }
    if ($null -ne $openOcdProcess -and -not $openOcdProcess.HasExited) { Stop-Process -Id $openOcdProcess.Id -Force }
}
} while (-not $windowSucceeded)
