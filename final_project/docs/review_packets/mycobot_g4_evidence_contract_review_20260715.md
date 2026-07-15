# myCobot G4 同批次证据契约 Review Packet

> 日期：2026-07-15
> 范围：G4 的证据一致性预检工具；不包含 SoC 生成、PNR、STA、烧录或板上 CPU Hello。
> 结论：**工具准备 PASS；G4 仍为 NOT VERIFIED。**

## 背景

`mycobot_arm_board_control_advancement_plan_20260715.md` §13.4 要求在 G4 后形成同批次 SoC/BSP/bitstream/ELF、PNR/STA 与 3 次 CPU Hello。历史 PNR 计数属于不同批次，不能拼接为 G4 证据；因此需要先有一个 fail-closed 的批次校验器。

## 新增内容

- `final_project/tools/verify_mycobot_g4_batch.py`
- `final_project/tools/tests/test_verify_mycobot_g4_batch.py`

manifest schema 为 `mycobot-g4-batch-v1`，要求：

1. `build_id`、commit、Efinity 版本、`arm_bringup` profile 和 `disabled` 或 `simulated` backend；
2. current project XML、periphery XML、SDC、top、`soc.h`、linker、startup、ELF、bitstream、map/PNR/STA 日志的路径与 SHA-256；
3. map/PNR/STA 原始日志中的具体 PASS 断言；
4. 三份独立 reset UART0 日志，各自含 `CPU HELLO` 与同一 `build_id`；
5. BSP/startup/linker 中不得存在 `STANDALONE_TEST`、`0xF0000000` 或 `APB_VISION_BASE_PLACEHOLDER`。

工具只做文件与文本证据验证。它不证明 GUI 配置正确、不会替代原始 PNR/STA/Programmer 日志，也不会执行任何硬件操作。

为使三次 UART0 日志能与 manifest 关联，`arm_bringup_main.c` 现输出 `CPU HELLO build=<ARM_BUILD_ID>`。`build_arm_profile.ps1` 已向其仍为 `NOT_FOR_FLASH` 的 G0–G3 制品注入同 manifest 的 id；一次目标 RISC-V disabled 构建 exit 0，且 `20260715_165504_64260b6` 在 ELF 原始字节中存在。此回归仅证明日志标识可编译/嵌入，不能替代 G4 的三次实机复位。

## 验证

```text
python -m py_compile final_project\tools\verify_mycobot_g4_batch.py \
  final_project\tools\tests\test_verify_mycobot_g4_batch.py
python -m unittest final_project.tools.tests.test_verify_mycobot_g4_batch -v
exit=0
2 tests PASS:
- 一致的完整模拟批次 -> PASS
- 混批 hash + provisional BSP -> FAIL
```

## G4 收证命令

```text
python final_project\tools\verify_mycobot_g4_batch.py \
  --manifest <G4 批次 manifest.json> \
  --report final_project\docs\debug_sessions\evidence\mycobot_g4_batch_verification.json
```

仅当命令 exit 0，并且 Codex 逐项回看 GUI、PNR、STA、Programmer 与三次 UART0 原始日志后，才可写 G4 PASS。

## 未关闭门

- A0 GUI 资源合法组合；
- A1/A2 SoC/periphery/top/PNR 根因与修复；
- A3 真正的同批次 SoC/BSP/ELF/bitstream、PNR/STA、Programmer 与三次 CPU Hello；
- G5–G11 全部仍未开始。
