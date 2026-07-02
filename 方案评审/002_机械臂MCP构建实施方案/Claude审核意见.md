# Claude 审核意见 — 机械臂 MCP 构建实施方案 v0.1

审核日期：2026-07-01  
审核人：Claude (Fable 5)  
审核对象：`机械臂MCP构建实施方案_v0.1_Codex初稿.md`  
对应工程：`D:\第十届集创赛-雄芯院材料`

---

## 审核结论

**方案总体合理，但存在 4 个重大遗漏和 7 个中等风险项，建议修正后进入 Phase 0。**

方案对 MCP 的定位（开发期工具、不进入比赛闭环）、安全门控分级（三阶权限)、工具清单覆盖度、实施阶段划分基本到位。但以下几个问题必须在 Phase 0（只读核验）阶段修正，否则 MCP 骨架将建立在不准确的假设上：

---

## 🚨 重大遗漏（必须修正）

### 1. Efinity 2025.2 未安装 — 方案未包含安装步骤

| 项目 | 状态 |
|------|------|
| 安装包位置 | `赛方提供材料/EDA软件/00 Efinity 2025.2.rar` |
| 补丁位置 | `赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch/` |
| 本机安装 | ❌ **未安装** — 无 `C:\Efinity\`，无 `C:\Program Files\Efinity\`，无 CLI in PATH |
| 解压工具 | ❌ 本机无 `unrar` / `7z` / `WinRAR` 可用 |

方案 v0.1 说"Claude 首轮必须先确认 Efinity CLI 的真实命令名和参数"，并在 `efinity_locate_toolchain` 中假定工具可能已安装。**事实是工具链完全不存在。** 需要：

- 追加 **Phase -1：Efinity 工具链安装** 阶段到实施顺序中。
- 明确 RAR 解压方式（本机无解压工具，需用户手动安装 7-Zip 或 WinRAR 后解压）。
- 补丁是 `run.bat` 脚本形式，需在安装后运行。
- Efinity 2025.2 的默认安装路径为 `C:\Efinity\2025.2\`，安装后 `efx_run` / `efx_pgm` 应位于 `C:\Efinity\2025.2\bin\`。MCP 应硬编码这个路径作为搜索起点。

### 2. Python 3.13 与 pymycobot 不兼容 — 必须使用 Python 3.10 虚拟环境

| 项目 | 状态 |
|------|------|
| 全局 Python | 3.13.1 — pymycobot **兼容性未验证** |
| pymycobot | ❌ 未安装 |
| pydantic | ✅ 已安装（全局） |
| pyserial | ✅ 已安装（全局） |
| mcp SDK | ❌ 未安装（全局和 pfmval_py310 均无） |

方案 v0.1 提到"优先不污染全局 Python"，但未指定目标 Python 版本。**已有一个现成的 Conda 环境可用：**

```
D:\conda_envs\pfmval_py310\  (Python 3.10.20, pip 26.0.1)
```

建议：
- **MCP 强制使用 `pfmval_py310` 作为运行环境**，或为此项目创建一个专用环境。
- 在该环境中安装 `mcp`、`pyserial`、`pydantic` 和 `pymycobot`。
- 方案应写明：`conda install -n pfmval_py310 pip` + 在该环境下 pip install。
- 如果 `pfmval_py310` 有其他用途需要隔离，新建 `fpga_mcp_py310` 环境。

### 3. MCP 服务注册 — 方案未说明如何在 Claude Code / Codex 中接入

方案描述了如何**构建** MCP 服务，但没有描述如何**注册**它。实际使用需要：

- 在 `.claude/settings.json`（项目级或全局级）中添加 MCP 条目：
  ```json
  {
    "mcpServers": {
      "fpga_robot_mcp": {
        "command": "D:\\conda_envs\\pfmval_py310\\python.exe",
        "args": ["-m", "fpga_robot_mcp.server"],
        "env": {
          "FPGA_ROBOT_MCP_CONFIG": "D:\\第十届集创赛-雄芯院材料\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
        }
      }
    }
  }
  ```
- 方案需要说明是注册到项目级（`项目根/.claude/settings.json`）还是全局（`~/.claude/settings.json`）。
- 建议项目级，方便团队成员同步。

### 4. 无手动回退路径 — MCP 故障时无 B 计划

比赛截止日期 2026-07-20 临近，MCP 作为开发期辅助工具如遇依赖冲突、Python 版本问题、Claude Code 升级导致 MCP 协议变化等，必须有明确的故障回退路径。

建议：
- 方案增加 **"手动等效操作指南"** 章节，对应每个 MCP 工具给出 PowerShell 命令等价物。
- 例如：`efinity_check_project` → `查看 mem_test.xml 是否存在并校验 XML 结构`。
- 确保即使 MCP 挂了，核心工作（Efinity 构建、串口测试、烧录）也能手动完成。

---

## ⚠️ 中等风险与建议

### 5. `board_list_uart_candidates` 缺少 JTAG 链检测（R1）

方案提到了 JTAG/BSCAN 经验链路，`board_list_uart_candidates` 只枚举 COM 口，不检测 JTAG 链。开发板 JTAG 连接状态对烧录至关重要。

建议：增加 `board_check_jtag_chain` 工具（只读），尝试通过 `efx_pgm --scan` 或等效命令探测 JTAG 链上的设备。

### 6. Review Packet 路径二义性（R2）

方案写 Review Packet 落点为 `final_project/docs/review_packets/`，但当前已存在的评审包在 `方案评审/001_项目文件架构方案/` 和 `方案评审/002_机械臂MCP构建实施方案/`。存在两套路径：
- 方案评审存放路径（按主题组织）
- 工程内 review_packets 目录（按时间/事件组织）

建议统一：评审包（Codex Gate 触发的包）放 `方案评审/NNN_名称/`，日常构建/动作日志放 `final_project/docs/evidence/`。

### 7. 缺少 UART 协议桥接测试工具（R3）

板上 CPU 到 myCobot 的 UART 桥接是决赛关键链路。方案只有 `board_generate_uart_test_plan`（写文档）。建议补充 `board_uart_loopback_test`（只读），在 CPU 侧发送 UART 测试帧并验证回环，不涉及机械臂动作。

### 8. BSCAN 仿真器可用性未核查（R4）

方案未检查本机是否有 JTAG 仿真器（Efinix 的 Download Cable 或兼容设备）。烧录 bitstream 需要它。第 7 条的 JTAG 链检测可以涵盖这个。

### 9. 配置文件的敏感字段处理未说明（R5）

`fpga_robot.local.json` 包含本机路径、串口配置等，方案说提交 `.example.json` 即可。但未说明真机测试时的 token 存储方式——`I_CONFIRM_*` token 是硬编码进配置还是每次对话由用户提供？

建议：token 不在任何文件中持久化保存，每次由用户在对话中提供。

### 10. **本机无 RAR 解压工具** — 工具链安装的第一个障碍（R6）

如第 1 条所述，Efinity 安装包是 `.rar` 格式，本机 `which unrar`、`which 7z`、`which winrar` 全部返回空。用户需要：
- 手动下载并安装 [7-Zip](https://www.7-zip.org/) 或 WinRAR。
- 或用 Python 的 `pyunpack`/`patool`（但可能不支持 RAR5）。
- 该步骤与 MCP 无关但必须在 Phase -1 完成。

### 11. MCP 测试策略不完整（R7）

方案只写"使用 MCP Inspector 验证"。对于本机场景，建议：
- Phase 1 完成时用 `uvx mcp-inspector` 或直接 `python -m fpga_robot_mcp.server` 验证 stdio 启动。
- 准备一组 `test_*.py` 直接调用工具函数测试逻辑，不依赖 MCP 协议层。
- 当前 `tests/` 目录下只有 README，需要实际测试文件。

---

## ✅ 方案亮点

尽管有上述问题，方案在以下方面设计合理：

1. **三阶安全门控**：只读 → 写构建产物 → 硬件副作用，层级清晰、token 确认规则合理。
2. **工具粒度**：每个工具职责单一，组合不重叠。`mycobot280_read_angles` 与 `mycobot280_read_coords` 分开是明智的。
3. **状态记录完整**：`mycobot280_export_session_log` 和 `efinity_collect_logs` 构成可追溯的操作链。
4. **工具命名前缀规整**：`fpga_robot_`、`efinity_`、`board_`、`mycobot280_` 便于 Claude 自动补全。
5. **Codex Gate 触发条件清晰**：烧录、动作、连续失败、多子系统变更都有明确的门控规则。
6. **不进入比赛闭环**：正确地把 MCP 定位为开发期辅助，不替代正式赛时逻辑。

---

## 需要你手动安装的软件清单

按优先级排列，[ ] 为待办，[x] 为已完成：

### 第 0 优先级（工具链前提，必须立即做）

- [ ] **7-Zip 或 WinRAR** → 解压 `赛方提供材料/EDA软件/00 Efinity 2025.2.rar`
  - 本机无任何 RAR 解压工具，这是第一步。
  - 下载地址：https://www.7-zip.org/（免费）

### 第 1 优先级（Efinity 工具链，必须立即做）

- [ ] **Efinity 2025.2 主程序**
  - 位置：`赛方提供材料/EDA软件/00 Efinity 2025.2.rar`
  - 安装路径推测：`C:\Efinity\2025.2\`
  - 约需 10-20 GB 磁盘空间
- [ ] **Efinity 2025.2 补丁**
  - 位置：`赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch/`
  - 安装后运行 `run.bat` 打补丁

### 第 2 优先级（MCP 运行环境，本周内做）

- [ ] **MCP Python 依赖**（在 `pfmval_py310` conda 环境中安装）：
  ```bash
  D:\miniconda\Scripts\conda.exe activate pfmval_py310
  pip install mcp pymycobot pyserial pydantic
  ```
  - Python 3.10.20 已在 `D:\conda_envs\pfmval_py310\` 可用
  - `pyserial` 和 `pydantic` 当前全局 Python 3.13 有，但 conda 3.10 环境内没有

### 第 3 优先级（机械臂联调，Phase 3 前做）

- [ ] **CP210x 串口驱动**
  - 位置：`赛方提供材料/大象机械臂mycobot–280安装调试说明/相关软件/CP210x_VCP_Windows/`
  - 安装后重新插拔 USB-TTL 线，检查设备管理器中是否出现 Silicon Labs CP210x COM 口
- [ ] **myBlockly（可选，开发期调试用）**
  - 位置：`赛方提供材料/大象机械臂mycobot–280安装调试说明/相关软件/myblockly.Setup.1.3.6.exe`
  - 用于在 PC 上验证机械臂串口通信和初始点位

### 不需要安装（已确认就绪）

- [x] **Python 3.10.20** ✅ — 通过 `pfmval_py310` conda 环境可用
- [x] **pip** ✅ — 环境内 `pip 26.0.1` 可用
- [x] **pydantic（全局）** ✅
- [x] **pyserial（全局）** ✅

---

## 建议的修正后实施顺序

```text
Phase -1: Efinity 工具链安装（新增）       ← 立即开始
  ├ 用户安装 7-Zip → 解压 Efinity 2025.2.rar → 安装
  ├ 运行补丁 run.bat
  └ 确认 efx_run / efx_pgm 可用

Phase 0: 只读核验与 MCP 环境准备
  ├ 在 pfmval_py310 中安装 MCP 依赖
  ├ 确认 Efinity 安装路径
  ├ 本文件（审核意见）定稿
  └ 生成初始 Review Packet

Phase 1: MCP 骨架实现（原方案 Phase 1）
  ├ 创建 fpga_robot_mcp Python 包
  ├ 只读工具：status / efinity_check / board_list_uart / mycobot_check_env
  ├ MCP 服务注册到项目 .claude/settings.json
  └ 用 MCP Inspector 或 tool call 验证调起

Phase 2-5: 按原方案继续
```

---

## 需要 Codex 确认的两个设计问题

1. **单服务 vs 双服务**：原方案问是否把 Efinity 工具和 myCobot 工具拆成两个 MCP。建议保持单服务——本项目只有两个开发者（Claude + Codex），单服务减少注册/调试成本；代码内用模块拆分即可满足关注点分离。

2. **Review Packet 路径**：建议区分"方案评审包"和"日常操作日志"。前者按 `方案评审/NNN_名称/` 存放，后者走 `final_project/docs/evidence/`。
