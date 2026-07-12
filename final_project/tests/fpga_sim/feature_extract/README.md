# feature_extract simulation

特征提取模块仿真目录。

`tb_vision_preprocess_channel.v` 是第一阶段独立自检 testbench。它不依赖摄像头、DDR 或厂商 IP，验证：

- Debayer `{B1,G1,R1,B0,G0,R0}` 两像素字节序。
- 半开 ROI 的边界和一拍两像素统计。
- RGB/Y 总和、红/蓝/黄面积、通用前景面积。
- bbox 与中心点。
- 未 ack 快照保持和 dropped frame 计数。
- ROI 无像素时仍产生带 `empty_foreground` 状态的稳定快照。

`tb_synthetic_2ppc_source.v` 验证摄像头替代源的 `vs/hs/de/valid` 基本时序和
`{B1,G1,R1,B0,G0,R0}` 打包。该源只为预处理测试提供确定输入，不接 HDMI/MIPI/DDR。

当前实现以 `de && valid` 的有效区上升沿为主划分行，同时接受 HS 上升沿作为同一行起点的交叉核验。接入 `top.v` 前仍必须用实际 Debayer 波形确认 HS 极性和时序。

有 Icarus Verilog 的环境执行：

```powershell
Set-Location final_project/tests/fpga_sim/feature_extract
.\run_vision_preprocess_iverilog.ps1
```

该脚本先运行预处理模块 testbench，再运行合成源 testbench；任一失败即返回非零。

若 Icarus 未加入 PATH，传入其 `bin` 目录：

```powershell
.\run_vision_preprocess_iverilog.ps1 -IcarusBin C:\iverilog\bin
```
