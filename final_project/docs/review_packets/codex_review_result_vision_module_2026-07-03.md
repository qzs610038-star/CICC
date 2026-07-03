# Codex 审查结果 - 视觉模块架构与寄存器配置实现计划

- 日期: 2026-07-03
- 审查对象: `codex_review_packet_vision_module_2026-07-03.md` 与 `vision_module_implementation_plan.md`
- 结论: **方向通过, 但不能直接解除全部待复核状态进入 RTL 扩大修改**。允许进入寄存器手册/decision_log 起草；进入 RTL/SoC 执行前必须补齐下列阻塞前置条件。

## 阻塞项

### B1. 正式 SoC/APB3 实例和地址仍未锁定

当前方案说 APB3 Slave 实例号、基地址、APB 时钟频率均以后续 SoC 配置为准, 这是正确约束, 但还不足以支撑 RTL/CPU 代码落地。正式执行前必须从当前 Efinity 生成物导出并记录:

- `soc.h`: UART0/UART1、CLINT、PLIC、APB user window、AXI user window、DDR base/size。
- SoC 顶层端口: 选用的 `io_apbSlave_x_*` 是否空闲、PADDR 宽度、PSEL/PREADY/PSLVERROR 语义。
- `SYSTEM_CLINT_HZ` 或实际 UART/定时器输入时钟。

证据:
- 当前 `bsp.h` 仍有占位 `IO_APB_SLAVE_0_INPUT 0xF8100000u`。
- 项目文档已明确官方例程至少有两套地址/频率风格, Review Packet 必须列出最终 `soc.h` 地址摘要。
- 官方例程能证明 APB3 访问方式可行, 但不能证明本工程的 Slave 0 可用；`01_eth_test-v4/source/top_soc.v` 中 `io_apbSlave_0_*` 接到了 `dma_apb3_*`。

最小修复建议:
1. 先生成或定位本工程 QCRV32/Sapphire SoC 配置输出。
2. 将地址摘要加入下一版 Review Packet 或寄存器手册附录。
3. CPU 侧使用 `#include "soc.h"` 后用 `#ifndef IO_APB_SLAVE_x_INPUT` 编译期报错；运行期再用 `REG_MAGIC` 二次探测。

### B2. OSD/feature_extract 目标视频通道存在 wb0/wb1 不一致

方案写主链路使用 `wb0_data_out[47:0]`, 但当前 `top.v` 中 DSI 和 HDMI 输出实际都消费 `wb1_*`:

- `dsi_tx_top_inst1.pixel_*_i` 连接 `wb1_vs_out/wb1_hs_out/wb1_de_out/wb1_data_out`。
- HDMI 串化也读取 `wb1_data_out[47:24]` 与 `[23:0]`。
- `wb0_*` 由 `u0_white_balance` 产生, 但未接到当前 HDMI/DSI 输出链路。

这不是路线否决, 但会直接决定 OSD 插入点、feature_extract 取数点、主相机是哪一路、以及双通道预留策略。若不先锁定, 很容易出现 CPU 读的是一路特征、显示调的是另一路画面。

最小修复建议:
1. 在方案中把“第一版主相机”改成真实信号名: 现状应优先按 `wb1_*` 审查, 或先修改顶层让 `wb0_*` 成为显示链路。
2. 执行前做一次 `top.v` fanout 表: `rgb0/rgb1 -> wb0/wb1 -> DSI/HDMI -> OSD/feature_extract`。
3. 寄存器手册中不要写抽象的 ch0/ch1 即可, 必须绑定到 `wb0_*`/`wb1_*` 与物理摄像头位置。

### B3. SYS_STATUS 同时 R/W 承载 valid 与 ack, 语义容易造成读改写误清

Register Map 将 `SYS_STATUS` 标为 R/W, bit0 是 FPGA 置位的 `feature_valid`, bit1 是 CPU 写 1 的 `feature_ack`, 高 16 位是 `frame_id`。这比读清零好, 但如果软件按普通 RMW 写状态寄存器, 可能把只读状态位、ack 脉冲和帧号混在一起。

最小修复建议:
- 拆成 `STATUS` 只读、`ACK` 写 1 脉冲、`FRAME_ID` 只读, 或至少把写掩码语义写死: 只有 bit1 写 1 有效, 其他写入忽略。
- `feature_valid` 不应靠 APB 读侧清除；像素域只有在同步后的 ack 且 frame_id 匹配时清除。

## 主要风险

### R1. CDC 方案方向正确, 但需要从"两级同步"升级为完整事件协议

多 bit 参数用 VSYNC 影子寄存器、单 bit 用同步器是合理起点。但 `feature_valid`/`feature_ack` 是跨域事件, 仅写“两级同步”不够完整。建议在寄存器手册里定义:

- FPGA 在帧结束一次性锁存 `RES_*` 和 `frame_id`, 再置 `valid`。
- CPU 读取同一个 `frame_id` 的稳定快照后写 `ack_frame_id` 或 `ack_toggle`。
- FPGA 只在看到同步后的 ack 且编号匹配时清 valid。

`frame_id` 不建议从 APB 域影子到像素域作为主机制；它天然由像素域产生并随结果快照到 APB 读侧。若要跨域传多 bit 帧号, 使用“valid 期间数据保持稳定 + ack 后再更新”比 Gray 编码更直观。

### R2. pixel_data_en 仍是视频链路高耦合复位/使能, OSD 插入必须先做局部验证

PR1 已记录 `pixel_data_en` 由 DSI 输出并被 AXI、debayer、framebuffer、HDMI 多处当 reset/enable 使用。当前 `top.v` 也显示 `pixel_data_en` 驱动 debayer、white_balance、AXI interconnect reset 等路径。OSD 插在 white_balance 后是可行候选, 但必须保证:

- 不改变 `pixel_data_en` 产生链路。
- OSD 自身 reset 跟随像素域同步 reset, 不反向影响 DSI/HDMI。
- HDMI 的 48-bit 双像素到 24-bit 单像素串化保持时序和奇偶顺序。

### R3. 颜色阈值需要双边界和亮度/饱和度保护

通道差值/比例方案比 HSV 硬件转换更适合首版 FPGA, 但单阈值不够抗曝光/AWB。建议寄存器手册至少预留:

- 每色 `min/max` 或 `lower/upper` 边界。
- 简单亮度门限 `sum_min/sum_max`。
- mask 面积上下限仍放 CPU 判定, FPGA 只输出 area/bbox/raw stats。

### R4. 多物体、PLIC 中断、尺寸阈值不要放进首版 RTL

packet 对这三项的降级判断合理: 首版单物体最大 bbox、CPU 查表做尺寸分类、轮询不接 PLIC。它们应写入 decision_log 的后续阶段, 不作为当前通过条件。

## 对 8 个问题的判断

1. **总体路线**: 可作为决赛主路线, 前提是先完成 B1/B2/B3。APB3 + 轮询 + OSD 符合当前项目边界。
2. **CDC**: 方向对, 但要把 valid/ack 定义成“稳定快照 + 帧号匹配 ack”事件协议。`frame_id` 不需要 Gray 编码作为首选。
3. **OSD 数据源**: 双框方案可行。为避免误用, 建议命名为 `LIVE_*` 与 `CPU_OSD_*`, 不要只靠地址区分。
4. **APB3 占用与基地址**: “待定 + soc.h 为准”是必要条件, 不是充分条件。需要编译期宏检查 + `REG_MAGIC` 运行期探嗅。
5. **OSD 插入点**: 必须先做 `top.v` fanout/diff, 尤其澄清当前显示链路用 `wb1_*`。
6. **颜色阈值**: 可行, 但寄存器需预留双阈值/亮度门限, CPU 负责最终鲁棒性。
7. **心跳看门狗**: 建议按毫秒超时由 CPU/CLINT 标定, FPGA 可另以帧计数近似显示状态。首版超时动作限定为 OSD/状态告警, 不自动触发 myCobot 急停或 freeze 视觉流水线, 避免安全语义混乱。
8. **解除待复核**: 可解除“方案方向待复核”, 允许起草寄存器手册和更新 decision_log；不能解除“RTL/SoC 执行前 Codex Gate”, 进入实现前需带 B1/B2/B3 的修订包再审。

## 允许的下一步

1. 修订 `vision_module_implementation_plan.md`: 明确 `wb1_*`/`wb0_*` 主通道选择, 拆分 `STATUS/ACK`, 补寄存器预留。
2. 起草《FPGA 与 CPU 视觉交互寄存器手册》, 但标注 `soc.h` 地址待生成。
3. 生成 SoC 地址摘要与 top fanout 表后, 再提交 RTL 执行 Review Packet。
