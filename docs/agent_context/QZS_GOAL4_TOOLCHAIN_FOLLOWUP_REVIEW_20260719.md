# QZS Goal 4 Toolchain Follow-up Review — 2026-07-19

`VERDICT=BLOCKED_EXECUTION_TOOLCHAIN`

本记录只覆盖固定 SHA 的静态反向审查与 qzs 治理修复，不授权 Efinity Programmer、OpenOCD、
GDB、JTAG、USER2、串口、APB、接线或机械臂动作。

## 固定输入

- qzs temporary-register origin：`fd3fc0881d4e71338f1aa34f361cd498b7cd2d4c`；当前远端 HEAD 以 `ls-remote` 为准
- 两人 patch base：`182fd6f5c4d628379760d6f4fc74e3b342e30083`
- WSC contract：`48548f47dfa5964b13aed7edf3b3e9da6f6583a2`
- libaoxun blocker snapshot：`2d713b80a41185e472837abaec3a10c01383c70f`

## 已关闭

- 三条远端 ref 已独立 `ls-remote` 回读；qzs worktree clean、`git diff --check` exit 0。
- WSC 提交直接基于 patch base，仅四个临时授权文件；contract verifier 与 G2/classifier 复跑通过。
- libaoxun 提交直接基于 patch base，十个变更均匹配临时登记，未迁移旧 `5a61c4c` 或 WSC 历史；
  PS1 为 CRLF，GDB/CFG 为 LF，execution manifest 已绑定 verifier 本身。

## 未关闭与原因

1. `riscv use_bscan_tunnel 6 1` 的参数是 DM TAP IR width 与 tunnel type；
   `riscv set_bscan_tunnel_ir` 的参数是访问 tunnel 的外层 FPGA IR。当前候选把三者误命名为
   type/user/width，并向外层 IR 传 `8`；Titanium USER2 必须为 `0x09`。
2. UART capture 只读取 `RESUME_ONCE` marker，无受控生产者；Hello GDB 的同步 `continue` 无法证明
   marker 对应真实 resume。
3. APB GDB 只打印 `timeout_ms=1000` 后同步 `continue`，没有 host timer、timeout 主动 halt、
   halt-reason Gate 或失败路径 RAM-read 抑制的真实 consumer。
4. 原始 build verifier 缺六个 evidence-root 文件，exit 1；`82/21` 是 manifest 数量，不是 PASS。
5. qzs schema-v1 final manifest 将三个 PS1 的 LF 工作树字节写入 hash，但策略要求 CRLF；fresh
   worktree 产生 CRLF 并正确暴露 mismatch。schema-v2 必须记录并验证实际 EOL 统计。

## 唯一下一链

qzs 先完成 schema-v2/fresh-worktree manifest 与临时 orchestrator 授权；libaoxun 再追加最小修复，
恢复原始 evidence 并交付固定 SHA；WSC 只读复核 consumer；最后由 qzs 做两份独立补丁的集成
审查。任一项未关闭时不得申请硬件窗口。

```text
GOAL4=BLOCKED_EXECUTION_TOOLCHAIN
HARDWARE_ACTIONS=NONE
USER2=NOT_VERIFIED
PC=NOT_VERIFIED
UART1_HELLO_ECHO=NOT_VERIFIED
APB_MAGIC=NOT_VERIFIED
```
