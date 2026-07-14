# 2026-07-14 三方个人分支更新汇总（2026-07-15 刷新）

审查者：Codex

远端刷新时间：2026-07-15（再次执行 `git fetch origin`）

比较基线：`origin/main@39e8a92`

## 1. 本方：myCobot 180°候选路径与明日上板准备

分支：`codex/mycobot-experiment-main-sync-20260714`

审查前 tip：`3882d06`

主要内容：

- 基于最新 `origin/main@39e8a92` 整理 PC 实验续跑、Route A 五点预设、调试记录和板上 G4–G11 边界。
- `route_a_20260714_2000` 最新 PC 日志完成 5/5 连续流程、0 轮人工扶正。
- Codex 独立复核将结论收敛为 `PASS_WITH_RISKS`：5/5 `drop_hover` 均依赖 2.02°软通过；15/15 夹爪动作无反馈；内部方位差 177.72°，但没有外部 180°与最大臂展证据。
- 明日仅放行 G4 正式构建/PNR、G5 断臂模拟和 D2 UART2 无臂回环；真实机械臂接线与动作继续 NO-GO。

评价入口：`final_project/docs/review_packets/mycobot_180deg_initial_verification_evaluation_20260714.md`

## 2. 队友一：libaoxun688 单摄候选工程

分支：`origin/dev/libaoxun688@8b340a7`

最近更新时间：2026-07-14 22:07:34 +0800

相对 `origin/main`：`0 behind / 3 ahead`，尚未进入主线

唯一提交链：

- `f100d5b`：Debayer `pixel_cnt=0` 防护、Bayer 相位参数化和 `top.v` 清理。
- `2db0345`：合并 `origin/main` 的 myCobot G0–G3、单摄候选与白平衡修复。
- `8b340a7`：HDMI-only 启动解耦、DSI 禁用、CSI 实际时钟名异步组、I²C ROM 增益、复位域修复、HDMI 像素相位与固定点显示白平衡。

真实改动面：8 个文件，包含 `src/top.v`、`constrain.sdc`、Debayer、CSI/I²C ROM 和 `WORK_LOG.md`；相对主线约 `+534/-79`。

Codex 评价：

- 有价值：`mipi_clk` 域复位、CSI 时钟组、SC431HAI 约 2.98 倍模拟增益已在中间版本获得真实 PNR/板级反馈；M0-27 记录 `TIMING PASS`、亮度约提升 2.5–3 倍且未饱和。
- 未闭环：M0-27 同时仍是 `IMAGE QUALITY FAIL / FIVE-COLOR NOT VERIFIED`。
- 当前 tip 又加入 HDMI 像素相位重锚和 R=1.75x/B=2x 固定白平衡；M0-28/M0-29 只有静态检查与同步，**当前 tip 尚无新的 Map/PNR/bitstream/板级画面**。
- 资源边界：DSI 活动路径已用宏禁用，但 `soft_mipi_rx_top1`、ch1 frame buffer 与 ch1 Debayer 仍存在，不能把它描述为“完整单摄资源裁剪完成”。
- 合并建议：暂不直接合并或升格。先用当前 tip 完整重跑 Efinity，要求 WNS/WHS 非负、CDC 报告、`i_sysclk_div2 -> hdmi_tx_slow_clk` slack、bitstream 哈希和固定五色画面；再判断颜色与重影是否改善。

## 3. 队友二：wsc6090 CPU

分支：`origin/dev/wsc6090-cpu@df45b5a`

最近更新时间：2026-07-15 00:11:06 +0800

相对 `origin/main`：`0 behind / 2 ahead`，其中 `df45b5a` 已同步 `main@39e8a92`

最近内容：新增 `cpu_result_semantics` 统一结果语义、ARM_DISABLED 候选 `main_loop_adapter`、task_matcher 真值 reason 和两项 Host 测试；原分支报告 MinGW GCC 14.2.0 下 `374/374`、`117/117` PASS。

Codex 评价：

- 该分支已有未合入主线的新内容，现由 `codex/wsc-mycobot-integration-20260715` 先行集成，CPU 语义以 wsc 设计为主，同时保留 main 的 16-bit 合同、`arm_runtime` 与构建安全门。
- `df45b5a` 同步 main 后，旧版 main adapter 调用被冲突解析掉，但 `CURRENT_STATE.md` 一度仍声称 main 与 Host 测试共享实现；正式融合已纠正为“统一语义投影接入 main，候选 adapter 在 G4 事件源/时基前不启用”。
- 修复前 Codex MSVC 19.42 `/W4 /WX` 因两个 `PASS()` 宏的局部 `int d` 触发 C4456，断言未执行；这属于警告策略差异暴露的测试可移植性问题，不是已证实的 CPU 功能失败。
- 集成版已修变量遮蔽、补 GCC `-Wshadow`、使用 `vswhere` 发现 MSVC，并把三个新源文件加入规范 RISC-V 构建器。本机 MSVC 已通过 `374/374`、`117/117`，四个 RISC-V profile/backend 组合 BUILD/ELF PASS；仍需 wsc 在其 MinGW/Efinity 环境复验同一集成 commit。

## 4. 三方合并判断

| 分支 | 当前状态 | 建议 |
|---|---|---|
| 本方 myCobot | `PASS_WITH_RISKS`，已并入 CPU 集成候选 | 保留点位、实验评价和安全门；走 G4→G5→D2，不直接动作 |
| libaoxun688 FPGA | 3 个主线外提交；当前 tip 缺新 PNR/板测 | 保持隔离，先补当前 tip 构建与五色画面证据 |
| wsc6090 CPU | 2 个主线外提交；集成候选已完成 MSVC/目标构建整改 | wsc 在 MinGW/Efinity 环境验收同一集成 commit 后再提 main PR |

当前不把三方一次性直接合并到 `main`：CPU + myCobot 先形成受审集成候选；FPGA 分支继续独立等待当前 tip 对应的 PNR/CDC/bitstream/五色板级证据。
