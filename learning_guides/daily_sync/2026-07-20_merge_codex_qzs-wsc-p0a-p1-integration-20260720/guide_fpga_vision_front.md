# 角色 A：看懂“不改 FPGA”也是严格集成

> 10–15 分钟目标：理解双人分支对 FPGA/Hard SoC 的影响边界。

## 核心结论

这次像给整车增加“发动机诊断仪和驾驶规则模拟器”，没有打开 FPGA 发动机舱：RTL、
XML/peri.xml、SDC、IP 和冻结接口均未改变。入口见
[双人集成记录](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/docs/merge_governance/records/2026-07-20_qzs_wsc_p0a_p1_integration.md)。

## FPGA 侧需要读懂的四件事

1. P0-A 固件有 canary、阶段码和有界 UART probe，但 Host PASS 不能证明 CPU 已在板上取指。
2. P1 Host 模型固定 snapshot、ACK、结果提交语义，但没有猜 APB 偏移，也没有实现 CDC/OSD wire ABI。
3. `interface_freeze=PASS` 只说明 8 个冻结文件没被改写，不是板级链路已打通。
4. libaoxun 的 UART1/USER2 实验仍在活动线上，本分支没有吸收其仍变化的硬件原子批次。

源码入口：
[P0-A 诊断](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/cpu_bringup/uart1_hello_onchip/src/p0a_diag.c)、
[UART1 冻结页](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md)。

## 为什么 Host READY 不是板级 READY

Host 测试像在桌面检查钥匙和仪表逻辑；真正上板还要证明时钟、复位、CPU 取指、RAM
映射、USER2 和 UART1 管脚属于同一原子批次。XML、SDC、IP、wrapper 或固件输入一变，
旧 bitstream/ELF 证据都不能继承。

## 5 分钟自检

- 找出集成记录中的 `BOARD_VERIFIED`。
- 解释 `P1_HOST_READY=YES` 为何不能推出 APB/CDC/OSD 完成。
- 说出双人分支与三人分支名字的差异。

## 自学入口

### 优先赛方资料

- [CURRENT_STATE](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/CURRENT_STATE.md)
- [接口冻结与所有权](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md)

### 拓展基础知识

- CDC 单槽事务、ready/valid 与跨时钟 ACK。
- 链接脚本、ELF LOAD 段、volatile canary 和 memory-mapped UART。

## 未验证

USER2、板上 PC/canary、Type-C UART1、真实 APB/MMIO/CDC/OSD 均为 `NOT VERIFIED`。
