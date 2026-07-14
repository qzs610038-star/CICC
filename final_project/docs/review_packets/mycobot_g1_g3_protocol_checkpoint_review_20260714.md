# myCobot G3 纯软件检查点审查包（含官方协议边界）

> 日期：2026-07-14
> 审查分支：`codex/mycobot-g0-g3-bringup-20260714`
> 基线提交：`d433bca`
> 审查性质：G1.5–G3.5 纯软件修复后的代码、构建和协议复核；未修改 FPGA，未烧录，未连接 J52/机械臂，未产生 UART2 或动作帧。
> 总判定：**PASS（G0–G3 纯软件、NOT_FOR_FLASH） / NO-GO（G4、正式烧录与真实机械臂）**。
> 并发工作区记录（已恢复）：2026-07-14 14:08:19 共享工作区曾被外部会话切到无关的 `codex/explore-log-return-20260714`；按用户明确指令，14:16 已切回 `codex/mycobot-g0-g3-bringup-20260714`。两分支均指向 `d433bca`，全部未提交文件保持完整，未执行清理或提交。

## 1. 执行结论

当前修复证明 `disabled` / `simulated` 两种后端能在 Host、严格 QEMU 和四种 RISC-V profile/backend 组合上通过。该结论只关闭 G0–G3 的软件门，并不提供进入 G4 的 SoC/PNR/部署证据。

分级状态如下：

| Gate | 当前状态 | 主要依据 |
|---|---|---|
| G0 基线与隔离 | PASS | 分支正确；Host 基线通过；测试临时目录已清理；未触碰硬件 |
| G1 构建隔离 | PASS（仅 G0–G3） | 规范 PowerShell 入口、Makefile 薄包装、manifest v2、严格 warning 门、`-BoardBuild` 早失败 |
| G2 disabled 主循环桥 | PASS（structural bridge only） | `main -> round_controller -> arm_runtime` 不直通机械臂；无受审事件/时基时不合成事务，也不把 legacy matcher 冒充逐轮结果 |
| G3 simulated + 目标侧证据 | PASS（纯软件） | Host、严格 QEMU（入口/超时注入）和 RISC-V 四组合 4/4 通过；ELF/static evidence 排除 UART2/real transport |
| G4 正式 SoC/PNR/部署 | NO-GO | 无正式 `soc.h`，linker/startup 仍为 provisional；无通过的正式制品链 |
| 烧录/J52/真实臂 | NO-GO | 本轮无任何板级、电气、UART2 或动作证据 |

因此，可以把 G3 标为“纯软件 PASS”，但不应将其外推为 G4 完成、允许 Efinity GUI 操作、烧录或接线。本包交付后按用户要求停止。

## 2. 本轮实际验证矩阵

| 验证项 | 新鲜结果 | 审查判定 |
|---|---|---|
| `run_arm_runtime_host.ps1` | disabled PASS；simulated PASS | PASS |
| `run_arm_runtime_qemu.ps1 -TimeoutSeconds 10` | disabled/simulated PASS；无限循环 timeout probe 在 1 秒被终止并 PASS | PASS：`ENTRY(_start)` 与 startup 一致；每步退出码和 unexpected warning 均受门控 |
| `competition + disabled` RISC-V link/ELF | BUILD PASS、ELF PASS、`NOT_FOR_FLASH` | PASS |
| `competition + simulated` RISC-V link/ELF | BUILD PASS、ELF PASS、`NOT_FOR_FLASH` | PASS；不代表正式事件闭环 |
| `arm_bringup + simulated` RISC-V link/ELF | BUILD PASS、ELF PASS、`NOT_FOR_FLASH` | PASS；仅 provisional 链接环境 |
| `arm_bringup + disabled` RISC-V link/ELF | 固定交错 20 轮零请求 self-check 保留 `round_controller_tick`；BUILD/ELF PASS | PASS |
| `deploy.bat` | 明确阻止部署并返回非零 | PASS（失败即安全） |
| `-BoardBuild` | 在输出目录创建前抛错；目标临时路径不存在 | PASS（G4 前 fail closed） |
| 正式 `soc.h` / bitstream / Programmer | 未运行，仓库内无正式 `soc.h` | NOT RUN / NO-GO |

`board_io.h` placeholder warning 只允许出现在 `NOT_FOR_FLASH` 构建中，并写入 manifest；其余 compiler/linker/QEMU warning 均失败。所有临时制品在系统临时目录下生成并清理。

## 3. 已关闭的 G1.5–G3.5 问题

### 已关闭：构建与 QEMU

1. `scratchpad.lds` 统一为 `ENTRY(_start)`；QEMU link 加 `--fatal-warnings`，已不允许默认入口回退。
2. `arm_bringup + disabled` 增加固定交错 20 轮零请求自检；required-symbol 门保持不变，4/4 矩阵实际通过。
3. QEMU 以 `ProcessStartInfo` 启动、有界等待、超时后 kill；独立无限循环 ELF 的 1 秒 timeout probe 已实际通过。
4. `-BoardBuild` 在任何目录或制品创建前直接失败。未来 G4 必须另建 board-policy verifier，当前 builder 不接受生成 SoC 输入。

### 已关闭：可追溯制品与 UART2 排除证据

1. shim、startup、每个 C 源和 linker 命令均单独检查退出码。
2. manifest v2 记录源码、startup、linker 与 ELF/map/hex/bin/listing 的路径和 SHA-256，记录完整 flags 和唯一允许的 placeholder warning；无 `soc.h`/bitstream 的事实明确标为 standalone。
3. PowerShell 已是唯一 source/profile truth；Makefile 只能调用该脚本，不能走 BoardBuild。
4. ELF verifier 改为精确 `nm --format=posix` 符号检查，并校验 manifest hashes、源码/flags、map 和 listing 中不存在 `UART2` / `mycobot_*transport` 等禁止证据。

### 已关闭：G2/G3 语义边界

1. `main.c` 当前没有经过验证的 `event_valid` / `observation_valid` 输入，这一点是正确的安全克制，但意味着 G2 只能称“结构桥接”，不能称“逐轮闭环”。
2. `competition_now_ms++` 已删除；无受审的单调硬件时基时，主循环只传递固定零时间而不声称 timeout 行为。Host/QEMU self-check 仍使用显式 test clock。
3. legacy matcher 的非 NONE 输出现在写为 `LEGACY_MATCH_NO_ROUND`；只有 round controller 的有效结果才能进入 per-round match action，且无事件源时不会出现该结果。
4. `arm_bringup` 的 `NOT_FOR_FLASH` banner 仍只是入口标签，不是制品身份；制品身份、build ID 和 hash 以 manifest v2 为准。G4 以后仍必须由 board build metadata 替换该标签。
5. Host 与 bring-up self-check 均采用固定交错 7 target / 13 skip；disabled 20 轮保持 accepted/rejected request 为 0。

## 4. 官方协议对方案的调整

Elephant Robotics 官方页面确认应用层采用 `1000000 bps, 8N1`，帧为 `FE FE LEN CMD ... FA`，`LEN=payload+2`；同时明确直接协议通信前 Basic 端应运行 `transponder`、Atom 端应运行最新 `atomMain`。官方页描述的是 USB Type-C 通信，**不能**据此推导 TJ375 J52 的线序、电平或可直连结论。

对后续 G7–G11 的强制调整：

1. 只实现命令白名单与命令级精确长度；官方页给出的通用 `LEN` 窗口为 `0x02..0x10`，但不能用通用窗口替代逐命令长度。
2. 查询类命令采用 single-flight，保存 expected command/length；官方称有返回值命令应在 500 ms 内响应，方案首版 deadline 保持 750 ms，并记录实际分布后再收紧。
3. `SEND_ANGLES(0x22)`、`SET_GRIPPER_STATE(0x66)`、`SET_GRIPPER_VALUE(0x67)`、`STOP(0x29)`均无返回值；TX 成功只能说明字节发出，不能写成动作/停止/夹持完成。
4. 动作到位依靠 `GET_ANGLES(0x20)` 连续回读；停止用 `IS_MOVING(0x2B)+GET_ANGLES(0x20)` 辅证；夹爪用 `IS_GRIPPER_MOVING(0x69)+GET_GRIPPER_VALUE(0x65)` 辅证。
5. REAL 点位必须逐关节检查官方范围：J1 `[-168,168]`、J2 `[-135,135]`、J3 `[-150,150]`、J4 `[-145,145]`、J5 `[-165,165]`、J6 `[-180,180]` 度，并叠加项目保守裕量。
6. 官方页面存在字段下标、负数阈值等内部歧义；实现以白名单、官方示例、标准二补码、本地 PC 抓包和真实只读抓包交叉确认，歧义未关闭时只缩小功能范围。

这些协议事项不要求现在把真实 UART2 代码加入 G1–G3；相反，当前 simulated ELF 必须继续排除真实 transport。

## 5. G1.5–G3.5 关闭记录

### G1.5：构建与测试门修复

由 Codex 完成，无需硬件：

1. 开始修改前确认当前仍在 `codex/mycobot-g0-g3-bringup-20260714`，并保留现有 dirty/untracked 文件。
2. 修复 QEMU `_start`/linker entry、真实超时和逐命令退出码检查；unexpected warning 失败。
3. 让 `arm_bringup + disabled` 运行 20 轮零动作自检，使四种组合都能通过同一合理的 required-symbol gate。
4. G4 前禁止 `-BoardBuild` 产生正式制品；拆分 standalone verifier 与未来 board verifier。
5. 选定唯一 source/profile truth，补 manifest 源清单、flags、路径与 SHA-256。

### G2.5：明确正式主循环的安全语义

由 Codex 完成，无需硬件：

1. 保留“无受审事件源就不合成事件”的边界，并在文档/日志中把 G2 标成 structural bridge。
2. 禁止 simulated competition 的 legacy matcher 输出冒充逐轮机械臂事务；统一 `ARM_NOT_READY` / SKIP / FAULT 的 reason 与 OSD/APB 映射。
3. 为未来时基定义接口；在 G4 提供正式 CLINT/mtime 真源前，Host/QEMU 使用显式 test clock。

### G3.5：重跑完整矩阵并形成检查点证据

由 Codex 完成，无需硬件：

1. 重跑既有 Host 基线、disabled/simulated runtime、严格 QEMU、四种 RISC-V profile/backend 和 fail-closed deploy。
2. 要求 4/4 RISC-V 组合 PASS、零 unexpected warning、QEMU 超时注入测试 PASS。
3. 对 simulated ELF 保存 map、精确符号列表、source manifest 和 forbidden UART2/myCobot transport 证明。
4. 已交付本 G3 Review Packet；按用户要求立即停止，不自动请求或开始 G4。

### G4 以后用户需要承担的工作

用户需要学习并现场辅助：Efinity/Interface Designer 生成物关系、Programmer/JTAG 恢复、UART0 日志采集、逻辑分析仪基础、J52 电气/线序复核、Basic/Atom 固件模式、急停/断电和机械安全。用户负责一切 GUI、供电、烧录、接线、测量和真实动作；Codex 负责生成命令、审查制品/日志、判断 Gate 和实现受控固件。

## 6. G3 通过结论与下一阶段边界

以下条件均已满足，因此 G3 标为纯软件 PASS：

- Host 基线与 runtime 全通过；
- QEMU 使用真实入口、真实超时，disabled/simulated 均通过且零 unexpected warning；
- RISC-V 四组合 4/4 通过；
- `arm_bringup + simulated` ELF 含 round/runtime/controller/sim transport，排除 UART2/real transport；
- manifest 可追溯且所有产物仍明确 `NOT_FOR_FLASH`；
- 工作区无测试临时产物。

即使 G3 PASS，仍然只是“可由用户决定是否开始准备 G4 的正式 SoC/PNR/部署链”，不是“现在可以烧录”，更不是“可以接机械臂”。

## 7. 可复现命令与残余 NO-GO

本次使用临时输出目录、完成后删除：

```powershell
final_project\cpu\tests\run_arm_runtime_host.ps1
final_project\cpu\tests\run_arm_runtime_qemu.ps1 -TimeoutSeconds 10
final_project\cpu\build_tools\build_arm_profile.ps1 -Profile <competition|arm_bringup> -Backend <disabled|simulated> -OutputRoot <temp>
final_project\cpu\build_tools\build_arm_profile.ps1 -Profile competition -Backend disabled -BoardBuild -OutputRoot <empty-temp>
```

最后一条预期非零，且 `<empty-temp>` 不得被创建。四个正常组合均输出 `ELF PASS ... entry=_start not_for_flash=True uart2=excluded`；QEMU 额外输出 `QEMU TIMEOUT PASS seconds=1`。最终复验已确认 `git diff --check`、测试制品残留检查及 fail-closed `deploy.bat` 均 PASS。

残余 NO-GO：仓库没有正式 `soc.h`、同批 linker/startup、bitstream、PNR/STA、JTAG/Programmer、UART0 板级日志或 J52 电气证据；所以不存在可烧录的 G4 制品，也没有连接、只读或动作许可。
