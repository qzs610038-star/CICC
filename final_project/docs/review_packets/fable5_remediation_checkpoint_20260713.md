# Fable 5 架构审查整改 Checkpoint（2026-07-13，Codex）

## 1. 结论

本轮已完成两项不依赖板卡、Efinity GUI 或机械臂动作的 P0 整改：

1. `round_controller` 的外部事件序号统一为 16 bit，并为新事件增加 `ACCEPTED/REJECTED` 显式 ACK 状态；完整控制器与轻量 `competition_contract` 统一使用半范围序号规则，重复或过期序号不重复消费，`0xFFFF -> 0x0000` 正常回绕。
2. FPGA 顶层改为 production 安全默认：未定义 `COMPETITION_DEBUG_SYNTHETIC` 时，两组合成源选择均为 `0`；只有显式 debug 宏才启用合成预处理和 HDMI 验证入口。

本轮没有修改 `main.c`、`mem_test.xml`、`.peri.xml` 或 `constrain.sdc`，没有运行机械臂动作，也没有宣称 APB、OSD、PNR、bitstream 或板级闭环完成。

## 2. 已修改文件

| 文件 | 修改 | 边界 |
|---|---|---|
| `cpu/app/include/round_controller.h` | `event_seq/event_ack_seq/last_event_seq` 改为 `uint16_t`；新增 ACK 状态枚举和输出字段 | 寄存器无关纯 C 契约 |
| `cpu/app/src/round_controller.c` | 增加半范围序号新旧判定、按当前状态接受事件、非法新事件显式拒绝 | 被拒绝的新事件仍被消费并 ACK，避免发送端永久重放 |
| `cpu/tests/test_round_controller.c` | 新增越界状态、过期序号、回绕、1000 事件随机流与恢复检查 | `ARM_DISABLED` 随机流中必须保持零动作请求 |
| `cpu/app/include/competition_contract.h`、`src/competition_contract.c` | 增加首事件标志，移除“序号 0 永久非法”特例 | 首事件可为任意 16-bit 值，回绕后 0 可正常消费 |
| `cpu/tests/test_competition_contract.c` | 增加 `0xFFFF -> 0 -> 1` 与回绕后陈旧事件测试 | 与 `round_controller` 序号语义一致 |
| `cpu/tests/run_round_controller_host.ps1` | 新增可复现 MSVC Host 运行入口 | 使用 APB 占位宏，仅用于 Host 测试 |
| `cpu/app/include/board_io.h` | 使占位地址警告同时兼容 MSVC 与 GCC | 无寄存器或运行行为变化 |
| `fpga/rtl/top/top.v` | production 默认真实输入；debug 宏显式启用合成入口 | 未生成或验证新的 Efinity 构建产物 |

## 3. 验证结果

### 3.1 round_controller

命令：

```powershell
& final_project/cpu/tests/run_round_controller_host.ps1
```

结果：`4919/4919 passed`，包含：

- 非法状态事件 `REJECTED` 且不迁移状态；
- 重复和过期事件无第二次 ACK/消费；
- 16-bit `0xFFFF -> 0x0000` 回绕；
- 20 轮 SKIP 无死锁、无动作请求；
- 1000 个确定性随机事件下状态始终有效、`ARM_DISABLED` 始终不请求动作，最后可由新序号 `SESSION_RESET` 回到 `CONFIG`。

同一 `round_controller.c` 已用 Efinity RISC-V GCC 8.3.0 执行 `rv32imac/ilp32 -Wall -Wextra -Werror` compile-only，通过。该结果不是正式链接、烧录或板上运行证据。

### 3.2 既有 CPU 回归

当前机器 `PATH` 无通用 `gcc`，仓库四个旧 Host runner 不能直接启动；使用 MSVC 19.42 从当前源码重新构建相同测试集合，结果：

- `competition_rounds`: `135/135 passed`；
- `competition_contract`: `45/45 passed`（新增回绕用例先以 `42/45` 失败，修复后全通过）；
- `competition_host_flow`: `164/164 passed`；
- `A13 FPGA snapshot replay`: `169/169 passed`。

### 3.3 FPGA 静态边界

静态核查确认 `top.v` 只有在 `COMPETITION_DEBUG_SYNTHETIC` 分支内把两选择器置 `1`，默认 `else` 分支均为 `0`；`git diff --check` 通过。遵守高置信交接规则，本轮未重复执行完整 Efinity map/PNR。production/debug 两种 Efinity 构建及其日志仍待 FPGA 队员完成。

## 4. 为什么本轮没有接入 main.c

Fable 5 建议在 `ARM_DISABLED` 下让 `main.c` 只通过 `round_controller` 推进轮次，方向正确；但当前更高优先级的 `CURRENT_STATE.md` 仍保留以下门禁：

- 正式视频工程没有同次生成的 `soc.h`、APB slave、事件锁存/ACK 和 CPU-to-OSD 提交接口；
- `register_map.md` 明确仍是候选契约，五色目标、任务模式、事件和结果原子提交槽位尚未冻结；
- A14/A7/A6 条目要求在 SoC/接口门关闭前不得把 Host 适配直接接入板上 `main.c`。

因此，本轮不虚构 MMIO 地址、按钮事件源或 OSD ACK。安全的后续接入顺序是：

```text
同次生成 soc.h + APB 时钟/复位证据
-> 冻结 TARGET_CFG / OPERATOR_EVENT / EVENT_ACK / RESULT_COMMIT
-> Host 测试 board_io 解码与 ACK
-> main.c 接 round_controller，强制 ARM_DISABLED
-> 固定结果 OSD
-> 真快照与 20 轮板上验证
```

这也符合官方细则“每轮摆放、识别、判断、执行/不执行及准确理由”的输出要求，以及四任务总时限 10 分钟的约束（比赛细则 §二第 3、4 项）。

## 5. 留给用户或队友的后续动作

### FPGA/SoC 队员

1. 在 Interface Designer 中审查 periphery 导出边界、目标器件和 SoC PLL 合法候选；不得手改 `.peri.xml` 或生成型 `constrain.sdc`。
2. 分别构建并留证：
   - production：不定义 `COMPETITION_DEBUG_SYNTHETIC`；
   - debug：显式定义该宏。
3. production 必须继续完成 PNR/时序并确认不再出现 1776 IO/outpad 阻塞；debug 结果不得作为真实摄像头验收。
4. 生成同一次 SoC 产物的 `soc.h`，提供 APB 基址、时钟、复位和接口生成日志。

### FPGA/CPU 接口联合审查

1. 冻结五色/四任务 `TARGET_CFG`、16-bit `OPERATOR_EVENT.event_seq`、`EVENT_ACK(status,seq)` 与原子 `RESULT_COMMIT`。
2. 白/黑统计必须是同一 `frame_id` 快照中的 `{status,bbox,fg_area,roi_pixel_count,sum_y}`；不得用 `LIVE_FG_AREA` 或 `sum_y` 任一单字段替代完整一致性契约。
3. 多位结果使用 staging + commit/sequence + active snapshot，不逐位裸跨 CDC。

### 用户/现场安全项

机械臂继续保持禁用。只有 `io_pin_map.md` T0 全 PASS、1 Mbps UART 回环/无运动只读验证通过，且 `round_controller -> arm_controller` 成为唯一动作请求路径后，才另开动作审查门。

## 6. 当前可直接执行的下一步

等待外部接口证据期间，CPU 侧可继续补 `round_controller` 从每个非运动状态的合法/非法复位矩阵，以及为最终 `board_io` 事件解码准备纯 Host 测试夹具；不提前写死候选 APB 地址。
