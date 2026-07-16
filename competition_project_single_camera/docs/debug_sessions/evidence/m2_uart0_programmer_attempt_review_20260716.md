# M2 UART0 CPU Hello — Programmer 尝试复核（制品身份不匹配）

> 日期：2026-07-16
> 适用范围：仅 `competition_project_single_camera/` 隔离 CPU UART0 Hello 候选。
> 裁定：`CONFIG_TRANSPORT_PASS / BATCH_IDENTITY_NOT_ACCEPTED`。本次不授权 `JTAG_USER2`、ELF 下载或 UART0 打开。

## 操作员截图中可确认的事实

操作员提供的 Efinity Programmer 截图（未复制入仓库；截图文件 SHA-256：`2E8D42B03268BB7C3BEAA1D6B95B783949206DC027BEEE6B20DB62BE4FEF6A70`）显示：

- USB target 为 `YLS_4232DL`，USB 标识为 FTDI `0403:6011`、序列号 `FTBI7G42`；
- FPGA 为 `TJ375N529`，模式为 JTAG，器件 ID 为 `0x006A0EF3`，频率为 6 MHz；
- 控制台记录 `jtag programming started`、器件 ID 读出、`finished with JTAG programming`，随后为 `Device is in user mode!`；
- 实际 Programmer 文件为 ASCII 暂存目录中的 `outflow/mem_test.bit`。

这证明本次 JTAG 传输、目标发现和进入 user mode 均有正向现象；截图中的 GUI `Checksum 8AF51DA4` 不是 SHA-256，不能替代制品身份核验。

## 新鲜 SHA-256 对比

在 Codex 复核时，对截图中实际路径及当前批准制品分别执行 `Get-FileHash -Algorithm SHA256`，两条命令均正常返回：

| 文件 | SHA-256 | 结论 |
|---|---|---|
| 已批准 M2 批次 `outflow_m2_cpuhello_20260716_1730/mem_test.bit` | `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347` | 唯一允许的本 Gate bitstream |
| 截图实际编程的 ASCII 暂存副本 `outflow/mem_test.bit` | `9515FAC58CBE4DC07ADED28B6038C64485989C6BE650D09E5C07F6D5C4F1A169` | 不同批次，不能用作本 Gate 证据 |

两文件大小均为 `11,843,718 B`，但 hash 不同；因此大小相同不构成同一 bitstream 的证据。

## 根因与最小修复

根因是生成物路径未被同步到手工烧录目录，而非 FPGA、JTAG 或中文路径故障。`tools/sync_to_manual_burn_dir.ps1` 为保护本地生成物，明确排除 `outflow`/`outflow_*`，且默认保留目标目录中的既有输出。因此不能用其结果推断暂存目录的 `outflow/mem_test.bit` 已更新为当前 M2 批次。

只允许以下一次性、可逆恢复：在 ASCII 路径创建专属批次目录，直接复制批准 bitstream，并对源与目标均核对完整 SHA-256：

```powershell
$repoRoot = '<repository root>'
$stagingRoot = '<operator-selected ASCII staging directory>'
$src = Join-Path $repoRoot 'competition_project_single_camera\outflow_m2_cpuhello_20260716_1730\mem_test.bit'
$dst = Join-Path $stagingRoot 'm2_cpuhello_20260716_1730\mem_test.bit'
$expected = '2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347'

Get-FileHash -LiteralPath $src -Algorithm SHA256
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
Copy-Item -LiteralPath $src -Destination $dst -Force
Get-FileHash -LiteralPath $dst -Algorithm SHA256
```

源和目标的输出必须均为 `$expected`；任一个不同立即 STOP。随后只可在 Programmer 中选择 `$dst`，保持 JTAG 的易失性配置，不选择 USER1/USER2、Flash、DDR、UART2/J52 或机械臂。成功页必须同时能看见该专属 ASCII 路径和 `Device is in user mode!`，再回传 Codex 复核。

## 未关闭的门

- 本尝试没有配置已批准的 `2EA4...B8347` 制品，故不形成候选 M2 bitstream 板级证据。
- `JTAG_USER2`、`0xF9000000` ELF 下载、CPU 取指、UART0 横幅和回显仍为 `NOT VERIFIED`。
- `final_project` A0、UART2/J52、myCobot 只读和任何动作门均未触及。
