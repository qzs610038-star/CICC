[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM[1-9][0-9]*$')]
    [string]$PortName,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$PcCheckpointPath,
    [ValidateSet(115200)]
    [int]$Baud = 115200,
    [ValidateRange(3, 30)]
    [int]$ListenSeconds = 5,
    [switch]$Listen
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expected = [ordered]@{
    batch_id = 'G1-20260717-A897-E5BC'
    bitstream_sha256 = 'A897E33514A1079BB1B46C02C464B0BD679AF551CEFCB67C4A0EBD5B8FCD1ACD'
    elf_sha256 = 'E5BC80A2F18A7E2951D53DA539BE2FC61AAECFA90C5CDADB29E65FFC6141928A'
    pc_range = '0xF9000000..0xF9003FFF'
    approval_result = 'G1_USER2_RAM_PC_GATE_APPROVED'
}

function Get-CheckpointStatus {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{ valid = $false; reason = 'pc_checkpoint_missing'; path = $Path }
    }
    try {
        $checkpoint = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        return [ordered]@{ valid = $false; reason = 'pc_checkpoint_unparseable'; path = $Path }
    }

    $checks = [ordered]@{
        result = ($checkpoint.result -eq $expected.approval_result)
        batch_id = ($checkpoint.batch_id -eq $expected.batch_id)
        bitstream_sha256 = ($checkpoint.bitstream_sha256 -eq $expected.bitstream_sha256)
        elf_sha256 = ($checkpoint.elf_sha256 -eq $expected.elf_sha256)
        pc_range = ($checkpoint.pc_range -eq $expected.pc_range)
        reviewer = (-not [string]::IsNullOrWhiteSpace("$($checkpoint.reviewer)"))
        approved_at = (-not [string]::IsNullOrWhiteSpace("$($checkpoint.approved_at)"))
    }
    $failed = @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)
    [ordered]@{
        valid = ($failed.Count -eq 0)
        reason = if ($failed.Count -eq 0) { 'approved_current_batch' } else { 'pc_checkpoint_fields_mismatch: ' + ($failed -join ', ') }
        path = (Resolve-Path -LiteralPath $Path).Path
        failed_checks = $failed
    }
}

$checkpointStatus = Get-CheckpointStatus -Path $PcCheckpointPath
$baseResult = [ordered]@{
    requested_port = $PortName
    baud = $Baud
    listen_requested = [bool]$Listen
    checkpoint = $checkpointStatus
    serial_port_enumerated = $false
    serial_open_attempted = $false
    serial_port_opened = $false
    uart_bytes_sent = 0
}

if (-not $checkpointStatus.valid) {
    $baseResult.result = 'HOLD_PC_CHECKPOINT_REQUIRED_NO_SERIAL_ACCESS'
    $baseResult | ConvertTo-Json -Depth 5
    exit 2
}
if (-not $Listen) {
    $baseResult.result = 'HOLD_EXPLICIT_LISTEN_SWITCH_REQUIRED_NO_SERIAL_ACCESS'
    $baseResult | ConvertTo-Json -Depth 5
    exit 2
}

$serialPort = $null
$rawBytes = [System.Collections.Generic.List[byte]]::new()
$exceptionMessage = $null
try {
    Add-Type -AssemblyName System.IO.Ports -ErrorAction Stop
    $baseResult.serial_open_attempted = $true
    $serialPort = [System.IO.Ports.SerialPort]::new($PortName, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serialPort.Handshake = [System.IO.Ports.Handshake]::None
    $serialPort.DtrEnable = $false
    $serialPort.RtsEnable = $false
    $serialPort.ReadTimeout = 100
    $serialPort.Open()
    $baseResult.serial_port_opened = $true
    $deadline = [DateTime]::UtcNow.AddSeconds($ListenSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($serialPort.BytesToRead -gt 0) {
            $buffer = [byte[]]::new($serialPort.BytesToRead)
            $read = $serialPort.Read($buffer, 0, $buffer.Length)
            for ($index = 0; $index -lt $read; $index++) { [void]$rawBytes.Add($buffer[$index]) }
        }
        Start-Sleep -Milliseconds 50
    }
}
catch {
    $exceptionMessage = $_.Exception.Message
}
finally {
    if ($null -ne $serialPort -and $serialPort.IsOpen) { $serialPort.Close() }
    if ($null -ne $serialPort) { $serialPort.Dispose() }
}

$baseResult.raw_rx_byte_count = $rawBytes.Count
$baseResult.raw_rx_base64 = [Convert]::ToBase64String($rawBytes.ToArray())
$baseResult.raw_rx_ascii = [System.Text.Encoding]::ASCII.GetString($rawBytes.ToArray())
$baseResult.listener_exception = $exceptionMessage
$baseResult.result = if ($null -eq $exceptionMessage) { 'UART0_READ_ONLY_CAPTURE_COMPLETE_NO_TX' } else { 'HOLD_UART0_READ_ONLY_CAPTURE_FAILED_NO_TX' }
$baseResult | ConvertTo-Json -Depth 5
if ($null -ne $exceptionMessage) { exit 2 }
