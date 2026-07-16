# CURRENT_STATE — 最新路线增量与当前状态索引

> 本文件记录对历史规划文件的最新覆盖项与当前阶段状态。优先级见 `AGENTS.md`「文档优先级与交接规则」。
> 真实源码、工程 XML、构建日志和上板现象始终是最终事实来源；本文件只记录对它们的最新结论和证据位置。
> 安全红线（机械臂动作、Codex 审查门、系统架构硬边界、Git 规范）不可被本文件覆盖。
> 活跃条目只保留当前路线、阻塞和下一步；PC 端历史实测归入“历史参考”，未定硬件选择归入“待定决策”。

## 状态条目格式

每条至少包含：

- 日期与来源 Agent
- 适用范围
- 最新结论
- 替代了哪个旧结论
- 证据路径或日志路径
- 失效条件

## 当前阶段

- 日期与来源 Agent：2026-07-07，用户批注 / Gemini 回答 / Codex 二次校验
- 适用范围：分赛区决赛主线阶段识别
- 最新结论：`HDMI 双摄透传 bring-up / 分赛区决赛保底方案` 已降级为历史基础链路或旧结论，不再作为默认当前核心攻关任务。后续当前目标必须由用户最新指令或最新 handoff 明确声明，例如 ROI/统计特征与 OSD、CPU 分类参数、CPU 与 myCobot UART 控制协议联调等；Agent 不得从旧保底方案自动推断当前阶段。
- 替代旧结论：`分赛区决赛实施开发路线.md` 中以 HDMI 双摄透传保底为当前核心目标的阶段性表述，以及 `debug_records/camera_hdmi_handoff_2026-07-06.md` 中的阶段性 bring-up 状态。
- 证据路径：`AGENTS.md`「分赛区决赛系统架构硬边界」（原「分赛区决赛主线」）、`分赛区决赛实施开发路线.md`、`debug_records/camera_hdmi_handoff_2026-07-06.md`、用户 2026-07-07 批注。
- 失效条件：用户明确恢复 HDMI 双摄透传为当前攻关目标，或新的 `CURRENT_STATE.md` 状态条目给出更高优先级结论。

## 活跃状态与路线覆盖项

- 日期：2026-07-16，来源 Agent：Codex / 用户现场确认（来源与精确 main 重建 Hard SoC bitstream 的 USER2 + 片上 RAM + UART0 Hello 板级 Gate）
  - 适用范围：仅隔离候选工程 `competition_project_single_camera/` 的最小 CPU Hello 板级启动门；不替代 `final_project/` 正式主线，也不表示 feature snapshot、CPU 分类、OSD、按键、UART2/J52 或机械臂闭环。
  - 最新结论：来源联合副本 bitstream `AA133887F3D5CE19768C35C3E1775019D370AAC66FE95ABEE46503A63BA96F31` 和精确 `origin/main@0737304` 重建 bitstream `6B672889BCAB2ED2F10CAFA279E13B67463F24B57A2964176F1C9D577D5CC2A4` 均已通过 JTAG 临时加载到 FPGA SRAM，均未写 Flash；两个批次均由用户确认真实摄像头 HDMI 画面正常。当前 `6B6728...` 批次通过 FTDI channel 1、BSCAN `USER2` IR=9 识别 4 个 RV32 hart（XLEN=32，`misa=0x4004112d`）；片上 RAM 中 538 bytes 与 Hello ELF SHA-256 `4AD5CA14147D45B4594C498A3976BDD36EC3570E982B09DC21744669D91CC78A` 校验一致。CPU0 从 `0xF9000000` 单步后到 `0xF9000004`，独立 resume 后运行至 UART 回显循环，采样 `pc/dpc=0xF900019A`、`mcause=0`、`mepc=0`、`mtval=0`。COM10 在 `115200 8N1` 收到完整三行横幅并回显 ASCII `K`，COM13 无输出。因此 `6B6728...` 批次为 `HDMI / USER2 JTAG / ONCHIP RAM / CPU EXECUTION / UART0 HELLO / UART0 ECHO = PASS`。
  - 启动与调试控制结论：NDMRESET assert/deassert 仍是必要步骤。当前 `6B6728...` 曾在 OpenOCD `RUN_SMP_APP` 聚合 target/resume 路径下出现运行后 `pc=0`、`mcause=1` 且 UART 无输出；同一 bitstream、ELF 和 RAM 内容改用非 SMP 的四个独立 hart target，明确 halt CPU1..3、只设置并 resume CPU0 后完整通过。该失败不得归因于 bitstream 载荷或片上 RAM；后续最小启动固定使用独立 hart 控制，不使用 SMP 聚合 resume。
  - 板级顺序与已知现象：带电直接重配曾出现 DDR 横带花屏；完全断电冷启动后再通过 JTAG SRAM 加载同一 bitstream，HDMI 恢复正常。推荐顺序为开发板完全断电 -> 冷启动 -> JTAG SRAM 临时加载匹配 bitstream -> USER2 连接 -> NDMRESET -> 四核 halt -> 片上 RAM 下载/校验 -> CPU0 设置 `pc=0xF9000000` 并 resume -> UART0 横幅/回显。断电会同时丢失 FPGA SRAM bitstream 和片上 RAM Hello，必须重走该顺序。
  - 替代旧结论：覆盖本条先前对当前 `6B6728...` 的 `BOARD LOAD NOT VERIFIED`，以及下方单摄 M2 条目中对来源 `AA1338...` 的 `CPU EXECUTION + UART0 NOT VERIFIED`；历史文件已丢失的 `1D697F...` 仍保持独立 `NOT VERIFIED`，不得借用当前文件结果。
  - 证据路径：`competition_project_single_camera/WORK_LOG.md` M2-34 至 M2-37、`competition_project_single_camera/docs/review_packets/m2_hard_soc_source_sync_review_20260716.md`；板级原始 OpenOCD/UART 输出由本次 Codex 任务记录保存，未把含本机路径的临时日志、ELF 或 bitstream 纳入 Git。
  - 继续禁止：未写 Flash，未使用 `USER1`，未初始化或访问外部 DDR，未连接 UART2/J52 或机械臂，未发送任何 myCobot 帧。UART0 Hello PASS 不得推断 UART2/myCobot 可用。
  - APB 审核结论：当前 `6B6728...` 对应真源的 Hard SoC 配置明确为 `PERI_APB_0..4=0`；生成 wrapper 只导出 UART0、DDR CFG、JTAG 和 AXI-A 端口，未导出 APB `PADDR/PSEL/PENABLE/PWRITE/PRDATA/PREADY/PSLVERROR`；提交的 `soc.h` 也没有 `IO_APB_SLAVE_0_INPUT` 或 `0xE8100000`，顶层和源码中不存在 `REG_MAGIC=0x375A0001` 从机。因此当前 bitstream 不具备可读 APB0，禁止用 CPU 对 `0xE8100000` 做试探性访问。
  - 下一门：需另行批准一个隔离工程改造批次：通过 Efinity IP Manager 启用 `PERI_APB_0`，重新生成 Hard SoC wrapper/BSP，在顶层接入只读零等待 `REG_MAGIC` 从机，并依次通过生成物审计、Map/Interface/PNR/STA/CDC、匹配 bitstream 冷启动、HDMI、USER2/UART0 回归后，才允许 CPU 实读。不得手改生成 wrapper，不得从旧 A3 隔离工程拼接 `.peri.xml`，不进入 feature snapshot、CPU 分类主循环、OSD、按键、UART2/J52 或机械臂。
  - 失效条件：bitstream、ELF、Hard SoC/BSP、OpenOCD USER TAP、UART0 引脚/波特率、时钟复位或板卡冷启动现象发生变化；任一变化后从匹配哈希与冷启动顺序重新验证。

- 日期：2026-07-15，来源 Agent：Codex（myCobot 上板 Goal：阶段 0/B1–B3/A0 首轮执行）
  - 适用范围：`mycobot_arm_board_control_advancement_plan_20260715.md` §13 的阶段 0、B1–B3 与 A0；不表示 G4 SoC/PNR、烧录、J52 回环、真实臂只读或动作已经完成。
  - 最新结论：B1–B3 的纯软件子检查点已完成。新增 `mycobot_transaction` 提供 750 ms single-flight、expected-command/精确 payload length、超时、迟到/重复与错误计数；协议层补齐 0x29/0x2B/0x69、官方 LEN 窗口和 J1..J6 绝对范围，越界 `SEND_ANGLES` 编码被拒绝而非饱和。GET_ANGLES 精确请求/响应、LEN 边界和错误向量已进入 QEMU 断言。B2 差分测试补齐 N-poll、DONE 后下一请求、fault/cancel 后 re-init；不新增重复的 transport 枚举。用户确认 COM10 为 myCobot 后，阶段 0 脚本以 1 Mbps 调用唯一允许 API `MyCobot280.get_system_version()`并成功返回 `7.3`；运行记录为 `motion_or_firmware_api_called=false`，设备侧已刷文件 hash 仍不可回读。B1 源已用 Efinity `rv32imac/ilp32` 工具链 `-Werror -fsyntax-only` 通过，但当前构建器仍被刻意限制为 `NOT_FOR_FLASH` 且硬性排除 UART2/真实 transport，故尚无可烧录 ELF。A0 当前工程的 PLL/JTAG 资源与旧 GUI 冲突审计一致，但旧隔离树已不存在，当前工程的 GUI 合法组合尚无本批次证据；因此 A0/G4 继续未关闭。
  - 验证：`run_mycobot_arm_skeleton_host.ps1` exit 0（Efinity RISC-V QEMU asserts executed PASS）；`run_arm_runtime_host.ps1` exit 0（disabled/simulated PASS）；显式 Efinity 工具链的 `run_arm_runtime_qemu.ps1` exit 0（两后端 QEMU PASS + 1 秒 timeout probe PASS）；`riscv-none-embed-gcc -march=rv32imac -mabi=ilp32 ... -Werror -fsyntax-only mycobot_protocol.c mycobot_transaction.c` exit 0；相关 banner 源改动后的 `arm_bringup/disabled` 目标 RISC-V 构建 exit 0，ELF 内已核验嵌入 manifest build ID，但仍为 `NOT_FOR_FLASH`；`python -m py_compile final_project\\tools\\mycobot_firmware_version_readonly.py` exit 0；`git diff --check` exit 0（仅 CRLF 预警）。
  - 证据路径：`final_project/docs/debug_sessions/evidence/mycobot_stage0_readonly_20260715_090247.json`、`final_project/docs/review_packets/mycobot_b1_b3_protocol_transaction_review_20260715.md`、`final_project/docs/review_packets/mycobot_g4_evidence_contract_review_20260715.md`、`final_project/docs/debug_sessions/mycobot_goal_execution_20260715.md`、`final_project/cpu/app/src/mycobot_transaction.c`、`final_project/tools/mycobot_firmware_version_readonly.py`、`final_project/tools/verify_mycobot_g4_batch.py`。后者已通过正反向自测，且只验证未来 G4 批次的一致性；没有实际 G4 manifest 时不得写作 G4 PASS。
  - 下一门：先由用户按 debug session 的 A0 操作卡完成当前隔离副本 GUI 资源审计，并确认 myCobot 的实际 COM 端口；在收到前，不改 XML/top/SDC、不跑联合 PNR、不接 J52 或机械臂。
  - 失效条件：协议真源/固件版本改变、B1/B3 相关回归失败、GUI 证实资源不同、端口身份或现场电气证据与本条不符。

- 日期：2026-07-16，来源 Agent：libaoxun688 补交 / Codex 合并复核（单摄 M2 Hard SoC 真源与离线构建已关闭，板上 CPU Hello 待验证）
  - 适用范围：仅隔离候选工程 `competition_project_single_camera/`；不替代 `final_project/` 正式主线，也不表示 CPU 分类、OSD 回写、UART2 或机械臂闭环。
  - 合入结论：在既有 `dev/libaoxun688@e129885` 的 framebuffer/feature tap/CPU Host 候选基础上，固定补交提交 `2d4b3b7b3d0c59d88ece0669534ae38de02ed938` 与 `0604d33f3fe76851e1dfe738403875b4a7d0721c` 已同步 `mem_test.xml`、`.peri.xml`、SDC、`top.v`、Hard SoC IP 可复现输入、最小 BSP 和 UART0 Hello。2026-07-15 的“Hard SoC 系统真源缺失/HOLD”已失效，不得再作为当前阻塞。
  - 系统契约：`mem_test.xml` 同时登记 `feature_stats_tap` 与 `EfxSapphireHpSoc_slb`，`debugger.auto_instantiation=off`；DDR AXI0 启用、AXI1 禁用；视频 JTAG 使用 `USER1`，QCRV32 使用 `USER2`；UART0 RX/TX 为 `GPIOR_165/GPIOR_145`，SoC reset 为 `GPIOL_79`；视频状态机继续唯一驱动 DDR `CFG_START/RST/SEL`，Hard SoC 只读取 `CFG_DONE`，未形成双驱动。FPGA/CPU 职责边界未改变。
  - Codex 新构建：在合入前对精确提交 `0604d33` 使用 Efinity 2025.2 重新运行完整工程，Map、Interface、Placement、Routing、bitstream 均完成；最差 Setup/Hold 为 `+1.742ns/+0.018ns`，CDC 为 `No Synchronizer warnings to report.`。Interface Check 为 `0 error / 4` 个物理距离 warning；post-synthesis netlist 另有 118 个 warning，主要来自继承 Demo/IP 与未使用的 AXI-A 顶层端口，不能与 4 个 Interface warning 混为一谈，也不能描述成零 warning。
  - 证据批次边界：Codex 新构建 bitstream SHA-256 为 `1D697F0DBA62CEDA3A8877729FF29A314F9BBA1A24CDCDFEDB751C7CF4B8AECC`，尚未上板。来源联合副本的旧 bitstream `AA133887F3D5CE19768C35C3E1775019D370AAC66FE95ABEE46503A63BA96F31` 与 J48/ch0 视频正常证据只属于来源批次，不能证明新构建已板测。
  - CPU 检查点：提交的最小 BSP fail-closed 构建通过；UART0 Hello 为 2608 B / 16 KiB，入口 `0xF9000000`，唯一 LOAD 段至 `0xF9000A30`，无未解析符号，`ELF_LOAD_AUDIT=PASS`。USER2 JTAG 实际连接、CPU 从片上 RAM 取指、UART0 115200 横幅和单字符回显仍为 `NOT VERIFIED`。
  - 调试边界：下一门只允许使用匹配的新 bitstream、FPGA `USER2` 和 `0xF9000000` 片上 RAM 执行 UART0 Hello；禁止 `USER1`、Flash erase/program、外部 DDR 初始化、UART2/J52、机械臂接线或动作。CPU Hello 通过前不得进入 feature snapshot、OSD、按键或 myCobot。
  - 证据路径：`competition_project_single_camera/docs/review_packets/m2_hard_soc_source_sync_review_20260716.md`、`competition_project_single_camera/WORK_LOG.md` M2-26 至 M2-34、`competition_project_single_camera/cpu_bringup/uart_hello_onchip/README.md`、`competition_project_single_camera/ip/EfxSapphireHpSoc_slb/settings.json`、`competition_project_single_camera/mem_test.xml`、`competition_project_single_camera/mem_test.peri.xml`、`competition_project_single_camera/constrain.sdc`、`competition_project_single_camera/src/top.v`。
  - 下一门：在不连接机械臂的安全台面上，先记录新 bitstream 完整 SHA-256，再烧录匹配 bitstream 并回归 J48/ch0 视频；随后选择 FPGA `USER2`，仅下载 Hello ELF 到片上 RAM，采集 UART0 完整横幅与单字符回显。任何一步失败都停在当前层，不扩展到 UART2/myCobot。
  - 失效条件：`mem_test.xml`、`.peri.xml`、SDC、`top.v`、Hard SoC IP/BSP、时钟复位、USER TAP、UART0 引脚或构建/板级现象发生变化，或新的 Efinity fatal/时序/CDC 问题出现。

- 日期：2026-07-15，来源 Agent：Codex（wsc CPU 优先融合与环境等价性整改）
  - 适用范围：`origin/dev/wsc6090-cpu@df45b5a` 的统一结果语义、ARM_DISABLED 候选 adapter、task_matcher 真值 reason、Host 测试和 G0–G3 规范目标构建；不表示 G4 正式 SoC、APB/OSD wire ABI、UART2 或真实机械臂闭环。
  - 最新结论：以 wsc 的 CPU 设计和其 MinGW/Efinity 环境结果为功能主门，同时关闭本机 MSVC 兼容门。两个测试的 `PASS()` 宏已将遮蔽业务变量的局部 `int d` 改为唯一名称；MSVC 19.42 `/std:c11 /W4 /WX` 实跑 `cpu_result_semantics 374/374`、`main_loop_arm_disabled 117/117` PASS。脚本改用显式参数/`VCVARS64_PATH`/`vswhere` 发现 VS，GCC 门补 `-Wshadow -Werror`，不得通过 `/wd4456` 或降低 `/WX` 规避。
  - 接入结论：同步 main 后丢失的状态口径已纠正。正式 `main.c` 现在消费 `cpu_display_from_round_output()`，以统一语义层校验 action/reason/is_target 并对矛盾组合 fail closed；`main_loop_adapter.c` 已进入 competition 目标构建清单，但在 G4 取得受审事件源和单调时基前不由正式 main 调用。禁止恢复 wsc 旧版未受审 UART 单字符事件和直接 CLINT 假设。
  - 目标构建：Efinity RISC-V GCC 8.3.0 对 `competition/arm_bringup × disabled/simulated` 四组合均 BUILD/ELF PASS，制品继续标记 `NOT_FOR_FLASH`，UART2/real transport 继续被排除；构建器同时修复 Windows PowerShell 原生 stderr 被提前升级为异常、导致完整 gcc 诊断丢失的问题，warning allowlist 与退出码门保持不变。
  - 环境差异记录：wsc 原环境报告 MinGW GCC 14.2.0 下 `374/374`、`117/117` PASS；Codex 修复前在 MSVC `/W4 /WX` 因 C4456 于断言执行前失败。该差异由警告策略不对称暴露了真实测试可移植性问题，不构成 CPU 功能逻辑失败。Codex 本机无 PATH gcc、也无仓库 `tools/mingw64/bin/gcc.exe`，带 `-Wshadow` 的 GCC 复验仍需 wsc 在其环境执行。
  - 替代旧结论：替代下方 2026-07-14 条目中“main.c 与 Host 测试链接同一 main_loop_adapter 实现”和“MSVC 本机不可用”的旧口径；旧 Host 结果保留为 wsc 原环境证据，不再描述当前正式 main 接入状态。
  - 证据路径：`final_project/cpu/app/src/main.c`、`final_project/cpu/app/src/main_loop_adapter.c`、`final_project/cpu/app/src/cpu_result_semantics_adapters.c`、`final_project/cpu/build_tools/build_arm_profile.ps1`、`final_project/cpu/tests/run_cpu_result_semantics_host.ps1`、`final_project/cpu/tests/run_main_loop_arm_disabled_host.ps1`、`final_project/docs/review_packets/wsc_cpu_mycobot_integration_acceptance_20260715.md`。
  - 下一门：由 wsc 在同一集成 commit 上执行 MinGW `-Wshadow` 两项 Host 回归和 Efinity 四组合构建，回传编译器版本、完整命令、退出码与 manifest；Codex 再核对结果后才允许向 main 提交 CPU 集成 PR。真实接线/烧录/动作继续 NO-GO。
  - 失效条件：集成 commit、语义枚举、main/adapter 接法、构建源码清单、编译器 flags 或目标工具链发生变化，或任一回归失败。

- 日期：2026-07-14，来源 Agent：Codex（最新云端 `main` 同步后的 myCobot 实验续跑与独立复核）
  - 适用范围：PC 端 180°候选五点路径真机初步验证，以及它与板上 CPU G4–G11 路线的边界；不表示官方 180°/最大臂展验收、板上 UART2 或比赛抓放已经闭环。
  - 分支基线：本地 `main` 已通过 fast-forward 与 `origin/main@39e8a92` 对齐，并从该提交创建 `codex/mycobot-experiment-main-sync-20260714`。原有 `.claude/settings.local.json` 与 `.agents/handoff/`、`.agents/shared/` 仅作为本机状态原样保留，不纳入本分支提交。
  - 最新结论：保留 `mycobot_pc_tests/archive/teach_replay_pick_success_baseline_20260714_153337.py` 不变。PC 端真机 A 路线候选路径初步验证为 `PASS_WITH_RISKS`：最新 `auto_run_20260714_204908.log` 在 `COM9` 完成 5/5 连续流程、0 轮人工扶正，日志未记录异常；但这不是官方目标位或板上闭环验收。候选点内部坐标方位差为 177.72°，pick/drop 平面半径约 223.5/223.0 mm；没有外部 180°基准或“臂展最大处”证据。
  - **已知风险点（板上移植前必须解决）**：
    1. **drop_hover 长距离过渡固件系统性不收敛**：最新 5/5 轮 step 6 固件均返回 0（未收敛或超时），靠“读实际角度 → max_diff=2.02° ≤ 3° → 软通过”兜底才没熔断。板上 RISC-V 移植时必须实现 `get_angles` 读回 + 3° 容差判断；读回不稳定时该点位不可放行动作。
    2. **夹爪完全开环，无任何到位反馈**：最新 5 轮 × 每轮 3 次夹爪动作共 15 次 `[GRIP_UNVERIFIED]`，0.8s 是纯定时经验值。板上移植时 0x66/0x67 无 ACK，必须用 0x69 轮询停止 + 0x65 读位置窗口确认；位置读回仍不能单独证明夹持力或未滑落。
    3. **外部几何与结构风险未关闭**：点位 JSON 无 `external_reference`；内部 177.72°不能替代台面外部量角，约 223 mm 半径未确认是“臂展最大处”。E3 被跳过，本次重复成功只证明当次条件下可运行，不能反推底座刚性风险已关闭。
  - **E3/E4 状态（2026-07-14 用户决策 + Codex 复核）**：现场因硬件条件跳过 E3 后完成 PC 真机候选路径验证，E4 记为 `PASS_WITH_RISKS`，E3 仍为 `NOT_VERIFIED`。本结果允许明日推进 G4 正式 SoC/PNR、G5 断臂板上模拟和 D2 UART2 无臂回环；不得直接放行真实机械臂接线或动作。
  - 与主线关系：PC 证据只用于开发期点位与指令序列参考，不能跳过 G4 正式 SoC/PNR、G5 断臂板上模拟、G7 UART2 无臂回环、G8 只读和 G10 动作 Review Packet；`competition_project_single_camera/` 尚未完成 M0 板级复现，也不改变本条机械臂门禁。
  - 证据路径：`final_project/docs/review_packets/mycobot_180deg_initial_verification_evaluation_20260714.md`、`debug_records/mycobot_route_a_experiment_debug_record_20260714.md`、`mycobot_pc_tests/presets/teach_points_route_a_20260714_2000.json`、本机忽略日志 `mycobot_pc_tests/audit_logs/auto_run_20260714_204908.log`（SHA-256 `EB89B99A...F9F475`）、`final_project/docs/technical_plans/mycobot_pc_experiment_continuation_plan_20260714.md`。
  - 失效条件：机械臂安装/基座/物块/投放区再次变化，脚本或预设发生修改，或出现新的外部测量、串口、板上或动作证据；失效后从离线门和机械固定门重新开始。

- 日期：2026-07-14，来源 Agent：Codex（myCobot G1.5–G3.5 纯软件关闭）
  - 适用范围：机械臂代码上板前的构建隔离、板上无臂模拟入口、Host/QEMU/RISC-V 制品门与主循环安全语义；不表示正式 SoC、bitstream、烧录、J52 或机械臂已经完成。
  - 最新结论：`mycobot_cpu_board_bringup_implementation_plan_20260714.md` 继续作为详细执行入口。`codex/mycobot-g0-g3-bringup-20260714@b48973b` 的 G1.5–G3.5 工作已通过合并提交 `81657c4` 进入 `origin/main@39e8a92`：默认 disabled，独立 `arm_bringup` 与正式 `competition` 两入口均只生成 `NOT_FOR_FLASH` 制品；真实 UART2/myCobot transport 仍被源码、flags、nm、map 和反汇编门排除。
  - 检查点结果：MSVC `/W4 /WX` 的 disabled/simulated runtime Host 回归均 PASS；严格 QEMU 两后端 PASS，`scratchpad.lds` 已与 `_start` 统一，逐步编译/链接均检查退出码，1 秒无限循环固件超时注入 PASS。RISC-V `competition/arm_bringup × disabled/simulated` 四组合均 BUILD/ELF PASS；disabled bring-up 现包含固定交错 20 轮零请求自检，未通过放宽 `round_controller_tick` 门规避。`-BoardBuild` 现在在创建输出目录前 fail closed；manifest v2 记录规范构建入口、输入/制品 SHA-256、完整 flags、warning policy，Makefile 仅是该 PowerShell 构建器的薄包装。
  - Gate 状态：G0 PASS；G1 PASS（仅 G0–G3 的 standalone/NOT_FOR_FLASH 构建隔离）；G2 PASS（**structural bridge only**：无受审事件源、无真实时基，legacy matcher 不再冒充逐轮机械臂事务）；G3 PASS（纯软件证据）。G4、正式 SoC/PNR/部署、烧录、J52 和真实机械臂继续 NO-GO。
  - 集成边界：上述代码/脚本已进入 `main` 的软件基线，但只证明 G0–G3 的 Host/QEMU/NOT_FOR_FLASH 软件门；本轮未修改 FPGA、未烧录、未连接 J52/机械臂、未产生 UART2 或动作帧，不能据此宣称 G4 或板级闭环。
  - 历史并发记录：最终 QA 时（2026-07-14 14:08:19）共享工作区曾被外部会话切到无关的 `codex/explore-log-return-20260714`；14:16 已切回并在 `b48973b` 完成提交。该记录仅用于追溯，不再描述当前 `main` 工作区。
  - 协议复核：Elephant Robotics 官方协议确认现有 `FE FE LEN CMD ... FA`、`LEN=payload+2`、1 Mbps 8N1、0x20/0x22/0x65/0x66/0x67 和角度缩放方向基本正确；同时明确 0x22、0x66、0x67、0x29 无返回值，有返回命令应在 500 ms 内响应，且直接协议通信前需 Basic=`transponder`、Atom=最新版 `atomMain`。因此 G7–G11 新增 single-flight/750 ms 初始 deadline、expected-command/精确长度、官方逐关节限位、0x29+0x2B+0x20 停止辅证和 0x69+0x65 夹爪确认；TX 成功不得再写成动作完成。
  - 责任边界：G1.5–G3.5 已由 Codex 在纯软件范围内关闭。G4 的正式 SoC/PNR/烧录需要用户操作 Efinity/板卡并提供日志；G5 起任何 Programmer、接线、仪器、只读和动作必须由用户现场确认与执行。
  - 下一步门禁：按用户要求在 G3 Review Packet 后停止。若要进入 G4，用户需另行确认板卡、Efinity 2025.2/许可证、JTAG、UART0 和仪器条件；仍不得依据本条目烧录、连接 J52 或连接机械臂。
  - 替代旧结论：替代 `mechanical_arm_member_next_step_brief_20260713.md` 中“16-bit 修复尚未合入”和学习指南中“ARM_DISABLED 已是现存宏”的过时状态描述；旧文档的 T0、D2、D3、D5、Gate D、F1/F2 和物理安全门继续有效。
  - 证据路径：`final_project/docs/review_packets/mycobot_g1_g3_protocol_checkpoint_review_20260714.md`、`final_project/docs/technical_plans/mycobot_cpu_board_bringup_implementation_plan_20260714.md`、`final_project/docs/technical_plans/mycobot_board_bringup_operator_sop_20260712.md`、`final_project/integration/mycobot_protocol_notes.md`、Elephant Robotics 官方 `6.6 基于串口通信协议开发使用`页面、当前分支真实源码/构建脚本。
  - 失效条件：官方页面或目标机械臂固件版本改变协议，或真实抓包与当前命令/响应语义冲突；冲突关闭前只缩小白名单，不扩大 REAL 放行。
- 日期：2026-07-14，来源：用户批准 / Codex M0 基线审计与主线合并复核
  - 适用范围：分赛区决赛单摄候选视频工程、CPU/机械臂迁移方案与 M0 基线复现；不表示新构建、单摄裁剪、SoC/APB/OSD 或机械臂闭环。
  - 最新结论：`competition_project_single_camera/` 作为隔离的单摄候选工程合入，来源是曾在 J48/ch0 稳定输出真实摄像头到 HDMI 的本地 Demo 镜像。`final_project/` 在候选工程完成新 Efinity 构建、匹配 bitstream、烧录和板级复现前仍是正式协作主线；不得提前废弃其视频工程或把候选工程写成唯一正式源码。候选路线采用单个斜上方摄像头、固定投放点、可配置识别框、硬核 QCRV32、UART 先行、最终 HDMI OSD，并按任务一五色→任务二 CUBE/NON_CUBE→任务三/四尺寸→F1 四任务 20 轮→F2 机械臂推进。
  - 交互冻结：第一版只使用`PLACE`和独立`ABANDON`小闭环，不设`REMOVE`、自动空场互锁或复杂按键菜单；目标配置开发期走UART。尺寸在标定前必须输出`UNAVAILABLE`。F1保持`ARM_ENABLED=0`。
  - 工程与同步边界：候选源码位于仓库，本地 Demo 目录仅作为人工 Efinity 构建/烧录镜像；每个 FPGA Gate 均须回归 J48/ch0 到 HDMI 画面，任何回归立即停止并回退。候选工程通过 M0 板级复现后，才可另行更新 `AGENTS.md`、主方案和状态索引以升格。
  - M0 执行结果：初始白名单复制时共 75 个文件与镜像 SHA-256 一致，53 个设计文件、2 个 IP 设置、SDC 和 4 个 include/mem 依赖存在；该 75/75 只描述初始快照。随后 M0-09 修改了 `white_balance.v`，本次合并又增加除零防护，当前状态必须结合新增的 delta manifest 阅读，不能再称当前树与初始镜像 75/75 一致。历史 bitstream 哈希 `A99F14AB8922783C51A71953288F9A25DE1C70DC3E3962F5508BBF53EF75057C` 只绑定历史可运行镜像，不绑定当前仓库源码。
  - 构建风险：历史Efinity 2025.2.288.4.15 flow从map到bitstream成功，资源ADD 2416/LUT4 14540/FF 11516/RAM10 211/DPRAM10 4，CDC报告无Synchronizer warning；但timing报告存在跨时钟setup负slack，最差`mipi_clk -> i_sysclk_div2 = -1.433ns`，因此只能称历史可运行基线，不能称STA PASS或warning可忽略。
  - 当前发现：本地镜像除官方目录结构外还存在 `outflow_ch0_*`、多个 `work_*`、`.lock`、源码内 `.bak` 和仿真数据库，因此 M0 采用白名单复制，未把整个目录视为干净官方原版，也未删除任何历史产物。
  - 路线关系：本条新增单摄候选与回退路线，不替代 `final_project/` 的正式主线身份，不覆盖其历史验证结论，也不改变 `AGENTS.md` 中的 FPGA/CPU/PC 职责和机械臂安全硬边界。
  - 证据路径：`competition_project_single_camera/docs/review_packets/m0_demo_baseline_review_packet_20260714.md`、`competition_project_single_camera/docs/baseline/m0_source_manifest_20260714.csv`、`competition_project_single_camera/docs/baseline/m0_post_baseline_delta_20260714.csv`、`competition_project_single_camera/WORK_LOG.md`。
  - 下一步门禁：用户按`competition_project_single_camera/docs/debug_sessions/m0_manual_build_board_check_20260714.md`手动运行完整Efinity构建、烧录、3次冷启动和10分钟画面复现。全部通过前，不裁剪ch1/DSI、不生成SoC、不迁移CPU、不修改视频链。
  - NOT VERIFIED：仓库副本对应的新Efinity构建/bitstream、重新烧录和画面复现、历史负slack约束安全性、单摄裁剪、SoC资源、UART1、APB、特征、按键、OSD、四任务、20轮和机械臂均未验证。

- 日期：2026-07-14，来源 Agent：Codex（队友 CPU 分支 + Fable 整改 + 个人 UART2 证据集成）
  - 适用范围：`dev/wsc6090-CPU@885e97a`、`codex/fable5-remediation-20260713@758e864` 与个人证据分支的本次集成；不表示正式 SoC/APB/OSD、PNR、bitstream、真实摄像头或机械臂闭环。
  - 最新结论：保留队友 CPU 分支新增的 `arm_busy` 发起动作安全门，以及 abandon、机械臂运动期软复位/放弃锁定、故障、超时和任务匹配补测；同时以 Fable 整改后的 16-bit `event_seq`/ACK ABI 为当前契约，采用半范围新旧判定，支持 `65535 -> 0`，拒绝重复、倒退和差值为 32768 的歧义事件。事件消费继续显式返回 `ACCEPTED`/`REJECTED`，1000 事件随机流也纳入回归。`vision_classifier.c` 的零结果常量改为标准 C 显式 `{0}` 初始化，以通过 MSVC `/W4 /WX`，不改变分类语义。
  - 替代旧结论：替代队友源分支中 8-bit `event_seq`/`last_event_seq` 和 175/175、944/944 的阶段性口径；也刷新 Fable 整改的 `round_controller 4919/4919` 快照。历史提交与 Review Packet 仍可追溯，但后续实现必须以 16-bit 契约为准。
  - 验证：MSVC 从当前集成源码重建并通过 classifier 31/31、param_table 81/81、task_matcher 154/154、round_controller 4979/4979、competition_rounds 135/135、competition_contract 45/45、competition_host_flow 164/164、A13 replay 169/169，合计 5758/5758；myCobot skeleton Host/Mock 原生断言运行通过但不计入总数。`round_controller.c`、`vision_classifier.c`、`task_matcher.c` 通过 Efinity RISC-V GCC 8.3.0 `rv32imac/ilp32` compile-only，仅放行 `soc.h`/APB 占位告警。
  - FPGA 构建：Efinity 2025.2 production/default map PASS，资源为 `EFX_ADD=2081`、`EFX_LUT4=11939`、`EFX_FF=10492`、`EFX_RAM10=250`、`EFX_DPRAM10=8`；`mem_test.warn.log` 137 行，不能标记为可忽略。PNR 仍 FAIL：2288 个 IO 无 placement，随后 `!available_io_sites.empty(): outpad` 断言失败；无可验收 bitstream/STA。显式 `COMPETITION_DEBUG_SYNTHETIC=1` 的 debug map 也 PASS，资源回到 ADD 1827、LUT4 10339、FF 7991、RAM10 154，warning 日志 138 行；它只证明合成源入口可显式打开，不能代替 production 或真实视频验收。
  - 证据路径：`final_project/docs/review_packets/cpu_branch_merge_adaptation_20260714.md`、`final_project/cpu/CPU_MODULE_PLAN.txt`、`final_project/cpu/app/src/round_controller.c`、`final_project/cpu/tests/test_round_controller.c`、`final_project/cpu/tests/test_task_matcher.c`、`final_project/fpga/rtl/top/top.v`。
  - 下一步门禁：先修复 Interface Designer/periphery 与顶层 IO 导出边界并重跑 PNR/STA；正式 `soc.h`、APB/CDC/OSD、UART2 D2 回环和真实机械臂 T0 未关闭前，保持 `ARM_DISABLED`，不得连接或驱动机械臂。
- 日期：2026-07-14，来源 Agent：Claude（Fable 5，cpu_result_semantics 统一语义层 Codex Gate 通过后归档）
  - 适用范围：`cpu_result_semantics` 统一结果/理由展示语义层（纯语义头 + 适配头拆分、上游枚举→统一语义转换、round_controller_output→cpu_display_result 合法组合精确校验）；不适用于 main.c、OSD/APB wire ABI、板级、UART2、真实机械臂闭环。
  - 最新结论：cpu_result_semantics 统一语义层已通过 Codex Gate 复审，当前为 Gate 通过版。核心实现：(1) 纯语义头 `cpu_result_semantics.h` 仅依赖 `<stdint.h>`，可独立编译，不含 board_io.h 的 APB 占位 warning；(2) 适配头 `cpu_result_semantics_adapters.h` 提供上游枚举/结构→统一语义的三条转换路径；(3) `cpu_display_from_round_output()` 精确校验 `(action + reason + is_target)` 完整组合，仅接受 A/B/C/D 四类合法组合，阻止/故障分支严格校验 is_target（ARM_NOT_READY/ARM_FAULT 只接受 is_target=1，其余阻止理由只接受 is_target=0），不匹配组合一律安全兜底 ERROR/FAULT/INVALID_INTERNAL，绝不 REQUESTED/正常 SKIP；(4) 全部公开枚举显式赋值，NONE=0，INVALID_INTERNAL=255（`_Static_assert` 固定），并注明仅用于 CPU 内部语义，非 APB/OSD wire ABI，禁止直接序列化。
  - 替代了哪个旧结论：替代 cpu_result_semantics 首版（未拆分头文件、未精确校验 is_target、理由码未细分、白名单为"超集"的旧口径）；将 CPU_MODULE_PLAN.txt [7] 模块状态从"实现完成，等待 Codex Gate 复审"更新为"Gate 通过，待后续接入"。
  - 验证（2026-07-14 复跑）：
    - test_cpu_result_semantics（repo mingw64 gcc 14.2.0，`-Wall -Wextra -Werror -Wno-error=cpp`）：**374/374 PASS**，含纯语义头独立编译门（无 APB 占位宏、不加 `-Wno-error=cpp`，纯 TU 无 board_io warning 输出）。
    - round_controller MinGW gcc 回归（同门）：**4979/4979 PASS**（round_controller.c/.h 未改，无回退）。
    - RISC-V compile-only（Efinity riscv-none-embed-gcc 8.3.0，rv32imac/ilp32，`-c`）：cpu_result_semantics.c exit 0，obj 5092，无 warning；cpu_result_semantics_adapters.c exit 0，obj 4292，仅既有 board_io.h:26 soc.h/APB 占位 warning。
    - `git diff --check`：pass。
  - 仅有的 warning：既有 `board_io.h:26` 的 soc.h/APB 占位 `#warning`，出现在 adapters TU / round_controller TU（经 vision_classifier.h 传递），由既有 `-Wno-error=cpp` 放行。纯语义头 TU 在严格门下无 warning。其余 warning=0。
  - 证据路径：`final_project/cpu/app/include/cpu_result_semantics.h`、`final_project/cpu/app/include/cpu_result_semantics_adapters.h`、`final_project/cpu/app/src/cpu_result_semantics.c`、`final_project/cpu/app/src/cpu_result_semantics_adapters.c`、`final_project/cpu/tests/test_cpu_result_semantics.c`、`final_project/cpu/tests/test_cpu_semantics_pure.c`、`final_project/cpu/tests/run_cpu_result_semantics_host.ps1`、`final_project/cpu/CPU_MODULE_PLAN.txt`。
  - 未完成边界：
    - 未接入 main.c。
    - 未接 OSD/APB wire ABI；未定义任何 APB 地址/寄存器位布局；硬件映射留给后续适配层。
    - 未做板级、UART2、真实机械臂闭环。
    - 未连接或驱动机械臂。
    - 本机无 MSVC，`/W4 /WX` 主路径未实跑（run 脚本保留 MSVC 主路径 + mingw64 兜底）。
    - 主动 ARM_FAULT 与机械臂等待超时在本层仍同映射 ARM_FAULT→FAULT（未拆分，待后续接入时决定）。
  - 失效条件：cpu_result_semantics 接口/枚举语义变更、main.c/APB/OSD 接入后行为改变，或后续回归重跑失败。

- 日期：2026-07-14，来源 Agent：Claude（Fable 5，ARM_DISABLED Host/mock main loop adapter Gate APPROVE；当前接入状态由 2026-07-15 条目覆盖）
  - 适用范围：`main_loop_adapter` ARM_DISABLED 候选适配器（含 `main_loop_adapter.c/.h`、`task_matcher` 的 `_return_match()`/`get_last_match()` 真值 reason）；不适用于当前正式 main 接入、APB/OSD wire ABI、UART2、真实机械臂或板级闭环。
  - 最新结论：ARM_DISABLED Host/mock 候选适配器已完成并通过其原环境 Gate。三个 P1 在候选实现内关闭：(P1-1) `main_loop_arm_disabled_step()` 抽出为独立函数并由 Host 测试直接链接；(P1-2) `attempted_event` 精确保存，只在 `REMOVE_CONFIRM+ACCEPTED` 时返回 1（ABANDON 不冒充 REMOVE），`SESSION_RESET+ACCEPTED` 返回 -1；(P1-3) `task_matcher_evaluate()` 的 `_return_match()` 保存完整 `task_match_result_t`（含真实 color/shape/size reason）。当前正式 main 仅接入统一结果语义投影，adapter 待 G4 事件源/时基后再接。
  - 验证（2026-07-14 复跑；本机 repo mingw64 gcc 14.2.0，`-Wall -Wextra -Werror -Wno-error=cpp`）：
    - test_main_loop_arm_disabled：**117/117 PASS**（16 测试）
    - test_cpu_result_semantics：**374/374 PASS**（回退无）
    - round_controller MinGW gcc 回归：**4979/4979 PASS**（回退无）
    - test_task_matcher：**154/154 PASS**（回退无）
    - RISC-V compile-only（Efinity riscv-none-embed-gcc 8.3.0，rv32imac/ilp32，`-c`）：main.c exit 0、main_loop_adapter.c exit 0、task_matcher.c exit 0、cpu_result_semantics.c exit 0（仅既有 board_io.h:26 soc.h/APB 占位 warning，由 `-Wno-error=cpp` 放行）
    - `git diff --check`：pass
  - 仅有的 warning：既有 `board_io.h:26` 的 soc.h/APB 占位 `#warning`，经 `-Wno-error=cpp` 放行。其余 warning=0。
  - 证据路径：`final_project/cpu/app/include/main_loop_adapter.h`、`final_project/cpu/app/src/main_loop_adapter.c`、`final_project/cpu/app/src/main.c`、`final_project/cpu/app/src/task_matcher.c`、`final_project/cpu/app/include/task_matcher.h`、`final_project/cpu/tests/test_main_loop_arm_disabled.c`、`final_project/cpu/tests/run_main_loop_arm_disabled_host.ps1`、`final_project/cpu/CPU_MODULE_PLAN.txt`。
  - 明确定义"未闭环"（非"已接入/已连接"）：
    - 未接入 OSD/APB wire ABI；未定义任何 APB 地址/寄存器位布局
    - 未接 arm_controller_request_*、UART2、myCobot transport
    - 不连接或驱动真实机械臂
    - 无板级证据
    - MSVC `/W4 /WX` 本机不可用（未实跑主路径；run 脚本保留 MSVC + mingw64 兜底）
  - 失效条件：main_loop_adapter 接口/返回语义变更、APB/OSD 接入后行为改变、ARM_ENABLED 过渡后 arm_enabled=1 路径重写，或后续回归重跑失败。

- 日期：2026-07-13，来源：用户提供 / Codex 官方资料交叉核查
  - 适用范围：TJ375N529 开发板 UART2/J52 到 myCobot 280 的板端接口真源；不表示正式 SDC/SoC/UART 驱动、真实接线、只读通信或机械臂动作已通过。
  - 最新结论：官方开发板说明、PINOUT 和端口图已交叉确认 J52 为右侧 2.54mm 4Pin UART2；Pin1 GND，Pin2 `FPGA_UART2_RXD/C14/Input`，Pin3 `FPGA_UART2_TXD/F12/Output`，Pin4 `VCC(5V/3.3V)` 必须悬空；C14/F12 均位于 3.3V I/O 域。用户已对 `TJ375N529开发板端口说明图.jpg` 进行人工核验，确认接口位置、外形和四针标签顺序与文字一致。用户确认候选接法为 GND→GND、C14/RXD←机械臂 TX、F12/TXD→机械臂 RX，机械臂独立 12V5A。
  - 安全纠偏：机械臂官方安装说明的 USB-TTL 接线文字为 TXD→机械臂 TX、RXD→机械臂 RX，与常规交叉接法存在命名冲突；该页也未给出机械臂端电平容限。因此 `io_pin_map.md` 仍保持总体 FAIL，正式交叉线序和“无需电平转换直连”须经断电双人复核、机械臂 TX 空闲电平和开发板 1 Mbps 波形测量后才能 PASS。
  - 当前工程边界：正式 `constrain.sdc`、`mem_test.xml` 和 `top.v` 尚未检出 C14/F12/UART2 绑定；本次未修改 RTL、约束、Efinity XML，未连接机械臂、未发帧、未动作。
  - 证据路径：`final_project/docs/review_packets/mycobot_uart2_j52_wiring_review_20260713.md`、`final_project/integration/io_pin_map.md`、`赛方提供材料/硬件文档/开发板使用说明1V0(5).pdf`、`赛方提供材料/硬件文档/TJ375N529_PINOUT_CONFIGURATION.pdf`、`赛方提供材料/硬件文档/TJ375N529开发板端口说明图.jpg`。
  - 下一步门禁：先完成 J52/机械臂端断电双签与双端电平测量，再做机械臂断开的 D2 100/100 帧回环/监听；T0 其余项仍须独立关闭。

- 日期：2026-07-13，来源 Agent：Codex（Fable 5 审查后的首轮无外设整改）
  - 适用范围：CPU `round_controller` 的寄存器无关事件契约、Host 测试，以及 FPGA 顶层 production/debug 合成入口选择；不表示 `main.c`、SoC/APB/OSD、PNR、bitstream、真实摄像头或机械臂闭环。
  - 最新结论：`round_controller` 的事件序号/ACK 序号/已消费序号已统一为 16 bit；新事件会按当前状态输出 `ACCEPTED` 或 `REJECTED`，重复及过期序号不重复消费。轻量 `competition_contract` 同步增加首事件标志并移除“序号 0 永久非法”特例，两层均支持 `0xFFFF -> 0x0000` 半范围回绕。新增越界事件、过期/回绕、20 轮和 1000 事件随机流验证，`ARM_DISABLED` 下保持零动作请求并可恢复到 `CONFIG`。FPGA `top.v` 已改为未定义 `COMPETITION_DEBUG_SYNTHETIC` 时默认选择两路真实输入，合成预处理/HDMI 只在显式 debug 宏下启用。
  - 验证：`run_round_controller_host.ps1` 从当前源码重建并通过 `4919/4919`；`round_controller.c` 通过 Efinity RISC-V GCC 8.3.0 `rv32imac/ilp32 -Wall -Wextra -Werror` compile-only；既有 `competition_rounds 135/135`、更新后的 `competition_contract 45/45`、`competition_host_flow 164/164`、`A13 169/169` 使用 MSVC 从当前源码回归通过。`git diff --check` 通过。本轮未重复运行完整 Efinity map/PNR。
  - 证据路径：`final_project/docs/review_packets/fable5_remediation_checkpoint_20260713.md`、`final_project/cpu/app/include/round_controller.h`、`final_project/cpu/app/src/round_controller.c`、`final_project/cpu/app/include/competition_contract.h`、`final_project/cpu/app/src/competition_contract.c`、`final_project/cpu/tests/test_round_controller.c`、`final_project/cpu/tests/test_competition_contract.c`、`final_project/cpu/tests/run_round_controller_host.ps1`、`final_project/fpga/rtl/top/top.v`。
  - 下一步门禁：`main.c` 的 `ARM_DISABLED` 接入仍须等待同次生成的正式 `soc.h`、APB 时钟/复位证据，以及 `TARGET_CFG/OPERATOR_EVENT/EVENT_ACK/RESULT_COMMIT` 联合 Review Packet；在此之前不得虚构 MMIO 地址或按钮事件源。FPGA 队员须分别生成 production/debug 构建证据，并从 Interface Designer/periphery 修复 1776 IO/outpad PNR 阻塞；不得把 debug 合成结果作为真实摄像头证据。
  - NOT VERIFIED：正式固件链接/烧录、`main.c` 调度、APB/CDC/OSD、production/debug Efinity map/PNR/时序/bitstream、真实摄像头、板级 20 轮和机械臂动作均未验证。

- 日期：2026-07-13，来源 Agent：Claude（B/CPU 支援：B 线列明 8 项 CPU Host 回归复跑，刷新 795 快照）
  - 适用范围：当前分支 `dev/wsc6090-CPU` 工作区、B 线当前纳入计数的 8 项 CPU Host 单测集合的聚合计数；仅 Host/Mock，不适用于 RISC-V 交叉、`soc.h`、APB、OSD、真实摄像头或机械臂。
  - 最新结论：用 `D:\CICC w\tools\mingw64\bin\gcc.exe`（14.2.0）逐项复跑，每项编译均加 `-Werror -Wno-error=cpp`（只放行 board_io.h:23 占位 warning，其它 warning 即编译失败），编译与运行退出码均 0，全部 PASS：vision_classifier 31/31、task_matcher 154/154、round_controller 175/175、competition_round_transaction 135/135、competition_contract 35/35、competition_host_flow 164/164（20 rounds）、a13_fpga_snapshot_replay 169/169（20 rounds；本次为 MinGW gcc 复跑，非评审记录里的 MSVC）。与旧 795/795 相同的 7 项成员刷新为 `863/863` PASS（唯一增量来自 task_matcher 146→154、round_controller 115→175，其余不变）。另 param_table 81/81 复跑通过，但本不在 795 成员内，作额外项记录；含 param_table 的 B 线列明 8 项 Host 回归集合为 `944/944` PASS。措辞订正：这里的“B 线列明 8 项”是 B 线当前纳入断言计数的集合，`944/944` 是该 8 项的断言合计，不等于仓库内全部 Host 测试的总断言数——仓库另有受版本控制的 `tests/test_mycobot_arm_skeleton.c` 与 `run_mycobot_arm_skeleton_host.ps1`，Codex 复跑编译/运行退出码均 0，但它属 Host/Mock、不连接或驱动真实机械臂，且使用原生 assert、无可直接相加的断言总数，故未纳入 `944/944`。
  - 替代了哪个旧结论：正式替代 `team_integration_merge_review_20260713.md` 的 `Host 合计 795/795` 快照——同成员刷新为 863/863；旧 795 不再作为当前 B 线 Host 基线，仅保留为历史快照。不替代任何板级、APB/CDC/OSD、真实特征、`main` 集成或机械臂未闭环的事实。
  - 证据路径：`final_project/cpu/tests/`（test_classifier / test_param_table / test_task_matcher / test_round_controller / test_competition_rounds / test_competition_contract / test_competition_host_flow / test_a13_fpga_snapshot_replay；另 test_mycobot_arm_skeleton 见下）、`final_project/cpu/tests/run_competition_*_host.ps1`、`final_project/cpu/tests/run_a13_fpga_snapshot_replay.ps1`、`final_project/cpu/tests/test_mycobot_arm_skeleton.c` 与 `run_mycobot_arm_skeleton_host.ps1`（Host/Mock、原生 assert、未纳入 944）、`final_project/docs/review_packets/team_integration_merge_review_20260713.md`（旧 795 来源表）、`final_project/cpu/CPU_MODULE_PLAN.txt`（“B 线列明的 8 项 CPU Host 回归复跑”块）。
  - 失效条件：任一 CPU Host 源/测试改动、成员集合调整，或在正式 `soc.h`/APB/OSD/板级/机械臂接入后需改以板级证据为准。

- 日期：2026-07-13，来源 Agent：Claude（B/CPU 支援：task_matcher 覆盖审查补齐）
  - 适用范围：`task_matcher` 动作层（NONE/GRAB/SKIP）的 Host 单测覆盖完整度；不适用于板级、APB、OSD、真实摄像头或机械臂。
  - 最新结论：按 7月13日 B 任务清单第 3 项逐项核对 `test_task_matcher.c`，五色目标、T1/T2（CUBE&&color）、T3（|obs-ref|==10mm）、T4（|obs-target|≤5mm）、UNKNOWN/size=0/NULL 不误触发、非目标 SKIP、旧版精确匹配兼容均已覆盖。补齐三处 task_matcher 层缺口（纯新增测试，未改源码）：T3 `delta=0`→SKIP 边界、MODE_1/MODE_2 非正方体→SKIP、非法/越界 `task_mode`→安全 NONE。Host 结果 `task_matcher 154/154`（146 基线 + 8 新断言），gcc 14.2.0，仅 board_io.h:23 占位 warning。
  - 关键澄清（Codex 复核修正）：`task_matcher_evaluate()` 只返回动作码 NONE/GRAB/SKIP，不产生理由码。当前仓库存在两套并行理由/任务契约：`task_matcher.h`/`round_controller` 使用 `reason_code_t`/`REASON_*`（`task_match_result_t.reason`，由 round_controller 消费），`competition_tasks.h/.c` 使用另一套 `competition_reason_t`/`COMP_REASON_*`；二者的统一/转换层尚未定版，无证据证明已闭环。故清单第 7—10 项的理由码派生不能写成”已由 competition_tasks 闭环”，只能说 task_matcher Host 层已验证对应动作码 SKIP/NONE 正确，不代表理由码统一完成。
  - T3/T4 基准尺寸校验（Codex 复核修正）：task_matcher 公开路径当前不强制 T3/T4 基准尺寸只能是 20/30mm；`competition_tasks` 的 validate 确实限制 20/30mm，但无证据证明它会校验所有进入 task_matcher 的目标。T4 以 `target_size` 为比较基准，与官方”评委选定目标物、差值≤0.5cm”语义一致，但 competition_tasks 侧可能称 `reference_size`，命名需统一或显式映射。本轮 Host 已验证官方有效值路径与非法/越界 mode 的安全行为；后续需选定唯一目标配置适配层作为入口，并补非法 T3/T4 基准值测试或转换测试。
  - 替代了哪个旧结论：将 `test_task_matcher.c` 计数从 146/146 更新为 154/154；不替代任何板级、APB/CDC/OSD、真实特征或机械臂未闭环的事实。
  - 证据路径：`final_project/cpu/tests/test_task_matcher.c`（组[14]）、`final_project/cpu/app/src/task_matcher.c`、`final_project/cpu/app/include/task_matcher.h`、`final_project/cpu/CPU_MODULE_PLAN.txt` [4]。
  - 失效条件：task_matcher 接口/动作语义变更、理由码职责上移到本层，或后续回归重跑失败。

- 日期：2026-07-13，来源 Agent：Claude（B/CPU 支援：round_controller Codex 安全缺口最小修复）
  - 适用范围：`round_controller` 的 arm_busy 安全门与 event_seq 过期/回绕规则；仍为 Host/Mock 层，不适用于板级、APB、OSD、`main.c` 集成或机械臂真实动作。
  - 最新结论：按 Codex 复核意见对 `round_controller.c` 做最小实现修复（本轮确有改源码，非纯补测）。[P1] arm_busy 安全门——目标轮 action=GRAB 且 arm_enabled=1 但 arm_busy=1 时，不发 request_arm_grab、不进入 WAIT_ARM_DONE，直接进入 ROUND_DONE，保留 `result_valid=1`/`is_target=1`，`decision_action=NONE`，`reason=REASON_ARM_NOT_READY`（等价 F1 下识别/判断已锁存但机械臂不可接收动作、不执行）。[P2] event_seq 过期/回绕——新增 `seq_is_newer(seq,last)`：`delta=(uint8_t)(seq-last)`，仅 `delta!=0 && delta<128` 视为向前并接受；重复(delta=0)与过期/倒退(delta>=128)一律不消费、不 ACK；回绕 255→0(delta=1) 接受、0→255(delta=255) 拒绝；`consume_new_event` 改用此判定。Host 结果 `round_controller 165/165`（146 基线 + 19 新断言），gcc 14.2.0，仅 board_io.h:23 占位 warning。随后按 Codex P3 建议追加两个 event_seq 半区间边界回归（`0→255` delta=255 拒绝、`10→138` delta=128 拒绝，各不改状态/不 ACK）+10 断言，Host 计数升至 `175/175`；该 P3 轮仅追加测试，未改 round_controller.c。
  - 替代了哪个旧结论：将 `test_round_controller.c` 计数从 146/146 更新为 175/175，`CPU_MODULE_PLAN.txt` [5] 同步；并修正上一条 round_controller 审查条目“纯新增测试、未改源码”的表述——本轮已对 round_controller.c 做最小逻辑修复（arm_busy/event_seq），其后 event_seq 半区间边界回归仅追加测试。不替代任何板级、APB/CDC/OSD、main 集成或机械臂未闭环的事实。
  - 证据路径：`final_project/cpu/app/src/round_controller.c`（`seq_is_newer`、EXECUTE_OR_SKIP 的 arm_busy 门）、`final_project/cpu/tests/test_round_controller.c`（`[codex-fix]` 组）、`final_project/cpu/CPU_MODULE_PLAN.txt` [5]。
  - 失效条件：round_controller 状态机/事件语义变更、接入 `main.c` 后行为改变，或后续回归重跑失败。

- 日期：2026-07-13，来源 Agent：Claude（B/CPU 支援：round_controller 覆盖审查补齐）
  - 适用范围：`round_controller` 逐轮编排状态机的 Host/Mock 单测覆盖完整度；不适用于板级、APB、OSD、真实摄像头、`main.c` 集成或机械臂真实动作。
  - 最新结论：按 7月13日 B 任务清单第 4 项逐项核对 `test_round_controller.c`。event_seq/ACK 去重、PLACE/REMOVE/SOFT_RESET/SESSION_RESET 独立事件、识别/判断单次锁存、单轮≤1 次动作请求、单 tick 脉冲、非目标 SKIP 直接完成、ARM_DISABLED→ARM_NOT_READY、识别超时可恢复、20 轮 Mock 无死锁均已由原测试覆盖。补齐三处 round_controller 层缺口（纯新增测试，未改源码/头文件逻辑）：ABANDON_ROUND 非运动态结束本轮并保留 `REASON_OPERATOR_ABANDON`；WAIT_ARM_DONE 期间软复位/放弃被忽略（不盲复位、不重复脉冲）；ARM_FAULT 输入与臂超时进入 `ROUND_STATE_ARM_FAULT` 且保留 `result_valid`/`is_target` 证据。Host 结果 `round_controller 146/146`（115 基线 + 31 新断言），gcc 14.2.0，仅 board_io.h:23 占位 warning。
  - 替代了哪个旧结论：将 `test_round_controller.c` 计数从 115/115 更新为 146/146，`CPU_MODULE_PLAN.txt` [5] 同步。不替代任何板级、APB/CDC/OSD、真实特征、main 集成或机械臂未闭环的事实；早前记录的两层整合回归 795/795 为更早快照，未含本轮及 task_matcher 154 增量，需全量复跑刷新。
  - 证据路径：`final_project/cpu/tests/test_round_controller.c`（审查补测组）、`final_project/cpu/app/src/round_controller.c`、`final_project/cpu/app/include/round_controller.h`、`final_project/cpu/CPU_MODULE_PLAN.txt` [5]。
  - 失效条件：round_controller 状态机/事件语义变更、接入 `main.c` 后行为改变，或后续回归重跑失败。

- 日期：2026-07-13，来源 Agent：Codex（团队整合后的文档新鲜度与 Codebase Memory 刷新）
  - 适用范围：`codex/team-integration-20260713@510caca` 的当前状态、接口候选契约、近期执行方案、架构入口和默认协作图谱；不改变官方细则、系统职责边界或任何板级/机械臂 Gate。
  - 最新结论：已把决赛主方案更新为 `v1.3-main`，将 Host/合成源任务标记为已形成证据，并把当前队列转向 `register_map/board_io` 契约收口、`ARM_DISABLED` 主循环集成、production/debug 构建拆分与 periphery/PNR。`register_map.md` 已和 `board_io.h` 当前候选偏移对齐，但正式基址/扩展字段仍等待同一次 SoC 生成的 `soc.h` 与 Review Packet；新增 `current_code_architecture_2026-07-13.md` 作为当前架构入口。
  - 图谱结果：使用 `codebase-memory-mcp 0.9.0` 对 `D:/cicc_cbm_link` 执行 `fast + persistence` 重建，运行时与 artifact 均为 `4514 nodes / 10958 edges`，artifact commit 为 `510caca79ce439da143916fa4c91854f79e3db7a`；`round_controller`、`competition_round_transaction`、`synthetic_2ppc_source` 查询回归通过。新会话/新机器仅依靠 artifact 的冷启动导入尚未验证。
  - 新鲜度结果：`tools/project_freshness_check.ps1` 在图谱重建前已达到 `FAIL=0`；保留的绝对路径 WARN 是隔离工程/工具安装位置的真实外部证据，不代表板级通过。图谱 artifact 与本轮文档的提交/推送状态必须以当前 `git status` 和 `origin/main` 为准，不从本条历史文字推断。
  - 证据路径：`maintenance_manifest.json`、`final_project/integration/register_map.md`、`final_project/docs/architecture/current_code_architecture_2026-07-13.md`、`final_project/docs/review_packets/team_integration_merge_review_20260713.md`、`CBM_CONFIG_GUIDE.md`、`.codebase-memory/artifact.json`、`.codebase-memory/graph.db.zst`。
  - 失效条件：代码结构再次变化、正式 `soc.h`/APB/CDC/OSD 接口定版、production/debug 构建结构改变，或冷启动验证发现 artifact 无法恢复同等查询结果。

- 日期：2026-07-13，来源 Agent：Codex（两位队友分支整合后的正式 FPGA 工程构建）
  - 适用范围：`codex/team-integration-20260713` 中 `final_project/fpga/efinity/mem_test.xml` 与合并后 `top.v` 的 Efinity 2025.2 map/PNR；不表示真实摄像头、HDMI 或 bitstream 上板通过。
  - 最新结论：中文仓库路径直接运行 map 会因 Efinity `filesystem error: Illegal byte sequence / EFX-0002` 在 HDL 前失败；改用仓库 ASCII junction `D:\cicc_cbm_link` 后 map PASS。资源为 `EFX_ADD=1827`、`EFX_LUT4=10339`、`EFX_FF=7991`、`EFX_RAM10=154`；`mem_test.warn.log` 有 132 条 warning 记录，综合摘要报告 post-synthesis netlist 共 1098 warnings，不能标记为可忽略。
  - PNR 结果：FAIL。工具报告 1776 个 IO 无 placement、将被随机放置，随后触发内部断言 `!available_io_sites.empty(): outpad`；未生成可验收 bitstream。该失败再次证明不能给所有顶层端口盲补约束，须先审查 Interface Designer/periphery 导出边界、普通 VDB 与 debug VDB、正式顶层端口和 `.peri.xml`。
  - 证据路径：`final_project/docs/review_packets/team_integration_merge_review_20260713.md`。原始 Efinity 输出位于本次临时构建目录，关键命令、资源、warning 计数和首个 PNR 错误已摘录到 Review Packet；生成型 VDB/netlist/outflow 不提交仓库。
  - 下一步门禁：先解决生产构建模式与 periphery/IO 绑定方法，再重跑 PNR/时序；该整合快照当时令 `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE=1'b1`、`HDMI_USE_SYNTHETIC_VERIFY=1'b1`，现已由上方“真实摄像头源恢复”条目覆盖为 `1'b0`。禁止把 map PASS 描述成 PNR、bitstream 或上板 PASS。

- 日期：2026-07-12，来源 Agent：Codex（A11 合成五色预处理隔离 Map）
  - 适用范围：摄像头无稳定数据流期间的 FPGA ROI/颜色面积/bbox/中心快照候选工程；不适用于 D 盘 HDMI 基线或正式比赛构建。
  - 最新结论：已创建 `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga`。该副本以 D 盘五色 HDMI 基线为源，仅增加 6 个预处理 RTL 和 ch1 合成 tap；HDMI 仍用 RGB 合成流，预处理支路单独转换为 Debayer BGR 合同，避免黄/红蓝通道误读。Efinity map PASS，预处理模块已展开，最终 v2 资源为 `EFX_ADD=1827`、`EFX_LUT4=10339`、`EFX_FF=7991`、`EFX_RAM10=154`。
  - 证据路径：`final_project/docs/debug_sessions/a11_synthetic_preprocess_isolated_map_20260712.md`、`C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\outflow_a11_v2\mem_test.map.out`。
  - 下一步门禁：A11 已生成 USER2 Debugger 配置、PNR 通过且已有专用 `.dbg.vdb/.bit/.hex`。下一步仅允许手动 JTAG SRAM 下载 A11 `.bit`，再采集 11 个快照探针；不得使用旧 D 盘 bitstream，且不得接入 SoC/APB/CPU/OSD。
  - 验证：A11 已经 JTAG SRAM 下载并分别采集黄色、红色、蓝色合成帧；三帧的对应颜色面积均为 `102400`，其他两种彩色面积为 0。白色帧由 HDMI 同时观察确认，快照显示三种彩色面积为 0、`fg_area=102400`、ROI 像素数 `1036800`、bbox `{380,320}..{699,639}`、中心 `{539,479}`、状态为 0。该结论仅覆盖 A11 隔离 bitstream 和已采集的合成帧；当前探针不能独立区分白/黑。
  - 验证增量（20:37）：操作者在 `Run Immediate` 时同步确认 HDMI 为黑色；`C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\la0_waveform.vcd`（最后写入 `2026-07-12 20:37:45`）显示红/蓝/黄面积均为 `0`、`fg_area=102400`、ROI 像素数 `1036800`、bbox `{380,320}..{699,639}`、中心 `{539,479}`、状态 `0`。黑色身份来自 HDMI 观察，当前 11 个探针不能独立区分黑/白；五种合成色的前景/几何快照验证至此完成。
  - NOT VERIFIED：白黑独立区分统计、A11 HDMI 回归和所有真实摄像头/CPU/OSD/机械臂路径均未验证。

- 日期：2026-07-12，来源 Agent：Codex（A12 白黑统计快照探针）
  - 适用范围：A11 隔离合成视频的白/黑基础统计可观测性；不适用于 D 盘 HDMI 基线或正式比赛工程。
  - 最新结论：A12 在 `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga` 的既有 `vision_preprocess_channel` 快照输出上，仅将 `sum_r/sum_g/sum_b/sum_y` 接至四根顶层 `mark_debug` 网络；无新分类 RTL、无像素路径改变。`efx_run --prj -f map` 输出 `outflow_a12`，map PASS，资源 `EFX_ADD=1827`、`EFX_LUT4=10339`、`EFX_FF=7991`，与 A11 v2 相同，map warning 数同为 134。
  - 证据路径：`final_project/docs/debug_sessions/a12_white_black_snapshot_probe_execution_20260712.md`、`C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\outflow_a12\mem_test.map.out`。
  - 下一步门禁：必须在 Efinity Debug Wizard 以 `USER2` 保留原 11 根探针并新增 4 根统计网，生成新的 A12 `.dbg.vdb` 后才允许 PNR、bitstream、JTAG SRAM 下载和白黑 VCD 采集；禁止手工编辑或伪造 `.dbg.vdb`。
  - NOT VERIFIED：A12 Debugger、PNR/时序/bitstream/JTAG 下载、白黑独立板级快照，以及真实摄像头/CPU/APB/OSD/尺寸/机械臂路径均未验证。
  - 验证增量（A12 板级）：15 探针 `USER2` Debugger、PNR/时序/bitstream/JTAG SRAM 下载已由操作者完成。`20:59:26` VCD 黑色快照为 RGB 三通道各 `119603200`、`sum_y=358809600`；`21:03:42` VCD 白色快照为 RGB 三通道各 `145715200`、`sum_y=437145600`。两帧均有彩色面积全 `0`、`fg_area=102400`、ROI 像素数 `1036800`、bbox `{380,320}..{699,639}`、中心 `{539,479}`、状态 `0`。白黑已由 FPGA 统计独立区分，不再依赖 HDMI 人工颜色确认。
  - 注意：HDMI 支路经过 2ppc-to-1ppc CDC/FIFO，瞬时屏幕颜色与源时钟预处理快照可能存在相位差；A12 颜色身份以 VCD 中 `sum_r/sum_g/sum_b/sum_y` 为准。
  - NOT VERIFIED：A12 完整 HDMI 回归，以及所有真实摄像头/CPU/APB/OSD/尺寸/机械臂路径均未验证。

- 日期：2026-07-12，来源 Agent：Codex（A13 FPGA 快照到 CPU Host 回放）
  - 适用范围：A11/A12 隔离合成五色特征到 CPU 分类器和逐轮契约的 Host 回放；不适用于板上固件或正式 APB ABI。
  - 最新结论：`feature_snapshot_t` 已为 Host 回放增加 `roi_pixel_count`、`sum_r/g/b/y`。CPU 对有 A12 帧统计的快照使用 `sum_y / (3 * roi_pixel_count)` 区分白黑；无统计字段时保留旧填充率路径。实测五色回放后，A13 完整 20 轮 Host 流程 `169/169` 通过：任务一、二输出唯一 `EXECUTE/SKIP + COLOR_MISMATCH`，任务三、四在尺寸未标定时固定 `WAIT + SIZE_UNAVAILABLE` 后放弃。
  - 证据路径：`final_project/docs/debug_sessions/a13_fpga_snapshot_cpu_host_replay_20260712.md`、`final_project/cpu/tests/test_a13_fpga_snapshot_replay.c`、`final_project/cpu/tests/run_a13_fpga_snapshot_replay.ps1`。
  - 下一步门禁：恢复 SoC/视频资源审查，先确认 PLL 重规划与正式 snapshot/APB 地址/CDC 契约；不得把 A13 接入 `main.c`、MMIO、OSD 或机械臂。
  - NOT VERIFIED：RISC-V、正式 SoC/APB/CDC、真实相机、OSD、尺寸标定、板级 20 轮和机械臂均未验证。A13 的 APB 占位宏和 `FG_AREA_AVAILABLE=1` 只表达 Host 测试输入，不构成正式硬件接口。

- 日期：2026-07-12，来源 Agent：Codex（A14 SoC/视频 PLL 重规划决策门）
  - 适用范围：从 A13 Host 回放恢复正式 SoC/APB 路线前的时钟资源决策；不适用于 D 盘基线或即时烧录。
  - 最新结论：只读复核确认硬 SoC 系统 PLL 只能使用已被视频占用的 `PLL_BL0/BL1/BL2`；`PLL_TR1` 的 I/O bank 存在不能证明它可替代 `PLL_BL1`。禁止猜测性改 PLL、手改 `.peri.xml` 或直接合并 SoC。唯一下一步是在 A8 隔离工程的 Interface Designer 中检查 GUI 是否提供 `pll_inst1` 的合法 Titanium 迁移候选；无候选即继续阻塞，有候选也只能生成新的隔离工程重审。
  - 证据路径：`final_project/docs/review_packets/a14_soc_pll_replanning_decision_gate_20260712.md`、`final_project/docs/debug_sessions/a4_soc_video_resource_audit_20260712.md`、`final_project/docs/debug_sessions/a9_gui_pll_jtag_resource_audit_20260712.md`。
  - 下一步门禁：取得 GUI 候选项或“不支持”证据前，不得接入 SoC/APB/MMIO/`main.c`/OSD/机械臂。

- 日期：2026-07-12，来源 Agent：Codex（A8 Efinity GUI PLL 审查隔离基线）
  - 适用范围：SoC/视频工程 PLL 资源可行性审查前的隔离工作目录。
  - 最新结论：已从 `D:\final_project\fpga` 单向创建 `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\fpga`，排除 `outflow` 和 `work_*` 构建目录。`top.v`、`mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc` 的 SHA-256 与 D 盘源逐项一致；未写入 D 盘或仓库工程，尚未打开 GUI、修改 PLL/SoC 资源、PNR 或烧录。
  - 证据路径：`final_project/docs/debug_sessions/a8_gui_isolation_baseline_20260712.md`、`C:\fpga_soc_isolated\tj375_video_soc_gui_a8\A8_GUI_ISOLATION_BASELINE_20260712.md`。
  - 下一步门禁：仅在该副本中以 Efinity GUI 读取并记录现有 PLL/JTAG 的真实用途与下游连接；禁止手改 `.peri.xml`、删除 LPDDR4 PLL、运行 PNR 或烧录。任何资源重规划结论必须以 GUI 实际生成物和新的审查记录为准。
  - NOT VERIFIED：GUI 可行性、PLL 重规划、SoC 集成、工程级 map/PNR、bitstream 与板级行为均未验证。

- 日期：2026-07-12，来源 Agent：Codex（A7 CPU Host 端到端 20 轮流程）
  - 适用范围：CPU 纯 Host 适配层的配置、事件、观察、结果和 ACK 调用顺序验证；不适用于板上正式固件。
  - 最新结论：新增 `competition_host_adapter` 和完整 20 轮 Mock。任务一、二各 5 轮可输出 `EXECUTE/SKIP` 与理由；尺寸标定仍暂缓，任务三、四各 5 轮固定 `WAIT + SIZE_UNAVAILABLE` 后通过 `ABANDON` 结束。每轮验证同序号 `PLACE` 幂等、错误 ACK 拒绝、正确 ACK 完成、`REMOVE` 后转入下一轮。Host 端到端测试 `164/164` 通过。
  - 替代旧结论：替代“仅单模块测试，未验证配置到结果完整调用顺序”的状态；不替代正式 `main.c`、SoC、APB、OSD、摄像头或机械臂未接入的事实。
  - 证据路径：`final_project/docs/debug_sessions/a7_cpu_host_end_to_end_flow_20260712.md`、`final_project/docs/review_packets/a7_cpu_host_end_to_end_flow_review_packet_20260712.md`、`final_project/cpu/tests/test_competition_host_flow.c`、`final_project/cpu/tests/run_competition_host_flow.ps1`。
  - 验证：端到端 `164/164`、契约 `35/35`、四任务/逐轮 `135/135`、原 matcher `82/82` Host 测试通过；均使用测试专用 APB 占位宏，且 Host 适配层不访问 MMIO。
  - 下一步门禁：不得将 Host 适配接入板上 `main.c`，直至 SoC/APB 资源门关闭；恢复时先审查目标/事件/结果字段的硬件快照、CDC、commit/ACK 与 OSD 映射。
  - NOT VERIFIED：正式 `main.c`、RISC-V、SoC/APB、FPGA 输入、OSD、摄像头、尺寸标定、板级 20 轮和机械臂均未验证。

- 日期：2026-07-12，来源 Agent：Codex（A6 CPU 接口契约与尺寸标定暂缓）
  - 适用范围：CPU 目标配置、操作事件和结果语义的寄存器无关冻结；用户已明确尺寸标定暂缓。
  - 最新结论：新增 `competition_contract`，冻结 `target_config` 的 staging/apply/轮内锁存、`operator_event` 的 `PLACE/REMOVE/ABANDON/RESET + event_seq`，以及 `result_status` 的识别、判断、决策、理由、状态和序号语义。尺寸状态为 `SIZE_UNAVAILABLE` 时，任务一、二仍可判断；任务三、四固定输出 `WAIT + SIZE_UNAVAILABLE`，不得执行、跳过或发布未标定尺寸。重复同序号事件幂等，旧序号拒绝，匹配 ACK 才结束一轮。
  - 替代旧结论：替代“尺寸字段必须在当前阶段参与四任务演示”的隐含假设；Mock 尺寸仍仅用于规则测试，不构成现场标定或任务三/四完成。
  - 证据路径：`final_project/docs/debug_sessions/a6_cpu_contract_size_deferred_20260712.md`、`final_project/docs/review_packets/a6_cpu_contract_size_deferred_review_packet_20260712.md`、`final_project/cpu/tests/test_competition_contract.c`、`final_project/cpu/tests/run_competition_contract_host.ps1`。
  - 验证：新契约 `35/35`、四任务/20轮 `135/135`、原 matcher `82/82` Host 测试通过；均使用测试专用 APB 占位宏，不代表 `soc.h`、APB 或板级运行。
  - 下一步门禁：在 SoC/FPGA 接口恢复前，保持本契约地址无关且不接入 `main.c`；恢复时先审查目标/事件/结果字段的快照、CDC、commit 和 ACK 映射。尺寸标定验收前，`SIZE_STATE` 必须保持 `UNAVAILABLE`。
  - NOT VERIFIED：APB 地址/位宽、FPGA 去抖/CDC、OSD、RISC-V、真实特征、板级轮次、尺寸标定和机械臂均未验证。

- 日期：2026-07-12，来源 Agent：Codex（A5 CPU 四任务与逐轮事务 Host 验证）
  - 适用范围：不依赖摄像头、SoC、APB、OSD 或机械臂的 CPU 纯软件前置闭环。
  - 最新结论：新增 `competition_tasks` 固化四任务规则和理由码，新增 `round_controller` 固化一轮一事务、最终结论锁存、`event_seq/ACK`、超时、放弃和软复位。任务三只接受 2cm/3cm 参考且目标差为 1cm，任务四只接受 2cm/3cm 目标且差不超过 0.5cm。Host 新测试 `135/135` 通过，原 `task_matcher` 回归 `82/82` 通过。
  - 替代旧结论：替代“CPU 端仅有旧精确颜色/形状/尺寸匹配且无逐轮状态机”的完成度描述；不替代当前视频工程未集成 SoC/APB/CDC/OSD 的事实。
  - 证据路径：`final_project/docs/debug_sessions/a5_cpu_competition_rounds_host_20260712.md`、`final_project/docs/review_packets/a5_cpu_competition_rounds_review_packet_20260712.md`、`final_project/cpu/tests/test_competition_rounds.c`、`final_project/cpu/tests/run_competition_rounds_host.ps1`。
  - 下一步门禁：先冻结 `target_config`、`operator_event`、`result_status` 的寄存器无关语义和事件序号/ACK 契约；实际 APB 地址、FPGA 输入、CPU 到 OSD、`main.c` 集成及任何 `round_controller -> arm_controller` 映射须等待 SoC/接口安全门。
  - NOT VERIFIED：RISC-V 交叉构建、SoC/APB、CDC/OSD、真实特征快照、板级 20 轮和机械臂均未验证。测试使用既有 APB 占位宏仅为通过 `board_io.h` 的测试安全门，不代表正式 `soc.h` 已生成。

- 日期：2026-07-12，来源 Agent：Codex（A4 SoC/视频物理资源核查）
  - 适用范围：A 队员 CPU/APB 最小闭环从隔离 A3 副本进入正式视频工程前的 Interface Designer 资源门禁。
  - 最新结论：**A2 最小硬核 SoC 不可直接合并到当前视频工程。**视频 `mem_test.peri.xml` 已占用 `PLL_BL0`、`PLL_BL1`、`PLL_BL2`、`PLL_TR0`、`JTAG_USER1`；A2 请求 `PLL_BL0`、`PLL_TR0`、`JTAG_USER1`。官方硬 SoC IP 的 `PLL_SOC_SYS_RESOURCE` 仅允许 `PLL_BL0/PLL_BL1/PLL_BL2`，且三者均已占用；虽可把 JTAG 改为 `JTAG_USER2`、把外设 PLL 改至其它合法资源，但系统 PLL 无未占用候选。A2 还会创建与视频 `clk_25m`/`ddr_clk_ref` 重叠的 `GPIOT_P_50`/`GPIOL_25` 时钟输入 GPIO。不得直接拼接 `.peri.xml`、手改生成 RTL 或重复声明同一 pad。
  - 替代旧结论：替代“A3 完成资源审查后即可将 `REG_MAGIC` 接入视频顶层”的下一步假设；A3 隔离 map 仍有效，但工程级接入被本条门禁阻塞。
  - 证据路径：`final_project/docs/debug_sessions/a4_soc_video_resource_audit_20260712.md`、`final_project/docs/review_packets/a4_soc_video_resource_audit_review_packet_20260712.md`、`C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\A4_SOC_VIDEO_RESOURCE_AUDIT_20260712.md`、`C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\fpga\efinity\mem_test.peri.xml`、`C:\fpga_soc_isolated\tj375_soc_a2_20260712\a2_soc_generated.peri.xml`、`D:\Efinity\2025.2\ipm\ip\efx_hard_soc\ipm\ip_component.xml`。
  - 下一步门禁：须由 Efinity GUI / Interface Designer 以视频工程为基准，先审查是否允许重新规划视频 `PLL_BL*` 与 DDR/MIPI/视频时钟依赖；仅在该架构决定获批准后，才可用官方 IP Manager 重新生成 SoC（JTAG 应使用 `JTAG_USER2`）并对实际产物做资源交集检查。
  - NOT VERIFIED：GUI 重规划可行性、时钟/复位、UART0/JTAG 引脚、工程级 map/PNR、bitstream、RISC-V 固件、CPU Hello 与 APB 实读均未验证；未修改 RTL/约束/工程 XML，未烧录或上板。

- 日期：2026-07-12，来源 Agent：Codex（A3 隔离 APB REG_MAGIC 前置实施）
  - 适用范围：A 队员 CPU/APB 最小闭环的隔离工程验证；不适用于 C/D 主工程或烧录基线。
  - 最新结论：A3 已从 `D:\final_project\fpga` 建立独立副本，创建时 `top.v` 与 `mem_test.xml` 哈希一致。APB0 语义裁定为生成 BSP `IO_APB_SLAVE_0_INPUT=0xe8100000` 的 4 KiB 窗口，RTL 从机使用 `PADDR[11:0]`；隔离 `REG_MAGIC` 于偏移 `0x000` 返回 `0x375A0001`。生成 SoC 到 APB 从机的 Efinity map 退出码为 0，隔离 CPU `board_io.c` 在生成 `soc.h` 下预处理通过。
  - 替代旧结论：替代“APB0 32/12 位宽不一致而没有可执行地址处理方案”的阻塞描述；32 位主包装端口不再作为寄存器从机地址总线，窗口内偏移固定为低 12 位。
  - 证据路径：`final_project/docs/debug_sessions/a3_apb_magic_isolated_20260712.md`、`final_project/docs/review_packets/a3_apb_magic_isolated_review_packet_20260712.md`、`C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\A3_APB_MAGIC_IMPLEMENTATION_RECORD_20260712.md`、`C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\map_check\efx_map.log`。
  - 未完成边界：未合并 SoC `.peri.xml` 到视频工程、未改视频 `top.v`/`mem_test.xml`/约束、未完成行为仿真或 RISC-V 交叉构建、未 PNR/烧录/板测/CPU Hello/APB 实读；未进入 `LIVE_FG_AREA`、CDC、OSD、UART2 或机械臂。
  - 下一步门禁：先审查 A3 SoC `.peri.xml` 与视频 Interface Designer 配置的资源、时钟、复位、JTAG 和 UART0 引脚兼容性；通过后才能在 A3 顶层接入 `REG_MAGIC` 并进行工程级 map。
  - 失效条件：Interface Designer 合并显示 PLL/JTAG/SoC 资源冲突、真实视频时钟复位不兼容，或板级 CPU/APB 证据改变候选地址/接口。

- 日期：2026-07-12，来源 Agent：Codex（A2 隔离硬核 SoC 重新生成与最小 map）
  - 适用范围：A 队员 CPU/APB 前置生成验证；不适用于当前 C/D 视频工程构建树。
  - 最新结论：已在 `C:\fpga_soc_isolated\tj375_soc_a2_20260712` 生成 `TJ375N529` 最小硬核 SoC 配置、BSP 和外围 RTL。IP Manager 参数校验通过；官方后处理补齐后，Efinity `efx_map` 对隔离 wrapper/外围 RTL 退出码为 0。该隔离 BSP 的候选地址为 UART0 `0xe8010000`、APB0 `0xe8100000`，APB0 窗口为 4 KiB、频率为 200 MHz；尚未成为正式工程 ABI。
  - 替代旧结论：仅替代“完全没有可供审查的生成 SoC/soc.h 候选物”的前置缺口，不替代 `final_project` 当前“尚未集成 SoC、APB slave 或 results CDC”的事实。
  - 关键风险：生成 wrapper 出现 APB0 `PADDR` 的 `32 -> 12 -> 32` 位宽 warning，且默认仅 `PREADY=1`、`PRDATA=0`、`PSLVERROR` 未驱动；因此不能用于 `REG_MAGIC`、`LIVE_FG_AREA` 或任何正式寄存器访问。
  - 证据路径：`final_project/docs/debug_sessions/a2_isolated_soc_generation_20260712.md`、`final_project/docs/review_packets/a2_isolated_soc_generation_review_packet_20260712.md`、`C:\fpga_soc_isolated\tj375_soc_a2_20260712\A2_SOC_GENERATION_RECORD_20260712.md`、`C:\fpga_soc_isolated\tj375_soc_a2_20260712\map_check\efx_map.log`。
  - 未完成边界：未创建视频工程隔离副本，未修改 C/D 的 `top.v`/`mem_test.xml`/`.peri.xml`/约束，未实现 APB 从机或 CDC，未构建 CPU 程序、未验证 CPU 启动/UART、未 PNR/烧录/上板，也未涉及 UART2 或机械臂。
  - 下一步门禁：审查包必须先裁定 APB 地址位宽和正式 SoC 接入方法；通过后才可在视频工程隔离副本实施 CPU Hello + APB `REG_MAGIC` 最小闭环。
  - 失效条件：重新生成的参数/工具版本改变端口或 BSP 地址，或者受控视频副本产生了新的 SoC/APB 实测证据。

- 日期：2026-07-13，来源 Agent：Codex（两位队友分支本地整合）
  - 适用范围：CPU 四任务匹配、逐轮控制与观察—判定—ACK 事务层的 Host/Mock 基线；不适用于板级闭环或真实机械臂动作放行。
  - 最新结论：保留 `origin/dev/wsc6090-CPU` 的正式 `round_controller`，覆盖 APPLY/PLACE/REMOVE 事件、识别与判断锁存、执行或跳过、机械臂等待/故障、软复位和 20 轮 Mock；同时将 `origin/dev/libaoxun688` 的轻量观察事务控制器改名为 `competition_round_transaction`，供 `competition_contract` 保留目标评估、event_seq、ACK、超时与放弃语义，避免同名覆盖丢失任一实现。
  - 本次验证：MSVC C11 `/W4 /WX`（A13 仅关闭编译器特有 `C4132`）实际运行 `task_matcher 146/146`、`vision_classifier 31/31`、`competition_round_transaction 135/135`、`competition_contract 35/35`、`competition_host_flow 164/164`、`A13 snapshot replay 169/169`、`round_controller 115/115`，合计 `795/795`；Efinity `riscv-none-embed-gcc 8.3.0` 对 matcher、classifier、两层 controller、contract、adapter、tasks 与 `main.c` 共 8 个源文件以 `-Wall -Wextra -Werror` compile-only PASS。
  - 当前边界：两层代码尚未接入 `main.c`、正式 `soc.h`、APB/CDC/OSD 或板上 UART；本地整合后的 Host/Mock 与交叉编译结果必须以本次验证记录为准，分支历史测试不能外推为板级通过。
  - 证据路径：`final_project/cpu/app/include/round_controller.h`、`final_project/cpu/app/src/round_controller.c`、`final_project/cpu/app/include/competition_round_transaction.h`、`final_project/cpu/app/src/competition_round_transaction.c`、`final_project/cpu/tests/test_round_controller.c`、`final_project/cpu/tests/test_competition_rounds.c`、`final_project/docs/review_packets/team_integration_merge_review_20260713.md`。
  - 失效条件：整合回归失败、正式寄存器/事件契约改变接口，或后续主循环集成形成新的可复现实测证据。
- 日期：2026-07-12，来源：用户现场反馈 + 当日 PC 原始日志 / Codex 复核
  - 适用范围：myCobot 280 的 180° PC 点位诊断、机械固定边界与板上 F2 放行顺序
  - 最新结论：PC 端已定位到 `pick_hover` 姿态下 J4 约 `1.84°–2.20°` 的指令—读回末段静差；正向 J4 偏置可将内部正运动学残差降至约 `3.7–3.8 mm`，但 `sync_send_angles` 仍返回 `0`，故不得视为严格到位或下探许可。用户新确认机械臂本体与乐高底座的连接会在运动中微倾；该位移不被 `get_coords()` 的底座坐标系测量，因而成为真实抓放稳定性的首要未解风险。乐高多点加固可缓解但不等同刚性固定。
  - 当前放行：PC 端 180°真实抓放和任何板上真实动作维持 NO-GO；D1（CPU/APB）和 D2（UART2 本地回环/监听）可立即并行，均不得连接真实机械臂控制线或发送动作帧。D3 真实臂只读仍以 T0 的 `soc.h`/UART2 真源、电平、线序、共地、急停及机械结构复核为前置；D5 动作须完成 D0–D4 和 Codex Review Packet。
  - 替代旧结论：替代“PC 端多轮动作稳定即可作为 180°点位或板上动作放行依据”的隐含判断；不否定 PC 端对协议与点位顺序的开发期参考价值。
  - 证据路径：`mycobot_pc_tests/audit_logs/20260712_180deg_j4_mount_debug_record.md`、`mycobot_pc_tests/audit_logs/auto_run_20260712_164639.log`、`mycobot_pc_tests/audit_logs/auto_run_20260712_173543.log`、`mycobot_pc_tests/audit_logs/auto_run_20260712_191421.log`、`mycobot_pc_tests/audit_logs/auto_run_20260712_191657.log`、`mycobot_pc_tests/test_j4_bias.py`、`final_project/docs/technical_plans/mycobot_board_debug_execution_plan_20260712.md`。
  - 失效条件：完成外部基准下的乐高连接位移验收并获得新的两次以上悬停复测证据，或新的 SoC/UART/电平/实机日志改变 D1–D5 的事实边界。

- 日期：2026-07-12，来源：用户逐项进度访谈 + Codex共享仓库核查
  - 适用范围：7月17日保底冻结前的真实进度、人力、阻塞、最低保底和立即工作队列
  - 最新结论（2026-07-13 版本号更新）：决赛主方案已从 `v1.2-main` 更新为 `v1.3-main`，拿分顺序和 F1/F2 边界不变；新增团队整合进度与下一 Gate。最低保底定义为F1“至少单路真实摄像头 + FPGA同帧统计/LIVE_FG_AREA + 板上CPU四任务判断 + 拨码/按键目标锁存 + 可恢复逐轮状态机 + OSD明确结果 + 非目标正确SKIP + 20轮≤10分钟”；机械臂板控作为F2条件升级，不再拖死F1。
  - 实时事实：两只旧摄像头交叉任一接口均花屏，新摄像头预计7月13日到货；官方fallback纯色轮切稳定，ch1 I2C地址阶段ACK且`0x0100` bit0读高，但未见CSI DE。赛方独立CPU例程历史上板成功，但当前视频工程无SoC IP、生成`soc.h`、APB从机或结果CDC。目标输入和决赛OSD未实现。PC机械臂多轮动作稳定但旧路径约10cm，开发板UART/电平/接线未定；7月12日可重新示教180°点位并低速带载。五色/三形状/三尺寸物体齐全，底板/相机/机械臂/摆放区仍待固定，背景/补光未定。
  - CPU特别风险（历史盘点，已由上方 2026-07-13 CPU 整合条目部分替代）：`vision_classifier.c` 的白/黑仍依赖 `LIVE_FG_AREA` 真源；四任务 matcher 与 round_controller 已有 Host 代码，但 `main`、正式 APB/OSD 和 arm_controller 仍未闭环。
  - 并行工作（已由 2026-07-13 整合条目覆盖）：FPGA 合成数据源、CPU 竞赛契约与两层逐轮控制器已合入 `codex/team-integration-20260713`；后续 A 线转入生产/调试构建拆分与 periphery/PNR，B 线转入 `main`/APB/OSD 的 `ARM_DISABLED` 最小集成，C 线继续执行 T0、D1/D2 安全门。合成源仍不作为真实摄像头验收。
  - 截止线：7月14日晚必须出现视频工程内CPU Hello+APB MAGIC，否则人力集中救SoC/APB；7月15中午双摄未稳则降单摄；7月15晚无LIVE_FG_AREA则白色列高风险；7月16中午板到臂安全链未过则冻结F1无机械臂；7月16晚F1必须完成20轮计时，7月17冻结。
  - 替代旧结论：替代“方案仍待访谈定版”、把Host旧测试视为决赛CPU完成、把花屏简单归因于摄像头硬件、以及机械臂必须先完成才有保底的表述。
  - 证据路径：`final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md`、`final_project/docs/debug_sessions/video_link_current_state_20260711.md`、`final_project/cpu/app/src/vision_classifier.c`、`final_project/cpu/app/include/board_io.h`、`final_project/cpu/app/src/task_matcher.c`、`final_project/cpu/CPU_MODULE_PLAN.txt`。
  - 失效条件：新摄像头/仪器测量改变视频故障边界、合成源或SoC/APB代码合入并产生新上板证据、任务三评分获官方确认，或用户改变7月17冻结目标。

- 日期：2026-07-12，来源 Agent：Codex（Gemini方案复核后的总控计划落地）
  - 适用范围：分赛区决赛最大化得分攻关顺序与跨CPU/FPGA/SoC/OSD/myCobot验收门
  - 最新结论（2026-07-13 版本号更新）：`final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md` 当前为 `v1.3-main` 并继续作为用户标定的“决赛主方案”。方案采用“CPU得分引擎 + FPGA生产构建”双P0并行，先闭合识别/判断/OSD/正确SKIP，再用一条固定正方体抓放路径补齐7个目标轮执行分；逐轮控制器新增事件锁存/序号/ACK、有界超时、人工放弃、分级复位和机械臂运动态安全恢复，且已补充团队整合盘点、F1/F2保底层级与7月12—17日工作队列。
  - 替代旧结论：Gemini方案中四任务均按1目标+4非目标估分、统一按25%/25%/50%解释任务三、直接通过约束/隔离2288个I/O修PNR、把逐轮状态机直接写入`main.c`、把新点位写入`arm_controller.c`等不严谨表述；同时纠正“把5位APB总线升级为16/32位”的说法，当前只是软件假设的32位MMIO寄存器中的5位载荷草案。
  - 证据路径：`final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md`、`final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md`、`final_project/cpu/app/src/task_matcher.c`、`final_project/cpu/app/src/main.c`、`final_project/docs/technical_plans/fpga_vision_preprocess_implementation_handoff_20260711.md`。
  - 当前状态（已由 2026-07-13 整合条目覆盖）：主方案已批准不等于板级实现完成。RTL/CPU 源码和工程配置已有整合改动，Host 侧四任务、竞赛契约与两层逐轮控制器以及 RISC-V compile-only 已形成证据；真实 Debayer、正式 APB/CDC/OSD、PNR/bitstream、板级特征采集与机械臂动作仍未放行。
  - 下一步最小闭环（2026-07-13 更新）：统一 `register_map.md`、`board_io.h` 与竞赛契约；在 `ARM_DISABLED` 下把正式 `round_controller` 最小接入 `main.c`；拆分 production/debug 合成源入口并审查 periphery/IO 导出后重跑 PNR。不得重复把已完成的 Host 真值表和纯 C 控制器作为未开始任务。
  - 失效条件：用户否决该总控顺序、官方细则更新、任务三评分获得不同现场确认，或新的真实板级证据改变PNR/视频/SoC阻塞判断。

- 日期：2026-07-12，来源 Agent：Codex（Agent 入口一致性维护）
  - 适用范围：`AGENTS.md`、`CLAUDE.md`、`.claude/commands/*`、`.agents/skills/*` 与活跃实施文档的权威层级
  - 最新结论：活跃入口已统一为四层职责：最新官方细则定义比赛任务/评分/时限，`AGENTS.md` 定义稳定系统架构与安全硬边界，`CURRENT_STATE.md` 定义可变完成度/阻塞，`分赛区决赛实施开发路线.md` 只作历史路线图和经验库。FPGA/CPU 总体分工未改变，但已显式补入四任务关系判定、逐轮事务、OSD 结果语义、唯一机械臂响应和“目标不等于完成状态”规则。
  - 替代旧结论：`AGENTS.md`“当前最高层路线文件是分赛区决赛实施开发路线”、`.claude/commands/fpga-plan.md`“旧路线是当前高层路线源”、两项 Skill 以旧路线作为共同权威的表述。
  - 同步范围：`AGENTS.md`、`CLAUDE.md`、`.claude/commands/fpga-plan.md`、`fpga-exec.md`、`fpga-codex-review.md`、`fpga-handoff.md`、`.agents/skills/fpga_vision/SKILL.md`、`.agents/skills/cpu_mycobot/SKILL.md`、`final_project/README.md`、`final_project/docs/source_materials_digest/05_agent_quickstart.md`、`final_project/docs/mycobot_migration_plan.md` 及两份活跃技术计划。
  - 当前状态不变：此次只维护文档和 Agent 指令，未修改 RTL/CPU 源码，未补齐四任务输入/关系判定、CPU/APB/OSD、正式 UART 或机械臂实机闭环。
  - 失效条件：官方细则更新、架构边界经用户明确改变，或活跃入口再次出现把旧路线当作当前唯一权威的表述。

- 日期：2026-07-12，来源 Agent：Codex（依据 0710 最新官方 PDF）
  - 适用范围：分赛区决赛任务定义、评分语义、现场流程与全项目验收优先级
  - 最新结论：`final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md` 已注册为核心目标与约束。完整演示必须覆盖四任务×5轮并在 10 分钟内完成；每轮按识别25%、判断25%、执行50%串行计分，且必须明确输出识别、判断和执行/不执行理由。任务三是相对参考物边长差等于1cm，任务四是相对目标物边长差≤0.5cm，不能按“精确目标尺寸匹配”代替。目标颜色必须覆盖白、黑、红、蓝、黄。
  - 替代旧结论：`分赛区决赛实施开发路线.md` 中“横向搬运距离大于10cm”“三类分赛区任务”“现场每项通常放置5次”等旧/模糊口径；正式目标位置改以官方细则的“相对起点旋转180°（±10°）且最大臂展处”为准。
  - 证据路径：`赛方提供材料/第十届集创赛分赛区决赛“雄芯院”企业命题比赛细则-0710新.pdf`、`final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md`、`final_project/docs/competition_manual/细则对照项目优化建议_20260712.md`。
  - 当前阻塞：`task_matcher_read_target_from_fpga()` 的目标输入仍只有旧 2-bit 红/蓝/黄，缺少白/黑色硬件输入路径（Host 层 matcher 已支持五色，见 2026-07-12 CPU 条目）；`main.c` 尚未把匹配结果接入 `arm_controller`，仍写 `ARM_STATE_IDLE`；OSD/APB/CDC 与 20 轮端到端时限均无板级证据。
  - 下一步最小闭环：Host 层 matcher 四任务规则 + 一轮一事务锁已完成（Codex 合并后重跑 258/258）。剩余：板上五色目标注入路径（3-bit TARGET_SEL 或 UART 命令）、arm_controller 主循环集成、round 推进逻辑接入。
  - 失效条件：组委会/企业专家发布更新版本，或现场给出不同书面确认；发生时必须保留版本历史并新增覆盖条目，不得静默改写 0710 转写。

- 日期：2026-07-11，来源 Agent：Codex
  - 适用范围：`final_project` FPGA 视觉预处理第一阶段
  - 最新结论：可复用的 RGB888 2ppc ROI/统计特征 RTL 已完成。ch1 Debayer 输出已以只读旁路接入 `top.v`，6 个源文件已纳入 `mem_test.xml`；正式 map 通过，资源为 `EFX_ADD=2081`、`EFX_LUT4=11945`、`EFX_FF=10484`。顶层 `mark_debug` 已识别 11 组 `i_sysclk_div2` 域快照探针。FPGA 只提供 ROI 与统计特征，颜色/形状/尺寸决策、参数管理和 myCobot 控制仍归板上 CPU。
  - 替代旧结论：`fpga_vision_preprocess_execution_plan_20260711.md` 开头“尚未接入 `top.v` 或 Efinity 工程 XML”的第一阶段规划状态。
  - 证据路径：`final_project/fpga/rtl/top/top.v`、`final_project/fpga/efinity/mem_test.xml`、`final_project/docs/review_packets/preprocess_ch1_tap_review_packet_20260711.md`、`final_project/docs/technical_plans/fpga_vision_preprocess_implementation_handoff_20260711.md`。
  - 失效条件：真实 Debayer 波形证明当前 `vs/hs/de/valid` 或 48-bit RGB 字节序假设错误，或 D 盘 build/flash 树合并时发生必须调整的顶层冲突。
  - 未完成边界：未完成 Efinity Debugger capture、PNR、bitstream、烧录、真实帧验证、APB/CPU CDC、OSD 或第二通道；另一 D 盘 build/flash 树与当前正式工程的 `top.v`/`mem_test.xml` 仍有差异，未经审查不得覆盖同步。

- 日期：2026-07-11，来源 Agent：Codex
  - 适用范围：`final_project` FPGA 视觉预处理的 PNR/Debugger 验证路径
  - 最新结论（历史构建，已由 2026-07-13 正式整合构建覆盖）：C 盘 ASCII junction 上的正式 map 再次通过。项目 `mem_test.xml` 的 Debugger 自动实例化处于开启状态，命令行 PNR 自动带 `--enable_dbg`，要求 Debug Wizard 先生成 `mem_test.dbg.vdb`；当时普通 VDB 路径因 2,288 个未约束 I/O 在 Efinity 内部 `outpad` 断言失败。当前整合分支的最新计数为 1,776，仍未产生时序签核结果；两次结果共同指向 periphery/顶层 IO 导出边界问题，不得混作同一次构建统计。
  - 替代旧结论：将 `mark_debug` profile 视为可直接进行命令行 Debugger/PNR capture 的假设。
  - 证据路径：`final_project/fpga/efinity/mem_test.xml` 的 `debugger.auto_instantiation=on`、`final_project/docs/technical_plans/fpga_vision_preprocess_implementation_handoff_20260711.md`、`final_project/docs/review_packets/preprocess_ch1_tap_review_packet_20260711.md`。
  - 失效条件：在受控 Efinity GUI Debug Wizard 中生成含 ch1 预处理探针的 `.dbg.vdb` 并成功完成对应 PNR，或工程接口/约束完整性修复后普通 PNR 不再出现未约束 I/O 的 `outpad` 断言。
  - 未完成边界：不得伪造、手工复制或修改 `.dbg.vdb`；不得把 PNR 失败归咎于新增预处理 RTL。未生成 bitstream、未烧录、未进行板级或机械臂动作。

- 日期：2026-07-11，来源 Agent：用户批准 / Codex
  - 适用范围：FPGA 视觉预处理的系统交接阶段
  - 最新结论：用户批准暂缓四项证据门槛：独立 testbench 实跑、真实 Debayer 波形确认、PNR/时序与 bitstream 闭环、板级特征采集。暂缓期间只允许推进不改变数据路径的接口审计与契约文档，不接入 CPU/APB、OSD 或 ch0 正式 RTL。
  - 替代旧结论：将第 1-4 项视为立即执行的前置任务顺序。
  - 证据路径：`final_project/docs/architecture/generated_soc_summary_2026-07-11.md`、`final_project/integration/preprocess_apb_cdc_contract_draft_20260711.md`、`final_project/docs/technical_plans/fpga_vision_preprocess_implementation_handoff_20260711.md`。
  - 失效条件：用户要求恢复验证，或生成 `soc.h`、SoC 端口与 APB 时钟/复位信息后提交新的 CPU/APB Review Packet。
  - 未完成边界：`generated_soc_summary_2026-07-11.md` 是已存在的缺口报告；当前仓库不存在由 Efinity SoC 生成的 `soc.h`、APB slave 或 `results_cdc` RTL。`final_project/cpu/app/include/board_io.h` 及寄存器偏移仍为草案，CPU 测试构建的 `0xF0000000` 是占位值，不能用于正式硬件。

- 日期：2026-07-12，来源 Agent：Claude（四任务 Host 层实现 + Codex 审查）
  - 适用范围：板上 CPU 识别决策软件 — task_matcher 四任务契约 + main.c error_code 修复
  - 最新结论：
    - CPU 分支原始实现报告 254/254；Codex 按官方细则修正任务二尺寸语义并补同目标重复写入锁测试后，当前源码通过 **258/258** host 单测（classifier 31 + param_table 81 + task_matcher 146，MSVC 19.42 `/std:c11`，全部从当前源码重建）。
    - `task_matcher` 四任务契约已在 Host 层定版：
      - `task_mode_t`（MODE_1 指定颜色正方体 / MODE_2 混合形状池中的指定颜色正方体且尺寸通配 / MODE_3 相对参照物差值 10mm / MODE_4 相对目标物差值 ≤5mm）。
      - `round_state_t` 一轮一事务锁（IDLE → TARGET_LOCKED → GRAB_REQUESTED → next_round → IDLE）。GRAB 后自动锁定，防止连续帧重复触发。
      - 五色目标（WHITE/BLACK/RED/BLUE/YELLOW）在 matcher 内部正确区分，不与 COLOR_UNKNOWN 混淆。
      - `set_target_ex()` 自动锁定到 ROUND_TARGET_LOCKED，不依赖调用方填写内部事务状态。
    - `main.c` 的 error_code 清零 bug 已修复：新增 `latched_err` 跨迭代锁存，`commit_global` 失败后下轮重试提交，成功才清零。commit_results 失败在 commit_global 成功后不会被吞掉。
    - `main.c` 中五色目标注入点和 round 推进条件已用注释显式标注占位。
    - **未完成（不是"已闭环"）**：
      - 五色目标在 Host 层 matcher 可测，但板上入口仍只有旧 `task_matcher_read_target_from_fpga()`（2-bit color_sel，仅红/蓝/黄）。正式目标注入路径（3-bit TARGET_SEL 或 UART 命令）阻塞于 FPGA 硬件确认。
      - 一轮一事务锁在 matcher 层完成，但 `task_matcher_next_round()` 当前仅在测试中调用。主循环的 round 推进（arm_controller 完成信号 / 物理按键 / 超时）阻塞于 arm_controller 集成和板上 UART 驱动。
      - 以上两项不得在 CURRENT_STATE 或 Review Packet 中被描述为"已完成"或"已闭环"。
  - 替代旧结论：2026-07-11 CPU 条目中"维护未重跑测试""仅支持精确尺寸匹配""缺白/黑色"等旧口径；2026-07-12 比赛规则条目中"下一步最小闭环"的 matcher 部分已执行。
  - 证据路径：`final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md` §二.3.1、`final_project/cpu/app/include/task_matcher.h`、`final_project/cpu/app/src/task_matcher.c`、`final_project/cpu/app/src/main.c`、`final_project/cpu/tests/test_task_matcher.c`（146/146）、`final_project/cpu/CPU_MODULE_PLAN.txt`。
  - 失效条件：后续测试重跑失败、生成 SoC 的寄存器语义与当前草案不兼容，或 Codex 下一轮审查发现 Host 层逻辑缺陷。
  - 未完成边界（已由 2026-07-13 CPU 整合条目部分覆盖）：8 个关键源文件的 RISC-V strict compile-only 已通过，但仍未生成正式 `soc.h`，未完成正式链接/烧录、APB/CDC、OSD、bitstream 或板级验证；五色目标硬件输入路径（3-bit TARGET_SEL / UART cmd）未定版；`TARGET_SEL`、`LIVE_FG_AREA` 及其地址均未定版；arm_controller 未接入主循环。

- 日期：2026-07-11，来源 Agent：维护核查（基于 2026-07-09 myCobot CPU 迁移记录）
  - 适用范围：板上 CPU myCobot 协议与控制器
  - 最新结论：纯 C 的协议编解码、控制器状态机、超时后的复读与一次重试逻辑，以及对应 mock 测试骨架均已存在。PC 端 `pymycobot` 脚本仍只作开发期健康证明和指令序列参考，不进入正式闭环。
  - 替代旧结论：把 PC 端调试脚本视为正式识别/控制闭环组成部分的任何表述。
  - 证据路径：`final_project/cpu/app/src/mycobot_protocol.c`、`final_project/cpu/app/src/arm_controller.c`、`final_project/cpu/tests/test_mycobot_arm_skeleton.c`、`final_project/docs/technical_plans/priority3_mycobot_cpu_migration_design.md`。
  - 失效条件：协议版本、板上 UART 驱动或控制器接口经 Review Packet 定版后变化。
  - 未完成边界：未接入正式板上 UART/SoC 主循环，未确认 FPGA-to-机械臂电平与接线，未进行机械臂实机动作验证。

## 历史参考：PC 端 myCobot 联调

- 日期：2026-07-08，来源 Agent：Codex 复核 + Claude 据实测日志交叉核查
  - 适用范围：PC端防假熔断降级重试与故障诊断 (`mycobot_pc_tests/` V2.12)
  - 最新结论：Codex 只读复核裁定 V2.12 改动链：(A) V2.11 EOF→release 修复 Safe；(B) confirm=2 遇 `get_angles_once` 持续 None/瞬态跳变 Safe（V2.12 加 `none_count` 诊断不改 confirm 行为）；(C) 缺陷B sync res==0 直接熔断 Insufficient→已补 post-failure 复读+有界重试三段式判定。run-25(N=5)全流程软到位收敛、0 兜底、0 人工扶正、Happy Path 零副作用（V2.12 诊断/retry 仅挂 res!=1 退路）。**如实风险**：V2.12 retry 分支因 run-25 全程零兜底**未被运行时触发**，代码层面经 Codex 复核闭环，运行时正确性无独立背书；Priority-3 文档须标注"代码审查通过、运行时未触发"。
  - 替代旧结论：先前版本中阻塞兜底失败（固件 sync 假失败返回 0）会直接引发全局强熔断和进程终止的硬判决（V2.4 安全失败语义正确但诊断不充分）。
  - 证据路径：`mycobot_pc_tests/audit_logs/v2_codex_review_migrated_findings.md`、`mycobot_pc_tests/audit_logs/trial_run_25_logs.md`
  - 失效条件：V2.12 retry 分支在后续 run-N 命中并实测证实运行时正确性，或板上 CPU (C 侧) 移植时重试控制流被重新设计。

- 日期：2026-07-08，来源 Agent：Codex 复核 + Claude 据实测日志交叉核查
  - 适用范围：PC端异常处理与主流程回归测试 (`mycobot_pc_tests/` V2.11)
  - 最新结论：V2.11 交互式终端 N=5 回归 100% 成功抓取（0 扶正，末段平滑回零 0% 兜底降级）。Codex 裁定 V2.11 EOF→release 修复 Safe：`try...except (EOFError, KeyboardInterrupt) finally` 逻辑闭环，正常 Enter/EOF/Ctrl+C 三路都执行 `release_all_servos`，解决 run-23 管道 stdin 耗尽导致的舵机上电锁死发热风险。Codex 指出既有设计遗留（异常转"打印一行+release"、无 traceback、input 非 EOF 异常误称"人工扶稳"）属调试可观测性，不影响硬件安全，留板上 CPU 侧系统性解决。
  - 替代旧结论：V2.10/run-23 管道喂入运行时 stdin 耗尽导致 EOF 绕过 release、进程死亡、臂上电锁死悬空的缺陷。
  - 证据路径：`mycobot_pc_tests/audit_logs/v2_codex_review_migrated_findings.md`、`mycobot_pc_tests/audit_logs/trial_run_24_logs.md`
  - 失效条件：板上 CPU (C 侧) 移植中异常分类与日志/traceback 体系被重新设计。

- 日期：2026-07-08，来源 Agent：Codex 复核 + Claude 据实测日志交叉核查
  - 适用范围：PC端回零二次读数与偶发兜底分析 (`mycobot_pc_tests/` V2.10)
  - 最新结论：初始与末段平滑回零的 `confirm=2` 机制运作正常。run-23 第 5 轮 step9 空载抬升出现软超时后 `sync_send_angles` 返回 0 触发安全熔断——Codex 裁定根因优先级为"固件 sync 死区假失败"（历史参数承认上行 sync 在 ~2° 残差可返回 0），其次为"4s 窗口内 confirm=2 确认不足"，**串口溢出/线缆热态归因证据弱（日志未见串口异常文本）**。run-24 开机回零出现一次 `max_diff=44.12°` sync 兜底（confirm=2 未能压制单帧强瞬态跳变），印证 Codex (B) "confirm=2 不能根治强瞬态、仅降低概率"。对 Priority-3 的规范：板上 CPU 侧轮询节拍规范化（定时器 50-100ms 间隔）属工程规范应实现，但**不得标注为"修复 run-23 熔断根因"**；超时/熔断分级应实现 Codex (C) post-failure 复读+有界重试+最终熔断保守失败语义。
  - 替代旧结论：先前认为"兜底率偏高因单帧瞬态毛刺"单一假设、以及把 run-23 res=0 归因于"串口缓冲溢出/高频轮询过载"的叙事（后者经证据否决）。
  - 证据路径：`mycobot_pc_tests/audit_logs/v2_codex_review_migrated_findings.md`、`mycobot_pc_tests/audit_logs/trial_run_23_logs.md`
  - 失效条件：板上 CPU 侧通过硬件定时器中断实现高稳定低延迟通信，或后续 run-N 实测复核改写根因排序。

- 日期：2026-07-08，来源 Agent：Gemini (Antigravity)
  - 适用范围：PC端调试与稳定性测绘 (`mycobot_pc_tests/`)
  - 最新结论：通过二次读数确认机制（`ASYNC_SHORT_CONFIRM_COUNT = 2`），彻底解决了机械臂减速振荡中单帧下探造成的“假到位/提前判定”Bug。在 N=5 连续带载实测中，Step 5 (pick->pick_hover) 耗时稳定收敛于 `1.4s~1.6s`，下探抓取点重复空间位置偏差平均在 `5.88mm` 左右，放置点偏差在 `3.20mm` 左右，极度稳定；末段平滑回零成功通过安全超时与阻塞 sync 兜底机制，实现 5 轮全流程 0 人工扶正。
  - 替代旧结论：V2.8 中单次判定可能在重力晃动下产生的提前退出隐患，以及先前 Step 5 卡在 3.5s 固件死区。
  - 证据路径：`mycobot_pc_tests/audit_logs/trial_run_22_logs.md`
  - 失效条件：板上 CPU 侧发现相同的二次确认逻辑需要额外的时延调整，或用户提出更高精度要求。

## 待定决策

- 日期：2026-07-09，来源 Agent：Antigravity (Gemini 3.5 Flash)
  - 适用范围：待讨论议题（板上参数标定掉电保存方案）
  - 最新结论：当前由于裸机缺少 Flash 物理驱动，系统无法断电保存现场调好的分类阈值和尺寸标定表。待与团队讨论是选择“在 FPGA 侧追加 SPI Flash 物理控制器及裸机驱动”，还是“调试完成后人工记录数值、直接在 C 语言源码中修改后重新编译烧录”。
  - 替代了哪个旧结论：无（新增待讨论议题）。
  - 证据路径：`final_project/cpu/app/src/param_table.c`、`final_project/cpu/CPU_MODULE_PLAN.txt`。
  - 失效条件：团队讨论后定版并有明确的存储驱动实现或规程锁定。

- 日期：2026-07-09，来源 Agent：Antigravity (Gemini 3.5 Flash)
  - 适用范围：待讨论议题（拨码开关 TARGET_SEL 与 LIVE_FG_AREA 寄存器地址）
  - 最新结论：任务目标物理拨码开关寄存器 `TARGET_SEL` 的 APB 地址（建议占位 `0x06C`）及用于形状判定填充率的 `LIVE_FG_AREA` 寄存器（建议占位 `0x0B0/0x1B0`）需要与 FPGA 侧队友锁定，并根据实际拨码物理引脚同步配置消抖与消重逻辑。上述数值不是当前硬件事实。
  - 替代了哪个旧结论：无（新增待讨论议题）。
  - 证据路径：`final_project/integration/register_map.md`、`final_project/cpu/CPU_MODULE_PLAN.txt`。
  - 失效条件：FPGA 侧完成软核连线与寄存器锁定并更新接口文档。

## 已废弃 / 历史参考方案

- 纯 FPGA 视觉识别主线（废弃，不可恢复，除非经 Codex Gate + 用户明确指令）
- 纯 FPGA myCobot 控制主线（废弃，同上）
- PC 端 pymycobot 进入正式识别/控制闭环（仅保留开发期调试，见 `mycobot_pc_tests/` 归档说明）
- HDMI 双摄透传保底方案（历史基础链路/旧结论；除非用户重新指定，否则不作为当前核心攻关目标）
