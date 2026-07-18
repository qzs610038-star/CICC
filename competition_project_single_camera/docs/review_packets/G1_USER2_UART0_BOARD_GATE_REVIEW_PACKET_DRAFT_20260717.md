# G1 USER2 / UART0 BOARD GATE — Review Packet Draft（2026-07-17）

状态：`OPERATION PACK OFFLINE READY / BOARD NOT VERIFIED`

## 目标与范围

为固定 G1 批次 `G1-20260717-A897-E5BC` 提供 fail-closed 的持板操作包。范围仅为：匹配 bitstream、`USER2`、片上 RAM Hello ELF、暂停 PC 范围证明，以及经独立批准后的 UART0 `115200` 只读监听。

本 Packet 不授权板卡操作；无原始板级证据前，`USER2`、CPU 取指、UART0、APB、视频和 OSD 均为 `NOT VERIFIED`。

## 固定输入

| 项目 | 固定值 |
| --- | --- |
| 输入基线 | `489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f` |
| bitstream | `mem_test.bit`, 11,847,132 bytes, `A897E33514A1079BB1B46C02C464B0BD679AF551CEFCB67C4A0EBD5B8FCD1ACD` |
| Hello ELF | `uart_hello_onchip.elf`, 31,116 bytes, `E5BC80A2F18A7E2951D53DA539BE2FC61AAECFA90C5CDADB29E65FFC6141928A` |
| ELF LOAD / entry | `0xF9000000..0xF9000A30` / `0xF9000000` |
| 合法 PC 范围 | `0xF9000000..0xF9003FFF` |
| TAP / UART0 | `USER2` / `115200 8N1` |

## 交付物与离线安全属性

- `g1_current_batch_manifest_20260717.json`：没有本机绝对路径；固定 batch、输入基线、制品身份、PC 范围、USER2、UART0 与禁止项。
- `capture_g1_user2_artifact_preflight.ps1`：仅接受显式 manifest/bit/ELF；同时验证 manifest 与制品的硬编码当前批次身份；不自动搜索制品。
- `prepare_g1_user2_staging.ps1`：先完整 preflight；仅接受 ASCII staging 目录；冲突目标拒绝覆盖；重新哈希源和目标。
- `capture_g1_uart0_banner.ps1`：无匹配 checkpoint、无审核人或时间、batch/hash/PC 范围不匹配时先退出，且不枚举/打开串口；显式 COM 和 `-Listen` 后才允许只读监听。
- 操作卡将 PC 证据与 UART0 监听划分为两个独立 checkpoint。

## 离线验证记录

| 检查 | 命令 / 方式 | 结果 |
| --- | --- | --- |
| PowerShell AST | 三个新脚本通过 `[System.Management.Automation.Language.Parser]::ParseFile` | PASS，3/3 零 error |
| 正确 preflight | 真实只读 bit/ELF 源 + 当前 manifest | PASS，exit 0，大小和完整 SHA-256 均匹配 |
| 正确 staging | `%TEMP%` ASCII 目录 + 真实只读源 | PASS，exit 0，复制后两份制品的大小和完整 SHA-256 均匹配 |
| 错误 bit / ELF / 缺文件 | `%TEMP%` 失配 fixture 与显式缺失路径 | PASS，三例均 exit 1；分别拒绝 bit identity、ELF identity 与缺失文件 |
| staging 冲突 | `%TEMP%` 中预置不同内容 `mem_test.bit` | PASS，exit 1，明确拒绝覆盖 |
| UART 拒绝路径 | 缺 checkpoint 与 checkpoint batch 不匹配 | PASS，两个例子均 exit 1；两份 JSON 均为 `serial_port_enumerated=false`、`serial_open_attempted=false`、`serial_port_opened=false`、`uart_bytes_sent=0` |
| offline presubmit（重计第 1 轮） | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\offline_presubmit.ps1` | PASS_WITH_WARNINGS，exit 0，freshness WARN=7，`ASSERTS_NOT_EXECUTED` 未出现 |
| offline presubmit（重计第 2 轮） | 同上 | PASS_WITH_WARNINGS，exit 0，freshness WARN=7，`ASSERTS_NOT_EXECUTED` 未出现 |
| offline presubmit（重计第 3 轮） | 同上 | PASS_WITH_WARNINGS，exit 0，freshness WARN=7，`ASSERTS_NOT_EXECUTED` 未出现 |

## 禁止项与边界

禁止 `USER1`、SoftTap、原始 `debug_ti.cfg`、Flash/SPI/PROM、外部 DDR、手填旧地址、UART2/J52，以及 myCobot 接线/帧/动作。

已证明：固定输入身份、G1 原子输入未变、PowerShell AST、真实制品 preflight/staging，以及错误制品、缺文件、staging 冲突、缺失或失配 checkpoint 的 fail-closed 拒绝路径。

未证明：任何硬件配置、PC 实际位置、UART0 字节、APB、视频、OSD 或机械臂行为。

## 持板机下一步输入

libaoxun 需提供：当前批次两份真实源文件路径、ASCII staging 路径、CheckPoint A 的 USER2/RAM/PC 原始证据；不得在未获审核签名 JSON 前 Resume 或监听 UART0。

## 当前收口阻断

三次连续离线 presubmit 已完成。期间曾有一次来自未跟踪学习指南行尾空白的外部工作区 FAIL；为保护并行工作未改写、未删除、未暂存该目录，待该目录修正后已从第 1 轮重新计数并取得上述三次 exit 0。仍需保持的唯一边界是：离线通过不等于任何硬件 PASS，持板机必须先提交 Checkpoint A 原始证据。
