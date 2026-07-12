# 今晚工作协调摘要（2026-07-12）

## 一句话状态

摄像头尚无稳定数据流，但已用隔离合成源完成“FPGA 五色特征快照 -> CPU 五色分类 -> Host 逐轮判断”的可重复验证。正式板上 CPU/APB/OSD/机械臂闭环仍被 SoC 与视频 PLL 资源冲突阻塞。

## 今晚完成

| 编号 | 完成项 | 已验证事实 | 证据 |
|---|---|---|---|
| A11 | 合成五色到 FPGA 预处理 | 红、蓝、黄各自颜色面积 `102400`；五色均有前景面积 `102400`、ROI `1036800`、正确 bbox/中心。 | `a11_synthetic_preprocess_isolated_map_20260712.md` |
| A12 | 白黑统计可观测性 | 黑：RGB 各 `119603200`、`sum_y=358809600`；白：RGB 各 `145715200`、`sum_y=437145600`。白黑已可由 FPGA 基础统计独立区分。 | `a12_white_black_snapshot_probe_execution_20260712.md` |
| A13 | CPU Host 五色回放 | CPU 用红/蓝/黄面积分类，用 A12 `sum_y/(3*ROI)` 分类白黑；五色实测快照驱动完整 20 轮 Host 事务，`169/169` 通过。 | `a13_fpga_snapshot_cpu_host_replay_20260712.md` |
| A14 | SoC 集成风险复核 | 当前视频工程占用 `PLL_BL0/BL1/BL2/TR0`；硬 SoC 系统 PLL 只能使用已占用的 `BL0/BL1/BL2`，不能直接合并。 | `a14_soc_pll_replanning_decision_gate_20260712.md` |

补充回归：分类器 `31/31`、契约 `35/35`、逐轮状态机 `135/135`、原 Host 流程 `164/164` 均通过。

## 当前架构分工

```text
FPGA：视频/ROI/颜色面积/RGB亮度累计/前景面积/bbox/中心/OSD像素渲染/硬件通道
CPU：五色、形状、尺寸分类；四任务判断；逐轮状态机；结果语义；机械臂控制
```

白色和黑色**不是**由 FPGA 做最终分类。A12 只证明 FPGA 能提供使 CPU 区分白黑的统计特征；五色最终分类统一在 CPU。

## 重要限制

- A11/A12 在 `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga`，为 JTAG SRAM 临时下载工程，不是 D 盘正式构建树。
- HDMI 显示经过 CDC/FIFO，与源时钟预处理快照可能有颜色相位差；白黑身份以 VCD 中 `sum_r/g/b/y` 为准，不能只看瞬时画面。
- A13 是 Host 测试，不访问 MMIO；其中 APB 占位宏和 `FG_AREA_AVAILABLE=1` 都不是正式硬件接口。
- 尺寸尚未标定，因此任务三、四目前必须 `WAIT + SIZE_UNAVAILABLE` 后人工放弃，不能伪造完成。
- 未完成：真实摄像头、正式 SoC/APB/CDC、`main.c`、OSD、板级 20 轮、尺寸、机械臂。

## 当前阻塞与请求

| 阻塞 | 事实 | 需要谁确认 |
|---|---|---|
| SoC/视频合并 | SoC 系统 PLL 与视频关键 PLL 冲突；JTAG 改 `USER2` 不能单独解决。 | FPGA/SoC：在 A8 隔离工程的 Interface Designer 查看 `pll_inst1 (PLL_BL1)` 是否有 GUI 合法迁移候选。 |
| 正式特征寄存器 | A12 已有数据字段，但没有正式 APB 地址、快照 CDC、ACK 或 `soc.h`。 | FPGA/SoC：SoC 资源门关闭后设计 snapshot/ACK/CDC 与 APB 窗口。 |
| 尺寸任务 | 没有像素到 cm 标定。 | 视觉/机械结构：固定相机、物体平面、光照后执行标定。 |
| 机械臂闭环 | 板到臂 UART、电平、接线和安全验证未完成。 | 机械臂成员：先做点位与安全链，不接入 FPGA 控制线。 |
| 摄像头 | 仍无稳定 CSI 数据流。 | 硬件/视觉成员：新相机到位后按单摄 I2C、MCLK、复位、CSI DE 顺序排查。 |

## 建议并行分工

### A（FPGA/SoC）

1. 在隔离 A8 工程做 Interface Designer 的 `PLL_BL1` 合法迁移候选确认，截图并记录“有候选/无候选”。
2. 若无候选，停止 SoC 直合并，转为准备正式 snapshot/APB/CDC 接口草案，等待架构替代方案。
3. 若有候选，只生成新的隔离工程，复核 DDR/MIPI/HDMI 时钟依赖，禁止直接改 D 盘。

### B（CPU/任务逻辑）

1. 基于 A13 固化五色 CPU 参数格式和现场标定表，但不要把合成门限当真实相机门限。
2. 保持任务三、四尺寸状态为 `UNAVAILABLE`；准备标定输入后再启用。
3. 继续完善结果文本/理由码到 OSD 的语义映射文档，不接 `main.c`。

### C（机械臂/场地）

1. 固定相机、补光、底板、起点与 180 度最大臂展放置区。
2. 做机械臂独立安全点位和夹爪验证，记录速度、急停与掉落风险。
3. 不把 PC Python 或 myBlockly 放入正式闭环；不接开发板 UART2，直到电平/协议/SoC 明确。

## 明日最小检查点

1. A 给出 Interface Designer 对 `PLL_BL1` 的候选结果。
2. B 保存真实相机五色采样/参数表模板。
3. C 给出固定场地和安全抓放点位的可复现记录。
4. 新摄像头到位后，优先恢复单摄真实 CSI，再以同一 A12 特征字段重新标定，不能直接沿用合成阈值。

## 入口文档

- 当前进度：`CURRENT_STATE.md`
- A11/A12 FPGA 证据：`docs/debug_sessions/a11_synthetic_preprocess_isolated_map_20260712.md`、`docs/debug_sessions/a12_white_black_snapshot_probe_execution_20260712.md`
- A13 CPU 回放：`docs/debug_sessions/a13_fpga_snapshot_cpu_host_replay_20260712.md`
- A14 SoC 风险门：`docs/review_packets/a14_soc_pll_replanning_decision_gate_20260712.md`
