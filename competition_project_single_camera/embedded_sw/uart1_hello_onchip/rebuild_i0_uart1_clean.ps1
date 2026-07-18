[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$DesignRoot,
    [Parameter(Mandatory)] [string]$EvidenceRoot,
    [Parameter(Mandatory)] [string]$WorkRoot,
    [Parameter(Mandatory)] [string]$EfinityRoot,
    [Parameter(Mandatory)] [string]$RiscvToolchainBin,
    [Parameter(Mandatory)] [string]$MakePath
)

$ErrorActionPreference = 'Stop'
$designSha = '6effdc3685d696cb4d33f3fbb1c449729ed72e33'
$projectRoot = Join-Path (Resolve-Path -LiteralPath $DesignRoot).Path 'competition_project_single_camera'
$scriptRoot = $PSScriptRoot
$inputOutput = Join-Path $scriptRoot 'I0_UART1_BUILD_INPUTS.sha256'

function Assert-DesignTree {
    if ((git -C $DesignRoot rev-parse HEAD).Trim() -ne $designSha) { throw 'HEAD does not equal DESIGN_SHA.' }
    if ((git -C $DesignRoot status --porcelain).Length -ne 0) { throw 'Design worktree is dirty.' }
    & git -C $DesignRoot diff --check
    if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed.' }
}

function Require-EmptyDirectory([string]$Path, [string]$Name) {
    if (Test-Path -LiteralPath $Path) { throw "$Name must not already exist: $Path" }
}

Assert-DesignTree
Require-EmptyDirectory $EvidenceRoot 'EvidenceRoot'
Require-EmptyDirectory $WorkRoot 'WorkRoot'

$inputSet = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
@('mem_test.xml','mem_test.peri.xml','constrain.sdc','debug_profile.wizard.json','src/top.v','src/apb_reg_magic.v','embedded_sw/uart1_hello_onchip/src/main.c','embedded_sw/uart1_hello_onchip/makefile') | ForEach-Object {[void]$inputSet.Add($_)}
[xml]$xml = Get-Content -LiteralPath (Join-Path $projectRoot 'mem_test.xml') -Raw
$ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable); $ns.AddNamespace('efx','http://www.efinixinc.com/enf_proj')
$xml.SelectNodes('//efx:design_file',$ns) | ForEach-Object {[void]$inputSet.Add($_.GetAttribute('name'))}
$xml.SelectNodes('//efx:ip',$ns) | ForEach-Object {$ip=$_.GetAttribute('instance_name');[void]$inputSet.Add($_.GetAttribute('path'));$_.SelectNodes('./efx:ip_src_file',$ns)|ForEach-Object {[void]$inputSet.Add(('ip/'+$ip+'/'+$_.GetAttribute('name')))}}
@('ip/EfxSapphireHpSoc_slb','embedded_sw/efx_hard_soc/software/standalone/common','embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc') | ForEach-Object {Get-ChildItem -LiteralPath (Join-Path $projectRoot $_) -Recurse -File | ForEach-Object {[void]$inputSet.Add($_.FullName.Substring($projectRoot.Length+1).Replace('\','/'))}}
$inputs=@($inputSet|Sort-Object)
foreach($relative in $inputs){if([IO.Path]::IsPathRooted($relative)-or $relative -match '(^|/)\.\.(/|$)'){throw "Unsafe input path: $relative"};$path=Join-Path $projectRoot $relative;if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing input: $relative"};if((git -C $projectRoot ls-files --error-unmatch -- $relative 2>$null).Trim() -ne $relative){throw "Untracked input: $relative"}}
$inputs|ForEach-Object {'{0}  {1}' -f (Get-FileHash -LiteralPath (Join-Path $projectRoot $_) -Algorithm SHA256).Hash,$_}|Set-Content -LiteralPath $inputOutput -Encoding ascii

$efxRun=Join-Path $EfinityRoot 'bin\efx_run.bat';$gcc=Join-Path $RiscvToolchainBin 'riscv-none-embed-gcc.exe';$readelf=Join-Path $RiscvToolchainBin 'riscv-none-embed-readelf.exe';$nm=Join-Path $RiscvToolchainBin 'riscv-none-embed-nm.exe'
foreach($tool in @($efxRun,$gcc,$readelf,$nm,$MakePath)){if(-not(Test-Path -LiteralPath $tool -PathType Leaf)){throw "Missing tool: $tool"}}
New-Item -ItemType Directory -Path $EvidenceRoot|Out-Null
$saved=@{EFINITY_HOME=$env:EFINITY_HOME;PYTHONHOME=$env:PYTHONHOME;PYTHONPATH=$env:PYTHONPATH;PATH=$env:PATH}
try {
    Remove-Item Env:EFINITY_HOME,Env:PYTHONHOME,Env:PYTHONPATH -ErrorAction SilentlyContinue;$env:PYTHONUTF8='1';$oldEap=$ErrorActionPreference;$ErrorActionPreference='Continue';Push-Location $projectRoot
    & $efxRun mem_test --prj -f compile --family Titanium --device TJ375N529 --timing_model I3 --dir (Join-Path $projectRoot 'ip\csi_rx_controller') --dir (Join-Path $projectRoot 'ip\dsi_tx') --dir (Join-Path $projectRoot 'ip\EfxSapphireHpSoc_slb') --output_dir (Split-Path -Leaf $EvidenceRoot) --work_dir (Split-Path -Leaf $WorkRoot) --timeout 7200 2>&1|Tee-Object -FilePath (Join-Path $EvidenceRoot 'efinity_console.log');$code=$LASTEXITCODE;Pop-Location;$ErrorActionPreference=$oldEap;if($code -ne 0){throw "Efinity failed: $code"}
} finally {if((Get-Location).Path -eq $projectRoot){Pop-Location};$env:EFINITY_HOME=$saved.EFINITY_HOME;$env:PYTHONHOME=$saved.PYTHONHOME;$env:PYTHONPATH=$saved.PYTHONPATH;$env:PATH=$saved.PATH}
& git -C $DesignRoot checkout -- 'competition_project_single_camera/mem_test.xml';if($LASTEXITCODE -ne 0){throw 'Failed to restore Efinity XML metadata.'}
$hello=Join-Path $projectRoot 'embedded_sw\uart1_hello_onchip';$env:PATH=$RiscvToolchainBin+';'+(Split-Path -Parent $MakePath)+';'+$saved.PATH
try {Push-Location $hello;& $MakePath clean;if($LASTEXITCODE -ne 0){throw 'Hello clean failed.'};& $MakePath 2>&1|Tee-Object -FilePath (Join-Path $EvidenceRoot 'uart1_hello_build.log');if($LASTEXITCODE -ne 0){throw 'Hello build failed.'};Pop-Location} finally {if((Get-Location).Path -eq $hello){Pop-Location};$env:PATH=$saved.PATH}
$elf=Join-Path $hello 'build\uart1_hello_onchip.elf';& $readelf -h -l $elf 2>&1|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'uart1_hello_readelf.txt') -Encoding ascii;$u=& $nm -u $elf 2>&1;$u|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'uart1_hello_undefined.txt') -Encoding ascii;if(-not(Test-Path -LiteralPath (Join-Path $EvidenceRoot 'uart1_hello_undefined.txt'))){New-Item -ItemType File -Path (Join-Path $EvidenceRoot 'uart1_hello_undefined.txt')|Out-Null}
Assert-DesignTree
foreach($line in Get-Content -LiteralPath $inputOutput){if($line -notmatch '^(?<hash>[0-9A-F]{64})  (?<path>.+)$'){throw "Invalid input line: $line"};if((Get-FileHash -LiteralPath (Join-Path $projectRoot $Matches.path) -Algorithm SHA256).Hash -ne $Matches.hash){throw "Input drift: $($Matches.path)"}}
"I0_UART1_REBUILD=PASS design_sha=$designSha inputs=$($inputs.Count)"
