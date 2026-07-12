# A12 白黑统计快照探针执行记录（隔离工程）

## 目的与范围

在摄像头数据流不稳定期间，为已完成的 A11 合成五色预处理增加可独立复核白色与黑色的基础统计证据。

- 隔离工程：`C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga`
- 未修改：`D:\final_project`、仓库正式 FPGA 基线、`.peri.xml`、SDC、SoC/APB/CPU/OSD 或机械臂路径。
- 本次仅保留 FPGA 的帧级 RGB/亮度统计供调试读取；不在 RTL 内新增白黑分类或任何比赛任务决策。

## RTL 改动

`rtl/top/top.v` 原先已实例化的 `vision_preprocess_channel` 会输出 `o_sum_r`、`o_sum_g`、`o_sum_b`、`o_sum_y`，但四个端口此前未接线。

本次将它们连接至四根 `mark_debug` 的 32 位顶层网络：

| 调试网 | 含义 |
|---|---|
| `preprocess_ch1_sum_r` | 当前完整 ROI 的 R 通道累计值 |
| `preprocess_ch1_sum_g` | 当前完整 ROI 的 G 通道累计值 |
| `preprocess_ch1_sum_b` | 当前完整 ROI 的 B 通道累计值 |
| `preprocess_ch1_sum_y` | 当前完整 ROI 的亮度累计值，逐像素定义为 `R + G + B` |

这四个信号来自现有帧快照寄存器，时钟域仍为 `i_sysclk_div2`。它们不驱动 HDMI、CSI、DDR、外部管脚或任何控制接口。

## A12 Map 验证

命令：

```powershell
cmd /c "call D:\Efinity\2025.2\bin\setup.bat && cd /d C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity && efx_run --prj -f map --output_dir outflow_a12 --work_dir work_syn_a12 mem_test.xml"
```

结果：`map : PASS`，Efinity `2025.2.288.4.15`。

| 资源 | A11 v2 | A12 | 结果 |
|---|---:|---:|---|
| `EFX_ADD` | 1827 | 1827 | 无变化 |
| `EFX_LUT4` | 10339 | 10339 | 无变化 |
| `EFX_FF` | 7991 | 7991 | 无变化 |
| map warning 数 | 134 | 134 | 无新增 |

证据：`C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\outflow_a12\mem_test.map.out`。

## 白黑预期值

合成源是 `960 x 1080` ROI，灰色背景为每通道 `128`，中心方块为 `320 x 320 = 102400` 像素。

| 画面 | `sum_r` | `sum_g` | `sum_b` | `sum_y` |
|---|---:|---:|---:|---:|
| 白色方块 | 145715200 | 145715200 | 145715200 | 437145600 |
| 黑色方块 | 119603200 | 119603200 | 119603200 | 358809600 |

白色相对黑色每个 RGB 通道相差 `26112000`，亮度累计相差 `78336000`。该差异来自同一面积的方块像素，能在不依赖 HDMI 人工观察的情况下使调试快照区分白与黑。

## 后续 GUI 操作门禁

当前 A11 `.dbg.vdb` 仅包含旧的 11 根探针，不能用于 A12。禁止手工编辑或伪造 `.dbg.vdb`。

必须在 Efinity Debug Wizard 中打开隔离工程的 A12 网表，保持 `JTAG_USER2`，保留原 11 根探针，并新增上述 4 根 32 位网络。由 Wizard 重新生成 `mem_test.dbg.vdb` 后，才允许对 A12 运行 PNR、生成 bitstream、手动 JTAG SRAM 下载，以及分别采集白色和黑色帧。

## Board Capture: Black Frame

At `20:59:26`, the A12 15-probe debugger capture reported:

| Probe | Captured value | Expected black | Result |
|---|---:|---:|---|
| `sum_r` / `sum_g` / `sum_b` | `119603200` each | `119603200` each | PASS |
| `sum_y` | `358809600` | `358809600` | PASS |
| red / blue / yellow area | `0` each | `0` each | PASS |
| `fg_area` | `102400` | `102400` | PASS |
| ROI / bbox / center / status | prior expected values | prior expected values | PASS |

## Board Capture: White Frame

At `21:03:42`, a later A12 capture reported:

| Probe | Captured value | Expected white | Result |
|---|---:|---:|---|
| `sum_r` / `sum_g` / `sum_b` | `145715200` each | `145715200` each | PASS |
| `sum_y` | `437145600` | `437145600` | PASS |
| red / blue / yellow area | `0` each | `0` each | PASS |
| `fg_area` | `102400` | `102400` | PASS |
| `roi_pixel_count` | `1036800` | `1036800` | PASS |
| bbox / center / status | `{380,320}..{699,639}` / `{539,479}` / `0` | same | PASS |

The VCD path is reused, so it now contains the white capture. The preceding black values remain recorded above. White and black are therefore independently distinguished by FPGA frame statistics, without treating an HDMI observation as the color identity.

## Observation: HDMI/Preprocess Sampling Phase

Two user-triggered captures made while HDMI was visually white still contained the black snapshot values. The later rapid captures reached the expected white values. The HDMI branch crosses a 2ppc-to-1ppc CDC/FIFO, while preprocessing reads the source-clock snapshot; do not use the instantaneous HDMI screen color as the sole color identity for a capture. For A12, use the `sum_r/sum_g/sum_b/sum_y` values in the VCD as ground truth.

## NOT VERIFIED

- The A12 15-probe Debug Wizard configuration, PNR, timing, bitstream, JTAG SRAM download, and black/white VCD captures are complete as recorded above. A full A12 HDMI regression was not separately recorded.
- Real camera input, CPU/APB/OSD, size calibration, and robot behavior remain unverified.
