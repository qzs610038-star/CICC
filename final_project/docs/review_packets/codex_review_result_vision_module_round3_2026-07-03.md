# Codex 三次审查结果 - 视觉模块 v4.2 手册草案前 Gate

- 日期: 2026-07-03
- 审查对象: `codex_review_packet_vision_module_2026-07-03.md` v4.2 与 `vision_module_implementation_plan.md` v4.2
- 结论: **B4 已闭环, 可作为寄存器手册契约基线; B5 状态订正准确。** 当前可起草《FPGA 与 CPU 视觉交互寄存器手册》草案, 状态写为"架构方向通过, RTL/SoC 前置待 B1 soc.h 摘要闭合"。在生成 `generated_soc_summary_YYYY-MM-DD.md` 前, 仍不解除 RTL/SoC 执行前 Gate。

## B4 判断

v4.2 新增的 `CFG_COMMIT` / `CFG_STATUS` 与 staging -> commit -> active shadow 三段机制可以解决第二轮指出的多寄存器半新半旧问题:

- CPU 写 `CFG_*` 时只更新 APB 域 staging。
- CPU 写 `CFG_COMMIT[15:0] = config_seq` 后, 这组 staging 才进入待生效状态。
- 像素域仅在 VSYNC 边界整组切换 active shadow。
- `CFG_STATUS` 提供 `active_seq` / `pending_seq`, CPU 可以确认配置是否已生效。
- 首版采用 ROI、阈值、亮度门限、CPU OSD bbox 统一提交, 不做分组提交, 语义足够清楚。

这可以作为寄存器手册契约基线。手册中建议补 4 个小边界, 不作为退回阻塞:

1. `config_seq` 复用/回绕: 建议要求 CPU 每次 commit 使用不同序号; 16-bit 回绕允许, 但不得连续两次使用相同值。
2. pending 覆盖规则: 若 CPU 在前一个 pending 尚未 VSYNC 生效前再次 commit, 首版建议定义为"最后一次 commit 胜出", 即 pending staging 被最新完整 staging 覆盖。
3. 复位默认: 复位后 `active_seq=0`, `pending_seq=0`, active shadow 使用安全默认阈值/ROI/OSD 关闭或全画面 ROI, 具体默认值在手册表中列出。
4. staging 读回: 若 R/W `CFG_*` 被 CPU 读回, 读到的是 staging 还是 active 需要写清。建议 CPU 读 `CFG_*` 得 staging, 读 `CFG_STATUS.active_seq` 判断像素域是否已采用。

## B5 判断

B5 状态订正准确:

- B1 的文档约束已收敛: 字段清单、`soc.h` 真源、编译期 `#ifndef`、运行期 `REG_MAGIC` 都已经写进 plan。
- B1 的实物前置未满足: 当前 `final_project` 下仍未看到正式 `soc.h` 快照/摘要, `bsp.h` 仍是占位地址。
- 因此正确状态是: **架构方向 Gate 解除; RTL/SoC 执行前 Gate 保留, 待 generated_soc_summary 闭合。**

## 仍需补齐

在补齐 `generated_soc_summary_YYYY-MM-DD.md` 后, 最终解除 RTL/SoC 执行前 Gate 还应检查:

1. `soc.h` 摘要包含 UART0/UART1、CLINT、PLIC、APB user window、AXI user window、DDR base/size、`SYSTEM_CLINT_HZ` 或等价时钟。
2. 选定的 `io_apbSlave_x_*` 端口空闲, 并记录 `PADDR` 宽度、`PSEL/PREADY/PSLVERROR` 语义。
3. CPU BSP 中占位地址被 `soc.h` 真源替代, 或编译期强制缺失即失败。
4. OSD 真正插入 `top.v` 前另交小包审查 fanout/timing: 选择 `wb1_*` 主通道不等于实现 OSD 时零改顶层。

## 对送审三问的回答

1. **B4 是否闭环**: 是。可进入寄存器手册草案, 只需在手册中补 config_seq、pending 覆盖、复位默认、staging 读回这 4 个边界。
2. **B5 表述是否成立**: 成立。应保持"架构方向 Gate 解除、RTL/SoC 执行前 Gate 保留"。
3. **补 generated_soc_summary 后还需什么**: 只需按上面的 SoC 摘要四项与 OSD 插入小包审查执行; plan/手册层没有新的方向性阻塞。

## 小问题

Packet 标题仍写"v4.1 二次复核", 但正文模式和计划来源已是 v4.2 三次复核。建议顺手改标题, 这是文档一致性问题, 不影响技术结论。
