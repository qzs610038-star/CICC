# FPGA / CPU Interface

本文件记录 FPGA 与板上 CPU 之间的 AXI、寄存器或 FIFO 接口。

必须明确：

- 地址基址和偏移。
- 读写方向。
- 时钟域和 CDC 处理。
- valid/ready 或 frame-stable 语义。
- CPU 回写结果与 OSD 显示之间的同步关系。

## 官方 RISC-V 例程给出的接口规则

1. 地址映射以最终 Efinity SoC 生成的 `soc.h` 为准。官方 `efx_hard_soc` 与 `gDMA/soc_dma_exp_0` 示例的 UART/APB/AXI 地址不同，不能混用。
2. 小数据主链路走 AXI/APB 寄存器窗口：ROI、bbox、面积、颜色统计、帧号、valid/ack、分类结果、动作状态和错误码。
3. 共享 DDR 只用于低频 ROI 或调试缓冲。CPU 读取 FPGA/DMA 写入的数据前必须 invalidate cache；CPU 写共享内存给 FPGA 读前必须 flush 或使用非 cacheable 区域。
4. 第一版使用轮询握手，后续再接 PLIC 中断。先验证 `REG_MAGIC -> CPU read -> CPU writeback -> OSD visible`，再接 frame interrupt。
5. 调试 UART 与 myCobot UART 要分清。官方 terminal UART 默认 115200；myCobot 资料要求 1000000，正式方案应规划第二路 UART 或清晰的复用/切换策略。

推荐窗口：

| 窗口 | 方向 | 用途 | 第一版建议 |
|---|---|---|---|
| `feature_regs` | FPGA -> CPU | 统计特征、帧号、valid、错误状态 | 必做 |
| `result_regs` | CPU -> FPGA | 分类结果、任务匹配、OSD 字段、动作状态 | 必做 |
| `control_regs` | CPU -> FPGA | 阈值页、ROI 配置、调试开关 | 必做 |
| `roi_buffer` | FPGA -> CPU | 低频 ROI 或调试图块 | 可选，寄存器链路稳定后再接 |
| `arm_uart_fifo` | CPU -> FPGA/UART | myCobot 串口发送/接收缓冲 | 若 UART IP 不由 CPU 直接控制，则必做 |

最小验收链路：

```text
FPGA exposes REG_MAGIC/REG_VERSION
CPU reads and prints them on UART
CPU writes CPU_HEARTBEAT/RESULT_* registers
OSD displays CPU_HEARTBEAT and RESULT_*
FPGA toggles FEATURE_VALID with FRAME_ID
CPU reads stable feature snapshot and writes FEATURE_ACK
```
