# 视觉模块架构与寄存器配置实现计划

**状态: 架构方向 Gate 已解除；RTL/SoC 执行前 Gate 保留，待 B1 `generated_soc_summary` 闭合**
*(注：本方案已完成 Codex 三次复核并更新至 decision_log；在补齐 SoC 地址/配置摘要前，只允许作为寄存器手册草案和实现准备依据，不进入 RTL/SoC 配置修改。)*

## 修订记录
*   **2026-07-03 (v4.3, Claude 直改)**: 根据 Codex 三次复核，补齐 §3.5 四个手册边界——`config_seq` 不可连续两次相同（否则视为无效 commit）、pending 覆盖规则"最后一次 commit 胜出"、复位默认值（active_seq/pending_seq=0，安全默认 ROI/阈值）、staging 读回语义（读 `CFG_*` 得 staging，读 `CFG_STATUS.active_seq` 判断是否生效）。架构方向 Gate 已解除；RTL/SoC 执行前 Gate 保留，待 B1 `generated_soc_summary` 闭合。B4 原子提交协议经 Codex 三次复核判定可作为寄存器手册契约基线。
*   **2026-07-03 (v4.2, Claude 直改)**: 根据 Codex 二次复核新增两项执行前阻塞项：**B4** 配置原子提交协议——新增 §3.5"staging→commit→active shadow"三段机制与 Register Map `0x04C CFG_COMMIT`(W)/`0x050 CFG_STATUS`(R)，解决多寄存器分次写被同一 VSYNC 锁存"半新半旧"的风险；**R1 补充**——写死 valid 未被匹配 ack 前不覆盖快照、CPU 只 ack 当前帧号、掉帧时 FPGA 采用"保持最新快照等 ack"策略。状态保持"架构方向通过，RTL/SoC 执行前 Gate 未解除（待 B1 soc.h 摘要生成）"。
*   **2026-07-03 (v4.1, Claude 直改)**: 在 v4 基础上修复 5 项残留：**P4** `SYS_ACK` 改为承载 [15:0] 要确认的 `frame_id`，FPGA 仅在 `ack.frame_id == 当前快照 frame_id` 匹配时清 valid（R1 事件协议落地）；**P5** 3.3 节双像素流 `wb0_data_out` → `wb1_data_out`（与 B2 主通道一致）；**P6** 第 0 节补"待导出字段清单"（UART/CLINT/PLIC/APB/AXI/DDR/时钟/Slave 空闲性/PADDR/PSLVERROR）；**P7** 3.2 节把 fanout 事实写成表格（wb0 旁路未接显示、wb1→DSI[HDMI]1598/1468-1474）；**P8** Register Map 新增 `0x03C CFG_LUMA_TH` 全局亮度门限寄存器，使 3.4 节 `TH_LUMA` 有寄存器落点。
*   **2026-07-03 (v4)**: 根据 Claude 第三轮审核 (B1-B3, R1-R4) 修正：新增 SoC 执行前置要求；主相机改绑 wb1_ 并增加 Fanout 验证；拆分 STATUS/ACK 避免 RMW 风险；升级 CDC 为稳定快照协议；完善双边界颜色阈值及寄存器规范命名 (LIVE_* / CPU_OSD_*)。
*   **2026-07-03 (v3)**: 根据 Claude 第二轮审核 (N1-N4) 修正：澄清 OSD 绘制数据源（区分 FPGA 实时与 CPU 回写），扩大 CDC 跨时钟域处理范围至所有握手信号与阈值，扩展多颜色阈值/面积寄存器，独立出 32-bit 递增心跳寄存器。
*   **2026-07-03 (v2)**: 根据 Claude 第一轮审核 (G1-G10) 修正：消除硬编码基地址，采用 `valid/ack` 帧号握手，增加双像素对齐与 OSD 跨时钟域逻辑，明确 UART 隔离。
*   **2026-07-03 (v1)**: 初稿，确定 APB3 动态参数配置与轮询机制基本路线。

---

根据此前的讨论与架构审核反馈，我们已经就视觉模块（FPGA 与 CPU 侧）的交互架构与分工作出了关键决策。本计划将这些决策落实为具体的实施路径，特别是针对基于 APB3 的动态寄存器配置方案和 OSD 调试方案。

## 0. 执行前置：SoC 地址与配置摘要 (B1)
由于 SoC/APB3 实例、基地址与时钟可能在正式工程中发生变动，本方案执行时必须遵守以下前置双重检查：
1. **编译期校验**：在 C 代码中通过 `#ifndef IO_APB_SLAVE_x_BASE \n #error "APB3 base address not defined in soc.h"` 强制报错，防止硬编码导致系统跑飞。
2. **运行期探测**：CPU 启动后第一步必须读取 `REG_MAGIC`，如果不匹配 `0x375A`，则立即挂起并打印错误，禁止写入任何寄存器。
3. **待导出字段清单（进入 RTL 前必须从当前 Efinity 生成物导出并记录）**：
   - `soc.h`：UART0/UART1 基址、CLINT、PLIC、**APB user window (`IO_APB_SLAVE_x_*`)**、AXI user window、DDR base/size。
   - 时钟：`SYSTEM_CLINT_HZ` / UART/Timer 实际输入时钟。
   - 选用 `io_apbSlave_x_*` 的**空闲性** + `PADDR` 宽度 + `PSEL/PREADY/PSLVERROR` 语义。
   - 将以上地址摘要写入《寄存器手册》附录，CPU 侧统一 `#include "soc.h"`，不散落占位地址。

## 1. 架构决策总结

*   **相机策略**: 第一版采用单路闭环（主相机俯视），优先跑通完整识别与抓取链路。**注意：为实现零改顶层 (B2-a)，第一版主相机应直接改绑至 `wb1_` 接口（实际消费给 DSI/HDMI 显示的链路），无需修改顶层逻辑。首版不涉及多物体、PLIC中断及 FPGA 内部尺寸分类。**
*   **同步机制**: CPU 采用基于 `valid` / `ack` 握手与 `frame_id` 序列号的轮询机制，取代存在跨时钟域风险的读清零机制。
*   **颜色识别**: FPGA 负责阈值匹配（通道差值/比例等轻量特征）与像素级统计，输出各颜色的面积（Area）；由 CPU 负责多帧滤波与最终色彩决策。
*   **形状识别**: FPGA 负责输出连通域的原始几何特征（Bounding Box 边界、面积）；由 CPU 负责计算长宽比与占空比，进行形状最终分类（正方体、圆柱、圆锥）。
*   **调试功能**: 必须引入硬件 OSD（On-Screen Display），在物理显示器输出上叠加 BBox 与中心点，用于实地校准。

## 2. 动态参数配置方案 (基于 APB3 总线)

该模块是软硬件握手的核心，用于下发阈值并回读识别结果。

### 2.1 总线选择
基于 QCRV32（基于 Efinix Sapphire SoC）提供的 **APB3 Slave 接口**。
*   **优势**: APB3 专为慢速控制信号设计，完全契合配置/状态寄存器的需求，免去了 AXI4 复杂的突发传输和握手逻辑，节省 FPGA 逻辑资源，时序收敛友好。

### 2.2 硬件实现路径 (FPGA 侧)
1.  **IP Manager 配置**: 在 SoC IP 中使能一个 APB3 Slave 实例。具体选用的 Slave 实例号待 SoC 配置锁定后确定（注意：示例工程中 Slave 0 可能已被 gDMA 控制占用，需选其它空闲端口或重新规划）。**寄存器基地址由 Efinity 最终生成的 `soc.h` 决定（如 `IO_APB_SLAVE_x_BASE`），绝对不在文档或 RTL 中硬编码，避免与系统 `0xF8100000u` 等地址段冲突。**
2.  **RTL 编写**: 参考 Efinity 生成的 `design_modules.v` 中的 `apb3_slave` 模块作为模板。
3.  **读写逻辑 (Register File)**:
    *   **写逻辑**: 在 APB 写周期（`PSEL` && `PWRITE` && `PENABLE`），解析 `PADDR` (地址偏移) 并将 `PWDATA` 赋值给对应参数寄存器。
    *   **读逻辑 (关键)**: 实现多路选择器 (MUX) 逻辑，将识别结果或配置参数返回给 `PRDATA`，确保读写对齐。

### 2.3 软件实现路径 (CPU 侧)
1.  **API 调用**: 使用包含在 `soc.h` 和 BSP 中的基地址宏，配合强转指针直接进行内存映射读写，即插即用。
2.  **动态下发与外设隔离**:
    *   **隔离原则**: 参数下发命令和调试日志均走 **UART0 (115200)**；而 myCobot 机械臂控制协议走独立的 **UART1 (1000000 bps)**。两者必须严格隔离，禁止混用。
    *   **代码影响**: 在 CPU 主循环的 UART0 接收回调中加入简易 ASCII 字符串解析，完成动态参数下推。

## 3. 硬件实现的关键注意事项 (CDC 与双像素)

### 3.1 跨时钟域边界 (CDC) 处理升级
APB3 接口在 APB 时钟域运行，而视觉处理流水线和 OSD 均在像素时钟域（Pixel Clock）运行。必须采用“稳定快照 + frame_id 匹配 ack”的完整事件协议防止亚稳态和撕裂：
*   **多 Bit 同步 (稳定快照协议)**：所有 CPU 下发的多 Bit 寄存器（包括 `frame_id`、`SYS_CTRL`、阈值、ROI 以及 CPU 回写的 OSD 坐标）**必须在 VSYNC 空白期整组锁存到像素时钟域的影子寄存器**。流水线只读取这些影子寄存器，确保帧内配置绝对一致。首选快照锁定，不走 Gray 编码。
*   **单 Bit 握手信号同步**：`feature_valid` (FPGA 到 CPU) 和 `feature_ack` (CPU 到 FPGA) 等单 Bit 控制信号，经两级触发器同步后，配合 `frame_id` 确认状态机流转。
*   **valid 与快照规则（寄存器手册必须写死）**：
  - `frame_id` 为 16-bit 单调递增，回绕可接受。
  - FPGA `feature_valid` 未被匹配 ack 前，**不覆盖当前快照**（保持待 ack）。
  - CPU 只 ack 当前 `SYS_STATUS.frame_id`，不得 ack 过期帧号。
  - CPU 掉帧（valid 仍置位、新帧已备好）时，FPGA 首版采用"**保持最新快照等待 ack**"策略：valid 维持、快照读侧保持上一帧数据，直到 CPU ack 同帧号后才推进。该二选一在手册中明确，避免实现者猜测。

### 3.2 OSD 画框的数据源歧义澄清与 Fanout 验证

**已核实 top.v fanout 事实**（`final_project/fpga/rtl/top/top.v`，与赛方原样一致）：

| 信号 | 产生源 | 实际消费方 | 说明 |
|---|---|---|---|
| `wb0_vs_out/hs_out/de_out/data_out[47:0]` | `u0_white_balance`（[top.v:1291-1310](../../fpga/rtl/top/top.v#L1291-L1310)） | **未接进 DSI/HDMI 显示链路** | 现状旁路，首版不进识别/显示 |
| `wb1_vs_out/hs_out/de_out/data_out[47:0]` | `u1_white_balance` | DSI: [top.v:1398-1401](../../fpga/rtl/top/top.v#L1398-L1401)；HDMI 串化: [top.v:1468-1474](../../fpga/rtl/top/top.v#L1468-L1474) | **主显示链路**，第一版主相机/OSD/feature_extract 均取此路 |

OSD 模块插入点（`wb1_` 之后、DSI/HDMI 之前）需事先进行 Fanout 验证，确保加入 OSD 逻辑后不会破坏原本 DSI/HDMI 的时序和扇出负载。OSD 模块将同时绘制两类时钟域处理方式截然不同的 BBox 框：
1.  **FPGA 实时特征框 (绿色)**: 数据来源于本帧实时计算出的 `LIVE_BBOX_MIN/MAX`，数据本身就在像素时钟域，因此**画这类框不需要 CDC 影子寄存器**。
2.  **CPU 回写的稳态框 (红色)**: 数据来源于 CPU 经过多帧滤波、分类判断后决定采用的稳定坐标。这些坐标由 CPU 写入新增的 `CPU_OSD_BBOX_*` 等回写寄存器。因为是从 APB 时钟域写入，**此类框的坐标必须通过 VSYNC 影子寄存器同步**后才能被 OSD 模块安全读取并绘制。

### 3.3 双像素流 (Even/Odd) 对齐处理
*   **风险**: 主显示链路白平衡输出 `wb1_data_out[47:0]` 采用双像素 RGB888 格式输出（每个时钟周期输出 Even 和 Odd 两个相邻像素）。直接与单像素坐标 `X_curr` 比较将导致边框出现半像素错位。
*   **方案**: OSD 模块需按双像素步进，X 坐标比较必须拆分为两路（分别对比 `X_curr` 和 `X_curr+1` 与 BBox 边界）。覆盖在 Even 还是 Odd 像素上应严格按奇偶性判断，实现精准覆盖。

### 3.4 颜色阈值判定逻辑
*   **原则**: 彻底放弃在 FPGA 像素流上做 RGB 到 HSV 转换的昂贵方案。
*   **方案**: 预留双边界（上限/下限）以及亮度门限。直接使用通道差值（如 `TH_RG_MIN < R - G < TH_RG_MAX`）、归一化比值及整体亮度（如 `R+G+B > TH_LUMA`）等轻量特征，通过加减法和移位实现高速颜色分割 Mask 提取。

### 3.5 配置原子提交协议 (B4)

**风险**：CPU 在一帧内分次写 `CFG_ROI_TL`/`CFG_ROI_BR` 或多色阈值时，VSYNC 边界可能正好落在两次写之间，像素域影子寄存器会锁存到"半新半旧"配置，导致 ROI/阈值在一帧内不自洽。

**三段 staging → commit → active shadow 机制**：

1. **APB 域维护两套配置**：
   - **staging**：CPU 写所有 `CFG_*` 参数的实际落点。
   - **active**：FPGA 统计/OSD 当前正在使用的一组完整配置。
2. **`CFG_COMMIT`（CPU 写）**：CPU 写完整一组参数后，写 `CFG_COMMIT[15:0] = config_seq`（单调递增的配置序号）表示"这组写完了"。
3. **VSYNC 边界切换**：像素域仅在 VSYNC 空白期看到 `staging.config_seq` 发生变化后，把整组 staging 配置原子地拷到 active shadow；中途不部分更新。
4. **`CFG_STATUS`（CPU 读）**：FPGA 回报 `active_config_seq[15:0]`（已生效序号）与 `pending_config_seq[15:0]`（已 commit 但尚未到 VSYNC 生效），CPU 可据此判断配置是否已生效。

**提交粒度**：首版采用**单组统一提交**——ROI、颜色阈值、亮度门限、`CPU_OSD_BBOX_*` 共用同一个 `config_seq`，写一个 `CFG_COMMIT` 即整组切换。若后续要分组提交，再在 `CFG_COMMIT` 定义位域（如 [ROI]/[THRESH]/[OSD]）分别触发，首版不实现分组。

**四个边界（寄存器手册必须写死）**：

1. **`config_seq` 复用/回绕**：CPU 每次 commit 必须使用与上一次不同的序号；16-bit 回绕允许，**不得连续两次使用相同值**，否则 FPGA 视为无效 commit（不更新 pending）。
2. **pending 覆盖规则**：若前一个 pending 尚未到 VSYNC 生效时 CPU 再次 commit，首版定义为"**最后一次 commit 胜出**"——pending staging 被最新一整组 staging 覆盖，`pending_seq` 更新为最新值。
3. **复位默认值**：复位后 `active_seq=0`、`pending_seq=0`；active shadow 使用安全默认（OSD 关闭、ROI=全画面、各色阈值取保守值），具体默认值在寄存器手册逐寄存器列出。
4. **staging 读回语义**：CPU 读 R/W 类 `CFG_*` 寄存器，**读到的是 staging 区当前值**（即 CPU 最近写入，未必已生效）；CPU 读 `CFG_STATUS.active_seq` 判断像素域是否已采用该组配置（`active_seq == 本次 commit_seq` 即已生效）。

**验收标准**：CPU 写多个 `CFG_*` 寄存器期间，未写 `CFG_COMMIT` 前继续使用上一组完整配置；写 `CFG_COMMIT` 后最多在下一帧 VSYNC 生效，且不会半帧/半组生效；连续两次相同 `config_seq` 的 commit 被忽略；pending 期间再次 commit 以最后一次为准。

## 4. Register Map 草案 (偏移量定义)

*(注：仅展示偏移量 Offset，基地址提取自 `soc.h` 的 `IO_APB_SLAVE_x_BASE`)*

| 偏移 (Offset) | 读写 | 寄存器名称 | 描述 |
| :--- | :--- | :--- | :--- |
| `0x000` | R | `REG_MAGIC` | 包含固定的 Magic Number (如 `0x375A` 和 Version)，用于 CPU 上电地址探嗅校验 |
| `0x004` | R/W | `SYS_CTRL` | [0] 视觉流水线使能; [1] OSD 使能 |
| `0x008` | R | `SYS_STATUS` | 握手状态(只读): [0] `feature_valid` (FPGA置位); [31:16] `frame_id` 帧号 |
| `0x00C` | W | `SYS_ACK` | 握手应答(只写): CPU 写入值 [15:0]= 要确认的 `frame_id`；FPGA 只在同步后 ack 且 `ack.frame_id == 当前快照 frame_id` 匹配时才清 `feature_valid`，避免任意脉冲误清导致的撕裂/RMW 风险 |
| `0x010` | R/W | `CPU_HEARTBEAT` | 32-bit 递增计数。CPU写递增，看门狗超时仅重置状态机，**不动机械臂** |
| `0x014` | R/W | `CFG_ROI_TL` | 高16位: ROI Y起点 (MIN_Y), 低16位: ROI X起点 (MIN_X) |
| `0x018` | R/W | `CFG_ROI_BR` | 高16位: ROI Y终点 (MAX_Y), 低16位: ROI X终点 (MAX_X) |
| `0x020` | R/W | `CFG_RED_TH_0` | 红色的判决阈值0 (双边界下限 / 亮度门限) |
| `0x024` | R/W | `CFG_RED_TH_1` | 红色的判决阈值1 (双边界上限) |
| `0x028` | R/W | `CFG_BLUE_TH_0` | 蓝色的判决阈值0 (下限) |
| `0x02C` | R/W | `CFG_BLUE_TH_1` | 蓝色的判决阈值1 (上限) |
| `0x030` | R/W | `CFG_YEL_TH_0` | 黄色的判决阈值0 (下限) |
| `0x034` | R/W | `CFG_YEL_TH_1` | 黄色的判决阈值1 (上限) |
| `0x03C` | R/W | `CFG_LUMA_TH` | 全局亮度门限: [31:16] `TH_LUMA_MAX`(R+G+B 上限), [15:0] `TH_LUMA_MIN`(下限)，用于抑制高光/黑噪点；各色 mask 判定统一引用 |
| `0x040` | R/W | `CPU_OSD_CTRL` | CPU 稳态回写控制字 (用于OSD显示最终判决颜色/形状类别) |
| `0x044` | R/W | `CPU_OSD_BBOX_MIN` | CPU 回写的稳态目标外接矩形最小坐标 (用于OSD绘制稳态框) |
| `0x048` | R/W | `CPU_OSD_BBOX_MAX` | CPU 回写的稳态目标外接矩形最大坐标 (用于OSD绘制稳态框) |
| `0x04C` | W | `CFG_COMMIT` | 配置原子提交(只写): CPU 写 [15:0]=`config_seq`(单调递增)，触发整组 staging->active 在下个 VSYNC 切换；未写 commit 前沿用上一组完整配置 |
| `0x050` | R | `CFG_STATUS` | 配置生效状态(只读): [31:16] `active_seq`(已生效), [15:0] `pending_seq`(已commit待VSYNC)；CPU 据此判配置是否生效 |
| `0x080` | R | `LIVE_RED_AREA` | 当前帧红色 Mask 总匹配像素数 |
| `0x084` | R | `LIVE_BLUE_AREA` | 当前帧蓝色 Mask 总匹配像素数 |
| `0x088` | R | `LIVE_YEL_AREA` | 当前帧黄色 Mask 总匹配像素数 (WHITE/BLACK保留 `0x08C/0x090`) |
| `0x0A0` | R | `LIVE_BBOX_MIN` | FPGA实时统计的最大物体外接矩形最小坐标: [31:16] Y_MIN, [15:0] X_MIN |
| `0x0A4` | R | `LIVE_BBOX_MAX` | FPGA实时统计的最大物体外接矩形最大坐标: [31:16] Y_MAX, [15:0] X_MAX |
| `0x0A8` | R | `LIVE_CENTER` | FPGA实时统计的目标中心坐标: [31:16] Y_CEN, [15:0] X_CEN |
