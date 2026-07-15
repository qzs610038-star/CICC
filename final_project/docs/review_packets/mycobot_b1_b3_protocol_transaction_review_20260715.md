# myCobot B1–B3 软件准备 Review Packet

> 日期：2026-07-15
> 范围：阶段 B1–B3 的纯软件补强；不涉及 UART2 MMIO、FPGA、J52、烧录或机械臂动作。
> 结论：**B1–B3 软件子检查点 PASS；G4–G11 仍未验证。**

## 变更与边界

- 新增 `cpu/app/include,mycobot_transaction.h` 与 `cpu/app/src/mycobot_transaction.c`：750 ms 有界、single-flight 的查询事务层；它只消费已解析帧，未接入任何 UART 驱动。
- `mycobot_protocol.*` 增加官方 `LEN=0x02..0x10` 窗口、`STOP(0x29)`、`IS_MOVING(0x2B)`、`IS_GRIPPER_MOVING(0x69)`、命令级精确响应长度和 J1..J6 绝对限位。
- `SEND_ANGLES` 编码在任一关节越出官方范围时拒绝，不再静默饱和。
- `test_mycobot_arm_skeleton.c` 覆盖 GET_ANGLES 精确请求帧 `FE FE 02 20 FA`、响应 `LEN=0x0E`、通用 LEN 边界、六轴上下界、错误命令/长度/域、超时、迟到/重复与 single-flight。
- B2 已用现有 memory-only `arm_sim_transport` 完成差分补测：可配置 N-poll 收敛、DONE 后下一事务、retry-failure/cancel 的 fail-closed，以及重新初始化后恢复 happy path。未为凑枚举新增重复 transport 分支。

此实现没有真实 UART2 backend、寄存器地址、F12/C14 访问、动作/夹爪 API 或 firmware flash 路径；故不得把它解释为 G7 回环或 G8 只读通过。

## 新鲜验证

| 命令 | 退出码 | 结果 |
|---|---:|---|
| `powershell -ExecutionPolicy Bypass -File final_project\\cpu\\tests\\run_mycobot_arm_skeleton_host.ps1` | 0 | Efinity RISC-V QEMU 完整断言执行 PASS |
| `powershell -ExecutionPolicy Bypass -File final_project\\cpu\\tests\\run_arm_runtime_host.ps1` | 0 | disabled/simulated Host 回归 PASS |
| `powershell -ExecutionPolicy Bypass -File final_project\\cpu\\tests\\run_arm_runtime_qemu.ps1 ...` | 0 | disabled/simulated QEMU PASS；1 秒 timeout probe PASS |
| `python -m py_compile final_project\\tools\\mycobot_firmware_version_readonly.py` | 0 | Stage-0 脚本语法通过 |
| `python final_project\\tools\\mycobot_firmware_version_readonly.py --help` | 0 | 脚本要求显式 `--port`，不自动选择设备 |
| `riscv-none-embed-gcc ... -fsyntax-only mycobot_protocol.c mycobot_transaction.c` | 0 | 目标 `rv32imac/ilp32` ABI 严格语法编译通过；不产生制品 |
| `git diff --check` | 0 | 无 whitespace error；仅报告 PowerShell 的 LF→CRLF 预警 |

## 风险与后续门

1. G7 仍需同批次 SoC/BSP/PNR/STA、正式 UART2 MMIO 与 `1 kΩ–2.2 kΩ` 限流的 F12→C14 板内回环，再做 100/100。
2. G8 前仍需设备端固件/电平/线序/共地/Pin4 悬空证据；首帧限制保持 GET_ANGLES-only。
3. G10 前仍需把逐关节拒绝接入受审 REAL backend；B2 的 software fail-closed 最小证据现已具备，但不能代替 G10 的实机安全门。
4. 当前 `build_arm_profile.ps1` 被刻意限定为 G0–G3 的 `NOT_FOR_FLASH` builder，硬性排除 UART2/真实 transport；它不是 G4 的可烧录构建路径。G4 必须在 A0 GUI 合法组合确认后，另行建立并验证同批次 SoC/BSP/ELF/bitstream 路径。

## 证据与关联

- 执行日志：`final_project/docs/debug_sessions/mycobot_goal_execution_20260715.md`
- 执行裁定：`final_project/docs/technical_plans/mycobot_arm_board_control_advancement_plan_20260715.md` §13
- 协议真源：`final_project/integration/mycobot_protocol_notes.md`
