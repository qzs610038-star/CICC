# final_project Current Code Architecture

> 日期：2026-07-13
>
> 基线：`codex/team-integration-20260713@510caca`
>
> 本文是两位队友分支与个人工作区整合后的代码/证据快照。真实 RTL、CPU 源码、Efinity 工程 XML、构建日志和上板现象优先；最新路线与阻塞以根目录 `CURRENT_STATE.md` 为准。2026-07-11 版本保留为历史快照。

## 系统职责边界

```text
真实或合成像素源
  -> FPGA: CSI / framebuffer / debayer / ROI / frame statistics
  -> APB/CDC candidate boundary
  -> CPU: classifier / four-task matcher / per-round transaction
  -> FPGA: result semantics / OSD rendering / HDMI
  -> CPU UART: myCobot protocol / guarded action sequence
```

- FPGA 只负责视频前端、ROI/统计特征、OSD 像素渲染和硬件通道；不承载四任务判定或机械臂动作状态机。
- 板上 CPU 负责五色/三形状/三尺寸分类、四任务关系、逐轮事务、参数管理和 myCobot 控制。
- PC `pymycobot`、Host replay 和合成源仅用于开发期验证，不进入正式比赛闭环。

## 当前代码分层

| 层 | 当前实现 | 已验证范围 | 未闭环边界 |
|---|---|---|---|
| FPGA 视频/预处理 | `vision_preprocess_channel`、ROI/统计 RTL；`synthetic_2ppc_source` 已在 `top.v` 提供验证输入 | 整合工程 Efinity map PASS | 合成选择当前由顶层常量启用；尚未拆分 production/debug profile；真实 CSI、PNR、bitstream、板级帧均未通过 |
| CPU 识别 | `vision_classifier`、`param_table`、五色/四任务 `task_matcher` | Host 回归与 A13 snapshot replay 已覆盖 | 白/黑仍需要真实 `LIVE_FG_AREA`；板上目标入口仍是旧红/蓝/黄候选 |
| 完整逐轮编排 | `round_controller`：CONFIG/放置/识别/判定/等待机械臂/移除/故障等完整状态 | 独立 Host 状态机和 20 轮无死锁测试 | 尚未接入 `main.c`、正式事件源、arm done/ACK |
| 轻量事务层 | `competition_round_transaction`：观测、event_seq、ACK、超时、放弃 | contract/host-flow/snapshot replay | 与正式 APB 寄存器的事件字段尚未冻结 |
| 主循环 | `main.c` 仍直接读取 `board_io`、分类并调用 matcher，写回两路结果 | 关键源文件 RISC-V strict compile-only | 尚未调用上述两层逐轮控制器；仍输出 `ARM_STATE_IDLE` |
| FPGA↔CPU 接口 | `board_io.h` 有双通道候选偏移；`integration/register_map.md` 已同步为候选契约 | Host/mock 可编译测试 | 无正式 `soc.h`、APB slave、CDC RTL、正式地址和板上 MAGIC/heartbeat |
| OSD | CPU 已有结果与 bbox writeback 数据结构 | 仅软件语义 | 无正式 CPU→OSD CDC、帧边界提交或板级显示证据 |
| myCobot | 纯 C protocol、transport、arm controller 与 PC 点位/日志存在 | Host/QEMU 骨架与 PC 开发期诊断 | 无 UART2 真源/驱动、电平/线序签核；底座微倾未关闭；真实动作 NO-GO |

## 当前验证基线

| Gate | 结果 | 解释边界 |
|---|---|---|
| Host CPU/competition tests | 795/795 PASS | 只证明 Host 契约与纯 C 行为 |
| RISC-V compile-only | 8 个关键源，`-Wall -Wextra -Werror` PASS | 不等于固件链接、烧录或板上运行 |
| Efinity map | PASS；ADD 1827 / LUT4 10339 / FF 7991 / RAM10 154 | 仅 map；warning 未签核 |
| Efinity PNR | FAIL；1,776 IO 无 placement 后 `outpad` 断言 | 无时序签核、bitstream 或上板证据 |
| myCobot 实机 | 本轮未发送动作命令 | D1/D2 可推进；真实臂只读/动作仍受 T0/D0–D5 门禁 |

## 当前关键分叉

### FPGA 构建

`top.v` 当前同时令 `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE=1'b1`、`HDMI_USE_SYNTHETIC_VERIFY=1'b1`。下一步不是删除合成源，而是建立可审查的 production/debug 两套入口：production 默认真实输入，debug 才显式启用合成验证。

PNR 必须从 Interface Designer/periphery、正式顶层端口、普通/debug VDB 与 `.peri.xml` 一致性排查；禁止给 1,776 个端口批量盲绑管脚。

### CPU 与接口

完整 `round_controller` 负责操作员事件和机械臂等待等逐轮编排；轻量 `competition_round_transaction` 保留 event_seq/ACK 与单轮观察事务。两者职责已拆层，但尚未进入 `main.c`。下一步先在 `ARM_DISABLED` 下接入，不发送真实动作。

`register_map.md` 现与 `board_io.h` 的候选偏移对齐，但正式地址仍只来自同一次 SoC 生成的 `soc.h`。帧统计扩展字段、五色目标输入、任务模式、理由码和全局提交仍需 Review Packet 分配槽位。

## 下一最小 Gate

1. 修正文档与接口索引，完成 Codebase Memory 图谱刷新。
2. 拆分 FPGA production/debug 构建入口并形成 Review Packet。
3. 关闭 periphery/IO 导出问题，重跑 PNR、时序与回归。
4. 在隔离 SoC 路径先完成 CPU Hello、APB MAGIC 和 heartbeat。
5. 在 `ARM_DISABLED` 下完成 `main -> round_controller -> result/OSD semantics` 的 20 轮流程。
6. T0、D1/D2 和机械结构复核未完成前，不连接真实机械臂控制线、不发送动作帧。

## 证据入口

- 当前状态：`../../../CURRENT_STATE.md`
- 团队整合 Review Packet：`../review_packets/team_integration_merge_review_20260713.md`
- CPU 模块与测试：`../../cpu/CPU_MODULE_PLAN.txt`
- FPGA/CPU 候选寄存器契约：`../../integration/register_map.md`
- SoC/APB 缺口：`generated_soc_summary_2026-07-11.md`
- myCobot 板上 SOP：`../technical_plans/mycobot_board_bringup_operator_sop_20260712.md`
