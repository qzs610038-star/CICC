[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$TargetRoot,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$EfinityRoot,
    [string]$EvidenceDir,
    [string]$Label = 'm2_user2_debug_bsp',
    [switch]$Apply
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $PSScriptRoot '..\docs\debug_sessions\evidence'
}

function Get-OptionalArtifactIdentity {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{ path = $Path; exists = $false; bytes = $null; sha256 = $null }
    }
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        path = $item.FullName
        exists = $true
        bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$targetResolved = (Resolve-Path -LiteralPath $TargetRoot -ErrorAction Stop).Path.TrimEnd('\')
$repoResolved = $repoRoot.TrimEnd('\')
if ($targetResolved -eq $repoResolved -or $targetResolved.StartsWith($repoResolved + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to generate BSP inside the repository: $targetResolved"
}

$targetXml = Join-Path $targetResolved 'mem_test.xml'
if (-not (Test-Path -LiteralPath $targetXml -PathType Leaf)) {
    throw "Manual ASCII target is missing mem_test.xml: $targetXml"
}

$settingsPath = Join-Path $repoRoot 'ip\EfxSapphireHpSoc_slb\settings.json'
$helloMapPath = Join-Path $repoRoot 'cpu_bringup\uart_hello_onchip\build\uart_hello_onchip.map'
$generator = Join-Path $EfinityRoot 'ipm\ip\efx_hard_soc\embedded_sw\sw_script.py'
$pythonHome = Join-Path $EfinityRoot 'python311'
$python = Join-Path $pythonHome 'bin\python.exe'
$ideRoot = Join-Path (Split-Path -Parent $EfinityRoot) 'efinity-riscv-ide-2025.2'
$tiLaunchTemplate = Join-Path $ideRoot 'templates\launch_configurations\ti.ftl'
$jtagCtrlModel = Join-Path $EfinityRoot 'pt\sim_models\verilog\EFX_JTAG_CTRL.v'
foreach ($required in @($settingsPath, $helloMapPath, $generator, $python, $tiLaunchTemplate, $jtagCtrlModel)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required USER2 BSP-generation input is missing: $required"
    }
}

$blockedProcesses = @('efinity-riscv-ide', 'openocd', 'jtag_daemon', 'riscv-none-embed-gdb')
$activeDebugProcesses = @(
    foreach ($name in $blockedProcesses) {
        Get-Process -Name $name -ErrorAction SilentlyContinue
    }
)
if ($activeDebugProcesses.Count -gt 0) {
    $names = ($activeDebugProcesses | Select-Object -ExpandProperty ProcessName -Unique) -join ', '
    throw "Refusing to change manual BSP while debug tooling is running: $names"
}

$settings = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
$resolvedParameters = @(
    $settings.conf.PSObject.Properties | ForEach-Object {
        $value = "$($_.Value)"
        if ($value -match "^\d+'b([01])$") {
            $value = $Matches[1]
        }
        '{0}={1}' -f $_.Name, $value
    }
)

$fullBspRoot = Join-Path $targetResolved 'embedded_sw'
$openocdDir = Join-Path $fullBspRoot 'efx_hard_soc\bsp\efinix\EfxSapphireSoc\openocd'
$profilePath = Join-Path $openocdDir 'debug_profile_hard.cfg'
$debugTiPath = Join-Path $openocdDir 'debug_ti.cfg'
$safeDebugTiPath = Join-Path $openocdDir 'debug_ti_m2_safe.cfg'
$debugSoftPath = Join-Path $openocdDir 'debug_softTap.cfg'
$ftdiTiPath = Join-Path $openocdDir 'ftdi_ti.cfg'
$externalCfgPath = Join-Path $openocdDir 'external.cfg'
$jtagDaemonCfgPath = Join-Path $openocdDir 'jtag_daemon.cfg'
$safeWorkAreaCfgPath = Join-Path $targetResolved 'm2_cpuhello_20260716_1730\m2_user2_safe_workarea.cfg'
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "git rev-parse HEAD failed with exit code $LASTEXITCODE"
}

$generatorExitCode = $null
$generatorOutput = @()
if ($Apply) {
    $oldPythonHome = $env:PYTHONHOME
    $oldPythonPath = $env:PYTHONPATH
    try {
        $env:PYTHONHOME = $pythonHome
        Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue
        $generatorOutput = @(& $python $generator --out_path $fullBspRoot --ip_path (Join-Path $repoRoot 'ip\EfxSapphireHpSoc_slb') --gen_name 'EfxSapphireHpSoc_slb' --ipm_path (Join-Path $EfinityRoot 'ipm\ip') --parameters $resolvedParameters 2>&1)
        $generatorExitCode = $LASTEXITCODE
    }
    finally {
        if ($null -eq $oldPythonHome) { Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue } else { $env:PYTHONHOME = $oldPythonHome }
        if ($null -eq $oldPythonPath) { Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue } else { $env:PYTHONPATH = $oldPythonPath }
    }
    if ($generatorExitCode -ne 0) {
        throw "Official Efinity Hard SoC BSP generator failed with exit code $generatorExitCode"
    }

    $safeWorkAreaDir = Split-Path -Parent $safeWorkAreaCfgPath
    New-Item -ItemType Directory -Force -Path $safeWorkAreaDir | Out-Null
    @'
# M2 USER2 RAM-debug work-area override; generated by prepare_m2_user2_debug_bsp.ps1.
# The fixed Hello ELF occupies 0xF9000000..0xF9000A30 in the 16 KiB on-chip RAM.
# Keep the debugger work area in the unused tail.  This file contains no init,
# halt, resume, reset, flash, program, or load command.
foreach target {fpga_spinal.cpu0 fpga_spinal.cpu1 fpga_spinal.cpu2 fpga_spinal.cpu3} {
    $target configure -work-area-phys 0xF9000C00 -work-area-size 1024 -work-area-backup 0
}
'@ | Set-Content -LiteralPath $safeWorkAreaCfgPath -Encoding utf8

    $sourceDebugTiText = Get-Content -LiteralPath $debugTiPath -Raw
    $sourceWorkArea = '-work-area-phys 0xF9000000 -work-area-size 1024'
    $safeWorkArea = '-work-area-phys 0xF9000C00 -work-area-size 1024'
    $sourceWorkAreaCount = [regex]::Matches($sourceDebugTiText, [regex]::Escape($sourceWorkArea)).Count
    # The official four-hart template contains three textual target-create
    # clauses: cpu0, the SMP loop for cpu1..3, and the non-SMP loop for
    # cpu0..3.  All three execute before init/halt in their respective branch.
    if ($sourceWorkAreaCount -ne 3) {
        throw "Refusing to derive safe debug_ti.cfg: expected exactly three source work-area clauses for the four-hart template, found $sourceWorkAreaCount"
    }
    # Do not let Set-Content append a final newline: the audit below requires
    # this file to be exactly the official source text plus the three address
    # substitutions, including its original end-of-file layout.
    [System.IO.File]::WriteAllText(
        $safeDebugTiPath,
        $sourceDebugTiText.Replace($sourceWorkArea, $safeWorkArea),
        [System.Text.UTF8Encoding]::new($false)
    )
}

$profileReady = Test-Path -LiteralPath $profilePath -PathType Leaf
$debugTiReady = Test-Path -LiteralPath $debugTiPath -PathType Leaf
$safeDebugTiReady = Test-Path -LiteralPath $safeDebugTiPath -PathType Leaf
$debugSoftReady = Test-Path -LiteralPath $debugSoftPath -PathType Leaf
$ftdiTiReady = Test-Path -LiteralPath $ftdiTiPath -PathType Leaf
$safeWorkAreaCfgReady = Test-Path -LiteralPath $safeWorkAreaCfgPath -PathType Leaf
$profileText = if ($profileReady) { Get-Content -LiteralPath $profilePath -Raw } else { '' }
$debugTiText = if ($debugTiReady) { Get-Content -LiteralPath $debugTiPath -Raw } else { '' }
$safeDebugTiText = if ($safeDebugTiReady) { Get-Content -LiteralPath $safeDebugTiPath -Raw } else { '' }
$debugSoftText = if ($debugSoftReady) { Get-Content -LiteralPath $debugSoftPath -Raw } else { '' }
$ftdiTiText = if ($ftdiTiReady) { Get-Content -LiteralPath $ftdiTiPath -Raw } else { '' }
$tiLaunchText = Get-Content -LiteralPath $tiLaunchTemplate -Raw
$targetXmlText = Get-Content -LiteralPath $targetXml -Raw
$jtagCtrlText = Get-Content -LiteralPath $jtagCtrlModel -Raw
$helloMapText = Get-Content -LiteralPath $helloMapPath -Raw
$safeWorkAreaCfgText = if ($safeWorkAreaCfgReady) { Get-Content -LiteralPath $safeWorkAreaCfgPath -Raw } else { '' }

$validation = [ordered]@{
    profile_exists = $profileReady
    debug_ti_exists = $debugTiReady
    safe_debug_ti_exists = $safeDebugTiReady
    debug_softtap_exists = $debugSoftReady
    ftdi_ti_exists = $ftdiTiReady
    profile_jtag_type_is_fpga_user_tap = ($profileText -match '(?m)^INTF_JTAG_TYPE=0\r?$')
    profile_tap_is_user2_9 = ($profileText -match '(?m)^INTF_JTAG_TAP_SEL=9\r?$')
    profile_device_is_tj375n529 = ($profileText -match '(?m)^DEVICE=TJ375N529\r?$')
    debug_ti_sets_bscan_tunnel_ir_9 = ($debugTiText -match '(?m)^\s*riscv set_bscan_tunnel_ir 9\s*$')
    debug_ti_has_unresolved_placeholder = ($debugTiText -match '\?')
    debug_ti_has_legacy_instr_addr_00001000 = ($debugTiText -match '(?m)^set instr_addr 0x00001000\r?$')
    debug_ti_consumes_instr_addr_variable = ($debugTiText -match '\$\{?instr_addr\}?')
    debug_softtap_has_legacy_instr_addr_00001000 = ($debugSoftText -match '(?m)^set instr_addr 0x00001000\r?$')
    ftdi_ti_vid_pid_is_0403_6011 = ($ftdiTiText -match '(?m)^ftdi vid_pid 0x0403 0x6011\r?$')
    ftdi_ti_channel_is_1 = ($ftdiTiText -match '(?m)^ftdi channel 1\r?$')
    target_xml_device_is_tj375n529 = ($targetXmlText -match '<efx:device name="TJ375N529"/>')
    official_tj375n529_jtag_id_is_006a0ef3 = ($jtagCtrlText -match '"TJ375N529"\s*:\s*IDCODE_value\s*=\s*32\x27h006A0EF3')
    ftdi_ti_supports_predefined_cputapid = ($ftdiTiText -match '(?s)if \{ \[info exists CPUTAPID\] \}.*?set _CPUTAPID \$CPUTAPID')
    ftdi_ti_default_cputapid_is_ti375_006a0a79 = ($ftdiTiText -match '(?m)^\s*set _CPUTAPID 0x006A0A79\s*$')
    hello_map_entry_is_f9000000 = ($helloMapText -match '(?im)^\s*0x0*f9000000\s+_start\s*$')
    hello_map_image_end_is_f9000a30 = ($helloMapText -match '(?im)^\s*0x0*f9000a30\s+__freertos_irq_stack_top\s*=')
    safe_workarea_cfg_exists = $safeWorkAreaCfgReady
    safe_workarea_range_is_onchip_tail = ($safeWorkAreaCfgText -match '-work-area-phys 0xF9000C00 -work-area-size 1024 -work-area-backup 0')
    safe_workarea_targets_all_harts = ($safeWorkAreaCfgText -match 'fpga_spinal\.cpu0 fpga_spinal\.cpu1 fpga_spinal\.cpu2 fpga_spinal\.cpu3')
    safe_workarea_has_no_execution_or_flash_command = -not ($safeWorkAreaCfgText -match '(?im)^\s*(init|halt|resume|reset|flash|program|load_image)\b')
    debug_ti_has_exactly_three_overlapping_workarea_clauses = ([regex]::Matches($debugTiText, [regex]::Escape('-work-area-phys 0xF9000000 -work-area-size 1024')).Count -eq 3)
    safe_debug_ti_has_exactly_three_tail_workarea_clauses = ([regex]::Matches($safeDebugTiText, [regex]::Escape('-work-area-phys 0xF9000C00 -work-area-size 1024')).Count -eq 3)
    safe_debug_ti_has_no_overlapping_workarea_clause = -not ($safeDebugTiText -match [regex]::Escape('-work-area-phys 0xF9000000 -work-area-size 1024'))
    safe_debug_ti_is_source_plus_only_workarea_substitution = ($safeDebugTiText -eq $debugTiText.Replace('-work-area-phys 0xF9000000 -work-area-size 1024', '-work-area-phys 0xF9000C00 -work-area-size 1024'))
    official_ti_template_uses_ftdi_ti = ($tiLaunchText -match 'openocd/ftdi_ti\.cfg')
    official_ti_template_uses_debug_ti = ($tiLaunchText -match 'openocd/debug_ti\.cfg')
    official_ti_template_debug_in_ram = ($tiLaunchText -match 'doDebugInRam" value="true"')
    official_ti_template_loads_image = ($tiLaunchText -match 'loadImage" value="true"')
    official_ti_template_loads_symbols = ($tiLaunchText -match 'loadSymbols" value="true"')
    official_ti_template_does_not_set_pc = ($tiLaunchText -match 'setPcRegister" value="false"')
    official_ti_template_stops_at_main = ($tiLaunchText -match 'setStopAt" value="true"' -and $tiLaunchText -match 'stopAt" value="main"')
    official_ti_template_sets_cputapid = ($tiLaunchText -match 'CPUTAPID')
}

$result = if (-not $Apply) {
    'DRY_RUN_NO_BSP_GENERATION'
} elseif ($validation.profile_jtag_type_is_fpga_user_tap -and $validation.profile_tap_is_user2_9 -and $validation.profile_device_is_tj375n529 -and $validation.debug_ti_sets_bscan_tunnel_ir_9 -and -not $validation.debug_ti_has_unresolved_placeholder -and $validation.ftdi_ti_vid_pid_is_0403_6011 -and $validation.ftdi_ti_channel_is_1 -and $validation.target_xml_device_is_tj375n529 -and $validation.official_tj375n529_jtag_id_is_006a0ef3 -and $validation.ftdi_ti_supports_predefined_cputapid -and $validation.ftdi_ti_default_cputapid_is_ti375_006a0a79 -and $validation.hello_map_entry_is_f9000000 -and $validation.hello_map_image_end_is_f9000a30 -and $validation.safe_debug_ti_exists -and $validation.debug_ti_has_exactly_three_overlapping_workarea_clauses -and $validation.safe_debug_ti_has_exactly_three_tail_workarea_clauses -and $validation.safe_debug_ti_has_no_overlapping_workarea_clause -and $validation.safe_debug_ti_is_source_plus_only_workarea_substitution -and $validation.official_ti_template_uses_ftdi_ti -and $validation.official_ti_template_uses_debug_ti -and $validation.official_ti_template_debug_in_ram -and $validation.official_ti_template_loads_image -and $validation.official_ti_template_loads_symbols -and $validation.official_ti_template_does_not_set_pc -and $validation.official_ti_template_stops_at_main -and -not $validation.official_ti_template_sets_cputapid) {
    'USER2_HARD_TAP_DEBUG_SUPPORT_READY_TJ375_CPUTAPID_SAFE_DEBUG_CFG_AND_RUNTIME_PC_GATE_REQUIRED'
} else {
    'HOLD_GENERATED_DEBUG_BSP_VALIDATION_FAILED'
}

$generatedArtifacts = [ordered]@{
    debug_profile_hard_cfg = Get-OptionalArtifactIdentity -Path $profilePath
    debug_ti_cfg = Get-OptionalArtifactIdentity -Path $debugTiPath
    debug_ti_m2_safe_cfg_required = Get-OptionalArtifactIdentity -Path $safeDebugTiPath
    debug_softtap_cfg_forbidden = Get-OptionalArtifactIdentity -Path $debugSoftPath
    ftdi_ti_cfg_required = Get-OptionalArtifactIdentity -Path $ftdiTiPath
    official_ti_launch_template = Get-OptionalArtifactIdentity -Path $tiLaunchTemplate
    target_project_xml = Get-OptionalArtifactIdentity -Path $targetXml
    official_jtag_ctrl_model = Get-OptionalArtifactIdentity -Path $jtagCtrlModel
    hello_linker_map = Get-OptionalArtifactIdentity -Path $helloMapPath
    safe_workarea_post_init_cfg_obsolete = Get-OptionalArtifactIdentity -Path $safeWorkAreaCfgPath
    external_cfg = Get-OptionalArtifactIdentity -Path $externalCfgPath
    jtag_daemon_cfg = Get-OptionalArtifactIdentity -Path $jtagDaemonCfgPath
}

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$evidencePath = Join-Path (Resolve-Path $EvidenceDir).Path ("{0}_{1}.json" -f $Label, $timestamp)
$evidence = [ordered]@{
    schema = 'm2_user2_debug_bsp_v5'
    timestamp_local = (Get-Date).ToString('o')
    repo_head = $head
    apply = [bool]$Apply
    target_root = $targetResolved
    target_xml = $targetXml
    settings_path = $settingsPath
    settings_sha256 = (Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash
    normalized_boolean_parameter_count = @($settings.conf.PSObject.Properties | Where-Object { "$($_.Value)" -match "^\d+'b[01]$" }).Count
    generator = $generator
    generator_exit_code = $generatorExitCode
    generator_output = @($generatorOutput | ForEach-Object { "$_" })
    validation = $validation
    generated_artifacts = $generatedArtifacts
    serial_port_opened = $false
    uart_bytes_sent = 0
    programmer_invoked = $false
    openocd_invoked = $false
    gdb_invoked = $false
    flash_operation_invoked = $false
    user_tap_selected = $null
    result = $result
    next_action = if ($result -eq 'USER2_HARD_TAP_DEBUG_SUPPORT_READY_TJ375_CPUTAPID_SAFE_DEBUG_CFG_AND_RUNTIME_PC_GATE_REQUIRED') {
        'Use only Efinity official Titanium hard-TAP launch semantics with this ordered OpenOCD chain: first -c ''set CPUTAPID 0x006A0EF3'', then ftdi_ti.cfg, then generated debug_ti_m2_safe.cfg in place of debug_ti.cfg. The current target is TJ375N529/0x006A0EF3 while ftdi_ti.cfg otherwise defaults to Ti375/0x006A0A79. The safe debug cfg is a byte-for-byte derivative of debug_ti.cfg except it moves both target-create work areas from the Hello image start to unused on-chip RAM 0xF9000C00..0xF9000FFF before its init/halt commands execute. Do not load debug_ti.cfg or the obsolete post-init m2_user2_safe_workarea.cfg in the hardware session. Keep Debug in RAM, image/symbol load, no manual PC override, and stop at main. The legacy instr_addr=0x00001000 declaration is statically unconsumed by the safe debug cfg but is not a runtime proof; after load, verify PC is 0xF9000000..0xF9003FFF before any resume or serial action.'
    } elseif ($result -eq 'DRY_RUN_NO_BSP_GENERATION') {
        'Review target and then rerun with -Apply while all debug tooling remains closed.'
    } else {
        'Keep USER2 load HOLD and return this JSON. Do not start OpenOCD, GDB, serial, or Programmer.'
    }
}
$evidence | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $evidencePath -Encoding utf8
[PSCustomObject]@{
    result = $result
    generator_exit_code = $generatorExitCode
    target_root = $targetResolved
    profile_path = $profilePath
    debug_ti_path = $debugTiPath
    evidence_path = $evidencePath
} | ConvertTo-Json -Depth 4

if ($Apply -and $result -ne 'USER2_HARD_TAP_DEBUG_SUPPORT_READY_TJ375_CPUTAPID_SAFE_DEBUG_CFG_AND_RUNTIME_PC_GATE_REQUIRED') {
    exit 2
}
