param(
    [string]$IcarusBin = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "../../..")).Path
$rtl = Join-Path $root "fpga/rtl"

if ($IcarusBin) {
    $iverilog = Join-Path $IcarusBin "iverilog.exe"
    $vvp = Join-Path $IcarusBin "vvp.exe"
} else {
    $iverilog = "iverilog"
    $vvp = "vvp"
}

$sources = @(
    (Join-Path $rtl "roi_crop/vision_stream_adapter_2ppc.v"),
    (Join-Path $rtl "roi_crop/roi_window_2ppc.v"),
    (Join-Path $rtl "feature_extract/pixel_mask_2ppc.v"),
    (Join-Path $rtl "feature_extract/feature_accumulator_2ppc.v"),
    (Join-Path $rtl "feature_extract/feature_snapshot.v"),
    (Join-Path $rtl "feature_extract/vision_preprocess_channel.v"),
    (Join-Path $PSScriptRoot "tb_vision_preprocess_channel.v")
)

function Invoke-Testbench {
    param(
        [string]$Top,
        [string[]]$TestSources
    )

    $output = Join-Path $PSScriptRoot ($Top + ".vvp")
    & $iverilog -g2012 -s $Top -o $output @TestSources
    if ($LASTEXITCODE -ne 0) { return $LASTEXITCODE }

    & $vvp $output
    return $LASTEXITCODE
}

$exitCode = Invoke-Testbench -Top "tb_vision_preprocess_channel" -TestSources $sources
if ($exitCode -ne 0) { exit $exitCode }

# The source test is kept separate from the feature test to prove the
# camera-replacement timing and 2ppc byte order independently.
$syntheticSources = @(
    (Join-Path $rtl "feature_extract/synthetic_2ppc_source.v"),
    (Join-Path $PSScriptRoot "tb_synthetic_2ppc_source.v")
)
$exitCode = Invoke-Testbench -Top "tb_synthetic_2ppc_source" -TestSources $syntheticSources
exit $exitCode
