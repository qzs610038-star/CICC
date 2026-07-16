# main 合并记录：@libaoxun688 Hard SoC 真源同步

## 身份与范围

- 合并日期：2026-07-17（Asia/Shanghai）
- 合并前 `main`：`24e39833f3a76f3f59ca3cb9bebc0d2fa14f3cba`
- 候选来源：`dev/libaoxun688-hard-soc-source-sync-20260716`
- 固定来源 SHA：`14b924866f9df8a27e65f9719c285d23b3b8fa7e`
- 裁决领域：FPGA/Hard SoC/IP/工程 XML/顶层连接/APB，按 `@libaoxun688` 的已批准固定提交优先。

## 实际纳入

完整保留同一批的 Hard SoC 原子集合：`mem_test.xml`、`mem_test.peri.xml`、`src/top.v`、`src/apb_reg_magic.v`、APB testbench、`ip/EfxSapphireHpSoc_slb/**` 与生成 BSP `soc.h`。该集合启用 APB0 并在 BSP 中定义 `IO_APB_SLAVE_0_INPUT=0xE8100000`；`apb_reg_magic` 的合法只读值为 `0x375A0001`。

## 明确舍弃

未纳入来源分支的以下 7 份状态性/交接文档改写：`CURRENT_STATE.md`、`competition_project_single_camera/WORK_LOG.md`、`competition_project_single_camera/docs/README.md`、`docs/debug_sessions/hard_soc_board_config.md`、以及 3 份 M2 review/handoff 文档。

原因是这些文档混入了当前批次的 Efinity/JTAG/HDMI/CPU/UART0 PASS 叙述，但本次合并未附其原始 outflow、工具输出或板测原始记录，且部分表述与本仓库现行“合并后新构建必须重新验证”的 Gate 冲突。舍弃的是未经本次独立复核的状态结论，不是 Hard SoC 代码、IP 或配置真源。

## Findings

- P0：无文本冲突；`git merge-tree --write-tree` 返回 0。
- P1：硬件生成输入发生变化，旧 bitstream、ELF、Map/PNR/STA/CDC 与板级证据不能继承；当前本机 `EFINITY_RISCV_IDE` 为空，UART0 Hello 构建无法启动，提示缺少 `make.exe`。
- P2：APB testbench 已随源码纳入，但本机没有 Icarus/Verilator/ModelSim/Questa 的可用验证命令，仿真未运行。

## 本次实际验证

- `tools/agent_handoff_health_check.ps1`：通过；合并前 `main` 为 `24e3983`、工作区干净。
- `git ls-remote`：来源 ref 与上述 SHA 一致。
- `git merge-tree --write-tree HEAD 14b9248...`：退出码 0，无文本冲突。
- `mem_test.xml`、`mem_test.peri.xml`：PowerShell XML 解析通过。
- 静态锚点：`top.v` 实例化 `apb_reg_magic`，BSP 含 `IO_APB_SLAVE_0_INPUT=0xe8100000`。
- `git diff --cached --check`：通过。
- UART0 Hello：未构建；本机未配置 Efinity RISC-V 工具链，脚本在寻找 `make.exe` 时安全失败。

## 合并后结论与下一 Gate

新结论仅为：当前 `main` 已有一套完整、不可拆分的 APB0 Hard SoC 源码配置，CPU 软件后续必须适配该 BSP/硬件 ABI。它不证明离线构建、bitstream、JTAG、USER2、CPU、UART0 或 APB 实读通过。

下一 Gate 由 `@libaoxun688` 在已配置 Efinity 环境从本次 `main` 重建，提交脱敏 Map/PNR/STA/CDC、warning、bitstream/ELF hash；之后只按 USER2 + 片上 RAM + UART0 115200 进行隔离板级验证。禁止 USER1、Flash、外部 DDR、UART2/J52 和任何机械臂接线或动作。
