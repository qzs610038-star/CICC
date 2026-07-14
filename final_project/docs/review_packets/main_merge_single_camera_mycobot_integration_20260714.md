# 2026-07-14 main 双分支集成 Review Packet

> 结论：`PASS WITH GATES`。两条目标分支已合入本地 `main`；软件与静态工程清点通过，但单摄候选板级复现、正式 SoC/PNR、UART2 和机械臂动作仍为 `NOT VERIFIED / NO-GO`。

## 1. 集成范围

| 顺序 | 来源 | 来源提交 | 本地合并提交 | 结论 |
|---|---|---|---|---|
| 1 | `origin/codex/mycobot-g0-g3-bringup-20260714` | `b48973b` | `81657c4` | 合入 G0-G3 Host/QEMU/NOT_FOR_FLASH 软件门 |
| 2 | `origin/dev/libaoxun688` | `edd5328` | `45c790c` | 合入隔离单摄候选工程并人工解决 `CURRENT_STATE.md` 冲突 |

明确不合并：7 月 13 日工作区备份分支与旧 MCP WIP 分支只作备份，不参与探测、cherry-pick 或 merge。

## 2. 冲突与纠偏

- `CURRENT_STATE.md` 冲突未采用整文件 ours/theirs；保留 myCobot 与单摄两条状态，并明确 `final_project/` 在 M0 板级复现前仍是正式主线。
- `white_balance.v` 在 M0-09 增量上增加 `pixel_cnt == 0` 除零防护；未改变端口、时钟、复位极性、顶层连线或 SDC。
- 初始 `75/75` 哈希只描述复制时快照；当前源码改动登记在 `m0_post_baseline_delta_20260714.csv`，历史 bitstream 不绑定当前树。
- 三个 myCobot 构建/测试脚本去除提交中的本机工具绝对路径，改为参数、环境变量和 PATH/安装目录发现。
- 两个 IP `settings.json` 的历史 `base_path` 改为相对各 IP 目录的 `..`；未重新生成 IP，下一次 IP Manager 操作前仍需 GUI 核对输出目录。

## 3. 约束与工程清点

| 项目 | 结果 | 边界 |
|---|---|---|
| `mem_test.xml` | 53/53 设计文件存在；2/2 IP 设置存在；0 个绝对 XML 路径 | 仅静态引用检查 |
| `mem_test.peri.xml` | XML 可解析 | 未打开 Interface Designer 重新生成 |
| IP `settings.json` | 2/2 JSON 可解析；`base_path=..` | 未验证重新生成行为 |
| `constrain.sdc` | 存在，55,570 bytes，检出 13 条 `create_clock` | 自动生成文件未手改；历史负 slack 未关闭 |
| include/mem | XML 引用源与现有 include/mem 依赖均保留 | 未运行 HDL 编译/仿真 |
| Codebase 图谱 | `D-cicc_cbm-main` 全量重建为 6078 nodes；可查到 18 个 `arm_runtime` 相关节点与单摄 `white_balance` | 精确边数见 artifact；旧 `D-cicc_cbm_link` 仅作历史兼容别名 |

镜像 RTL/IP/SDC 保留来源格式，未为消除历史空格而批量格式化；本次 authored 文档与脚本使用排除镜像文件后的 `git diff --check` 验证。

## 4. 已运行验证

- `run_arm_runtime_host.ps1`：disabled/simulated 两后端 PASS。
- `run_round_controller_host.ps1`：`4979/4979` PASS。
- `run_arm_runtime_qemu.ps1`：disabled/simulated 与 1 秒超时注入 PASS。
- RISC-V 构建：`competition/arm_bringup × disabled/simulated` 四组合 ELF/manifest 校验 PASS，全部 `NOT_FOR_FLASH`。
- `-BoardBuild`：在创建输出目录前 fail closed PASS。
- 单摄工程：53 个设计引用、2 个 IP 设置、XML/JSON 解析与绝对路径清点 PASS；`white_balance.v` 静态除零门存在。

## 5. 未验证与继续门禁

- 未运行候选工程 Efinity Map/PNR/STA、bitstream、烧录、3 次冷启动或 10 分钟画面复现。
- 未证明历史 bitstream 与修改后的 `white_balance.v` 对应。
- 未生成正式 `soc.h`，未关闭正式主线的 periphery/IO/PNR 阻塞。
- 未连接 J52 或机械臂，未产生真实 UART2 帧或动作；G4 及后续物理门保持 NO-GO。

## 6. 下一步

1. 用户在 `<local-demo-mirror>` 从当前候选源码做完整 Efinity flow，并回传原始 Map/PNR/STA/bitstream 证据。
2. 记录源码提交与 bitstream SHA-256，完成烧录、3 次冷启动和 10 分钟 J48/ch0→HDMI 复现。
3. 全部通过后，才讨论将单摄候选升格并原子更新 `AGENTS.md`、`CLAUDE.md`、主方案、`CURRENT_STATE.md` 和 Codebase 图谱。
4. myCobot 继续停在 G3；进入 G4 或任何物理接线/动作前需新的 Review Packet 与用户现场确认。
