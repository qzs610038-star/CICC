# CPU↔FPGA 视觉特征接口冻结清单 / 协作确认稿

> 状态：草案，待 FPGA 队友确认  
> 日期：2026-07-11  
> 来源 Agent：Claude（本机 `D:\CICC w` 工作树）  
> 适用范围：FPGA 预处理首阶段（ROI + 统计特征）↔ CPU 识别侧接口  
> 不涉及：RTL 编写、top.v 修改、Efinity 工程修改、APB/SoC 集成、机械臂动作

---

## 0. 当前结论

1. **CPU 侧 5 大模块全部完成**（board_io / vision_classifier / param_table / task_matcher / main），PC 单测全通过，RISC-V 交叉编译通过，上板待验证。
2. **FPGA 预处理首阶段**仅负责 ROI、颜色面积、通用前景 fg_area、bbox、center、sum/count 等原始特征，不做分类或机械臂控制。
3. **接口三文件中 `register_map.md`（v1）已整体过期**，`vision_register_handbook_draft_2026-07-03.md`（v2）和 `board_io.h` 是当前工作基线且内部一致。
4. **FPGA 执行计划 §5.3 的特征快照输出字段**比当前 handbook v2 和 board_io.h 更完整，需要统一。
5. **TARGET_SEL_AVAILABLE=0 和 FG_AREA_AVAILABLE=0 的保护正确**，符合当前阶段（FPGA 寄存器未实现）。
6. **以下内容需要 FPGA 队友在本稿确认后再进入 APB 寄存器 RTL**。

---

## 1. CPU 侧对 FPGA 预处理模块的最小必需字段

以下字段是 CPU 识别主循环 (`main.c`) 正常运行的硬依赖。

| 序号 | 字段 | 宽度 | 来源 | 用途 | 当前状态 |
|------|------|------|------|------|----------|
| 1 | `frame_id` | 16-bit | SYS_STATUS_N[31:16] | 帧握手、防撕裂 | ✅ handbook v2 / board_io.h 已有 |
| 2 | `roi_pixel_count` | 32-bit | 新增 LIVE 寄存器 | CPU 用 sum/count 计算 RGB/Y 均值 | ⚠️ board_io.h 已预留，handbook v2 标记为保留区 |
| 3 | `sum_r` | 32-bit | 新增 LIVE 寄存器 | 颜色分类均值计算 | ⚠️ board_io.h 已预留，handbook v2 标记为保留区 |
| 4 | `sum_g` | 32-bit | 新增 LIVE 寄存器 | 颜色分类均值计算 | ⚠️ board_io.h 已预留，handbook v2 标记为保留区 |
| 5 | `sum_b` | 32-bit | 新增 LIVE 寄存器 | 颜色分类均值计算 | ⚠️ board_io.h 已预留，handbook v2 标记为保留区 |
| 6 | `sum_y` | 32-bit | 新增 LIVE 寄存器 | 亮度辅助判断 | ⚠️ board_io.h 已预留，handbook v2 标记为保留区 |
| 7 | `red_area` | 32-bit | LIVE_RED_AREA_N | 颜色分类（红） | ✅ 已有 |
| 8 | `blue_area` | 32-bit | LIVE_BLUE_AREA_N | 颜色分类（蓝） | ✅ 已有 |
| 9 | `yellow_area` | 32-bit | LIVE_YEL_AREA_N | 颜色分类（黄） | ✅ 已有 |
| 10 | `fg_area` | 32-bit | LIVE_FG_AREA_N (0x0B0/0x1B0) | 通用前景填充率（白/黑物块识别关键） | ⚠️ board_io.h 已预留，handbook v2 仅标为保留区 |
| 11 | `bbox_min` | 32-bit `{y_min[15:0], x_min[15:0]}` | LIVE_BBOX_MIN_N | 物块边界框 | ✅ 已有 |
| 12 | `bbox_max` | 32-bit `{y_max[15:0], x_max[15:0]}` | LIVE_BBOX_MAX_N | 物块边界框 | ✅ 已有 |
| 13 | `center` | 32-bit `{y_cen[15:0], x_cen[15:0]}` | LIVE_CENTER_N | OSD / 日志 / 偏差检查 | ✅ 已有 |
| 14 | `height_px` | 32-bit | LIVE_HEIGHT_PX (0x1AC, Cam1 only) | Cam1 侧面尺寸查表 | ✅ 已有；CPU 也可从 bbox 推导 |
| 15 | `status` | ≥8-bit | 新增 LIVE 寄存器 | 空前景/非法ROI/溢出/丢帧等状态 | ⚠️ board_io.h 已预留，handbook v2 标记为保留区 |

### 1.1 需新增的寄存器偏移建议

基于 handbook v2 当前布局，建议在 Cam0/Cam1 的 LIVE 区域内分配：

| 寄存器 | Cam0 建议偏移 | Cam1 建议偏移 | 说明 |
|--------|--------------|--------------|------|
| `LIVE_ROI_PIXEL_COUNT` | `0x08C` | `0x18C` | ROI 内参与统计的像素总数 |
| `LIVE_SUM_R` | `0x090` | `0x190` | ROI 内 R 通道累加和 |
| `LIVE_SUM_G` | `0x094` | `0x194` | ROI 内 G 通道累加和 |
| `LIVE_SUM_B` | `0x098` | `0x198` | ROI 内 B 通道累加和 |
| `LIVE_SUM_Y` | `0x09C` | `0x19C` | ROI 内亮度累加和 |
| `LIVE_FG_AREA` | `0x0B0` | `0x1B0` | 已由 board_io.h 预留，保持不变 |
| `LIVE_STATUS` | `0x0B4` | `0x1B4` | 帧状态位（见 §3） |

> **注意**：`0x08C-0x09F` 在 handbook v2 §2.4 中标记为 "Cam0 WHITE/BLACK 面积寄存器" 预留区。如果 `sum_r/g/b/y` 占用该区域，白/黑面积寄存器需重新规划或降级（CPU 用排除法可替代）。请 FPGA 队友权衡。

---

## 2. CPU 侧接受的字段语义

### 2.1 坐标与区间

| 项目 | 语义 | 备注 |
|------|------|------|
| **ROI 区间** | 半开区间 `[x0, x1) × [y0, y1)` | 宽 = x1−x0，高 = y1−y0，无 ±1 歧义 |
| **bbox 输出** | 半开区间 `[x_min, x_max) × [y_min, y_max)` | x_max/y_max 为前景像素 max 坐标 + 1（exclusive）；宽 = x_max−x_min（无 ±1） |
| **center** | `(x_min + x_max) / 2`, `(y_min + y_max) / 2` | 整数除，向零截断（FPGA 可用右移 1-bit） |
| **height_px** | `y_max − y_min`（从 bbox 半开区间推导） | 或 FPGA 直接提供 `LIVE_HEIGHT_PX`，两者不能冲突。对齐 CPU `_bbox_area()` |

### 2.2 颜色面积

- `red_area` / `blue_area` / `yellow_area` 必须是 **ROI 内** 对应颜色掩码的像素计数。
- 这些面积互不重叠（一个像素最多命中一种颜色掩码）。
- 面积不包含 `fg_area` 的非颜色部分（白/黑物块）。

### 2.3 通用前景 fg_area

- `fg_area` 必须是 **ROI 内通用前景像素数**（基于 RGB 通道差 + 亮度门限），**不能**等于 `red_area + blue_area + yellow_area`。
- 若 `fg_area = red|blue|yellow`，则白色和黑色物块无法得到有效的 fg_area 和 bbox，形状分类的填充率计算（区分方/圆/三角）将完全失效。
- CPU 使用 `fg_area / bbox_area` 计算填充率；FPGA 不需要做除法。

### 2.4 均值计算

- CPU 使用 `sum_r / roi_pixel_count` 等自行计算均值。
- FPGA **不需要**做除法或浮点运算。
- 若 `roi_pixel_count == 0`，CPU 跳过均值计算并将颜色分类结果置为 UNKNOWN。

### 2.5 空前景 (empty_foreground)

当 ROI 内无通用前景像素时：

```text
fg_area   = 0
bbox_min  = 0
bbox_max  = 0
center    = 0
status.empty_foreground = 1
```

**CPU 在 `empty_foreground=1` 时不得使用 bbox 或 center 做任何决策**，必须返回 UNKNOWN。

### 2.6 尺寸分类

- 首版只依赖 **Cam1 侧面 height_px**（或从 Cam1 bbox 推导的 height）。
- CPU 通过 `param_table` 中的 `height_px_20mm / height_px_25mm / height_px_30mm` 标定表查表分类。
- 不建广角/畸变模型；固定位置下的畸变由实测标定表吸收。
- Cam0 俯视不参与尺寸分类。

### 2.7 形状分类

- `target_shape` 首版固定为 `SHAPE_CUBE`（CPU 识别侧只抓正方体）。
- 形状判别依赖 Cam0 俯视填充率（圆 vs 方）和 Cam1 侧面填充率（方 vs 三角）。
- 填充率 = `fg_area / bbox_area`，由 CPU 计算。

### 2.8 TARGET_SEL 语义

```
bits [1:0]  color_sel    00=通配(颜色必过), 01=红, 10=蓝, 11=黄
bits [3:2]  size_sel     00=通配(尺寸必过), 01=小, 10=中, 11=大
bit  [4]    target_valid  0=暂不匹配/不抓取, 1=目标有效
bits [31:5] 保留
```

- `color_sel=00` 和 `size_sel=00` 表示**通配（该维度必过）**，不表示无效。
- `target_valid=0` 时正式主线必须清空目标并输出 NONE，不沿用旧目标。
- `color_sel=00 && size_sel=00 && target_valid=1` 等价于"看到正方体就抓"，只按形状过滤。
- target_valid 首版使用拨码位，不用瞬时按键。
- FPGA 负责物理开关同步/消抖/编码，CPU 只读稳定寄存器。
- **首版形状不从拨码输入**，CPU 固定为 `SHAPE_CUBE`。

### 2.9 LIVE_FG_AREA 语义

- `LIVE_FG_AREA0` (Cam0 俯视, 0x0B0)：ROI 内通用前景像素总数。
- `LIVE_FG_AREA1` (Cam1 侧面, 0x1B0)：ROI 内通用前景像素总数。
- 当前 `FG_AREA_AVAILABLE=0`（编译期默认），CPU 降级使用 `(red+blue+yellow)_area / bbox_area` 近似填充率。
- **必须等 FPGA 寄存器真实实现且上板验证后**，才能将 `FG_AREA_AVAILABLE` 从 0 改为 1。

---

## 3. status 位建议

| 位 | 名称 | 语义 | CPU 行为 |
|----|------|------|----------|
| `[0]` | `empty_foreground` | ROI 内无通用前景像素 | 不使用 bbox/center，返回 UNKNOWN |
| `[1]` | `invalid_roi` | ROI 配置非法（反向/越界/空） | 丢弃本帧，等待下一帧 |
| `[2]` | `overflow` | 任一累加器溢出（面积或 sum） | 丢弃本帧，报告错误 |
| `[3]` | `dropped_snapshot` | 新帧到达但旧快照未被 CPU ack | CPU 检查 ack 延迟，报告性能告警 |
| `[4]` | `roi_empty` | ROI 内像素数为 0（合法但无数据） | 与 empty_foreground 等价处理 |
| `[7:5]` | 保留 | 置 0 | — |

**CPU 处理流程**：FPGA 通过 valid/ack 协议保证整帧快照原子锁存，因此 status 和其他 LIVE 寄存器的读取顺序不影响数据一致性。CPU 在 `board_io_read_features()` 中完整读取所有 LIVE 字段（含 status），在 `classify_frame()` 入口检查 status 位：`bits[4,2,1,0]` 任一置位 → 直接返回 UNKNOWN，跳过颜色/形状分类。

---

## 4. CPU 不接受的内容（硬边界）

以下内容违反 `AGENTS.md`「分赛区决赛主线」和用户当前指令，CPU 侧不会消费：

| 不接受项 | 原因 | 来源规则 |
|----------|------|----------|
| FPGA 输出最终颜色/形状/尺寸分类 | 分类由 CPU 负责 | AGENTS.md 决赛主线 |
| FPGA 直接控制 myCobot 机械臂 | 机械臂控制由 CPU 负责 | AGENTS.md 决赛主线 |
| 视觉坐标 `grab_center` 直接驱动机械臂实时抓取 | 正式主线采用固定 P_pick 抓取序列 | CPU_MODULE_PLAN §4 |
| `grab_center` 作为正式 arm_controller 接口 | 仅用于 OSD/日志/偏差检查 | CPU_MODULE_PLAN §4/§5 |
| `fg_area = red\|blue\|yellow` | 白/黑物块无 fg_area，填充率计算失效 | CPU_MODULE_PLAN §2 |
| FPGA 内完成除法输出均值 | CPU 用 sum/count 自行计算 | FPGA plan §4.4 |
| 0x06C / 0x0B0 / 0x1B0 当作已由 FPGA 实现 | 当前仅 CPU 预留 | CURRENT_STATE.md |
| 纯 FPGA 视觉识别主线恢复 | 已废弃 | AGENTS.md + CURRENT_STATE.md |

---

## 5. 三份接口文件一致性检查结果

### 5.1 对齐矩阵

| 检查项 | register_map.md | handbook v2 | board_io.h | 判定 |
|--------|:---:|:---:|:---:|------|
| 双路 Cam0/Cam1 寄存器分块 | ❌ 单路 | ✅ 双路 | ✅ 跟随 v2 | register_map 过期 |
| Staging→Commit→Active 协议 | ❌ 无 | ✅ 完整 | ✅ 完整实现 | register_map 过期 |
| REG_MAGIC @ 0x000 | ✅ 独立 | ✅ Magic+Version 合并 | ✅ 跟随 v2 | 语义不同 |
| 0x004 寄存器 | REG_VERSION | SYS_CTRL | SYS_CTRL | **冲突** |
| TARGET_SEL @ 0x06C | 无 | 保留区 | 占位（含保护） | 一致（均待确认） |
| LIVE_FG_AREA @ 0x0B0/0x1B0 | 无 | 保留区 `0x0B0-0x0FF` | CPU 预留（含保护） | 一致（均待确认） |
| roi_pixel_count | 无 | 无（标记为保留区） | board_io.h 已预留，编译开关控制 | CPU 侧已就绪，handbook v2 待更新 |
| sum_r/g/b/y | REG_SUM_* @ 0x34-0x40 | 无（标记为保留区） | board_io.h 已预留，编译开关控制 | register_map 旧定义偏移不同；CPU 侧已就绪 |
| status | 无 | 无（标记为保留区） | board_io.h 已预留，编译开关控制 | CPU 侧已就绪，handbook v2 待更新 |

### 5.2 具体冲突详析

#### 冲突 A：`register_map.md` 整体过期

`register_map.md` 是决赛第一版单路寄存器草案，其布局与当前工作基线（handbook v2 / board_io.h）完全不同：

- 无 Cam0/Cam1 双路分块
- 无 staging/commit 原子提交协议
- 无 OSD 寄存器
- 无 ARM_STATE / ERROR_CODE / TARGET_SEL
- ROI 作为 FPGA→CPU 只读寄存器（当前设计中 ROI 由 CPU 配置）
- 0x00C0-0x00C8 的 TASK_MODE / TARGET_COLOR / TARGET_SIZE 在 v2 中不存在

**推荐**：在 `register_map.md` 顶部增加过期声明，指向 handbook v2 作为当前工作基线。不要同时维护两份不同布局的寄存器表。

#### 冲突 B：`0x004` — REG_VERSION vs SYS_CTRL

| 文件 | 0x004 内容 | 方向 |
|------|-----------|------|
| register_map.md | `REG_VERSION` | FPGA→CPU (R) |
| handbook v2 | `SYS_CTRL` | R/W |
| board_io.h | `OFF_SYS_CTRL` | R/W |

**根因**：v1 将 Magic 和 Version 放在两个独立寄存器；v2 将 Version 合并到 `REG_MAGIC` 高 16-bit，腾出 `0x004` 给 `SYS_CTRL`。

**推荐**：以 handbook v2 / board_io.h 为准（`REG_MAGIC[31:16]=version, REG_MAGIC[15:0]=0x375A`，`0x004=SYS_CTRL`）。register_map.md 的 `REG_VERSION` 废弃。

#### 冲突 C：`sum_r/g/b/y` 和 `roi_pixel_count` — CPU 侧已预留，handbook v2 待更新

FPGA 执行计划 §5.3 明确列出这些特征快照输出。

- **handbook v2**：LIVE 区只有颜色面积 + bbox + center，缺少 sum/count/status 寄存器（标记为保留区）
- **board_io.h**：`feature_snapshot_t` 已新增 roi_pixel_count、sum_r/g/b/y、status 字段，
  偏移宏 `OFF_LIVE_ROI_PIXEL_COUNT0/1` 等已就绪，编译开关 SUM_RGB_AVAILABLE/STATUS_AVAILABLE 默认关闭
- **register_map.md**：有 `REG_SUM_R/G/B/Y` @ 0x34-0x40 和 `REG_COUNT` @ 0x44，但其整体布局已过期，偏移与 v2 不兼容

**当前状态**：CPU 侧代码已就绪，等待 FPGA 队友确认寄存器偏移（见 FPGA_CONFIRMATION_NEEDED.txt [A]-[C]）。
确认后需将 handbook v2 的保留区升级为正式寄存器定义。

#### 冲突 D：ROI 命名暗示歧义

handbook v2 使用 `CFG_CAM0_ROI_TL` (MIN_Y, MIN_X) 和 `CFG_CAM0_ROI_BR` (MAX_Y, MAX_X)，命名中的 "MAX" 可能被误读为闭区间上界。

FPGA 计划 §4.2 和 CPU_MODULE_PLAN 均明确使用半开区间 `[x0, x1) × [y0, y1)`。

**推荐**：handbook v2 中 ROI 寄存器改名为 `CFG_CAM0_ROI_X0Y0` / `CFG_CAM0_ROI_X1Y1`，并在注释中写明半开区间语义。或在当前命名下增加注释 `// x1, y1 are exclusive (half-open)`。

### 5.3 保护开关验证 ✅

| 开关 | board_io.h 默认值 | 行为 | 判定 |
|------|:---:|------|:---:|
| `FG_AREA_AVAILABLE` | `0` | `board_io_read_features()` 不读 0x0B0/0x1B0，`fg_area` 填 0 | ✅ 正确 |
| `TARGET_SEL_AVAILABLE` | `0` | `board_io_read_target_sel_raw()` 返回 0（target_valid=0），task_matcher 清空目标返回 NONE | ✅ 正确 |
| `HANDSHAKE_MODE` | `INDEPENDENT` | `MERGED` 触发 `#error` | ✅ 安全 |

---

## 6. 等 FPGA 队友确认的问题清单

### 6.1 立即确认（阻塞阶段 B RTL 开始）

- [ ] **Q1**: 通道映射表（`stream_id` ↔ 物理接口 ↔ RTL `ch0/ch1` ↔ `wb0/wb1` ↔ CPU Cam0/Cam1 ↔ 俯视/侧视角色）是否已有结论？参见 FPGA plan §3.3 映射表。
- [ ] **Q2**: 48-bit 双像素字节序 `{B1,G1,R1,B0,G0,R0}` 是否经最小仿真确认？pixel 0 = 偶像素，pixel 1 = 右侧奇像素？
- [ ] **Q3**: `de` 与 `valid` 的实际关系（是否恒等）是否已通过 Debayer 波形确认？
- [ ] **Q4**: 预处理最终取 Debayer 原始 RGB 还是白平衡后 RGB？哪个信号名？
- [ ] **Q5**: 本稿 §1.1 的 `sum_r/g/b/y`、`roi_pixel_count`、`fg_area`、`status` 新增寄存器偏移建议是否可接受？或有其他分配方案？
- [ ] **Q6**: 0x08C-0x09F 原预留为 "WHITE/BLACK 面积寄存器"，若被 sum 寄存器占用，白/黑面积是否降级为 CPU 排除法（当前方案）？
- [ ] **Q7**: `LIVE_ROI_PIXEL_COUNT` 的含义是 "ROI 内参与统计的全部像素数" 还是 "ROI 内通过亮度门限的像素数"？需要 FPGA 队友明确定义。

### 6.2 进入 APB 集成前确认（阶段 E 阻塞门）

- [ ] **Q8**: `soc.h` 中真实 APB user window 基地址和选用的 `io_apbSlave_x_*` 是否已确认空闲？
- [ ] **Q9**: `PADDR` 宽度是否 ≥ 9 bit（覆盖 `0x000-0x1FF`）？
- [ ] **Q10**: APB 时钟、复位、`PREADY`、`PSLVERROR` 语义是否已记录？
- [ ] **Q11**: UART、CLINT、PLIC、AXI 和 DDR 地址是否与新寄存器窗口无冲突？
- [ ] **Q12**: 正式寄存器偏移合并方案：handbook v2 和 board_io.h 是否需要合并为单一受控文档？建议以 handbook v2 为单一真相源，board_io.h 从之。
- [ ] **Q13**: TARGET_SEL 最终地址（当前建议 0x06C）和实际占用的物理拨码引脚？当前 `i_sw[0]` (复位) 和 `i_sw[1]` (HDMI 切换) 已被占用。
- [ ] **Q14**: `wb0_*`（Cam1 侧面）是否已接入 feature_extract？当前 handbook v2 标记为"需 FPGA 队友改接"。
- [ ] **Q15**: Cam1 `LIVE_HEIGHT_PX` (0x1AC) 是 FPGA 直接从 bbox 推导 (`y_max − y_min`，对齐半开区间)，还是 CPU 从 bbox 自行计算？两者不能冲突。

### 6.3 语义确认（不阻塞但需提前对齐）

- [ ] **Q16**: 通用前景掩码的 RGB 通道差阈值和亮度门限默认值由谁提供首版标定值？CPU 可在运行时通过 `param_table` 覆盖。
- [ ] **Q17**: 背景参考值 `bg_r/bg_g/bg_b` 的更新策略：每帧自动更新？CPU 触发采样？固定标定值？
- [ ] **Q18**: 颜色面积是否限定在 bbox 内部（当前为 ROI 内全区域）？如果限定在 bbox 内，需要 FPGA 做两遍扫描（第一遍找 bbox，第二遍做颜色计数），还是在同一遍内完成？
- [ ] **Q19**: VSYNC 中断是否可用（优化 CPU 轮询）？当前设计为独立轮询。
- [ ] **Q20**: `dropped_snapshot` 位是否需要 FPGA 提供独立的丢帧计数器寄存器，还是 status 位即可？

---

## 7. 进入 APB/SoC 集成前的阻塞门

以下检查项必须在任何人开始写 APB 寄存器 RTL 之前闭合（引用自 FPGA plan §6.1）：

1. **`generated_soc_summary_YYYY-MM-DD.md`** 已生成并审查通过。
2. 至少确认：
   - 当前 Efinity 生成 `soc.h` 中真实 APB user window 基地址
   - 选用的 `io_apbSlave_x_*` 当前确实空闲
   - `PADDR` 宽度 ≥ 9 bit（覆盖 `0x000-0x1FF`）
   - APB 时钟、复位、`PREADY` 和 `PSLVERROR` 语义
   - UART、CLINT、PLIC、AXI 和 DDR 地址无冲突
3. 在以上闭合前：
   - 不修改 SoC IP 设置
   - 不修改 `.peri.xml`
   - 不把草案中的 APB 地址当成真实地址
   - 不解除 CPU 侧 `REG_MAGIC` 运行期探测
4. `FG_AREA_AVAILABLE` 从 0 改为 1 前，必须实测 `LIVE_FG_AREA` 寄存器有效。
5. `TARGET_SEL_AVAILABLE` 从 0 改为 1 前，必须实测 `TARGET_SEL` 寄存器有效。
6. CDC 报告中的多位总线、握手和复位问题必须有证据闭环；任何"可忽略"判定必须经过 Codex Gate。

---

## 8. 建议的统一方案（不改代码，仅文档记录）

由于任务明确要求"不要直接大改代码"，以下仅为推荐方向，等待确认后再执行：

1. **废弃 `register_map.md`**：在文件顶部加 `> ⚠️ 本文件已过期 (v1 单路草案)。当前工作基线见 vision_register_handbook_draft_2026-07-03.md (v2 双路版)。`
2. **handbook v2 作为单一真相源**：将 §1.1 的 `LIVE_SUM_*`、`LIVE_ROI_PIXEL_COUNT`、`LIVE_FG_AREA`、`LIVE_STATUS` 正式写入 handbook v2（从"保留区"升级为"已定义"）。
3. **board_io.h 同步更新**：`feature_snapshot_t` 增加 `roi_pixel_count`、`sum_r/g/b/y`、`status` 字段；`board_io_read_features()` 增加对应的 `mmio_read32` 调用。
4. **ROI 寄存器命名统一**：`CFG_CAM0_ROI_TL/BR` → `CFG_CAM0_ROI_X0Y0/X1Y1`，注释写明半开区间。
5. **handbook v2 §2.4 保留区更新**：`0x08C-0x09F` 改为 "LIVE_SUM_R/G/B/Y + LIVE_ROI_PIXEL_COUNT"，原 WHITE/BLACK 面积降级注释。

---

## 9. 修改/新增文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `final_project/integration/cpu_fpga_preprocess_interface_freeze_20260711.md` | **新增** | 本稿 |
| （其他文件） | **不改** | 按任务要求，仅输出文档 |

---

## 10. 运行过的检查命令

```powershell
# 确认工作树状态
git status
git log --oneline -5

# 确认关键文件存在
Test-Path "final_project/cpu/CPU_MODULE_PLAN.txt"
Test-Path "final_project/fpga_vision_preprocess_execution_plan_20260711.md"
Test-Path "final_project/integration/register_map.md"
Test-Path "final_project/docs/architecture/vision_register_handbook_draft_2026-07-03.md"
Test-Path "final_project/cpu/app/include/board_io.h"
Test-Path "final_project/cpu/app/src/board_io.c"

# 确认偏移量一致性（board_io.h 内 Cam0 和 Cam1 偏移差）
# Cam1 = Cam0 + 0x100，所有 OFF_*1 = OFF_*0 + 0x100 ✅
```

所有文件均存在，工作树在 `dev/wsc6090-CPU` 分支，HEAD 为 `d478b51`。

---

## 附录 A：两份寄存器文档的地址空间对比

| 地址范围 | register_map.md (v1) | handbook v2 + board_io.h |
|----------|---------------------|--------------------------|
| `0x000` | REG_MAGIC | REG_MAGIC（含 Version 高 16-bit） |
| `0x004` | REG_VERSION | **SYS_CTRL** ⚠️ 冲突 |
| `0x008` | REG_FRAME_ID | SYS_STATUS0 (frame_id + valid) |
| `0x00C` | REG_FEATURE_VALID | SYS_ACK0 |
| `0x010` | REG_FEATURE_ACK | CPU_HEARTBEAT |
| `0x014-0x018` | — | CFG_CAM0_ROI_TL/BR |
| `0x020-0x024` | REG_ROI_XY (R) | CFG_CAM0_RED_TH_0/1 |
| `0x028-0x02C` | REG_ROI_WH (R) | CFG_CAM0_BLUE_TH_0/1 |
| `0x030-0x034` | REG_BBOX_X0Y0 | CFG_CAM0_YEL_TH_0/1 |
| `0x034-0x044` | REG_SUM_R/G/B/Y + REG_COUNT | CFG_CAM0_LUMA_TH + OSD |
| `0x054-0x060` | — | CPU_RESULT_*0 |
| `0x064-0x068` | — | CPU_ARM_STATE / CPU_ERROR_CODE |
| `0x06C` | — | TARGET_SEL（占位） |
| `0x080-0x088` | — | LIVE_RED/BLUE/YEL_AREA0 |
| `0x0A0-0x0A8` | — | LIVE_BBOX_MIN/MAX0 + LIVE_CENTER0 |
| `0x0B0` | — | LIVE_FG_AREA0（CPU 预留） |
| `0x100-0x1FF` | — | Camera 1 镜像块 |

---

*本稿等待 FPGA 队友审阅。确认后进入 FPGA plan 阶段 B（独立 RTL）。*
