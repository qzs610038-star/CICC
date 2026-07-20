# Goal 4 I0 UART1 — libaoxun 单主机执行与证据闭环

> 生效：2026-07-20。此文件覆盖此前 Goal 4 派工中“qzs 汇总/批准/签发、WSC READY 或实时判读”为执行前置条件的部分；不改变冻结接口，也不授权 UART2/J52、myCobot、Flash 或任何持久化烧录。
>
> **探索期覆盖（当前优先）**：用户已暂时放弃窗口与严格哈希校验，并将 Goal 4 的通过条件缩减为“在 libaoxun 板卡主机的 Type-C UART1 看到 CPU Hello 三行”。当前只按 [`GOAL4_LIBAOXUN_HELLO_MINIMUM_OPERATION_CARD_20260720.md`](GOAL4_LIBAOXUN_HELLO_MINIMUM_OPERATION_CARD_20260720.md) 操作和判定；本页其余 hash、runner 和完整 evidence 要求仅保留作后续恢复严格 Gate 时的历史/备用信息。

## 结论

Goal 4 的硬件段交由 **libaoxun 在连接 `YLS_4232DL` 的证据主机独立打通**。qzs 不再签发硬件执行、不再作为 READY 汇总者，也不要求在窗口中在线；wsc 的合同仅是 runner 已固定消费的只读输入，不是本次执行的人工前置交接。

本机 qzs 的唯一剩余职责是：若 libaoxun 主动提供完整原始证据包，则可将事实归档到状态文档。归档不是通过条件，未归档不阻塞执行。

## 唯一有效执行输入

- runner ref：`origin/codex/qzs-goal4-runner-portable-20260720@2434f013aaa57d98ee3eda040da2414ff1092b59`。
- WSC 只读合同 checkout：`48548f47dfa5964b13aed7edf3b3e9da6f6583a2`。
- board：`YLS_4232DL`；Type-C UART1：`COM10`，`VID=0403`，`PID=6011`，serial=`FTBI7G42C`。
- 固定制品：bitstream `D05EFD4E...E2BC544`、Hello ELF `919B291A...F5F7FA`、probe ELF `6CB7E5E1...3DFEAEC`、`soc.h` `25BABB96...2C982B`。
- OpenOCD 外层 TAP：`0x006A0EF3`；仅 volatile JTAG 的 `.bit`，明确 `mode=jtag`。

`I0_UART1_GIT_WINDOW_APPROVAL_PORTABLE_20260720.json` 及
`QZS_GOAL4_I0_UART1_PORTABLE_RUNNER_APPROVAL_20260720.md` 中的 02:15–05:30 窗口已经过期，**只作静态兼容性历史证据，绝不可重用为 live 授权**。

## 单主机开始条件

libaoxun 仅在自己的干净 detached checkout 中执行；不使用 dirty 个人工作区，也不从 qzs 主机复制路径。开始一次 live 尝试前，操作人自行完成并记录：

1. runner checkout 的 HEAD 严格为 `2434f013...`、工作区 clean，runner/manifest 所列文件 hash 全部匹配。
2. 原始制品、同批 `soc.h`、工具 SHA-256/版本、板卡、FTDI PnP identity 全部匹配；零或多个 Efinity 目标均 fail-closed。
3. UART2/J52 与机械臂物理隔离；稳定供电、Type-C/JTAG 固定、无异味/异常热、无不稳定 hub；Ctrl-C 与断电方式明确。
4. 在执行 checkout 外新建本次 run directory，并在其中生成一份 **operator start record**。它是 runner 所需 schema-v2 的兼容记录，说明此轮由 libaoxun 基于用户的单主机授权启动；它不是 qzs 批准或签名。

operator start record 的 `window_start_utc` / `window_end_utc` 必须包住当前时间，建议最长 60 分钟；每次重新开始均生成新记录和新 run directory。记录的全部 hash 必须与 runner manifest 一致；任何字段漂移即停止，而不是手改制品、配置或 checkout 来“通过”。可从历史 JSON 复制工具/制品字段，仅可更新窗口、`window_id` 和下列操作人证明字段：

```json
"record_type": "LIBAOXUN_OPERATOR_START_RECORD",
"operator": "libaoxun",
"execution_authority": "USER_DELEGATED_SINGLE_OWNER_20260720",
"physical_precheck": "PASS_WITH_OPERATOR_ATTENDANCE",
"uart2_j52_isolated": true,
"qzs_execution_signoff_required": false
```

runner 中 `ApprovalRecordPath`、`APPROVAL_SHA256` 是 schema 兼容字段名；它们不表示 qzs 仍有签发权。

## 唯一允许链与停止规则

只运行 `-Mode Live -Scenario run`：同批 hash preflight → volatile JTAG USER2 / RAM PC → 三行 UART1 Hello 与一个无 CR/LF 的可打印 `U` 回显 → WSC APB probe 的断点/四次 RAM 读/只读 `0x375A0001`。

以下任一情况立即停止本次尝试，保存目录，不回退或扩展：hash、工具、XML、设备或 PnP drift；Flash/PROM/SPI/erase/`.hex`；USER2/PC gate 失败；UART0/COM17/CH340；APB 写/扫描/意外地址；UART2/J52；异常热、气味、掉电或 USB 不稳。

执行结论只能是 `I0-SMOKE=PASS` 或第一个精确失败点：`USER2_FAIL`、`PC_RANGE_FAIL`、`UART1_FAIL`、`APB_MAGIC_FAIL`。PASS 不外推 I1–I4、OSD、机械臂或 UART2。

## 最小证据包

不再发送 READY、批准请求、实时判读或多轮交接。一次执行只保留一个 evidence bundle：

- operator start record 及 SHA-256；runner `execution.log`、OpenOCD stdout/stderr、volatile-config log、GDB/probe log、PnP/USB snapshot；
- checkout HEAD/clean status、runner/manifest/制品/工具 SHA-256 与版本；
- 每阶段 UTC 时间、exit code、PC、串口收发字节计数、APB 地址来源与读值；
- 第一失败的原始输出，或完整 PASS 的所有原始日志索引。

原始日志留在 libaoxun 证据主机；提交到 Git 的只能是脱敏摘要、相对索引和 SHA-256。qzs 若之后归档，只能逐项引用该 bundle，不能把主机外的推测写成板级事实。
