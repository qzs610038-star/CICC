# qzs Goal 2 — wsc classifier / Host / UART1 Hello 审查记录

> 审查时间：2026-07-18（Asia/Shanghai）
> 审查性质：只读；未合并、未切换候选、未修改 CPU/测试/硬件 ABI、未执行 JTAG/串口/机械臂操作。
> **唯一结论：BLOCKED**

## 1. 固定输入与现场

| 项目 | 固定事实 |
|---|---|
| qzs 审查分支 / HEAD | `codex/qzs-final-integration-goals-20260718` / `6d5e33a2b188abac2fbc5e36dab3155eba45d4f2` |
| 开始时 dirty | `?? docs/agent_context/QZS_TASK_BREAKDOWN_AND_STRONG_GOALS_20260718.md`（既存、保留） |
| wsc 远端权威 ref | `dev/wsc6090-uart1-cpu-20260719` |
| wsc 固定 SHA | `35b074c385fa34a588f7168307f360c1f38d7152` |
| SHA 发现方法 | `git ls-remote --heads origin`；未依赖本地 `origin/dev/wsc*`。远端仍有 `dev/wsc6090-CPU` 与 `dev/wsc6090-cpu` 大小写冲突，未迁移 refs 后端。 |
| 审查基线 | `f47af290c2f014dfa8a131a3baebec1e9560ae21` |
| handoff health | PASS；当前分支/HEAD/dirty 与上表一致。 |
| `git fetch --prune origin` | WARN：受限环境拒绝写 `.git/FETCH_HEAD`（exit 255）；随后只按 `ls-remote` 全 SHA 定位，并只抓取对应对象审查。 |

## 2. 候选范围裁决

`f47af290...35b074c` 的完整文件差分仅有以下两项，均属于 wsc 白名单 `competition_project_single_camera/cpu/tests/**`：

| 路径 | 范围 | 审查结果 |
|---|---|---|
| `competition_project_single_camera/cpu/tests/test_single_camera_classifier.c` | IN_SCOPE | C4127 修复：将 `CHECK` 的 `do { ... } while (0)` 宏替换为 `record_check()` 函数加单表达式宏。 |
| `competition_project_single_camera/cpu/tests/run_g2_host_evidence.ps1` | IN_SCOPE | 仅增加 GCC 可用时的严格 `-Wall -Wextra -Werror` 分支；VS 路径仍保留 `/std:c11 /W4 /WX`。 |

没有冻结头文件、状态/治理文档、RTL、XML、SDC、IP、BSP、myCobot 或 `cpu_bringup/uart1_hello_onchip/**` 的候选差分。因此没有可纳入的 UART1 Hello 实现或制品。

## 3. 已验证的 CPU Host 事实

在隔离临时目录从固定 WSC SHA 解包并实际运行；机器无 GCC，所有四项使用 VS2022 `cl.exe 19.42.34436`。Host runner 的严格选项为 `/std:c11 /W4 /WX`，每项可执行文件均实际运行并返回 0。

| 入口 | 本轮真实结果 | exit code |
|---|---:|---:|
| `run_single_camera_classifier_host.ps1` | `54/54 passed` | 0 |
| `run_single_camera_f1_host.ps1` | `213/213 passed` | 0 |
| `run_single_camera_feature_adapter_host.ps1` | `33/33 passed` | 0 |
| `run_g2_host_evidence.ps1 -RunDir <temp>` | runtime `648/648 passed`；bundle `VALIDATION_PASS` | 0 |

G2 manifest 记录 `assertions_passed=648`、`assertions_failed=0`、`arm_disabled=true`、`arm_request_count=0`、`arm_send_count=0`、`source=host_fixture_or_fake_transport`、`risc_v_elf_not_built=true`、`board_not_verified=true`。历史 `182/182`、`921/921` 没有被用作本轮结论。

### C4127 审查

修复只改变测试断言包装方式：`record_check()` 仍对每次 `CHECK(expr)` 计数、保留失败输出，并将条件转为 `!!(expr)`；没有降低 `/W4` 或 `/WX`、没有禁用 warning、也没有跳过可执行文件。上述 MSVC 严格构建及 `54/54` 实跑证明该候选关闭了此前的 C4127 阻塞。

### ARM / myCobot 边界

runtime 的实跑 bundle 和测试均维持 `ARM_ENABLED=0`。候选范围不含 UART2、J52 或 myCobot transport 代码；本轮没有初始化真实 transport，且没有发送 myCobot 帧。该 Host 证据不构成 UART2/J52、myCobot、板上 CPU、OSD 或 APB PASS。

## 4. UART1 Hello / Goal 1 批次核验

此项不能通过，原因均为可复查的缺失事实：

1. 候选 `35b074c...` 没有 `cpu_bringup/uart1_hello_onchip/**` 差分；目录只有 `SCAFFOLD ONLY / GENERATED SOC.H REQUIRED / NOT BUILT / BOARD NOT VERIFIED` README。
2. 候选 `soc.h` 只有 `SYSTEM_UART_0_*`，包括旧 `SYSTEM_UART_0_IO_CTRL=0xe8010000`；`SYSTEM_UART_1_*` 仅出现于冻结说明/骨架 README，未出现于生成的 `soc.h`。片上 RAM 宏仍为 `SYSTEM_RAM_A_CTRL=0xf9000000`、`SYSTEM_RAM_A_CTRL_SIZE=0x4000`，但没有同批 UART1 硬件身份可供绑定。
3. 本轮未提供或在远端发现 Goal 1 `APPROVE` 的 libaoxun UART1 原子 SHA、batch ID、生成 `soc.h`、Efinity 构建摘要、Hello 构建命令、Hello ELF SHA-256、ELF 大小/入口/LOAD 段布局。远端可发现的 `dev/libaoxun688@bedd14847aedca09cc331709ce68be0cb3fd1735` 同样只有 UART0，不能冒充 Goal 1 输入。
4. 因而不存在可审计的新 Hello ELF，也不能确认其只引用 `SYSTEM_UART_1_*`、位于同批片上 RAM、未猜 IRQ、未复用 UART0/R0 ELF。

## 5. Findings

### P0

- **Goal 1 批次及 UART1 Hello 制品缺失。** 没有可固定的 Goal 1 APPROVE libaoxun SHA/batch ID/生成 `soc.h`，WSC 候选也没有 Hello 源码、构建记录或 ELF。继续把该 SHA 送入 Goal 3 会失去 CPU 与 Hard SoC ABI 的原子绑定，故阻断。

### P1

- **不得将候选的 UART0 `soc.h` 解释为 UART1 输入。** 当前生成头文件仍定义 `SYSTEM_UART_0_*`，而不是 `SYSTEM_UART_1_*`；任何基于 `0xe8010000`、旧 IRQ 或旧 ELF 的 Hello 都违反 I0 冻结边界。

### P2

- **G2 runner 的 GCC fallback 不削弱本机 MSVC 严格门。** 本机无 GCC，实际走 VS2022 `/W4 /WX`；未来安装 GCC 时，新增分支仍使用 `-Wall -Wextra -Werror`。这不是阻断项，但后续仍应在交付证据中记录实际编译器。

## 6. 结论与后续固定输入

**BLOCKED。** C4127 与四项 Host/G2 回归已在固定 WSC SHA 上真实通过，且 ARM/myCobot 仍为 fail-closed；但 Goal 2 的 UART1 Hello 与 Goal 1 同批 ABI核验没有任何可审计输入或制品，不能给出 APPROVE，也不能为 Goal 3 固定 WSC+硬件组合。

恢复审查至少需要：

1. Goal 1 的 `APPROVE` 审查记录，包含 libaoxun 远端 ref、完整 SHA、batch ID、生成 `soc.h` 路径与 SHA-256；
2. 该批 `SYSTEM_UART_1_*` 宏、RAM 合法范围和 Efinity 原子输入/bitstream 身份；
3. wsc 新固定 SHA（或同一 SHA 的补充、不可变证据包），包含 UART1 Hello 源码、完整构建命令、所有输入 SHA、ELF SHA-256、大小、入口和 LOAD 段布局；
4. 明确 `ARM_ENABLED=0`、无 UART2/J52 transport 初始化、无 myCobot 帧的构建/运行证据。

在上述缺口关闭前，Goal 3 没有可用的 WSC APPROVE 输入；禁止合并候选，UART1/APB/OSD/板级以及 UART2/J52/myCobot 状态继续为 `NOT VERIFIED` / `NO-GO`。

## 7. 本 Goal 文档自检

- `git diff --check`：PASS（exit 0）。
- `git diff --no-index --check`（空文件对本记录）：复验无空白诊断；exit 1 为 `--no-index` 的“文件不同”预期返回，不是检查失败。
- `tools/team_scope_check.ps1 -Role qzs -BaseRef 6d5e33a2b188abac2fbc5e36dab3155eba45d4f2`：PASS（exit 0）；既存 Goal 0 文档与本记录均在 `docs/**`，不扩大 qzs 范围。
