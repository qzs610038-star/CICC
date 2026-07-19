[CmdletBinding()]
param(
    [Parameter(Mandatory)] [ValidatePattern('^COM[0-9]+$')] [string]$Port,
    [Parameter(Mandatory)] [string]$LogPath,
    [ValidateRange(3, 120)] [int]$CaptureSeconds = 20,
    [Parameter(Mandatory)] [ValidatePattern('^[\x20-\x7E]$')] [string]$EchoByte
)

$ErrorActionPreference = 'Stop'

function Get-EnumeratedUartIdentity {
    param([string]$RequestedPort)

    $serialPort = Get-CimInstance -ClassName Win32_SerialPort |
        Where-Object { $_.DeviceID -eq $RequestedPort } |
        Select-Object -First 1
    if ($null -eq $serialPort) {
        throw "PnP enumeration has no serial device for $RequestedPort."
    }

    $instanceId = [string]$serialPort.PNPDeviceID
    $pnp = Get-CimInstance -ClassName Win32_PnPEntity |
        Where-Object { $_.DeviceID -eq $instanceId } |
        Select-Object -First 1
    if ($null -eq $pnp) {
        throw "PnP enumeration cannot resolve the serial instance for $RequestedPort."
    }

    $vidPid = [regex]::Match($instanceId, '(?i)VID_([0-9A-F]{4})&PID_([0-9A-F]{4})')
    if (-not $vidPid.Success) {
        throw "PnP instance has no VID/PID: $instanceId"
    }

    $serialNumber = ($instanceId -split '\\')[-1]
    $locationPaths = @()
    if (Get-Command Get-PnpDeviceProperty -ErrorAction SilentlyContinue) {
        $location = Get-PnpDeviceProperty -InstanceId $instanceId -KeyName 'DEVPKEY_Device_LocationPaths' -ErrorAction SilentlyContinue
        if ($null -ne $location -and $null -ne $location.Data) { $locationPaths = @($location.Data) }
    }
    if ($locationPaths.Count -eq 0) { $locationPaths = @([string]$pnp.Location) }
    if ([string]::IsNullOrWhiteSpace($locationPaths[0])) {
        throw "PnP enumeration has no location identity for $RequestedPort."
    }

    [pscustomobject]@{
        Port = $serialPort.DeviceID
        InstanceId = $instanceId
        FriendlyName = [string]$pnp.Name
        VID = $vidPid.Groups[1].Value.ToUpperInvariant()
        PID = $vidPid.Groups[2].Value.ToUpperInvariant()
        Serial = $serialNumber
        Location = ($locationPaths -join '|')
    }
}

if ($EchoByte -match '[\x00-\x1F\x7F]') {
    throw 'EchoByte must be exactly one printable 0x20..0x7E byte; CR/LF/control bytes are prohibited.'
}

$identity = Get-EnumeratedUartIdentity -RequestedPort $Port
if ($identity.Port -eq 'COM17' -or $identity.FriendlyName -match '(?i)CH340' -or ($identity.VID -eq '1A86' -and $identity.PID -eq '7523')) {
    throw 'COM17/CH340 is prohibited for I0 Type-C UART1.'
}

$directory = Split-Path -Parent $LogPath
if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

$serial = [System.IO.Ports.SerialPort]::new($identity.Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.ReadTimeout = 250
$serial.WriteTimeout = 1000
$rxCount = 0
$txCount = 0
$start = Get-Date
$helloLines = @(
    'I0 UART1 HELLO',
    'UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100',
    'Type characters to verify echo.'
)
$lineBuffer = [System.Text.StringBuilder]::new()
$helloIndex = 0
$helloComplete = $false
$echoVerified = $false

function Add-ByteEvent {
    param([string]$Direction, [byte]$Value)
    $stamp = (Get-Date).ToString('o')
    if ($Direction -eq 'RX') { $script:rxCount++ } else { $script:txCount++ }
    '{0} {1} 0x{2:X2} rx_bytes={3} tx_bytes={4}' -f $stamp, $Direction, $Value, $script:rxCount, $script:txCount |
        Add-Content -LiteralPath $LogPath -Encoding ascii
}

try {
    $serial.Open()
    $deadline = $start.AddSeconds($CaptureSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $value = $serial.ReadByte()
            if ($value -lt 0) { continue }

            [byte]$rxByte = $value
            Add-ByteEvent -Direction 'RX' -Value $rxByte
            if (-not $helloComplete) {
                if ($rxByte -eq 0x0A) {
                    $line = $lineBuffer.ToString().TrimEnd("`r")
                    $lineBuffer.Clear() | Out-Null
                    if ($line.Length -eq 0 -and $helloIndex -eq 0) { continue }
                    if ($line -cne $helloLines[$helloIndex]) {
                        throw "UART1 Hello line $($helloIndex + 1) mismatch: [$line]"
                    }
                    $helloIndex++
                    if ($helloIndex -eq $helloLines.Count) {
                        $helloComplete = $true
                        ('{0:o} HELLO_COMPLETE lines=3 rx_bytes={1} tx_bytes={2}' -f (Get-Date), $rxCount, $txCount) |
                            Add-Content -LiteralPath $LogPath -Encoding ascii
                        [byte]$txByte = [byte][char]$EchoByte
                        $serial.Write([byte[]]@($txByte), 0, 1)
                        Add-ByteEvent -Direction 'TX' -Value $txByte
                    }
                } else {
                    [void]$lineBuffer.Append([char]$rxByte)
                }
            } elseif (-not $echoVerified) {
                [byte]$expected = [byte][char]$EchoByte
                if ($rxByte -ne $expected) {
                    throw ('UART1 echo mismatch: expected 0x{0:X2}, got 0x{1:X2}' -f $expected, $rxByte)
                }
                $echoVerified = $true
                ('{0:o} ECHO_COMPLETE byte=0x{1:X2} rx_bytes={2} tx_bytes={3}' -f (Get-Date), $expected, $rxCount, $txCount) |
                    Add-Content -LiteralPath $LogPath -Encoding ascii
                break
            }
        } catch [System.TimeoutException] {
            # Preserve the capture window while no byte is available.
        }
    }
    if (-not $helloComplete) { throw 'UART1 complete three-line Hello was not received before deadline.' }
    if (-not $echoVerified) { throw 'UART1 printable-byte echo was not received before deadline.' }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
    @(
        'route=Type-C UART1',
        'format=115200 8N1',
        "port=$($identity.Port)",
        "pnp_instance_id=$($identity.InstanceId)",
        "pnp_friendly_name=$($identity.FriendlyName)",
        "pnp_vid=$($identity.VID)",
        "pnp_pid=$($identity.PID)",
        "pnp_serial=$($identity.Serial)",
        "pnp_location=$($identity.Location)",
        "capture_start=$($start.ToString('o'))",
        "capture_end=$((Get-Date).ToString('o'))",
        "rx_bytes=$rxCount",
        "tx_bytes=$txCount",
        "hello_complete=$helloComplete",
        "echo_complete=$echoVerified"
    ) | Add-Content -LiteralPath $LogPath -Encoding ascii
}
