# 架构审计报告 — Claude Fable 5

> **审计范围：** 系统架构、接口契约、决赛执行方案、PNR/SoC/CDC/OSD/CPU/机械臂全链路只读审计
> **审计来源：** Claude Fable 5（本日最高可用模型）
> **审计日期：** 2026-07-13
> **审计方式：** 只读，不修改文件，不构建/仿真/烧录/机械臂动作
> **审计依据：** 官方细则 → AGENTS.md 硬边界 → CURRENT_STATE.md → 决赛主方案 → 真实源码/工程

---

## 一、总体裁决

| 维度 | 裁决 | 一句话说明 |
|------|------|-----------|
| 系统架构 | **CONDITIONAL PASS** | FPGA 视频/统计/OSD + 板上CPU 四任务/逐轮/控制 的职责划分正确，不必推倒重来 |
| 当前执行方案 | **CONDITIONAL PASS** | Gate/F1-F3 分层方向正确，但实现尚未跨过 PNR + SoC/APB + main集成 三个硬门 |

---

## 二、P0 阻塞表（6 项）

| # | 阻塞 | 精确证据 | 对 F1/F2 影响 | 最小修复或验证 | 验收条件 | 回退点 |
|---|------|----------|--------------|--------------|---------|-------|
| 1 | **PNR 1776 IO 无 placement** | `team_integration_merge_review_20260713.md:43-45`；`mem_test.xml:123-124` peri-syn 关闭；`constrain.sdc:12` 器件 Ti180J484 与 TJ375N529 不匹配 | F1/F2 全阻塞 | 核对 periphery 导出/器件/peri-syn 开关；不盲绑管脚 | 生产 PNR 无 outpad 断言 | 回退到仅 map，不宣称 bitstream |
| 2 | **SoC PLL 资源未证实可接** | `mem_test.peri.xml:216/249/339` 三块 PLL_BL 全被视频占用；`CURRENT_STATE.md:110-116` | F1 缺 APB/MMIO，F2 更无从谈起 | 只做 GUI 资源审查，确认硬 SoC 是否有合法 PLL 路径 | 有同次生成的 `soc.h` 与合法时钟/复位证据 | 冻结 SoC 接入决定 |
| 3 | **`main.c` 仍绕过新控制器** | `main.c:340-388` 直接调旧 `task_matcher_evaluate()`，round 推进占位为空；`CURRENT_STATE.md:139-143` | F1 无法保证"一轮一次/正确 SKIP/ARM_DISABLED" | 在 `ARM_DISABLED` 下只接 `round_controller`，不接动作 | 20 轮无动作流程板上可跑 | 回退到 Host，不宣称板级 |
| 4 | **APB/CDC/OSD 仅文档候选** | `register_map.md:1-5` 未冻结；`feature_snapshot.v:42-86` 仅像素域 valid/ack，无跨时钟域同步器 | F1 结果无法原子显示 | 先做 MAGIC/heartbeat → 固定结果 → OSD 最小闭环 | 同轮 frame_id/round_seq 原子提交可见 | 回退到固定结果显示 |
| 5 | **五色真源不完整** | `board_io.h:51-62` FG_AREA_AVAILABLE 默认 0；`board_io.h:153` TARGET_SEL 仅 2-bit 不支持白/黑；`task_matcher.c:281-291` 正式构建强制清空目标 | F1 白/黑检测不可靠，目标输入不全；F2 无意义 | 优先补板上目标输入；白/黑至少暴露同帧统计 | 板上五色目标可注入，白/黑不靠猜 | 回退为明确 UNVERIFIED，不误抓 |
| 6 | **机械臂板级安全门未过** | `io_pin_map.md:7-29` 六项全 FAIL；`mycobot_board_bringup_operator_sop_20260712.md:12-17` | F2 禁止启用 | 严格按 T0→S4，只读回环前不接真臂 | T0 全 PASS + UART2 100 帧回环 | 保持 F1，机械臂禁用 |

---

## 三、冲突与错误假设（5 项）

| 类型 | 描述 | 证据位置 |
|------|------|---------|
| **CONFIRMED** | 任务三评分正文 25/25/50 与评分表 1/1/1 不一致，只能现场确认 | 细则 L88-109 |
| **CONFIRMED** | F1 在回退表像"机械臂禁用的软件闭环"，在主方案正文又要求"真实摄像头+板上CPU+OSD+20轮"，口径不一致 | 主方案 L571-576 vs L688-710 |
| **CONFIRMED** | Host 795/795 或 compile-only 不能外推为板级准备度 | CURRENT_STATE.md L140-141 |
| **INFERENCE** | 1776 IO 根因是工程模式/器件/Periphery 绑定失配，不是业务 RTL 端口暴涨 | mem_test.xml L123-124 + constrain.sdc L12 |
| **UNVERIFIED** | `LIVE_FG_AREA` 字段名不一定最终定版，但"同帧白黑判别真源"是 P0 必需 | board_io.h L51-53 |

---

## 四、最小架构优化

| 操作 | 内容 |
|------|------|
| **KEEP** | FPGA/CPU 现有职责边界；`round_controller + competition_round_transaction` 双层拆分 |
| **CHANGE** | `main.c` 必须只通过 `round_controller` 驱动轮次与 `ARM_DISABLED`；production/debug 必须显式分离，合成源不能默认开启 |
| **DEFER** | 双摄完整闭环、任务三/四高置信尺寸标定、真实机械臂整场，均在 7/17 冻结后 |
| **STOP** | 盲绑 1776 IO、猜测性接 SoC、未过 T0 推进 UART2 真臂、继续留 `main.c` 在旧 matcher 直通路径 |

---

## 五、截止日前关键路径

```
FPGA/SoC 线：periphery/PNR根因 → 生产/调试构建拆分 → GUI 确认 SoC PLL → MAGIC/heartbeat
CPU/OSD 线：register_map 收口 → main 接 round_controller(ARM_DISABLED) → 固定结果 OSD → 真快照判定
机械臂线：T0 真源表 → UART2 回环/只读 → 仍禁动作

汇合 Gate：先 Gate B1 + 最小 APB，再 Gate C
F1 冻结点：单摄真实输入 + 板上 CPU + OSD + 正确 SKIP + 无动作 20 轮
F2 启用条件：T0 全 PASS + UART2 只读回环 PASS + round_controller→arm_controller 唯一请求路径已接通
```

---

## 六、文件级建议修改清单

| 文件 | 建议改动 |
|------|---------|
| `final_project/fpga/efinity/mem_test.xml` | 核对 periphery 生成与 `peri-syn-*` 策略 |
| `final_project/fpga/efinity/constrain.sdc` | 核对器件/Interface Designer 生成源 |
| `final_project/fpga/rtl/top/top.v` | 拆分 production/debug 入口，合成源不默认 |
| `final_project/cpu/app/src/main.c` | 接入 `round_controller`，移除旧直通闭环 |
| `final_project/integration/register_map.md` | 补齐五色目标、帧统计、原子提交真源 |
| `final_project/cpu/app/include/board_io.h` | 与 register_map.md 对齐，补五色目标/帧统计 |
| `final_project/integration/io_pin_map.md` | 继续作为 F2 唯一放行表 |

---

## 七、最终待确认问题（5 个）

1. **任务三评分比例**：按 25/25/50 还是 1/1/1？需现场专家组确认
2. **SoC PLL 可行性**：Interface Designer 在当前视频工程里是否给硬 SoC 留有合法 PLL 重规划路径？
3. **白/黑真源形式**：最终走 `LIVE_FG_AREA` 还是 `sum_y + roi_pixel_count`？
4. **单/双摄决策**：7月15日后若双摄未稳，是否正式锁定单摄？
5. **机械臂底座复核**：外部基准复核何时能形成可签字证据？

---

## 附录：审计方法

- 3 个低配 subagent 并行采集证据（规则/Gate、FPGA/SoC/PNR、CPU/OSD/机械臂）
- 主模型只打开影响最终结论的精确证据范围，不重读全文
- 代码发现优先使用 `D-cicc_cbm_link` 图谱（4514 nodes/10958 edges），最终证据回到真实源文件
- 所有结论绑定精确文件路径+行号

---

## 补充：Codex 复核与独立探索修复记录（2026-07-13）

> 本节由 Codex 在 Fable 5 原始审核完成后追加。上方 Fable 审核正文保持原样；追加前原文共 99 行，SHA-256 为 `0604B8AE013EE2794F7F2DA079FDD9872AF9C6472A6FC3BBBA4382D5A9C99550`。
>
> 探索修复源自独立分支 `codex/fable5-remediation-20260713`、提交 `758e8649ea5b27e1000cc37936c2828919bd72a8`；本次 CPU/Fable 集成分支已吸收该提交并进入 PR 验收流程。因此下述测试结果代表集成源码证据，但仍不代表 bitstream、板级能力或真实机械臂闭环已经完成。

### 1. Codex 对 Fable 方案的复核意见

Codex 认可 Fable 的总体裁决：保持现有 FPGA/CPU 职责边界，不推倒重来；当前应优先解决 production 构建、SoC/APB、逐轮事务和真实观测闭环。以下几点需要补充或纠偏：

1. **PNR 根因不能由单一配置项直接推出。** `peri-syn=0` 在初赛和赛方衍生工程中也存在，不能单独证明它导致 1,776 个 IO 无 placement；`constrain.sdc` 中 Ti180J484 的生成头与 TJ375N529 工程真源不一致，属于必须核查的 provenance 风险，但也不能仅凭该注释断定实际器件已选错。应在 ASCII 路径隔离副本中通过 Interface Designer/periphery 重新生成并比较真实产物，禁止盲绑管脚或手改生成文件。
2. **白/黑真源不是 `LIVE_FG_AREA` 与亮度统计二选一。** 正式判定至少需要同一 `frame_id` 下的一致快照 `{status, bbox, fg_area, roi_pixel_count, sum_y}`；CPU 必须先验证快照一致性，再联合使用前景面积和亮度统计。任何单字段都不足以证明白/黑稳定。
3. **完整控制器和轻量事务层原先存在事件语义漂移。** `round_controller` 原为 8-bit `event_seq`，而 `competition_contract` 为 16 bit；非法状态的新事件还会在确认状态机可接受前被记录和 ACK。该问题适合先在寄存器无关的纯 C 层收口，再映射 APB。
4. **F2 放行条件应继续服从决赛主方案和 Gate D。** 除 T0、UART 回环/只读和唯一动作请求路径外，还必须包含 180° 点位、至少 5 轮低速带载、电气/共地、急停/断电、底座固定及无跌落证据。Fable 正文中的简写不能替代完整安全门。
5. **`main.c` 接入方向正确，但不能先于接口真源。** 在正式 `soc.h`、APB 时钟/复位、`TARGET_CFG/OPERATOR_EVENT/EVENT_ACK/RESULT_COMMIT` 和 OSD 原子提交尚未联合审查前，不应通过虚构 MMIO 地址把 Host 适配直接接入板上主循环。

### 2. 独立探索分支的主要结果

探索分支：[`codex/fable5-remediation-20260713`](https://github.com/qzs610038-star/CICC/tree/codex/fable5-remediation-20260713)

- `round_controller` 的事件序号、ACK 序号和 last-consumed 序号统一为 16 bit；新增 `ACK_NONE/ACCEPTED/REJECTED`，非法状态的新事件会显式拒绝且不迁移状态，重复和过期事件不会二次消费。
- 完整控制器与轻量 `competition_contract` 统一采用 16-bit 半范围序号规则，支持 `0xFFFF -> 0x0000 -> 0x0001` 回绕；轻量契约新增用例曾先以 `42/45` 失败，修复后 `45/45` 通过。
- `round_controller` 增加非法状态、过期序号、回绕、20 轮 SKIP、1000 个确定性随机事件和最终恢复测试；Host 当前源码重建为 `4919/4919 passed`，随机流在 `ARM_DISABLED` 下始终无动作请求。
- FPGA `top.v` 改为 production 安全默认：未定义 `COMPETITION_DEBUG_SYNTHETIC` 时预处理与 HDMI 均选择真实输入；只有显式 debug 宏才启用合成源。该修改只通过静态边界检查，尚无新的 production/debug Efinity map、PNR、时序或 bitstream 证据。
- `board_io.h` 的占位地址警告增加 MSVC/GCC 兼容处理，并新增可复现的 Host 测试脚本；未把占位地址提升为正式 APB 事实。
- 探索提交刻意没有修改 `main.c`、`mem_test.xml`、`.peri.xml` 或 `constrain.sdc`，也没有运行机械臂动作。

探索提交内的详细证据文件：`final_project/docs/review_packets/fable5_remediation_checkpoint_20260713.md`。

### 3. 对后续工作的影响与未完成边界

该探索结果可以作为后续实现的候选修复和测试基线，但不能作为已合入主线或板级闭环的声明。建议继续按以下顺序推进：

1. FPGA/SoC 队员分别生成 production/debug 构建证据，并通过 Interface Designer/periphery 关闭 1,776 IO/outpad PNR 阻塞。
2. 提供同一次 SoC 生成的 `soc.h`、APB 基址、时钟和复位证据；联合冻结五色四任务目标、16-bit 操作事件/ACK、结果理由和原子提交字段。
3. 在 Host 解码/ACK 测试通过后，才把 `main.c` 接入 `round_controller`，第一版强制 `ARM_DISABLED`，先完成固定结果 OSD，再接同帧真实快照。
4. 机械臂继续保持 NO-GO，直到完整 F2/Gate D 全部通过；PC、`pymycobot` 或临时脚本不得进入正式比赛闭环。

以上补充不覆盖 Fable 原始审核意见，而是记录其后续独立复核、已尝试的候选优化以及仍需团队协作完成的硬件验证边界。
