# 单摄 FPGA 到 CPU 特征快照契约

> 版本：v0.1-draft
>
> 日期：2026-07-15
>
> 状态：`HOST CONTRACT VERIFIED / FEATURE RTL PRESENT BUT DISCONNECTED / BOARD NOT VERIFIED`

## 1. 目的与边界

此文档是正式单摄视频/识别工程唯一的 FPGA 到板上 CPU 基础特征契约。它定义 CPU
分类器所需的统计语义，不定义 APB 地址、`soc.h` 宏、IRQ、UART 命令、OSD
寄存器格式或机械臂接口。

- FPGA 只产生 ROI、颜色面积、前景面积、亮度和 bbox 等基础统计，不对物体
  做颜色、形状或尺寸分类，也不判断四项比赛任务。
- CPU 使用此快照产生 `sc_observation_t`、任务判断和可解释结果语义。
- F1 固定 `ARM_ENABLED=0`。任何目标命中仍仅为
  `EXECUTE_ARM_DISABLED`，不得通过本契约触发机械臂。
- 尺寸字段本版本固定为不可用；任务三、四保持 `SIZE_UNAVAILABLE`。

## 2. 像素 Tap

统计模块必须是只读旁路，不得参与、回压、替换或延迟原有画面主链：

```text
ch0 framebuffer -> debayer_top_2to1 -> rgb0_data_rgb -> HDMI
                                      \-> feature tap -> snapshot/CDC (future)
```

当前候选工程的建议 tap 位于 `src/top.v` 的 ch0 Debayer 后：

| 项目 | 实际信号/语义 |
|---|---|
| 时钟域 | `i_sysclk_div2` |
| 复位 | `pixel_data_en` 低有效的现有像素链复位语义 |
| 帧/行/有效 | `rgb_vs`、`rgb_hs`、`rgb_de` |
| 像素总线 | `rgb0_data_rgb[47:0]` |
| 像素 0 | `R=rgb0_data_rgb[47:40]`，`G=[39:32]`，`B=[31:24]` |
| 像素 1 | `R=rgb0_data_rgb[23:16]`，`G=[15:8]`，`B=[7:0]` |
| 统计有效 | 每个 `rgb_de=1` 时钟处理两个像素 |

统计源是 Debayer 后、HDMI 显示专用固定白平衡之前的 `rgb0_data_rgb`。
这避免显示校正系数改变 CPU 原始特征语义；显示支路继续可独立调整而不破坏
分类器输入契约。

`raw_diag_en=1`、`ch0_frame_stable=0`、特征溢出或 ROI 非法时，禁止发布可供
分类的快照。

## 3. CPU 配置语义

后续合法 SoC/APB 接入时，CPU 必须以 staging/commit/active 的整组方式下发
以下配置，并仅以 active 版本统计一个完整帧。地址、寄存器布局和 PSTRB 在
同次 Efinity GUI 生成 SoC 后另行冻结。

| 配置组 | 必需语义 |
|---|---|
| `roi` | 闭区间左上/右下坐标；必须满足 `0 <= x0 <= x1 < 1920`、`0 <= y0 <= y1 < 1080` |
| `background` | 标定的背景 RGB 均值与前景差异门限；前景判定为当前像素与背景的绝对 RGB 差之和达到门限 |
| `color_masks` | 红、蓝、黄三类的 CPU 管理阈值；FPGA 只按 active mask 计数，不输出颜色类别 |
| `config_seq` | 16 位单调版本；快照必须回传实际使用的 active 版本 |

白和黑不需要 FPGA 专用颜色类别。CPU 根据 `sum_luma / roi_pixel_count` 以及
前景面积做白/黑/未知判定。

## 4. 帧快照载荷

下表字段对应 `cpu/include/single_camera_classifier.h` 中的 `sc_features_t`。
字段宽度为最小无溢出要求；实现可用更宽累加器，但发布值不得截断。

| 字段 | 最小位宽 | 语义 |
|---|---:|---|
| `frame_id` | 16 | 每个已发布的有效 ch0 帧递增，复位后从 0 开始 |
| `config_seq` | 16 | 统计该帧使用的 active 配置版本 |
| `red_area` | 21 | ROI 内命中红色 mask 的像素数 |
| `blue_area` | 21 | ROI 内命中蓝色 mask 的像素数 |
| `yellow_area` | 21 | ROI 内命中黄色 mask 的像素数 |
| `foreground_area` | 21 | ROI 内满足前景判定的像素数 |
| `roi_pixel_count` | 21 | ROI 内实际处理像素数；不得由配置面积猜测 |
| `sum_luma` | 31 | ROI 内逐像素 `R+G+B` 之和，范围为 `0..255*3*roi_pixel_count` |
| `bbox_width` | 11 | 前景 bbox 宽；无前景时为 0 |
| `bbox_height` | 11 | 前景 bbox 高；无前景时为 0 |
| `source_flags` | 8 | 见下表 |

`source_flags`：

| 位 | 名称 | 为 1 的含义 |
|---:|---|---|
| 0 | `FRAME_STABLE` | 输入帧长度检测稳定 |
| 1 | `ROI_VALID` | active ROI 合法且该帧实际计到像素 |
| 2 | `STATS_VALID` | 此快照可被 CPU 消费 |
| 3 | `DIAG_ACTIVE` | SW4 中灰诊断生效；此状态下 `STATS_VALID` 必须为 0 |
| 4 | `COUNTER_OVERFLOW` | 任一发布字段溢出；此状态下 `STATS_VALID` 必须为 0 |
| 5 | `SNAPSHOT_OVERRUN` | 上一个快照未 ACK 时已完成新帧；当前快照不可作为新的稳定帧 |
| 6 | `SOURCE_CH0` | 固定为 1，防止错误接入 ch1 |
| 7 | 保留 | 必须为 0 |

`bbox_width` 与 `bbox_height` 是前景包围盒的包含端点尺寸：
`x_max - x_min + 1`、`y_max - y_min + 1`。这与 CPU 的填充率公式一致，避免
一像素边界的除零或偏差。

## 5. 帧原子性与 ACK

1. FPGA 在像素域内完整累积一帧，遇到本帧结束边界后一次性锁存全部字段。
2. `STATS_VALID=1` 时，从 `frame_id` 到全部统计字段必须保持不变，直到 CPU
   使用相同 `frame_id` 进行 ACK。
3. CPU 只能读取 `STATS_VALID=1` 的快照；读取后再次检查 `frame_id`，两次不同
   则丢弃整帧并重读，不能拼接字段。
4. ACK 的 `frame_id` 不匹配时，FPGA 必须拒绝 ACK 并保留原快照。
5. 快照未 ACK 时，像素统计器仍可继续运行，但不得阻塞 HDMI/DDR。若完成新的
   候选帧，置位 `SNAPSHOT_OVERRUN`，并在下一次合法发布前清除；CPU 必须把该
   帧视为不稳定。
6. 像素域到 CPU/总线域必须使用经审查的 multi-bit snapshot CDC 或异步 FIFO。
   不得逐字段直接打两拍后自行拼接。
7. Host runtime 的集成裁决允许在本轮结果已经锁存后，对成功读取的同一
   `frame_id` 做 release-only ACK：只释放单槽并更新帧序，不再分类或提交结果。
   这只是待审语义，不是已冻结 wire ABI；真实 APB/CDC 必须另行定义如何区分
   业务消费 ACK 与终态释放，并覆盖撕裂、错 ACK、overrun 和跨轮行为。

## 6. F1 Host 映射

CPU 将有效快照逐字段映射为：

```text
sc_features_t -> sc_classify_features() -> sc_observation_t
                -> sc_f1_observe() -> decision/reason
```

只有下列条件同时成立时，CPU 才可把 `sc_observation_t.stable=1` 送入 F1：

- `STATS_VALID=1`；
- `FRAME_STABLE=1`；
- `DIAG_ACTIVE=0`；
- `COUNTER_OVERFLOW=0`；
- `SNAPSHOT_OVERRUN=0`；
- `SOURCE_CH0=1`；
- 分类器输出颜色和形状均非 unknown。

否则 CPU 保持采集状态或等待超时，绝不把不完整特征误写为 `SKIP` 或
`EXECUTE_ARM_DISABLED`。

## 7. 当前实现状态

| 项目 | 状态 |
|---|---|
| ch0 Debayer 后 RGB tap 位置 | 已由真实 `src/top.v` 审计 |
| 画面旁路不回压原则 | 已冻结 |
| `sc_features_t` 和 Host 分类器 | 已实现并 Host 测试 |
| FPGA 统计 RTL | 源码与 testbench 已存在；顶层 `i_capture_enable=1'b0`，ACK 固定关闭，输出 unused |
| Hard SoC / `soc.h` / APB0 MAGIC | 源码与同批生成物已存在；仅离线冷构建有证据，板级仍未验证 |
| 业务 APB/CDC/ACK/result/OSD | 未实现；当前 feature 不可由 CPU 读取 |
| 正式业务地址/IRQ | 未定义，禁止手填；`IO_APB_SLAVE_0_INPUT` 只作为同批 APB0 MAGIC 候选基址，尚未板级实读 |
| 板级特征、五色准确率、OSD | 未验证 |

## 8. 后续实施门禁

开始 feature/APB/CDC/OSD 业务接入前必须同时满足：

1. 新 UART1 原子批次完成后，在一次批准窗口内连续通过 USER2、Type-C UART1 Hello/回显与既有 APB MAGIC 实读；
2. 用户批准包含 feature、目标/事件、result/OSD 的最小 F1 原子批次 Review Packet；
3. 保留并扩展独立 RTL testbench，覆盖双像素展开、ROI 边界、帧锁存、溢出、
   ACK 不匹配、CDC 和 APB 访问方向；
4. RTL 只做受审旁路统计及业务通道连接，不修改 framebuffer、Debayer、HDMI
   数据路径；若 `constrain.sdc`、`mem_test.xml`、`.peri.xml`、IP 或顶层发生变化，
   必须作为同一原子批次重开全部构建和板级证据；
5. 初次综合/PNR 必须回传资源、Setup、Hold、CDC 和 HDMI 回归证据；
6. 业务地址与 `soc.h` 未同批冻结前，feature RTL 不得伪装为 CPU 可读 APB 外设。
