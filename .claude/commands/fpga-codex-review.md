# 生成 Codex 审查包

用于把 Claude 的方案、修改或调试状态交给 Codex 高能力模型独立复核。审查包必须短而完整，能让 Codex 直接回到真实文件和命令验证。

## 使用时机
- 架构初设完成后。
- 修改跨越两个以上子系统。
- 涉及 `src/top.v`、时钟、复位、视频时序、AXI、framebuffer、双通道同步。
- 修改 `constrain.sdc`、`mem_test.xml`、`.peri.xml` 或 IP `settings.json`。
- 涉及 QCRV32、BSCAN/JTAG、CPU DDR/AXI、`axi_reg_file`、`results_cdc`、CPU/OSD 回写链路。
- 涉及视觉识别职责边界，尤其是是否要把颜色/形状/尺寸分类放回纯 FPGA RTL。
- 涉及 myCobot 协议、动作状态机、点位表、安全互锁、FPGA-to-机械臂 UART/GPIO 或任何会让机械臂运动的命令。
- warning 被认为可忽略。
- 同一问题连续两轮调试失败。
- 用户要求判断方案是否合理可行。

## 输出格式
```md
# Codex Review Packet

## 任务目标

## 当前结论

## 修改或计划涉及的文件
- 文件：
- 是否已修改：
- 修改摘要：

## 关键模块与信号链路

## CPU/FPGA 职责边界
- FPGA：
- 板上 CPU：
- PC/外部 MCU：
- 是否偏离 `分赛区决赛实施开发路线.md`：

## 时钟、复位、AXI、framebuffer、双通道影响

## 机械臂 / 外设状态
- myCobot 是否涉及：
- COM 口 / CP210x 状态：
- 波特率：
- 是否执行动作：
- 安全确认：

## 已运行验证
- 命令：
- 工作目录：
- 结果：
- 日志位置：
- 关键 warning / error：

## 未验证项和风险假设

## 希望 Codex 判断的问题
1.
2.
3.

## 建议 Codex 优先查看
- 文件：
- 命令：
- 日志：
```
