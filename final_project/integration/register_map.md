# Register Map

本表是决赛第一版 FPGA/CPU 寄存器窗口草案。实际基址必须来自最终 Efinity SoC 生成的 `soc.h`，例如官方示例中的 `SYSTEM_AXI_A_BMB` 或 `IO_APB_SLAVE_0_INPUT`，不要硬抄这里的占位地址。

## 地址窗口

| 符号 | 偏移 | 方向 | 说明 |
|---|---:|---|---|
| `REG_MAGIC` | `0x0000` | FPGA -> CPU | 固定魔数，用于确认 CPU 读到的是正确窗口。 |
| `REG_VERSION` | `0x0004` | FPGA -> CPU | RTL 寄存器版本。 |
| `REG_FRAME_ID` | `0x0008` | FPGA -> CPU | 特征快照帧号。 |
| `REG_FEATURE_VALID` | `0x000C` | FPGA -> CPU | FPGA 写完稳定快照后置 1。 |
| `REG_FEATURE_ACK` | `0x0010` | CPU -> FPGA | CPU 读完当前帧后写入已处理帧号。 |
| `REG_ROI_XY` | `0x0020` | FPGA -> CPU | `{y[15:0], x[15:0]}`。 |
| `REG_ROI_WH` | `0x0024` | FPGA -> CPU | `{h[15:0], w[15:0]}`。 |
| `REG_BBOX_X0Y0` | `0x0028` | FPGA -> CPU | `{y0[15:0], x0[15:0]}`。 |
| `REG_BBOX_X1Y1` | `0x002C` | FPGA -> CPU | `{y1[15:0], x1[15:0]}`。 |
| `REG_AREA` | `0x0030` | FPGA -> CPU | 前景像素面积。 |
| `REG_SUM_R` | `0x0034` | FPGA -> CPU | ROI/前景 R 或 Bayer R 统计和。 |
| `REG_SUM_G` | `0x0038` | FPGA -> CPU | ROI/前景 G 统计和。 |
| `REG_SUM_B` | `0x003C` | FPGA -> CPU | ROI/前景 B 统计和。 |
| `REG_SUM_Y` | `0x0040` | FPGA -> CPU | 亮度统计和。 |
| `REG_COUNT` | `0x0044` | FPGA -> CPU | 参与统计的像素数。 |
| `REG_CPU_HEARTBEAT` | `0x0080` | CPU -> FPGA | CPU 主循环心跳，OSD 可显示。 |
| `REG_RESULT_COLOR` | `0x0084` | CPU -> FPGA | `0=unknown,1=white,2=black,3=red,4=blue,5=yellow`。 |
| `REG_RESULT_SHAPE` | `0x0088` | CPU -> FPGA | `0=unknown,1=cube,2=cylinder,3=cone`。 |
| `REG_RESULT_SIZE` | `0x008C` | CPU -> FPGA | 单位 0.1 cm，例如 20/25/30。 |
| `REG_MATCH_ACTION` | `0x0090` | CPU -> FPGA | bit0 match, bit1 grab, bit2 skip, bit3 error。 |
| `REG_ARM_STATE` | `0x0094` | CPU -> FPGA | myCobot 状态机状态。 |
| `REG_ERROR_CODE` | `0x0098` | CPU -> FPGA | CPU/识别/机械臂错误码。 |
| `REG_TASK_MODE` | `0x00C0` | CPU -> FPGA | 当前任务模式，可供 OSD 显示。 |
| `REG_TARGET_COLOR` | `0x00C4` | CPU -> FPGA | 目标颜色。 |
| `REG_TARGET_SIZE` | `0x00C8` | CPU -> FPGA | 目标尺寸，单位 0.1 cm。 |
| `REG_DEBUG_STATUS` | `0x00F0` | FPGA -> CPU | CDC、丢帧、FIFO、UART 等摘要。 |

## 握手规则

- FPGA 只在一组特征全部写完后更新 `REG_FRAME_ID` 并置 `REG_FEATURE_VALID=1`。
- CPU 读取时先读 `REG_FRAME_ID`，再读特征，最后复读 `REG_FRAME_ID`；两次一致才接受该帧。
- CPU 完成分类后先写 `REG_RESULT_*`，最后写 `REG_FEATURE_ACK=REG_FRAME_ID`。
- OSD 只显示 CPU 已写完的一组结果，避免半更新字段上屏。
- 若后续启用共享 DDR ROI，寄存器中只放 ROI buffer 地址、长度和帧号；cache invalidate/flush 由 CPU 侧显式执行。
