# libaoxun UART1 Next-Stage Handoff

## Pull point

After qzs commits and the user authorizes a push, libaoxun pulls `codex/qzs-wsc-libaoxun-cpuhello-candidate-20260721` and verifies its fixed commit intake against `competition_project_single_camera/docs/review_packets/CPU_HELLO_UART1_H0_H6_RECEIPT_20260721.md`.

## Allowed next board gate

Only the staged H0 → H1 → H2 → H3 → H4 → H5 CPU Hello route may be evaluated. H4/H5 remain `DRAFT / NOT AUTHORIZED` until the user authorizes the corresponding board window and all preceding receipts are closed.

## Continuing prohibitions

- No APB MAGIC claim or probe under this handoff.
- No I1/APB/CDC, CPU→OSD, UART2/J52 or myCobot expansion.
- No Flash, DDR, USER1, UART0, `COM10` or `COM13` as a UART1 candidate.
- No rewriting historical FAIL_STOP records as PASS.

## Return package

Return only the fixed SHA, receipt-table fields, sanitized evidence summary and relative raw-evidence index. qzs then performs the fixed-SHA review and fresh post-merge gates.
