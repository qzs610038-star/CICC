# IO Pin Map

本文件记录正式工程使用的 MIPI、HDMI、UART、JTAG、GPIO 和机械臂相关引脚。

任何修改 `constrain.sdc`、`mem_test.xml` 或 `.peri.xml` 后，都要同步更新本文件。

## T0 板到臂真源冻结表（2026-07-13 更新）

> 状态：**FAIL / 部分文档真源已冻结**。J52/C14/F12 与开发板 3.3V VCCIO 已由官方资料确认；机械臂端线序、电平实测、正式工程约束和其余 T0 项尚未全部通过。本表只登记可追溯的事实；`未知`、占位宏、PC 调试日志或口头确认均不得当作板上真源。
> 在表中所有项目均为 PASS 前，不得进入 S2（CPU Hello/APB）后的 UART2 硬件适配、不得接通真实机械臂控制线，也不得发送任何真实机械臂帧。

| 项目 | 必须冻结的值 | 当前值 | 状态 | 证据路径 / 日志 / 照片编号 | 复核人 | 日期 |
|---|---|---|---|---|---|---|
| CPU / SoC | 视频工程内 SoC IP、生成 `soc.h`、linker、ELF 下载方式 | 未知；仓库未发现生成的 `soc.h` | FAIL | 待 A 提供生成目录、Efinity 工程节点和 CPU Hello 日志 | 待定 | 待定 |
| APB 用户从机 | 基址、`REG_MAGIC` 地址、时钟、复位、CPU 可见读回路径 | 未知；`board_io.h` 仅含偏移，测试基址不可上板 | FAIL | 待 `soc.h` + RTL/工程 XML + 读回日志 | 待定 | 待定 |
| UART2 MMIO | 基址、输入时钟、1 Mbps 分频、DATA/STATUS/FIFO 位、8N1 配置 | 未知；不得引用 `bsp.h` 占位宏 | FAIL | 待 `soc.h` / 生成驱动 / RTL / 逻辑分析截图 | 待定 | 待定 |
| UART2 PLIC | source ID、priority、enable、threshold、claim/complete 语义 | 未知 | FAIL | 待生成文件或赛方同配置示例及板级日志 | 待定 | 待定 |
| 板端物理接口 | UART2 TX/RX 引脚、方向、空闲电平、逻辑电平、GND、是否需电平转换 | 文档冻结且用户人工图像核验一致：右侧 J52 2.54mm 4Pin；Pin1 GND，Pin2 `FPGA_UART2_RXD/C14/Input`，Pin3 `FPGA_UART2_TXD/F12/Output`，Pin4 `VCC(5V/3.3V)` 必须悬空；C14/F12 均为 3.3V VCCIO。空闲电平、正式约束与直连兼容性未实测 | FAIL（板端位置/针序已核验，电气未验） | `final_project/docs/review_packets/mycobot_uart2_j52_wiring_review_20260713.md` + `赛方提供材料/硬件文档/TJ375N529开发板端口说明图.jpg` | 用户人工图像核验；Codex 文档复核 | 2026-07-13 |
| myCobot 线序 | 控制器端 TX/RX/GND 对应、供电隔离、连接器型号 | 用户确认候选：J52 GND→机械臂 GND，C14/RXD←机械臂 TX，F12/TXD→机械臂 RX，VCC NC；机械臂独立 12V5A。机械臂官方 USB-TTL 说明存在 TX→TX/RX→RX 的命名冲突，且底座信号电平容限未在该页给出 | FAIL（待断电双人复核/测量） | `final_project/docs/review_packets/mycobot_uart2_j52_wiring_review_20260713.md` + 待现场接线照片/测量记录 | 用户提供；待 A/C 双签 | 2026-07-13 |
| 急停真源 | 输入来源、默认电平、去抖、CPU 可见路径、触发后的停发新命令语义 | 未知 | FAIL | 待原理图/RTL/固件读回与触发日志 | 待定 | 待定 |
| 机械结构 | 底座加固、外部基准复核、工作区净空、断电方式和现场安全员 | 尚未获得外部基准无相对位移证据 | FAIL | 待 C 提供复核记录、照片/录像和安全员签核 | 待定 | 待定 |

### 冻结规则

1. 每项必须提供可访问的证据路径或现场照片/录像编号；没有证据即为 FAIL。
2. `bsp.h`、`board_io.h` 的占位地址和 Host/QEMU 测试宏只可用于测试，不能填写为正式地址。
3. UART2 引脚、电平、线序和共地必须在断电状态下完成双人复核；不相容时先记录批准的电平转换/隔离方案。
4. 急停触发的失效安全语义必须是“停止下发新动作并进入 ESTOP/FAULT”；不得把自动释放扭矩当作默认动作。
5. 本表由 A（SoC/RTL）、B（CPU）和 C（现场安全）分别签核后，才可将本节状态改为 PASS，并附对应 Codex Review Packet。

## UART2 J52 文档冻结速查

| J52 | 信号 | FPGA 管脚 | 方向 | 正式候选接法 |
|---:|---|---|---|---|
| Pin1 | GND | - | 参考地 | 机械臂 GND，必须共地 |
| Pin2 | `FPGA_UART2_RXD` | C14 | Input | 接机械臂 TX（待实物双签） |
| Pin3 | `FPGA_UART2_TXD` | F12 | Output | 接机械臂 RX（待实物双签） |
| Pin4 | `VCC(5V/3.3V)` | - | 电源 | **悬空，禁止连接** |

开发板侧 C14/F12 均为 3.3V VCCIO。该事实不自动证明机械臂端信号容限，也不替代空闲电平、1 Mbps 波形、共地和线序实测。
