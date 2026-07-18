# QZS Goal 1 — libaoxun UART1 I0-BUILD 固定 SHA 审查记录

> 审查结论：`BLOCKED`
>
> 审查时间：2026-07-19（Asia/Shanghai）
> 审查边界：只读固定提交与仓库内可发现材料；未合并、未运行 Efinity、未执行 USER2/UART/APB/板级或机械臂操作，未修改任何 RTL/XML/SDC/IP/BSP。

## 固定审查元组

| 字段 | 固定值 |
|---|---|
| 审查开始分支 / HEAD | `codex/qzs-final-integration-goals-20260718` / `6d5e33a2b188abac2fbc5e36dab3155eba45d4f2` |
| 审查开始 dirty | 3 个既有未跟踪 qzs 文档：`COMPLETE_DEVELOPMENT_ROUTE_AND_TEAM_TASKS_20260718.md`、`QZS_GOAL2_WSC_CPU_UART1_AUDIT_20260718.md`、`QZS_TASK_BREAKDOWN_AND_STRONG_GOALS_20260718.md`；均不归属本 Goal |
| 远端 ref / 完整 SHA | `refs/heads/dev/libaoxun688-uart1-i0-20260719` / `6effdc3685d696cb4d33f3fbb1c449729ed72e33` |
| 候选父提交 | `f47af290c2f014dfa8a131a3baebec1e9560ae21` |
| Review Packet / 构建摘要 | `NOT PROVIDED`：候选树与本地工作区均未发现本批次 packet 或 build summary |
| 原始 Efinity 证据位置 | `NOT PROVIDED`：未发现同批 Map/Interface/PNR/STA/CDC/warning、bitstream 或其 SHA-256 |
| 当前结论可消费性 | Goal 2（wsc UART1 Hello）与 Goal 3（qzs 集成）均不可消费；等待本记录“解除阻塞所需补件”全部齐全后重新只读审查 |

远端 ref 由 `git ls-remote --heads origin` 发现并以 40 位 SHA 固定；为读取该固定对象，仅获取了对应远端跟踪 ref，未做 merge/cherry-pick/checkout。`tools/agent_handoff_health_check.ps1` 在审查开始时通过。

## 候选文件与范围裁定

`git diff-tree --name-status -r 6effdc...` 显示下列 15 个文件。它们均匹配 `tools/team_scope_check.ps1 -Role libaoxun` 的静态白名单，故均为 `IN_SCOPE`；本候选无 `OUT_OF_SCOPE` 文件。

| 状态 | 文件 |
|---|---|
| IN_SCOPE | `competition_project_single_camera/mem_test.xml` |
| IN_SCOPE | `competition_project_single_camera/mem_test.peri.xml` |
| IN_SCOPE | `competition_project_single_camera/src/top.v` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb.v` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.v` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.vhd` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_wrapper.v` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/hard_ip_args.ini` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/ipm_pt_map.json` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/settings.json` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/source/Axi4PeripheralTop.v` |
| IN_SCOPE | `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/source/peri_config` |
| IN_SCOPE | `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h` |
| IN_SCOPE | `competition_project_single_camera/embedded_sw/uart1_hello_onchip/makefile` |
| IN_SCOPE | `competition_project_single_camera/embedded_sw/uart1_hello_onchip/src/main.c` |

`constrain.sdc` 与 `src/apb_reg_magic.v` 属于强制原子审查集合，但本候选未改：父/候选 Git blob 分别相同为 `de49ba3ff8320feb95008f840a41090b3039f2f8` 与 `5793ec42655568e9d1fe23f84be3d3f3238b4694`。未改不等于可跨批次继承其构建结论；它们仍必须进入本次原子输入 SHA-256 清单。

## 已核对的静态事实（不等于构建或板级通过）

- `settings.json` / `hard_ip_args.ini` 显示 `PERI_UART_0=0`、`PERI_UART_1=1`、`PERI_UART_2=0`，wrapper、IP 顶层和工程顶层的端口由 `system_uart_0_*` 改为 `system_uart_1_*`。
- `mem_test.peri.xml` 将 RX 配为 `system_uart_1_io_rxd` / `GPIOR_96`，TX 配为 `system_uart_1_io_txd` / `GPIOR_100`。`B12` / `D12` 是冻结路线中的板级管脚映射；候选未提供可将这两个 GPIO 映射回该物理管脚的原始 Efinity 报告或导出证据。
- 候选 `soc.h` 的同树 Git blob 为 `d3d5127644b3a93932c2ea4b172da2441cf587e2`，包含 `SYSTEM_UART_1_IO_CTRL=0xe8011000`、`SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_BAUDRATE=115200`、`...PARITY=NONE`、`...STOP=ONE` 和 UART1 FIFO/采样参数；它没有 `SYSTEM_UART_0_*` 定义。
- 新增 Hello 源码只引用 `SYSTEM_UART_1_*` 宏，没有硬编码 UART 控制器基址。它只是源码，候选未交付匹配 ELF、ELF SHA-256、段/入口检查或实际构建记录。
- 本候选未变更 `CURRENT_STATE.md`、`SESSION_HANDOFF.md` 或其他状态文档，因此没有在源码提交中把 BUILD 外推为 USER2、UART1、APB、OSD 或板级 PASS。当前状态文档的 `NOT STARTED/NOT VERIFIED` 不能被本次未随附的证据隐式升级。

## P0 / P1 / P2 Findings

### P0 — 同批 I0-BUILD 证据包缺失，无法裁定原子构建成功

没有可审查的 Build ID、Efinity 完整版本与调用记录、工程 SHA/原子输入 SHA-256 清单、Map/Interface/PNR/STA/CDC 原始报告、完整 warning 清单/处置、bitstream 文件与 SHA-256、或以相同 batch ID 标记的 `soc.h` SHA-256。候选树的代码与 Git blob 只能证明“该树包含某些 UART1 文本”，不能证明 Efinity 正式生成、离线构建成功、时序/CDC 状态、warning 边界或制品身份。

这同时使“没有手改生成 wrapper/`soc.h`”无法独立证明。`mem_test.peri.xml` 的版本字段 `2025.2.288.4.15` 只是文件内元数据，不是本批 Efinity 执行日志。

### P1 — 物理管脚和 8N1 编码缺少同批可追溯说明

候选静态树可见 `GPIOR_96/GPIOR_100` 与 115200、NONE、ONE 宏，但没有原始 Efinity report/export 将其闭合到冻结的 `B12/D12`，也没有说明 `...INIT_CONFIG_DATA_LENGTH=7` 在该 IP 的 frame-config 编码中如何对应要求的 8 data bits。审查者不能据此擅自解释编码或替代工具报告。

### P2 — Hello 源码不可作为 Goal 2 构建产物

`embedded_sw/uart1_hello_onchip/**` 在 libaoxun 静态白名单内，故不构成白名单越界；但它没有构建日志或 ELF，且 Goal 2 仍必须由 wsc 基于获批的、同批身份已验证的 `soc.h` 审查其实际 Hello 构建。不得把这两个源码文件写成 Goal 2 PASS，也不得由此推断 USER2/UART/APB/OSD 或板级通过。

## 失效边界与必须重跑 Gate

候选改变了 `mem_test.xml`、`mem_test.peri.xml`、Hard SoC IP/wrapper、顶层和 BSP/`soc.h`；因此所有旧 G1/R0 UART0 的 bitstream、ELF、Map/PNR/STA/CDC、slack、warning、USER2、UART0 和 APB 记录均为 `HISTORICAL / NOT REUSABLE`，不得继承。本候选仍须以同一原子输入批次重跑并留存：

1. Efinity Map、Interface、PNR、STA（含 slack）、CDC，以及完整 warning inventory；
2. 匹配同批 bitstream 的 SHA-256/size 与构建日志；
3. 同批 `soc.h` 的路径、SHA-256、`SYSTEM_UART_1_*` 宏摘录及生成链；
4. 在 Goal 1 再审查获得 `APPROVE` 后，才允许 wsc 构建并提供匹配 `soc.h` 的 UART1 Hello ELF；
5. 仅当 bitstream、ELF、`soc.h`、输入 hash 与用户批准窗口全部匹配后，才可进入一次连续的 USER2 → UART1 Hello/echo → read-only APB MAGIC Gate。

UART2/J52、myCobot 接线、查询和动作始终不在本审查范围内。

## 解除阻塞所需补件

libaoxun 必须提供一个不可变证据位置（仓库脱敏索引可链接到外部原始材料），其中每条均带 batch ID、时间、执行者和候选完整 SHA：

1. Review Packet / build summary：明确纳入/排除文件、Efinity 版本与命令身份、候选 SHA、同批原子输入 SHA-256；
2. 原始 Map、Interface、PNR、STA、CDC 日志/报告和完整 warning 文件及处置；
3. bitstream SHA-256、size、生成时间，以及与上述原子输入/工具版本的绑定；
4. 同批生成的 `soc.h` 路径与 SHA-256、UART1 宏摘录，以及 `GPIOR_96/B12`、`GPIOR_100/D12`、115200 8N1 的工具可追溯证据；
5. 明确无旧 R0/UART0 bitstream、ELF、warning 或板级记录混入的声明及可复查依据。

在补件前，结论保持 `BLOCKED`；本记录不授权 wsc 构建、qzs 集成、Efinity 运行、USER2、串口写入或 APB 操作。

## 审查命令与结果

- `tools/agent_handoff_health_check.ps1`：PASS。
- `git status --short --branch`、`git rev-parse HEAD`、`git branch --show-current`、`git ls-remote --heads origin`：已固定审查基线与候选 ref/SHA。
- `git diff-tree --name-status/-stat` 与 `git diff --name-status f47af290... 6effdc...`：15 个候选文件，均在 libaoxun 静态白名单内。
- `git diff --check 6effdc...^ 6effdc...`：PASS（无空白错误）。
- 对候选树的 `git show` / `git diff`：取得 UART1、GPIO、BSP 宏、顶层/wrapper 与 Hello 源码的静态事实；未运行 Efinity 或硬件操作。
- 本 Goal 审查记录的 `git diff --no-index --check`：PASS（无空白错误）。
- `tools/team_scope_check.ps1 -Role qzs -BaseRef 6d5e33a...`：WARN / 非本 Goal 阻塞。脚本会检查整个共享工作区；审查开始后的检查发现 `ppt_doc_outlines/defense_ppt_outline.md` 与 `ppt_doc_outlines/technical_document_outline.md` 不在 qzs 白名单内，故全局返回 FAIL。两者不在本 Goal 的允许写入范围，未被移动、删除或修改；本记录自身位于 `docs/`，属于 qzs 白名单。
