[CmdletBinding()]
param(
    [string]$EvidenceDir = (Join-Path $PSScriptRoot '..\docs\debug_sessions\evidence'),
    [string]$Label = 'm2_ftdi_enumeration',
    [switch]$RequireFtdi
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-FieldValue {
    param(
        [Parameter(Mandatory = $true)][string]$Block,
        [Parameter(Mandatory = $true)][string]$Field
    )

    $match = [regex]::Match($Block, "(?m)^$([regex]::Escape($Field))\s*:\s*(?<value>.+)$")
    if ($match.Success) {
        return $match.Groups['value'].Value.Trim()
    }
    return $null
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$pnputilLines = @(& pnputil /enum-devices /connected /class Ports)
$pnputilExitCode = $LASTEXITCODE
if ($pnputilExitCode -ne 0) {
    throw "pnputil connected Ports enumeration failed with exit code $pnputilExitCode"
}

$pnputilText = $pnputilLines -join [Environment]::NewLine
$blocks = [regex]::Split($pnputilText.Trim(), '(?:\r?\n){2,}') |
    Where-Object { $_ -match '(?m)^Instance ID\s*:' }

$ports = foreach ($block in $blocks) {
    $description = Get-FieldValue -Block $block -Field 'Device Description'
    $instanceId = Get-FieldValue -Block $block -Field 'Instance ID'
    $comMatch = [regex]::Match($description, '\(COM(?<number>\d+)\)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    [PSCustomObject]@{
        instance_id = $instanceId
        description = $description
        status = Get-FieldValue -Block $block -Field 'Status'
        driver = Get-FieldValue -Block $block -Field 'Driver Name'
        com_port = if ($comMatch.Success) { "COM$($comMatch.Groups['number'].Value)" } else { $null }
        is_ftdi_0403_6011 = ($block -match '(?i)VID_0403[&+]PID_6011')
        is_ch340_1a86_7523 = ($block -match '(?i)VID_1A86&PID_7523')
    }
}

$ftdiPorts = @($ports | Where-Object { $_.is_ftdi_0403_6011 })
$ch340Ports = @($ports | Where-Object { $_.is_ch340_1a86_7523 })
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$evidencePath = Join-Path (Resolve-Path $EvidenceDir).Path ("{0}_{1}.json" -f $Label, $timestamp)

$result = if ($ftdiPorts.Count -gt 0) { 'FTDI_ENUMERATED' } else { 'HOLD_NO_CONNECTED_FTDI' }
$evidence = [ordered]@{
    schema = 'm2_ftdi_preflight_v1'
    timestamp_local = (Get-Date).ToString('o')
    repo_head = $head
    read_only_actions = @(
        'pnputil /enum-devices /connected /class Ports',
        'git rev-parse HEAD'
    )
    serial_port_opened = $false
    uart_bytes_sent = 0
    programmer_invoked = $false
    flash_operation_invoked = $false
    user_tap_selected = $null
    ports = @($ports)
    ftdi_0403_6011_connected_count = $ftdiPorts.Count
    ch340_1a86_7523_ports = @($ch340Ports | ForEach-Object { $_.com_port })
    result = $result
    next_action = if ($ftdiPorts.Count -gt 0) {
        'Return this JSON and a Device Manager screenshot for port-differential review; do not configure FPGA yet.'
    } else {
        'Keep PRE-FLIGHT HOLD; connect only the board USB/JTAG-IF path when available, then rerun with -RequireFtdi.'
    }
    raw_pnputil_connected_ports = $pnputilText
}

$evidence | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $evidencePath -Encoding utf8
[PSCustomObject]@{
    result = $result
    ftdi_0403_6011_connected_count = $ftdiPorts.Count
    ch340_1a86_7523_ports = @($ch340Ports | ForEach-Object { $_.com_port })
    evidence_path = $evidencePath
} | ConvertTo-Json -Depth 4

if ($RequireFtdi -and $ftdiPorts.Count -eq 0) {
    exit 2
}
