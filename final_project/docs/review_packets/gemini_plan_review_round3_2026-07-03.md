# 视觉模块 Gemini 实施计划 — 第三轮修改清单（给 Gemini）

> 来源：Codex Review 结果 [codex_review_result_vision_module_2026-07-03.md](codex_review_result_vision_module_2026-07-03.md)
> 已核实：Claude 已逐条核实 Codex 的三项阻塞前置条件，关键技术事实（B2: wb0/wb1）已对照 [final_project/fpga/rtl/top/top.v](../../fpga/rtl/top/top.v) 与赛方原工程 `TJ375N529_SC431HAI2LCD_Demo_V3/src/top.v` 确认成立。
> 被修改对象：[vision_module_implementation_plan.md](../architecture/vision_module_implementation_plan.md)（当前 v3）
> 状态：本清单为给 Gemini 的修改指令，每项按"问题 → 改成什么"格式。改完后回贴新版，Claude 再过一轮核对，通过后才起草《寄存器手册》。

---

## 0. Codex 总体判定（已核实有效）

- **方向通过**：APB3 + valid/ack 轮询 + 硬件 OSD 可作为决赛主路线。
- **允许**：起草《寄存器手册》、更新 decision_log 记录"方向通过"。
- **不允许**：在三项阻塞前置条件（B1/B2/B3）补齐前，不得进入 RTL/SoC 实现；进入实现前需带修订包再过一次 Codex Gate。

---

## 1. 阻塞前置条件（必改）

### B1. SoC/APB3 实例、基地址、时钟未锁定

- **问题**：当前 [bsp.h:43](../../cpu/app/include/bsp.h#L43) 仍是占位 `IO_APB_SLAVE_0_INPUT 0xF8100000u`。"实例号待定 + 以 soc.h 为准"是必要条件，但作为 RTL 落地约束还不够充分；官方例程（`01_eth_test-v4/source/top_soc.v` 中 `io_apbSlave_0_*` 接到 `dma_apb3_*`）证明 Slave 0 可能已被占用，但不能证明本工程哪个 Slave 空闲。
- **改成**：
  1. 在计划中新增一节"执行前置：SoC 地址摘要"，明确正式实现前必须从当前 Efinity 生成物导出并记录：UART0/UART1、CLINT、PLIC、APB user window、AXI user window、DDR base/size、`SYSTEM_CLINT_HZ`、选用 `io_apbSlave_x_*` 的空闲性 + PADDR 宽度 + PSEL/PREADY/PSLVERROR 语义。
  2. CPU 侧约定：`#include "soc.h"` 后用 `#ifndef IO_APB_SLAVE_x_INPUT` 编译期报错；运行期再用 `REG_MAGIC` 双重探测。
  3. 在计划顶部状态行旁标注"B1 待生成 soc.h 地址摘要"。
- **说明**：B1 是"待生成"性质，可在手册中标注为待定，不撤方案方向。

### B2. OSD/feature_extract 目标通道：主显示链路实际消费 wb1_*，不是 wb0_*

- **核实证据**（Claude 已对照当前工程与赛方原工程，行号一致）：
  - [top.v:1291-1310](../../fpga/rtl/top/top.v#L1291-L1310)：`wb0_*` 由 `u0_white_balance` 产生，**定义但未接进显示链路**。
  - [top.v:1398-1401](../../fpga/rtl/top/top.v#L1398-L1401)：`dsi_tx_top_inst1.pixel_*_i` 接 `wb1_vs_out/wb1_hs_out/wb1_de_out/wb1_data_out`。
  - [top.v:1468-1474](../../fpga/rtl/top/top.v#L1468-L1474)：HDMI 串化也读 `wb1_data_out[47:24]` 与 `[23:0]`。
  - 赛方原工程 `TJ375N529_SC431HAI2LCD_Demo_V3/src/top.v` 同样行号即 `wb1_*`，**PR1 照迁未改动**，这是赛方原样。
- **问题**：当前计划正文与 Register Map、3.2 节 OSD 全部以 `wb0_*` 为主通道。若照此起草手册并落地 RTL，会出现"CPU 读 wb0 特征、OSD 叠在 wb0、但 HDMI 显示的是 wb1"，即"读一路特征、调另一路画面"。
- **改成**：
  1. 第一版"主相机"绑定到真实信号名。二选一并明确：
     - **方案 B2-a（推荐，零改顶层）**：把第一版主相机定义为 **wb1_*** 那一路（现状显示链路）。feature_extract 取 `wb1_data_out[47:0]`，OSD 叠在 `wb1_*` 之后、DSI/HDMI 之前。wb0 先不用。
     - **方案 B2-b**：改顶层让 `wb0_*` 成为显示链路，feature_extract/OSD 走 wb0。这涉及 [top.v:1398-1401](../../fpga/rtl/top/top.v#L1398-L1401) 与 1468-1474 改写，**命中 Codex Gate**（顶层连接），需单独再审。
     除非有强理由，默认选 B2-a，避免动顶层。
  2. 在计划中加一张 `top.v` fanout 表：`rgb0/rgb1 → wb0/wb1 → DSI/HDMI → OSD/feature_extract`，把每段真实信号名钉死。
  3. Register Map 注释/手册中不要用抽象的 ch0/ch1，必须绑定到 `wb0_*`/`wb1_*` 与物理摄像头位置（哪路 MIPI 对应 wb1）。
  4. "双通道预留"要重述：第一版 wb0 不进识别/显示，预留为后续侧视相机；主相机俯视 = wb1 对应的那路物理摄像头。

### B3. SYS_STATUS 拆分：valid/ack/frame_id 不要混装在一个 R/W 字

- **问题**：Register Map `0x008 SYS_STATUS` 标 R/W，bit0 是 FPGA 置位的 `feature_valid`、bit1 是 CPU 写 1 的 `feature_ack`、高 16 位是 `frame_id`。若软件按普通 RMW 写状态寄存器，会把只读状态位、ack 脉冲和帧号混在一起，误清状态。
- **改成**（二选一，推荐拆分）：
  - **拆分方案**：
    - `0x008 STATUS`（R/只读）：bit0=`feature_valid`、bit[31:16]=`frame_id`，其它位保留。
    - `0x00C` 附近新增 `ACK`（W/写 1 脉冲，只写）：CPU 写当前要确认的 `frame_id` 或一个 toggle 位作为 ack 脉冲，其它位忽略。
    - `frame_id` 单独只读寄存器或仍是 STATUS 的高位只读，但**不允许**把 ack 和 valid/frame_id 同址可写。
  - 或保留合并寄存器，但**写掩码语义写死**：只有 ack bit 写 1 有效，其它任何位写入硬件忽略；并在手册中明确 CPU 驱动必须 `write_u32(0x2, addr)` 而非 RMW。
- **配合 (R1)**：`feature_valid` 不靠 APB 读侧清除；像素域只有在同步后的 ack 且 `frame_id` 匹配时才清 valid。

---

## 2. 主要风险（强烈建议一并改，进手册）

### R1. CDC 升级为"稳定快照 + frame_id 匹配 ack"完整事件协议

- **问题**：当前 3.1 节"多 bit 走 VSYNC 影子寄存器、单 bit 走两级同步器"是合理起点，但对 `feature_valid`/`feature_ack` 这类跨域事件不完整。`frame_id` 跨多 bit 直接同步会撕裂。
- **改成**：在计划（或手册草案章节）中定义事件协议：
  1. FPGA 在帧结束一次性锁存 `RES_*` 整组与 `frame_id` 到 APB 读侧快照，再置 `feature_valid`。
  2. `frame_id` 天然由像素域产生、随结果快照到 APB 读侧；**不要**反过来从 APB 域影子到像素域作为主机制。跨域传多 bit 帧号用"valid 期间数据保持稳定 + ack 后再更新"，**不必用 Gray 编码**作为首选。
  3. CPU 读同一个 `frame_id` 的稳定快照后写 `ack`（带 frame_id 或 toggle）。
  4. FPGA 只在看到同步后的 ack 且 `frame_id` 匹配时清 `feature_valid`，并允许更新下一帧快照。
- **改 3.1 节标题/正文**：从"两级同步 + VSYNC 影子"升级为上述事件协议描述。

### R3. 颜色阈值寄存器预留双边界 + 亮度门限

- **问题**：当前颜色阈值每色只一个 `CFG_COLOR_xxx_TH`。单阈值对现场 AWB/曝光波动不够鲁棒。
- **改成**：Register Map 每色阈值寄存器至少预留：
  - `min/max` 或 `lower/upper` 双边界；
  - 简单亮度门限 `sum_min/sum_max`（如 `R+G+B` 上下限）；
  - mask 面积上下限仍放 CPU 判定，FPGA 只输出 area/bbox/raw stats。
  - 偏移重新排布（每色占 2~3 个 32-bit 字），并在表注里说明位段。

---

## 3. 其它建议（采纳，写入手册后续阶段，不进首版 RTL）

- **R2**：OSD 插入点必须先做 `top.v` fanout/diff（即 B2 的 fanout 表）；OSD 自身 reset 跟随像素域，不得反向影响 `pixel_data_en` 与 DSI/HDMI；HDMI 48-bit 双像素到 24-bit 单像素串化要保持时序和奇偶顺序。把这条作为"进入 RTL 前必须验证"项写入计划。
- **R4**：多物体、PLIC 中断、FPGA 尺寸分类**不进首版 RTL**，写入 decision_log 的后续阶段，不作为当前通过条件。
- **看门狗**：`CPU_HEARTBEAT` 超时按毫秒由 CPU/CLINT 标定，FPGA 可另以帧计数近似显示；首版超时动作限定为 OSD/状态告警，**不**自动触发 myCobot 急停或 freeze 视觉流水线，避免安全语义混乱。
- **OSD 寄存器命名**：为避免误用，建议把 `RES_BBOX_*`（实时）命名为 `LIVE_BBOX_*`，CPU 回写稳态命名为 `CPU_OSD_BBOX_*`，不要只靠地址区分。

---

## 4. 改完后请回贴

请 Gemini 改出 v4 并回贴全文（或改动 diff）。Claude 将按本清单逐条核对，通过后再起草《FPGA 与 CPU 视觉交互寄存器手册》并更新 decision_log（记录"方向通过 + B1/B2/B3 已落实"）。在 RTL/SoC 实现前需带修订包再过一次 Codex Gate。
