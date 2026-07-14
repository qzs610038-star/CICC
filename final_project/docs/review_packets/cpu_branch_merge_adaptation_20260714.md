# 队友 CPU 分支合并适配说明（2026-07-14）

> 来源分支：`origin/dev/wsc6090-CPU@885e97a`
>
> 同步吸收：`origin/codex/fable5-remediation-20260713@758e864`
>
> 集成分支：`codex/cpu-fable-integration-20260714`
>
> 结论：CPU Host/Mock 与 compile-only 已达到 PR 审查标准；FPGA production map 通过，但 PNR 仍被 IO/periphery 边界阻塞，因此不构成 bitstream 或板级闭环。

## 1. 为什么不能直接原样合并

队友分支补上了有价值的安全门和测试，但它是在较早的 8-bit `event_seq` 口径上继续开发；Fable 整改分支已经把 `round_controller`、`competition_contract` 和 ACK 序号统一为 16 bit，并增加显式 `ACCEPTED/REJECTED` ACK、`65535 -> 0` 回绕与 1000 事件随机流。若直接选择任一侧，会分别丢掉队友的机械臂安全补测或主线已经收敛的 ABI。

本次按“保留安全意图、统一契约、重新验证”处理，而不是用 `ours/theirs` 覆盖一方。

## 2. 从队友分支保留的改动

| 队友增量 | 合并后的处理 | 保留理由 |
|---|---|---|
| `arm_enabled=1` 但 `arm_busy=1` 时禁止发起抓取 | 原样保留安全语义，并与 16-bit 事件 ACK 共存 | 忙碌机械臂不能接收第二个物理动作请求；否则可能出现重复分拣或状态错配 |
| 非运动状态 `ABANDON` 终止本轮并保留理由 | 保留实现与测试 | 评委轮次需要可解释的结束原因，不能静默丢轮 |
| 机械臂运动期屏蔽软复位和放弃 | 保留测试 | 物理运动中改变事务状态会造成软件认为结束、机械臂仍在运动的危险分裂 |
| 机械臂故障、超时证据 | 保留测试 | 超时与故障必须进入可解释终态，不能无限等待或自动重发动作 |
| `task_matcher` 五色、四任务和非法模式补测 | 保留并纳入 154/154 回归 | 对齐官方细则“二、实物演示环节比赛要求”中五色、四任务与每轮唯一响应要求 |

关键实现位于 `final_project/cpu/app/src/round_controller.c`，关键补测位于 `final_project/cpu/tests/test_round_controller.c` 与 `final_project/cpu/tests/test_task_matcher.c`。

## 3. 合并时做的适配及理由

### 3.1 把队友的 8-bit 序号适配到 16-bit 半范围契约

- `event_seq`、`last_event_seq`、ACK 序号继续使用 `uint16_t`。
- 新旧判断使用 `delta = (uint16_t)(seq - last)`：`1..32767` 为新事件，`0` 为重复，`32768..65535` 为歧义或旧事件。
- 明确接受 `65535 -> 0`，拒绝 `0 -> 65535` 和差值正好为 `32768` 的事件。
- 保留首次事件单独放行，避免把合法的序号 `0` 永久当作无效值。

理由：主线的 `competition_contract` 已采用同一 16-bit ABI；继续保留 8-bit 局部实现会在 `255 -> 0` 附近与另一层契约产生不同结论，也缩短了序号复用周期。

### 3.2 保留显式 ACK，而不是“旧事件不回应”

合法新事件会按当前状态返回 `ACCEPTED` 或 `REJECTED`；重复/过期事件不再次消费。这样上游能区分“事件已看到但当前状态不接受”和“没有新的 ACK”，同时确保单个物理动作不会因重放而重复触发。

### 3.3 修正 MSVC 严格编译下的标准 C 初始化

`vision_classifier.c` 的只读零结果由无初始化常量改为 `static const vision_result_t zero_result = {0};`。该修改消除 MSVC C4132，使 `/W4 /WX` 可作为 PR 编译门；所有字段仍为 0，不改变分类语义。

### 3.4 同步事实文档

- `CURRENT_STATE.md` 新增 2026-07-14 集成条目，覆盖源分支 8-bit 和旧断言计数口径。
- `final_project/cpu/CPU_MODULE_PLAN.txt` 更新为 16-bit ABI、5758/5758 Host 基线和 RISC-V compile-only 边界。
- 三轨学习指南记录 FPGA、CPU 与系统维护视角，方便队友拉取后快速复核。

## 4. 验证证据

### 4.1 CPU Host/Mock

| 测试 | 结果 |
|---|---:|
| vision classifier | 31/31 |
| param table | 81/81 |
| task matcher | 154/154 |
| round controller | 4979/4979 |
| competition rounds | 135/135 |
| competition contract | 45/45 |
| competition host flow | 164/164（20 轮，尺寸仍 deferred） |
| A13 snapshot replay | 169/169（20 轮，尺寸仍 deferred） |
| 合计 | **5758/5758** |

另运行 `test_mycobot_arm_skeleton` 的原生断言测试，编译与运行退出码为 0；它不连接或驱动真实机械臂，且没有可直接累加的断言计数，因此不纳入 5758。

### 4.2 RISC-V compile-only

Efinity RISC-V GCC 8.3.0 以 `rv32imac/ilp32 -Wall -Wextra -Werror -Wno-error=cpp` 编译通过：

- `round_controller.c`
- `vision_classifier.c`
- `task_matcher.c`

只放行 `board_io.h` 明示的 `soc.h`/APB 占位告警；占位基址不能用于正式固件或上板。

### 4.3 FPGA production/default 构建

- Efinity 2025.2 map：PASS。
- 资源：`EFX_ADD=2081`、`EFX_LUT4=11939`、`EFX_FF=10492`、`EFX_RAM10=250`、`EFX_DPRAM10=8`。
- `mem_test.warn.log`：137 行 warning，未标记为可忽略。
- PNR：FAIL。工具报告 `2288 IO cells will have random placement`，随后触发 `!available_io_sites.empty(): outpad` 内部断言。
- 结果边界：未生成可验收的 PNR/STA/bitstream 证据；不能宣称生产 FPGA 或板级闭环通过。

### 4.4 显式 debug 宏构建

`COMPETITION_DEBUG_SYNTHETIC` 通过 Efinity map 命令行显式定义，日志明确记录 `verilog-macros=COMPETITION_DEBUG_SYNTHETIC=1`：

- Efinity 2025.2 debug map：PASS。
- 资源：`EFX_ADD=1827`、`EFX_LUT4=10339`、`EFX_FF=7991`、`EFX_RAM10=154`。
- `mem_test.warn.log`：138 行，post-synthesis netlist 汇总 1098 warnings，不能标记为可忽略。

该构建只验证合成源入口可被显式打开，不能替代 production、真实摄像头或现场识别验收；本轮未继续对 debug 构建运行 PNR/bitstream。

## 5. 尚未完成与安全边界

- 正式视频工程仍没有已验收的 SoC/APB/CDC/OSD 闭环和可用 bitstream。
- UART2 J52 的板端 C14/F12 与 3.3V 文档真源已冻结，但机械臂端线序、电平、正式 SDC/periphery、D2 100/100 帧仍未通过。
- 本轮没有连接机械臂、发送控制帧或执行任何动作；`ARM_DISABLED` 必须保持到 T0/D2 和其余安全门关闭。
- 官方细则“二、实物演示环节比赛要求”规定每轮要输出识别、判断、执行/不执行理由，并要求机械臂给出明确唯一响应；Host 测试支持这些软件语义，但尚无现场 20 轮和 180°±10°最大臂展证据。

## 6. 请队友重点评价

1. 是否同意以后统一以 16-bit 半范围 `event_seq` 为唯一 ABI，并废弃源分支的 8-bit 局部口径。
2. `arm_busy` 时直接以 `ARM_NOT_READY` 完成本轮、且不发动作请求，是否符合现场操作预期；若要改为 WAIT，必须先补超时和唯一响应测试。
3. `ABANDON`、运动期锁定、故障与超时理由码是否需要与 OSD 文案建立单一映射表。
4. FPGA 队员应优先确认 periphery/Interface Designer 顶层 IO 导出边界，而不是为 2288 个普通 IO 盲补 SDC。
