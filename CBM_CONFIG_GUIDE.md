# codebase-memory-mcp 图谱初始化与刷新指南

> 更新日期: 2026-07-05
> 适用项目: `D:\第十届集创赛-雄芯院材料`
> 工具版本: `codebase-memory-mcp 0.8.1`
> 本机程序: `D:\codebase-memory-mcp\codebase-memory-mcp.exe`
> 当前共享图谱项目: `D-cicc_cbm_link`

本仓库已完成第一版 codebase-memory-mcp 图谱初始化。CBM 用于帮助 Codex、Claude 和队友 Agent 快速缩小阅读范围；涉及 RTL/SoC/myCobot 高风险结论时，仍必须回到真实源码、工程 XML、日志和上板现象核查。

---

## 1. 当前状态

2026-07-05 本机实测：

- 仓库真实路径：`D:\第十届集创赛-雄芯院材料`
- CBM 访问路径：`D:\cicc_cbm_link`
- `D:\cicc_cbm_link` 是指向真实仓库的 Windows junction，用于绕过 CBM 0.8.1 对中文根路径的索引/持久化问题。
- 共享 artifact 已生成：`.codebase-memory/graph.db.zst`
- artifact 元数据：`.codebase-memory/artifact.json`
- artifact 合并策略：`.codebase-memory/.gitattributes`
- 当前图谱项目：`D-cicc_cbm_link`
- 当前图谱规模：`967 nodes / 1356 edges`

本轮初始化命令返回：

```text
project: D-cicc_cbm_link
status: indexed
artifact_present: true
artifact: .codebase-memory/graph.db.zst
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

## 4. 安装与注册

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

## 5. 首次拉取后的检查

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

## 6. 初始化 / 刷新命令

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

## 7. 提交范围

图谱初始化相关文件：

```text
.cbmignore
.codebase-memory/.gitattributes
.codebase-memory/artifact.json
.codebase-memory/graph.db.zst
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

## 8. 使用方式

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

## 9. 刷新策略

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

## 10. 已知限制

- 真实中文路径 `D:/第十届集创赛-雄芯院材料` 直接索引会失败；当前使用 `D:/cicc_cbm_link` junction 作为稳定入口。
- PowerShell CLI 的 `index_repository` 参数解析不可用；索引动作使用 MCP 工具调用。
- 图谱主要是文件/目录级，不保证 Verilog module 级语义。
- `final_project/docs`、`final_project/integration`、`final_project/tools` 当前被 CBM 自动排除；文档审查仍要回到真实 Markdown 和源码。

---

## 11. 初始化验收清单

- [x] `codebase-memory-mcp --version` 输出 `0.8.1`。
- [x] 仓库根目录已创建 `.cbmignore`。
- [x] 已创建 ASCII junction：`D:\cicc_cbm_link`。
- [x] `index_repository(repo_path="D:/cicc_cbm_link", mode="fast", persistence=true)` 成功。
- [x] `index_status` 显示 `D-cicc_cbm_link` 为 `ready`。
- [x] `.codebase-memory/graph.db.zst` 已生成且非 0 字节。
- [x] `.codebase-memory/.gitattributes` 已生成，声明 `graph.db.zst merge=ours binary`。
- [ ] 图谱初始化文件已提交并推送。

---

## 12. 一句话规则

```text
第一版共享 codebase-memory 图谱通过 D:\cicc_cbm_link 生成，只覆盖 Git 协作工程和必要归档；它负责加速定位和管理上下文，不替代源码、Efinity 工程和审查证据。
```
