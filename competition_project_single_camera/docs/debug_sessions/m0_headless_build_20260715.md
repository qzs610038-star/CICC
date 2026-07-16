# M0 无板命令行构建记录

> 日期：2026-07-15
> 来源 Agent：Claude（Fable 5，无 FPGA 板卡，命令行构建）
> 工程：`competition_project_single_camera/mem_test.xml`
> 构建类型：HEADLESS（命令行，不打开 GUI）
> 整体结果：**MAP FAIL — BUILD STOPPED**
> 记录状态：**APPROVED FAILURE RECORD — EVIDENCE ONLY**（2026-07-15 Codex2 最终确认）
>
> **修正范围**：本记录已经 Codex2 审查并修正根因分类、参数传播链、warning 统计、IV/license 表述、源码身份和 outflow/junction 表述。修正未改变 MAP FAIL 结论，未修改任何 RTL、SDC、XML 或 IP 设置。

---

## 0. 构建前状态

### Git 身份

| 项目 | 值 |
|---|---|
| HEAD commit | `df45b5ac0362ef1da8cd54a9ceecfd238ffcea3e` |
| 消息 | `merge: sync main 39e8a92 into CPU branch` |
| 分支 | `dev/wsc6090-CPU` |
| Dirty | ` M CURRENT_STATE.md`、` M final_project/cpu/CPU_MODULE_PLAN.txt`（均为预期修改） |

### 关键工程文件 SHA-256

| 文件 | SHA-256 |
|---|---|
| `competition_project_single_camera/mem_test.xml` | `95737324033E0B99316A26637FDF1C5BD99B89426FB6A1B69A329816739A8B1C` |
| `competition_project_single_camera/constrain.sdc` | `8A9CB9E7491F169B4E2A6D38F44ED33002201D155B22CD1FC30028A4F7C63102` |
| `competition_project_single_camera/mem_test.peri.xml` | `507D0ABC16A8AB076C06D0550303760FF15897388233DACD8161AD000E2B0768` |
| `competition_project_single_camera/src/top.v` | `7F6537C5F9544709E66997B685361BC81D25236A6C6C6A166711A8BD4CA0828D` |
| `competition_project_single_camera/src/mipi_dsi/dsi_tx_top.v` | `789BD0EAF9C3AE4D2B5D01410A41E6FBEB3F1E406AF1D5F29ECAF2EE4924559D`（当前工作树） |
| 初始 manifest `dsi_tx_top.v` | `6385FD5A560FFA0CC56AF8C32C8F0A2679020749EECC43EBCB386AF87E4AD3E4`（历史 CRLF 字节快照） |

### 源码身份差异说明

- 当前 `dsi_tx_top.v` SHA-256（`789BD0EA...924559D`）与初始 manifest/原始镜像字节 SHA（`6385FD5A...E4AD3E4`）不同。
- 内容核查未发现功能语义差异，差异与 CRLF/LF 字节表示一致。
- 当前 Git HEAD 与工作树语义内容一致。
- 初始 manifest 是历史 CRLF 字节快照，不能再直接作为当前文件字节 SHA。

未来路径修复必须：
1. 记录修改前完整 SHA-256。
2. 记录修改后完整 SHA-256。
3. 更新 delta manifest。
4. 更新 WORK_LOG.md。
5. 更新 M0 Review Packet。
6. 从 Map 重新开始构建。

### 工具与环境

| 项目 | 值 |
|---|---|
| Efinity 版本 | `2025.2.288.4.15`（编译时间 Mar 30 2026） |
| efinity.exe | `D:\Efinity\2025.2\bin\efinity.exe` — EXISTS |
| efx_run.bat | `D:\Efinity\2025.2\bin\efx_run.bat` — EXISTS |
| RISC-V 交叉编译器 | NOT FOUND（`D:\Efinity\2025.2\` 全盘搜索无 `riscv-none-embed-gcc.exe`） |
| 许可证 | 未显式检出 license.dat；Efinity 和 Map 进程能够启动并进入 elaboration，不能据此确认许可证整体可用 |
| 工程目标器件 | Titanium `TJ375N529`，时序模型 `I3`（来自 `mem_test.xml`） |
| outflow/ | 本轮 outflow 中仅发现五个同时间戳的 Map 日志（`.map.out`、`.warn.log`、`.err.log`、`.info.log`、`.log`）；未发现 PNR、STA、CDC、bitstream、hex 或 programmer 产物。因未保存构建前目录清单，不能独立证明 outflow 在构建前不存在 |
| ASCII junction | 本轮构建未使用 `D:\cicc_cbm_link`；Codex2 审查时该路径不存在。CURRENT_STATE.md 存在历史使用该 junction 的记录，不能外推为历史上从未存在 |

### IV / 许可证补充

真实 Map 日志（`mem_test.warn.log`、`mem_test.map.out`、`mem_test.err.log`）中**没有** `cannot find correct IV value` 文字。该文字曾在未保存的控制台前置输出中观察到，但未进入本轮提供的 Map 日志，含义未确认。不得将其计入 `mem_test.warn.log`、不得写成 EFX-WARNING 事件、不得断言它是许可证错误或加密初始化向量错误。当前只能确认：Efinity 和 Map 进程能够启动并进入 elaboration。

---

## 1. Map

### 命令

```powershell
Set-Location "D:\CICC w\competition_project_single_camera"
& "D:\Efinity\2025.2\bin\efx_run.bat" --prj -f map mem_test.xml
```

### 结果：FAIL

| 项目 | 值 |
|---|---|
| 退出码 | **1** |
| 状态 | **FAIL** |
| 耗时 | 1.03 秒 |
| 首个 fatal/error | `[EFX-0010 VERI-ERROR] cannot open file '/src/mipi_dsi/Panel_1080p_reg.mem' (VERI-1012)` |
| 首个 error 位置 | `src/mipi_dsi/true_dual_port_ram.v:52` — `$readmemh` 无法打开文件 |
| 终止原因 | `[EFX-0021 ERROR] Elaboration of module 'top' failed.` |

### 根因分析

**错误类型：MULTI-FACTOR。**

#### 因子 1：HDL PORTABILITY DEFECT

- 唯一实际生效的根路径来源：`src/mipi_dsi/dsi_tx_top.v:144`
- 该处显式覆盖：`.INITIAL_CODE("/src/mipi_dsi/Panel_1080p_reg.mem")`
- 以 `/` 开头的路径依赖外部磁盘根目录布局，不是可携带的工程相对路径。
- 这不是本轮 Claude 新增的缺陷；原始赛方/D 盘镜像中也存在同一根路径字符串。

#### 因子 2：PROJECT COPY/LAYOUT / ENVIRONMENT TRIGGER

- 候选工程正确包含：`src/mipi_dsi/Panel_1080p_reg.mem`（SHA-256 `1B192B20B57BDA019D39B24126789C582AE5DA0B9DF5395152854A8C2C4576B7`，已在 M0 manifest CSV 中确认）。
- 但没有复现原历史构建可能依赖的驱动器根目录 `/src` 布局。
- 当前 Windows 命令行构建因此无法解析该路径。

#### 明确排除

- 不是 `.mem` 文件漏复制。
- 不是仓库路径包含空格导致的首个错误。
- 不是纯环境问题。
- 当前日志证明的是源码可移植性缺陷被当前布局触发。

### 参数传播链（真实链路）

Elaboration 日志（`mem_test.map.out:394-395`）确认实际生效的参数值：

1. **`dsi_tx_top.v:144`** — 显式覆盖（唯一实际生效的根路径来源）：
   ```verilog
   .INITIAL_CODE("/src/mipi_dsi/Panel_1080p_reg.mem")
   ```

2. **`panel_config.v:3`** — 默认值实际为：
   ```verilog
   parameter INITIAL_CODE = "NT35596_1080p_reg.mem"
   ```
   本次默认值被顶层 `dsi_tx_top.v:144` 覆盖，**没有生效**。

3. **`panel_config.v:68`** — 将生效的 `INITIAL_CODE` 传给：
   ```verilog
   .RAM_INIT_FILE(INITIAL_CODE)
   ```
   实际传入 `/src/mipi_dsi/Panel_1080p_reg.mem`。

4. **`true_dual_port_ram.v:22`** — 默认值实际为：
   ```verilog
   parameter RAM_INIT_FILE = "ram_init_file.mem"
   ```
   本次默认值同样被上游参数覆盖，**没有生效**。

5. **`true_dual_port_ram.v:52`** — `$readmemh(RAM_INIT_FILE, ram)` 执行时无法打开根路径。

**关键澄清**：`panel_config.v` 和 `true_dual_port_ram.v` 的默认参数值本身**不是** `/src/...`。两个通用模块的默认值是工程无关的占位文件名，只有 `dsi_tx_top.v:144` 的显式覆盖引入了硬编码绝对路径。

### Warning 统计

#### `mem_test.warn.log` 精确统计

| 项目 | 值 |
|---|---|
| 文本行数 | 132 |
| warning 事件数 | 126 |
| warning code 类别 | 13 |
| EFX-WARNING | **0** |

#### 精确分布

| Warning Code | 事件数 |
|---|---|
| VERI-1330（位宽不匹配） | 50 |
| VERI-1209（位宽截断） | 27 |
| VERI-1927（端口未连接） | 16 |
| VERI-2435（端口未连接） | 6 |
| VDB-1002（无驱网线） | 5 |
| VERI-1199（localparam） | 5 |
| VERI-1190（除零） | 5 |
| VDB-1053（端口未连接） | 3 |
| VERI-2457（静态函数赋值） | 3 |
| VERI-1142（$display 综合忽略） | 2 |
| VERI-1173（full_case） | 2 |
| VERI-1959（超宽移位） | 1 |
| VDB-1013（端口未连接） | 1 |
| **合计** | **126** |

#### `map.out` Analysis 阶段 INFO 事件（不在 `warn.log` 的 126 个 warning 事件内）

- **VERI-2561**（`undeclared symbol ... assumed default net`）：**47 条**
- 它们是 **INFO** 事件，不是 VERI-WARNING。
- 但仍是后续必须审查的 RTL 风险，不能忽略。
- 涉及：`csi_rx_controller.sv`、`dsi_tx.sv`、`top.v`、`ddr_rd_buffer.v`、`frame_buffer.v`、`ddr_buffer.v`、`i2c_master_ctrl_top.v`、`mipi_csi_top.sv`、`uvc_top.v`、`hdmi_top.v`、`vid_info_det_v7.v`、`soft_mipi_rx_top.v`。

### Warning 历史身份

这些 warning 来自当前 Demo 基线源码路径，**尚未逐项处置**。目前没有找到与本次构建匹配的历史 warning 事件清单，因此**不能证明 126 个事件与历史构建逐项相同，也不能证明无新增风险**。

以下类别继续需要审查：
- undeclared symbol / default net（VERI-2561，47 条 INFO）
- 位宽不匹配（VERI-1330，50 条）
- 截断（VERI-1209，27 条）
- 除零（VERI-1190，5 条）
- 无驱动网络（VDB-1002，5 条）
- 未连接端口（VERI-1927/VERI-2435/VDB-1013/VDB-1053，共 26 条）
- full_case（VERI-1173，2 条）
- 超宽移位（VERI-1959，1 条）
- CDC 尚未执行，不能从 Map warning 推断 CDC 安全

**不得把任何 warning 标记为可忽略。**

### 资源报告

**无** — Map 在 Elaboration 阶段失败，未进入综合阶段，未产生资源统计（`EFX_ADD`、`EFX_LUT4`、`EFX_FF`、`EFX_RAM10`、`EFX_DPRAM10` 均无数据）。

### 产物

| 产物 | 路径 | 状态 |
|---|---|---|
| map report | `outflow/mem_test.map.out` | 生成（含完整分析 + elaboration 日志） |
| warning log | `outflow/mem_test.warn.log` | 生成（132 文本行，126 warning 事件，13 类别，0 EFX-WARNING） |
| error log | `outflow/mem_test.err.log` | 生成（2 条 error） |
| info log | `outflow/mem_test.info.log` | 生成 |
| log | `outflow/mem_test.log` | 生成 |

---

## 2. PNR — NOT REACHED

Map 失败，按规则不执行 PNR。

---

## 3. STA — NOT REACHED

PNR 未执行，STA 不可用。

| 项目 | 值 |
|---|---|
| setup 最差 slack | **N/A** |
| hold 最差 slack | **N/A** |

---

## 4. CDC — NOT REACHED

无 CDC 报告。

| 项目 | 值 |
|---|---|
| CDC warning | **N/A** |

---

## 5. Bitstream — NOT GENERATED

无 bitstream 生成。

| 项目 | 值 |
|---|---|
| bitstream 路径 | **N/A** |
| bitstream SHA-256 | **N/A** |
| 未生成原因 | Map 在 Elaboration 阶段因路径解析错误失败，后续 flow 全部未执行 |

---

## VERIFIED BUILD FACTS

1. **Efinity 2025.2.288.4.15 已安装且可执行。** `efx_run.bat` 成功启动综合流程，分析阶段正常完成（58 个 Verilog 文件全部通过分析，0.4 秒级）。
2. **工程文件完整。** 所有 53 个 `mem_test.xml` 设计文件、2 个 IP 设置和 4 个编译期依赖均存在并被 Efinity 成功读取。
3. **工程目标器件和时序模型**：Titanium TJ375N529 / I3（来自 `mem_test.xml`，与历史基线一致）。
4. **RISC-V 交叉编译器不可用。** 全盘搜索 `D:\Efinity\2025.2\` 无 `riscv-none-embed-gcc.exe`。

## FAILED/STOPPED STEP

**Map** — `[EFX-0010 VERI-ERROR] cannot open file '/src/mipi_dsi/Panel_1080p_reg.mem'`

根因分类：**MULTI-FACTOR**。
- **因子 1 — HDL PORTABILITY DEFECT**：`dsi_tx_top.v:144` 显式覆盖 `.INITIAL_CODE("/src/mipi_dsi/Panel_1080p_reg.mem")`，以 `/` 开头的路径依赖外部磁盘根目录布局，不是可携带的工程相对路径。
- **因子 2 — PROJECT COPY/LAYOUT / ENVIRONMENT TRIGGER**：候选工程正确包含 `src/mipi_dsi/Panel_1080p_reg.mem`，但没有复现原历史构建可能依赖的驱动器根目录 `/src` 布局，当前 Windows 命令行构建因此无法解析。
- 文件本身存在且完整（SHA-256 已在 M0 manifest 中确认）。
- 不是 `.mem` 文件漏复制、不是路径含空格错误、不是纯环境问题、不是本轮 Claude 新增缺陷。

## WARNINGS NOT YET DISPOSITIONED

- **132 文本行**，**126 个 VERI-WARNING 事件**，**13 个 warning code 类别**，**0 个 EFX-WARNING**。
- **47 条 VERI-2561 INFO**（undeclared symbol / default net）在 `map.out` Analysis 阶段，不在 126 个 warning 事件内，但仍是后续必须审查的 RTL 风险。
- 这些 warning 来自当前 Demo 基线源码路径，尚未逐项处置。目前没有找到与本次构建匹配的历史 warning 事件清单，因此**不能证明 126 个事件与历史构建逐项相同，也不能证明无新增风险**。
- **不能标记为可忽略**。

## BOARD VALIDATION PENDING

- 未烧录
- 未冷启动
- 未验证 HDMI 画面
- 未验证 10 分钟连续运行
- UART2、J52、机械臂均未连接

## NOT VERIFIED

- Map/PNR/STA/CDC/bitstream — Map 失败，其余未执行
- 历史负 slack 路径（`mipi_clk → i_sysclk_div2 = -1.433ns` 等）的约束正确性
- 构建后 bitstream 的板级画面复现
- ch1/DSI 裁剪后的资源和时序
- QCRV32、UART1、APB、按键、特征、OSD、机械臂

---

## 最小修复建议（CODE2 建议，本轮未批准实施）

失败记录中记录 Codex2 建议：

- **首选修复点**：`dsi_tx_top.v:144`
- **原则**：将唯一实际生效的根路径 `/src/mipi_dsi/Panel_1080p_reg.mem` 改成受控工程相对路径。
- **不修改**两个通用模块（`panel_config.v`、`true_dual_port_ram.v`）的默认参数。
- **不创建**`D:\src`。
- **不复制** mem 文件到驱动器根目录。
- **不用** ASCII 镜像伪造根目录布局。

**Codex2 本轮只要求修正失败记录。尚未批准路径修复实施。本轮不得修改 RTL。修正记录获批后，再单独进入路径修复 Gate。**

---

## 状态

**APPROVED FAILURE RECORD — EVIDENCE ONLY**

Codex2 最终确认本失败记录。仅批准失败证据，不批准 Map PASS、warning 可忽略、PNR/STA/CDC/bitstream 或 M0 PASS。路径修复已另行启动。
