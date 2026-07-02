# Architecture Decisions

## ADR-001: 采用 FPGA 前端 + 板上 CPU 决策控制

决策：正式路线采用 FPGA 负责视频前端、ROI、统计特征和 OSD，板上 CPU 负责颜色/形状/尺寸分类、任务判定、参数管理和 myCobot 控制。

原因：纯 RTL 识别和纯 RTL 机械臂控制在阈值调整、异常处理和赛场调试上风险过高；板上 CPU 属于指定 FPGA 平台内部资源，适合承担规则决策和协议控制。

## ADR-002: 以官方 RISC-V BSP 生成物作为 CPU 平台真源

决策：CPU 平台层以 Efinity 当前工程生成的 `soc.h`、linker、OpenOCD/debug profile 为真源；官方 `RISC-V例程` 只作为流程、BSP 结构、AXI/APB 访问和 cache 风险参考，不直接硬抄地址。

原因：官方例程中 `efx_hard_soc` 与 `gDMA/soc_dma_exp_0` 的 UART、APB、AXI、CLINT 地址和频率不同。正式工程若复用错误示例地址，会出现 UART 能打印但寄存器/DDR/中断不可用的隐蔽故障。详细吸收记录见 `riscv_official_examples_integration.md`。
