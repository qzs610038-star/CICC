# CPU 工程区

本目录承载板上 CPU 正式闭环：识别分类、任务匹配、参数管理、myCobot 协议和动作状态机。

- `bsp_vendor/`：从最终 Efinity SoC 工程复制的最小 BSP。真源必须是当前生成的 `soc.h`、linker 和 OpenOCD/debug profile，不能硬抄任一官方示例地址。
- `app/`：正式应用源码、启动文件、链接脚本和 Makefile。
- `params/`：阈值、标定、点位和任务配置。
- `build_tools/`：Windows 批处理、部署脚本和旧工程迁移基线。

## 官方 RISC-V 例程吸收点

参考目录：`赛方提供材料/例程/RISC-V例程`。

- `01_eth_test-v4/embedded_sw/efx_hard_soc/`：优先参考其 BSP、OpenOCD、linker、UART/GPIO/Timer/DDR 示例和 Efinity RISC-V IDE makefile project 组织。
- `03_SD_test/.../ip/gDMA/.../embedded_sw/soc_dma_exp_0/`：参考 `EfxAxi4Example`、`EfxApb3Example`、`dCacheFlushDemo`，用于 CPU 访问自定义 FPGA 逻辑和共享 DDR cache 风险验证。
- eMMC/SD 操作说明只吸收流程：先下载 FPGA bitstream，再导入/构建 CPU makefile project，进入 debug，打开串口终端运行。

当前 `app/include/bsp.h` 和 `app/linker/linker.ld` 是占位骨架。正式接入 SoC 后必须用最终工程生成物校正 UART、CLINT、PLIC、AXI/APB user window、DDR base/size 和工具链前缀。
