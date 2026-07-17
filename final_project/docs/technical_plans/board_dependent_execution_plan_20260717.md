# 2026-07-17—2026-07-21 单摄项目收敛与板卡/协作依赖执行方案

> 审计状态：`AUDITED PLAN / BOARD F0`
>
> 当前攻关对象：`competition_project_single_camera/`（板级 Gate 关闭前仍是隔离候选，不替代 `final_project/` 正式协作主线）
>
> 目的：把网页对话中的建议校正为与真实源码、固定制品和当前 Gate 一致的执行任务卡。
>
> 本文不是烧录、接线、串口、Flash 或机械臂动作授权。

## 0. 执行裁定

当前不是“代码缺失”，而是“离线证据已存在、板级证据链尚未起步”：

| 范围 | 2026-07-17 实读结论 | 能证明 | 不能证明 |
|---|---|---|---|
| Git | 审计时 `codex/no-board-debug-plan-20260717@1dc6bce`，工作树 clean；本地/远端 `main=9acf4d8` | 当前无板改动范围可追溯 | 当前分支已经进入 `main` |
| G1 FPGA/SoC | 固定输入基线 `489ab5b0...`；当前 HEAD 的原子输入未变化 | 当前 G1 冷构建证据仍适用 | 视频、CPU、UART、APB 已板级通过 |
| G1 bitstream | 11,847,132 B；SHA-256 `A897E33514A1079BB1B46C02C464B0BD679AF551CEFCB67C4A0EBD5B8FCD1ACD` | 当前允许预检的唯一 bitstream 身份 | 已成功配置到板 |
| G1 Hello ELF | 31,116 B；SHA-256 `E5BC80A2F18A7E2951D53DA539BE2FC61AAECFA90C5CDADB29E65FFC6141928A`；LOAD `0xF9000000..0xF9000A30` | 当前允许预检的唯一 Hello ELF 身份 | CPU 已取指或 UART0 已输出 |
| G2 CPU | 本轮 fresh offline presubmit：C Host `182/182`、Python `3/3`、QEMU arm assertions executed；总结果 `PASS_WITH_WARNINGS` | Host/fake transport、顺序与负例逻辑 | RISC-V 正式固件、真实 MMIO/APB、OSD、机械臂 |
| 当前比赛能力 | `F0` | 离线构建和 Host 逻辑有基础 | `F1/F2/F3` |

**下一主阻塞不是重新冷构建，也不是继续写分类器，而是现有板卡操作卡和脚本仍绑定 2026-07-16 旧制品。** 在形成并审查当前 G1 批次专用操作包前，持板成员不得直接照旧卡执行。

## 1. 对网页方案的必要纠偏

### 1.1 [Contradiction Report]

1. 现有 M2 操作卡/脚本固定旧 bitstream `2EA4AD28...` 与旧 ELF `C99FD39D...`，不匹配当前 G1 的 `A897.../E5BC...`；旧材料只能作为历史证据，不能改写后复用。
2. `SESSION_HANDOFF.md` 仍称当前 SHA 尚未重跑 Efinity；这已被 G1 冷构建 Review Packet 和 `CURRENT_STATE.md` 推翻。
3. 单摄 feature RTL 已存在并有 testbench，但 `src/top.v` 中 `i_capture_enable=1'b0`、ACK 固定关闭、输出标为 unused；正确状态是“RTL 存在但未进入 APB/CPU”，不是“RTL 未实现”，更不是“板级可用”。
4. 当前 APB0 已有只读 MAGIC：offset `0` 返回 `0x375A0001`。不得再新增固定测试寄存器；那会重复造轮子并使当前 G1 原子批次失效。
5. `competition_project_single_camera/cpu` 与 `final_project/cpu` 是两套未收敛 CPU 栈；不得整体复制后者的 `0xF...` 占位地址或 `board_io` 到单摄工程。
6. 单摄 MMIO transport 当前全部 fail-closed，CPU→OSD 业务通道也没有实现；OSD 不是“最后接一根线”，而是需要独立 ABI、APB write、CDC 和像素渲染审查的 Gate。
7. “纯 FPGA 视频已通、加 CPU 后黑屏”是历史观察；当前 G1 Packet 没有匹配批次的视频/屏幕原始证据，因此只能作为排障线索，不能据此先改 RTL/XML。
8. 本轮 freshness 实跑为 `WARN=7 / FAIL=0`，不是旧计划中的 WARN=4。WARN 不阻断 G1 原子制品身份，但必须保留，不能擦成纯 PASS。

### 1.2 明确取消的建议

- 不新增 `project_control/`、`FINAL_BASELINE.md`、`DAILY_FACTS.md` 或第二份总控方案；现有 `AGENTS.md`、`CURRENT_STATE.md`、Review Packet、合并治理和本文件已经覆盖这些职责。
- 不新建长期 `release-final` 或 `integration-debug` 分支；板级能力仍为 F0 时，`release` 命名会制造错误成熟度。
- 不把 feature 契约缩成 `area/x/y/valid`。必须保留 `frame_id`、`config_seq`、颜色/前景/ROI/亮度/bbox、`source_flags`、原子快照与 ACK。
- 不把机械臂放进 F0→F1 关键路径；F1 验收期间 `ARM_ENABLED=0`。

## 2. 唯一关键路径

~~~mermaid
flowchart LR
    A["L0 当前 G1 操作包"] --> B["L1 USER2 + RAM + PC 范围证明"]
    B --> C["L2 UART0 115200 三次启动"]
    C --> D["L3 既有 APB MAGIC 实读"]
    D --> E["L4 冻结 F1 ABI"]
    E --> F["L5 单次受审 F1 硬件原子批次"]
    F --> G["L6 单摄 RISC-V F1 + OSD"]
    G --> H["L7 20 轮 ARM=0 验收"]
    H -. "另立电气/动作 Gate" .-> I["F2 UART2 / myCobot"]
~~~

硬停止规则：

- L0 不通过：不得上板。
- L1 的 PC 不在 `0xF9000000..0xF9003FFF`：不得 Resume、不得打开串口。
- L2 不通过：不得读 APB、不得改地址、不得接 feature/OSD。
- L3 不通过：不得开始 F1 硬件批次；先核查同批 `soc.h`、clock/reset 和 APB read path。
- L4 未批准：`single_camera_mmio_transport.c` 必须继续 fail-closed。
- L7 未通过：UART2/J52、myCobot 帧、接线和动作继续 HOLD。

## 3. 分工总表

| 任务 ID | 主责 | 本机/队友 | 前置依赖 | 交付物 | 验证人 |
|---|---|---|---|---|---|
| LOCAL-G1-01 | qzs + 本机 Codex | 本机 | 当前 G1 Packet | 当前批次操作卡、manifest、fail-closed 脚本 | qzs + libaoxun |
| LOCAL-GOV-01 | qzs + 本机 Codex | 本机 | 实读 Git/状态 | 状态、handoff、依赖看板和证据索引一致 | qzs |
| LIB-BOARD-01 | libaoxun | 持板机 | LOCAL-G1-01 批准 | USER2/RAM/PC 原始证据 | qzs/Codex |
| LIB-BOARD-02 | libaoxun | 持板机 | PC checkpoint 批准 | UART0 三次启动原始证据 | qzs + wsc |
| WSC-CPU-01 | wsc | 队友机，可无板并行 | 当前同批 BSP/`soc.h` | 独立 APB MAGIC reader 源码、ELF 静态审计 | qzs/Codex |
| TEAM-APB-01 | wsc + libaoxun | 联合 | UART0 PASS | 既有 `0x375A0001` 板级实读 | qzs/Codex |
| TEAM-ABI-01 | wsc + libaoxun，qzs 裁决 | 联合 | APB MAGIC PASS | F1 寄存器/CDC/OSD Review Packet | Codex Gate |
| LIB-FPGA-02 | libaoxun | 队友机/持板机 | TEAM-ABI-01 批准 | 单次 F1 硬件原子批次、冷构建和视频回归 | qzs/Codex |
| WSC-CPU-02 | wsc | 队友机 | 同批生成物与 ABI | 单摄 RISC-V F1 固件，ARM=0 | qzs/Codex |
| TEAM-F1-01 | 三人 | 持板机 | FPGA + CPU 同批制品 | OSD 与 20 轮事务证据 | qzs |
| ARM-F2-01 | qzs 主责 | 当前不执行 | F1 + G7/G8 全部 PASS | 独立 UART2/动作 Review Packet | 用户 + Codex |

若 wsc 暂时无法线下参与，本机 Agent 可以在个人分支准备 WSC-CPU-01 的最小代码和测试骨架，但不得隐式取消 wsc 的 CPU ABI owner 审查，也不得在未经固定 SHA 审查时并入硬件批次。

## 4. 本机立即执行的修改

### LOCAL-G1-01：当前 G1 批次专用操作包

只新增，不覆盖 2026-07-16 历史卡和脚本：

- `competition_project_single_camera/docs/debug_sessions/g1_user2_uart0_board_operator_card_20260717.md`
- `competition_project_single_camera/docs/debug_sessions/g1_current_batch_manifest_20260717.json`
- `competition_project_single_camera/docs/review_packets/G1_USER2_UART0_BOARD_GATE_REVIEW_PACKET_DRAFT_20260717.md`
- `competition_project_single_camera/tools/capture_g1_user2_artifact_preflight.ps1`
- `competition_project_single_camera/tools/prepare_g1_user2_staging.ps1`
- `competition_project_single_camera/tools/capture_g1_uart0_banner.ps1`

实现要求：

1. manifest 固定基线 `489ab5b0...`、bit/ELF 完整 SHA-256、大小、LOAD/entry、UART0 115200、USER2 和禁止项。
2. 脚本读取 manifest 与显式 `-BitPath/-ElfPath/-StagingRoot`；禁止自动选择目录中的第一个 `.bit/.elf`。
3. hash 或大小不符必须非零退出；已有 staging 文件不一致时停止，不静默覆盖。
4. preflight 和 staging 默认不访问 Efinity、JTAG、Programmer 或 COM。
5. banner 采集默认 fail-closed；只有携带同批 PC checkpoint 批准记录时，才允许在持板机只读打开 UART0 115200；首轮不发送字符。
6. 操作卡必须把过程拆成两个 checkpoint：
   - A：匹配 bitstream → USER2 → RAM ELF → 暂停并证明 PC 范围；
   - B：qzs/Codex 审核 A 后，才 Resume 并监听 UART0。
7. 明确禁止 `USER1`、SoftTap、原始 `debug_ti.cfg`、Flash、DDR、UART2/J52 和机械臂。

无板验收：

- PowerShell AST 语法检查；
- 正确 hash dry-run PASS；
- 错误 bit、错误 ELF、缺文件、已有冲突 staging、缺 checkpoint 均非零退出；
- 未批准情况下不得枚举/打开 COM；
- `git diff --check` 通过。

### LOCAL-GOV-01：状态和交接收口

本机负责：

1. 修正 `SESSION_HANDOFF.md` 中“尚未重跑 Efinity”的旧结论；
2. 在 `CURRENT_STATE.md` 把“当前操作包绑定旧批次”列为 NEXT_GATE 前置阻塞；
3. 修正单摄 integration 文档中“Hard SoC/feature RTL 未实现”的过期描述；
4. 等当前分支完成并决定合并时，再在结果 `main` 更新 `MERGE_REGISTER.md`、`maintenance_manifest.json` 和 freshness 入口；
5. 连续三次运行 `tools/offline_presubmit.ps1`，分别保留 exit code、摘要与 WARN；`PASS_WITH_WARNINGS` 不得简写为 PASS；
6. 收到每个板级 Gate 的原始证据后才更新 `CURRENT_STATE.md`，不得预写 PASS。

## 5. libaoxun 的持板任务

### LIB-BOARD-01：USER2 / RAM / PC Gate

允许范围只有：

1. 在持板机 ASCII staging 路径重新核对：
   - bit：11,847,132 B，`A897E335...FCD1ACD`；
   - ELF：31,116 B，`E5BC80A2...41928A`。
2. 仅用 volatile/SRAM 路径配置匹配 bitstream。
3. RISC-V 调试仅选择 TJ375/QCRV32 的硬 TAP `USER2`。
4. 调试配置使用 `ftdi_ti.cfg + debug_ti_m2_safe.cfg`；不得加载原始 `debug_ti.cfg`。
5. 仅下载匹配 ELF 到片上 RAM，暂停后回传 Console、PC 和反汇编证据。
6. PC 必须在 `0xF9000000..0xF9003FFF`；此阶段不得 Resume、不得打开 UART。

必须回传：操作者、时间、branch/HEAD、源与 staging 制品完整 hash/大小、器件/USER2/RAM 配置截图、下载 Console、PC/反汇编、全部 warning。

### LIB-BOARD-02：UART0 Gate

仅在 qzs/Codex 对 LIB-BOARD-01 签发通过后：

1. Resume；
2. 只读监听 UART0 115200；
3. 完成三次独立启动并保存原始字节/文本、COM 号、时间和异常；
4. 无输出、乱码或 reset 异常立即停止，不改 TAP、地址、时钟、XML 或 RTL。

合格只关闭“当前批次 CPU 取指 + UART0”子门，不自动关闭 APB、视频、OSD 或机械臂。

## 6. wsc 的 CPU 任务

### WSC-CPU-01：独立 APB MAGIC 固件

可在 UART0 板测等待期间无板并行准备，新建：

`competition_project_single_camera/cpu_bringup/apb_magic_onchip/`

要求：

1. 保持 `uart_hello_onchip/` 基线不变；
2. 复用同批启动代码、linker、BSP 和 UART0 输出；
3. 从生成的 `soc.h` 读取 `IO_APB_SLAVE_0_INPUT`，不得硬编码 `0xE8100000`；
4. 只读取 offset 0，期望 `0x375A0001`；禁止 APB 写；
5. 输出唯一、可审计的 `APB_MAGIC_PASS` 或 `APB_MAGIC_FAIL observed=... expected=...`；
6. ELF 只有一个 LOAD，且完全位于 `0xF9000000..0xF9003FFF`；
7. 单独形成 Review Packet；Hello PASS 前不得上板执行。

### TEAM-APB-01：真实 MAGIC 实读

前置：LIB-BOARD-02 PASS、固定 bitstream 身份未变、WSC-CPU-01 Review Packet 批准。

失败时只核查同批 `soc.h`、CPU load、APB clock/reset/read path；不得先猜新地址、修改 MAGIC 或恢复旧候选地址。

### WSC-CPU-02：正式单摄 F1 固件

只能在 TEAM-ABI-01 批准后开始：

- 新增 production transport，不覆盖 fail-closed stub；
- 链接 feature adapter、classifier、F1 和 runtime；
- 保持 `SC_RUNTIME_ARM_ENABLED=0`；
- 移除板上构建中的 `BOOT_HOST_ONLY` 语义；
- 审计 `snprintf` 对 16 KiB 片上 RAM 的体积风险；
- 接入经审查的 CLINT/mtime 毫秒时基，不用循环次数伪装毫秒；
- 只使用同批生成 `soc.h` 和批准 ABI。

## 7. Hello 与 MAGIC 之后的单次 F1 原子批次

### TEAM-ABI-01：先冻结接口

Review Packet 至少冻结：

- `TARGET_CFG`；
- `OPERATOR_EVENT / EVENT_ACK`；
- `FEATURE_SNAPSHOT` 全字段、frame 二次检查、ACK 与 overrun；
- `ROUND_RESULT`；
- CPU→OSD 的识别、判断、执行/不执行与 reason；
- 地址、字段宽度、访问方向、PSTRB、reset 默认值；
- pixel/APB/OSD 各时钟与复位、multi-bit CDC；
- 同批生成物、`soc.h` 和硬件/软件 owner。

### LIB-FPGA-02：只允许一个受审硬件原子批次

在 TEAM-ABI-01 批准后，由 libaoxun 统一实现最小 F1 所需的：

1. APB0 write/PWDATA 路径；
2. feature tap 启用、原子快照、CDC 和 ACK；
3. 目标配置与逐轮事件输入；
4. `ROUND_RESULT/reason` 写回；
5. 最小 OSD 像素渲染。

任一 RTL/top/XML/SDC/IP 输入变化都会使当前 G1 bitstream/ELF、STA/CDC/warning 和板级证据失效。新批次必须重新完成：

- RTL/testbench；
- Efinity cold Map/Interface/PNR/STA/CDC/bitstream；
- 输入 hash、制品 hash、warning 分类；
- 纯视频旁路回归；
- 新批次 USER2/CPU/UART/APB 分层复验。

不得把 `final_project/cpu` 的占位地址、旧 outflow 或历史视频观察带入该批次。

## 8. F1 验收与 F2 边界

### TEAM-F1-01：最低可交付闭环

F1 必须同时满足：

- live feature snapshot 到 CPU；
- CPU 五色/形状基础分类与四任务判断；
- 非目标明确 `SKIP + reason`；
- 目标明确 `EXECUTE_ARM_DISABLED + reason`；
- OSD 显示识别、判断、执行/不执行及理由；
- 20 轮无重复 ACK、无重复结果、无死锁，且总时限不超过 10 分钟；
- arm request/send 始终 `0/0`；
- 固定 SHA、匹配 bitstream/ELF 和原始运行证据可复查。

### ARM-F2-01：当前 HOLD

只有 TEAM-F1-01 通过后，才可另立 UART2/J52、电气兼容、协议只读和动作 Review Packet。UART0 115200 的任何成功都不能授权 myCobot 1000000、接线或动作。

## 9. 7/17—7/21 时间盒与降级

| 截止 | 必须关闭 | 若失败 |
|---|---|---|
| 7/17 | LOCAL-G1-01 设计/审查；WSC-CPU-01 可并行准备；状态矛盾收口 | 不上板，不改硬件 |
| 7/18 上午 | USER2/RAM/PC checkpoint | 保持 F0，只在当前批次内定位 |
| 7/18 下午 | UART0 三次启动；随后另 Gate 读 MAGIC | Hello 未过则不进入 APB；MAGIC 未过则不冻结 F1 ABI |
| 7/19 | TEAM-ABI-01 与最小 F1 批次范围冻结 | 不再扩展字段或算法 |
| 7/20 | F1 新批次构建、板测与 20 轮尝试 | 冻结最高已验证层级，禁止以 Host 结果冒充 |
| 7/21 | 代码/制品/文档/PPT/演示材料冻结 | 不再扩大架构；F2 未通过即保持机械臂禁用 |

机械臂不占用上述关键路径。任何一天都以“新增了哪个可复查 PASS 证据”为成果，不以代码行数或 Agent 已修改文件数计进度。

## 10. Git、交接与文档/PPT

### Git

- 当前 `codex/no-board-debug-plan-20260717` 只承载无板测试、治理和本方案；验证后走 PR，不直接把实验写入 `main`。
- G1 操作包、WSC APB 固件和 F1 原子硬件分别使用固定范围短期分支；禁止长期动态集成分支。
- 每次交接固定远端 ref、完整 SHA、纳入/排除范围；合并前按治理规则 fetch、merge-tree 和受影响验证。
- 不吸收会修改当前 Hello 源码/构建链的 `c3a6c75`，除非明确重开 ELF/bitstream 批次。
- 不整树合并 G1/G2 dirty worktree，不清理队友机器本地配置。

### 每次交接统一格式

~~~text
TASK_ID:
OWNER:
BRANCH / FULL_SHA:
CHANGED_FILES:
FIXED_INPUTS_AND_ARTIFACT_HASHES:
COMMANDS_AND_EXIT_CODES:
VERIFIED:
NOT_VERIFIED:
WARNINGS:
BLOCKED_BY:
STOP_CONDITION:
NEXT_OWNER_AND_REQUIRED_INPUT:
~~~

### 文档与 PPT

- 不另建一套并行状态文档；事实只写入 `CURRENT_STATE.md`、对应 Review Packet、合并登记和本执行方案。
- qzs 每关闭一个 Gate 就同步一条“证明/未证明/证据路径”，PPT 直接复用这些内容。
- PPT 只展示：系统边界、证据阶梯、实际通过的 Gate、F1/F2 降级和安全设计；不得把 Host、旧视频观察或计划状态写成板级成果。
- 三轨学习指南只在真实分支合并后依据最终 Git diff 与 handoff 生成，不为未落地计划提前造指南。

## 11. 当前立即动作

1. **本机 qzs/Codex**：实现并 dry-run LOCAL-G1-01；同时完成 LOCAL-GOV-01，不碰 G1 原子输入。
2. **wsc**：在独立分支准备 WSC-CPU-01，只读已有 MAGIC，不改 Hello/RTL/XML。
3. **libaoxun**：暂不按旧 M2 卡上板；先核对新操作包、持板机路径和 Efinity/USER2 配置，等 qzs/Codex 发出 PC Gate 执行许可。

这三项完成前，不安排 feature、OSD、UART2 或机械臂开发。
