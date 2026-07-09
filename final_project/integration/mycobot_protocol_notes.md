# myCobot Protocol Notes

记录 myCobot 280 串口参数、协议帧格式、点位表、动作序列、返回值解析、超时和急停策略。

默认只记录开发期验证结论；正式闭环由板上 CPU 控制，PC 工具仅用于标定和安全验证。

## 当前协议证据（2026-07-09）

本机 PC 端实测脚本使用 `pymycobot.mycobot.MyCobot` 类。已核对本机安装包：

- `C:\Users\33696\AppData\Roaming\Python\Python313\site-packages\pymycobot\common.py`
- `C:\Users\33696\AppData\Roaming\Python\Python313\site-packages\pymycobot\generate.py`
- `C:\Users\33696\AppData\Roaming\Python\Python313\site-packages\pymycobot\mycobot.py`

本机包版本：`pymycobot 4.0.5`。`MyCobot` 的 `get_angles()`、`send_angles()`、`set_gripper_state()`、`set_gripper_value()` 均来自 `pymycobot.generate.CommandGenerator`。

### 基础帧格式

`MyCobot` 类经 `DataProcessor._mesg()` 封包，基础帧格式为：

```text
0xFE 0xFE LEN CMD PAYLOAD... 0xFA
```

其中：

- `LEN = payload_len + 2`，覆盖 `CMD` 与结尾 `0xFA`。
- 多字节数值用 big-endian signed int16，证据为 `common.py` 的 `_encode_int16()`。
- 角度编码使用 `_angle2int(angle) = int(angle * 100)`；C 侧内部当前保存 `deg_x10`，因此上板发送 payload 时再乘 10 变为 `deg_x100`。
- 坐标编码使用 `_coord2int(coord) = int(coord * 10)`；当前 CPU 迁移第一阶段暂不依赖坐标闭环。
- `pymycobot` 中存在 CRC 机器人类别（如 Mercury/Pro 系列），但当前 PC 脚本导入的是 `MyCobot`，不属于 `crc_robot_class`，因此当前迁移不得默认使用 CRC 帧。

关键源码索引：

| 事实 | 证据 |
| --- | --- |
| `HEADER = 0xFE` / `FOOTER = 0xFA` | `common.py:131-132` |
| `LEN = len(command_data) + 2`，非 CRC 类追加 `0xFA` | `common.py:554-589` |
| CRC 机器人列表不含当前 `MyCobot` 类 | `common.py:541` |
| big-endian int16 编码 | `common.py:599-604` |
| 角度/坐标缩放 | `common.py:618-624` |
| list 参数按 int16 pair 展开，scalar 参数按 byte 拼接 | `common.py:673-705` |
| 返回 payload 取 `LEN - 2` 并按 int16 pair 解码 | `common.py:736-790` |

### 当前已固化的命令

| 命令 | ID | 请求 payload | 返回/解析 | C 侧 helper 状态 |
| --- | --- | --- | --- | --- |
| `GET_ANGLES` | `0x20` | 无参数，完整请求帧为 `FE FE 02 20 FA` | 预期 12 字节 payload：6 个 big-endian signed int16，单位 `deg_x100`；C 侧解码回 `deg_x10`。 | `mycobot_decode_get_angles_response()` 要求 `payload_len == 12`。 |
| `SEND_ANGLES` | `0x22` | 6 个 big-endian signed int16 角度 `deg_x100` + 1 字节 speed，payload 共 13 字节，完整帧 `LEN = 0x0F`。 | 上层 `pymycobot` 标记 `has_reply=True`；CPU 第一阶段只固化发送 payload 与后续角度读回确认，不依赖完成码。 | `mycobot_encode_send_angles_payload()` 生成 13 字节；speed 必须为 `1..100`，无效速度直接拒绝。 |
| `SET_GRIPPER_STATE` | `0x66` | 默认 2 字节：`flag, speed`。`pymycobot` 注释含 `0=open, 1=close, 10=release`，也存在带 `_type_1` 的三字节变体。 | 不作为当前闭环确认依据；夹爪动作由状态机节拍和后续机械姿态读回间接约束。 | 当前 helper 仅放行 `flag=0/1` 和 `speed=1..100`。`release` 需单独安全状态与固件确认后再启用。 |
| `SET_GRIPPER_VALUE` | `0x67` | 默认 2 字节：`value, speed`；`value=0..100`，`speed=1..100`。带 `gripper_type` 时存在三字节变体。 | `pymycobot` 标记 `has_reply=True`；CPU 第一阶段只固化默认二字节 payload。 | `mycobot_encode_gripper_value_payload()` 拒绝越界 value/speed。 |

命令 ID 证据：

- `GET_ANGLES = 0x20`、`SEND_ANGLES = 0x22`、`GET_COORDS = 0x23`：`common.py:195-198`
- `GET_GRIPPER_VALUE = 0x65`、`SET_GRIPPER_STATE = 0x66`、`SET_GRIPPER_VALUE = 0x67`：`common.py:276-278`
- `get_angles()` 请求无参数：`generate.py:142-148`
- `send_angles()` 先 `angle * 100`，再 `_mesg(SEND_ANGLES, angles, speed, has_reply=True)`：`generate.py:161-170`
- `set_gripper_state()` 默认 `flag, speed`，可选 `_type_1`：`generate.py:526-543`
- `set_gripper_value()` 默认 `value, speed`，可选 `gripper_type`：`generate.py:545-561`

### 与 PC 实测脚本的交叉对齐

- `mycobot_pc_tests/` 里的 PC 脚本统一使用 `MyCobot(PORT, BAUD)`，且波特率为 `1000000`。
- 赛方机械臂资料可支持安装、型号选择、`1000000` 波特率、TX/RX/GND 接线等结论，但当前本地资料未抽取到完整命令 ID/帧字段表；命令字段仍以本机 `pymycobot` 源码和后续串口抓包为准。
- 当前迁移不得把“通信降频防溢出”写成 V2 失败根因；50-100 ms 级轮询是工程节拍规则，真实根因叙事见 `mycobot_pc_tests/audit_logs/v2_codex_review_migrated_findings.md`。

待补：

- 用官方 GitBook/手册或串口抓包再次确认 myCobot 280 机型固件的帧格式。
- 抓包确认 `SET_GRIPPER_STATE` 的 release 语义到底采用 `10`、`254` 或其他固件变体；在确认前，自动闭环只允许 open/close。
- 补充 `GET_GRIPPER_VALUE` 的真实返回 payload 长度和成功/失败码。
- 在真实板侧发送前，用逻辑分析仪确认 `0xFE 0xFE LEN CMD ... 0xFA` 波形与波特率 `1000000`。
