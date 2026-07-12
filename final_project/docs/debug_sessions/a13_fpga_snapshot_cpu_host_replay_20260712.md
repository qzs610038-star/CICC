# A13 FPGA 快照到 CPU Host 回放记录

## 目的与边界

将 A11/A12 隔离工程已验证的合成五色 FPGA 快照，回放给现有 CPU 分类器与逐轮事务契约，验证“FPGA 基础特征 -> CPU 五色分类 -> 任务判断”这条软件语义链。

- 不接入 `main.c`、SoC、APB、OSD 或机械臂。
- 不修改 D 盘 FPGA 基线，也不把任何 APB 占位地址当作正式硬件地址。
- 形状仅验证合成正方体；尺寸标定按既定状态保持不可用。

## 代码变更

| 文件 | 变更 |
|---|---|
| `cpu/app/include/board_io.h` | `feature_snapshot_t` 增加 `roi_pixel_count`、`sum_r/g/b/y`，当前仅供 Host 回放；正式 APB 偏移未定义。 |
| `cpu/app/include/vision_classifier.h` | 追加 A12 平均亮度门限，与旧填充率回退门限分离。 |
| `cpu/app/src/vision_classifier.c` | 当快照包含 `sum_y` 和 ROI 像素数时，以 `sum_y / (3 * roi_pixel_count)` 分类白/黑；无统计字段时维持旧填充率回退。 |
| `cpu/tests/test_a13_fpga_snapshot_replay.c` | 新增五色实测快照回放和完整 20 轮 Host 测试。 |
| `cpu/tests/run_a13_fpga_snapshot_replay.ps1` | 新增可复现测试命令。 |

白色与黑色的 A12 平均通道亮度分别约为 `140.37` 和 `115.36`，当前合成回放的门限为白 `>=130`、黑 `<=125`，中间保留 UNKNOWN 区间。该门限仅适用于当前合成源和灰背景，真实摄像头必须重新标定。

## 回放数据

| 颜色 | 已验证输入 |
|---|---|
| 红 | `red_area=102400` |
| 蓝 | `blue_area=102400` |
| 黄 | `yellow_area=102400` |
| 白 | `sum_y=437145600` |
| 黑 | `sum_y=358809600` |

所有样本的共同 A11/A12 证据为 `fg_area=102400`、`roi_pixel_count=1036800`、bbox `{380,320}..{699,639}`、中心 `{539,479}`、状态 `0`。

## 验证

```powershell
powershell -ExecutionPolicy Bypass -File final_project\cpu\tests\run_a13_fpga_snapshot_replay.ps1
```

结果：`a13_fpga_snapshot_replay: 169/169 passed (20 rounds, size deferred)`。

覆盖内容：

- 五种快照均被 CPU 分类为正确颜色，且由已验证 `fg_area` 识别为正方体。
- 任务一白色目标与任务一黑色目标各 5 轮：五色回放产生唯一的 `EXECUTE` 或 `SKIP + COLOR_MISMATCH`。
- 任务三、四各 5 轮：按尺寸未标定门禁输出 `WAIT + SIZE_UNAVAILABLE`，随后用 `ABANDON` 结束，不伪造尺寸判断。

回归结果：分类器 `31/31`、契约 `35/35`、轮次 `135/135`、旧 Host 流程 `164/164` 均通过。

## 限制与下一步

此测试以 `-DAPB_VISION_BASE_PLACEHOLDER` 编译，只为满足头文件的测试门禁；Host 适配层不访问 MMIO。A13 也以 `-DFG_AREA_AVAILABLE=1` 表达 A11 已经实测的前景面积字段，不代表正式 APB 寄存器已存在。

下一步应恢复 SoC/视频资源审查，先通过 Efinity Interface Designer 明确 PLL 重规划可行性和正式快照/APB 地址契约；在此之前，不得接入 `main.c` 或宣称板上 CPU 闭环完成。
