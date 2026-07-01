# myCobot 280 联调

用于大象机械臂 myCobot 280 的安装核查、串口探测、Python / myBlockly 环境检查和安全联调。默认只读，不直接执行会让机械臂运动的命令。

## 资料入口
- 说明目录：`赛方提供材料/大象机械臂mycobot–280安装调试说明/`
- 控制软件：`相关软件/myblockly.Setup.1.3.6.exe`
- Python 安装包：`相关软件/python-3.10.4-amd64.exe`
- 串口驱动：`相关软件/CP210x_VCP_Windows/`
- 安装说明：`机械臂mycobot280的安装及调试说明.pdf`
- 调试案例：`机械臂mycobot280调试案例.docx`

## 工作规则
- 先阅读 `CLAUDE.md`、`AGENTS.md` 和本命令文件。
- 正式决赛路线是板上 CPU 控制 myCobot：协议封包、点位表、动作序列、互锁、超时和异常处理放到 CPU；FPGA RTL 只提供 UART/FIFO/寄存器等硬件通道。
- PC + `pymycobot` 只用于开发期标定、验证机械臂安全范围和记录点位，不进入正式比赛闭环。
- 不提出纯 FPGA RTL 直接承载 myCobot 动作状态机的主线方案。
- 先做环境检查：Python、`pyserial`、`pymycobot`、CP210x 驱动、COM 口。
- 串口探测优先识别 Silicon Labs / CP210x / `VID_10C4`，不要把蓝牙 COM 口当作机械臂。
- myBlockly 初始化选择 `MyCobot`，波特率使用 `1000000`。
- 通电前必须确认机械臂处于说明文档要求的安全姿态。
- 接线按资料记录：`TXD -> 机械臂 TX`、`RXD -> 机械臂 RX`、`GND -> 机械臂 GND`；若现场连不上，先复核接线，不要反复发送动作命令。
- 任何关节、坐标、夹爪、快速移动命令都必须先输出动作摘要并等待用户明确确认。

## 建议只读检查命令
```powershell
python -c "import sys, importlib.util; print(sys.executable); print(sys.version); print('serial', bool(importlib.util.find_spec('serial'))); print('pymycobot', bool(importlib.util.find_spec('pymycobot')))"
python -c "import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]"
Get-PnpDevice -Class Ports -ErrorAction SilentlyContinue | Select-Object Status,Class,FriendlyName,InstanceId
```

## 输出格式
```md
# myCobot 280 Check / Debug Log

## 本轮目标

## 环境状态
- Python：
- pyserial：
- pymycobot：
- myBlockly：
- CP210x / COM：

## 硬件连接检查
- 电源姿态：
- USB-TTL 接线：
- 端口：
- 波特率：

## 已执行命令
- 命令：
- 结果：

## 是否允许动作
- 结论：
- 原因：
- 需要用户确认的动作摘要：

## 决赛迁移边界
- PC 调试得到的点位/参数：
- 迁移到板上 CPU 的协议/状态机内容：
- FPGA RTL 仅需提供的硬件通道：

## 下一步
```
