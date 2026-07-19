# Goal 4 临时所有权登记 — 2026-07-19

## 生效事实与总边界

- 远端交接记录为 `codex/qzs-wsc-libaoxun-integration-20260718@fd3fc0881d4e71338f1aa34f361cd498b7cd2d4c`；
  两人的独立个人补丁基线仍固定为 `182fd6f5c4d628379760d6f4fc74e3b342e30083`。读取登记与建立补丁
  使用不同 SHA，不得把 qzs 治理提交并入个人补丁。
- `a840f0869c11bab0915757d64c56a167f6d4f917`（WSC）与
  `5a61c4cb2932ba1bd1eb86687563eb89d8823845`（libaoxun）均**不是** `182fd6f` 的祖先。
  它们只能作为待拆解的设计输入，禁止直接 merge、rebase、cherry-pick 整条链或将其称为
  已集成内容。
- 本登记只定义未来独立补丁的精确写入面和回收点；不构成“开始修改”指令，不改变冻结接口，
  不授权硬件、JTAG、USER2、串口、APB、Flash、DDR、UART2/J52 或机械臂动作。
- 若后续工作开始，必须从干净 `182fd6f` 建立各自个人分支，提交只包含下表允许文件；每个
  commit 都要通过相应 `team_scope_check`，并在 qzs 固定 SHA 审查后才可进入集成候选。

## WSC 临时范围（来源保真）

| 临时写入者 | 精确允许文件 | 用途 | 回收点 |
|---|---|---|---|
| WSC | `competition_project_single_camera/embedded_sw/apb_magic_onchip/APB_PROBE_DEBUGGER_CONTRACT.md` | 定义只读 APB probe 的 CPU 调试契约 | WSC 从 `182fd6f` 产生单一、可审查 SHA 后冻结；该文件的日常维护归还 libaoxun，但来源必须保留 WSC SHA |
| WSC | `competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.contract.txt` | 仅文本契约摘要，不含可执行制品 | 同上 |
| WSC | `competition_project_single_camera/embedded_sw/apb_magic_onchip/verify_apb_probe_contract.ps1` | 契约的静态/Host 校验器 | 同上 |
| WSC | `final_project/docs/review_packets/goal4_i2_apb_magic_probe_review_packet_20260719.md` | WSC Review Packet，必须保留 WSC 固定 SHA、命令和未验证项 | qzs 接收并固定 Packet 后，文档维护归还 qzs；WSC 作者/提交归属不得重写 |

WSC 不得写 `embedded_sw/apb_magic_onchip` 之外的 `embedded_sw/**`，不得提交 ELF、objdump、
readelf、symbols、undefined 或其他原始制品；不得把同名文件交给 libaoxun 复制后另立来源。

## libaoxun 临时范围（治理文件例外）

libaoxun 原有 `embedded_sw/apb_magic_onchip/{build_apb_probe_evidence.ps1,makefile,src/main.c}`
仍按既有 `embedded_sw/**` 所有权处理；本表只登记其跨入 qzs 文档/工具面的临时写入权。

| 临时写入者 | 精确允许文件 | 用途 | 回收点 |
|---|---|---|---|
| libaoxun | `competition_project_single_camera/docs/debug_sessions/I0_UART1_CLEANLF_USER2_OPERATION_CARD_20260719.md` | 更新为只读预检/停止条件，不声明硬件 PASS | qzs 接收固定 SHA 后归还 qzs |
| libaoxun | `competition_project_single_camera/docs/review_packets/I0_UART1_CLEANLF_USER2_EXECUTION_CONFIG_REVIEW_20260719.md` | 固定制品、工具与证据 root 边界 | 同上 |
| libaoxun | `competition_project_single_camera/tools/capture_i0_uart1_raw.ps1` | 仅审查/捕获工具；不得执行硬件动作 | 同上 |
| libaoxun | `competition_project_single_camera/tools/i0_uart1_cleanlf_ram_halt.gdb` | 受审 PC/RAM 观察脚本 | 同上 |
| libaoxun | `competition_project_single_camera/tools/i0_uart1_cleanlf_user2.cfg` | USER2 配置身份文件；不等于允许 USER2 | 同上 |
| libaoxun | `competition_project_single_camera/tools/i0_uart1_execution_manifest.json` | 执行前身份 manifest | 同上 |
| libaoxun | `competition_project_single_camera/tools/i0_uart1_wsc_apb_probe.gdb` | 只读 APB probe 脚本 | 同上 |
| libaoxun | `competition_project_single_camera/tools/i0_uart1_wsc_probe_gate.ps1` | APB probe Gate 静态校验器 | 同上 |
| libaoxun | `competition_project_single_camera/tools/verify_i0_uart1_execution_config.ps1` | 静态执行配置校验器 | 同上 |
| libaoxun | `competition_project_single_camera/tools/wsc_i0_apb_probe_contract.json` | WSC 契约消费摘要；须链接 WSC SHA | 同上 |
| libaoxun | `competition_project_single_camera/tools/run_i0_uart1_execution_chain.ps1` | 唯一 host orchestrator；只允许离线实现和 mock fixture，负责 `CAPTURE_READY → resume marker` 与 APB timeout/halt/reason/PC Gate | qzs 收口后归还 qzs；硬件模式仍需独立用户授权 |
| libaoxun | `final_project/docs/review_packets/goal4_i2_apb_magic_probe_review_packet_20260719.md` | 只能追加 libaoxun 证据引用；不得覆盖 WSC 来源段落 | qzs 收口后归还 qzs；WSC 段落保留 WSC 提交归属 |

以下旧提交内容**不在临时授权内**：

- 删除 `competition_project_single_camera/tools/i0_uart1_cleanlf_apb_read.gdb`；必须保留，除非
  qzs 在独立 Packet 中确认等价替代和回退方法。
- `apb_magic_onchip/artifacts/apb_magic_onchip.elf` 及 objdump/readelf/symbols/undefined/SHA
  原始制品；按仓库制品红线留在证据主机，以相对索引和 hash 交付，禁止直接提交。
- 任何冻结接口、RTL/XML/SDC/IP/BSP、CPU 业务源码、`CURRENT_STATE.md`、`SESSION_HANDOFF.md`
  或机械臂控制文件。

## 收口条件

1. 两人各自提交基于 `182fd6f` 的最小个人补丁，而不是迁移 `a840f08`/`5a61c4c` 整链；
   每份 Packet 逐项引用本登记文件。
2. qzs 复核 diff、来源 SHA、`team_scope_check`、EOL、制品排除与 WSC 段落保真后，正式回收
   临时权限：embedded 契约日常维护归 libaoxun，review/state/governance 文件归 qzs。
3. 任一人超出精确路径、试图携带禁止制品、删除旧 GDB、重写 WSC 来源或触及冻结接口，立即
   `TEMP_SCOPE_BLOCKED`；该临时权限不自动扩大。

## 当前派工状态

WSC 已固定 `48548f47dfa5964b13aed7edf3b3e9da6f6583a2`，四个临时文件与 fail-closed Host 合同通过
qzs/Codex 静态复核。libaoxun 的 `2d713b80a41185e472837abaec3a10c01383c70f` 仅接受为
`BLOCKER_SNAPSHOT`：USER2 OpenOCD 参数语义、host timeout/resume-marker orchestrator 与原始 82/21 evidence
尚未关闭。允许 libaoxun 在同一分支追加静态修复提交；禁止硬件、JTAG、OpenOCD、GDB、串口或 APB 会话。
