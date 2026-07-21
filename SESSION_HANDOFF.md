# SESSION_HANDOFF — CPU Hello UART1 候选

## 恢复入口

- 候选分支：`codex/qzs-wsc-libaoxun-cpuhello-candidate-20260721`。
- 当前纳入来源：`origin/codex/qzs-wsc-libaoxun-integration-20260718@0e5ab490559c58642734b0095753c6cf8787c709`。
- P1 来源已保留在本候选历史中；Host 回放与 tamper 资产不得被 UART1 接入覆盖。
- 合并登记仅在实际进入 `main` 后写入 `docs/merge_governance/MERGE_REGISTER.md`。

## 当前路线与替代关系

- 当前执行路线：SoC UART1 → Type-C UART1，`115200 8N1`，RX=`GPIOR_96/B12`，TX=`GPIOR_100/D12`，片上 RAM=`0xF9000000..0xF9003FFF`。
- 当前端口：`COM17` 为 Type-C UART1 CH340；`COM10`、`COM13` 为 J44/USER FTDI，禁止作为 UART1 候选。
- 当前 H3：TAP ID=`0x006A0EF3`，BSCAN=`6,1`，inner IR=`9`。
- UART0/G1/R0、旧 COM、旧 inner IR=`8`、旧 CPU TAP=`0x006A0A79` 和旧批次结论均为 `HISTORICAL / SUPERSEDED`；保留原始 FAIL_STOP 索引，不得继承为当前 PASS。

## 状态边界

- I0-BUILD、H0、H1、H2、H3、H4、H5、APB MAGIC、I1/APB/CDC、CPU→OSD、UART2/J52/myCobot 必须逐层记录，任何前层不得替代后层。
- 仅 H4/H5 完整闭合后可写 CPU Hello PASS；H5 不授权 APB，APB 不授权 I1、OSD 或机械臂。
- 当前固定状态见 `CURRENT_STATE.md`：`CPU_HELLO=WAITING_LIBAOXUN_FIXED_SHA`、`BOARD_PASS=NOT_CLAIMED`、`APB_MAGIC=NOT_VERIFIED`、`I1_APB_CDC=NOT_VERIFIED`、`CPU_TO_OSD=NOT_VERIFIED`、`UART2_J52_MYCOBOT=NO_GO`。

## 下一执行者

1. 先读取 `CURRENT_STATE.md`、`competition_project_single_camera/docs/review_packets/CPU_HELLO_UART1_H0_H6_RECEIPT_20260721.md` 和 `docs/agent_context/LIBAOXUN_UART1_NEXT_STAGE_HANDOFF_20260721.md`。
2. 等待 libaoxun 提供完整 SHA、父提交、改动文件、同批 SHA-256、H0–H5 原始证据索引和禁止项声明。
3. 只按固定 SHA 审查；合并后执行 fresh 门禁，更新本文件和 `CURRENT_STATE.md`，由用户决定提交与推送。
