# Codex Review Packet — CPU → APB/OSD 结果打包设计

> **状态：APPROVED BY CODEX2 — DESIGN PACKET ONLY**
> 日期：2026-07-15
> 来源 Agent：Claude（Fable 5，执行员工）
> 适用范围：`cpu_display_result_t` → 未来 APB/OSD 寄存器的逻辑字段语义映射设计
> 不修改任何 C/RTL、main.c、board_io、register_map、CURRENT_STATE、CPU_MODULE_PLAN、XML、SDC 或 IP 配置。
>
> **批准边界**：
> - Codex2 最终复审未发现 P0/P1/P2/P3。
> - **仅批准** CPU→APB/OSD 逻辑契约（本 Review Packet 范围内的设计文档）。
> - **不批准**：独立复位恢复协议（§6.3.B 仍 TBD）、正式 wire ABI、APB/CDC RTL、OSD 实现、SoC 集成、PNR、bitstream 或板级闭环。
> - **不批准**：UART2、J52 或机械臂动作。
> - §6.3.B 和 §9 继续阻塞 CDC RTL 实现。
> - 所有 PROPOSED/TBD、未验证项和 FPGA/SoC 联合确认事项（§9）仍然有效，未因 APPROVE 而降级。

---

## 1. 当前目标与适用边界

本 Review Packet 的目标是为下一步 APB/OSD 结果打包接入提供一份独立设计文档——只定义**逻辑字段与语义契约**，不定义 wire 编码、地址、offset、bit 位置或最终寄存器宽度。

### 1.1 适用边界

- **适用**：OSD 结果展示的逻辑字段语义设计、staging→commit→active 原子更新原则、CDC 握手/快照传输约束、request/ACK toggle（单比特）与 16-bit sequence（两者独立）、fail-safe 规则、FPGA/SoC 联合确认清单。
- **不适用**：不定义任何 APB 基址、寄存器 offset、wire 位布局、C 代码实现、RTL 修改、OSD 字符渲染能力、PDU 格式或 FPGA 内部编码。
- **不覆盖**：本设计不替代 `CURRENT_STATE.md` 中尚未关闭的任何 Gate（SoC PLL、APB/CDC RTL、PNR/STA、bitstream、板级烧录、UART2、机械臂实机控制）。

### 1.2 与其他文档的关系

| 文档 | 关系 |
|---|---|
| `AGENTS.md` §分赛区决赛系统架构硬边界 | 遵循 CPU→FPGA 职责划分：CPU 产生结果语义，FPGA 完成 OSD 像素渲染 |
| `CURRENT_STATE.md`（2026-07-14 条目） | 当前未定义 APB/OSD wire ABI；本 Packet 不改变该事实 |
| `CPU_MODULE_PLAN.txt` [7] / 明日优先事项 | 第 2 项"先准备 APB/OSD 结果打包设计"的直接交付 |
| `cpu_result_semantics.h` | 本设计以 `cpu_display_result_t` 为唯一上游，所有枚举为 CPU 内部语义 |
| `register_map.md` | 本设计提供未来签约的逻辑字段清单；现有候选偏移仍为 PROPOSED/TBD |
| `第十届集创赛…细则_0710.md` §二.3.3 | 明确要求"准确无异议的输出各环节结果"——本设计为此提供 fail-safe 语义保证 |

---

## 2. 已有 `cpu_display_result_t` 的内部语义（回顾）

当前 `cpu_result_semantics.h` 定义了以下纯语义类型（**CPU 内部使用，非 wire ABI**）：

```c
typedef struct {
    uint8_t         valid;      // 1 = 本轮结果已锁存；0 = 尚无结果
    uint8_t         is_target;  // 1 = 判定为目标；0 = 非目标/不可判定
    cpu_decision_t  decision;   // NONE(0) / EXECUTE(1) / SKIP(2) / ERROR(3)
    cpu_execution_t execution;  // NONE(0) / REQUESTED(1) / SKIPPED_NON_TARGET(2) /
                                // BLOCKED(3) / FAULT(4)
    cpu_reason_t    reason;     // 见下方枚举
} cpu_display_result_t;
```

**统一理由枚举**（显式赋值；`NONE=0`，`INVALID_INTERNAL=255`；全部仅在 CPU 内部使用，禁止直接序列化）：

| 枚举常量 | 值 | 语义 |
|---|---|---|
| `CPU_REASON_NONE` | 0 | 无理由 / 尚无结果 |
| `CPU_REASON_TARGET_MATCH` | 1 | 命中目标 |
| `CPU_REASON_COLOR_MISMATCH` | 2 | 颜色不匹配 |
| `CPU_REASON_SHAPE_MISMATCH` | 3 | 形状不匹配 |
| `CPU_REASON_SIZE_DIFF_NOT_10MM` | 4 | 任务三：\|obs-ref\| ≠ 10mm |
| `CPU_REASON_SIZE_DIFF_OVER_5MM` | 5 | 任务四：\|obs-target\| > 5mm |
| `CPU_REASON_OBSERVATION_UNKNOWN` | 6 | 分类未知/观测不可判定 |
| `CPU_REASON_OBSERVATION_UNSTABLE` | 7 | 观测不稳定 |
| `CPU_REASON_SIZE_UNAVAILABLE` | 8 | 尺寸不可用 |
| `CPU_REASON_INVALID_TARGET` | 9 | 目标配置非法 |
| `CPU_REASON_OPERATOR_ABANDON` | 10 | 操作员放弃本轮 |
| `CPU_REASON_ARM_NOT_READY` | 11 | 机械臂未就绪 |
| `CPU_REASON_ARM_FAULT` | 12 | 机械臂故障/等待超时故障 |
| `CPU_REASON_ACQUIRE_STABILITY_TIMEOUT` | 13 | 取样稳定超时 |
| `CPU_REASON_ROUND_TIMEOUT` | 14 | 轮级超时 |
| `CPU_REASON_INVALID_INTERNAL` | 255 | 未知/越界/矛盾组合安全兜底 |

**合法 (action + reason + is_target) 组合**（由 `cpu_display_from_round_output()` 精确校验——真实轮次全部命中，非超集）：

| 类别 | action | reason | is_target | decision | execution |
|---|---|---|---|---|---|
| A | GRAB | TARGET_MATCH | 1 | EXECUTE | REQUESTED |
| B | SKIP | COLOR/SHAPE/SIZE_* | 0 | SKIP | SKIPPED_NON_TARGET |
| C | NONE | ARM_NOT_READY | **1** | NONE | BLOCKED |
| C | NONE | OPERATOR_ABANDON | **0** | NONE | BLOCKED |
| C | NONE | STABILITY_TIMEOUT | **0** | NONE | BLOCKED |
| C | NONE | OBSERVATION_UNKNOWN | **0** | NONE | BLOCKED |
| C | NONE | TARGET_INVALID | **0** | NONE | BLOCKED |
| D | NONE | ARM_FAULT | **1** | NONE | FAULT |
| 矛盾 | 任意 | 任意（is_target 与理由语义矛盾） | — | ERROR | FAULT |

> **关键约束**：任何矛盾/非法组合一律返回 `(valid=1, is_target=0, decision=ERROR, execution=FAULT, reason=INVALID_INTERNAL)`——绝不输出 `REQUESTED` 或正常 `SKIPPED`。

---

## 3. 未来 OSD 逻辑记录需表达的字段

以下逻辑字段清单基于官方细则 §二.3.3（"准确无异议的输出各环节过程中的结果：识别结果、判断结果、执行或不执行分拣的准确理由"）和 §二.3.5（评分表中识别、判断、执行三环节串行计分）的要求推演。**所有 wire 编码、地址、offset、bit 位置均标记为 PROPOSED/TBD。**

### 3.1 识别结果字段

| 逻辑字段 | 宽度（PROPOSED）| 编码（PROPOSED）| 说明 |
|---|---|---|---|
| `observed_color` | 3-bit | `000`=unknown, `001`=white, `010`=black, `011`=red, `100`=blue, `101`=yellow, 其余RESERVED | 覆盖五色（白/黑/红/蓝/黄）+ UNKNOWN |
| `observed_shape` | 2-bit | `00`=unknown, `01`=cube, `10`=cylinder, `11`=cone | 三形状 + UNKNOWN |
| `observed_size_cm_x10` | 8-bit | 0=不可用/通配；20/25/30=0.1cm 单位 | Cam1 侧面像素高度→尺寸分类结果 |

> **形状编码注意**：2-bit 编码已无 RESERVED 值（`00`=unknown, `01`=cube, `10`=cylinder, `11`=cone）。非法形状必须通过 `valid`/`decision=ERROR`/fail-safe 状态表达，或未来扩大编码宽度；不得将合法 cone (`11`) 判为 ERROR。
>
> **注意**：`observed_color` 编码与 `board_io.h` 的 `result_writeback_t.color`（0=unknown,1=white,2=black,3=red,4=blue,5=yellow）**语义一致但 wire 编码独立**——OSD 侧可能选择不同的 bit 打包策略（如跳过 unknown、使用独热码等），具体编码需 FPGA 队员确认 OSD 字符 ROM 后定版。

### 3.2 判断结果字段

| 逻辑字段 | 宽度（PROPOSED）| 编码（PROPOSED）| 说明 |
|---|---|---|---|
| `is_target` | 1-bit | 0=非目标/不可判定，1=判定为目标 | 与 `cpu_display_result_t.is_target` 语义一致 |
| `decision` | 2-bit | PROPOSED: 映射 `cpu_decision_t` → `00`=NONE, `01`=EXECUTE, `10`=SKIP, `11`=ERROR | 独立于 CPU 内部枚举值 |
| `reason` | 4-bit | PROPOSED: 16 个槽位，映射 14 个合法理由 + INVALID_INTERNAL + 1 保留 | **绝对独立于 `cpu_reason_t` 数值** |

### 3.3 执行结果字段

| 逻辑字段 | 宽度（PROPOSED）| 编码（PROPOSED）| 说明 |
|---|---|---|---|
| `execution` | 3-bit | PROPOSED: 映射 `cpu_execution_t` → `000`=NONE, `001`=REQUESTED, `010`=SKIPPED_NON_TARGET, `011`=BLOCKED, `100`=FAULT | 覆盖全部 5 种执行状态 |
| `arm_state` | 3-bit | PROPOSED: `000`=IDLE, `001`=READY, `010`=BUSY, `011`=DONE, `100`=FAULT, 其余RESERVED | 当前 ARM_DISABLED 时固定 IDLE |
| `error_code` | 16-bit | PROPOSED: 0=无错误，非零为最近一轮 CPU 侧错误码 | 来自 `board_io_write_global_state()` |

### 3.4 任务与序号字段

| 逻辑字段 | 宽度（PROPOSED）| 编码（PROPOSED）| 说明 |
|---|---|---|---|
| `task_mode` | 3-bit | PROPOSED: `000`=NONE, `001`=MODE_1, `010`=MODE_2, `011`=MODE_3, `100`=MODE_4 | 当前任务模式 |
| `round_seq` | 8-bit | 0–19（20 轮），0-based | 当前轮次序号 |
| `event_seq` | 16-bit | 0–65535，半范围回绕 | 最后一次被消费的操作员事件序号 |
| `result_sequence` | 16-bit | 0–65535，半范围回绕 | 每次 commit 递增，供 OSD 检测 stale 数据 |
| `target_color` | 3-bit | 与 `observed_color` 相同编码 | 当前任务目标颜色 |
| `comparison_size_cm_x10` | 8-bit | 0=通配/不适用；20/25/30 | 当前任务参照尺寸：MODE_3 时 OSD 显示 REFERENCE_SIZE（规则：\|obs-ref\|==10mm），MODE_4 时 OSD 显示 TARGET_SIZE（规则：\|obs-target\|≤5mm） |

### 3.5 字段分类与 OSD 显示优先级

按官方细则 §二.3.5 评分表（识别→判断→执行 串行），OSD 应至少满足以下三层显示：

1. **识别层**：`observed_color`、`observed_shape`、`observed_size_cm_x10` — 每轮必须显示。
2. **判断层**：`is_target`、`decision`、`reason`、`task_mode`、`target_color`、`comparison_size_cm_x10` — 每轮必须显示。
3. **执行层**：`execution`、`arm_state`、`error_code` — 每轮必须显示。目标轮显示执行、阻止或故障状态及理由；非目标轮必须明确显示 `SKIPPED_NON_TARGET`，并显示颜色、形状或尺寸不匹配理由。这用于满足官方细则 §二.3.3 "执行或不执行分拣的准确理由" 要求。
4. **一致性层**：`round_seq`、`event_seq`、`result_sequence` — 供裁判/调试核对逐轮一致性。

---

## 4. Staging → Commit → Active 原子更新原则

### 4.1 问题

OSD 必须避免读到跨轮混合字段——当前轮 `is_target` 配上一轮的 `reason`、或 `color` 已更新而 `decision` 仍为旧值。比赛细则要求"准确无异议的输出"，混合字段违反此要求。

### 4.2 设计原则

沿用 `board_io.h` / `register_map.md` 中已定义的 staging→commit→active 影子寄存器协议，并扩展到完整的 OSD 结果集：

```
CPU 写 staging 寄存器组（所有结果字段）
       ↓
CPU 写 RESULT_COMMIT（含 16-bit result_sequence）
       ↓
FPGA 在像素域 VSYNC 边界原子切换 active 快照
       ↓
OSD 渲染只读 active 快照
```

**原子性要求**：

1. **全部字段或全不更新**：FPGA 在 VSYNC 边界将 staging 整组原子复制到 active。任何时候 active 快照包含同一轮次的完整、一致字段。OSD 渲染引擎只读 active 快照，不读 staging。
2. **禁止部分提交**：CPU 不能在两次 VSYNC 之间分别提交 color+shape 和 decision+reason——必须一次写完所有 staging 字段，再写一次 commit。
3. **提交确认**：CPU 写完 commit 后必须等待 ACK toggle 返回，APB 域确认 `confirmed_result_sequence == committed_sequence` 后，本轮结果才算在 OSD 可见。

**Bundled-data 稳定性约束**：

4. **request 发出后冻结 staging**：源域（APB 域）必须在翻转 request toggle 后冻结整组 staging 寄存器。
5. **ACK 返回前禁止修改**：收到返回 ACK toggle 匹配前，CPU 不得修改任何 staging 字段。
6. **禁止同周期采样**：目标域（像素域）不得在刚检测到 request toggle 变化的同一周期立即采样总线；必须等待由 FPGA/SoC 联合审查确定的目标域稳定周期（PROPOSED/TBD），再在 VSYNC 边界捕获。
7. **整组原子更新**：active 快照的所有位必须在同一像素时钟沿更新，不得分散在多个周期逐字段加载。
8. **ACK 仅在捕获后产生**：像素域只能在 active 整组捕获完成后翻转 ACK toggle。
9. **稳定周期标记 PROPOSED/TBD**：具体稳定周期数（如等待 N 个像素时钟周期）继续标记为 PROPOSED/TBD，不擅自写成硬件事实。

### 4.3 与现有协议的对齐

- `OFF_CPU_RESULT_COLOR`（当前 Host 头文件/`register_map.md` 候选偏移 `0x054`）、`OFF_CPU_RESULT_SHAPE`（候选 `0x058`）、`OFF_CPU_RESULT_SIZE`（候选 `0x05C`）、`OFF_CPU_MATCH_ACTION`（候选 `0x060`）为每通道 staging 寄存器的草案名称；这些候选偏移不是生成 `soc.h`、APB RTL 或板级硬件事实。
- `OFF_CFG_COMMIT`（候选 `0x04C`）提交配置和 OSD 字段；同样不是硬件事实。
- **本设计建议**：结果字段（color/shape/size/decision/reason/execution/sequence/arm_state/error_code）单独使用一条 **REQUIRED: NEW** `RESULT_COMMIT` 通道，独立于 `CFG_COMMIT`——理由：配置提交和结果提交有不同的时序要求（配置在任务开始前一次性提交，结果每轮提交 1-2 次）。若 FPGA 资源不足以支持第二条 commit 通道，则复用现有 `CFG_COMMIT` 并在 Review Packet 中声明时序约束。

> **PROPOSED/TBD**：`RESULT_COMMIT` 的 offset、宽度和与 `CFG_COMMIT` 的互锁关系由 FPGA/SoC 联合 Review Packet 确认。

---

## 5. 多位 CPU→FPGA CDC：握手/快照传输，禁止逐位直接同步

### 5.1 问题

OSD 结果集预计 ≥64 bit（color/shape/size/decision/execution/reason/is_target/sequence/arm_state/error_code/task_mode/target 字段）。如果 CPU 逐位写入 APB 寄存器、FPGA 在像素域逐位采样，会出现跨时钟域（APB 域 → 像素域 `i_sysclk_div2`）的部分更新——OSD 可能显示混合了新旧两轮字段的画面。

### 5.2 硬性要求

**多位 CPU→FPGA 结果传输必须通过 request-toggle / ACK-toggle 握手协议，禁止逐位直接同步、禁止单周期窄脉冲经 2-FF 直接传递。**

完整的 request-toggle / ACK-toggle 握手契约（10 步）：

1. APB 域（CPU 侧）完成整组 staging 寄存器写入。
2. staging 至少在 request toggle 翻转前满足源域稳定要求（见 §4.2 bundled-data 约束）。
3. APB 域翻转 request toggle（单比特）。
4. request toggle 经 2-FF 同步器进入像素域。
5. 像素域检测 toggle 变化（边沿/异同检测），将其锁存为 pending。
6. 像素域等待规定的目标域稳定周期（PROPOSED/TBD：具体周期数由 FPGA/SoC 联合审查确定）。
7. 在明确规定的 VSYNC 边界整组捕获 staging 到 active。
8. 捕获完成后，像素域翻转 ACK toggle（单比特）。
9. ACK toggle 经 2-FF 同步器返回 APB 域。
10. APB 域检测 ACK toggle 匹配后，才认为提交完成。

**关键约束**：

- 2-FF 同步器**仅**用于同步单比特 toggle 信号（request/ACK）。
- **不允许**用 2-FF 直接同步单周期窄脉冲——窄脉冲宽度可能小于目标域时钟周期，导致漏采。
- **不允许**逐位同步多位 staging 总线——每一位的亚稳态窗口不同，会读到部分更新的位。
- **不允许**在非 VSYNC 边界的任意时刻部分更新 active 快照。
- **不允许**依赖"APB 频率远低于像素时钟所以自然安全"的假设——任何跨时钟域的多位总线传输必须经过标准 CDC 握手。

### 5.3 pending/busy 互锁

1. **单 outstanding commit**：同时最多存在一个未完成的结果提交（request 已发出、ACK 未返回）。
2. **busy/pending 状态**：request toggle 翻转后进入 busy/pending 状态。
3. **ACK 前禁止修改/重复提交**：ACK 返回前，CPU 禁止修改 staging、禁止再次翻转 request toggle。
4. **重复 toggle 不重复应用**：像素域重复观察到同一个 request toggle 值（与上次锁存值相同）不得重复应用、不得重复翻转 ACK。
5. **VSYNC 碰撞推迟**：若 request toggle 变化在 VSYNC 同周期被检测到、或不满足捕获准备条件（如目标域稳定周期未满），则统一推迟到下一次完整 VSYNC 边界。
6. **不覆盖未捕获提交**：在上一次提交的 ACK 返回前，不允许用新数据覆盖尚未捕获的 staging。
7. **超时安全语义**：任何超时（等待 ACK 超时、VSYNC 超时等）只进入 ERROR / STALE / BLOCKED，不产生 REQUESTED 或机械臂动作。

### 5.4 返回方向 CDC：ACK toggle + APB 域 confirmed 镜像

**禁止 CPU 直接异步读取像素域的多位 sequence 总线。** 旧草案术语 `active_result_sequence` 已弃用；本设计统一使用 APB 域 `confirmed_result_sequence`（APB 域根据 ACK toggle 维护的确认镜像，非像素域直接返回）。

选择以下最小方案：

1. 像素域完成 active 整组捕获后，**只返回单比特 ACK toggle**（见 §5.2 步骤 8）。
2. ACK toggle 经 2-FF 同步器返回 APB 域。
3. APB 域在检测到匹配 ACK toggle 后，把本地 pending `result_sequence` 复制为 `confirmed_result_sequence`（APB 域内部寄存器）。
4. CPU 读取 APB 域的 `confirmed_result_sequence`/status，不直接跨域读取像素域的多位 sequence。
5. §10 汇总表中的 `confirmed_result_sequence` 是 APB 域根据 ACK 维护的**确认镜像**，不是像素域多位总线直接返回。

**PROPOSED/TBD**：`confirmed_result_sequence` 的 offset、宽度和与现有 `CFG_STATUS` 的关系由 FPGA/SoC 联合 Review Packet 确定。

---

## 6. 16-bit Sequence、Commit、ACK 与回绕语义

### 6.1 当前约定（由 2026-07-14 Fable 整改确定）

`round_controller` 和 `competition_contract` 已统一为 16-bit `event_seq`。当前语义：

- `(uint16_t)(new - last)` 计算增量。
- `delta != 0 && delta < 32768` → 向前，接受。
- `delta == 0` → 重复，拒绝（幂等）。
- `delta >= 32768` → 倒退/过期，拒绝。
- `65535 → 0`（delta=1）→ 接受（半范围回绕）。
- `0 → 65535`（delta=65535）→ 拒绝。

### 6.2 扩展到 `result_sequence` 和 `RESULT_COMMIT`

新增 16-bit `result_sequence`：

- CPU 每次 `RESULT_COMMIT` 前递增本地 `result_sequence`。
- CPU commit 后等待 ACK toggle 返回，APB 域将本地 pending `result_sequence` 复制为 `confirmed_result_sequence`。
- CPU 轮询确认 `confirmed_result_sequence == committed_sequence` → 提交已在像素域生效。
- OSD 侧可通过 `result_sequence` 检测 stale：若连续 N 帧 `result_sequence` 不变 → 无新结果。
- `confirmed_result_sequence`（§10 汇总表）是 APB 域根据 ACK 维护的确认镜像，不是像素域多位总线直接返回。

**ACK 语义（注意区分 toggle 与 sequence）**：

- `request_toggle` 和 `ACK_toggle` 是**单比特状态信号**，只做相等/不等或变化检测，不使用数值大小比较，不存在 16-bit 半范围回绕。
- **结果提交 ACK**：指 §5.2 的 request-toggle / ACK-toggle 握手——单比特硬件握手，保证 staging→active 的 CDC 传输完成。CPU 通过 APB 域 `confirmed_result_sequence` 确认提交已生效。
- **事件 ACK**：继续使用现有 `round_controller` 的 `event_ack_seq`/`event_ack_status` 契约，是 CPU 对操作员事件（PLACE/REMOVE 等）的消费确认。
- `event_seq` 是独立的 16-bit 操作员事件序号，使用半范围回绕规则（见 §6.1）。
- `result_sequence` 是独立的 16-bit 结果提交序号，使用半范围回绕规则（见 §6.1）。
- `event_seq` 和 `result_sequence` 可以采用同一种半范围算法，但两者独立维护、不可互相比较。
- `confirmed_result_sequence` 是 APB 域收到匹配 ACK toggle 后，对本地 pending `result_sequence` 的确认镜像。

**重复事件**：

- CPU 对同一 `event_seq` 重复调用 `round_controller_tick()` 不重复消费——`seq_is_newer()` 保证幂等。
- 但 CPU **可能**在消费事件后、commit 结果前因异常重启/软复位重新进入同一轮。此时 `result_sequence` 会从复位后的值重新递增。复位后的行为见 §6.3 reset/session 语义。

**回绕**：

- `result_sequence` 从 `0xFFFF` 回绕到 `0x0000` 是正常行为（半范围判定 delta<32768 接受）。OSD 不应把合法回绕误判为过期。

### 6.3 Reset / Session 语义

#### A. 当前可成立的安全原则（fail-safe 边界）

以下原则不依赖具体复位拓扑即可成立，必须在任何实现中保证：

1. **RECOVERING 进入条件**：任一相关域（APB 域或像素域）检测到本域复位，或检测到对端 session 未对齐，立即进入 RECOVERING 状态。
2. **RECOVERING 期间禁止项**：
   - 禁止发起普通 result request；
   - 禁止把任何 ACK toggle 当作普通提交确认；
   - 禁止 active 快照更新；
   - 清除或屏蔽 pending/busy 状态；
   - OSD 只显示 `RECOVERING` 或 `NO_RESULT`；
   - 不显示 `REQUESTED`；
   - 不产生机械臂动作。
3. **RECOVERING 退出条件**：只有双方完成明确的 baseline 对齐，并收到与该 baseline 对应的确认后，才允许退出 RECOVERING。
4. **Session 隔离**：不同 session 之间不得沿用旧 active、旧 pending、旧 ACK 或旧 sequence 比较基准。
5. **Session 边界**：合法的 `0xFFFF→0x0000` 半范围回绕只在同一有效 session 内使用。复位后的首个有效 sequence 视为新 session 起点，不跟前一 session 的末序号做增量比较。

#### B. 当前尚未定版、阻塞 RTL 实现的联合确认项

以下事实在 FPGA/SoC 联合 Review Packet 确认前，**不允许实现或放行任何 CDC RTL**：

1. APB 域和像素域是否同源、同步复位。
2. 两个域的复位解除顺序。
3. 如何检测只有一侧发生复位（单侧复位检测）。
4. `request_toggle` 和 `ACK_toggle` 的初始/重新基准值如何交换。
5. baseline 的请求类型、识别方式和对应 ACK 如何区别于普通提交。
6. 单侧复位后由哪一侧发起恢复。
7. 如何证明旧 ACK 不会被误认为 baseline ACK。
8. 如何证明旧 request 不会被误认为新 session 的普通请求。
9. baseline 完成和退出 RECOVERING 的唯一判据。

**必须明确**：

- 当前文档只冻结 fail-safe 边界（§6.3.A）。
- 具体独立复位恢复协议仍为 **PROPOSED/TBD**。
- 这是后续 FPGA/SoC 联合 Review Packet 的**阻塞项**。
- 在该阻塞项关闭前，**不允许声称 reset/session 协议已闭合，不允许实现正式 CDC RTL**。

> **已废弃表述**：旧版"执行一次 baseline handshake 即可建立新的 toggle 基准"过度确定。正确表述为：必须执行经联合 Review Packet 定版、可区分普通提交且能够证明无旧边沿误认的 baseline 对齐协议；其具体编码和时序当前 **TBD**。

> **Boot/session epoch**：可列为未来替代方案（如增加 epoch 字段以区分不同 session），但不作为当前设计选择，也不在本文档中展开。

---

## 7. Fail-Safe 规则：非法枚举、缺失字段、过期序号和矛盾组合

### 7.1 总原则

OSD 和机械臂控制链路必须 fail-safe：任何异常输入→安全输出，绝不在异常情况下误驱动机械臂、误显示 "REQUESTED" 或正常 "SKIPPED"。

### 7.2 具体规则

#### R1：非法枚举值

- `observed_color` 出现 RESERVED 编码（110/111）→ OSD 显示 "COLOR_ERR" 或等价；`decision` 强制 ERROR。
- `observed_shape` 的 2-bit 编码已无 RESERVED 值（`00`=unknown, `01`=cube, `10`=cylinder, `11`=cone）。若未来扩展编码宽度出现非法值，OSD 显示 "SHAPE_ERR"；`decision` 强制 ERROR。当前不得把合法 cone (`11`) 判为 ERROR。
- `decision` 或 `execution` 出现未定义编码 → 按 `INVALID_INTERNAL` 处理；OSD 显示 "ERROR"；**绝不启动 arm_controller_request_grab()**。
- `reason` 值在 4-bit 字段中越界（超过已分配的 14+2 个槽位）→ 显示 "INVALID"；`decision=ERROR`，`execution=FAULT`。

#### R2：缺失字段

- `observed_size_cm_x10 == 0` 且任务需要尺寸（MODE_3/MODE_4）→ OSD 显示 "SIZE_UNAVAILABLE"；即使 `is_target` 可能为 1，OSD 也必须同时显示尺寸不可用的原因；不得以正常目标姿态显示。
- `valid == 0` → OSD 显示 "NO_RESULT" 或保留上一轮结果并标记 "STALE"。不得显示新鲜结果的外观（颜色/符号正常但实际来自旧数据）。

#### R3：过期序号

- `confirmed_result_sequence`（APB 域）< 上一帧记录的 `last_seen_sequence`（按半范围判定过期）→ OSD 检测到 stale 快照，保持当前显示并加 "STALE" 标记，不要用过期快照覆盖当前显示。

#### R4：矛盾组合

- 上游 `cpu_display_from_round_output()` 已将所有矛盾组合（is_target 与 reason 语义不一致、action 与 reason 不一致等）安全兜底为 `(ERROR, FAULT, INVALID_INTERNAL)`。
- APB/OSD 适配层应**信任**该兜底——即：如果写入 staging 的结果是 `decision=ERROR, execution=FAULT, reason=INVALID_INTERNAL`，OSD 必须如实显示 "ERROR / FAULT / INVALID_INTERNAL"，不得解释为 "NONE" 或覆盖为默认值。
- **绝对禁止**：任何状态下 OSD 不得显示 `execution=REQUESTED`（值 1）除非经过 `cpu_display_from_round_output()` 合法组合 A 的校验。APB/OSD 适配层不得绕过 `cpu_result_semantics` 直接写 `REQUESTED`。

#### R5：OSD 与机械臂的隔离

- `OSD 显示 REQUESTED` ≠ `arm_controller_request_grab()` 被调用。OSD 只回放语义状态；机械臂动作必须通过 `round_controller` 的 `request_arm_grab` 脉冲 + `arm_busy`/`arm_enabled` 安全门独立放行。
- ARM_DISABLED 下，`request_arm_grab` 永不为 1，OSD `execution` 永不为 REQUESTED——双重保证。

### 7.3 Fail-Safe 优先级表

| 异常条件 | OSD 显示 | decision | execution | 是否触发 arm |
|---|---|---|---|---|
| 合法组合 A（目标命中）| 正常目标信息 | EXECUTE | REQUESTED | 仅当 arm_enabled=1 且 arm_busy=0 |
| 合法组合 B（非目标跳过）| 正常非目标信息 | SKIP | SKIPPED_NON_TARGET | 否 |
| 合法组合 C（阻止）| 阻止原因文本 | NONE | BLOCKED | 否 |
| 合法组合 D（故障）| 故障信息 | NONE | FAULT | 否 |
| CPU 内部矛盾 | "ERROR" | ERROR | FAULT | 否 |
| 非法枚举 | "INVALID" | ERROR | FAULT | 否 |
| valid=0 | "NO_RESULT" 或 "STALE" | NONE | NONE | 否 |
| 过期 result_sequence | 保持 + "STALE" | 不改变 | 不改变 | 否 |

---

## 8. `cpu_result_semantics` 枚举值是 CPU 内部值，非 Wire ABI

### 8.1 硬性规则

`cpu_result_semantics.h` 中所有枚举（`cpu_reason_t=0..14,255`、`cpu_decision_t=0..3`、`cpu_execution_t=0..4`）的数值**仅在 CPU 内部有效**。以下行为被禁止：

1. **禁止直接序列化**：不得把 `cpu_reason_t` 的 C 枚举值（如 `CPU_REASON_TARGET_MATCH=1`、`CPU_REASON_INVALID_INTERNAL=255`）直接写入 APB 寄存器作为 wire 编码。
2. **禁止隐式等值假设**：不得假设 `CPU_REASON_ARM_FAULT=12` 意味着 APB 侧第 12 号 OSD 字符——APB/OSD 侧必须有自己独立的映射表。
3. **强制显式转换**：从 `cpu_display_result_t` 到 staging 寄存器的打包，必须通过一个独立的 `osd_result_pack()`（或等价适配函数），在该函数内以 `switch` 对每个枚举值做显式 wire 编码映射。默认分支一律输出安全兜底值。

### 8.2 设计意图

- CPU 内部枚举值可能随维护而调整（如新增细分理由、重排顺序）。如果 APB/OSD 侧直接依赖这些值，任何内部枚举调整都会破坏 OSD 显示——这违反了分离原则。
- Wire 编码由 FPGA OSD 字符 ROM 的实际查表顺序决定；CPU 无权假设其顺序。

### 8.3 两层映射架构

```
round_controller_output_t (action/reason/is_target)
       │
       ▼
cpu_display_from_round_output()     ← 组合合法性校验 + 安全兜底
       │
       ▼
cpu_display_result_t (cpu_reason_t / cpu_decision_t / cpu_execution_t)
       │   CPU 内部语义边界
       ▼
osd_result_pack()                   ← 显式 switch 映射（TBD：待 FPGA/SoC Review Packet 定版）
       │
       ▼
APB staging 寄存器组 (wire 编码)    ← PROPOSED/TBD
       │
       ▼
FPGA CDC → active 快照 → OSD 渲染
```

---

## 9. 待 FPGA/SoC 联合确认事项

以下事项必须在 APB/OSD 接入编码开始前，由 FPGA/SoC 队员与 CPU 队员在同一份 Review Packet 中联合确认。**本表不构成任何硬件事实。**

| # | 确认事项 | 负责方 | 阻塞影响 |
|---|---|---|---|
| 1 | 正式 `soc.h`：是否已由 Efinity IP Manager 生成？`APB_VISION_BASE` 的实际值？ | FPGA/SoC | 阻塞所有 APB 地址计算 |
| 2 | APB 从机基址与窗口大小：当前占位 `0xe8100000`/4KiB 是否最终？APB0 `PADDR` 位宽问题是否已解决？| FPGA/SoC | 阻塞寄存器访问 |
| 3 | OSD 结果寄存器数量与位宽：新增结果字段（见 §3）需要多少个 32-bit 字？是否复用现有 `OFF_CPU_RESULT_*` 还是新增一组？| FPGA/SoC | 阻塞打包函数设计 |
| 4 | APB 时钟域频率与像素域频率：`PCLK` (APB) vs `i_sysclk_div2` (像素) 的实际频率？| FPGA/SoC | 阻塞 CDC 参数 |
| 5 | **独立复位与 RECOVERING 协议（阻塞项，见 §6.3.B）**：两域复位关系（同源/独立）、单侧复位检测机制、toggle 基准初始化方式、baseline 请求/ACK 与普通提交的区分方式、恢复发起方、RECOVERING 退出唯一判据。**以上事实定版前禁止实现或放行任何 CDC RTL。** | FPGA/SoC | 阻塞上电行为、独立复位恢复、CDC RTL 实现 |
| 6 | CDC 所有权：OSD 结果快照的 staging→active CDC 在 RTL 哪一侧实现？CPU 侧只写 staging + commit，还是 CPU 侧也需要参与 CDC 时序？| FPGA/SoC | 阻塞 CPU→FPGA 接口契约 |
| 7 | OSD 字符 ROM 能力：支持多少种字符/字符串？能否支持 "INVALID_INTERNAL"、"SIZE_UNAVAILABLE" 等长字符串？还是 CPU 侧需做短码映射（如 "E255" 替代长字符串）？| FPGA/SoC | 阻塞文本接口 |
| 8 | `RESULT_COMMIT` vs `CFG_COMMIT`：是否需要独立的结果 commit 通道（见 §4.3），还是复用现有 `CFG_COMMIT`？| FPGA/SoC | 阻塞提交协议 |
| 9 | AXI/APB 互锁：`CFG_COMMIT` 的 16-bit 全值比较是否已在 RTL 实现？确认 `active_seq` 和 `pending_seq` 的读取位宽和时序。| FPGA/SoC | 阻塞轮询逻辑 |
| 10 | 现有 `OFF_CPU_MATCH_ACTION/0x060` 的 bitfield 编码（match/grab/skip/error 独立位）是否在 OSD 升级后保持不变，还是需要扩展为更宽的语义字段？| FPGA/SoC | 阻塞向后兼容策略 |

> **独立复位阻塞项说明**：§9 第 5 项（独立复位与 RECOVERING 协议）已从单一"复位策略"扩展为包含两域复位关系、单侧复位检测、toggle 基准初始化、baseline 请求/ACK 区分、恢复发起方、RECOVERING 退出条件等子项的完整阻塞清单。在 FPGA/SoC 联合 Review Packet 定版这些事实前，**禁止实现或放行任何 CDC RTL**。详见 §6.3.B。

---

## 10. 逻辑字段汇总表（全字段 PROPOSED/TBD）

以下为 CPU→OSD 结果的具体逻辑字段。**所有 Wire 编码、APB 地址、offset、bit 位置、寄存器宽度和 FPGA 内部表示均为 PROPOSED/TBD，不得视为已定版硬件事实。** SS=3-bit, S8=8-bit, S16=16-bit 等为 CPU 内部类型宽度，最终 wire 宽度由 FPGA OSD 渲染需求决定。

| 逻辑字段 | CPU 类型 | 方向 | 原子快照组 | 说明 |
|---|---|---|---|---|
| `observed_color` | S8 (0–5) | CPU→FPGA | 结果组 | 识别颜色；wire 编码 PROPOSED/TBD |
| `observed_shape` | S8 (0–3) | CPU→FPGA | 结果组 | 识别形状；wire 编码 PROPOSED/TBD |
| `observed_size_cm_x10` | S8 (0,20,25,30) | CPU→FPGA | 结果组 | 识别尺寸；wire 编码 PROPOSED/TBD |
| `is_target` | S8 (0/1) | CPU→FPGA | 结果组 | 目标判定；wire 编码 PROPOSED/TBD |
| `decision` | cpu_decision_t (0–3) | CPU→FPGA | 结果组 | 判断动作；wire 编码独立于 CPU 枚举 PROPOSED/TBD |
| `reason` | cpu_reason_t (0–14,255) | CPU→FPGA | 结果组 | 统一理由；wire 编码独立于 CPU 枚举 PROPOSED/TBD |
| `execution` | cpu_execution_t (0–4) | CPU→FPGA | 结果组 | 执行状态；wire 编码独立于 CPU 枚举 PROPOSED/TBD |
| `arm_state` | S8 (TBD) | CPU→FPGA | 全局组 | 机械臂状态；当前固定 IDLE；wire 编码 PROPOSED/TBD |
| `error_code` | S16 | CPU→FPGA | 全局组 | CPU 错误码；wire 编码 PROPOSED/TBD |
| `task_mode` | S8 (0–4) | CPU→FPGA | 配置/目标组 | 当前任务模式 PROPOSED/TBD |
| `round_seq` | S8 (0–19) | CPU→FPGA | 结果组 | 轮次序号 PROPOSED/TBD |
| `event_seq` | S16 (0–65535) | CPU→FPGA | 结果组 | 最近消费的事件序号 PROPOSED/TBD |
| `result_sequence` | S16 (0–65535) | CPU→FPGA | 结果组 | 提交序号，每次 commit 递增 PROPOSED/TBD |
| `target_color` | S8 (0–5) | CPU→FPGA | 配置/目标组 | 当前目标颜色 PROPOSED/TBD |
| `comparison_size_cm_x10` | S8 (0,20,25,30) | CPU→FPGA | 配置/目标组 | MODE_3 为 REFERENCE_SIZE / MODE_4 为 TARGET_SIZE PROPOSED/TBD |
| `confirmed_result_sequence` | S16 | FPGA→CPU | 只读 | APB 域根据 ACK 维护的确认镜像，非像素域直接返回 PROPOSED/TBD |

> **字段分组说明**：
> - **结果组**：每轮更新，通过 RESULT_COMMIT 在 VSYNC 边界原子切换到 active 快照。
> - **全局组**：跨轮持续，随结果组一起提交或使用独立提交通道。
> - **配置/目标组**：任务开始前一次性写入（或每任务切换一次），使用现有 CFG_COMMIT 通道。
>
> **MODE_3/MODE_4 尺寸标签说明**：`comparison_size_cm_x10` 字段的 OSD 标签和理由必须按 `task_mode` 区分——MODE_3 时 OSD 标签显示 `REFERENCE_SIZE`（规则：`abs(observed_size - reference_size) == 10 mm`），MODE_4 时 OSD 标签显示 `TARGET_SIZE`（规则：`abs(observed_size - target_size) <= 5 mm`）。Wire 存储可以复用一个字段，但 OSD 标签和理由不可混淆。

---

## 11. ARM_DISABLED、UART2 与机械臂状态

### 11.1 ARM_DISABLED 保持

本设计明确继续保持 `ARM_DISABLED`（`arm_enabled=0`）。在 `main_loop_adapter` 的 ARM_DISABLED 路径下：

- `round_controller_input_t.arm_enabled = 0`
- `request_arm_grab` 永不为 1
- OSD `execution` 在合法组合 C（ARM_NOT_READY）时显示 `BLOCKED`，不显示 `REQUESTED`
- OSD `arm_state` 固定为 IDLE

### 11.2 不接 UART2

本设计不定义 UART2 的寄存器映射、波特率配置或 myCobot 协议帧通道。OSD 结果打包与 UART2 完全解耦。

### 11.3 不连接或驱动机械臂

`arm_state` / `request_arm_grab` 等字段仅用于 OSD 显示完整性——在 ARM_DISABLED 下它们都有安全默认值。OSD 显示 "BLOCKED (ARM_NOT_READY)" 不代表机械臂已就绪或将在未来就绪。任何机械臂动作需在 Gate G4（正式 SoC/PNR/bitstream）和 Gate G5（板到臂安全链）全部通过后才可讨论。

---

## 12. 明确不表示的闭合

本 Review Packet 不代表以下任何一项已完成或已闭环：

- ❌ APB 从机 RTL
- ❌ `soc.h` 生成
- ❌ SoC PLL 资源方案
- ❌ CDC RTL（staging→active 快照在像素域的原子加载）
- ❌ OSD 字符 ROM 能力
- ❌ Efinity PNR/STA/bitstream
- ❌ 板级烧录或上板
- ❌ UART2 / myCobot 协议帧
- ❌ 机械臂实机动作
- ❌ `main.c` 接入 OSD 结果打包路径
- ❌ 任何 APB 地址、offset、寄存器宽度或 wire 位布局

本设计仅提供从 `cpu_display_result_t` 到未来 APB/OSD 的逻辑字段语义映射——是下一步接入前的独立设计门，不是实现。

---

## 验证

### 验证命令

```powershell
git diff --check
```

### 验证结果

- **`git diff --check`**：PASS（无输出，无空白告警）。
- **`git status --short`**：仅新增本文件 `cpu_apb_osd_result_packing_design_20260715.md`。预存的未跟踪目录 `.cbm_test_index/` 和 `tools/mingw64/` 未被触碰。
- **文档内部审查**：全文 PROPOSED/TBD/待确认/候选/草案/占位 等限定词覆盖所有 wire 编码、地址、offset、bit 位置。文档包含历史/Host 草案中的候选 offset（`0x054`、`0x058`、`0x05C`、`0x060`、`0x04C` 等），但每一处均明确标记为未冻结，不能用于正式硬件访问。
- **rg 核查**：
  - 全文不存在仍被认可的"单周期脉冲经 2-FF"方案（已替换为 request-toggle + ACK-toggle）。
  - `observed_shape` 的 `11` 只表示 cone，不存在 "11=RESERVED"。
  - 每轮 execution/reason 必显（§3.5 执行层已改为每轮必须显示）。
  - 旧 `target_size_cm_x10` 已按设计范围全部改为 `comparison_size_cm_x10`。
  - 所有具体 offset 均带"候选/未冻结/非硬件事实"限定。
- **本轮整改自检**：
  1. `rg active_result_sequence`：除一次"旧草案术语已弃用"的迁移说明外应为 0。
  2. `rg 'ACK.*半范围|toggle.*半范围|16-bit.*ACK'`：不得再把单比特 toggle 写成 16-bit 回绕对象。
  3. 检查 §6.3 明确包含：RECOVERING 期间禁止普通请求/ACK/active 更新；独立复位协议仍 TBD；联合 Review Packet 阻塞项；未关闭前禁止正式 CDC RTL 实现。
  4. `git diff --check` PASS。

### 不运行的验证

- 完整 Efinity 综合/PNR/STA
- ModelSim/Questa 仿真
- 机械臂动作测试
- Host 单测重跑（本次未修改任何 C 源码）

---

## 交付

### 修改文件列表

- **新增**：`final_project/docs/review_packets/cpu_apb_osd_result_packing_design_20260715.md`（本文件）

无其他文件修改。

### 文档关键设计摘要

1. 以 `cpu_display_result_t` 为唯一上游，定义 15 个逻辑字段覆盖识别（color/shape/size）、判断（is_target/decision/reason）、执行（execution/arm_state/error_code）、任务上下文（task_mode/round_seq/event_seq/result_sequence/target_color/target_size）四层 OSD 显示需求。
2. 坚持 staging→commit→active 原子快照模型：全部结果字段在 VSYNC 边界一次性切换到 active，OSD 只读 active 快照，杜绝跨轮混合字段。
3. 多位 CPU→FPGA CDC 采用 request-toggle + ACK-toggle 握手协议（10 步完整契约）：APB 域翻转 request toggle → 2-FF 同步 → 像素域检测并锁存 pending → 等待稳定周期 → VSYNC 边界整组原子捕获 → 翻转 ACK toggle → 2-FF 返回 → APB 域确认。禁止单周期窄脉冲经 2-FF 直接传递、禁止逐位同步多位总线。bundled-data 冻结、单 outstanding commit、VSYNC 原子捕获、APB 域 confirmed sequence 镜像均已纳入。
4. `event_seq` 与 `result_sequence` 分别独立使用 16-bit 半范围规则（delta<32768 向前）；`request_toggle`/`ACK_toggle` 只使用单比特变化/匹配检测，不参与数值回绕比较。返回方向：ACK toggle → APB 域 `confirmed_result_sequence` 镜像，不直接跨域读取像素域多位 sequence。复位后进入 RECOVERING（§6.3），具体独立复位恢复协议仍 TBD、阻塞 CDC RTL 实现。
5. 全面的 fail-safe 规则：非法枚举 → ERROR/FAULT；缺失字段 → 显示不可用原因；矛盾组合 → ERROR/FAULT/INVALID_INTERNAL；过期序号 → 保持旧显示 + "STALE" 标记。**任何异常状态下绝不显示 REQUESTED、绝不驱动 arm_controller_request_grab()。**
6. `cpu_result_semantics` 枚举值明确为 CPU 内部值，禁止直接序列化；APB/OSD wire 编码必须经过显式 `osd_result_pack()` 适配层。
7. 列出 10 项 FPGA/SoC 联合确认事项（soc.h、APB 基址、寄存器数量与位宽、时钟、复位、CDC 所有权、OSD 字符能力等）。
8. 所有 wire 编码、地址、offset、bit 位置全部标记为 PROPOSED/TBD。
9. 继续 ARM_DISABLED，不接 UART2，不连接或驱动机械臂。
10. 明确本设计不表示 APB/OSD/板级/比赛闭环完成。

### 未验证项

- 本设计对应的 C 适配代码（`osd_result_pack()` 等）尚未编写
- APB/OSD 寄存器地址和位布局未与 FPGA 队员确认
- FPGA OSD 字符 ROM 能力未确认
- 正式 `soc.h` 未生成
- 所有 10 项 FPGA/SoC 联合确认事项（§9）均为待确认
- 无 Efinity 构建、仿真、上板或机械臂测试

### Git 状态

- **不提交 Git**（按任务要求）。

---

## 附录 A：参考文档链

| 文档 | 版本/日期 | 用途 |
|---|---|---|
| `AGENTS.md` | 2026-07-14 | 系统架构硬边界 |
| `CURRENT_STATE.md` | 2026-07-14 | 当前阻塞/下一步 |
| `CPU_MODULE_PLAN.txt` | 2026-07-14 | 明日优先事项 [2] |
| `cpu_result_semantics.h` | Gate 通过版 | 上游统一语义类型 |
| `cpu_result_semantics_adapters.h` | Gate 通过版 | 上游转换接口 |
| `cpu_result_semantics_adapters.c` | Gate 通过版 | 组合合法性校验实现 |
| `round_controller.h` | 2026-07-14 16-bit 版 | 事件/ACK 语义 |
| `competition_contract.h` | 2026-07-13 | 目标/事件/结果契约 |
| `register_map.md` | 2026-07-13 | 候选寄存器偏移（未冻结）|
| `board_io.h` | V2 双路版 | staging/commit/active 协议 |
| `preprocess_apb_cdc_contract_draft_20260711.md` | 2026-07-11 | CDC 契约草案 |
| `第十届集创赛…细则_0710.md` | 2026-07-10 | 官方比赛任务与评分 |

## 附录 B：合法组合矩阵（从 adapters 实现复述）

```
action  reason               is_target  decision  execution        类别
GRAB    TARGET_MATCH         1          EXECUTE   REQUESTED        A
SKIP    COLOR_MISMATCH       0          SKIP      SKIPPED_NON_...  B
SKIP    SHAPE_MISMATCH       0          SKIP      SKIPPED_NON_...  B
SKIP    SIZE_DIFF_NOT_10MM   0          SKIP      SKIPPED_NON_...  B
SKIP    SIZE_DIFF_OVER_5MM   0          SKIP      SKIPPED_NON_...  B
NONE    ARM_NOT_READY        1          NONE      BLOCKED          C
NONE    OPERATOR_ABANDON     0          NONE      BLOCKED          C
NONE    STABILITY_TIMEOUT    0          NONE      BLOCKED          C
NONE    OBSERVATION_UNKNOWN  0          NONE      BLOCKED          C
NONE    TARGET_INVALID       0          NONE      BLOCKED          C
NONE    ARM_FAULT            1          NONE      FAULT            D
*       * (矛盾组合)         *          ERROR     FAULT            兜底
```
