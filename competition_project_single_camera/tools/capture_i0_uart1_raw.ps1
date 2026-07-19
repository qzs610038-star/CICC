[CmdletBinding()]
param(
    [Parameter(Mandatory)] [ValidatePattern('^COM[0-9]+$')] [string]$Port,
    [Parameter(Mandatory)] [string]$DeviceIdentity,
    [Parameter(Mandatory)] [string]$LogPath,
    [ValidateRange(1, 120)] [int]$CaptureSeconds = 20,
    [ValidatePattern('^[\x00-\x7F]$')] [string]$EchoByte = ''
)

$ErrorActionPreference = 'Stop'

if ($Port -eq 'COM17' -or $DeviceIdentity -match '(?i)CH340|1A86[&:]7523') {
    throw 'COM17/CH340 is prohibited for I0 Type-C UART1.'
}

$directory = Split-Path -Parent $LogPath
if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

$serial = [System.IO.Ports.SerialPort]::new($Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.ReadTimeout = 250
$serial.WriteTimeout = 1000
$rxCount = 0
$txCount = 0
$start = Get-Date

try {
    $serial.Open()
    if ($EchoByte.Length -eq 1) {
        $serial.Write($EchoByte)
        $txCount = 1
    }

    $deadline = $start.AddSeconds($CaptureSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $value = $serial.ReadByte()
            if ($value -ge 0) {
                $rxCount++
                '{0:o} RX 0x{1:X2}' -f (Get-Date), $value | Add-Content -LiteralPath $LogPath -Encoding ascii
            }
        } catch [System.TimeoutException] {
            # Preserve timing until the capture window ends.
        }
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
    @(
        'route=Type-C UART1',
        'format=115200 8N1',
        "port=$Port",
        "device_identity=$DeviceIdentity",
        "capture_start=$($start.ToString('o'))",
        "capture_end=$((Get-Date).ToString('o'))",
        "rx_bytes=$rxCount",
        "tx_bytes=$txCount"
    ) | Add-Content -LiteralPath $LogPath -Encoding ascii
}
