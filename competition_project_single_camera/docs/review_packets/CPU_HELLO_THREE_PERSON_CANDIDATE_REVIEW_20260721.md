# CPU Hello 三人集成候选审查包（2026-07-21）

## 结论

本提交是供三人继续集成的 **本地候选**，尚未推送、尚未合入 `main`。
它只引入可离线复现的 CPU/Host P1 模型与回归工具；不宣称 CPU Hello、UART0、APB、OSD、Hard SoC、bitstream 或板级通过。

## 固定来源与范围

| 项目 | 固定值 |
|---|---|
| 审查基线 | `main` / `9acf4d8b2ec788ccd5777f3833a7bfb756c51cad` |
| 候选提交 | `eb76110a2aa336d4da7daf9feea6368b7b4c9521` |
| CPU/Host 来源 | `codex/qzs-wsc-p0a-p1-integration-20260720` / `40b42dd` |
| 已在基线的 G2 资产 | `run_g2_host_evidence.ps1`、运行时/transport 与 `final_project/tools/board_observability/`；未重复摘取 |
| 历史三人线 | `0e5ab490` 的可用内容已被 `40b42dd` 覆盖，不作为新基线 |

纳入：P1 Host 模型、P1 回放生成及验证、P1 向量/模式文件、特征适配器的增量测试，以及让 Host 回归同时兼容 gcc 与本机 MSVC 的通用编译器封装。

## 明确排除

- `mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、顶层 RTL、`soc.h`、`ip/EfxSapphireHpSoc_slb/**` 与其他 Hard SoC 原子配置；
- 历史 `CURRENT_STATE.md`、旧构建/板级结论、旧 outflow、ELF、bitstream、原始日志；
- I0/UART1 的旧执行清单、JTAG/APB 执行器和任何历史固定 SHA-256 白名单；
- 机械臂控制、接线、电平、动作或串口命令。

P1 回放只在新建临时目录中对本次生成的四个文本文件计算归一化 hash，用于发现同次回放被篡改；这些临时产物及 hash 均未提交，也不作为 CPU Hello 或硬件 Gate 的前置白名单。

## 本地验证

| 命令/检查 | 结果 |
|---|---|
| `run_p1_host_model.ps1` | `p1_host_model: 37/37 passed` |
| `run_single_camera_feature_adapter_host.ps1` | `single_camera_feature_adapter: 39/39 passed` |
| `run_g2_host_evidence.ps1` | `single_camera_runtime: 182/182 passed`，`VALIDATION_PASS: offline bundle` |
| `run_p1_replay_bundle.ps1` | 20 轮；`P1_VECTOR_SCHEMA=PASS`、`P1_REPLAY_SCHEMA=PASS`、`P1_TAMPER_NEGATIVE=PASS`、`ARM=0` |
| `test_p1_replay_manifest_verifier.ps1` | 当前生成包 hash 校验通过，篡改 `rounds.jsonl` 被拒绝 |
| `git diff --check` 与排除清单扫描 | 通过；未发现被排除的 Hard SoC/I0/JTAG/历史状态路径 |

## libaoxun CPU Hello 成功后的最短接入动作

1. 在 libaoxun 的工作分支提交 CPU Hello 的源码、构建输入与本次运行证据，固定完整 SHA；若 Hard SoC 输入改变，按原子批次一并提交并重新声明旧板级证据失效。
2. 将该固定 SHA 和实际纳入/排除范围交给本候选分支审查；不要以分支名或旧 hash 清单代替本次产物身份。
3. 仅对 CPU Hello 所需文件进行定点摘取和冲突审查，随后重跑 Host 回归；UART0、APB、OSD 与板级 Gate 仍各自独立。
4. 经审查后才推送本候选为共享三人集成分支，供队友拉取；合入 `main` 前再写合并登记与当前状态。

## 未验证项与风险

- P1 的模型/回放是 Host 证据，不能证明板上 CPU 已取指或 UART0 已输出 Hello。
- libaoxun 若改变 XML、IP、顶层、约束、BSP 或硬件 ABI，必须在其批准的 Hard SoC 原子批次下重新适配 CPU 代码；本候选不能反向覆盖硬件事实。
- 当前候选未推送，队友暂时不能拉取；推送是后续独立、可审查的协作动作。
