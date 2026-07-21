# G-NB-01 myCobot 查询事务时间边界 Review Packet（草稿）

> 日期：2026-07-17
> 范围：仅 Host 测试 `final_project/cpu/tests/test_mycobot_arm_skeleton.c`。
> 安全状态：`HOST_ONLY`；未启用 UART2、REAL backend、J52、myCobot 帧发送或任何机械臂动作。

## 目标与改动

为 `mycobot_transaction` 新增五组独立的 fail-closed 回归：

1. 从开始时刻起 749 ms 保持在途，750 ms 恰好超时；
2. `uint32_t` 时基在 deadline 回绕到 `0` 后，`UINT32_MAX` 尚未超时、`0` 恰好超时；
3. 错误命令、错误长度、错误域响应均返回对应错误并保持在途；
4. 首个合法 `GET_ANGLES` 响应结束事务；其后两帧迟到/重复响应仅计入 `late_or_duplicate`；
5. 在途事务拒绝第二次 `begin`，并保持原查询命令不变。

实现文件 `final_project/cpu/app/src/mycobot_transaction.c` 未改动。

## 验证记录

| 命令 | 退出码 | 结果 |
|---|---:|---|
| 在 `%TEMP%\\g_nb_01_mycobot_transaction` 作为运行目录执行 `powershell -ExecutionPolicy Bypass -File D:\\第十届集创赛-雄芯院材料\\final_project\\cpu\\tests\\run_mycobot_arm_skeleton_host.ps1` | 0 | PASS；PATH 中没有 native C 编译器，runner 自动使用 Efinity RISC-V QEMU，日志确认 `RESULT: PASS (QEMU asserts executed).` |
| `git diff --check` | 0 | PASS；无 whitespace error。 |
| `git diff -- final_project/cpu/tests/test_mycobot_arm_skeleton.c` | 0 | 审查范围仅为新增五组 Host 测试及其 `main` 调用。 |

新增 **41** 条 `assert`，分别为：时间边界 7、回绕 8、错误帧保持在途 12、首个合法响应获胜 8、single-flight 6。

## 未验证边界与风险

- 本结果只证明 Host/QEMU 上的纯 C 事务逻辑；不证明 UART2、1,000,000 baud、解析器接收时序、MMIO、OSD、板上 CPU 或机械臂闭环。
- 本测试覆盖 750 ms 的正常与回绕边界；未覆盖跨越 `INT32_MAX` 的不现实超长在途区间。当前 timeout 仅 750 ms，位于该比较约束的安全范围内。
- 测试直接构造已解析的 `mycobot_frame_t`；串行噪声、帧拆分/拼接及硬件到达时间仍应由 transport/parser 与后续独立 Gate 验证。

## 裁定

**HOST_ONLY PASS。** 新断言全部与现有 fail-closed 实现一致，未出现 red-test，也没有扩大运行时或硬件控制范围。
