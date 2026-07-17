# 2026-07-17 板卡/协作依赖执行方案

> 目的：让无板成员准备可审计的材料，让持板成员只执行当前允许的最小 Gate。本文不是烧录、接线、串口或动作授权。

## 0. 当前约束

- 当前允许的下一 Gate 仅为：匹配 bitstream、FPGA `USER2`、将匹配 ELF 下载到 `0xF9000000` 片上 RAM、验证 UART0 `115200` Hello。
- 当前禁止：`USER1`、Flash、外部 DDR、UART2/J52、机械臂接线、myCobot 帧和任何动作。
- UART0 `115200` 与 myCobot `1000000` 是两条独立 Gate；前者成功不能授权后者。

事实来源：根目录 `CURRENT_STATE.md`、`competition_project_single_camera/docs/review_packets/G1_EFINITY_COLD_BUILD_REVIEW_PACKET_20260717_0307.md` 与当前匹配生成物。

## E-COORD-01：用户/队友协作的状态维护闭环

**需要的用户协助**：指定谁负责合并/记录板测证据，并确认是“先修当前过期 handoff/治理文档”还是“等本轮 USER2 证据落盘后统一刷新”。

### 待处理事项

| 事项 | 当前症状 | 负责人建议 | 完成证据 |
|---|---|---|---|
| SESSION_HANDOFF 刷新 | 仍称当前 SHA 未重新跑 Efinity | 文档维护者 + 板测负责人复核 | current SHA、G1 离线 PASS、板级 NOT VERIFIED、下一 Gate 一致 |
| Merge register 补记 | 未覆盖 `9acf4d8` 的上下文合并 | 合并负责人 | 一行登记 + 对应记录，不虚构新硬件批次 |
| freshness WARN=4 处理 | 四份受管入口触发更新提示 | 文档维护者 | `WARN=0`，或逐项记录为何暂保留 WARN |
| 图谱刷新 | artifact 基线早于当前 HEAD | 代码维护者 | artifact commit/节点数与本次结构变更范围一致 |

### 停止条件

若板测队友正在写 `CURRENT_STATE.md`、handoff 或证据文件，任何人不得并行覆盖；在个人分支形成 diff/Review Packet，等待其证据落盘后语义合并。

## E-BOARD-01：USER2 + UART0 最小板级证据

**Agentmemory action**：`act_mrok18lp_e5d9ecf01840`
**外部 checkpoint**：`ckpt_mrok1b6g_bb0e0158065f`
**物理执行人**：持板队友；无板成员只负责 preflight 和证据审查。

### Preflight（无板成员可准备）

1. 固定本次 HEAD、bitstream SHA-256、ELF SHA-256、ELF LOAD/entry 地址和 warning 分类；
2. 核对操作卡与当前 PC/制品 hash，不一致即停止；
3. 预置三次启动的原始记录表：时间、操作者、制品 hash、USER2 选择、RAM 地址、UART0 原始字节、异常/warning；
4. 预写回退条件：hash/warning/制品身份不一致，立即停止并退回 G1 冷构建证据核对。

### 持板队友的最小操作范围

| 次序 | 允许动作 | 必须记录 | 立即停止 |
|---|---|---|---|
| 1 | 仅核对匹配制品与连接状态 | SHA-256、warning、操作者 | 任一身份不一致 |
| 2 | 仅选择 `USER2`，RAM 下载匹配 ELF 至 `0xF9000000` | 下载输出与地址 | 误选 USER1、Flash 或 DDR 路径 |
| 3 | 仅观察 UART0 `115200` Hello/回显 | 原始串口证据、三次启动结果 | 无输出、乱码、异常 reset 或错误制品 |

### 合格与不合格

- 合格：当前批次证据完整，且仅可据此关闭 CPU 取指/UART0 子门；必须另立 Review Packet 才能讨论 APB/MMIO。
- 不合格：任一制品或行为不符，只能记录 `NOT VERIFIED`，不可从旧 bitstream、Host 测试或 PC 工具补推结论。

## E-BOARD-02：UART2/J52 及机械臂后续门（当前 HOLD）

| Gate | 必须先有的外部条件 | 未来唯一允许的最小验证 | 当前状态 |
|---|---|---|---|
| G7 | 正式 UART2 HAL/ABI、3.3 V 针序双签、限流回环、J52/机械臂物理隔离 | 板内回环 100/100 | HOLD |
| G8 | 机械臂 TX 电压兼容或隔离证据、共地/线序确认、只读 Review Packet | `GET_ANGLES` 白名单、≤1 Hz、30/30 | HOLD |
| G9 | UART 事件来源、PLIC 或有界 polling 审查 | 只读 dry-run | HOLD |
| G10/G11 | 固定、净空、急停、点位与独立动作 Review Packet | 低速空载单段后逐级扩大 | NO-GO |

无板成员不得提前实现 REAL backend、猜 UART2 地址、连接 J52，或把协议单测解释为电气兼容。
