# codebase-memory-mcp 团队使用协商指南

> 生成日期: 2026-07-05
> 适用项目: `D:\第十届集创赛-雄芯院材料`
> 工具版本: `codebase-memory-mcp 0.8.1`
> 安装路径: `D:\codebase-memory-mcp\codebase-memory-mcp.exe`
> 目标: 用共享知识图谱减少 Agent 反复扫仓库带来的 token 消耗，并给团队代码/文档管理提供统一索引入口。

---

## 1. 项目与工具定位

本仓库是第十届集创赛雄芯院方向的 FPGA 竞赛资料与决赛工程协作仓库。当前长期主线仍以 `分赛区决赛实施开发路线.md` 为最高层路线文件：

- FPGA 负责视频前端、ROI、统计特征、OSD 和必要硬件通道。
- 板上 CPU 负责颜色/形状/尺寸识别决策、阈值参数管理和 myCobot 控制。
- `赛方提供材料/` 与 `初赛demo/` 是只读资料库和经验库。
- `final_project/` 是正式决赛工程落点。
- `方案评审/` 与 `final_project/docs/review_packets/` 用于沉淀方案审查与证据。

`codebase-memory-mcp` 的角色不是替代人工审查，也不是替代 Efinity 工程真相；它是本地代码知识图谱服务，帮助 Codex、Claude 等 Agent 优先通过图谱定位文件、目录、符号和依赖关系，从而少读无关文件、少烧 token。

---

## 2. 当前实测状态

2026-07-05 在本机实测：

```powershell
codebase-memory-mcp --version
# codebase-memory-mcp 0.8.1

codebase-memory-mcp config list
# auto_index = false
# auto_index_limit = 50000
```

已确认：

- 主程序存在于 `D:\codebase-memory-mcp\codebase-memory-mcp.exe`，并可从 PATH 调用。
- Codex 配置中已有 `[mcp_servers.codebase-memory-mcp]`，指向该 exe。
- Claude 配置中也已有 `codebase-memory-mcp` MCP server。
- 本项目目前尚未提交 `.cbmignore`。
- 本项目目前尚未生成或提交 `.codebase-memory/graph.db.zst`。
- 用 MCP 工具做过一次 `fast` 索引烟测，项目在工具里显示为 `D`，状态 ready，约 `15617 nodes / 15012 edges`。

重要限制：

- 当前实测图谱主要生成 `Project` / `Folder` / `File` 节点，尚未可靠生成 Verilog module 级语义节点。
- `search_code` 在本中文路径、大量 Verilog 文件和 Windows PowerShell 组合下出现过较慢和路径错误输出。
- 因此现阶段应把它定位为“导航与文件级上下文加速器”，不要把它当成已经可靠的 RTL 模块层级/调用链事实源。

---

## 3. 队友本机安装步骤

队友首次使用时建议按下面顺序来，避免误删已有本地索引。

### 3.1 安装二进制

Windows PowerShell：

```powershell
irm https://raw.githubusercontent.com/DeusData/codebase-memory-mcp/main/scripts/setup-windows.ps1 | iex
codebase-memory-mcp --version
```

预期版本：

```text
codebase-memory-mcp 0.8.1
```

如果团队决定锁版本，应在群里约定统一版本号。否则不同版本生成的图谱行为可能不同，尤其是 Windows 路径、`.cbmignore`、Verilog 解析和 artifact 导入行为。

### 3.2 注册到 Agent

```powershell
codebase-memory-mcp install
```

注意：

- 若安装过程提示已有旧索引需要删除，先选 `n`，不要直接删除。
- 注册后重启 Codex、Claude 或其他 Agent。
- 在 Agent 工具列表中确认出现 `codebase-memory-mcp`。

不建议新人直接运行：

```powershell
codebase-memory-mcp install --dry-run
```

原因是本机实测 `--dry-run` 也可能停在“是否删除已有索引”的交互提示处，容易造成误解。

---

## 4. 本项目初始化索引步骤

当前 `0.8.1` 的 CLI 用法不是旧版文档里的 `index_repository --dir="."`，而是通过 `cli <tool> <json>` 调用。

### 4.1 推荐首次索引命令

在仓库根目录执行：

```powershell
Set-Location "D:\第十届集创赛-雄芯院材料"
codebase-memory-mcp cli index_repository '{"repo_path":"D:/第十届集创赛-雄芯院材料","mode":"fast","persistence":true}'
```

参数含义：

- `repo_path`: 当前仓库路径。
- `mode: fast`: 优先速度和文件级导航，适合日常协作。
- `persistence: true`: 写出 `.codebase-memory/graph.db.zst`，便于团队共享。

### 4.2 验证索引是否可用

```powershell
codebase-memory-mcp cli list_projects
codebase-memory-mcp cli index_status '{"project":"D"}'
codebase-memory-mcp cli get_architecture '{"project":"D","aspects":["all"]}'
Test-Path ".codebase-memory\graph.db.zst"
```

如果 `list_projects` 显示项目名不是 `D`，后续命令里的 `project` 应替换成实际项目名。

### 4.3 在 Codex / Claude 中使用

可直接对 Agent 说：

```text
请优先使用 codebase-memory-mcp 搜索当前项目，先检查 list_projects / index_status；如果未索引，请以 fast 模式索引当前仓库。
```

适合的问题类型：

- “这个功能相关文件在哪里？”
- “仓库大体有哪些目录和文件簇？”
- “帮我快速定位 UART / OSD / ROI / QCRV32 相关源码或文档。”
- “先用图谱缩小范围，再回到真实文件核查。”

不适合直接相信的类型：

- “某个 RTL 模块调用链是否绝对正确。”
- “Efinity 工程实际编译使用了哪些文件。”
- “某个 warning 是否可忽略。”
- “myCobot 实机动作是否安全。”

这些仍必须回到真实文件、工程清单、日志和上板现象验证。

---

## 5. `.cbmignore` 建议

本项目文件混合了正式工程、赛方资料、初赛 demo、工具补丁、安装包和生成物。团队应先协商索引范围，再创建 `.cbmignore`。

推荐第一版目标：

- 保留 `final_project/`。
- 保留主 demo 源码：`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/src/`、必要 `ip/` 和工程 XML。
- 保留初赛 demo 中有复用价值的源码、文档、QCRV32 和 OSD 相关内容。
- 排除安装器、视频、压缩包、Efinity 补丁包、仿真/综合/布局布线生成目录。

建议 `.cbmignore` 初稿：

```gitignore
# OS / editor
Thumbs.db
Desktop.ini
.DS_Store
.vscode/
.idea/
.qoder/

# Python / local env
.venv/
**/.venv/
__pycache__/
*.pyc

# Efinity / simulation generated outputs
**/outflow/
**/work_syn/
**/work_pnr/
**/work_pt/
**/work_dbg/
**/work_sim/
**/work/
**/modelsim/
*.vdb
*.wlf
*.vcd
*.gtkw
transcript

# RISC-V / C build outputs
*.o
*.elf
*.hex
*.bin
*.lst
*.map
*.asm
*.d
*.qdb
*.db
*.rs
*.pickle
*.launch

# Large archives, installers, media
*.zip
*.rar
*.7z
*.tar
*.gz
*.exe
*.msi
*.mp4
*.avi
*.mov
*.mkv
*.iso

# Local software packages and vendor tool distributions
myblockly/
赛方提供材料/EDA软件/
赛方提供材料/EDA软件培训文档及视频/
赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch/

# Local machine-specific config
.claude/settings.json
final_project/tools/mcp/fpga_robot_mcp/configs/fpga_robot.local.json
```

注意：

- 不建议第一版直接排除整个 `赛方提供材料/`，否则主工程参考价值会下降。
- 不建议第一版直接排除整个 `初赛demo/`，否则 QCRV32、OSD 回写、调试经验和已知问题定位会变慢。
- `.cbmignore` 在 Windows 上的实际排除效果需要索引后用 `get_architecture` / `search_graph` 验证，不要只凭文件存在判断。

---

## 6. 团队共享 artifact

显式索引并开启 `persistence:true` 后，工具应在仓库根目录生成：

```text
.codebase-memory/
  graph.db.zst
```

该文件是压缩后的知识图谱快照。队友 pull 后再次索引时，工具可先导入该 artifact，再做本地增量补全，避免每个人从零开始全量索引。

### 6.1 首次提交

```powershell
git status --short
git add .codebase-memory/graph.db.zst
git add .gitattributes
git commit -m "chore: add codebase memory graph"
git push
```

如果首次导出自动修改 `.gitattributes`，需要一起审查并提交。通常它会为二进制 artifact 设置合并策略，减少多人同时更新图谱时的冲突。

### 6.2 队友拉取后

```powershell
git pull
codebase-memory-mcp --version
codebase-memory-mcp cli index_repository '{"repo_path":"D:/第十届集创赛-雄芯院材料","mode":"fast","persistence":true}'
codebase-memory-mcp cli index_status '{"project":"D"}'
```

如果队友路径不同，比如仓库在 `E:\competition\...`，只需要替换 `repo_path`；`project` 名称以 `list_projects` 输出为准。

---

## 7. 建议团队管理规则

建议不要把 `.codebase-memory/graph.db.zst` 变成每次提交都必须更新的硬要求。它是协作加速器，不是源码事实本身。

推荐规则：

1. 大结构变化后更新图谱。
   - 新增/移动大量 RTL、C、Python、文档目录。
   - 调整 `final_project/` 架构。
   - 引入新的 CPU/FPGA 接口文件或 review packet 目录。

2. 普通小改不强制更新图谱。
   - 单个 Markdown 修订。
   - 小范围注释修改。
   - 临时调试说明。

3. 每周或重要评审前由一人刷新一次。
   - 比如 Codex Gate 前、架构评审前、合并重要分支前。

4. artifact 冲突时采用“后索引者覆盖”。
   - `.codebase-memory/graph.db.zst` 是生成物。
   - 冲突时不手工合并数据库。
   - 以最新主分支重新索引并提交。

5. Agent 审查时必须说明图谱是否新鲜。
   - 回答里区分“图谱定位结果”和“真实文件核查结果”。
   - 高风险结论必须回到源码、工程 XML、日志或 Efinity 输出。

---

## 8. Token 节约与使用边界

适合用 CBM 节约 token 的场景：

- 新队友快速了解仓库结构。
- Agent 开始任务前先定位相关文件。
- 在 `赛方提供材料/`、`初赛demo/`、`final_project/` 之间快速找同名模块或文档。
- 审查 packet 时快速确认涉及的源码/文档分布。
- 让 Claude/Codex 少做全仓 `rg`、少读无关大文件。

不应依赖 CBM 单独作结论的场景：

- RTL 连线、时钟、复位、AXI、帧缓存地址等高风险审查。
- Efinity / ModelSim / Questa warning 是否可忽略。
- myCobot 实机动作、串口接线、电平安全。
- 判断某方案是否“合理可行”。
- 判断赛方资料、初赛 demo 和正式工程之间的版本差异。

这些场景中，CBM 只能用于缩小范围；最终结论仍要以真实文件、最新构建日志、上板现象和 Review Packet 为准。

---

## 9. 可视化 UI

如果安装的是 UI 版，可启动 3D 图谱界面：

```powershell
codebase-memory-mcp --ui=true --port=9749
start http://localhost:9749
```

适合用途：

- 给队友展示仓库文件结构。
- 辅助解释 `final_project/`、`赛方提供材料/`、`初赛demo/` 的边界。
- 答辩或内部沟通时展示项目管理方法。

当前不建议把 UI 图谱作为 Verilog 模块层次展示的唯一依据，因为本项目实测尚未可靠得到 module 级图谱。

---

## 10. 常用命令速查

```powershell
# 版本与配置
codebase-memory-mcp --version
codebase-memory-mcp config list
codebase-memory-mcp config set auto_index true
codebase-memory-mcp config set auto_index_limit 100000

# 注册 Agent
codebase-memory-mcp install

# 项目索引
codebase-memory-mcp cli index_repository '{"repo_path":"D:/第十届集创赛-雄芯院材料","mode":"fast","persistence":true}'

# 项目列表与状态
codebase-memory-mcp cli list_projects
codebase-memory-mcp cli index_status '{"project":"D"}'

# 架构概览
codebase-memory-mcp cli get_architecture '{"project":"D","aspects":["all"]}'

# 图谱搜索示例
codebase-memory-mcp cli search_graph '{"project":"D","query":"uart","limit":20}'
codebase-memory-mcp cli search_graph '{"project":"D","query":"qcrv32 osd","limit":20}'

# 共享 artifact
git add .codebase-memory/graph.db.zst
git commit -m "chore: update codebase memory graph"
```

---

## 11. 协商清单

和队友正式启用前，建议确认以下问题：

- 是否统一安装 `codebase-memory-mcp 0.8.1`，还是允许各自升级？
- 是否把 `.codebase-memory/graph.db.zst` 纳入 Git？
- 谁负责在重要合并前刷新 artifact？
- `.cbmignore` 是否保留 `初赛demo/` 的源码和文档？
- `.cbmignore` 是否保留 `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/` 主工程源码？
- artifact 冲突时是否统一采用“主分支重新索引后覆盖”？
- Agent 输出中是否必须区分“CBM 图谱定位”和“真实文件核查”？
- 高风险 RTL / SoC / myCobot 审查是否仍强制走 Codex Gate？

推荐默认答案：

- 提交 `.codebase-memory/graph.db.zst`，但只在大结构变化、重要评审前或每周例行更新。
- 第一版 `.cbmignore` 不排除主 demo 和初赛 demo 的源码/文档，只排除生成物、安装包、视频、补丁和本机配置。
- 将 CBM 作为 token 节约和项目导航工具，不作为最终工程事实源。

---

## 12. 当前验收清单

正式启用前建议逐项打勾：

- [ ] `codebase-memory-mcp --version` 输出 `0.8.1`
- [ ] Codex / Claude 能看到 `codebase-memory-mcp` MCP 工具
- [ ] 仓库根目录已创建 `.cbmignore`
- [ ] `index_repository` 使用 `repo_path/mode/persistence` JSON 成功执行
- [ ] `list_projects` 能看到当前项目
- [ ] `index_status` 显示 ready
- [ ] `.codebase-memory/graph.db.zst` 已生成
- [ ] `.gitattributes` 已审查，确认 artifact 合并策略合理
- [ ] 队友 pull 后能基于 artifact 快速完成索引
- [ ] Agent 能用 CBM 定位文件，但高风险结论仍回到真实文件核查

---

## 13. 建议写入协作规范的短句

可加入 `AGENTS.md` / `CLAUDE.md` 的一句话版本：

```text
代码发现优先使用 codebase-memory-mcp：先检查 list_projects / index_status，已索引时优先 search_graph、get_architecture、search_code 缩小范围；若图谱不足或结论涉及 RTL/SoC/myCobot 高风险链路，必须回到真实文件、工程清单和日志核查。
```

用于队友沟通的一句话版本：

```text
我们共享 .codebase-memory/graph.db.zst 来减少 Agent 反复扫仓库的 token 消耗；它负责加速定位和管理上下文，不替代源码、Efinity 工程和审查证据。
```
