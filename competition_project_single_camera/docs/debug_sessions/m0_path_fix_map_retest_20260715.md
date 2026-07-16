# M0 路径修复 Map 重测记录

> 日期：2026-07-15
> 来源 Agent：Claude（Fable 5，路径修复 + Map 重测）
> 工程：`competition_project_single_camera/mem_test.xml`
> 构建类型：HEADLESS（命令行，不打开 GUI）
> 整体结果：**MAP PASS / RECORD CORRECTION PENDING / PNR HOLD**
>
> **范围**：仅路径修复后 Map 重测。PNR/STA/CDC/bitstream 均未运行。M0 仍未通过。板级仍 PENDING BOARD。

---

## 0. 前置：失败记录状态

旧 Map 失败记录 `m0_headless_build_20260715.md` 状态已更新为：

**APPROVED FAILURE RECORD — EVIDENCE ONLY**

旧 Map 失败日志已归档至：
`docs/debug_sessions/evidence/m0_map_fail_pre_path_fix_20260715/`

| 文件 | SHA-256 |
|---|---|
| `mem_test.map.out` | `E0E4D3FE1C64C416015E8D8FA6C3410B8454053ECDDF5F5A375242C2A2D11AD3` |
| `mem_test.warn.log` | `B87B7AE5DAF2654365114DFAA767242E64404D0C7563B7A4874A271919E6A1F2` |
| `mem_test.err.log` | `1705126F46D40F16C8561B6A2CAE0BF41F99A0C7242C8963A4DACEBA43A1C16E` |
| `mem_test.info.log` | `4CC767449EC7352CFEB97C68C97A0BE836E8DAF75DF913591E9364C8693FA1ED` |
| `mem_test.log` | `9857D98E3D4B0357AB72EF37B6B2791B1D43D70B8D884ECEE0881263D0BBB9F3` |

---

## 1. RTL 修改

### 修改目标

修复 `dsi_tx_top.v:144` 的硬编码绝对路径，将 `/src/mipi_dsi/Panel_1080p_reg.mem` 改为受控工程相对路径 `src/mipi_dsi/Panel_1080p_reg.mem`。

### 修改内容

```diff
-	.INITIAL_CODE	("/src/mipi_dsi/Panel_1080p_reg.mem"),
+	.INITIAL_CODE	("src/mipi_dsi/Panel_1080p_reg.mem"),
```

仅删除开头 `/`。

### SHA-256

| 阶段 | SHA-256 |
|---|---|
| 修改前 | `789BD0EAF9C3AE4D2B5D01410A41E6FBEB3F1E406AF1D5F29ECAF2EE4924559D` |
| 修改后 | `DDAF952AE26A599A988235FBE5EB2815AA9C8FDE66B7346EB359A24996F031D7` |

### 未修改文件

`panel_config.v`、`true_dual_port_ram.v`、`constrain.sdc`、`mem_test.peri.xml`、所有 IP `settings.json`、所有 `.mem` 文件、`top.v` 及所有其他 RTL。

注：`mem_test.xml` 在 Map 运行期间被 Efinity 自动改写（`last_run_flow` bitstream→syn、XML 自闭合标签空格格式化），随后按 Codex2 指令恢复到构建前精确 Git 字节状态。Map 证据全部由 `outflow/` 日志承载。

### 修改性质

仅改变文件定位，不改变初始化内容或视频逻辑。板级闭环仍需完整构建。

---

## 2. Map 重测

### 命令

```powershell
Set-Location "D:\CICC w\competition_project_single_camera"
& "D:\Efinity\2025.2\bin\efx_run.bat" --prj -f map mem_test.xml
```

### 结果：PASS

| 项目 | 值 |
|---|---|
| 退出码 | **0** |
| 状态 | **PASS** |
| 旧 `$readmemh` 错误 | **已消除** — `mem_test.err.log` 新运行段无任何 error |
| 首个新错误 | **无** — 新运行段零 error |

### 资源报告

| 资源 | 值 |
|---|---|
| EFX_ADD | 4367 |
| EFX_LUT4 | 19399 |
| EFX_FF | 11800 |
| EFX_DSP48 | 4 |
| EFX_RAM10 | 211 |
| EFX_DPRAM10 | 4 |
| EFX_SRL8 | 1 |
| INPUT PORTS | 1213 |
| OUTPUT PORTS | 1649 |
| 综合耗时 | 64 秒 |

### Warning 统计（新运行段，22:01:42）

#### `mem_test.warn.log`

| 项目 | 旧运行段 (20:59:08, Map FAIL) | 新运行段 (22:01:42, Map PASS) |
|---|---|---|
| 非空行数 | 132 | 1042 |
| VERI-WARNING 事件数 | 126 | 223 |
| VERI-WARNING 类别数 | 13 | 15 |
| EFX-0256 WARNING 事件数 | 0 | 785 |
| EFX-WARNING（其他） | 0 | 0 |

#### 新运行段 VERI-WARNING 精确分布

| Warning Code | 事件数 |
|---|---|
| VERI-1330（位宽不匹配） | 54 |
| VDB-8003 | 48 |
| VERI-1209（位宽截断） | 32 |
| VDB-1002（无驱网线） | 28 |
| VERI-1927（端口未连接） | 21 |
| VDB-1053（端口未连接） | 7 |
| VERI-2435（端口未连接） | 6 |
| VERI-1190（除零） | 5 |
| VERI-2457（静态函数赋值） | 5 |
| VERI-1199（localparam） | 5 |
| VDB-8002 | 4 |
| VDB-1013（端口未连接） | 3 |
| VERI-1173（full_case） | 2 |
| VERI-1142（$display 综合忽略） | 2 |
| VERI-1959（超宽移位） | 1 |
| **合计** | **223** |

#### EFX-0256 WARNING

785 个事件，均为 `primary output port '...' is not driven`（综合阶段才出现的 warning，旧运行段在 elaboration 阶段即失败，未产生此类 warning）。

按顶层端口族分类：axi1 698（TBD/BLOCKING——`is_axi_enable="true"`）、MIPI DSI ch0 70（TBD——仅 ch0 未驱动，ch1 由 `dsi_tx_top_inst1` 正常驱动）、axi0 10（EXPECTED，暂维持）、LCD/外设 3（TBD——与 DSI ch0 关联）、JTAG 2（TBD——`jtag_inst1_TDO`/`jtag_inst2_TDO`，非 axi1 高位；`jtag_inst2` 不在 `mem_test.peri.xml` 中）、LED 2（UNEXPECTED——`top.v` 仅驱动 `led[1:0]`，periphery 声明全部 4 位）。详细静态审计见 `m0_efx0256_interface_audit_20260715.md`（2026-07-15 修正版），PNR 前方案见 `../review_packets/m0_pnr_interface_resolution_plan_20260715.md`。

整文件 warn.log：1177 行，1173 非空行。`Found 587 warnings in post-synthesis netlist` 仅为工具汇总语句，未计入 223 或 785。

#### VERI-2561 INFO（`map.out` Analysis 阶段）

**47 条**，与旧运行段数量相同。不在 223 个带 code 的 VERI-WARNING 事件内，但仍是后续必须审查的 RTL 风险。

### 与旧日志差异摘要

| 维度 | 旧运行段（Map FAIL） | 新运行段（Map PASS） |
|---|---|---|
| 终止阶段 | Elaboration | 综合完成 |
| `$readmemh` 错误 | 存在（根因） | 已消除 |
| VERI-WARNING | 126（13 类别） | 223（15 类别） |
| EFX-0256 | 无（未到达综合） | 785 |
| 新增 VERI-WARNING 类别 | — | VDB-8003、VDB-8002、延迟出现的 VDB/VERI |
| 资源报告 | 无 | 有（ADD 4367 等） |

新增 warning 的主要来源：
1. 综合阶段新暴露的 warning（EFX-0256、VDB-8002/8003），旧运行段未到达此阶段。
2. IP 生成文件中延迟出现的 VERI-WARNING。

---

## 3. PNR — NOT RUN

按指令仅重跑 Map，PNR 未执行。

---

## 4. STA — NOT RUN

---

## 5. CDC — NOT RUN

---

## 6. Bitstream — NOT GENERATED

---

## BOARD VALIDATION PENDING

- 未烧录
- 未冷启动
- 未验证 HDMI 画面
- 未验证 10 分钟连续运行
- UART2、J52、机械臂均未连接

## NOT VERIFIED

- PNR/STA/CDC/bitstream — 均未运行
- 历史上板可运行 bitstream 的负 slack 路径安全性
- 构建后 bitstream 的板级画面复现
- ch1/DSI 裁剪后的资源和时序
- QCRV32、UART1、APB、按键、特征、OSD、机械臂

---

## 状态

**MAP PASS / RECORD CORRECTION PENDING / PNR HOLD**

路径修复成功消除 `$readmemh` 错误。Map 通过但仅是 M0 完整 Gate 的 ~20%。不表示 M0 PASS、warning 可忽略、PNR/STA/CDC/bitstream 或板级闭环。M0 仍为 PENDING BOARD。

记录修正（2026-07-15，Codex2 指令）：
- warning 统计修正：223 VERI/VDB（15 类别），非 224（16 类别）；新运行段 1042 非空行，非 1041；整文件 1177 行/1173 非空行；`Found 587 warnings` 不计入任何统计。
- Efinity 曾自动改写 `mem_test.xml`（`last_run_flow` bitstream→syn、XML 重新格式化），已按 Codex2 要求恢复到构建前精确字节状态。Map 证据全部由 `outflow/` 日志承载。
- 新增 EFX-0256 接口归属静态审计：`m0_efx0256_interface_audit_20260715.md`。

接口审计修正（2026-07-15，Codex2 第二轮指令）：
- AXI1 698 个从 EXPECTED 修正为 TBD/BLOCKING——`mem_test.peri.xml:455` 确认 `axi_target1 is_axi_enable="true"`。
- MIPI DSI 70 个从"整个 DSI 未激活"修正为"仅 ch0 未驱动"——`dsi_tx_top_inst1` 实际驱动 ch1（`ck1/dp10-dp13`）。
- JTAG 新增发现：`jtag_inst2` 不在 `mem_test.peri.xml` 中（仅 `jtag_inst1` 有声明）。
- 删除"785 项无 DDR 主通道问题"的错误表述。
- 新增 PNR 前接口归属解决方案：`../review_packets/m0_pnr_interface_resolution_plan_20260715.md`。
- LED 修复与 AXI/JTAG/MIPI 配置变更分开审查；禁止手改 `mem_test.peri.xml`。

下一步门禁：Codex2 审查修正后的 EFX-0256 审计和 PNR 前接口归属解决方案。批准后方可进入 PNR。
