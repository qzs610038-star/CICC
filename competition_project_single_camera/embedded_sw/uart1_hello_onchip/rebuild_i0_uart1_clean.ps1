[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$DesignRoot,
    [Parameter(Mandatory)] [string]$EvidenceRoot,
    [Parameter(Mandatory)] [string]$OutflowRoot,
    [Parameter(Mandatory)] [string]$WorkRoot,
    [Parameter(Mandatory)] [string]$EfinityRoot,
    [Parameter(Mandatory)] [string]$RiscvToolchainBin,
    [Parameter(Mandatory)] [string]$MakePath
)

$ErrorActionPreference = 'Stop'
$designSha = '6effdc3685d696cb4d33f3fbb1c449729ed72e33'
$designParent = 'f47af290c2f014dfa8a131a3baebec1e9560ae21'
$designRoot = (Resolve-Path -LiteralPath $DesignRoot).Path
$projectRoot = Join-Path $designRoot 'competition_project_single_camera'

function Assert-DesignTree {
    if ((git -C $designRoot rev-parse HEAD).Trim() -ne $designSha) { throw 'HEAD does not equal DESIGN_SHA.' }
    if ((git -C $designRoot status --porcelain).Length -ne 0) { throw 'Design worktree is dirty.' }
    & git -C $designRoot diff --check
    if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed.' }
}

function Assert-FreshDirectory([string]$Path, [string]$Label) {
    if (Test-Path -LiteralPath $Path) { throw "$Label already exists: $Path" }
}

function Get-RelativeProjectPath([string]$Path) {
    $prefix = $projectRoot.TrimEnd('\') + '\'
    if (-not $Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Input outside project root: $Path" }
    $relative = $Path.Substring($prefix.Length).Replace('\', '/')
    if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|/)\.\.(/|$)') { throw "Unsafe input path: $relative" }
    return $relative
}

function Add-DirectoryInputs([System.Collections.Generic.HashSet[string]]$Set, [string]$RelativeDirectory) {
    $directory = Join-Path $projectRoot $RelativeDirectory
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw "Missing input directory: $RelativeDirectory" }
    Get-ChildItem -LiteralPath $directory -Recurse -File | ForEach-Object { [void]$Set.Add((Get-RelativeProjectPath $_.FullName)) }
}

function Get-ArtifactRecord([string]$Root, [string]$RelativePath, [string]$RootName) {
    $path = Join-Path $Root $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing artifact: $RootName/$RelativePath" }
    $file = Get-Item -LiteralPath $path
    return [ordered]@{ root = $RootName; path = $RelativePath.Replace('\', '/'); size = [Int64]$file.Length; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
}

Assert-DesignTree
Assert-FreshDirectory $EvidenceRoot 'EvidenceRoot'
Assert-FreshDirectory $OutflowRoot 'OutflowRoot'
Assert-FreshDirectory $WorkRoot 'WorkRoot'

# Efinity writes output relative to the project. Refuse paths outside this tree.
foreach ($path in @($OutflowRoot, $WorkRoot)) {
    $full = [IO.Path]::GetFullPath($path)
    if (-not $full.StartsWith($projectRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'OutflowRoot and WorkRoot must be below project root.' }
}

$inputs = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
@('mem_test.xml','mem_test.peri.xml','constrain.sdc','debug_profile.wizard.json','src/top.v','src/apb_reg_magic.v','embedded_sw/uart1_hello_onchip/src/main.c','embedded_sw/uart1_hello_onchip/makefile') | ForEach-Object { [void]$inputs.Add($_) }
[xml]$projectXml = Get-Content -LiteralPath (Join-Path $projectRoot 'mem_test.xml') -Raw
$ns = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable)
$ns.AddNamespace('efx', 'http://www.efinixinc.com/enf_proj')
$projectXml.SelectNodes('//efx:design_file', $ns) | ForEach-Object { [void]$inputs.Add($_.GetAttribute('name')) }
$projectXml.SelectNodes('//efx:ip', $ns) | ForEach-Object {
    $ip = $_.GetAttribute('instance_name')
    [void]$inputs.Add($_.GetAttribute('path'))
    $_.SelectNodes('./efx:ip_src_file', $ns) | ForEach-Object { [void]$inputs.Add(('ip/' + $ip + '/' + $_.GetAttribute('name'))) }
}
Add-DirectoryInputs $inputs 'ip/EfxSapphireHpSoc_slb'
Add-DirectoryInputs $inputs 'embedded_sw/efx_hard_soc/software/standalone/common'
Add-DirectoryInputs $inputs 'embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc'
$inputPaths = @($inputs | Sort-Object)
$seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
foreach ($relative in $inputPaths) {
    if (-not $seen.Add($relative)) { throw "Duplicate input: $relative" }
    $path = Join-Path $projectRoot $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing input: $relative" }
    if ((git -C $projectRoot ls-files --error-unmatch -- $relative 2>$null).Trim() -ne $relative) { throw "Untracked input: $relative" }
}

New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
$inputFile = Join-Path $EvidenceRoot 'I0_UART1_BUILD_INPUTS.sha256'
$inputPaths | ForEach-Object { '{0}  {1}' -f (Get-FileHash -LiteralPath (Join-Path $projectRoot $_) -Algorithm SHA256).Hash, $_ } | Set-Content -LiteralPath $inputFile -Encoding ascii
@("design_sha=$designSha", "design_parent=$designParent", "head=" + (git -C $designRoot rev-parse HEAD).Trim(), "status=" + ((git -C $designRoot status --porcelain) -join ''), "inputs=$($inputPaths.Count)") | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'preflight.txt') -Encoding ascii

$efxRun = Join-Path $EfinityRoot 'bin\efx_run.bat'
$efxMap = Join-Path $EfinityRoot 'bin\efx_map.exe'
$gcc = Join-Path $RiscvToolchainBin 'riscv-none-embed-gcc.exe'
$readelf = Join-Path $RiscvToolchainBin 'riscv-none-embed-readelf.exe'
$nm = Join-Path $RiscvToolchainBin 'riscv-none-embed-nm.exe'
foreach ($tool in @($efxRun, $efxMap, $gcc, $readelf, $nm, $MakePath)) { if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Missing tool: $tool" } }

$saved = @{ EFINITY_HOME=$env:EFINITY_HOME; PYTHONHOME=$env:PYTHONHOME; PYTHONPATH=$env:PYTHONPATH; PATH=$env:PATH }
try {
    Remove-Item Env:EFINITY_HOME,Env:PYTHONHOME,Env:PYTHONPATH -ErrorAction SilentlyContinue
    $env:PYTHONUTF8 = '1'
    $previousEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    Push-Location $projectRoot
    & $efxRun mem_test --prj -f compile --family Titanium --device TJ375N529 --timing_model I3 --dir (Join-Path $projectRoot 'ip\csi_rx_controller') --dir (Join-Path $projectRoot 'ip\dsi_tx') --dir (Join-Path $projectRoot 'ip\EfxSapphireHpSoc_slb') --output_dir (Split-Path -Leaf $OutflowRoot) --work_dir (Split-Path -Leaf $WorkRoot) --timeout 7200 2>&1 | Tee-Object -FilePath (Join-Path $EvidenceRoot 'efinity_console.log')
    $efinityExit = $LASTEXITCODE
    Pop-Location
    $ErrorActionPreference = $previousEap
    if ($efinityExit -ne 0) { throw "Efinity compile failed: $efinityExit" }
} finally {
    if ((Get-Location).Path -eq $projectRoot) { Pop-Location }
    $env:EFINITY_HOME=$saved.EFINITY_HOME; $env:PYTHONHOME=$saved.PYTHONHOME; $env:PYTHONPATH=$saved.PYTHONPATH; $env:PATH=$saved.PATH
}

# The Efinity run rewrites project metadata; restore committed bytes before drift checks.
& git -C $designRoot checkout -- 'competition_project_single_camera/mem_test.xml'
if ($LASTEXITCODE -ne 0) { throw 'Failed to restore mem_test.xml.' }

$helloRoot = Join-Path $projectRoot 'embedded_sw\uart1_hello_onchip'
$env:PATH = $RiscvToolchainBin + ';' + (Split-Path -Parent $MakePath) + ';' + $saved.PATH
try {
    Push-Location $helloRoot
    & $MakePath clean
    if ($LASTEXITCODE -ne 0) { throw 'Hello clean failed.' }
    & $MakePath 2>&1 | Tee-Object -FilePath (Join-Path $EvidenceRoot 'uart1_hello_build.log')
    if ($LASTEXITCODE -ne 0) { throw 'Hello build failed.' }
    Pop-Location
} finally {
    if ((Get-Location).Path -eq $helloRoot) { Pop-Location }
    $env:PATH=$saved.PATH
}

$elf = Join-Path $helloRoot 'build\uart1_hello_onchip.elf'
& $readelf -h -l $elf 2>&1 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'uart1_hello_readelf.txt') -Encoding ascii
$undefined = & $nm -u $elf 2>&1
$undefined | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'uart1_hello_undefined.txt') -Encoding ascii
if (-not (Test-Path -LiteralPath (Join-Path $EvidenceRoot 'uart1_hello_undefined.txt'))) { New-Item -ItemType File -Path (Join-Path $EvidenceRoot 'uart1_hello_undefined.txt') | Out-Null }

Assert-DesignTree
foreach ($line in Get-Content -LiteralPath $inputFile) {
    if ($line -notmatch '^(?<hash>[0-9A-F]{64})  (?<path>.+)$') { throw "Invalid input line: $line" }
    if ((Get-FileHash -LiteralPath (Join-Path $projectRoot $Matches.path) -Algorithm SHA256).Hash -ne $Matches.hash) { throw "Input drift: $($Matches.path)" }
}
@("design_sha=$designSha", "head=" + (git -C $designRoot rev-parse HEAD).Trim(), "status=" + ((git -C $designRoot status --porcelain) -join ''), 'post_input_hashes=PASS') | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'postflight.txt') -Encoding ascii

$artifactSpec = @(
    @{root='evidence';path='efinity_console.log';base=$EvidenceRoot}, @{root='outflow';path='mem_test.log';base=$OutflowRoot}, @{root='outflow';path='mem_test.map.out';base=$OutflowRoot}, @{root='outflow';path='mem_test.map.rpt';base=$OutflowRoot}, @{root='outflow';path='mem_test.warn.log';base=$OutflowRoot}, @{root='outflow';path='mem_test.pt.rpt';base=$OutflowRoot}, @{root='outflow';path='mem_test.interface.csv';base=$OutflowRoot}, @{root='outflow';path='mem_test.pinout.rpt';base=$OutflowRoot}, @{root='outflow';path='mem_test.route.rpt';base=$OutflowRoot}, @{root='outflow';path='mem_test.timing.rpt';base=$OutflowRoot}, @{root='outflow';path='mem_test.cdc.rpt';base=$OutflowRoot}, @{root='outflow';path='mem_test.pgm.out';base=$OutflowRoot}, @{root='outflow';path='mem_test.bit';base=$OutflowRoot}, @{root='outflow';path='mem_test.hex';base=$OutflowRoot}, @{root='work';path='mem_test.lbf';base=$WorkRoot}, @{root='project';path='embedded_sw/uart1_hello_onchip/build/uart1_hello_onchip.elf';base=$projectRoot}, @{root='evidence';path='uart1_hello_build.log';base=$EvidenceRoot}, @{root='evidence';path='uart1_hello_readelf.txt';base=$EvidenceRoot}, @{root='evidence';path='uart1_hello_undefined.txt';base=$EvidenceRoot}, @{root='evidence';path='preflight.txt';base=$EvidenceRoot}, @{root='evidence';path='postflight.txt';base=$EvidenceRoot}
)
$artifacts = @($artifactSpec | ForEach-Object { Get-ArtifactRecord $_.base $_.path $_.root })
$toolSpec = @(@{name='efx_run.bat';root='efinity';relative_path='bin/efx_run.bat';full_path=$efxRun}, @{name='efx_map.exe';root='efinity';relative_path='bin/efx_map.exe';full_path=$efxMap}, @{name='riscv-none-embed-gcc.exe';root='riscv';relative_path='riscv-none-embed-gcc.exe';full_path=$gcc}, @{name='make.exe';root='make';relative_path='make.exe';full_path=$MakePath})
$tools = @($toolSpec | ForEach-Object {$f=Get-Item -LiteralPath $_.full_path;[ordered]@{name=$_.name;root=$_.root;relative_path=$_.relative_path;size=[Int64]$f.Length;sha256=(Get-FileHash -LiteralPath $_.full_path -Algorithm SHA256).Hash}})
$warningText=Get-Content -LiteralPath (Join-Path $OutflowRoot 'mem_test.warn.log') -Raw
$manifest=[ordered]@{batch_id='I0_UART1_20260719_CLEAN_LF_FINAL';design_sha=$designSha;design_parent=$designParent;eol='LF clean checkout with core.autocrlf=false';tool=[ordered]@{name='Efinity';version='2025.2.288.4.15';family='Titanium';device='TJ375N529';timing_model='I3'};toolchain_files=$tools;artifacts=$artifacts;warnings=[ordered]@{EFX_0011=([regex]::Matches($warningText,'EFX-0011 VERI-WARNING')).Count;EFX_0200=([regex]::Matches($warningText,'EFX-0200 WARNING')).Count;EFX_0201=([regex]::Matches($warningText,'EFX-0201 WARNING')).Count;EFX_0256=([regex]::Matches($warningText,'EFX-0256 WARNING')).Count;ERROR_OR_FATAL=([regex]::Matches($warningText,'EFX ERROR|EFX FATAL')).Count};non_claims=@('USER2, UART1 terminal I/O, APB MAGIC, ch0/HDMI board regression, UART2/J52, and myCobot remain NOT VERIFIED.')}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'I0_UART1_BUILD_MANIFEST.json') -Encoding ascii
"I0_UART1_REBUILD=PASS batch=$($manifest.batch_id) design_sha=$designSha inputs=$($inputPaths.Count) artifacts=$($artifacts.Count)"
