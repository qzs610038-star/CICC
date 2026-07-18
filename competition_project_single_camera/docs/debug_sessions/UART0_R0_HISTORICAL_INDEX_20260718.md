# UART0 / R0 历史证据索引

> 状态：`HISTORICAL ONLY / SUPERSEDED BY I0 UART1 DECISION`
> 决策日期：2026-07-18

用户与 wsc、libaoxun、qzs 已确认：I0 改用 SoC UART1 → 板载 Type-C UART1；
UART0 被烧录器链路占用，不再作为活动 CPU 生命证明接口。

以下内容保留原文，只证明当时 UART0/R0 批次的构建或排障过程：

- `r0_current_batch_manifest_20260718.json`
- `R0_CURRENT_HEAD_COLD_BUILD_REVIEW_PACKET_20260717.md`
- `G1_USER2_UART0_BOARD_GATE_REVIEW_PACKET_DRAFT_20260717.md`
- `g1_user2_uart0_board_operator_card_20260717.md`
- `evidence/r0_uart0_ch340_passive_listen_20260718.md`
- `evidence/r0_uart0_synchronized_banner_blocker_20260717.md`
- `cpu_bringup/uart_hello_onchip/**`
- `tools/capture_g1_uart0_banner.ps1`、`tools/capture_m2_uart0_banner.ps1`

不得把这些文件批量替换为 UART1；那会篡改历史证据。它们的 bitstream、ELF、
COM口和 0-byte 结果均不得继承到新 UART1 批次。
