# FPGA 架构初设

用于重大架构方案、跨模块修改、视频链路/AXI/framebuffer/时序方案。此命令默认走只读探索，适合使用高能力模型。

## 工作规则
- 先阅读 `AGENTS.md`、最新官方细则、`CURRENT_STATE.md`、`CLAUDE.md` 和必要的真实工程文件。
- 先用 codebase-memory-mcp 确认 `D-cicc_cbm_link` 图谱状态，并用 `get_architecture` / `search_graph` / `search_code` 缩小代码范围。
- 官方任务、评分和时限以 `final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md` 为准；系统职责以 `AGENTS.md`「分赛区决赛系统架构硬边界」为准；当前完成度和阻塞以 `CURRENT_STATE.md` 为准。
- `分赛区决赛实施开发路线.md` 仅作历史路线图和经验库，不得单独作为当前权威来源。
- 默认不改 RTL、约束、IP 配置或工程文件。
- 优先核对 `mem_test.xml`、`src/top.v`、相关 `src/` 子目录、`ip/*/settings.json`。
- 不把赛方资料包、生成 IP、构建产物或历史日志当作可随意重写对象。
- `初赛demo/2ChMIPICSI_2ChMIPIDSI_Demo_Test/` 只作为经验库和风险清单；不得把其中的识别 RTL、`DEMO_MODE`、临时脚本或旧 outflow 结论当作决赛基线。
- 不提出纯 FPGA 视觉识别主线；颜色/形状/尺寸分类、目标匹配、阈值表和参数切换默认放到板上 CPU。
- 不提出纯 FPGA myCobot 控制主线；协议封包、点位表、动作序列、安全互锁和异常处理默认放到板上 CPU。
- 涉及正式比赛闭环时，必须说明五色/三形状/三尺寸、四任务关系判定、逐轮事务、明确结果输出、唯一机械臂响应和 20 轮时限由哪些模块承担。
- 涉及双通道时，必须同时说明通道 0 / 通道 1 的影响。

## 输出格式
```md
# FPGA Architecture Plan

## 目标

## 真实工程入口
- CBM 图谱状态：
- 主工程：
- 顶层：
- 相关源码：
- 相关 IP / 约束：

## 当前架构理解

## 权威来源与当前事实
- 官方细则：
- AGENTS 架构边界：
- CURRENT_STATE 当前状态：
- 历史路线仅借鉴内容：

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
