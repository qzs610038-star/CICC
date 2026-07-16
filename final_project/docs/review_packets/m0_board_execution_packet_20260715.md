# M0 板级上板执行包

> **状态：NOT VERIFIED — 当前无 FPGA 板卡，只能准备，不得勾选 PASS**
> 日期：2026-07-15
> 来源 Agent：Claude（Fable 5，无硬件准备任务）
> 目标工程：`competition_project_single_camera/mem_test.xml`
> 器件：TJ375N529，时序模型：I3
>
> **关键约束**：本文件所有板级检查项（冷启动、持续运行、画面验证、烧录）均为 **PENDING BOARD**。没有 FPGA 板卡时只能准备流程，不能勾选 PASS、不能声称板级闭环。

---

## 1. Efinity 版本与工程路径

| 项目 | 值 |
|---|---|
| Efinity 版本 | `2025.2.288.4.15` |
| 目标器件 | Titanium `TJ375N529` |
| 时序模型 | `I3` |
| 工程文件 | `competition_project_single_camera/mem_test.xml` |
| 约束文件 | `competition_project_single_camera/constrain.sdc` |
| 源码根目录 | `competition_project_single_camera/src/` |
| IP 配置 | `competition_project_single_camera/ip/` |

**Efinity 已安装** — `D:\Efinity\2025.2\bin\efinity.exe` 及 `efx_run.bat` 均已确认存在。器件和工程文件与 `mem_test.xml` 声明一致（TJ375N529/I3）。无板条件下允许命令行构建（Map、PNR、STA、CDC、bitstream），禁止烧录、冷启动和 HDMI 画面验证。详见 §8。

## 2. Map → PNR → STA → Bitstream 执行顺序

构建流程按 Efinity 标准 flow 顺序执行，不得跳过步骤：

```
Step 1: Map (综合 + 映射)
   ↓
Step 2: PNR (布局布线) — 仅在 Map PASS 后执行
   ↓
Step 3: STA (静态时序分析) — 仅在 PNR PASS 后执行
   ↓
Step 4: Bitstream 生成 — 仅在 STA 无 blocking violation 后生成
```

### 2.1 每步产物

| 步骤 | 产物 | 检查 |
|---|---|---|
| Map | `outflow/mem_test.map.rpt`、`outflow/mem_test.warn.log` | exit 0、无 fatal error |
| PNR | `outflow/mem_test.route.out` | exit 0、无未约束 IO 断言 |
| STA | `outflow/mem_test.timing.rpt` | 无 setup/hold violation（负 slack） |
| CDC | `outflow/mem_test.cdc.rpt` | 无 Synchronizer warning |
| Bitstream | `outflow/mem_test.bit`、`outflow/mem_test.pgm.out` | SHA-256 记录 |
| 资源 | `outflow/mem_test.map.rpt` | ADD/LUT4/FF/RAM10/DPRAM10 记录 |

### 2.2 历史基线（仅参考，不绑定当前源码）

| 指标 | 历史值（旧镜像 bitstream） |
|---|---|
| 资源 | ADD 2416 / LUT4 14540 / FF 11516 / RAM10 211 / DPRAM10 4 |
| CDC | 无 Synchronizer warning |
| 时序 | 存在跨时钟 setup 负 slack，最差 `mipi_clk -> i_sysclk_div2 = -1.433ns` |
| 历史 bitstream SHA-256 | `A99F14AB8922783C51A71953288F9A25DE1C70DC3E3962F5508BBF53EF75057C` |

> **注意**：历史 bitstream SHA-256 只绑定历史可运行镜像，不绑定当前仓库源码。本次构建必须生成新的 SHA-256 并记录。

**PENDING BOARD** — 未执行任何 Efinity 构建步骤。

## 3. 烧录文件与 SHA-256 记录项

### 3.1 烧录前检查

- [ ] PENDING BOARD：确认板卡 JTAG 连接正常，Efinity Programmer 可识别器件。
- [ ] PENDING BOARD：确认板卡供电正常（12V DC）。
- [ ] PENDING BOARD：摄像头连接 J48/ch0。
- [ ] PENDING BOARD：HDMI 连接显示器/采集卡。

### 3.2 烧录记录表

| 项目 | 值 |
|---|---|
| 烧录时间 | PENDING BOARD |
| Bitstream 路径 | PENDING BOARD |
| Bitstream SHA-256 | PENDING BOARD |
| Programmer 版本 | PENDING BOARD |
| 烧录方式（JTAG SRAM / SPI Flash） | PENDING BOARD |
| 烧录结果 | PENDING BOARD |

**PENDING BOARD** — 无 bitstream、未烧录。

## 4. J48/ch0 → HDMI 接线与检查项

### 4.1 接线拓扑

```
摄像头 → J48 (ch0 MIPI CSI) → FPGA → HDMI 输出 → 显示器
```

### 4.2 检查清单

- [ ] PENDING BOARD：摄像头 FPC 排线完整插入 J48 接口
- [ ] PENDING BOARD：FPC 排线方向正确（触点朝上/下按硬件文档确认）
- [ ] PENDING BOARD：HDMI 线缆连接 FPGA 板 HDMI 输出到显示器
- [ ] PENDING BOARD：显示器已上电并可接收 HDMI 信号
- [ ] PENDING BOARD：摄像头镜头无遮挡、对焦未偏移
- [ ] PENDING BOARD：光照条件稳定（无频闪干扰）

**PENDING BOARD** — 未接线。

## 5. 冷启动 3 次记录表

每次冷启动前必须完全断电（拔 12V DC）至少 10 秒，再重新上电。

| 次数 | 时间 | 上电后画面 | 是否有花屏/冻结/纯色 | 结果 |
|---|---|---|---|---|
| 第 1 次 | PENDING BOARD | PENDING BOARD | PENDING BOARD | PENDING BOARD |
| 第 2 次 | PENDING BOARD | PENDING BOARD | PENDING BOARD | PENDING BOARD |
| 第 3 次 | PENDING BOARD | PENDING BOARD | PENDING BOARD | PENDING BOARD |

**Gate 条件**：3 次冷启动全部正常画面 (PASS) 方可通过。

**PENDING BOARD** — 未上板。

## 6. 连续运行 10 分钟记录表

| 项目 | 值 |
|---|---|
| 开始时间 | PENDING BOARD |
| 结束时间 | PENDING BOARD |
| 画面是否持续正常 | PENDING BOARD |
| 是否出现花屏/条纹 | PENDING BOARD |
| 是否出现冻结 | PENDING BOARD |
| 是否出现 fallback 纯色 | PENDING BOARD |
| 结果 | PENDING BOARD |

**Gate 条件**：10 分钟连续运行无花屏、无冻结、无纯色轮切方可通过。

**PENDING BOARD** — 未上板。

## 7. 停止与回退规则

### 7.1 画面异常（烧录后）

画面异常包括：无输出、花屏、条纹、冻结、纯色轮切、帧率明显下降、分辨率异常。

1. **立即停止**：不要继续冷启动或长时间运行。
2. **保留现场**：截屏/拍照记录当前画面现象。
3. **检查接线**：确认 J48 和 HDMI 物理连接、摄像头 FPC 方向。
4. **禁止猜测修复**：不要修改 RTL、约束、IP 配置或 XML 试图"修一下试试"。
5. **记录并报告**：将现象、相关日志和接线照片交给 Codex 核查。

### 7.2 Map 构建失败

1. **保留完整日志**：`outflow/mem_test.warn.log`、Efinity console 输出。
2. **检查工具版本**：确认 Efinity `2025.2.288.4.15` 和补丁已安装。
3. **检查工程路径**：确认工程路径为 ASCII junction（`D:\cicc_cbm_link`），避免中文路径 `filesystem error: Illegal byte sequence`。
4. **禁止修改源码**：Map 失败不等于 RTL 需要修改。先检查工具、路径、许可证。
5. **不跳过到 PNR**：Map 失败不得尝试 PNR 或生成 bitstream。

### 7.3 PNR 构建失败

1. **检查约束**：确认 `constrain.sdc` 和 `mem_test.peri.xml` 未被修改。
2. **检查 IO**：工具报告未约束 IO 或 `outpad` 断言时，不盲补约束。
3. **保留日志**：`outflow/mem_test.route.out`。
4. **禁止绕过**：不得通过删除端口、删除约束或关闭检查来"让 PNR 通过"。

### 7.4 通用回退规则

- 任何失败先退回历史可运行镜像（SHA-256 `A99F14...`）确认板卡和接线仍然正常。
- 回退确认通过后才允许重试新构建。
- 连续 2 次新构建失败 → 停止，生成 Review Packet，等待 Codex 分析。

**PENDING BOARD** — 未触发任何停止条件。

## 8. 无 FPGA 板卡时的构建与验证边界

当前（2026-07-15）本机已安装 Efinity 2025.2（`D:\Efinity\2025.2\bin\efinity.exe`、`efx_run.bat` 均确认存在）但无 TJ375N529 板卡。以下操作**强制执行**：

| 允许 | 禁止 |
|---|---|
| ✅ 准备本执行包文档 | ❌ 勾选任何板级 PASS |
| ✅ 确认工程文件存在且可读 | ❌ 声称板级闭环 |
| ✅ 确认 Efinity 版本、许可证和工程器件 | ❌ 烧录/冷启动/上板 |
| ✅ 运行 Map、PNR、STA、CDC 报告 | ❌ 验证 HDMI 画面 |
| ✅ 生成 bitstream（仅当前置 flow 允许时） | ❌ 从历史 bitstream 反推当前源码状态 |
| ✅ 记录新 bitstream SHA-256 | ❌ 以 BUILD COMPLETE 冒充 M0 PASS |
| ✅ 运行 Host/compile-only CPU 测试 | ❌ 修改 RTL/SDC/XML/IP 试图修复构建失败 |
| ✅ 运行静态接口审计 | |
| ✅ 记录 SHA-256 表项和检查清单 | |

**即使构建和 bitstream 成功，也只能标记为 `BUILD COMPLETE / BOARD VALIDATION PENDING`，不能标记为 M0 PASS。**

---

## 验证

```powershell
git diff --check
```

**PENDING BOARD** — 本执行包的所有板级检查项均为 PENDING BOARD，无任何 PASS 被勾选。

---

## 参考

- M0 人工构建记录模板：`competition_project_single_camera/docs/debug_sessions/m0_manual_build_board_check_20260714.md`
- M0 基线审计：`competition_project_single_camera/docs/review_packets/m0_demo_baseline_review_packet_20260714.md`
- CURRENT_STATE.md 2026-07-14 单摄候选条目
