# 04 MyCobot 280 Notes

> 来源路径仅用于本机追溯；原件线下获取，仓库不收录。

## 来源入口

- `赛方提供材料/大象机械臂mycobot–280安装调试说明/`
- 安装调试说明：`机械臂mycobot280的安装及调试说明.pdf`
- 调试案例：`机械臂mycobot280调试案例.pdf`、`机械臂mycobot280调试案例.docx`
- 软件说明：`相关软件/myblockly&python安装说明.docx`
- 驱动目录：`相关软件/CP210x_VCP_Windows/`

## 分工

本机负责机械臂调试主线：CP210x 驱动确认、串口枚举、`pymycobot` 环境、myBlockly 基础连接、安全姿态、只读状态读取和后续小幅动作验证。队友只需根据本文件理解控制边界，不需要在视觉分析机上复现机械臂动作。

## 当前控制规则

- myBlockly 初始化型号选择 `MyCobot`。
- 串口选择本机实际 COM 口。
- 波特率使用 `1000000`。
- 调试先做只读状态读取、串口识别和日志观察，再考虑 RGB 灯板或极小幅安全演示。
- 任何会导致机械臂运动的命令都必须显式确认目标、速度、角度范围、周边空间和急停/断电方式。

## 与 FPGA 的边界

- 正式比赛闭环应由板上 CPU 封装 myCobot 协议、点位表、动作序列、互锁、返回值解析、超时和异常处理。
- FPGA RTL 只保留 UART/FIFO/寄存器等硬件通道，不承担复杂机械臂动作状态机。
- PC、myBlockly、`pymycobot` 只用于开发期标定、调试和安全验证，不进入正式识别/控制闭环。

## 只读检查命令

```powershell
python -c "import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]"
python -c "import importlib.util; print('pymycobot', bool(importlib.util.find_spec('pymycobot')))"
```

这些命令不应触发机械臂运动。
