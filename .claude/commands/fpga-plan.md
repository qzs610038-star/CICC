# FPGA 架构初设

用于重大架构方案、跨模块修改、视频链路/AXI/framebuffer/时序方案。此命令默认走只读探索，适合使用高能力模型。

## 工作规则
- 先阅读 `CLAUDE.md`、`AGENTS.md` 和必要的真实工程文件。
- 必须把 `分赛区决赛实施开发路线.md` 作为当前高层路线源：主线为 FPGA 视频前端/ROI/统计特征/OSD + 板上 CPU 识别决策与 myCobot 控制。
- 默认不改 RTL、约束、IP 配置或工程文件。
- 优先核对 `mem_test.xml`、`src/top.v`、相关 `src/` 子目录、`ip/*/settings.json`。
- 不把赛方资料包、生成 IP、构建产物或历史日志当作可随意重写对象。
- `初赛demo/2ChMIPICSI_2ChMIPIDSI_Demo_Test/` 只作为经验库和风险清单；不得把其中的识别 RTL、`DEMO_MODE`、临时脚本或旧 outflow 结论当作决赛基线。
- 不提出纯 FPGA 视觉识别主线；颜色/形状/尺寸分类、目标匹配、阈值表和参数切换默认放到板上 CPU。
- 不提出纯 FPGA myCobot 控制主线；协议封包、点位表、动作序列、安全互锁和异常处理默认放到板上 CPU。
- 涉及双通道时，必须同时说明通道 0 / 通道 1 的影响。

## 输出格式
```md
# FPGA Architecture Plan

## 目标

## 真实工程入口
- 主工程：
- 顶层：
- 相关源码：
- 相关 IP / 约束：

## 当前架构理解

## CPU/FPGA 职责边界
- FPGA：
- 板上 CPU：
- PC/外部 MCU：

## 方案

## 备选方案

## 关键模块与信号链路

## 时钟、复位、AXI、framebuffer、双通道影响

## 主要风险

## 验证计划

## Codex Review Packet
### 当前结论
### 希望 Codex 判断的问题
### 需要重点复核的文件/信号/warning
```
