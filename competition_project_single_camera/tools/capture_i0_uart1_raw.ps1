[CmdletBinding()]
param(
    [Parameter(Mandatory)] [ValidatePattern('^COM[0-9]+$')] [string]$Port,
    [Parameter(Mandatory)] [string]$LogPath,
    [Parameter(Mandatory)] [string[]]$PnPAllowlist,
    [Parameter(Mandatory)] [ValidatePattern('^[\x20-\x7E]$')] [string]$EchoByte,
    [Parameter(Mandatory)] [string]$ResumeMarkerPath,
    [ValidateRange(1, 120)] [int]$SilentWindowSeconds = 5
)

$ErrorActionPreference = 'Stop'

function Get-Identity {
    param([string]$RequestedPort)
    $serialPort = Get-CimInstance Win32_SerialPort | Where-Object { $_.DeviceID -eq $RequestedPort } | Select-Object -First 1
    if ($null -eq $serialPort) { throw "No PnP serial identity for $RequestedPort." }
    $instance = [string]$serialPort.PNPDeviceID
    $match = [regex]::Match($instance, '(?i)VID_([0-9A-F]{4})&PID_([0-9A-F]{4})')
    if (-not $match.Success) { throw "PnP identity has no VID/PID: $instance" }
    $pnp = Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -eq $instance } | Select-Object -First 1
    $serial = ($instance -split '\\')[-1]
    [pscustomobject]@{
        Port = $serialPort.DeviceID; Instance = $instance; FriendlyName = [string]$pnp.Name
        VID = $match.Groups[1].Value.ToUpperInvariant(); PID = $match.Groups[2].Value.ToUpperInvariant(); Serial = $serial
    }
}

function Add-Event {
    param([string]$Kind, [byte]$Value)
    $stamp = (Get-Date).ToString('o')
    if ($Kind -eq 'RX') { $script:rxCount++ } elseif ($Kind -eq 'TX') { $script:txCount++ }
    "$stamp $Kind 0x$($Value.ToString('X2')) RX_COUNT=$script:rxCount TX_COUNT=$script:txCount" | Add-Content -LiteralPath $LogPath -Encoding ascii
}

if ($Port -eq 'COM17') { throw 'COM17 is prohibited for I0 Type-C UART1.' }
if ($EchoByte -match '[\x00-\x1F\x7F]') { throw 'Only one printable ASCII byte is allowed; CR/LF is prohibited.' }
$identity = Get-Identity $Port
if ($identity.FriendlyName -match '(?i)CH340' -or ($identity.VID -eq '1A86' -and $identity.PID -eq '7523')) { throw 'CH340 is prohibited for I0 Type-C UART1.' }
$identityKey = "VID=$($identity.VID);PID=$($identity.PID);SERIAL=$($identity.Serial);INSTANCE=$($identity.Instance)"
if ($PnPAllowlist -notcontains $identityKey) { throw "PnP identity is not allowlisted: $identityKey" }

$directory = Split-Path -Parent $LogPath
if (-not (Test-Path -LiteralPath $directory)) { New-Item -ItemType Directory -Path $directory -Force | Out-Null }
$helloLines = @('I0 UART1 HELLO', 'UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100', 'Type characters to verify echo.')
$rxCount = 0; $txCount = 0; $hello = [System.Text.StringBuilder]::new(); $helloIndex = 0; $helloComplete = $false; $echoComplete = $false; $resumeRecorded = $false
$serial = [System.IO.Ports.SerialPort]::new($identity.Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.Handshake = [System.IO.Ports.Handshake]::None; $serial.ReadTimeout = 250; $serial.WriteTimeout = 1000
try {
    $serial.Open()
    $captureReady = Get-Date
    "CAPTURE_READY=$($captureReady.ToString('o')) ROUTE=Type-C_UART1 FORMAT=115200_8N1 PNP=$identityKey AUTO_CRLF=DISABLED" | Add-Content -LiteralPath $LogPath -Encoding ascii
    $deadline = $captureReady.AddSeconds(120)
    while ((Get-Date) -lt $deadline -and -not $echoComplete) {
        try {
            if (-not $resumeRecorded -and (Test-Path -LiteralPath $ResumeMarkerPath -PathType Leaf)) {
                $marker = (Get-Content -LiteralPath $ResumeMarkerPath -Raw).Trim()
                if ($marker -notmatch '^RESUME_ONCE=(?<time>.+)$') { throw 'Resume marker must be exactly RESUME_ONCE=<ISO-8601 timestamp>.' }
                $resumeTime = [DateTimeOffset]::Parse($Matches.time)
                if ($resumeTime.UtcDateTime -lt $captureReady.ToUniversalTime()) { throw 'Resume marker predates CAPTURE_READY.' }
                "RESUME_ONCE=$($resumeTime.ToString('o'))" | Add-Content -LiteralPath $LogPath -Encoding ascii
                $resumeRecorded = $true
            }
            [byte]$value = $serial.ReadByte(); Add-Event RX $value
            if (-not $helloComplete) {
                if ($value -eq 0x0A) {
                    $line = $hello.ToString().TrimEnd("`r"); $hello.Clear() | Out-Null
                    if ($line -cne $helloLines[$helloIndex]) { throw "Hello line $($helloIndex + 1) mismatch: [$line]" }
                    $helloIndex++
                    if ($helloIndex -eq $helloLines.Count) {
                        if (-not $resumeRecorded) { throw 'Hello arrived before a valid RESUME_ONCE marker.' }
                        $helloComplete = $true
                        "HELLO_COMPLETE=$((Get-Date).ToString('o'))" | Add-Content -LiteralPath $LogPath -Encoding ascii
                        [byte]$tx = [byte][char]$EchoByte; $serial.Write([byte[]]@($tx), 0, 1); Add-Event TX $tx
                    }
                } else {
                    [void]$hello.Append([char]$value)
                }
            } elseif ($value -eq [byte][char]$EchoByte) {
                $echoComplete = $true
                "ECHO_COMPLETE=$((Get-Date).ToString('o'))" | Add-Content -LiteralPath $LogPath -Encoding ascii
                $silentEnd = (Get-Date).AddSeconds($SilentWindowSeconds)
                while ((Get-Date) -lt $silentEnd) { try { [byte]$extra = $serial.ReadByte(); Add-Event RX $extra } catch [System.TimeoutException] {} }
                "SILENT_WINDOW_END=$($silentEnd.ToString('o'))" | Add-Content -LiteralPath $LogPath -Encoding ascii
            } else { throw "Echo mismatch: expected 0x$(([byte][char]$EchoByte).ToString('X2')), got 0x$($value.ToString('X2'))" }
        } catch [System.TimeoutException] {}
    }
    if (-not $helloComplete) { throw 'Hello was not received before deadline.' }
    if (-not $echoComplete) { throw 'Single-byte echo was not received before deadline.' }
    if (-not $resumeRecorded) { throw 'No valid RESUME_ONCE marker was recorded.' }
} finally {
    if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose()
    "TX_COUNT=$txCount RX_COUNT=$rxCount" | Add-Content -LiteralPath $LogPath -Encoding ascii
}
