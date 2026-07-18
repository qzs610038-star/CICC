[CmdletBinding()]
param(
    [ValidateSet('COM12', 'COM17')]
    [string]$PortName = 'COM12',
    [ValidateRange(3, 60)]
    [int]$ListenSeconds = 5,
    [string]$PcGateApproval,
    [string]$ManifestPath,
    [string]$EvidenceDir,
    [string]$Label = 'r0_uart0_banner_capture',
    [switch]$Listen
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $PSScriptRoot '..\docs\debug_sessions\evidence'
}

$requiredApprovalResult = 'R0_USER2_RAM_PC_GATE_APPROVED'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (& git -C $projectRoot rev-parse --show-toplevel).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($repoRoot)) {
    throw 'Unable to resolve the Git repository root.'
}
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $projectRoot 'docs\debug_sessions\r0_current_batch_manifest_20260718.json'
}
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "R0 manifest is missing: $ManifestPath"
}
try {
    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json -ErrorAction Stop
}
catch {
    throw "R0 manifest is not valid JSON: $ManifestPath"
}

$manifestChecks = [ordered]@{
    schema = ($manifest.schema -eq 'r0_current_batch_manifest')
    version = ($manifest.version -eq 1)
    batch_id = ($manifest.batch_id -eq 'R0-20260717-9F6F-CD4C')
    bitstream_sha256 = ("$($manifest.bitstream.sha256)" -match '^[A-Fa-f0-9]{64}$')
    bitstream_size = ([int64]$manifest.bitstream.size_bytes -eq 11847132)
    elf_sha256 = ("$($manifest.hello_elf.sha256)" -match '^[A-Fa-f0-9]{64}$')
    elf_size = ([int64]$manifest.hello_elf.size_bytes -eq 31148)
    pc_range = ("$($manifest.hello_elf.valid_pc_range)" -eq '0xF9000000..0xF9003FFF')
    allowed_ports = (@($manifest.uart0.allowed_ports) -contains $PortName)
    ch340_vid_pid = ("$($manifest.uart0.required_usb_vid_pid)" -eq '1A86:7523')
}
$inputBlobChecks = [ordered]@{}
foreach ($property in $manifest.input_blobs.PSObject.Properties) {
    $actualBlob = (& git -C $repoRoot rev-parse "HEAD:$($property.Name)" 2>$null).Trim()
    $inputBlobChecks[$property.Name] = ($LASTEXITCODE -eq 0 -and $actualBlob -eq "$($property.Value)")
}
$failedManifestChecks = @($manifestChecks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)
$failedInputBlobs = @($inputBlobChecks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)
if ($failedManifestChecks.Count -gt 0 -or $failedInputBlobs.Count -gt 0) {
    throw ('R0 manifest does not bind the checked-out hardware/Hello inputs. checks=' +
        ($failedManifestChecks -join ',') + '; blobs=' + ($failedInputBlobs -join ','))
}
$expectedBitSha256 = "$($manifest.bitstream.sha256)".ToUpperInvariant()
$expectedElfSha256 = "$($manifest.hello_elf.sha256)".ToUpperInvariant()

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
        reason = if ($valid) { 'approved' } else { 'approval_fields_do_not_match_current_r0_batch' }
        path = (Resolve-Path -LiteralPath $Path).Path
        result = $result
        bitstream_sha256 = $bitHash
        elf_sha256 = $elfHash
        pc_range = $pcRange
    }
}

function Get-ConnectedPortNames {
    if (Initialize-SerialPortApi) {
        try {
        return @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
        }
        catch {
            # Fall through to the kernel map when enumeration itself fails.
        }
    }

    try {
        # Some PowerShell hosts cannot load System.IO.Ports even though Windows
        # has enumerated serial devices.  Use the kernel serial map as a
        # read-only fallback; opening the port still remains separately gated.
        $serialMap = Get-ItemProperty 'HKLM:\HARDWARE\DEVICEMAP\SERIALCOMM' -ErrorAction Stop
        return @(
            $serialMap.PSObject.Properties |
                Where-Object { $_.Name -notmatch '^PS' } |
                ForEach-Object { "$($_.Value)" } |
                Where-Object { $_ -match '^COM\d+$' } |
                Sort-Object -Unique
        )
    }
    catch {
        return @()
    }
}

function Initialize-SerialPortApi {
    if ($null -ne ('System.IO.Ports.SerialPort' -as [type])) {
        return $true
    }

    foreach ($assemblyPath in @(
        'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\System.dll',
        'C:\Windows\Microsoft.NET\Framework\v4.0.30319\System.dll'
    )) {
        if (-not (Test-Path -LiteralPath $assemblyPath)) {
            continue
        }
        try {
            Add-Type -Path $assemblyPath -ErrorAction Stop
            if ($null -ne ('System.IO.Ports.SerialPort' -as [type])) {
                return $true
            }
        }
        catch {
            # Try the remaining framework assembly before failing closed.
        }
    }

    return $false
}

function Get-Ch340PortIdentity {
    param([string]$Name)

    try {
        $escapedName = [regex]::Escape($Name)
        $device = @(
            Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction Stop |
                Where-Object {
                    "$($_.Name)" -match "\($escapedName\)" -and
                    "$($_.PNPDeviceID)" -match '(?i)VID_1A86&PID_7523'
                }
        ) | Select-Object -First 1
        if ($null -eq $device) {
            return [ordered]@{
                valid = $false
                reason = 'selected_port_is_not_enumerated_as_CH340_VID_1A86_PID_7523'
                port = $Name
            }
        }
        return [ordered]@{
            valid = $true
            reason = 'approved_ch340_identity'
            port = $Name
            name = "$($device.Name)"
            pnp_device_id = "$($device.PNPDeviceID)"
        }
    }
    catch {
        return [ordered]@{
            valid = $false
            reason = 'ch340_identity_query_failed'
            port = $Name
            error = $_.Exception.Message
        }
    }
}

$approval = Read-PcGateApproval -Path $PcGateApproval
$connectedPortNames = Get-ConnectedPortNames
$portIdentity = Get-Ch340PortIdentity -Name $PortName
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
elseif (-not $portIdentity.valid) {
    $result = 'HOLD_SELECTED_PORT_NOT_APPROVED_CH340_NO_SERIAL_OPEN'
}
else {
    $port = $null
    try {
        if (-not (Initialize-SerialPortApi)) {
            throw 'System.IO.Ports.SerialPort is unavailable in this PowerShell host.'
        }
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
    schema = 'r0_uart0_banner_capture_v2'
    timestamp_local = (Get-Date).ToString('o')
    repo_head = $head
    manifest_path = (Resolve-Path -LiteralPath $ManifestPath).Path
    manifest_batch_id = "$($manifest.batch_id)"
    manifest_checks = $manifestChecks
    input_blob_checks = $inputBlobChecks
    requested_port = $PortName
    selected_port_identity = $portIdentity
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
    expected_bitstream_sha256 = $expectedBitSha256
    expected_bitstream_size_bytes = [int64]$manifest.bitstream.size_bytes
    expected_elf_sha256 = $expectedElfSha256
    expected_elf_size_bytes = [int64]$manifest.hello_elf.size_bytes
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
