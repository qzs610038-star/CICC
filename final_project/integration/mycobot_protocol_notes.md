# myCobot Protocol Notes

记录 myCobot 280 串口参数、协议帧格式、点位表、动作序列、返回值解析、超时和停止策略。

默认只记录开发期验证结论；正式闭环由板上 CPU 控制，PC 工具仅用于标定和安全验证。

## 证据优先级与适用范围（2026-07-14）

一级协议证据是 Elephant Robotics 官方文档：[6.6 基于串口通信协议开发使用](https://docs.elephantrobotics.com/docs/mycobot_280_ar_cn/3-FunctionsAndApplications/6.developmentGuide/CommunicationProtocolPackage/18-communication.html)（2026-07-14 读取）。本机 `pymycobot 4.0.5` 源码只作为实现交叉验证；如库的 `has_reply` 标记与官方线协议“无返回值”冲突，以官方线协议和后续真实抓包为准。

官方页描述的是 `USB Type-C` 通信设置。它可以证明应用层帧格式和 `1000000 8N1` 参数，但**不能证明** TJ375N529 J52 TTL 与机械臂底部接口可直接电气连接；J52 的电平、方向、共地、线序和机械臂端口协议仍必须按独立接线门验证。

## 直接协议通信前置条件

官方页要求：Basic 端烧录 `transponder`，Atom 端烧录最新版 `atomMain`。进入真实机械臂只读门前必须记录二者的固件来源、版本或文件哈希和烧录/核验时间；无法确认时保持 G8 NO-GO，不得用“无响应”反推 UART2 驱动错误。

官方公开关节范围如下。REAL 后端必须在编码前逐关节检查，并在点位标定后使用经 Review Packet 批准的保守裕量；不能只依赖 `int16_t` 可表示范围。

| 关节 | 官方最小角 | 官方最大角 |
|---|---:|---:|
| J1 | -168° | 168° |
| J2 | -135° | 135° |
| J3 | -150° | 150° |
| J4 | -145° | 145° |
| J5 | -165° | 165° |
| J6 | -180° | 180° |

官方同时列出关节最大速度 `150°/s`、最大加速度 `200°/s²`。协议中的 `speed` 是 0–100/百分比式参数，不能直接当作 `°/s`；项目当前主动拒绝 `speed=0`、只允许 `1..100`，属于更严格的失败即安全约束。

## 基础帧与事务规则

```text
0xFE 0xFE LEN CMD PAYLOAD... 0xFA
```

- `LEN = payload_len + 2`，覆盖 `CMD` 与结尾 `0xFA`，不覆盖两个帧头和 `LEN` 自身。
- 当前命令的多字节角度按 big-endian signed int16 编码，单位为 `deg_x100`；C 侧内部当前保存 `deg_x10`，发送时乘 10，接收时除 10。
- 官方页把有效 `LEN` 写为 `0x02..0x10`。当前通用 C parser 的 `MYCOBOT_MAX_PAYLOAD=64` 比官方窗口宽，后续 readonly/real 事务层必须至少拒绝超出官方长度窗口或不符合该命令精确长度的帧；不能因通用 parser 能装下就接受。
- 当前 `MyCobot` 帧不使用 CRC，完整性依赖双帧头、长度、命令、精确 payload 长度和 `0xFA`；因此噪声恢复与命令匹配必须显式测试。
- 官方说明“有返回值”的命令在 500 ms 内响应。首版板上事务使用**单请求在途**：只在收到匹配响应或超时并完成重同步后发下一请求；初始响应期限设为 750 ms（500 ms 官方上限 + 调度余量），真实只读首测请求周期不快于 1 s。
- 协议没有事务序号。收到帧后必须同时匹配 `expected_cmd`、精确 `LEN/payload_len` 和 payload 取值；未知、迟到、重复或命令不匹配帧只计数和丢弃，不得推进动作状态机。
- `SEND_ANGLES`、夹爪设置和 `STOP` 均无协议 ACK。TX 队列清空只表示字节已发出，不能表示机械动作已执行、到位、夹住或停止。

## 已核对的命令真值表

| 命令 | ID | 请求/响应 | 板上策略 |
|---|---:|---|---|
| `GET_ANGLES` | `0x20` | 请求 `FE FE 02 20 FA`；响应 `LEN=0x0E`、同命令字、12 字节角度 payload | readonly 第一阶段唯一允许主动发送的命令；解码后还要做关节范围检查 |
| `SEND_ANGLES` | `0x22` | 12 字节 big-endian `deg_x100` + 1 字节 speed，`LEN=0x0F`；**无返回值** | 仅 REAL 可达；完成判据使用后续 `GET_ANGLES` 连续到位读回，绝不等待 0x22 ACK |
| `STOP` | `0x29` | 无 payload；**无返回值** | 可作为软件停止请求，但不是安全额定急停；随后用 `IS_MOVING` 和角度稳定性核验，物理断电仍是最终保护 |
| `IS_IN_POSITION` | `0x2A` | 目标参数 + type，返回 1 字节 bool | 官方页对 type 又写“暂未使用”，首版不作为唯一到位证据；可在抓包确认后作为辅证 |
| `IS_MOVING` | `0x2B` | 无 payload；返回 1 字节 `0/1` | STOP 后的辅证；必须与 `GET_ANGLES` 稳定读回共同使用 |
| `GET_GRIPPER_VALUE` | `0x65` | 无 payload；响应 1 字节 `0..100` | 夹爪设置后的实际位置读回之一 |
| `SET_GRIPPER_STATE` | `0x66` | `open/close(0/1), speed`；**无返回值** | 官方页只证明 0/1；不放行 `10/254` 等库/固件变体 |
| `SET_GRIPPER_VALUE` | `0x67` | `value(0..100), speed`；**无返回值** | 不能以函数返回作为夹爪到位 |
| `IS_GRIPPER_MOVING` | `0x69` | 无 payload；响应 1 字节 `0/1` | 先确认停止，再读 0x65；仍不能单独证明物体已夹牢 |

夹爪建议完成条件是：发 0x66/0x67 后以单请求事务轮询 0x69，停止后至少两次读取 0x65 为稳定值并满足该点位的允许窗口。物体接触可能使夹爪无法到达空载目标值，因此 0x65/0x69 只能证明位置/运动状态，不能证明夹持力或物体没有滑落；带载成功仍需低速实物测试、抬升后观察和录像证据。

## 当前 C 实现审计

| 项目 | 当前文件/事实 | 结论 |
|---|---|---|
| 帧构造 | `cpu/app/src/mycobot_protocol.c` 写入 `LEN=payload_len+2`、双 `0xFE`、尾 `0xFA` | 与官方示例一致 |
| 角度发送 | `mycobot_encode_send_angles_payload()` 生成 13 字节，big-endian，`deg_x10 -> deg_x100`，speed `1..100` | 与 0x22 帧字段一致；尚缺逐关节官方硬限位 |
| 角度接收 | `mycobot_decode_get_angles_response()` 要求 12 字节并按 signed int16 解码 | 与 0x20 返回结构一致；内部 `deg_x10` 会舍去百分之一度精度，可接受但要在测试中明确 |
| 夹爪发送 | 0x66/0x67 helper 仅允许官方 0/1、0..100 和 speed `1..100` | 发送字段一致 |
| parser 上限 | `MYCOBOT_MAX_PAYLOAD=64` | 宽于官方 `LEN<=0x10`；G7 前需加命令级精确长度/官方窗口校验 |
| 事务层 | 现有 transport 负责 ring buffer、重同步和 TX 队列，但无 expected command、500 ms deadline 或 single-flight 状态 | G7/G8 阻塞项 |
| 停止与夹爪确认 | 未固化 0x29/0x2B/0x69，也无 0x65 bool/value decoder | G10/G11 阻塞项 |
| 点位校验 | 当前 controller 校验相邻点位变化和若干结构规则 | 未覆盖 J1..J6 官方绝对范围，REAL NO-GO |

## 官方文档歧义处理

官方页存在字段下标笔误和内部不一致，例如角度负数判断阈值写成 `33000`，通用命令号范围写成 `00..8F` 但后文又列出更高命令号，0x2A 的 type 字段既给取值又称暂未使用。处理规则：

1. 只实现本项目白名单中的命令，不按页面通用范围自动开放其他命令。
2. signed int16 按标准二补码解码；用官方示例帧、边界向量、本机 `pymycobot` 源码和真实抓包四方交叉验证。
3. 对每个带返回值命令写精确 `CMD + payload_len + payload domain` 测试，不接受“通用帧合法”替代事务合法。
4. 任何文档/库/真实抓包冲突必须写入 Review Packet；在冲突关闭前 readonly 保持单命令，real 保持 NO-GO。

## 本机 `pymycobot` 交叉证据

本机包版本为 `pymycobot 4.0.5`，主要参考：

- `C:\Users\33696\AppData\Roaming\Python\Python313\site-packages\pymycobot\common.py`
- `C:\Users\33696\AppData\Roaming\Python\Python313\site-packages\pymycobot\generate.py`
- `C:\Users\33696\AppData\Roaming\Python\Python313\site-packages\pymycobot\mycobot.py`

`GET_ANGLES=0x20`、`SEND_ANGLES=0x22`、`GET_GRIPPER_VALUE=0x65`、`SET_GRIPPER_STATE=0x66`、`SET_GRIPPER_VALUE=0x67` 及编码缩放均与官方页对齐。库内部某些 API 使用 `has_reply=True` 不得被解释成线协议一定有 ACK；板上完成判据按上表显式回读。

## 后续必须补齐的证据

- G7 前补官方示例帧与边界向量测试：长度上下界、错误命令、迟到/重复帧、负角、关节越界、750 ms 超时和重同步。
- G8 前确认 Basic `transponder` 与最新版 Atom `atomMain`，并记录版本/哈希；完成 J52 独立电气门。
- G8 用逻辑分析仪确认 `1000000 8N1`、`FE FE 02 20 FA` 与 30/30 次匹配响应；首测不自动重发。
- G10 前用真实抓包确认 0x29/0x2B 组合的停止表现；即使通过也不得删除物理断电门。
- G11 前抓包确认 0x65/0x69 在当前夹爪和固件上的真实行为，并完成空载、接触和滑落失败路径。
