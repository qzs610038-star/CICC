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
$output = Join-Path $PSScriptRoot "tb_vision_preprocess_channel.vvp"

& $iverilog -g2012 -s tb_vision_preprocess_channel -o $output @sources
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $vvp $output
exit $LASTEXITCODE
