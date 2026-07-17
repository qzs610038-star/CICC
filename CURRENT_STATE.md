# CURRENT_STATE — 当前状态快照

> 状态基线：`main@489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f`（G3 固定基线，2026-07-17）。
> 真实源码、工程 XML、构建日志和板上现象是最终事实；本文件仅索引当前结论、阻塞和下一 Gate。
> 稳定架构与安全红线见 [AGENTS.md](AGENTS.md)，官方任务见 [0710 比赛细则](final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md)。

## CURRENT_SNAPSHOT

### 当前 main / SHA

- 固定状态基线：`489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f`。
- `G1_G2_VERIFIED_PATCHES_IMPORTED`：G1/G2 源 worktree 均没有新增 commit；经独立核验的补丁已导入本 G3 worktree，但仍是未提交整合，不得称为 Git merge 或已进入 `main`。
- 本快照描述“固定 SHA + 当前 G3 未提交整合补丁”；G1/G2 源 worktree 的其他 dirty 内容和外部产物不能自动成为事实。
- `POST_MERGE_REFRESH_REQUIRED`：审核者形成最终 commit/合并后，必须基于结果 HEAD 重跑 freshness/context budget，更新 manifest 的 `last_verified_commit` 并复核本快照和 Review Packet；本轮不得预写未来 SHA。

### 单摄 FPGA / Hard SoC

- `competition_project_single_camera/` 是隔离候选，不替代 `final_project/` 正式主线。
- 当前 SHA 已包含 Hard SoC/APB0/IP/XML/顶层、最小 BSP/UART0 Hello 和 DSI 相对路径修复的原子真源；不得再沿用“Hard SoC 源码缺失”旧结论，也不得拆分该原子批次。
- G1 已针对该固定 SHA 的原子输入完成 Efinity 2025.2 冷构建：Map、Interface、PNR、STA、CDC 与 bitstream 生成通过；STA 的 WNS/WHS 为正，warning 保持分类记录。匹配 bitstream SHA-256 为 `A897E33514A1079BB1B46C02C464B0BD679AF551CEFCB67C4A0EBD5B8FCD1ACD`。
- UART0 Hello 已完成冷构建和静态 LOAD/入口审计；匹配 ELF SHA-256 为 `E5BC80A2F18A7E2951D53DA539BE2FC61AAECFA90C5CDADB29E65FFC6141928A`，LOAD 范围位于 `0xF9000000..0xF9000A30`。这些仅是离线构建证据，不是板级 PASS。
- `USER2`、CPU 实际取指、UART0 115200 bps 横幅/回显和 APB 实读仍均为 `NOT VERIFIED`。旧批次 bitstream、ELF、slack、CDC、warning 和板级记录仍不得继承；操作前须重新核对上述匹配 hash。
- 证据索引：[G1 冷构建 Review Packet](competition_project_single_camera/docs/review_packets/G1_EFINITY_COLD_BUILD_REVIEW_PACKET_20260717_0307.md)、[合并记录](docs/merge_governance/MERGE_REGISTER.md)、[单摄候选目录](competition_project_single_camera/)。

### CPU 决策

- 板上 CPU 负责五色、三形状、三尺寸分类、四任务关系判断、逐轮状态机、参数和 myCobot 控制；Host/mock 或 compile-only 结果不等于板级闭环。
- G2 已导入 Host/fake transport 运行时缝、结构化事件和离线 L0 bundle；当前复测为 C Host `182/182`、Python `3/3`。MMIO transport 对未定版地址保持 fail-closed，机械臂命令保持禁用。
- 上述计数只证明 G2 离线负例/顺序/ACK 校验，不证明 RISC-V 固件链接、真实 MMIO/APB、板级目标输入、逐轮事务或动作闭环。
- 证据索引：[G2 CPU 可观测性 Review Packet](final_project/docs/review_packets/g2_cpu_observability_review_packet_20260717_025111.md)、[CPU 模块计划](final_project/cpu/CPU_MODULE_PLAN.txt)、[决赛主方案](final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md)。

### APB / OSD

- APB0 候选 BSP 定义为 `IO_APB_SLAVE_0_INPUT=0xE8100000`，但当前 SHA 的 APB 实读仍未验证；不得以候选地址发起未经 Gate 的试探性 MMIO。
- OSD 契约保持“CPU 产生识别/判断/动作及理由语义，FPGA 渲染像素”；当前不得宣称 CPU→OSD 回写已板级闭环。
- 证据索引：[寄存器契约索引](final_project/integration/register_map.md)。

### myCobot

- 正式闭环必须由板上 CPU 控制；PC、`pymycobot`、myBlockly 仅用于开发期调试、标定、健康检查、日志和录像。
- UART0 Hello 固定 `115200`；myCobot 固定 `1000000`。两者是隔离 Gate，禁止混用或相互外推。
- 当前禁止 UART2/J52、机械臂接线、myCobot 帧和任何动作。任何动作必须先由用户明确确认目标、速度、角度范围、安全姿态与急停/断电方式，并通过 Codex Review Gate。
- 证据索引：[myCobot 板控计划](final_project/docs/technical_plans/mycobot_arm_board_control_advancement_plan_20260715.md)、[PC 历史归档](mycobot_pc_tests/)。

## CURRENT_BLOCKERS

1. `USER2`、片上 RAM CPU 取指、UART0 横幅/回显和 APB 实读尚未验证；G1 离线 PASS 不得升级为板级 PASS。
2. G2 真实 RISC-V/MMIO transport ABI、正式寄存器地址和板级证据尚未定版；不得从 Host/fake seam 推断硬件接口。
3. CPU→OSD、正式目标输入、板级逐轮事务和 myCobot UART2/J52 均未形成当前批次证据。

## NEXT_GATE

1. 在操作边界重新核对 G1 Review Packet、固定 SHA、匹配 bitstream/ELF SHA-256 和 warning 分类；任何不一致都使该批次失效并退回冷构建。
2. 仅允许使用上述匹配 bitstream、FPGA `USER2`，并仅下载匹配 ELF 到 `0xF9000000` 片上 RAM，验证 UART0 115200 bps Hello；记录 CPU 取指与串口原始证据。
3. 本 Gate 禁止 `USER1`、Flash 擦写/烧录、外部 DDR 初始化、UART2/J52、机械臂接线和动作；Hello 通过后必须另立 Review Packet 才能扩大范围。
4. G2 的真实 MMIO/APB transport 必须在板级基础 Gate 后另行审查，不得先行猜测地址或启用机械臂命令。

## PENDING_DECISIONS

- 板上参数掉电保存：追加受审存储驱动，或人工记录后重编译；未定版。
- `TARGET_SEL` / `LIVE_FG_AREA` 等正式地址与字段：须以同一次 SoC 生成物、`soc.h` 和接口 Review Packet 定版，当前历史占位值不是硬件事实。
- 任务三评分比例若现场口径与正文/表格不一致，须当场确认。

## DEPRECATED_ROUTES

- 纯 FPGA 视觉分类、四任务关系判断或 myCobot 状态机：废弃，不得恢复。
- PC/`pymycobot` 进入正式识别/控制闭环：禁止。
- HDMI 双摄透传保底：仅历史基础链路，除非用户重新指定，不是当前攻关目标。

## HISTORY_ARCHIVE_INDEX

- 原 `CURRENT_STATE.md` 已逐字归档至 [CURRENT_STATE_20260717_pre_g3.md](debug_records/state_history/CURRENT_STATE_20260717_pre_g3.md)。
- 归档 SHA-256、原行号映射、替代关系和失效条件见 [archive_manifest.md](debug_records/state_history/archive_manifest.md)。
- 历史正文只用于审计和追溯；回答当前 Gate 不得依赖阅读全文历史。
