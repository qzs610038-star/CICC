# A 队员 FPGA/SoC 执行方案

> 日期：2026-07-12  
> 状态：**部分实施，继续受 Codex Gate 约束**；A2–A4 已有隔离工程/Review Packet 证据，正式协作工程仍未形成 SoC/APB/CDC/OSD 闭环
> 负责人：A（FPGA/SoC）  
> 关联上位方案：`competition_score_maximization_execution_plan_20260712.md`（v1.3-main）
> 关联分工：`three_member_execution_board_20260712_17.md`

## 1. 目标与完成定义

本方案的近期目标不是完成整套识别或机械臂闭环，而是在不破坏当前可见 HDMI 合成验证的前提下，建立可追溯的板上 CPU 最小硬件闭环：

```text
视频工程内 QCRV32 SoC
  -> 生成的 soc.h
  -> CPU UART1 输出 BUILD_ID
  -> CPU 读取 FPGA REG_MAGIC
  -> CPU 周期写 FPGA REG_HEARTBEAT
  -> FPGA LED 或 debug probe 可观察 heartbeat
```

该闭环通过后，才允许进入特征快照 CDC、CPU 结果回写和固定 `RESULT_STATUS` OSD。

本方案不把以下事项视为已完成：真实摄像头恢复、真实帧特征、四任务板级判断、机械臂动作、双摄融合或比赛最终 bitstream。

## 2. 已知基线

### 2.1 已具备

- `synthetic_2ppc_source.v` 已作为 RGB888 2ppc 合成输入接入预处理；方块为 960x1080 有效区中心的 320x320 正方形。
- 合成画面可经 HDMI 显示，方块轮换红、蓝、黄、白、黑五色。该事实只证明合成源到 HDMI 的验证路径可见。
- `vision_preprocess_channel` 已以 ch1 像素域旁路实例化，能产出 ROI、前景面积、bbox、中心点和帧快照等像素域信号。
- CPU 侧已有分类、参数表、任务匹配和机械臂协议代码的 host/mock 基础。

### 2.2 未具备，且是硬门槛

- 视频 Efinity 工程中没有 QCRV32 SoC IP、正式生成的 `soc.h`、正式 BSP/linker 或已确认的 CPU 下载路径。
- 没有可确认空闲的 APB slave 端口，没有 `REG_MAGIC` RTL、`results_cdc` RTL 或 OSD RTL。
- `cpu/app/include/bsp.h` 中的地址是占位值，禁止作为板级地址使用。
- C 盘协作树与 `D:\final_project` 手动构建树的 `top.v` 和 `mem_test.xml` 当前不同，禁止整树覆盖同步。

## 3. 不可突破的边界

- FPGA 仅负责视频前端、ROI/统计特征、OSD 像素渲染及 APB/UART/CDC 硬件通道；颜色、形状、尺寸分类和四任务关系判断留在板上 CPU。
- 不恢复纯 RTL 分类或纯 RTL myCobot 控制。
- 不执行机械臂动作，不改变机械臂接线、电平或串口控制。
- 不伪造、复制或手改 `.dbg.vdb`，不通过批量补 2,288 个 I/O 约束绕过 PNR 问题。
- 2026-07-13 正式整合构建的最新 PNR 计数为 1,776 个 IO 无 placement；2,288 仅保留为旧构建历史。两者均不得通过盲绑管脚绕过。
- 每次修改 `top.v`、`mem_test.xml`、`.peri.xml`、`constrain.sdc`、时钟、复位、CDC 或 SoC/IP 设置前，先建立该次改动的 Review Packet 并经 Codex Gate 复核。
- 真实摄像头/比赛构建前必须使：

```verilog
PREPROCESS_CH1_USE_SYNTHETIC_SOURCE = 1'b0;
HDMI_USE_SYNTHETIC_VERIFY = 1'b0;
```

## 4. 执行阶段

### A0：冻结可见合成源基线

**目的**：让之后的 SoC 集成出现问题时有可恢复的 HDMI 对照版本。

**允许动作**

- 只读比对 C 盘与 D 盘的 `top.v`、`mem_test.xml` 和合成源文件。
- 记录当前 D 盘 bitstream SHA-256、Efinity 版本、构建时间、板卡截图、五色显示顺序和已见现象。
- 建立合成源收口 Review Packet；注明该版本仅用于开发期验证。

**禁止动作**

- 不覆盖 D 盘文件，不修改时钟、CDC、约束或 IP。
- 不把板上画面称为真实摄像头或预处理特征验证。

**验收证据**

```text
1. C/D 差异清单（逐文件决定保留、合并或暂不合并）
2. D 盘 bitstream SHA-256 与时间戳
3. HDMI 截图/录像：灰底、居中正方形、五色轮换
4. Review Packet：开关、输入格式、回退方式、NOT VERIFIED 项
```

**停止条件**：无法证明当前烧录产物对应哪份源码时，停止后续 D 盘操作，仅保留证据并重新手动构建。

### A1：SoC 迁移可行性勘查与隔离副本准备

**目的**：确认能从官方 RISC-V 例程提取“生成方式”，而不是复制其地址或直接移植其顶层。

**输入**

- 官方 RISC-V 例程中的 SoC/IP 配置、生成脚本、`top_soc.v`、已生成 `soc.h`、linker、OpenOCD/下载配置。
- 当前视频工程 `mem_test.xml`、`top.v`、现有时钟/复位/HDMI 路径。

**允许动作**

- 只读列出例程与视频工程的 SoC 迁移清单。
- 在无中文、无空格的隔离目录创建视频工程副本；原 C/D 工程均不用于首次 SoC 试验。
- 输出端口/时钟/复位/APB/UART/soc.h 路径对照表。

**必须回答的事实问题**

```text
1. QCRV32 SoC IP 的确切配置文件和生成命令是什么？
2. 生成物中的 soc.h、linker、OpenOCD 配置分别在哪里？
3. 哪个 io_apbSlave_x 确实空闲？其 PADDR 宽度和 APB 信号语义是什么？
4. APB 时钟、复位名称和频率是什么？
5. UART1 对应哪组 SoC 端口，如何映射到已知板级调试链路？
6. 生成 SoC 后，视频顶层需要新增哪些端口和时钟域连接？
```

**验收证据**

- `generated_soc_summary_YYYYMMDD.md`，其中每项均引用实际生成物或真实例程路径。
- 迁移清单：新增/修改/严禁复制文件，且不包含 APB 地址硬编码。

**停止条件**：未能获得当前配置对应的 `soc.h` 或无法确认空闲 APB slave 时，不进入 A2；此时可继续相机单变量排查，但不得写 APB RTL。

### A2：隔离副本生成最小 QCRV32 SoC

**目的**：让视频工程副本含一个可构建的 SoC，但不接入视觉寄存器、OSD 或机械臂。

**最小范围**

- 只引入 SoC IP、其必要的生成文件、CPU ROM/RAM 或官方要求的下载链路。
- 只接通 SoC 时钟、复位和 PC 调试 UART1。
- 首版 CPU 程序只输出 `BUILD_ID` 与固定 heartbeat 文本。

**明确不做**

- 不新增 APB 自定义从机 RTL。
- 不改视频输入、Framebuffer、DDR、Debayer、HDMI 数据选择或合成源开关。
- 不接 UART2/myCobot。

**验收**

```text
CPU 交叉构建 PASS
生成 soc.h 存在且被 CPU 工程 include
CPU UART1 可见 BUILD_ID 和连续 heartbeat
视频 HDMI 合成画面仍正常
Efinity map PASS；记录资源和全部新增 warning
```

**失败处理**

- CPU 不启动：保留生成日志、UART 原始输出和最终 `soc.h`，回退隔离副本，不污染 D 盘验证树。
- 视频回归：先恢复 A0 基线，仅对 SoC 顶层连线做最小差分定位。

### A3：APB 最小闭环（REG_MAGIC + REG_HEARTBEAT）

**进入条件**：A2 生成的 `soc.h`、APB 端口、APB 时钟/复位与 CPU UART 已全部实证。

**RTL 最小接口**

```text
REG_MAGIC       0x000：只读固定值，例如 0x5649534E ("VISN")
REG_VERSION     0x004：只读 RTL 接口版本
REG_HEARTBEAT   0x008：CPU 可写，FPGA 可读/可观测
```

偏移仅是 APB 窗口内的候选布局；最终窗口基地址只来自 A2 生成的 `soc.h`。

**实现要求**

- 使用 A2 已确认的 `PSEL/PENABLE/PWRITE/PADDR/PWDATA/PSTRB/PREADY/PSLVERROR` 语义。
- 读 `REG_MAGIC` 的 CPU 端必须使用生成 `soc.h` 的宏，且编译期缺少宏即报错。
- heartbeat 先映射到 debug probe；若 LED 复用安全且可确认，再映射到 LED。不可改变已有诊断 LED 语义。
- APB 域与像素域尚不交换多位数据，本阶段不实现 `results_cdc`。

**验收证据**

```text
1. CPU UART：打印 soc.h 版本/基址、REG_MAGIC 实测读值、heartbeat 递增值
2. FPGA：heartbeat probe 或 LED 有连续变化
3. APB transaction 波形或 Debugger capture（若工具链可用）
4. Efinity map/PNR/时序报告：Setup、Hold、CDC 和关键 warning
5. HDMI 合成图仍可见
```

**停止条件**：`REG_MAGIC` 值不稳定、地址与 `soc.h` 不一致、APB 时钟/复位不明确或 HDMI 回归时，停止在 A3，不扩展寄存器表。

### A4：特征快照 CDC 与 CPU 固定结果回写

**进入条件**：A3 通过，且 B 已冻结 `round_output` / reason code 对外语义。

**实现范围**

- 新建 `results_cdc`：像素域发布稳定快照，APB 域读取，CPU 回写匹配 `frame_id` 的 ACK。
- 新建 APB staging/commit：ROI、阈值和 CPU OSD 字段只写 staging，在 VSYNC 边界整组切到 active。
- 首次 CPU 侧仅读取固定合成快照，并回写固定 `RESULT_STATUS`；不将特征用于机械臂决策。

**不可变 CDC 规则**

```text
snapshot_valid=1 期间，像素域完整快照保持不变。
CPU 读 status/frame_id -> 读字段 -> 复读 status/frame_id。
仅 ACK(frame_id) 匹配时，像素域才清除 valid。
APB 多位配置不直连像素域；只能 staging -> commit -> VSYNC active。
```

**验收**

- CPU 读取的 `frame_id` 和合成画面轮换时序一致。
- 错帧/陈旧 ACK 不清除快照。
- 逐字段写配置不会造成一帧内的半新半旧配置。
- A3 的 UART、APB、HDMI 回归均通过。

### A5：固定语义 OSD 与真实摄像头恢复

**进入条件**：A4 通过；OSD 插入点、扇出、像素格式和 B 的结果语义均有独立 Review Packet。

**顺序**

1. 固定文本/状态 OSD：显示识别结果、目标/非目标、执行/跳过及原因码的 CPU 语义，不显示无解释的裸寄存器值。
2. 仅在 OSD 稳定后，将 `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE` 设为 `0` 做单摄像头恢复。
3. 同一 bitstream、同一线缆方向，只替换摄像头，记录 I2C、LED、画面和 MCLK/复位证据。
4. 首先完成单摄；双摄仅作为增强分支。

**停止/降级**

- 摄像头仍无 CSI DE：保留合成源下的 CPU/APB/OSD 开发，不把故障归因给合成源。
- A4 未通过：不插入 OSD。
- UART 电平、接线和安全检查未通过：不进入板到臂控制，保持 `ARM_DISABLED`。

## 5. 阶段门与回退矩阵

| 阶段 | 可交付物 | 下一阶段的唯一许可条件 | 失败后的回退 |
|---|---|---|---|
| A0 | 合成源基线包 | 源码、bitstream、截图可追溯 | 重新手动构建并记录 |
| A1 | SoC 生成摘要、迁移清单 | 有真实 `soc.h` 和空闲 APB 事实 | 停止 APB RTL，继续只读勘查 |
| A2 | CPU Hello/UART | CPU 在视频工程副本内运行且 HDMI 回归通过 | 回退隔离副本，不影响 D 盘 |
| A3 | APB MAGIC/heartbeat | 基址来自 `soc.h`，读写和可观察性通过 | 保持 CPU Hello，不扩寄存器 |
| A4 | 快照 CDC/固定结果回写 | frame_id ACK 和 commit 语义通过 | 回退到 A3 固定寄存器 |
| A5 | OSD + 单摄恢复 | 语义、扇出和真摄证据完整 | 回到合成源 + A4 |

## 6. 交付记录模板

每个阶段结束都新增或更新一个记录，格式固定为：

```text
目标：
修改文件：
生成物/工具版本：
运行命令：
PASS/FAIL：
证据路径：
资源、Setup/Hold、CDC 与关键 warning：
仍未验证：
回退点：
下一步最小动作：
```

记录写入 `final_project/docs/debug_sessions/` 或对应 Review Packet；影响下一次手动构建/烧录的说明同步到 `D:\final_project\docs\`，但不覆盖 D 盘 RTL。

## 7. 审核请求

请审核以下边界后再授权执行：

1. 是否批准按 A0 -> A1 -> A2 的顺序执行，且 A2 仅在隔离副本进行？
2. 是否确认 A3 的最小寄存器仅为 `REG_MAGIC`、`REG_VERSION`、`REG_HEARTBEAT`？
3. 是否确认 A4 前不接 OSD、真实摄像头分类、UART2 或机械臂？
4. 是否确认 C/D 不做整树同步，后续每次 D 盘合并均先提交差异清单？
