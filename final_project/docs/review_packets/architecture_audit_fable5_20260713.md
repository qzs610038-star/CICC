# 架构审计报告 — Claude Fable 5

> **审计范围：** 系统架构、接口契约、决赛执行方案、PNR/SoC/CDC/OSD/CPU/机械臂全链路只读审计
> **审计来源：** Claude Fable 5（本日最高可用模型）
> **审计日期：** 2026-07-13
> **审计方式：** 只读，不修改文件，不构建/仿真/烧录/机械臂动作
> **审计依据：** 官方细则 → AGENTS.md 硬边界 → CURRENT_STATE.md → 决赛主方案 → 真实源码/工程

---

## 一、总体裁决

| 维度 | 裁决 | 一句话说明 |
|------|------|-----------|
| 系统架构 | **CONDITIONAL PASS** | FPGA 视频/统计/OSD + 板上CPU 四任务/逐轮/控制 的职责划分正确，不必推倒重来 |
| 当前执行方案 | **CONDITIONAL PASS** | Gate/F1-F3 分层方向正确，但实现尚未跨过 PNR + SoC/APB + main集成 三个硬门 |

---

## 二、P0 阻塞表（6 项）

| # | 阻塞 | 精确证据 | 对 F1/F2 影响 | 最小修复或验证 | 验收条件 | 回退点 |
|---|------|----------|--------------|--------------|---------|-------|
| 1 | **PNR 1776 IO 无 placement** | `team_integration_merge_review_20260713.md:43-45`；`mem_test.xml:123-124` peri-syn 关闭；`constrain.sdc:12` 器件 Ti180J484 与 TJ375N529 不匹配 | F1/F2 全阻塞 | 核对 periphery 导出/器件/peri-syn 开关；不盲绑管脚 | 生产 PNR 无 outpad 断言 | 回退到仅 map，不宣称 bitstream |
| 2 | **SoC PLL 资源未证实可接** | `mem_test.peri.xml:216/249/339` 三块 PLL_BL 全被视频占用；`CURRENT_STATE.md:110-116` | F1 缺 APB/MMIO，F2 更无从谈起 | 只做 GUI 资源审查，确认硬 SoC 是否有合法 PLL 路径 | 有同次生成的 `soc.h` 与合法时钟/复位证据 | 冻结 SoC 接入决定 |
| 3 | **`main.c` 仍绕过新控制器** | `main.c:340-388` 直接调旧 `task_matcher_evaluate()`，round 推进占位为空；`CURRENT_STATE.md:139-143` | F1 无法保证"一轮一次/正确 SKIP/ARM_DISABLED" | 在 `ARM_DISABLED` 下只接 `round_controller`，不接动作 | 20 轮无动作流程板上可跑 | 回退到 Host，不宣称板级 |
| 4 | **APB/CDC/OSD 仅文档候选** | `register_map.md:1-5` 未冻结；`feature_snapshot.v:42-86` 仅像素域 valid/ack，无跨时钟域同步器 | F1 结果无法原子显示 | 先做 MAGIC/heartbeat → 固定结果 → OSD 最小闭环 | 同轮 frame_id/round_seq 原子提交可见 | 回退到固定结果显示 |
| 5 | **五色真源不完整** | `board_io.h:51-62` FG_AREA_AVAILABLE 默认 0；`board_io.h:153` TARGET_SEL 仅 2-bit 不支持白/黑；`task_matcher.c:281-291` 正式构建强制清空目标 | F1 白/黑检测不可靠，目标输入不全；F2 无意义 | 优先补板上目标输入；白/黑至少暴露同帧统计 | 板上五色目标可注入，白/黑不靠猜 | 回退为明确 UNVERIFIED，不误抓 |
| 6 | **机械臂板级安全门未过** | `io_pin_map.md:7-29` 六项全 FAIL；`mycobot_board_bringup_operator_sop_20260712.md:12-17` | F2 禁止启用 | 严格按 T0→S4，只读回环前不接真臂 | T0 全 PASS + UART2 100 帧回环 | 保持 F1，机械臂禁用 |

---

## 三、冲突与错误假设（5 项）

| 类型 | 描述 | 证据位置 |
|------|------|---------|
| **CONFIRMED** | 任务三评分正文 25/25/50 与评分表 1/1/1 不一致，只能现场确认 | 细则 L88-109 |
| **CONFIRMED** | F1 在回退表像"机械臂禁用的软件闭环"，在主方案正文又要求"真实摄像头+板上CPU+OSD+20轮"，口径不一致 | 主方案 L571-576 vs L688-710 |
| **CONFIRMED** | Host 795/795 或 compile-only 不能外推为板级准备度 | CURRENT_STATE.md L140-141 |
| **INFERENCE** | 1776 IO 根因是工程模式/器件/Periphery 绑定失配，不是业务 RTL 端口暴涨 | mem_test.xml L123-124 + constrain.sdc L12 |
| **UNVERIFIED** | `LIVE_FG_AREA` 字段名不一定最终定版，但"同帧白黑判别真源"是 P0 必需 | board_io.h L51-53 |

---

## 四、最小架构优化

| 操作 | 内容 |
|------|------|
| **KEEP** | FPGA/CPU 现有职责边界；`round_controller + competition_round_transaction` 双层拆分 |
| **CHANGE** | `main.c` 必须只通过 `round_controller` 驱动轮次与 `ARM_DISABLED`；production/debug 必须显式分离，合成源不能默认开启 |
| **DEFER** | 双摄完整闭环、任务三/四高置信尺寸标定、真实机械臂整场，均在 7/17 冻结后 |
| **STOP** | 盲绑 1776 IO、猜测性接 SoC、未过 T0 推进 UART2 真臂、继续留 `main.c` 在旧 matcher 直通路径 |

---

## 五、截止日前关键路径

```
FPGA/SoC 线：periphery/PNR根因 → 生产/调试构建拆分 → GUI 确认 SoC PLL → MAGIC/heartbeat
CPU/OSD 线：register_map 收口 → main 接 round_controller(ARM_DISABLED) → 固定结果 OSD → 真快照判定
机械臂线：T0 真源表 → UART2 回环/只读 → 仍禁动作

汇合 Gate：先 Gate B1 + 最小 APB，再 Gate C
F1 冻结点：单摄真实输入 + 板上 CPU + OSD + 正确 SKIP + 无动作 20 轮
F2 启用条件：T0 全 PASS + UART2 只读回环 PASS + round_controller→arm_controller 唯一请求路径已接通
```

---

## 六、文件级建议修改清单

| 文件 | 建议改动 |
|------|---------|
| `final_project/fpga/efinity/mem_test.xml` | 核对 periphery 生成与 `peri-syn-*` 策略 |
| `final_project/fpga/efinity/constrain.sdc` | 核对器件/Interface Designer 生成源 |
| `final_project/fpga/rtl/top/top.v` | 拆分 production/debug 入口，合成源不默认 |
| `final_project/cpu/app/src/main.c` | 接入 `round_controller`，移除旧直通闭环 |
| `final_project/integration/register_map.md` | 补齐五色目标、帧统计、原子提交真源 |
| `final_project/cpu/app/include/board_io.h` | 与 register_map.md 对齐，补五色目标/帧统计 |
| `final_project/integration/io_pin_map.md` | 继续作为 F2 唯一放行表 |

---

## 七、最终待确认问题（5 个）

1. **任务三评分比例**：按 25/25/50 还是 1/1/1？需现场专家组确认
2. **SoC PLL 可行性**：Interface Designer 在当前视频工程里是否给硬 SoC 留有合法 PLL 重规划路径？
3. **白/黑真源形式**：最终走 `LIVE_FG_AREA` 还是 `sum_y + roi_pixel_count`？
4. **单/双摄决策**：7月15日后若双摄未稳，是否正式锁定单摄？
5. **机械臂底座复核**：外部基准复核何时能形成可签字证据？

---

## 附录：审计方法

- 3 个低配 subagent 并行采集证据（规则/Gate、FPGA/SoC/PNR、CPU/OSD/机械臂）
- 主模型只打开影响最终结论的精确证据范围，不重读全文
- 代码发现优先使用 `D-cicc_cbm_link` 图谱（4514 nodes/10958 edges），最终证据回到真实源文件
- 所有结论绑定精确文件路径+行号