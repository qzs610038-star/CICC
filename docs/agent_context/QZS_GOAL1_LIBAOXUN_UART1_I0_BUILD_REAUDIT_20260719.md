# QZS Goal 1 — libaoxun UART1 I0-BUILD 重新推送复审

> 结论：`CHANGES_REQUESTED`
>
> 审查时间：2026-07-19（Asia/Shanghai）
> 审查模式：固定 SHA、只读。未合并、未运行 Efinity、未执行 USER2/UART/APB/板级/机械臂操作，未修改 RTL/XML/SDC/IP/BSP。

## 固定审查元组

| 字段 | 值 |
|---|---|
| 远端 ref / 证据提交 | `refs/heads/dev/libaoxun688-uart1-i0-20260719` / `7fb3d3bc162e2aec4335eeb4aaea2f8b2ea72c83` |
| 被构建的设计提交 / 父提交 | `6effdc3685d696cb4d33f3fbb1c449729ed72e33` / `f47af290c2f014dfa8a131a3baebec1e9560ae21` |
| Batch ID | `I0_UART1_20260719_FINAL` |
| 审查工作区 | `codex/qzs-final-integration-goals-20260718@6d5e33a2b188abac2fbc5e36dab3155eba45d4f2`；既有共享 dirty 保持不动 |
| 提供的 evidence 文件 | `embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_{EVIDENCE.md,INPUTS.sha256,MANIFEST.json}` 与 `verify_i0_uart1_build_evidence.ps1` |
| 声明的原始证据根 | `D:\cicc_cbm_link\competition_project_single_camera\outflow_i0_uart1_20260719_final` |
| 声明的 work 根 | `D:\cicc_cbm_link\competition_project_single_camera\work_i0_uart1_20260719_final` |

候选相对设计提交 `6effdc...` 仅新增上述 4 个 evidence 文件；相对 `f47af290...` 的 19 个候选文件均匹配 libaoxun 静态白名单（`embedded_sw/**`、IP、XML、顶层）。`git diff --check` 对两个比较范围均通过。

## 已确认的静态事实

- `PERI_UART_0=0`、`PERI_UART_1=1`、`PERI_UART_2=0`；`mem_test.peri.xml` 的 UART1 RX/TX 为 `GPIOR_96/GPIOR_100`。
- 候选 `soc.h` 含 `SYSTEM_UART_1_IO_CTRL=0xe8011000`、115200、`DATA_LENGTH=7`、`PARITY=NONE`、`STOP=ONE`。evidence 文档说明 `7` 为数据长度减一的寄存器编码；但此说明尚未由可读取的原始工具/driver 文件独立复核。
- 新 packet 明确保持 USER2、Type-C UART1 Hello/echo、APB MAGIC、ch0/HDMI 板级回归、UART2/J52 与 myCobot 为 `NOT VERIFIED`/out of scope；未把 BUILD 外推成板级 PASS。这一状态边界正确。
- Manifest 声明 Efinity `2025.2.288.4.15`、Map/Interface/PNR/STA/CDC/PGM 结果、bitstream SHA-256 `8F02D32335C0D4DD70256C2A7D37B84F0A8DB59D3506374256B8D8F6514864E1`，以及 Hello ELF SHA-256 `36EBB1BB06E73479897DA834DED6E4E8A06175EEBF31421F1C47CF5F4CE83241`；这些目前只是 manifest 声明，尚未通过原始文件 hash 核验。

## P0 Findings

### P0-1：固定提交的清洁检出无法通过输入哈希校验

对候选 Git tree 的 ZIP 无损展开后，`ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb.v` 的实际 SHA-256 是：

```text
git tree bytes (LF): 53E63128C9E76CD46AD11E979215E13F663F98BB617BB05290404FF343ACB597
manifest expected:  18801C59925F963DD633B6A87D2A5BE1DBEB24FA05485E9F4E4D2BD04F2D6816
same text as CRLF:  18801C59925F963DD633B6A87D2A5BE1DBEB24FA05485E9F4E4D2BD04F2D6816
```

`git check-attr` 对该路径返回 `text: set`、`eol: lf`。因此 packet 记录的是非规范 CRLF 工作区字节，而固定候选要求该文件按 LF 检出；`verify_i0_uart1_build_evidence.ps1` 在清洁检出的固定 SHA 上会报 `Atomic input SHA-256 mismatch`。这不是本机是否保留 outflow 的问题，而是“固定 SHA 与被哈希的实际构建输入”没有以同一字节表示绑定。

若 libaoxun 机器仅在其遗留 CRLF 工作区通过脚本，则该 PASS 不能证明一个清洁检出的固定提交可复现。必须在该机清洁检出候选后重建哈希清单并重跑 Efinity，或以该机真实、明确记录的字节状态生成新的不可变提交/证据链。

### P0-2：本机不保留原始 outflow/work 不是本次否决依据，但必须在 libaoxun 机器实跑校验

按用户说明，本机不要求存在 libaoxun 的受控目录；本机对 manifest 的 `raw_evidence_root` / `raw_work_root` 的 `Test-Path` 均为 `False` 仅记录为环境差异，不单独否决本包。完成 P0-1 修复后，libaoxun 必须在其实际保留原始证据的机器执行 `verify_i0_uart1_build_evidence.ps1` 并保留 exit 0 的完整输出。该输出是 Map/Interface/PNR/STA/CDC/warning/bitstream/ELF hash 可消费的最低证据。

## P1 Findings

### P1-1：原子输入清单不覆盖 BSP/`soc.h` 或 Hello/link 输入

`I0_UART1_BUILD_INPUTS.sha256` 有 73 行，但实测：

- `soc.h` 匹配行：`0`；
- `embedded_sw/**` 匹配行：`0`；
- `uart1_hello_onchip/**` 匹配行：`0`；
- Manifest `artifacts[]` 中 `soc.h` 项：`0`。

这与本项目要求的 Hard SoC XML/peri/SDC/IP/wrapper/顶层/BSP/Hello/APB 原子审查不一致。证据文档虽单独写出一个 `soc.h` SHA-256，但没有让校验脚本或清单实际验证该文件，也没有绑定 Hello source、makefile、linker/BSP 输入。即使恢复原始 outflow，也不能据现有清单完整验证同批 `soc.h` 与 Hello 身份。

### P1-2：物理 pinout 与 8N1 的原始报告仍待补齐

Packet 声明 `GPIOR_96_CLK13/B12` 和 `GPIOR_100/D12`，并用外部 Efinity driver 解释 `DATA_LENGTH=7`。由于 pinout report、driver 文件和它们的声明路径均不可读取，qzs 只能确认源码/manifest 文本，不可独立确认物理 pinout 或该编码解释。

## P2 Findings

新增 evidence 文件本身属于 libaoxun 静态白名单，候选未新增 CPU 业务、qzs 治理、冻结语义或机械臂文件；其非板级声明也保持正确。问题在于证据可用性和原子覆盖，而非本次文档 diff 的空白或静态路径范围。

## 重新审查前的最小补件

1. 在 libaoxun 机器清洁检出固定候选后，按 `.gitattributes` 实际字节重建全部输入 SHA-256 清单；不要把 CRLF/LF 归一化为“等价”，因为 Efinity 消费的是实际文件字节。随后的 Efinity 构建、bitstream 和报告必须使用同一份清单。
2. 在该同一工作区重新运行并保留 `verify_i0_uart1_build_evidence.ps1` 的成功原始输出。外部 evidence 路径可只在 libaoxun 机器保留，但每个 manifest artifact 必须存在、size 与 SHA-256 完全匹配。
3. 将生成的 `embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h` 的相对路径、SHA-256 和同批身份加入实际受验证清单；同时纳入 Hello source/makefile、linker/BSP 输入或明确将 Hello 构建留给 Goal 2 后再从 I0-BUILD packet 移除其 ELF 声明。
4. 使清单/校验脚本覆盖冻结要求的完整原子集合后，再提供 Map、Interface/pinout、PNR、STA、CDC、warning、bitstream 和工具版本的可核验摘要与 libaoxun 本机验证输出。

在以上补件完成并重审前，wsc 不得消费该 `soc.h` 进行 Goal 2 UART1 Hello 构建，qzs 不得进行 Goal 3 集成；USER2、UART1/APB 与所有机械臂相关 Gate 继续不授权。

## 执行的只读检查

- `git ls-remote --heads origin`：固定重新推送 ref 为 `7fb3d3bc162e2aec4335eeb4aaea2f8b2ea72c83`。
- `git fetch` 该精确 ref：仅更新本地远端跟踪对象，未 merge。
- `git diff-tree` / `git diff --check`：候选文件范围与空白检查通过。
- 读取 candidate evidence/manifest/input-list/verification script：发现 input-list 覆盖缺口。
- 对 manifest 声明目录的只读存在性、目录枚举与 artifact hash 预检：本机两个根目录均不存在，按用户说明仅记录为环境差异。
- 对候选 Git tree 的 ZIP 无损展开和 SHA-256 复算：发现 `EfxSapphireHpSoc_slb.v` 的 manifest hash 仅匹配 CRLF 变体，而 `.gitattributes` 固定该路径为 LF。
