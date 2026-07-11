# codebase-memory-mcp 图谱初始化与刷新指南

> 更新日期: 2026-07-11
> 适用项目: `D:\第十届集创赛-雄芯院材料`
> 工具版本: `codebase-memory-mcp 0.8.1`
> 本机程序: `D:\codebase-memory-mcp\codebase-memory-mcp.exe`
> 当前共享图谱项目: `D-cicc_cbm_link`

本仓库已完成第一版 codebase-memory-mcp 图谱初始化。CBM 用于帮助 Codex、Claude 和队友 Agent 快速缩小阅读范围；涉及 RTL/SoC/myCobot 高风险结论时，仍必须回到真实源码、工程 XML、日志和上板现象核查。

---

## 1. 当前状态

当前已提交共享 artifact（2026-07-09 刷新记录）：

- 仓库真实路径：`D:\第十届集创赛-雄芯院材料`
- CBM 访问路径：`D:\cicc_cbm_link`
- `D:\cicc_cbm_link` 是指向真实仓库的 Windows junction，用于绕过 CBM 0.8.1 对中文根路径的索引/持久化问题。
- 共享 artifact 已生成：`.codebase-memory/graph.db.zst`
- artifact 元数据：`.codebase-memory/artifact.json`
- artifact 合并策略：`.codebase-memory/.gitattributes`
- 当前图谱项目：`D-cicc_cbm_link`
- artifact 记录：commit `efd1bb7f011eeb856edecf50f7c2aee61a359e70`、`2200 nodes / 4613 edges`
- 新鲜度：该 artifact 早于 2026-07-11 合入的 FPGA 预处理结构，不能用于定位 `vision_preprocess_channel` 等新增符号；重要审查仍须回到真实源码。

本轮初始化命令返回：

```text
project: D-cicc_cbm_link
status: ready
artifact_present: true
artifact: .codebase-memory/graph.db.zst
artifact_commit: efd1bb7
```

已确认排除目录包括：

```text
.agents
.claude
.codex
.git
myblockly
初赛demo
赛方提供材料
mycobot_pc_tests/__pycache__
final_project/.codebase-memory
final_project/docs
final_project/integration
final_project/tools
```

说明：

- 第一版共享图谱只覆盖 Git 协作工程主干，不纳入 `赛方提供材料/`、`初赛demo/`、`myblockly/` 等本地资料库或软件包。
- CBM 0.8.1 的 `fast` 索引会自动过滤部分文档/工具目录，例如 `final_project/docs`、`final_project/integration`、`final_project/tools`；这些仍要用真实文件检索。
- 图谱主要是 File/Folder 级导航，不应当作 Verilog module 级调用链事实源。

---

## 2. 维护责任

图谱审查、刷新时机判断、artifact 更新和冲突取舍由 `@qzs610038-star` 负责。

队友如需更新图谱，应先提交变更说明或 Review Packet，不直接覆盖共享 artifact。

---

## 3. 目录与边界

Git 协作范围：

- `final_project/`
- `mycobot_pc_tests/`
- `AGENTS.md` / `CLAUDE.md` / `.claude/commands/*`
- 方案评审、路线文档、README、CBM 指南等协作文档

默认不纳入共享图谱：

- `赛方提供材料/`
- `初赛demo/`
- `myblockly/`
- 安装包、视频、压缩包、补丁包
- Efinity / ModelSim / Questa 生成物
- 本机 Agent 状态、缓存和私有配置

如果后续要把资料库纳入扩展图谱，建议单独建立第二份图谱或第二套 `.cbmignore`，不要直接覆盖第一版共享 artifact。

---

## 4. Phase 2 资料库图谱审查

结论：可以继续推进第二阶段，但不建议把全量资料库直接纳入默认共享 artifact。

已建立的 Phase 2 本机 junction：

```text
D:\cicc_phase2_official_demo  -> 赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3
D:\cicc_phase2_prelim_demo    -> 初赛demo\2ChMIPICSI_2ChMIPIDSI_Demo_Test
D:\cicc_phase2_riscv_examples -> 赛方提供材料\例程\RISC-V例程
D:\cicc_phase2_prelim_src     -> 初赛demo\2ChMIPICSI_2ChMIPIDSI_Demo_Test\src
D:\cicc_phase2_prelim_sw      -> 初赛demo\2ChMIPICSI_2ChMIPIDSI_Demo_Test\sw
D:\cicc_phase2_prelim_ip      -> 初赛demo\2ChMIPICSI_2ChMIPIDSI_Demo_Test\ip
```

非持久化索引烟测结果：

| 项目 | 入口 | 节点/边 | 建议 |
|---|---|---:|---|
| `D-cicc_phase2_official_demo` | 官方主 demo | `638 / 982` | 推荐纳入 Phase 2 默认审查集 |
| `D-cicc_phase2_prelim_src` | 初赛 demo `src/` | `438 / 615` | 推荐纳入 Phase 2 默认审查集 |
| `D-cicc_phase2_prelim_sw` | 初赛 demo `sw/` | `83 / 120` | 推荐纳入 Phase 2 默认审查集 |
| `D-cicc_phase2_prelim_ip` | 初赛 demo `ip/` | `12417 / 29124` | 按需索引，不默认持久化 |
| `D-cicc_phase2_prelim_demo` | 初赛 demo 全量 | `85941 / 201806` | 过大，仅临时分析使用 |
| `D-cicc_phase2_riscv_examples` | RISC-V 示例全量 | `48515 / 114677` | 过大，仅按需分析使用 |

推荐推进路线：

1. Phase 2A：持久化 `official_demo`、`prelim_src`、`prelim_sw` 三个精选图谱。
2. Phase 2B：只在需要核对 QCRV32/IP/SoC 细节时临时索引 `prelim_ip` 或 RISC-V 示例。
3. Phase 2C：若要共享资料库扩展 artifact，单独提交，不覆盖第一版 `.codebase-memory/graph.db.zst`。

Phase 2A 已完成持久化，产物存放在：

```text
.codebase-memory/phase2/official_demo/
.codebase-memory/phase2/prelim_src/
.codebase-memory/phase2/prelim_sw/
```

本轮持久化采用临时 staging Git 仓库生成 artifact，再复制回主仓库。原因是 CBM 0.8.1 只会在被索引路径是 Git 仓库根目录时写出 `.codebase-memory/graph.db.zst`；直接对 Phase 2 junction 使用 `persistence=true` 可以完成索引，但不会生成 artifact。

Phase 2A artifact：

| 目录 | 项目名 | 节点/边 | `graph.db.zst` |
|---|---|---:|---:|
| `.codebase-memory/phase2/official_demo/` | `D-cbm_phase2_stage_20260705-official_demo` | `628 / 962` | `67,195 bytes` |
| `.codebase-memory/phase2/prelim_src/` | `D-cbm_phase2_stage_20260705-prelim_src` | `438 / 615` | `46,245 bytes` |
| `.codebase-memory/phase2/prelim_sw/` | `D-cbm_phase2_stage_20260705-prelim_sw` | `83 / 120` | `12,468 bytes` |

注意：

- Phase 2 图谱只能作为资料定位与经验对照，不改变“初赛 demo 不是决赛代码基线”的边界。
- 全量初赛 demo 图谱很大，默认不建议持久化进 Git。
- 若要提交 Phase 2 artifact，应采用单独目录或命名，避免和第一版协作工程图谱混淆。

---

## 5. 安装与注册

Windows PowerShell：

```powershell
irm https://raw.githubusercontent.com/DeusData/codebase-memory-mcp/main/scripts/setup-windows.ps1 | iex
codebase-memory-mcp --version
codebase-memory-mcp install
```

注意：

- 当前验证版本是 `codebase-memory-mcp 0.8.1`。
- 注册后重启 Codex、Claude 或其他 Agent。
- 如果安装提示删除旧索引，先确认项目路径和 artifact 状态，不要盲删。

---

## 6. 首次拉取后的检查

队友拉取后：

```powershell
git pull --ff-only
codebase-memory-mcp --version
codebase-memory-mcp cli list_projects
Test-Path ".codebase-memory\graph.db.zst"
```

如果本机没有 `D:\cicc_cbm_link`，先创建 junction：

```powershell
cmd /c mklink /J "D:\cicc_cbm_link" "D:\第十届集创赛-雄芯院材料"
```

如果队友仓库路径不同，把后一个路径改成自己的真实仓库路径。例如：

```powershell
cmd /c mklink /J "D:\cicc_cbm_link" "E:\CICC"
```

---

## 7. 初始化 / 刷新命令

当前 PowerShell 下 `codebase-memory-mcp cli index_repository ...` 会误报 `repo_path is required`。刷新索引应通过 Codex / Claude 暴露的 MCP 工具执行：

```json
{
  "tool": "index_repository",
  "arguments": {
    "repo_path": "D:/cicc_cbm_link",
    "mode": "fast",
    "persistence": true
  }
}
```

只读检查可以继续用 CLI：

```powershell
codebase-memory-mcp cli list_projects
codebase-memory-mcp cli index_status "{`"project`":`"D-cicc_cbm_link`"}"
codebase-memory-mcp cli get_architecture "{`"project`":`"D-cicc_cbm_link`",`"aspects`":[`"all`"]}"
```

预期状态：

```text
project = D-cicc_cbm_link
status = ready
graph.db.zst exists
```

---

## 8. 提交范围

图谱初始化相关文件：

```text
.cbmignore
.codebase-memory/.gitattributes
.codebase-memory/artifact.json
.codebase-memory/graph.db.zst
.codebase-memory/phase2/
CBM_CONFIG_GUIDE.md
```

提交前检查：

```powershell
git status --short
git diff --cached --check
Test-Path ".codebase-memory\graph.db.zst"
Get-Item ".codebase-memory\graph.db.zst" | Select-Object Length, LastWriteTime
codebase-memory-mcp cli index_status "{`"project`":`"D-cicc_cbm_link`"}"
```

不要提交：

- `.claude/settings.local.json` 的个人权限变化
- `myblockly/`
- `赛方提供材料/`
- `初赛demo/`
- 安装包、视频、Efinity 生成目录、仿真数据库

`.codebase-memory/graph.db.zst` 是生成物。冲突时不手工合并数据库，以最新 `main` 重新索引后覆盖。

---

## 9. 使用方式

Agent 使用建议：

```text
请优先使用 codebase-memory-mcp 搜索当前项目。先检查 list_projects / index_status；已索引时优先 get_architecture / search_graph 缩小范围。若图谱不足或结论涉及 RTL/SoC/myCobot 高风险链路，必须回到真实文件、工程清单和日志核查。
```

适合的问题：

- 快速定位 UART / OSD / ROI / QCRV32 相关 RTL、CPU 源码或测试入口。
- 在正式工程中找 RTL、CPU、Efinity 工程文件、测试入口。
- 审查 Review Packet 时先确认涉及文件分布。
- 给新队友快速说明协作工程结构。

不适合单独依赖的问题：

- RTL 连线、时钟、复位、AXI burst、帧缓存地址、位宽转换、双通道同步是否正确。
- Efinity / ModelSim / Questa warning 是否可忽略。
- myCobot 实机动作、串口接线、电平安全。
- 判断高层方案是否“合理可行”。

---

## 10. 刷新策略

不建议每次小改都更新图谱 artifact。推荐：

1. 大结构变化后刷新。
   - 新增/移动大量 RTL、C、Python、文档目录。
   - 调整 `final_project/` 架构。
   - 引入新的 CPU/FPGA 接口文件或 review packet 目录。

2. 普通小改不强制刷新。
   - 单个 Markdown 修订。
   - 小范围注释修改。
   - 临时调试记录。

3. 重要评审前刷新。
   - Codex Gate 前。
   - 架构评审前。
   - 合并队友较大分支前。

4. 冲突处理。
   - artifact 冲突时以最新 `main` 重新索引。
   - 不手工合并 `graph.db.zst`。

---

## 11. 已知限制

- 真实中文路径 `D:/第十届集创赛-雄芯院材料` 直接索引会失败；当前使用 `D:/cicc_cbm_link` junction 作为稳定入口。
- PowerShell CLI 的 `index_repository` 参数解析不可用；索引动作使用 MCP 工具调用。
- 2026-07-11 维护核查发现：对已存在项目调用 `index_repository(..., mode="fast"/"moderate", persistence=true)` 虽返回 `indexed`，但未更新 `.codebase-memory/artifact.json`、`graph.db.zst`，运行态检索也未出现 7 月 11 日新增预处理模块。遇到这类情况不得声称 artifact 已刷新；必须先核对 `artifact.json` 的 `commit`、`indexed_at`、节点数，以及对新增符号执行一次 `search_graph`。本次尝试删除旧项目以重建时 MCP 返回 `Permission denied`，因此 artifact 刷新仍为 WARN，待图谱服务权限恢复后重建。
- 图谱主要是文件/目录级，不保证 Verilog module 级语义。
- `final_project/docs`、`final_project/integration`、`final_project/tools` 当前被 CBM 自动排除；文档审查仍要回到真实 Markdown 和源码。

---

## 12. 初始化验收清单

- [x] `codebase-memory-mcp --version` 输出 `0.8.1`。
- [x] 仓库根目录已创建 `.cbmignore`。
- [x] 已创建 ASCII junction：`D:\cicc_cbm_link`。
- [x] `index_repository(repo_path="D:/cicc_cbm_link", mode="fast", persistence=true)` 成功。
- [x] `index_status` 显示 `D-cicc_cbm_link` 为 `ready`。
- [x] `.codebase-memory/graph.db.zst` 已生成且非 0 字节。
- [x] `.codebase-memory/.gitattributes` 已生成，声明 `graph.db.zst merge=ours binary`。
- [x] Phase 2A 精选资料库图谱已生成到 `.codebase-memory/phase2/`。
- [x] 图谱初始化文件已提交并推送。
- [ ] 2026-07-11 之后的结构性 RTL/CPU 变更已写入共享 artifact（当前受 MCP 项目删除权限阻塞）。

---

## 13. 一句话规则

```text
第一版共享 codebase-memory 图谱通过 D:\cicc_cbm_link 生成，只覆盖 Git 协作工程和必要归档；它负责加速定位和管理上下文，不替代源码、Efinity 工程和审查证据。
```
