# QZS Goal 4 I0 UART1 `eda235a1` 窗口批准记录

## 结论

`VERDICT=APPROVED_FOR_LIBAOXUN_HARDWARE_WINDOW`

仅连接开发板的 libaoxun 主机可以执行。本次 qzs 审查没有枚举 qzs 本机端口，也没有启动 Efinity、OpenOCD、GDB、串口或任何硬件会话。

窗口为 `2026-07-20 01:35:00+08:00` 至 `2026-07-20 04:45:00+08:00`，共 3 小时 10 分钟。尝试次数不设上限；每次尝试仍须独立记录并 fail-closed。到期、用户 Ctrl-C 或严重阻塞立即终止。

## 固定输入与静态复核

- libaoxun 远端：`refs/heads/dev/libaoxun688-goal4-static-20260719` = `eda235a1a0a1470cab2e166b1a388e87954b922b`。
- parent：`293dbd7ac6f9e6609585f056a1b9e442b4ef0737`。
- changed files：runner、manifest、verifier 三项。
- manifest SHA-256：`72622BEDFCCFD914D5B70C9D51C67B4EDDB0CE3A77642A01BC4712B432D6EF70`。
- runner SHA-256：`406F1AB83B759F631D25C6C4ECE5346FB57B40A90927D32AD782DDC97BE100DB`。
- verifier SHA-256：`3FB6C6F50DC1B26CA4FAFC717EB57142727D56FD252C45650EFF85B53A258C37`。
- `git diff --check`：PASS。
- 隔离 checkout 静态 verifier：exit 0，`I0_UART1_EXECUTION_CONFIG_STATIC=PASS`，`HARDWARE_ACTIONS=NONE`。
- volatile route：只允许 `--flow program --pgm_opts mode=jtag source=<fixed.bit>`；默认 active、Flash、PROM、SPI、JTAG Bridge 和 `.hex` 均拒绝。
- Efinity USB target：只能在 libaoxun 主机运行时枚举，并要求唯一 `0403:6011 / FTBI7G42C`。
- TJ375N529 JTAG ID：`0x006A0EF3`；不得退回 Ti375 默认 `0x006A0A79`。

## 操作前人工硬件核查

1. 开发板供电稳定，电源线与 Type-C/JTAG 线缆无松动，不使用不稳定 USB Hub。
2. 板旁没有异常发热、异味、供电反复重启或 USB 反复掉线。
3. Efinity Programmer、旧 OpenOCD/GDB、串口终端和旧 runner 进程全部退出，避免 FTDI 通道占用。
4. 在 libaoxun 主机确认 COM10 仍是 `VID=0403;PID=6011;SERIAL=FTBI7G42C;INSTANCE=...\\0000`；不得选择 COM17、CH340 或 J44/UART0。
5. UART2/J52 与机械臂保持隔离，本窗口不授权任何机械臂查询或动作。
6. 操作员能够在异常时立即 Ctrl-C，并具备安全断电手段。

## 主要剩余风险

- runner 对可恢复失败会进入下一独立 attempt；必须有人值守。若同一错误重复，应 Ctrl-C，先检查刚完成的 attempt 日志，再在窗口内重新运行，禁止无人值守持续循环。
- JTAG volatile config 成功只证明 FPGA 进入用户模式，不等于 USER2 CPU TAP、PC、UART1 或 APB 已通过。
- USB 枚举与实际配置之间仍存在很短的设备状态变化窗口；任何重枚举、掉线或目标数量变化均视为严重阻塞。
- UART 无硬件握手；Hello 丢失可能来自 capture 时序、复位/时钟、UART1 管脚或 CPU 未执行，不能据此切换 UART0。
- APB 仅允许 CPU probe 单次读取；debugger 不得直接监视或读取 APB 区间。

## 授权文件

`competition_project_single_camera/docs/evidence_manifests/I0_UART1_GIT_WINDOW_APPROVAL_EDA235_20260720.json`

USER2、PC、UART1 Hello/Echo 与 APB MAGIC 在执行前均保持 `NOT_VERIFIED`。
