# 官方 RISC-V 例程吸收记录

> 日期：2026-07-02  
> 来源：`赛方提供材料/例程/RISC-V例程` 与 Efinity RISC-V Embedded Software IDE 流程图截图。

## 1. 可吸收结论

官方 CPU 例程对本项目最有价值的部分不是 Ethernet、eMMC 或 SD 功能本身，而是 Sapphire SoC 的软件工程组织、BSP 生成物、AXI/APB 访问方式、JTAG/OpenOCD 调试流程，以及 CPU 与自定义逻辑共享 DDR 时的 cache 风险。

本项目应把它吸收到正式架构中：

- FPGA 继续负责 MIPI/ROI/统计特征/OSD。
- 板上 RISC-V CPU 负责颜色、形状、尺寸分类，任务判定，参数管理和 myCobot 动作状态机。
- FPGA 与 CPU 之间优先用 AXI/APB 寄存器传小量稳定特征；共享 DDR 只用于低频 ROI 或调试缓冲。
- CPU BSP、地址映射、时钟频率和 linker 必须来自当前 Efinity SoC 生成物，不能硬抄某一个示例的旧地址。

## 2. 关键来源文件

官方示例中有两类容易混淆的 SoC 参考：

| 来源 | 价值 | 注意 |
|---|---|---|
| `01_eth_test-v4/embedded_sw/efx_hard_soc/` | 更贴近 TJ375C529 顶层工程的硬核 SoC 软件包，含 BSP、OpenOCD、linker、UART/GPIO/Timer/DDR/Ethernet 示例。 | `soc.h` 中 UART/APB/AXI 地址与 gDMA 示例不同，必须以最终工程重新生成的 `soc.h` 为准。 |
| `03_SD_test/.../ip/gDMA/T120F576_devkit/embedded_sw/soc_dma_exp_0/` | 提供更小的 Sapphire standalone 示例，尤其是 `EfxAxi4Example`、`EfxApb3Example`、`dCacheFlushDemo`，适合学习 CPU 访问自定义逻辑。 | 这是 IP 示例内的软核配置，地址和时钟不应直接当作 TJ375N529 决赛工程地址。 |
| `RISC-V eMMC Demo操作说明.docx`、`RISC-V SDHC Demo操作说明.docx` | 证明官方推荐流程是先下载 FPGA bit/hex，再在 Efinity RISC-V IDE 中导入 makefile project、build、进入 debug、打开串口终端运行。 | 文档中有硬编码本机路径，只能借鉴流程，不能照搬路径。 |
| Efinity RISC-V IDE 截图 | 说明 IDE 是 Eclipse/RiscFree 风格，支持 C/C++ makefile 工程、BSP/FreeRTOS 导入、OpenOCD/debug、寄存器/内存/外设窗口。 | 我们应保持 CPU 工程可被 IDE 导入，同时保留命令行 make 作为可复现构建入口。 |

## 3. 地址映射启示

当前官方例程里至少出现了两套地址风格：

- `efx_hard_soc` 示例：`SYSTEM_UART_0_IO_CTRL=0xe8010000`、`SYSTEM_AXI_A_BMB=0xe8000000`、`IO_APB_SLAVE_0_INPUT=0xe8100000`、`SYSTEM_CLINT_HZ=200000000`。
- `gDMA/soc_dma_exp_0` 示例：`SYSTEM_UART_0_IO_CTRL=0xf8010000`、`IO_APB_SLAVE_0_INPUT=0xf8100000`、`SYSTEM_CLINT_HZ=50000000`。

因此正式工程规则如下：

1. `final_project/cpu/app/include/bsp.h` 里的地址只能作为占位。
2. 每次 Efinity SoC/IP 配置变化后，必须同步当前生成的 `soc.h`、linker 和 OpenOCD/debug profile。
3. Review Packet 必须列出 `soc.h` 地址摘要：UART、CLINT、PLIC、AXI user window、APB user window、DDR base/size。
4. 如果 CPU hello 能打印但读写 FPGA 寄存器失败，优先检查地址窗口是否来自错误示例。

## 4. FPGA/CPU 接口落地

官方 `EfxAxi4Example` 证明 CPU 可以用 `write_u32/read_u32` 访问 AXI user window；`EfxApb3Example` 证明可通过 APB 寄存器结构控制自定义逻辑。对本项目建议：

- `APB/AXI-lite feature_regs`：正式主接口，承载 ROI 坐标、bbox、面积、颜色统计、frame counter、valid/ack、CPU 分类结果、OSD 字段、机械臂状态。
- `DDR roi_buffer`：只作为可选大块缓冲，用于低频 ROI、调试截图或算法验证，不作为每帧全图扫描路径。
- `interrupt`：第一版可轮询；当帧稳定后再接 PLIC 中断，避免初期把 CDC、地址、PLIC 配置混成一个问题。
- `UART`：官方 terminal UART 默认 115200；myCobot 需要 1000000，应规划第二路 UART 或明确复用策略。正式比赛不建议把调试终端和机械臂控制混在同一个串口。

## 5. Cache 与 DDR 风险

官方 `dCacheFlushDemo` 明确展示了一个风险：自定义逻辑通过 AXI 写 DDR 后，CPU 可能仍从 D-cache 读到旧值，必须 invalidate 或 flush。

对本项目的约束：

- CPU 读取 FPGA 写入的共享 DDR/ROI 前，必须先执行 cache invalidate，或把该区域配置成非 cacheable。
- CPU 写给 FPGA/OSD 的共享内存结果，必须 flush 后再置 valid。
- 对每帧只需几十到几百字节的统计特征，优先走寄存器窗口，避免 cache 一开始就进入主链路。
- 如果后期使用 DMA/DDR，验收用例必须包含“FPGA 写新 ROI -> CPU 不 invalidate 读旧值 -> invalidate 后读新值”的显式对照。

## 6. 软件流程图吸收

截图中的 SoC Sapphire Design Flow 可以转成我们的比赛软件流程：

1. Efinity 生成硬件 SoC/RTL，并把 bitstream 下载到 FPGA。
2. Efinity RISC-V IDE 或 makefile 构建 CPU C/C++ 程序。
3. 通过 JTAG/OpenOCD debug 下载/运行 CPU 程序。
4. CPU 通过 UART 打印日志，通过 IDE 查看寄存器、内存、外设状态。
5. CPU 读 FPGA 特征寄存器，完成识别/任务/动作决策，再写回 OSD 与机械臂控制状态。

这要求正式工程保留三类可观察性：

- UART 日志：启动、地址摘要、帧号、分类结果、动作状态、错误码。
- OSD 可视化：ROI、bbox、识别结果、match/skip/grab、CPU heartbeat。
- IDE 调试：breakpoint、step、register view、memory view、peripheral register view。

## 7. 推荐启动顺序

1. **CPU hello**：bitstream 下载后，RISC-V IDE 导入/构建 makefile project，UART 打印 `hello`，JTAG halt/resume 可用。
2. **寄存器 ID**：FPGA 暴露只读 `REG_MAGIC/REG_VERSION`，CPU 读出并打印。
3. **写回 OSD**：CPU 写 `CPU_HEARTBEAT`、`RESULT_COLOR/SHAPE/SIZE`，OSD 画面能看到变化。
4. **frame-stable 握手**：FPGA 更新统计后置 `FEATURE_VALID` 与 `FRAME_ID`，CPU 读完写 `FEATURE_ACK`，确认无撕裂。
5. **ROI/DDR 可选链路**：只在寄存器链路稳定后接入，并加入 cache invalidate/flush 对照测试。
6. **myCobot 串口**：先只读/极小安全动作，再接入任务状态机；串口参数按 myCobot 资料使用 1000000 波特率。
7. **FreeRTOS 延后**：官方 IDE 支持 FreeRTOS，但分赛区保底版先用 standalone super-loop；只有当串口、计时、异常处理复杂到必须调度时再引入。

## 8. 当前需要修正的项目假设

- `final_project/cpu/app/include/bsp.h` 现在的地址注释不能再写成唯一来源。它应标注为占位，后续由最终 Efinity SoC 生成的 `soc.h` 覆盖。
- `final_project/cpu/app/linker/linker.ld` 现在假设程序从 DDR `0x00000000` 运行；官方 `efx_hard_soc/default.ld` 使用 `0x00001000`，gDMA RAM 示例使用 `0x80000000`。正式链接脚本必须等 SoC boot/loader 方式定版后再锁定。
- `final_project/cpu/app/Makefile` 目前使用 `riscv32-unknown-elf-`；官方示例包含 `riscv64-unknown-elf.mk`。最终工具链前缀需按 Efinity 2025.2 实机安装目录验证。

## 9. 决赛架构增量

把官方 CPU 例程吸收后，决赛架构应新增一个明确的 `cpu_platform` 层：

```text
cpu_platform/
  generated_soc.h snapshot      # 当前 Efinity 生成地址真源
  linker/current_linker.ld      # 当前 boot/run 方式对应 linker
  openocd/debug profile         # 当前 JTAG/IDE 调试入口
  bsp_adapter.h                 # 项目只依赖这一层，不直接散落 vendor 地址
```

正式应用继续保持：

```text
cpu/app/
  board_io.c            # 读 FPGA 特征寄存器、写 OSD/状态寄存器
  vision_classifier.c   # 颜色/形状/尺寸规则分类
  task_matcher.c        # 现场任务条件匹配
  arm_controller.c      # myCobot 动作状态机
  mycobot_protocol.c    # 协议封包与回包解析
  param_table.c         # 阈值、点位、任务参数
```

这样既利用官方 IDE/BSP/debug 能力，又避免把赛方 Ethernet/SD/eMMC 示例的复杂外设栈带进比赛主链路。
