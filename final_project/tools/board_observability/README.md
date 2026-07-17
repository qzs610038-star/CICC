# G2 offline board-observability tools

`g2_run_bundle.py` creates and validates an offline evidence bundle from Host/fake-transport `@E|v=1|...` lines. It does not enumerate, open, read, or write any COM/serial device; it does not invoke a programmer or emit myCobot data.

Run the Host evidence path from the dedicated worktree:

```powershell
.\competition_project_single_camera\cpu\tests\run_g2_host_evidence.ps1 -RunDir D:\CICC-g2-run-bundle-<id>
```

The output directory contains `manifest.json`, preserved `raw.log`, parsed `events.jsonl`, and `summary.md`. Validation is fail-closed for malformed records, missing BOOT/ACK, ACK mismatch, sequence errors, duplicate results, mixed hashes, and any non-zero ARM state.

The negative fixture regression is also fully offline:

```powershell
python -m unittest final_project/tools/board_observability/tests/test_g2_run_bundle.py -v
```

This is `OFFLINE OBSERVABILITY L0` only. It does not prove FPGA APB, RISC-V ELF, board UART, or any mechanical-arm capability.
