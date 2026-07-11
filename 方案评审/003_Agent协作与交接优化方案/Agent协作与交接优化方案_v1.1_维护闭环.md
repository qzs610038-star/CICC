# Agent 协作与交接优化方案 v1.1：维护闭环

> 状态：已落地的维护规则与工具索引。本文不覆盖 `AGENTS.md` 的安全红线，也不替代真实 RTL、SoC 生成物、构建日志或上板证据。

## 目标

让“代码已变、状态未变、入口互相矛盾”的问题在合并后立即可见，同时不把 FPGA、SoC 或机械臂高风险验证伪装成普通文档检查。

## 闭环

```text
结构性变更或合并
  -> 更新 CURRENT_STATE 与受影响入口
  -> 运行 project_freshness_check.ps1
  -> 修正 FAIL / 记录 WARN
  -> 需要时刷新 CBM artifact
  -> 提交 Review Packet 或 handoff
```

## 已落地机制

| 层级 | 机制 | 作用 |
|---|---|---|
| 硬边界 | `AGENTS.md` | 决赛主线、机械臂安全、Codex Gate、Git 规则。 |
| 可变状态 | `CURRENT_STATE.md` | 活跃路线、历史参考、待定决策和证据索引。 |
| 接力 | `HANDOFF_TEMPLATE.md` + 健康检查 | 默认接受高置信事实，允许只读握手和最小范围纠偏。 |
| 模块细则 | `.agents/skills/` | FPGA 与 CPU/myCobot 的专属约束。 |
| 新鲜度 | `maintenance_manifest.json` + `tools/project_freshness_check.ps1` | 检查关键文件、链接、状态证据、图谱与 Git。 |

## 刷新触发条件

| 变更范围 | 必须同步 |
|---|---|
| `top.v`、`mem_test.xml`、约束、视频 RTL | `CURRENT_STATE.md`、视频状态/架构文档、Review Packet；结构性变更后刷新 CBM。 |
| CPU 主循环、分类、参数、任务匹配 | `CURRENT_STATE.md`、`CPU_MODULE_PLAN.txt`、CPU README；接口语义变更时更新寄存器契约。 |
| myCobot 协议或控制器 | `CURRENT_STATE.md`、迁移设计、测试/证据；实机动作另走 Codex Gate。 |
| APB/CDC/寄存器地址 | `register_map.md`、接口契约、CPU 头文件、RTL 常量和 Review Packet；必须以生成 `soc.h` 为事实来源。 |
| 合并、交接或大规模目录调整 | 运行新鲜度检查；若影响代码结构则刷新 CBM artifact。 |

## 未落地项

- 正式 SoC/APB 未生成前，不建立寄存器自动生成链，也不冻结建议偏移。
- 普通维护检查不运行 Efinity PNR、烧录或机械臂动作；这些仍由显式任务和硬件安全门控制。
- 图谱刷新只在结构性代码变化、重要评审或合并前执行，不对每次 Markdown 小改强制执行。

## 日常命令

```powershell
powershell -ExecutionPolicy Bypass -File tools/project_freshness_check.ps1
powershell -ExecutionPolicy Bypass -File tools/project_freshness_check.ps1 -CheckRemote
powershell -ExecutionPolicy Bypass -File tools/agent_handoff_health_check.ps1 -Handoff debug_records/some_handoff.md
```
