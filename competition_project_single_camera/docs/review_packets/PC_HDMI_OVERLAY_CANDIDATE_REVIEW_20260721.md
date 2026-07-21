# PC HDMI Overlay 候选审查包（2026-07-21）

## 结论

本候选只建立 PC 端视频显示、动态前景框和启发式识别文字的无板卡开发路径。
当前状态为 `HOST_SYNTHETIC_PREPARED`；由于板卡被 libaoxun 用于 CPU Hello 排障，
且本机未枚举到 HDMI-UVC 采集设备，`HDMI_UVC=BLOCKED_NO_CAPTURE_DEVICE`。

## 固定基线与隔离

| 项目 | 固定值 |
|---|---|
| 基线 | `main` / `9acf4d8b2ec788ccd5777f3833a7bfb756c51cad` |
| 分支 | `codex/pc-hdmi-overlay-demo-20260721` |
| 工作树 | `D:\CICC-pc-hdmi-demo-20260721` |
| 实现提交 | `cd1ff13615080e693105a770dd64630694f8ca77` |
| CPU-Hello 候选 | 不修改、不依赖、不摘取 |
| libaoxun 板卡工作 | 不访问、不打断 |

## 本次范围

- 输入：确定性合成帧、视频文件或 Windows UVC 摄像头。
- 处理：背景差分、最大轮廓、动态 bbox、颜色/形状/尺寸启发式标签、5 帧多数稳定。
- 输出：PC 窗口或 headless 截图和 JSON 运行摘要。
- 固定醒目标识：`PC VISUAL DEMO`、`ARM DISABLED`、
  `BOARD RECOGNITION NOT VERIFIED`。

## 明确排除

- 不修改 RTL、XML/peri.xml、SDC、IP、BSP、CPU 固件、APB 或 OSD；
- 不访问串口，不导入 `pymycobot`，不连接或控制机械臂；
- 不把本机摄像头、视频文件或合成帧结果写成 HDMI/FPGA/板级证据；
- 不宣称识别准确率、比赛计分闭环或正式架构完成。

## 无板卡验证

| 检查 | 新鲜结果 |
|---|---|
| `python -m unittest discover -s tests -v` | `5/5 PASS` |
| Python compileall | PASS |
| 合成源 headless 回放 | `180` 帧，`135` 帧检出，退出码 `0` |
| 运行摘要边界 | `ARM=0`、`BOARD_VERIFIED=false`、`HDMI_UVC_VERIFIED=false` |
| 禁止能力/路径扫描 | 未发现串口、机械臂、MMIO、RTL、XML、SDC、IP 或 CPU bring-up 路径 |
| `git diff --check` | PASS |

生成图已目视确认包含动态 bbox、识别文字、来源标签和醒目的非板级验证横幅。
测试仅使用确定性合成帧，没有打开本机摄像头，也没有访问板卡。

## 当前待验证项

1. USB HDMI 采集卡的设备身份、实际分辨率和 FPS；
2. 板卡 HDMI OUT 是否可被采集卡连续读取；
3. 实际场景背景差分阈值、框稳定性与端到端延迟；
4. 采集卡拔出时是否 fail-closed，并清除旧识别结果；
5. 10 分钟连续显示与现场截图/录像。

以上项目必须等板卡和 HDMI-UVC 采集卡可用后另立新鲜运行记录，不得由合成帧外推。
