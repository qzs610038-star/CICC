# libaoxun UART1 Next-Stage Handoff

## Pull point

libaoxun first returns an independently created fixed SHA with the H0–H5 receipt package. qzs then reviews and merges that exact SHA, reruns fresh gates, and pushes the resulting candidate HEAD. Only after that result HEAD is published may libaoxun pull `codex/qzs-wsc-libaoxun-cpuhello-candidate-20260721` for the subsequently authorized independent board stage.

## Allowed next board gate

Only the staged H0 → H1 → H2 → H3 → H4 → H5 CPU Hello route may be evaluated. H6 is only a minimal non-destructive failure classification when an earlier stage fails; it is `NOT_INVOKED` when H0–H5 close. H4/H5 remain `DRAFT / NOT AUTHORIZED` until the user authorizes the corresponding board window and all preceding receipts are closed.

## Continuing prohibitions

- No APB MAGIC claim or probe under this handoff.
- No I1/APB/CDC, CPU→OSD, UART2/J52 or myCobot expansion.
- No Flash, DDR, USER1, UART0, `COM10` or `COM13` as a UART1 candidate.
- No rewriting historical FAIL_STOP records as PASS.

## Return package

Return only the fixed SHA, receipt-table fields, sanitized evidence summary and relative raw-evidence index. qzs then performs the fixed-SHA review and fresh post-merge gates.
