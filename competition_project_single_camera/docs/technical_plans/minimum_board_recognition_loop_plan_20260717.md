# 单摄板上图像识别最小闭环紧急计划

> **HISTORICAL / SUPERSEDED（2026-07-18）：** 本文记录旧 R0/UART0 路线，保留用于追溯，不再作为执行入口。当前 I0 已固定为 SoC UART1 → 板载 Type-C UART1（RX=`GPIOR_96/B12`、TX=`GPIOR_100/D12`）；执行以 [`I0_UART1_INTERFACE_FREEZE.md`](../../integration/I0_UART1_INTERFACE_FREEZE.md)、根目录 `CURRENT_STATE.md` 和团队最终日分工为准。不得执行本文的 UART0 步骤或继承旧制品 PASS。

> 日期：2026-07-17
> 实时仓库基线：`main@9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`
> 状态：`PLAN ONLY / IMPLEMENTATION NOT STARTED`
> 目标：用最少变量先打通“真实摄像头 -> FPGA 同帧统计 -> APB0 -> 板上 QCRV32 CPU 分类 -> UART0 输出”。

## 1. 结论先行

当前不需要重写识别算法，也不需要先接 OSD、拨码、四任务状态机或机械臂。最短路径是：

1. 用当前 `main` 匹配的 G1 bitstream/Hello ELF 重验 `USER2 + UART0 + APB MAGIC` 基础 Gate。
2. 审查并拆出本地备份分支中已存在的 feature mailbox/APB/CDC 候选实现，只将识别链所需最小差分落到新分支。
3. 复用已有 `single_camera_classifier` 和 `single_camera_feature_adapter`，使用精简片上 RAM 固件，不先链接完整 G2 runtime。
4. 先实现 UART0 可见的五色/粗形状识别；尺寸仍明确输出 `SIZE=NA`。

这个结果是“板上图像识别闭环”，但还不是比赛 F1 完整保底闭环。官方要求每轮输出识别、目标判断、执行/不执行及理由，见比赛细则第二章第 3 节第 3、5 款；本计划只先关闭其中的“真实识别”硬阻塞。

## 2. 当前进度分类

### 2.1 已完成且可直接复用

| 模块 | 当前事实 | 证据/边界 |
|---|---|---|
| 单摄视频工程 | Hard SoC、APB0、UART0、USER2、片上 16 KiB RAM 和 ch0 视频真源已入库 | `CURRENT_STATE.md` 第 16-23 行；G1 离线 Map/PNR/STA/CDC/bitstream PASS，不等于当前板级 PASS |
| RGB 特征 tap | `feature_stats_tap.v` 已列入 `mem_test.xml`，已旁路接到 ch0 Debayer 后 `rgb0_data_rgb` | 不回压、不驱动 HDMI；当前 `i_capture_enable=0` |
| FPGA 基础特征 | 已实现 ROI、R/B/Y 面积、前景面积、ROI 像素数、亮度和 bbox 累加 | RTL 已进入 G1 离线构建；发布/ACK 路径尚未启用 |
| CPU 特征适配 | `single_camera_feature_adapter` 已实现 flags 校验和 fail-closed | Host 验证，非板级 |
| CPU 分类器 | `single_camera_classifier` 已输出五色与三形状语义 | Host 验证；黑/白与形状阈值未经真实板级标定 |
| CPU 事务框架 | G2 runtime/fake transport 已通过 C Host `182/182`、Python `3/3` | 适用于后续证据链；完整 runtime 不是首个 16 KiB 固件的必需项 |
| 最小启动固件 | UART0 Hello 已冷构建，入口/LOAD 固定于 `0xF9000000` | 当前 `CURRENT_STATE.md` 仍要求按匹配 hash 重验板级 Gate |

### 2.2 有候选实现，但不能宣称完成

本地备份分支 `codex/backup-before-dev-libaoxun688-20260717-154836@6390ace` 保存了一套未合入 `main` 的候选：

- `feature_snapshot_apb`：snapshot mailbox、ACK、配置 shadow/commit 和双向 CDC。
- `feature_uart_onchip`：读 APB 特征、调用 CPU 分类器、UART0 输出。
- 候选 feature ELF 静态审计仅使用 `3744 B / 16 KiB`。
- 候选地址修正版 bitstream SHA-256 为 `828937321C581E41FBD63DB10D7C274FA063A76DB7C3417C8CA52B5222022987`，本机仍保留产物。
- 历史记录表明 FTDI `/2`、USER2 tunnel IR 9、COM10 和 APB word-offset 是有价值的排错线索，但不能替代当前批次证据。

该备份分支的 `top.v` 还混有 RAW gain 和 LED 探针实验，禁止整个 cherry-pick。必须以当前 `main` 为基线，只提取 feature mailbox、tap 原子性修复、顶层必需连线和精简固件。

### 2.3 尚未完成的真实阻塞

| 优先级 | 缺口 | 当前现象 |
|---|---|---|
| P0 | 当前批次 `USER2 + UART0 + APB MAGIC` 板级身份 | `CURRENT_STATE.md` 仍为 `NOT VERIFIED` |
| P0 | feature snapshot 发布/ACK | tap 采集使能硬绑 `0`，输出全为 `_unused` |
| P0 | APB 特征寄存器和 CPU 写通道 | 当前只有 `MAGIC@+0x000`，SoC `PWDATA` 未连接 |
| P0 | 受审 snapshot CDC | 像素域与 `axi0_ACLK` 尚未形成当前 `main` 的冻结 payload 握手 |
| P0 | 板上 MMIO/识别固件 | `single_camera_mmio_transport` 仍 fail-closed；`main` 不含 feature UART 固件源码 |
| P0 | 真实数据标定 | ROI、背景、前景门限、颜色门限、填充率门限仍为启发式默认值 |
| P1 | 尺寸识别 | CPU 明确固定 `size_cm_x10=0`，任务三/四不可用 |
| P1 | OSD、目标输入、逐轮状态机 | 未进入本次最小闭环 |
| P2 | UART2/J52 与 myCobot | 继续禁止，与本次无关 |

## 3. 第一闭环的准确定义

```text
J48/ch0 真实摄像头
  -> 现有 MIPI/DDR/Debayer
  -> feature_stats_tap（固定单 ROI，同帧统计）
  -> 单槽冻结 snapshot + APB0
  -> QCRV32 片上 RAM 固件
  -> single_camera_feature_adapter
  -> single_camera_classifier
  -> UART0 115200: frame/raw feature/COLOR/SHAPE/SIZE=NA
  -> 匹配 frame_id ACK
```

本闭环不依赖 PC 分类；PC 只监听 UART 并保存日志。FPGA 只输出基础统计，分类仍由板上 CPU 完成，符合 `AGENTS.md` 架构硬边界。

### 首版验收输出

```text
BOOT FEATURE_RECOGNITION_V1
APB_MAGIC=375A0001 CFG=1
F=123 FLAGS=47 R=... B=... Y=... FG=... ROI=... L=... BOX=...x...
RESULT COLOR=RED SHAPE=CUBE SIZE=NA STABLE=1
ACK=123
```

禁止用“只能读到寄存器数字”作为识别通过。必须看到 CPU 输出可解释的 `COLOR/SHAPE`，且移走物体后原始特征与结果能可重复变化。

## 4. 实施顺序与时间盒

### Gate R0：重验基础 CPU/APB（15-30 分钟）

1. 核对当前 G1 bitstream/Hello ELF hash，不混用历史批次。
2. 只用 JTAG/SRAM 易失性配置和 `USER2`，不用 Flash/USER1。
3. 按已验证启动顺序执行：`NDMRESET -> 4 hart halt -> 重载 ELF -> PC=0xF9000000 -> continue`。
4. 验收 UART0 Hello 和 `0xE8100000 -> 0x375A0001`。

停止条件：任一项失败则不改 feature RTL，先修复基础 Gate。历史证据只用于快速重现 FTDI `/2`、USER2 IR 9 和 COM10，不直接继承 PASS。

### Gate R1：候选批次方向性复测（30-45 分钟）

在 R0 PASS 且用户批准板级操作后，可以用本机保留的候选地址修正版 `828937...` 做一次诊断性复测：

- 使用与 R0 相同的 `NDMRESET/重载/PC/continue` 顺序，不沿用早先直接 `continue` 的失败流程。
- 如出现 feature UART 原始统计和 CPU 分类，只裁定“方向可行”，不作为最终源码/bitstream。
- 如仍取指失败，保留原始 OpenOCD/GDB/UART 日志，跳过无意义的阈值调整。

该 Gate 只为节省实施时间，不得将备份分支中的 RAW gain/LED 实验带入最终差分。

### Gate R2：正式最小 RTL/APB 实现（60-90 分钟）

新建 `codex/minimum-board-recognition` 分支，仅修改：

- `src/feature_stats/feature_stats_tap.v`
- `tests/rtl/feature_stats_tap_tb.v`
- 新增独立 `src/feature_stats/feature_snapshot_apb.v`，不再堆进 `apb_reg_magic.v`
- `src/top.v`
- 新增 `tests/rtl/feature_snapshot_apb_tb.v`
- `integration/single_camera_feature_contract.md`

实现要点：

1. 保留 `MAGIC@+0x000`，新增 STATUS/FRAME_CFG/R/B/Y/FG/ROI/LUMA/BBOX/ACK 和最小配置寄存器。
2. 连接 Hard SoC `PWDATA`；在独立 APB 模块内处理已观测的 word-offset，并用 testbench 固化 `CPU base+4 -> APB PADDR=1 -> STATUS`。
3. 像素域发布后冻结全部 payload，直到匹配 `frame_id` ACK；总线域仅在同步 valid 后一次拷贝冻结 payload。
4. 配置使用 shadow + commit toggle，ACK 使用 frame-id + toggle；禁止逐字段裸 CDC。
5. mailbox 忙时丢弃新候选帧，必须保持旧 snapshot 不变。首版不将 `SNAPSHOT_OVERRUN` 附加到下一个可用 snapshot，避免 CPU 拒绝后不 ACK 造成永久占用。
6. 默认 `capture_enable=0`，只有 CPU 写入配置并 commit 后开始发布；不影响未运行固件时的 HDMI 路径。
7. 不修改 `constrain.sdc`、`.peri.xml`、Hard SoC IP `settings.json`、Debayer、framebuffer 或 HDMI 数据路径。

必须覆盖的 RTL 用例：双像素展开、ROI 边界、帧发布、错 ACK、正确 ACK、未 ACK 前的新帧丢弃、配置 commit、APB 非法读写和两个时钟域的 reset。

### Gate R3：精简 CPU 识别固件（45-60 分钟）

以候选 `feature_uart_onchip` 为参考，在当前 `main` 正式新增片上 RAM 固件：

- 链接 `single_camera_classifier.c` 与 `single_camera_feature_adapter.c`。
- 不首先链接 `single_camera_runtime.c`，避免 `snprintf`/事件 schema 扩大体积和板级变量。
- 启动先读 `REG_MAGIC`；不匹配则仅输出 `FATAL APB_MAGIC`，不继续试探。
- 写入固定 ROI/背景/门限/配置序号并 commit，然后读 snapshot。
- 采用 `FRAME_CFG -> payload -> FRAME_CFG` 双读一致性校验。
- 对完整且一致的 mailbox，先 ACK，再打印长 UART 文本，避免 115200 bps 打印占用 mailbox。
- 对 flags/config 不可用的完整 mailbox，输出 `REJECT` 并释放；不得因 fail-closed 拒绝而永久卡住快照槽。
- 输出 raw feature + `COLOR/SHAPE/SIZE=NA/STABLE`，不发出任何 arm request。

构建验收：入口 `0xF9000000`，唯一 LOAD 不超过 `0xF9003FFF`，未解析符号为 0，RAM 使用保留至少 4 KiB 栈/运行余量。

### Gate R4：冷构建与当前批次身份（15-30 分钟）

1. 从新分支当前 HEAD 冷构建 FPGA 和 feature ELF。
2. 记录 Map/Interface/PNR/STA/CDC、warning 分类、bitstream/ELF hash 和输入 hash。
3. 任一工程输入变化都使旧 `828937...`/旧 ELF 失效；最终板测只能使用新批次匹配产物。
4. Setup/Hold 必须非负，CDC 不能出现未处理 warning，Interface/post-synthesis warning 分开记录。

### Gate R5：上板真实识别（45-90 分钟）

1. 只做 JTAG/SRAM 易失性配置，先确认 J48/ch0 HDMI 不回归。
2. 用 `USER2` 下载 feature ELF 到 `0xF9000000`，使用 R0 已验证启动顺序。
3. UART0 先看 `APB_MAGIC` 和连续帧号，再摆放真实物体。
4. 按“空背景 -> 红 -> 蓝 -> 黄 -> 黑 -> 白 -> 正方体/圆柱/锥体”保存原始特征与 CPU 结果。
5. 首轮标定只调 CPU/APB 配置，不反复重构建 FPGA。

第一板级 PASS 标准：

- 连续至少 100 个 frame-id 无死锁，ACK 后帧能继续前进。
- 空背景和摆物的 `FG/BBOX` 有可重复区分。
- 红/蓝/黄三色每类连续 5 次均输出正确颜色。
- 黑/白各 5 次输出结果并保留 raw feature；如未达标必须明确标记高风险，不伪报。
- 至少能稳定区分 `CUBE` 与 `NON_CUBE/UNKNOWN`；圆柱/锥体细分不在首个 Gate 强行宣称。
- UART 原始日志、配置值、bitstream/ELF hash 与当前 HEAD 一起写入 Review Packet。

## 5. 最小 APB ABI 草案

> 仅为 R2 实施输入；必须随当前 SoC 生成物和 RTL testbench 定版。

| CPU byte offset | 方向 | 语义 |
|---:|---|---|
| `0x000` | R | `MAGIC=0x375A0001` |
| `0x004` | R | valid + source flags |
| `0x008` | R | config_seq + frame_id |
| `0x00C..0x020` | R | R/B/Y/FG/ROI_COUNT/SUM_LUMA |
| `0x024` | R | bbox width/height |
| `0x028` | W | ACK frame_id |
| `0x040` | R/W | capture enable + commit |
| `0x044/0x048` | R/W | ROI x/y |
| `0x04C` | R/W | background RGB |
| `0x050` | R/W | foreground delta |
| `0x054` | R/W | color thresholds |
| `0x058` | R/W | config sequence |

首版不加 IRQ、不加 CPU->OSD result registers、不加目标配置和操作员事件。CPU 轮询单槽 mailbox 已足够完成第一识别闭环。

## 6. 风险排序与回退

| 风险 | 最小处理 | 禁止的扩展 |
|---|---|---|
| 基础 CPU 启动再失败 | 回到 R0，只看 NDMRESET/hart/PC/UART | 不同时改 APB/feature/阈值 |
| APB offset 错位 | 用 `MAGIC/+4 STATUS` 独立测试固化 word-offset | 不在 CPU 端猜多套偏移 |
| mailbox 死锁 | 保持已发布 payload，对每个完整快照明确 ACK | 不将不可用 flags 留在单槽中永不释放 |
| UART 太慢导致过载 | ACK 先于长文本，只每 N 帧打印一次 | 不回压视频主链 |
| 黑/白不稳 | 收紧固定 ROI，固定背景/补光，保留 raw luma/FG | 不用 CPU 硬编假结果 |
| 形状门限不稳 | 首先保住 CUBE/NON_CUBE，用 FG/bbox 标定 | 不在首个 Gate 扩展复杂几何 RTL |
| FPGA 时序/黑屏回归 | 回退到 G1 匹配 bitstream，保留 UART Hello | 不叠加 OSD/按键/机械臂 |

## 7. 明确延后项

以下全部延后到 R5 真实识别 PASS 之后：

- 尺寸 `2/2.5/3 cm` 标定与任务三/四。
- 拨码/按键目标输入、event_seq/ACK。
- 完整 G2 runtime、四任务 matcher 和逐轮状态机。
- CPU->OSD 结果回写和像素渲染。
- UART2/J52、myCobot 协议、点位和任何动作。
- Flash 擦写/固化和外部 DDR CPU 程序。

R5 之后的紧接顺序应为：`尺寸标定 -> 目标输入 -> 逐轮判断 -> OSD/SKIP -> 机械臂`，不得跳过 OSD/SKIP 直接连机械臂。

## 8. 预计紧急总耗时

| 路径 | 乐观 | 保守 |
|---|---:|---:|
| R0 基础 Gate | 15 min | 30 min |
| R1 候选方向复测 | 30 min | 45 min |
| R2 RTL/APB 正式最小差分 | 60 min | 90 min |
| R3 精简 CPU 固件 | 45 min | 60 min |
| R4 冷构建/证据 | 15 min | 30 min |
| R5 上板/首轮标定 | 45 min | 90 min |
| **合计** | **3.5 h** | **5.75 h** |

这个时间预估的前提是：R0 可快速重现、Efinity 当前安装可用、摄像头/HDMI 基线不回归，且现场有固定背景和补光。若 R0 失败，总时间不再可信，必须立即转为 CPU 启动救援。

## 9. 第一执行指令

获得用户对本计划的确认后，立即从 **Gate R0** 开始，不先编辑 RTL。R0 PASS 后产生一个独立 Review Packet，再扩大到 R1/R2。所有板级步骤仍禁止 USER1、Flash、外部 DDR、UART2/J52 和机械臂。
