# 真实摄像头源恢复记录（2026-07-13）

## 目标

将 `final_project/fpga` 顶层从合成像素验证源切回真实摄像头输入，同时保留合成源模块供后续显式 debug 构建使用。

## 修改

- `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE`：`1'b1 -> 1'b0`
  - ch1 预处理输入改为第二路摄像头 Debayer 输出 `rgb1_vs/rgb1_hs/rgb1_de/rgb1_valid/rgb1_datax2`。
- `HDMI_USE_SYNTHETIC_VERIFY`：`1'b1 -> 1'b0`
  - HDMI 输入改为 `channel_sel` 选择的 `hdmi0_bridge_*` 或 `hdmi1_bridge_*` 真实摄像头 CDC 输出。
- `channel_sel` 复位值为 `1'b1`，默认显示 ch1；SW4 稳定按下后在 ch1/ch0 间切换。

合成源 `u_synthetic_preprocess_source` 和专用 `u_synthetic_hdmi_video_cdc` 未删除，避免破坏已有 debug 验证入口；默认 mux 不再选择它们。

## 验证

静态检查：

```powershell
git diff --check
```

结果：PASS。

Efinity 2025.2 map：

```powershell
cmd /c "call D:\Efinity\2025.2\bin\setup.bat && cd /d D:\cicc_cbm_link\final_project\fpga\efinity && efx_run.bat mem_test --prj -f map --work_dir work_syn_codex_camera_source_20260713 --output_dir outflow_codex_camera_source_20260713 --timeout 600"
```

结果：PASS，map 用时约 62 秒。

资源：

| 资源 | 数量 |
|---|---:|
| EFX_ADD | 2081 |
| EFX_LUT4 | 11939 |
| EFX_FF | 10492 |
| EFX_RAM10 | 250 |
| EFX_DPRAM10 | 8 |

综合网表检查确认保留：

- `soft_mipi_rx_top_inst` / `soft_mipi_rx_top_inst1`
- `u_frame_buffer` / `u_frame_buffer1`
- `debayer_top` / `debayer_top1`
- `u_preprocess_ch1_tap`
- `u_hdmi0_video_cdc` / `u_hdmi1_video_cdc`

## Warning 与边界

- `mem_test.warn.log`：135 行。
- post-synthesis netlist：586 条 warning，包含大量未连接顶层端口；未判定为可忽略。
- 本轮未继续 PNR。当前正式工程已有 1776 个 IO 未 placement、`outpad` 内部断言失败的阻塞记录，需先修复 periphery/Interface Designer 与正式顶层 IO 导出边界。
- map 生成目录位于 `final_project/fpga/efinity/outflow_codex_camera_source_20260713/`，属于本地生成产物，不提交 Git。

## NOT VERIFIED

- PNR、setup/hold timing 与 bitstream 生成
- JTAG SRAM 下载
- 真实摄像头 MIPI 帧、DDR 写读、Debayer、预处理统计和 HDMI 画面
- SW4 双通道切换板级行为
