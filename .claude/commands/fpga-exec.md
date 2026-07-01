# FPGA 具体执行

用于局部 RTL、testbench、脚本、工程配置或文档同步。此命令适合使用廉价执行模型，但必须小步推进并保留验证证据。

## 执行规则
- 先确认本轮只解决一个明确问题。
- 先核对本轮修改是否符合 `分赛区决赛实施开发路线.md`：FPGA 做视频前端/ROI/统计特征/OSD，板上 CPU 做视觉分类、参数管理和 myCobot 控制。
- 修改前列出影响范围和计划改动。
- 优先改问题所属子系统，只有系统连线变化时才改 `src/top.v`。
- 不做无关格式化或生成文件重排。
- 不直接改 `ipm/`、赛方补丁、原始压缩包、波形、`outflow/` 等生成产物。
- 不直接迁移初赛 demo 的识别 RTL、`DEMO_MODE`、临时脚本或硬编码路径。
- 不把颜色/形状/尺寸分类和 myCobot 动作状态机写回纯 RTL；如确需硬件加速，只能作为 FPGA 特征统计或串口/FIFO/寄存器通道。
- 涉及双通道、时钟、复位、AXI、framebuffer、约束或工程文件时，执行后必须生成 Codex Review Packet。
- 涉及 QCRV32、BSCAN/JTAG、CPU DDR/AXI、`axi_reg_file`、`results_cdc`、CPU/OSD 回写、机械臂动作或串口接线时，执行后必须生成 Codex Review Packet。
- 同一问题连续两轮失败时，停止继续试补丁，转为请求 Codex 审查。

## 输出格式
```md
# FPGA Execution Log

## 本轮目标

## 修改前判断
- 影响范围：
- 计划修改：
- 需要保护的文件：
- CPU/FPGA 职责边界是否保持：

## 实际修改
- 文件：
- 摘要：

## 已运行验证
- 命令：
- 结果：
- 日志位置：
- 关键 warning：

## 未验证项

## 是否需要 Codex Gate
- 结论：
- 原因：
```
