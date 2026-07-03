# Codex 二次审查结果 - 视觉模块 v4.1 RTL 前 Gate

- 日期: 2026-07-03
- 审查对象: `codex_review_packet_vision_module_2026-07-03.md` v4.1 与 `vision_module_implementation_plan.md` v4.1
- 结论: **不建议解除 RTL/SoC 执行前 Codex Gate**。v4.1 已基本收敛第一轮 B2/B3/R1/R3/R4, 但还存在 2 个执行前阻塞项。可继续起草寄存器手册草案, 但必须先把下面的协议约束写入 plan/register map 后再宣称 Gate 解除。

## 阻塞项

### B4. 多寄存器配置缺少原子提交协议

问题:

v4.1 写到 `SYS_CTRL`、阈值、ROI、`CPU_OSD_BBOX_*` 在 VSYNC 空白期整组锁存到像素域影子寄存器, 但没有定义 CPU 何时算"一组参数写完"。如果 CPU 在一帧内分次写 `CFG_ROI_TL` / `CFG_ROI_BR` 或多色阈值, VSYNC 可能锁存到"半新半旧"配置。这个风险不能留到 RTL 阶段靠实现者猜。

改成什么:

在 Register Map 中增加配置提交协议, 推荐最小方案:

- 新增 `CFG_COMMIT` W 寄存器: CPU 写入 `config_seq[15:0]` 表示一组参数写完。
- 新增 `CFG_STATUS` R 寄存器: FPGA 回报 `active_config_seq[15:0]` 和 `pending/applied` 状态。
- APB 域维护 active/staging 两套配置, CPU 只写 staging；像素域仅在 VSYNC 边界看到新的 `config_seq` 后整组切换到 active shadow。
- ROI、颜色阈值、亮度门限、CPU OSD bbox 至少要走同一套 commit 语义；若要分组, 明确 `CFG_COMMIT` 位域分别提交 `ROI/THRESH/OSD`。

验收标准:

寄存器手册必须明确: CPU 写多个配置寄存器时, 未写 `CFG_COMMIT` 前 FPGA 仍使用上一组完整配置；写 commit 后最多在下一帧生效, 且不会半帧/半组生效。

### B5. B1 仍是"文档约束已补齐", 不是"RTL/SoC 前置已满足"

问题:

v4.1 第 0 节已把 `soc.h` 摘要、空闲 `io_apbSlave_x`、PADDR、PSLVERROR、时钟等列成前置条件, 这是正确的。但当前 `final_project` 下没有正式 `soc.h` 快照, `bsp.h` 仍保留占位 `IO_APB_SLAVE_0_INPUT 0xF8100000u`。因此不能说 SoC/RTL 执行 Gate 已完全解除。

改成什么:

在 packet/plan 结论中把 B1 状态改为:

- "B1 文档约束已收敛, 实物前置未满足"。
- 允许寄存器手册起草时保留 `IO_APB_SLAVE_x_*` 占位。
- 进入 SoC/IP 配置和 CPU BSP 改造前, 必须补一份 `generated_soc_summary_YYYY-MM-DD.md` 或等价附件, 列出 UART/CLINT/PLIC/APB/AXI/DDR/时钟/Slave 端口摘要。

验收标准:

没有当前工程生成的 `soc.h` 摘要时, 不应写"解除 RTL/SoC 执行前 Gate";最多写"解除架构方向 Gate, 保留 SoC 地址 Gate"。

## 已收敛项

- B2 已收敛: 真实 `top.v` 中 DSI 和 HDMI 都消费 `wb1_*`, v4.1 将主通道改为 `wb1_*` 是对的。
- B3 已收敛: `SYS_STATUS` 只读、`SYS_ACK` 只写并携带 `frame_id` 比原 R/W 混装安全。
- R1 大体收敛: `valid` 期间稳定快照 + `ack.frame_id` 匹配清 valid 是可落地的首版协议。补充建议: `frame_id` 16-bit 回绕可接受, 但文档需规定 CPU 不得 ack 非当前 `SYS_STATUS.frame_id`。
- R3 已收敛: 双边界阈值 + 全局 luma 门限可作为首版基线。
- R4 已收敛: 多物体、PLIC、FPGA 尺寸分类不进入首版; 心跳不触发 myCobot 动作/急停。

## 对 8 个问题的回答

1. **B1/B2/B3**: B2/B3 可通过; B1 仅文档收敛, 实物前置未满足, 不能完全解除 SoC 执行 Gate。
2. **P4 事件协议**: 基本闭合。建议寄存器手册规定 CPU 只 ack 当前读到的 `frame_id`, FPGA valid 未被 ack 前不覆盖快照; 若 CPU 掉帧, FPGA 保持最新或保持待 ack 二选一必须写清。
3. **B2-a 零改顶层**: 作为第一版取 `wb1_*` 成立。但插入 OSD 本身仍会改顶层连线; "零改顶层"只对选择主通道成立, 不等于 OSD 实现时不改 `top.v`。
4. **Register Map 契约基线**: 可作为基线, 但必须补 `CFG_COMMIT/CFG_STATUS` 或等价原子提交机制。
5. **CDC 多 bit 写原子性**: 当前未解决, 是本轮最主要阻塞项。按 B4 修改。
6. **颜色阈值鲁棒性**: 首版够用。每色独立 luma 可预留地址, 不要求首版实现。
7. **心跳看门狗**: 语义安全。建议手册规定首版超时只置状态/OSD 告警和重置视觉状态机, 阈值可先按毫秒由 `SYSTEM_CLINT_HZ` 标定。
8. **是否解除 Gate**: 不解除 RTL/SoC 执行前 Gate。可解除"架构方向 Gate", 允许起草寄存器手册草案; 待补 B4 并附 B1 的 soc 摘要前置后再最终解除。

## 下一步建议

1. Claude 直接修改 plan: 加 `CFG_COMMIT/CFG_STATUS` 和 staging/active/shadow 三段语义。
2. 修改 packet 顶部对照表: B1 不标"已满足", 改为"文档约束已收敛, 实物摘要待生成"。
3. 起草寄存器手册草案可以开始, 但标题状态应写"架构方向通过, RTL/SoC 前置待 B4/B1 闭合"。
