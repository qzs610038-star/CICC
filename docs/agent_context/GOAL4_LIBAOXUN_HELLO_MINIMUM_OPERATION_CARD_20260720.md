# Goal 4：libaoxun 板卡 CPU Hello 最小操作卡

> 当前探索阶段唯一目标：**在板载 Type-C UART1 上看到 CPU 输出的三行 Hello。**
>
> 本卡不要求窗口、审批、哈希、APB、OSD、UART2/J52 或机械臂步骤。按本卡看到三行 Hello，即按当前定义记录 `GOAL4=PASS (CPU_HELLO_ONLY)`。

## 先准备：只开两个窗口

### 窗口 A：串口终端

打开你平时使用的串口工具（任意一种即可：Efinity 的 serial terminal、PuTTY、MobaXterm、SSCOM 等）。

1. 将电脑用 USB 直连板卡的 **Type-C UART1** 口。
2. 在串口工具中选择“插上这根 Type-C 线后新出现的端口”。**不要选择 COM17 或名称含 CH340 的端口**，它们是历史 UART0 路线。
3. 设置：`115200`、`8 data bits`、`No parity`、`1 stop bit`、`No flow control`。
4. 开启“保存日志”或准备稍后截图；保持接收窗口为空，**先不要发送任何字符**。

### 窗口 B：板卡控制窗口

只在下列两种情况中选一种：

- 当前探索分支的 FPGA/CPU 镜像已经在板上运行：使用板卡的复位键，或按你们当前正常方式断电再上电。
- 当前镜像尚未在板上运行：打开你们平时使用的 Efinity/JTAG 下载窗口，只下载当前探索分支的镜像到 FPGA；选择 **JTAG/volatile** 模式，禁止选择 Flash、SPI、PROM、erase 或 `.hex`。

不要打开机械臂软件，不接 UART2/J52，也不要打开旧 UART0 调试窗口。

## 实际操作：三步完成

1. **先开始接收**：确认窗口 A 正在接收、端口设置为 `115200 8N1`，并清空旧文本。
2. **再让板卡启动**：在窗口 B 对当前探索镜像执行一次复位/上电；如果未运行当前镜像，先做一次 volatile JTAG 下载，再复位一次。
3. **只看 10 秒**：不要点击发送、不要粘贴文本。若接收区依次出现以下三行，立即截图或保存日志：

```text
I0 UART1 HELLO
UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100
Type characters to verify echo.
```

首行前允许有一个空行；三行必须在**同一次启动后的同一串口窗口**中、按上述顺序出现。

## 判定

| 观察到的结果 | 结论 | 下一步 |
|---|---|---|
| 三行完整、顺序正确 | `GOAL4=PASS (CPU_HELLO_ONLY)` | 保存截图即可，停止；不继续 APB/UART2/机械臂。 |
| 乱码 | `NOT PASS` | 先确认窗口 A 是 115200 8N1，再重做一次“先接收、后复位”。 |
| 只有部分行或行序不对 | `NOT PASS` | 保存截图；不手工补行，不换 UART0。 |
| 10 秒内没有任何输出 | `NOT PASS` | 保存截图；检查是否选错端口或当前探索镜像是否真的已下载。 |

## 可选的 10 秒强确认（不是 PASS 必要条件）

在三行 Hello 出现后，在窗口 A **只发送字符 `U`，不要按 Enter**。若收到同一个 `U`，记为 `ECHO_U=PASS`。这额外证明 UART1 的收发都通，但未做此项不影响上述 Hello-only 的 Goal 4 PASS。

## 最小回传格式

```text
GOAL4=PASS (CPU_HELLO_ONLY)
uart_port=<实际端口>
settings=115200 8N1 no-flow-control
capture_started_before_reset=YES
hello_three_lines=EXACT_ORDER_PASS
evidence=<截图或日志文件名>
echo_U=<PASS / NOT_TESTED>
```

此结果只证明板上 CPU 的 UART1 Hello 已打通；不证明 APB、OSD、UART2 或机械臂。
