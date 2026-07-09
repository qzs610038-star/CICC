# CURRENT_STATE — 最新路线增量与当前状态索引

> 本文件记录对历史规划文件的最新覆盖项与当前阶段状态。优先级见 `AGENTS.md`「文档优先级与交接规则」。
> 真实源码、工程 XML、构建日志和上板现象始终是最终事实来源；本文件只记录对它们的最新结论和证据位置。
> 安全红线（机械臂动作、Codex 审查门、决赛主线边界、Git 规范）不可被本文件覆盖。

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
- 证据路径：`AGENTS.md`「分赛区决赛主线」、`分赛区决赛实施开发路线.md`、`debug_records/camera_hdmi_handoff_2026-07-06.md`、用户 2026-07-07 批注。
- 失效条件：用户明确恢复 HDMI 双摄透传为当前攻关目标，或新的 `CURRENT_STATE.md` 状态条目给出更高优先级结论。

## 路线覆盖项

- 日期：2026-07-08，来源 Agent：Codex 复核 + Claude 据实测日志交叉核查
  - 适用范围：PC端防假熔断降级重试与故障诊断 (`mycobot_pc_tests/` V2.12)
  - 最新结论：Codex 只读复核裁定 V2.12 改动链：(A) V2.11 EOF→release 修复 Safe；(B) confirm=2 遇 `get_angles_once` 持续 None/瞬态跳变 Safe（V2.12 加 `none_count` 诊断不改 confirm 行为）；(C) 缺陷B sync res==0 直接熔断 Insufficient→已补 post-failure 复读+有界重试三段式判定。run-25(N=5)全流程软到位收敛、0 兜底、0 人工扶正、Happy Path 零副作用（V2.12 诊断/retry 仅挂 res!=1 退路）。**如实风险**：V2.12 retry 分支因 run-25 全程零兜底**未被运行时触发**，代码层面经 Codex 复核闭环，运行时正确性无独立背书；Priority-3 文档须标注"代码审查通过、运行时未触发"。
  - 替代旧结论：先前版本中阻塞兜底失败（固件 sync 假失败返回 0）会直接引发全局强熔断和进程终止的硬判决（V2.4 安全失败语义正确但诊断不充分）。
  - 证据路径：`mycobot_pc_tests/audit_logs/v2_codex_review_migrated_findings.md`、`trial_run_25_logs.md`、`auto_run_20260708_175657.log`
  - 失效条件：V2.12 retry 分支在后续 run-N 命中并实测证实运行时正确性，或板上 CPU (C 侧) 移植时重试控制流被重新设计。

- 日期：2026-07-08，来源 Agent：Codex 复核 + Claude 据实测日志交叉核查
  - 适用范围：PC端异常处理与主流程回归测试 (`mycobot_pc_tests/` V2.11)
  - 最新结论：V2.11 交互式终端 N=5 回归 100% 成功抓取（0 扶正，末段平滑回零 0% 兜底降级）。Codex 裁定 V2.11 EOF→release 修复 Safe：`try...except (EOFError, KeyboardInterrupt) finally` 逻辑闭环，正常 Enter/EOF/Ctrl+C 三路都执行 `release_all_servos`，解决 run-23 管道 stdin 耗尽导致的舵机上电锁死发热风险。Codex 指出既有设计遗留（异常转"打印一行+release"、无 traceback、input 非 EOF 异常误称"人工扶稳"）属调试可观测性，不影响硬件安全，留板上 CPU 侧系统性解决。
  - 替代旧结论：V2.10/run-23 管道喂入运行时 stdin 耗尽导致 EOF 绕过 release、进程死亡、臂上电锁死悬空的缺陷。
  - 证据路径：`mycobot_pc_tests/audit_logs/v2_codex_review_migrated_findings.md`、`trial_run_24_logs.md`
  - 失效条件：板上 CPU (C 侧) 移植中异常分类与日志/traceback 体系被重新设计。

- 日期：2026-07-08，来源 Agent：Codex 复核 + Claude 据实测日志交叉核查
  - 适用范围：PC端回零二次读数与偶发兜底分析 (`mycobot_pc_tests/` V2.10)
  - 最新结论：初始与末段平滑回零的 `confirm=2` 机制运作正常。run-23 第 5 轮 step9 空载抬升出现软超时后 `sync_send_angles` 返回 0 触发安全熔断——Codex 裁定根因优先级为"固件 sync 死区假失败"（历史参数承认上行 sync 在 ~2° 残差可返回 0），其次为"4s 窗口内 confirm=2 确认不足"，**串口溢出/线缆热态归因证据弱（日志未见串口异常文本）**。run-24 开机回零出现一次 `max_diff=44.12°` sync 兜底（confirm=2 未能压制单帧强瞬态跳变），印证 Codex (B) "confirm=2 不能根治强瞬态、仅降低概率"。对 Priority-3 的规范：板上 CPU 侧轮询节拍规范化（定时器 50-100ms 间隔）属工程规范应实现，但**不得标注为"修复 run-23 熔断根因"**；超时/熔断分级应实现 Codex (C) post-failure 复读+有界重试+最终熔断保守失败语义。
  - 替代旧结论：先前认为"兜底率偏高因单帧瞬态毛刺"单一假设、以及把 run-23 res=0 归因于"串口缓冲溢出/高频轮询过载"的叙事（后者经证据否决）。
  - 证据路径：`mycobot_pc_tests/audit_logs/v2_codex_review_migrated_findings.md`、`trial_run_23_logs.md`、`auto_run_20260708_172705.log`(run-24 L37-L38 44° 兜底)
  - 失效条件：板上 CPU 侧通过硬件定时器中断实现高稳定低延迟通信，或后续 run-N 实测复核改写根因排序。

- 日期：2026-07-08，来源 Agent：Gemini (Antigravity)
  - 适用范围：PC端调试与稳定性测绘 (`mycobot_pc_tests/`)
  - 最新结论：通过二次读数确认机制（`ASYNC_SHORT_CONFIRM_COUNT = 2`），彻底解决了机械臂减速振荡中单帧下探造成的“假到位/提前判定”Bug。在 N=5 连续带载实测中，Step 5 (pick->pick_hover) 耗时稳定收敛于 `1.4s~1.6s`，下探抓取点重复空间位置偏差平均在 `5.88mm` 左右，放置点偏差在 `3.20mm` 左右，极度稳定；末段平滑回零成功通过安全超时与阻塞 sync 兜底机制，实现 5 轮全流程 0 人工扶正。
  - 替代旧结论：V2.8 中单次判定可能在重力晃动下产生的提前退出隐患，以及先前 Step 5 卡在 3.5s 固件死区。
  - 证据路径：`mycobot_pc_tests/audit_logs/trial_run_22_logs.md`
  - 失效条件：板上 CPU 侧发现相同的二次确认逻辑需要额外的时延调整，或用户提出更高精度要求。

- 日期：2026-07-09，来源 Agent：Antigravity (Gemini 3.5 Flash)
  - 适用范围：待讨论议题（板上参数标定掉电保存方案）
  - 最新结论：当前由于裸机缺少 Flash 物理驱动，系统无法断电保存现场调好的分类阈值和尺寸标定表。待与团队讨论是选择“在 FPGA 侧追加 SPI Flash 物理控制器及裸机驱动”，还是“调试完成后人工记录数值、直接在 C 语言源码中修改后重新编译烧录”。
  - 替代了哪个旧结论：无（新增待讨论议题）。
  - 证据路径：`final_project/cpu/app/src/param_table.c`、`cpu_target_vs_current.md`。
  - 失效条件：团队讨论后定版并有明确的存储驱动实现或规程锁定。

- 日期：2026-07-09，来源 Agent：Antigravity (Gemini 3.5 Flash)
  - 适用范围：待讨论议题（拨码开关 TARGET_SEL 与 LIVE_FG_AREA 寄存器地址）
  - 最新结论：任务目标物理拨码开关寄存器 `TARGET_SEL` 的 APB 地址（暂定 0x06C）及用于形状判定填充率的 `LIVE_FG_AREA` 寄存器（暂定 0x0B0/0x1B0）需要与 FPGA 侧队友锁定，并根据实际拨码物理引脚同步配置消抖与消重逻辑。
  - 替代了哪个旧结论：无（新增待讨论议题）。
  - 证据路径：`final_project/integration/register_map.md`、`CPU_MODULE_PLAN.txt`。
  - 失效条件：FPGA 侧完成软核连线与寄存器锁定并更新接口文档。

（随路线调整追加，每条按六字段填写。例：）

- 日期：YYYY-MM-DD，来源 Agent：Claude
- 适用范围：颜色分类阈值管理
- 最新结论：阈值表放到板上 CPU 可调参数区，FPGA 只出 ROI 统计特征
- 替代旧结论：初赛 demo 的纯 FPGA HSV 阈值路线
- 证据路径：`final_project/integration/register_map.md`
- 失效条件：评委现场要求改回 FPGA 硬件阈值（需经 Codex Gate）

## 已废弃 / 历史参考方案

- 纯 FPGA 视觉识别主线（废弃，不可恢复，除非经 Codex Gate + 用户明确指令）
- 纯 FPGA myCobot 控制主线（废弃，同上）
- PC 端 pymycobot 进入正式识别/控制闭环（仅保留开发期调试，见 `mycobot_pc_tests/` 归档说明）
- HDMI 双摄透传保底方案（历史基础链路/旧结论；除非用户重新指定，否则不作为当前核心攻关目标）
