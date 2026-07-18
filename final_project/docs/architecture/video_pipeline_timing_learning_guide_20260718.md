# 视频流水线与时序学习讲义

> 适用范围：`CICC/final_project` 的 FPGA 视频前端学习。
> 源码观察基线：`main@9acf4d8`（2026-07-18）。
> 文档性质：教学与源码导读，不构成 Efinity 构建、bitstream、板级采集、APB 或 OSD 闭环已经通过的证据。当前工程状态以仓库根目录的 `CURRENT_STATE.md` 为准。

## 1. 学完后应能回答的问题

读完本讲义后，应能不用“屏幕有图”这一条现象，独立回答下面的问题：

1. 一拍视频数据中到底有几个像素，每个像素的 R/G/B 位于哪几个 bit？
2. `VS`、`HS`、`DE`、`valid`、`ready` 各自约束什么；它们为什么不能混为一谈？
3. RAW10、RAW8、Bayer、RGB888、2ppc、1ppc 在当前工程中如何转换？
4. 为什么 Debayer、白平衡、Gamma、OSD 都必须让数据和时序标志经历相同的延迟？
5. ROI 的边界为何使用半开区间，奇数边界在 2ppc 中如何处理？
6. 统计快照为什么在“下一帧开始”才发布，以及怎样保证 CPU 不会读到撕裂的数据？
7. 为什么显示画面、统计快照和 OSD 结果必须各自携带帧身份和配置身份？

## 2. 先建立正确的心智模型

### 2.1 一拍不一定等于一个像素

FPGA 视频逻辑以时钟拍处理数据。每拍可能带 1、2、4 个像素，称为 `pixels per clock`（ppc）。因此不要用“数据宽度 / 8”直接猜测像素数。

```text
一个可追溯的视频样本应被理解为：

{时钟域, 帧边界, 行边界, 有效标志, 每拍像素数,
 数据格式与字节序, 像素坐标, 配置版本, 源帧身份}
```

例如，当前预处理输入的 `48-bit` 数据不是一个 48-bit 像素，而是两颗 RGB888 像素；CSI 侧的 `40-bit` 数据不是一个 40-bit 像素，而是四颗 RAW10 像素的打包结果。

### 2.2 视频正确性有四层

| 层次 | 要证明的事实 | 仅凭 HDMI 画面能否证明 |
|---|---|---|
| 电气/链路 | 摄像头、MIPI、DDR、TMDS 有活动 | 部分，不能定位具体断点 |
| 像素格式 | 位宽、Bayer 相位、RGB 字节序正确 | 不可靠，颜色偏差可能不明显 |
| 时序 | `VS/HS/DE/valid` 与数据同拍、无丢拍/下溢 | 不可靠 |
| 帧语义 | 统计、ROI、CPU 结果、OSD 是否来自同一源帧 | 不能 |

本项目真正需要的是第四层：每一轮识别所依据的数据，必须知道来自哪一路摄像头、哪一帧、哪一套 ROI/阈值配置。显示画面只是其中一个观测面。

## 3. 当前工程与目标架构

`integration/video_pipeline.md` 描述的目标链路为：

```text
video_in -> raw_unpack -> debayer -> wb_gamma -> roi_crop
         -> feature_extract -> osd -> dvi_tx
```

当前 `top.v` 中已接入的实际主干与目标并不完全相同：

```text
CSI RX
  RAW10, 40-bit, 4ppc, VS/HS/DE
    -> 保留每颗像素高 8 bit，得到 RAW8, 32-bit, 4ppc
    -> DDR framebuffer / AXI
    -> 16-bit Bayer 样本对, 2ppc
    -> Debayer
    -> RGB888 x2, 48-bit, {B1,G1,R1,B0,G0,R0}
       |- ch1 预处理旁路：ROI、掩码、统计、快照（i_sysclk_div2 域）
       `- HDMI 支路：2ppc -> 1ppc CDC FIFO -> hdmi_top -> DVI encoder -> TMDS
```

当前事实要特别区分：

- 预处理只实例化在 `ch1` 的 Debayer 后 RGB 流上；它不改变 HDMI 主数据流。
- `white_balance` 模块已实例化，但 `HDMI_BYPASS_WHITE_BALANCE=1'b1`，当前 HDMI 旁路它。
- 当前 RTL 中没有独立的 Gamma 模块或 OSD 渲染模块，也没有正式 APB/CDC 快照接口；这些属于目标架构或接口草案，不是现有闭环。
- HDMI 可选 ch0/ch1，而预处理固定取 ch1。因此若屏幕正在显示 ch0，画面和统计天然不是同一摄像头。

## 4. RAW 原理：从光子到 Bayer 数字码

### 4.1 RAW 不是“黑白图片”，而是传感器的原始测量结果

`RAW` 的核心含义是：图像传感器刚把光转换为数字量后、尚未被还原为完整彩色画面的一组原始采样值。

它通常具有下列特征：

- 每个感光位置只测得一个数，不是完整的 R、G、B 三个数；
- 数值尽量保持与入射光量的近似线性关系，便于后续测量、白平衡和颜色恢复；
- 尚未经过 Debayer、Gamma、锐化、压缩、显示颜色空间转换等“把数据做成好看画面”的处理；
- 它不是“没有做过任何处理”。真实传感器或 CSI 接收器可能已经完成黑电平校正、坏点校正、裁剪、位宽打包或格式重排。是否发生过这些处理，必须以传感器配置和接收器接口契约为准。

把一个像素位置的形成过程简化为：

```text
场景光线
  -> 镜头聚焦
  -> 微透镜（常见）
  -> 彩色滤光片 CFA
  -> 光电二极管积累电荷
  -> 读出电路 / 放大
  -> ADC 量化
  -> RAW 数字码
```

感光单元本身主要只能感知“有多少光子到达”，并不天然知道光是红、绿还是蓝。颜色信息来自它上方的彩色滤光片。一个红色滤光位置主要放行红光，一个绿色滤光位置主要放行绿光，一个蓝色滤光位置主要放行蓝光。

用教学用的简化模型表示，某一感光位置的输出近似为：

```text
raw_code = clip(black_level + gain * photo_signal + noise, 0, full_scale)
```

其中：

- `photo_signal`：曝光时间内积累的有效光生电荷，通常随入射光量增加；
- `black_level`：即使遮光也可能存在的偏置码值，不应默认是 0；
- `gain`：模拟增益或数字增益；
- `noise`：光子散粒噪声、读出噪声、暗电流等；
- `clip`：ADC 或后级的最大码值限制。达到上限即饱和，亮部细节不可恢复。

这个公式用于理解，不代表当前摄像头的真实黑电平、白电平或增益实现。那些参数必须从传感器数据手册、寄存器配置和实际采样中确认。

### 4.2 为什么一个 RAW 像素没有完整颜色：CFA 与 Bayer 阵列

图像传感器前方通常覆盖 `CFA`（Color Filter Array，彩色滤光阵列）。最常见的 CFA 是 Bayer 阵列，例如一种 `RGGB` 排列：

```text
坐标奇偶决定滤光颜色，而不是数据总线本身决定颜色。

          x=0  x=1  x=2  x=3
y=0       R    G    R    G
y=1       G    B    G    B
y=2       R    G    R    G
y=3       G    B    G    B
```

这里每个格子都只保存一个 RAW 值：

- R 位置保存该位置透过红滤光片后的光强；
- G 位置保存透过绿滤光片后的光强；
- B 位置保存透过蓝滤光片后的光强。

绿色通常占一半，红色和蓝色各占四分之一。一个实用原因是亮度细节主要依赖绿色附近的采样，绿色采样较多有利于重建细节；但这不表示绿色像素“更亮”，而是 CFA 的空间采样密度更高。

例如同一小块场景的真实光谱可能让红、绿、蓝通道分别偏高、中、低；Bayer 阵列不会在一个位置同时保存三个值，而是把这三类采样分布在相邻位置：

```text
R-site:  800       G-site: 510
G-site:  500       B-site: 190
```

这些数值只表示各自位置、各自滤光颜色下的采样，不能直接拼成某一颗像素的 `{R=800,G=510,B=190}`。要得到完整 RGB，必须参考相邻位置并进行 Debayer。

常见 Bayer 排列并不只有 RGGB，还包括 `BGGR`、`GRBG`、`GBRG`。如果实际排列与 Debayer 算法假设不同，即使时钟、位宽和画面轮廓都正确，也可能出现红蓝互换、整体偏色或棋盘状伪色。

**最重要的结论：** RAW 数据中的“颜色身份”来自 `(x, y)` 坐标加 CFA 模式。数据值 `500` 本身不带“我是红色”或“我是绿色”的标签。

### 4.3 RAW10：每颗像素为什么是 10 bit

`RAW10` 表示每个 Bayer 采样使用 10 bit 保存，可表示：

```text
0 .. 1023，共 1024 个量化级别
```

相较于 8 bit 的 `0..255`，10 bit 的量化台阶更细。它的价值不只是“数字更大”，而是亮暗过渡、暗部细节和后续增益处理有更多可表达的码值空间。

但应避免两个误解：

1. 10 bit 不等于实际有效精度一定有 10 bit。若噪声很大，若干相邻码值可能在物理上不可区分。
2. 10 bit 不等于动态范围自动更大。动态范围还受满阱容量、读出噪声、曝光、模拟增益和 ADC 性能影响。

RAW 值通常更接近场景线性光。去除黑电平并按白电平归一化后，可用下式理解：

```text
linear_signal = clamp((raw_code - black_level) / (white_level - black_level), 0, 1)
```

`black_level` 和 `white_level` 是传感器/模式相关参数，不能把 `0` 和 `1023` 自动当作它们。尤其是遮住镜头时，RAW 仍可能稳定在一个非零黑电平附近；这往往是正常的传感器偏置，不应直接认定为“画面漏光”。

### 4.4 曝光、增益、白平衡和 Gamma 分别改变什么

这四者经常被混淆，但它们发生在不同层次。

| 操作 | 通常作用位置 | 它真正改变的量 | 主要风险 |
|---|---|---|---|
| 曝光时间 | 传感器积分阶段 | 收集到的光子和电荷数量 | 过曝饱和、运动模糊 |
| 模拟增益 | ADC 前或附近 | 放大电信号 | 放大噪声，不能创造丢失细节 |
| 数字增益 | ADC 后 | 对数字码值做乘法 | 裁剪高光、放大量化与噪声 |
| 白平衡 | 通常 Debayer 前后按通道处理 | R/G/B 通道相对比例 | 阈值、颜色统计与标定变化 |
| Gamma | 面向显示的非线性映射 | 视觉亮度分布 | 不再保持近似线性测量意义 |

曝光增加会让更多光子进入感光单元，通常能改善信噪比，直到亮部饱和。模拟增益和数字增益主要把已有信号放大，不能恢复已经被噪声淹没或被饱和裁剪的细节。

Gamma 的目标是让画面更适合显示器和人眼观看，不是让像素值继续代表成比例的光量。因此如果一个颜色阈值、面积统计或背景模型是基于线性 RAW/RGB 标定的，不能不经验证地搬到 Gamma 后的数据域。

### 4.5 “MIPI 线上字节”与“FPGA 内部并行字”不是一回事

RAW10 的逻辑含义是四颗 10-bit 像素可合成 40 bit：

```text
P0[9:0], P1[9:0], P2[9:0], P3[9:0]
```

但 MIPI CSI-2 在线上传输的是字节流。常见 RAW10 的五字节打包可抽象为：

```text
byte0 = P0[9:2]
byte1 = P1[9:2]
byte2 = P2[9:2]
byte3 = P3[9:2]
byte4 = {P3[1:0], P2[1:0], P1[1:0], P0[1:0]}
```

接收器 IP 通常会把这些串行字节重新组织为内部并行总线。于是必须区分两件事：

```text
传感器 MIPI 线上的五字节顺序
            !=
CSI 接收器输出到 FPGA 逻辑的 40-bit 位号顺序
```

当前工程的 `soft_mipi_rx_top` 向顶层输出 `rx_out_data[39:0]`。顶层函数 `raw10_4pix_to_raw8_4pix` 按以下内部契约取数：

```text
假定内部并行字已经表示为：
  P3[9:0] | P2[9:0] | P1[9:0] | P0[9:0]

取每颗像素最高 8 bit：
  P3[9:2] = raw10_4pix[39:32]
  P2[9:2] = raw10_4pix[29:22]
  P1[9:2] = raw10_4pix[19:12]
  P0[9:2] = raw10_4pix[ 9: 2]
```

这段 RTL 说明的是**当前 CSI 接收器输出到本工程的并行接口假设**。它不能反过来证明 MIPI 线上字节恰好也是这个 bit 编号，更不能替代传感器手册、CSI IP 文档和真实波形验证。

如果把 MIPI 原始字节顺序直接当成内部 40-bit 像素顺序，或把 P0/P1 的空间先后弄反，可能得到三类问题：

- 颜色通道错误或 Bayer 相位错误；
- 图像横向出现周期性 4 像素纹理；
- 画面看似有内容，但 Debayer 后颜色、ROI 掩码和统计均不可信。

### 4.6 当前工程的 RAW10 -> RAW8：发生了什么信息损失

本工程当前在进入 framebuffer 前执行：

```text
raw8 = raw10[9:2]
```

也就是丢弃每颗 RAW10 像素最低 2 bit。数值上等价于对非负数做除以 4 的向下取整：

```text
RAW10: 0..3       -> RAW8: 0
RAW10: 4..7       -> RAW8: 1
...
RAW10: 1020..1023 -> RAW8: 255
```

这一步的结果是：

- 数据带宽从 40 bit/4ppc 降为 32 bit/4ppc；
- 后续 DDR、FIFO 和 Debayer 处理的是 8-bit Bayer 数据；
- 被丢弃的低 2 bit 无法在后级恢复；
- 它**不是** Debayer，也没有让一颗 RAW 像素突然拥有 RGB 三通道；
- 暗部、接近阈值的像素和细微的颜色差异会拥有更粗的量化步长，需要结合实测判断是否满足识别需求。

选择 RAW10 保留、截断、压缩还是更高精度的处理，是图像质量、DDR 带宽、存储容量、LUT/RAM 资源和时序压力之间的工程取舍。当前源码实施了截断，但本讲义不把它表述为已经完成画质评估的最优选择。

### 4.7 RAW 与 RGB 的本质差异

| 维度 | RAW Bayer 像素 | RGB888 像素 |
|---|---|---|
| 每个空间位置保存什么 | 一个 CFA 颜色的单通道采样 | R、G、B 三个完整通道 |
| 当前常见位宽 | RAW10 为 10 bit/像素，RAW8 为 8 bit/像素 | 24 bit/像素，即 8+8+8 |
| 颜色身份来源 | 坐标奇偶 + Bayer 模式 | 数据字段本身明确区分 R/G/B |
| 是否可直接正确显示 | 不宜；会呈现马赛克/偏色 | 可以送入显示链路 |
| 是否近似线性光测量 | 通常更接近 | 取决于是否已白平衡/Gamma/颜色变换 |
| 当前工程中的主要用途 | CSI 接收、framebuffer、Debayer 输入 | ROI、颜色掩码、统计、HDMI 输入 |

因此，下面两句话都不正确：

```text
“RAW10 是 10-bit RGB。”
“RAW8 就是灰度图。”
```

更准确的表述是：RAW10/RAW8 是每个 CFA 位置的单通道原始采样；它既不等于完整 RGB，也不等于没有颜色信息的普通灰度图。颜色信息分散在 Bayer 空间阵列中。

### 4.8 Debayer 怎样从 RAW 恢复 RGB

Debayer 的输入是一幅 Bayer 马赛克，输出是每个位置都有 R、G、B 的图像。最简单的插值思路是：

```text
若当前位置是 R-site：
  R = 本位置的 RAW 值
  G = 上下左右绿色邻居的估计
  B = 对角蓝色邻居的估计
```

对于 G-site 或 B-site，推算关系会不同。真实算法还会考虑边缘方向，避免在强边缘上跨物体插值；本工程的 Debayer 通过行缓存保存相邻行，以便形成必要的二维邻域。

这也解释了 Debayer 的两个重要工程特征：

1. **它有延迟。** 要等到邻居像素和相邻行到齐，才能输出当前 RGB。因此 RGB 的 `VS/HS/DE/valid` 必须是与 RGB 数据对齐后的输出时序，不能直接复用 RAW 输入时序。
2. **它依赖坐标相位。** RAW 流整体横移一颗像素或丢失一行，就会把 R-site 当成 G-site/B-site。即使所有数值仍在变化，颜色也会错误。

当前工程在 framebuffer 输出到 Debayer 前有 `CH0_BAYER_SWAP_PIXELS` 和 `CH1_BAYER_SWAP_PIXELS` 的像素对调逻辑。这反映了“相邻 RAW 像素的空间先后顺序”是接口契约的一部分。它不能仅靠变量名判断正确，必须用已知颜色目标、传感器 Bayer 模式和真实波形联合验证。

### 4.9 RAW 与视频时序为何必须一起理解

在 RGB 图像中，像素横移一格往往只是空间错位；在 Bayer RAW 中，横移一格还会改变 CFA 颜色身份：

```text
正确坐标：R G R G
整体右移 1：G R G R

同一个 raw_code 被 Debayer 解释为不同颜色采样，结果不只是画面移动，而可能是偏色。
```

因此 RAW 流调试必须同时验证：

- `VS` 是否在真实帧边界出现；
- `HS`、`DE` 是否使每行像素数和起点稳定；
- `pixel_per_clk` 与实际解包器一致；
- 每拍内 P0/P1/P2/P3 的空间顺序正确；
- 行首和帧首是否保持 Bayer 相位；
- 裁剪、缩放或 ROI 若发生在 RAW 域，是否保持 x/y 奇偶关系。

本工程的 ROI/颜色统计位于 Debayer 后 RGB 域，避免了在 RAW 域直接按颜色阈值的复杂性。但它仍继承 Debayer 正确性的前提：只要 RAW 格式、Bayer 相位或时序有错，后面的 RGB、颜色面积和 bbox 都会被污染。

### 4.10 RAW 调试的最小验证闭环

不要从自然场景“看着差不多”开始判断 RAW 是否正确。更可靠的顺序是：

1. **格式：** 确认 CSI 输出的 datatype 为 RAW10、`pixel_per_clk=4`，并确认有效数据只在预期 `DE` 区间使用。
2. **黑场：** 遮光或使用均匀暗场时，观察 RAW 是否稳定在可解释的黑电平附近；不要先假定必须为零。
3. **均匀亮场：** 用均匀白/灰目标检查同色位置的 RAW 是否存在异常条纹、明显列噪声或饱和。
4. **纯色目标：** 使用红、绿、蓝目标验证 Bayer 相位和 RGB 字节序。红色目标被 Debayer 后若主要落在蓝通道，优先检查 CFA 模式、像素顺序和通道重排，而不是先调分类阈值。
5. **空间图案：** 使用棋盘格、竖线或横线确认每拍多像素解包没有周期性位移；4ppc 解包错误常表现为每 4 像素重复的异常。
6. **帧持续性：** 连续多帧检查 `VS/HS/DE`、framebuffer 下溢、Debayer valid 和 RGB 数据变化，防止偶发丢行或跨帧拼接被单帧画面掩盖。

这些检查属于数据链路验证；它们不等于已经完成板级 PASS。实际硬件运行、烧录、接口电平和当前 Gate 仍必须遵守 `CURRENT_STATE.md` 的限制。

### 4.11 RAW 自测题

1. 一个 RAW10 像素的数值 `600`，能否直接解释为 `{R=600,G=?,B=?}`？为什么？
2. 把 RAW10 的最低 2 bit 丢弃后，`raw10=511` 会变成多少 RAW8？
3. 为什么 Bayer 流漏掉一列可能导致颜色错误，而不仅是图像横向少一列？
4. 为什么遮住镜头后 RAW 不一定是 0？
5. 为什么不能把 MIPI CSI 线上的 RAW10 五字节顺序直接假定为顶层 `rx_out_data[39:0]` 的 bit 顺序？

答案：

1. 不能。它只代表该坐标 CFA 所过滤的一种颜色采样，完整 RGB 需要 Debayer 邻域估计。
2. `511 >> 2 = 127`。
3. CFA 颜色身份依赖 x/y 奇偶；漏列会使后续位置的 R/G/B 相位翻转。
4. 存在黑电平偏置、读出噪声、暗电流和可能的传感器内部处理。
5. CSI 接收 IP 可能已经完成字节解包、位重排或并行总线打包；应以 IP 接口契约和真实波形为准。

## 5. 时序信号词典

| 信号 | 含义 | 典型误解 |
|---|---|---|
| `VS` | 帧同步边界；极性和“上升沿是开始还是结束”必须按模块实际约定核对 | 以为所有模块都使用同一种极性/边沿 |
| `HS` | 行同步或行边界辅助信息 | 把它当作“这一拍像素有效” |
| `DE` | data enable，显示有效区；一般表示当前拍属于有效画面 | 认为 `DE=1` 必然意味着流水线结果已有效 |
| `valid` | 当前数据的有效性；可用于表示经过流水线延迟后结果是否有效 | 忽略它，只按 `DE` 统计 |
| `ready` | 接收方此拍能否接收数据；AXI 等可回压协议常用 | 认为所有视频流都有 `ready` |
| `frame_ready` | 本工程 framebuffer 输出链路的健康/就绪状态 | 把它当成逐像素 `ready` |
| `ack` | 消费者确认已读取某个快照 | 用常量高电平代替带帧号的确认 |

### 5.1 无回压视频流与 AXI 的区别

CSI、Debayer 和大多数像素流水线在有效期间每拍都会产生数据。它们通常没有逐拍 `ready`：下游一旦忙不过来，不能要求传感器停一拍，只能使用足够吞吐的流水、FIFO、丢帧策略或帧缓存。

DDR 侧 AXI 则是有回压协议：只有 `VALID && READY` 同时为 1 的拍，读写地址或数据才真正完成传输。调试 DDR 时要看 `AWVALID/AWREADY`、`WVALID/WREADY`、`ARVALID/ARREADY`、`RVALID/RREADY`，而不是只看某个数据总线有翻转。

## 6. 从采集到 RGB：逐级理解数据与时序

### 6.1 CSI RAW10：40 bit 是四颗像素

当前顶层参数 `PACK_BIT=40`，并检查 MIPI CSI datatype `6'h2B`（RAW10）和 `pixel_per_clk=4`。每拍包含四个 10-bit Bayer 采样值：

```text
RAW10 packed 40 bit
  P3[9:0] | P2[9:0] | P1[9:0] | P0[9:0]

当前转换保留每颗像素的高 8 bit：
  P3[9:2] | P2[9:2] | P1[9:2] | P0[9:2]
  = RAW8, 32 bit, 4ppc
```

这一步不是 Debayer。RAW8 中每颗像素仍只代表 Bayer 阵列中的一种颜色采样，例如某个位置只有 R，邻近位置可能只有 G 或 B。

采集端最小观测集：

- `rx_out_vs`、`rx_out_hs`、`rx_out_de` 是否存在且关系稳定；
- `rx_out_datatype == 6'h2B`；
- `rx_out_pixel_per_clk == 4`；
- `rx_out_data` 在 `DE` 时是否变化；
- 摄像头切换或复位后，上述信号来自哪一路 ch0/ch1。

### 6.2 DDR framebuffer：把“实时流”变成可平滑读取的帧

framebuffer 输入是 `32-bit` RAW8 x4，输出配置为 `16-bit` Bayer 样本对。它同时跨越输入像素时钟、AXI/DDR 时钟和输出像素时钟，所以这里是最重要的时序风险点之一。

简化过程如下：

```text
输入像素域                         AXI/DDR 域                    输出像素域
VS/HS/DE + RAW8 x4
  -> 写 FIFO -> AXI 写突发 -> DDR -> AXI 读突发 -> 读 FIFO -> Bayer x2 + VS/HS/DE
```

本工程中可观察的帧级状态包括：

- `frame_start`、`frame_stable`：输入帧边界与连续帧长度稳定性；
- `wr_start`、`wr_frame_done`：一帧是否真正写完 DDR；
- `rd_frame_available`、`rd_start`：读侧是否有完整帧可读；
- `fifo_rd_underflow`：输出取数速度超过可用数据，当前帧已不可靠；
- `frame_ready`：输出帧使能、启动同步与无下溢共同满足时才成立。

**学习要点：** framebuffer 的“同一帧”不是肉眼看到的一帧。它至少包含写入完成、读帧选择、读 FIFO 起播、输出 `VS/DE` 四个阶段。任何一个阶段切换错误都可能造成撕裂、旧帧、新旧行混合或周期性条纹。

### 6.3 Debayer：从单色采样恢复 RGB

Debayer 使用行缓存和邻域插值。输入的 Bayer 值要参考相邻行、相邻列才能恢复一颗 RGB 像素，因此它有实际流水延迟，且图像最外缘通常需要专门处理。

当前 `debayer_top_2to1` 的接口是：

| 输入/输出 | 宽度 | 时序意义 |
|---|---:|---|
| `raw_datax4_i` | 16 | 两个 8-bit Bayer 样本 |
| `raw_vs_i/raw_hs_i/raw_de_i/raw_valid_i` | 各 1 | Bayer 输入时序与有效性 |
| `rgb_datax2_o` | 48 | 两个 RGB888 像素 |
| `rgb_vs_o/rgb_hs_o/rgb_de_o/rgb_valid_o` | 各 1 | 与 RGB 输出对齐的时序 |

输出打包严格为：

```text
rgb_datax2_o = {B1, G1, R1, B0, G0, R0}

pixel 0（左侧、偶像素）：R0=[7:0]   G0=[15:8]  B0=[23:16]
pixel 1（右侧、奇像素）：R1=[31:24] G1=[39:32] B1=[47:40]
```

在当前代码中，变量命名存在历史遗留，例如中间信号后缀不总能反映 `DE` 或 `valid` 的真实语义。因此调试时应以模块端口契约和波形为准：只在 `rgb_de_o && rgb_valid_o` 对应的数据拍把 `rgb_datax2_o` 当成有效像素。

### 6.4 白平衡与 Gamma：必须是“数据和时序一起走”的变换

白平衡可按帧统计 RGB 平均值，更新 R/B 增益，再对后续像素执行定点乘法、截位和饱和。Gamma 通常是每通道 LUT 映射，用于压缩或拉伸亮度响应。

它们的共同规则是：

```text
若数据延迟 L 拍，则 VS、HS、DE、valid、像素坐标、帧标签也必须延迟 L 拍。
```

否则会出现两类隐蔽故障：

- 颜色数据来自像素 `(x,y)`，但 `DE` 属于 `(x+1,y)`，造成边缘或行错位；
- 新帧的像素使用了旧帧一半更新后的增益，造成帧内色彩不一致。

当前 `white_balance.v` 以 48-bit RGB x2 输入、48-bit RGB x2 输出，并随时序输出寄存器同步输出。但 HDMI 当前旁路它，预处理 tap 也在它之前。因此现阶段的 ROI 阈值含义是“Debayer 后、未白平衡/未 Gamma 的 RGB”。未来若把统计接到 WB/Gamma 后，阈值、背景色和已采集的标定数据都必须重新定义。

## 7. ROI：空间边界也必须有时序语义

### 7.1 半开区间避免双重计数

当前 ROI 定义为：

```text
[x0, x1) x [y0, y1)
```

即左/上包含，右/下不包含。它的像素数量在无裁剪时为：

```text
roi_pixel_count = (x1 - x0) * (y1 - y0)
```

例如 `[1,5) x [0,2)` 宽 4、高 2，应恰好统计 8 个像素。若用闭区间，会多算右边和下边；若多个 ROI 拼接，半开区间也不会在边界重复统计。

### 7.2 2ppc 下每颗像素独立判断

一拍有 `P0(x)` 和 `P1(x+1)` 两颗像素。ROI 不能因为一颗命中就把整拍都计入：

```text
roi_hit0 = pixel_valid0 && x0 <= x < x1 && y0 <= y < y1
roi_hit1 = pixel_valid1 && x0 <= x+1 < x1 && y0 <= y < y1
```

因此 ROI 左右边界为奇数时完全可以正确工作。例如 ROI 从 `x=1` 开始，第一拍的 `P0(x=0)` 不命中，`P1(x=1)` 命中；不能为了方便把 ROI 向偶数坐标取整。

### 7.3 当前坐标发生器的帧/行定义

`vision_stream_adapter_2ppc` 使用：

- `pixel_active = i_de && i_valid`；
- `frame_start = i_vs && !vs_d`；
- `DE` 上升沿作为主要行起点；
- `HS` 上升沿作为交叉核验；
- 每个有效拍 `x_count += 2`。

这是一份明确的接口假设，不是“所有视频模块都天然如此”。把真实 Debayer 流接入或修改其时序之前，必须抓取 `rgb1_vs/rgb1_hs/rgb1_de/rgb1_valid/rgb1_datax2` 波形，确认：

1. `VS` 的实际极性及上升沿是否真的是帧起点；
2. 一行开始时 `DE` 与 `HS` 的相对位置；
3. 一行有效宽是否为偶数；
4. `valid` 是否总与 `DE` 同步，或是否存在流水空洞。

## 8. 统计：从逐像素结果到帧级快照

### 8.1 每拍计算什么

`pixel_mask_2ppc` 对 ROI 内的每颗像素独立产生：红、蓝、黄、通用前景掩码和亮度和。

当前亮度定义为未除以 3 的和：

```text
Y = R + G + B, 范围为 0..765，使用 10 bit 保存
```

累计器输出：

| 字段 | 宽度 | 含义 |
|---|---:|---|
| `roi_pixel_count` | 32 | ROI 内实际参与统计的像素数 |
| `sum_r/g/b/y` | 各 32 | ROI 内通道总和，CPU 可再除以像素数求平均值 |
| `red/blue/yellow_area` | 各 32 | 三类轻量颜色掩码命中像素数 |
| `fg_area` | 32 | 通用前景面积，不等于三种颜色面积之和 |
| `bbox_min/max` | 各 32 | `{y[15:0], x[15:0]}`，前景像素坐标极值 |
| `center` | 32 | bbox 中点 `{y, x}` |
| `status` | 32 | bit0 空前景，bit1 非法 ROI |

对 1920x1080 全画面，最多约 2,073,600 像素；`sum_y` 最大约 1,586,304,000，仍落在 32-bit 无符号范围内。若未来提高分辨率、位深或把多个通道累加到同一字段，必须重做位宽上界计算，不能沿用 32 bit 的结论。

### 8.2 bbox 与 ROI 的边界语义不同

ROI 是半开区间。bbox 的 `min/max` 却是命中像素的实际坐标极值，二者均为包含端点。因此 bbox 像素宽高应计算为：

```text
bbox_width  = bbox_x_max - bbox_x_min + 1
bbox_height = bbox_y_max - bbox_y_min + 1
```

若 `fg_area=0`，当前逻辑把 bbox 和 center 清零，并置 `status[0]=1`。CPU 和 OSD 不得把此时的 `{0,0}` 误解释为左上角真的检测到了目标。

### 8.3 为什么快照在下一帧开始时发布

累加器在帧 N 的所有有效像素上持续更新。若 CPU 在帧 N 尚未结束时读取面积、bbox、和，字段可能来自不同累计时刻，形成撕裂结果。

当前模块用下一帧的 `VS` 上升沿处理这个问题：

```text
VS(N)       ：锁存 N 帧的 active ROI/阈值，清零 N 帧累加器
N 的有效像素：逐拍累加
VS(N+1)     ：把“已完成的 N 帧”整组锁存为 snapshot
              同时开始累加 N+1 帧
```

这意味着：**在 `VS(N+1)` 时可见的快照，语义上属于 N，而不是 N+1。** 如果摄像头停止且永远不再出现下一次 `VS`，最后一帧不会被当前设计发布为完成快照。

### 8.4 `frame_id` 的能力与局限

当前 `o_frame_id` 在一个完整统计帧被成功装入 snapshot 时递增。它适合做“这份快照是否已被 ACK”的版本号，但不是完整的传感器原始帧计数器：若旧快照未被 ACK，后续帧会记入 `dropped_frames` 而不会产生新的 `frame_id`。

因此，将来若需要严谨地把统计与显示画面逐帧对应，应增加：

```text
capture_seq             ：每次源 VS 都递增，不因丢快照而跳过
snapshot_capture_seq    ：锁存入快照，说明统计来自哪个源帧
cfg_seq                 ：说明此帧使用哪套 ROI/阈值配置
display_capture_seq     ：说明当前 HDMI/OSD 正在显示哪个源帧
```

只用当前 `frame_id` 加肉眼观察，不能证明屏幕与统计来自同一物理源帧。

## 9. 快照 CDC：怎样避免 CPU 读到半帧数据

像素域与 APB/CPU 域通常时钟不同。禁止把持续变化的 32-bit 统计总线逐位同步到 CPU 域，因为每一位可能在不同拍被采到，得到一个从未存在过的数。

正确协议是“稳定数据总线 + 单比特请求 + 带帧号 ACK”：

```text
像素域：锁存完整 snapshot 寄存器组
  -> snapshot_valid=1，保持所有字段不变
  -> request/toggle 跨 CDC

APB/CPU 域：同步 request
  -> 读 status/frame_id
  -> 读完整字段组
  -> 再读 status/frame_id
  -> 两次一致才接受
  -> ACK(frame_id) 经 CDC 返回

像素域：只在 ACK 的 frame_id 匹配当前 snapshot 时清 valid
```

当前预处理顶层把 `i_snapshot_ack` 固定为 `1'b1`，只用于像素域 debug。它会使快照在新帧边界短暂出现后很快被清除，**不能当作 CPU 已稳定读取的 APB 接口**。正式 APB/CDC RTL、生成的 `soc.h` 和对应硬件验证仍未完成。

## 10. 显示路径：2ppc 到 1ppc 的时钟域跨越

Debayer/预处理在 `i_sysclk_div2` 域工作；HDMI 使用 `hdmi_tx_slow_clk`。当前 `video_2pix_to_1pix_cdc` 用异步 FIFO 跨域：

```text
写侧每项：{VS, HS, DE, RGB888x2} = 51 bit
读侧第 1 拍：输出低 24 bit，即第 0 颗像素
读侧第 2 拍：输出高 24 bit，即第 1 颗像素
```

顶层为每路 HDMI bridge 配置 `FIFO_DEPTH=4096`、`START_LEVEL=256`。读侧积累到起播水位才 `o_active=1`；运行中 FIFO 读空时会报告 `o_underflow` 并退出活动状态。

这说明：

- FIFO 解决的是跨时钟和速率差，不自动证明 RGB 字节序正确；
- `o_active` 只说明 bridge 已开始输出，不说明输入是正确的摄像头帧；
- `o_underflow` 出现过的帧不应作为画质、ROI 或统计一致性的证据；
- 2ppc 的两个像素必须按确定顺序拆成两个 1ppc 输出拍，`VS/HS/DE` 也必须与这两个输出拍保持一致。

当前 HDMI 支路在旁路白平衡时直接使用 Debayer 的 `{B1,G1,R1,B0,G0,R0}` 打包；顶层在送入 `hdmi_top` 时再从 24-bit 输出中抽取 R/G/B。因此应使用红、绿、蓝、白、黑的已知测试图或真实标定卡核验字节序，不能只看自然场景“颜色大致正常”。

## 11. OSD：它是像素渲染，不是 CPU 文本打印

系统职责应是：CPU 根据稳定统计快照做颜色/形状/尺寸和任务判断；FPGA 只把 CPU 提交的识别、判断、执行/不执行及理由渲染为像素。

合理的 OSD 放置位置如下：

```text
RGB 像素流 -> OSD renderer -> 显示 CDC -> HDMI
                 ^
                 | 在像素域 VS 边界切换的 active OSD 配置
CPU -> staging 寄存器 -> commit(config/result_seq) -> CDC
```

CPU 不能逐字节、逐字段直接改正在扫描的 OSD 寄存器，否则会出现半个 bbox 属于旧结果、文字属于新结果的画面。正确方法：

1. CPU 写完整 staging 组：enable、bbox、颜色、文本/理由码、`source_frame_id`；
2. CPU 最后写 `COMMIT(result_seq)`；
3. 像素域把整组同步到 shadow；
4. 只在 `VS` 边界将 shadow 切为 active；
5. OSD renderer 整帧只读取 active 组。

当前工程的 CPU->OSD、APB 和 renderer 尚未形成正式闭环，因此本节是实现门槛而不是完成声明。

### 11.1 “同一帧叠框”比“同一帧统计”更难

统计帧 N 在 `VS(N+1)` 才发布，CPU 计算又需要时间，HDMI 经过 framebuffer 和 CDC 还有显示延迟。CPU 得到 N 的 bbox 时，屏幕可能已经在显示 N+1、N+2 或更晚的画面。

所以 OSD 需要明确选择一种语义：

- **帧锁定显示：** framebuffer/显示链路携带 `capture_seq`，仅在显示 N 时叠加 `source_frame_id=N` 的结果；
- **低延迟状态显示：** OSD 显示“最近一次已完成识别结果”，并保留 `source_frame_id`，但不宣称 bbox 与当前画面逐像素重合；
- **只显示语义，不画旧 bbox：** 对比赛决策更保守，显示识别/判断/理由，bbox 仅用于调试。

无论选择哪种，都不能通过“看上去框在物体上”替代帧身份验证。

## 12. 一次完整的帧级对齐示例

假设 ROI/阈值版本为 `cfg_seq=17`，摄像头 ch1 正在采集：

```text
时刻 A，VS(N) 上升沿
  - 像素域把 cfg_seq=17 的 ROI/阈值切为 active
  - 清零 N 的统计累加器

时刻 B，帧 N 的所有 DE && valid 拍
  - P0/P1 分别判断 ROI 命中
  - 累加 ROI 面积、颜色面积、前景面积、bbox

时刻 C，VS(N+1) 上升沿
  - 原子锁存 snapshot：{snapshot_frame_id, cfg_seq=17, ROI, fields}
  - snapshot 的内容属于 N
  - 开始累加 N+1

时刻 D，CPU/APB 域
  - 两次读取 status/frame_id 一致后接受整组数据
  - 用同一个 frame_id ACK，而不是只写一个无标签 ACK

时刻 E，CPU 得到分类/判断
  - 写完整 OSD staging
  - 提交 source_frame_id=N、result_seq
  - 像素域只在后续 VS 边界整体切换 OSD active 组
```

这条链中若缺少 `cfg_seq`、通道号或 `source_frame_id`，就无法证明“红色面积、bbox、CPU 判断和 OSD 框”使用的是同一条件下的同一源帧。

## 13. 调试时该看什么，而不是只看屏幕

| 位置 | 建议探针/日志 | 正常现象 | 常见故障含义 |
|---|---|---|---|
| CSI 输出 | `VS/HS/DE`、datatype、ppc、数据变化 | RAW10 + 4ppc 与有效像素活动一致 | 摄像头未启动、格式不符、解析端无数据 |
| framebuffer 写侧 | `frame_start`、`frame_stable`、写 FIFO、`AWVALID` | 帧长度稳定后发起完整写帧 | 写 FIFO/DDR/帧长度异常 |
| framebuffer 读侧 | `wr_frame_done`、`rd_frame_available`、`ARVALID`、underflow | 完整帧可读且无下溢 | 黑屏、条纹、旧帧或读取饥饿 |
| Debayer | `rgb_vs/hs/de/valid/datax2` | 输出时序与 RGB 打包同拍 | Bayer 相位、行缓存延迟、字节序错误 |
| ROI adapter | `frame_start`、`x0/x1/y`、`pixel_valid0/1` | 每有效拍坐标前进 2，行首归零 | 横向翻倍、行号错位、ROI 边界错 |
| accumulator | hit、面积、bbox valid | 仅 ROI 内命中被累计 | ROI 外污染、双像素重复/漏计 |
| snapshot | valid、frame_id、status、dropped | 一整组字段稳定到 ACK | 半帧数据、ACK 不匹配、消费者太慢 |
| HDMI CDC | FIFO level、active、underflow | 达水位起播，运行无下溢 | 速率不匹配、时钟域或 FIFO 参数问题 |
| HDMI | 输入 DE/VS/HS、使用输入视频状态 | 已确认的有效输入被编码为 TMDS | 仅有 fallback 色条或时序尺寸不匹配 |

### 13.1 常见错误与快速判断

| 现象 | 不应得出的结论 | 更合理的排查 |
|---|---|---|
| HDMI 有色条 | “摄像头链路正常” | 色条可能只是 HDMI fallback；先看 CSI、DDR、bridge、输入选择 |
| HDMI 有真实画面 | “ROI/统计/CPU 已对齐” | 看 ch1 tap、快照、帧号、配置版本、APB ACK |
| ROI 面积是期望值两倍 | “阈值错误” | 先检查 2ppc 是否把一拍当一像素或两次计数 |
| bbox 总在右移一像素 | “物体在移动” | 检查 P0/P1 字节序、`x_count += 2` 和行首条件 |
| 颜色统计红蓝互换 | “分类器阈值不好” | 先用已知色卡核验 `{B,G,R}` 与 `{R,G,B}` 的转换 |
| CPU 偶尔读到不可能的数 | “CPU 算法不稳定” | 检查跨域多位总线撕裂、双读校验和 frame_id ACK |
| OSD 框滞后 | “OSD renderer 坏了” | 分清 capture、snapshot、CPU、display 的帧延迟，检查 source_frame_id |

## 14. 建议的学习与验证顺序

1. **先读单模块接口。** 从 `vision_stream_adapter_2ppc.v` 开始，手算一拍 48-bit 数据如何拆为两颗 RGB 像素。
2. **跑/读独立 testbench。** `tests/fpga_sim/feature_extract/tb_vision_preprocess_channel.v` 覆盖 2ppc 字节序、半开 ROI、面积、bbox、无 ACK 保持和丢帧计数。当前是否在本机实际运行，要以本次日志为准。
3. **用小尺寸人工帧做手算。** 例如 ROI `[1,5) x [0,2)`，逐拍写出 P0/P1 的命中、累加值和 bbox。
4. **再看真实 Debayer 波形。** 先确认 `VS/HS/DE/valid` 与 48-bit 字节序，才允许相信统计结果。
5. **最后做跨域。** 先把 snapshot request/ACK 做成稳定数据 + toggle，再接 APB/CPU；不要反过来把变化中的统计总线直接接 MMIO。
6. **OSD 最后接入。** 先实现无副作用的 bbox/状态渲染，再处理 CPU 结果语义和 display-frame 对齐。

## 15. 自测题

### 题 1：48-bit 数据拆包

若 `i_rgb_2ppc = {B1,G1,R1,B0,G0,R0}`，请写出 P0、P1 的 R/G/B bit 范围。

答案：P0 为 `R0=[7:0]`、`G0=[15:8]`、`B0=[23:16]`；P1 为 `R1=[31:24]`、`G1=[39:32]`、`B1=[47:40]`。

### 题 2：ROI 面积

ROI 为 `[3,8) x [10,13)`，面积是多少？如果一拍处理 x=2 和 x=3 两个像素，哪一个命中？

答案：宽 5、高 3，面积 15。x=2 的 P0 不命中，x=3 的 P1 命中。

### 题 3：快照归属

在 `VS(N+1)` 的同一拍，模块锁存统计快照并清零累计器。该快照属于哪一帧？

答案：属于 N；它在 N+1 的边界才被发布。

### 题 4：为什么双读 frame_id

CPU 读 `frame_id`、读面积/bbox、再读 `frame_id`。两次不一致为什么必须丢弃？

答案：说明读期间快照可能被切换，面积和 bbox 可能不属于同一原子快照。即使单个字段看起来合理，组合也不可信。

### 题 5：为什么“画面正常”不等于“统计正常”

至少列出两种原因。

答案：统计可能固定取 ch1 而 HDMI 显示 ch0；统计可能位于 Debayer 后、白平衡前，而显示位于另一路/另一时刻；APB ACK 与快照 CDC 未实现；HDMI 已显示较晚一帧而快照代表较早一帧。

## 16. 跟着一帧数据走完整视频链路

本节只讲当前 `main@9acf4d8` 的源码连接。它从 CSI 接收“一拍四颗 RAW10”开始，一直走到 HDMI TMDS 输出，并单独标出 ch1 的统计旁路。阅读时始终记住：一条链路的模块存在，不表示当前批次已经完成 PNR、bitstream 或板级验证。

### 16.1 总览：一帧数据分成主显示链与统计旁路

```text
Camera ch0 / ch1
  |
  | MIPI CSI-2 serial lanes
  v
soft_mipi_rx_top
  | i_sysclk_div2 domain
  | {VS, HS, DE, RAW10 x4}, 40 bit/clock
  v
raw10_4pix_to_raw8_4pix
  | {RAW8 x4}, 32 bit/clock
  v
frame_buffer (each channel has an independent instance)
  | write FIFO -> AXI0/DDR -> AXI read -> read FIFO
  | {VS, HS, Bayer RAW8 x2}, 16 bit/clock
  v
debayer_top_2to1
  | {VS, HS, DE, valid, RGB888 x2}, 48 bit/clock
  +-----------------------------+---------------------------------------+
  |                             |                                       |
  | ch1 only                    | ch0/ch1 display selectable            |
  v                             v                                       |
vision_preprocess_channel   white_balance instances                     |
  | ROI/mask/area/bbox         (currently bypassed for HDMI)            |
  | snapshot debug wires             |                                  |
  |                                  v                                  |
  |                         video_2pix_to_1pix_cdc                      |
  |                           i_sysclk_div2 -> hdmi_tx_slow_clk         |
  |                           51-bit FIFO item -> RGB888 x1             |
  |                                  |                                  |
  +-- no current APB/CPU/OSD -------+--> hdmi_top -> dvi_encoder -> TMDS
```

ch0 与 ch1 的采集、framebuffer、Debayer 和 HDMI CDC 是对称的两路。顶层通过按键选择某一路作为 HDMI 输入；**但预处理当前只从 ch1 取样**。所以后续的“显示帧”和“统计快照”只有在 HDMI 正在选择 ch1、且进一步建立源帧身份时才有机会对应；当前代码并未实现这种端到端帧锁定。

### 16.2 第 0 步：摄像头与 CSI 接收器之前

摄像头传感器把曝光获得的 Bayer RAW 数据封装为 MIPI CSI-2 字节流，通过时钟 lane 和数据 lane 送到 FPGA。顶层为 ch0、ch1 各实例化一个 `soft_mipi_rx_top`，它们分别接收各自摄像头的 MIPI 信号与 I2C 配置接口。

这个阶段要区分两套时钟：

```text
MIPI/byte-clock 相关时钟：用于物理层与 CSI 字节接收
i_sysclk_div2：CSI IP 输出的像素域，后续 RAW/framebuffer/Debayer 主要在此工作
```

`soft_mipi_rx_top` 的输出是并行像素域接口，而不是把 MIPI 串行比特直接交给后级 RTL。顶层从每个通道得到：

| 信号 | 含义 |
|---|---|
| `rx_out_vs` | CSI 输出帧同步 |
| `rx_out_hs` | CSI 输出行同步 |
| `rx_out_de` | 当前拍的像素数据有效 |
| `rx_out_data[39:0]` | 当前拍携带的四颗 RAW10 采样 |
| `rx_out_datatype` | 当前 CSI 数据类型；期望 RAW10 `6'h2B` |
| `rx_out_pixel_per_clk` | 当前拍像素数；期望 4 |

换言之，`40 bit` 的逻辑语义是：

```text
P3[9:0] | P2[9:0] | P1[9:0] | P0[9:0]
```

只有当 `DE` 有效，且 datatype 与 ppc 符合期望时，后续才有理由把这拍解释为四颗 RAW10 像素。当前顶层用 `ch*_csi_format_ok` 监测 `RAW10 + 4ppc`，但这个监测信号不是数据通路本身的替代品。

### 16.3 第 1 步：RAW10 x4 截为 RAW8 x4

顶层函数 `raw10_4pix_to_raw8_4pix` 对每颗 10-bit RAW 取最高 8 bit：

```text
输入：P3[9:0] | P2[9:0] | P1[9:0] | P0[9:0]    = 40 bit
输出：P3[9:2] | P2[9:2] | P1[9:2] | P0[9:2]    = 32 bit
```

此处最容易犯的错是以为“40 bit -> 32 bit”把四颗像素变成了两颗像素。实际上 ppc 没变：仍是四颗像素/拍，只是每颗从 10 bit 降为 8 bit。

```text
CSI pixel domain
{VS, HS, DE, RAW10 x4}
    -> 同拍保留 VS/HS/DE
{VS, HS, DE, RAW8 x4}
```

这个模块不做颜色恢复。每个 RAW8 值仍是 Bayer 阵列中的单通道采样，颜色身份由其空间坐标和传感器 CFA 模式决定。

### 16.4 第 2 步：framebuffer 将实时 RAW 流写入并读回 DDR

每个通道各有一个 `frame_buffer` 实例：

```text
ch0: ch0_raw8_4pix -> u_frame_buffer  -> ch0_vs/hs/de + {ch0_g,ch0_b}
ch1: ch1_raw8_4pix -> u_frame_buffer1 -> ch1_vs/hs/de + {ch1_g,ch1_b}
```

这里的变量名带有历史遗留，不要根据 `g/b` 名称推断“已经变成 RGB”。输出的 `{ch*_g,ch*_b}` 是 16-bit 的 Bayer 样本对，下一步仍要进入 Debayer。

framebuffer 内部的完整过程可分为五段：

```text
1. 输入像素域 i_sysclk_div2：
   VS/HS/DE + RAW8 x4 -> 对齐/写 FIFO

2. 帧边界判断：
   frame_info_det 统计帧长度，连续帧长度稳定后产生 frame_stable

3. AXI/DDR 域 axi0_ACLK：
   写 FIFO -> AXI 写突发 -> DDR
   写完一帧后产生 wr_frame_done

4. AXI/DDR 域：
   有完整写帧可用后 -> AXI 读突发 -> 读 FIFO

5. 输出像素域 i_sysclk_div2：
   读 FIFO -> par2ser_parse -> data_tx
   重新生成输出 VS/HS/DE 与 Bayer x2
```

framebuffer 不是简单“缓存一下数据”。它把传感器持续输出的写侧流，与显示所需的稳定读侧流隔开，并引入了至少帧级的延迟与多时钟域控制。此处的关键健康条件为：

```text
frame_stable          ：输入帧长度连续稳定
wr_frame_done         ：完整帧已写入 DDR
rd_frame_available    ：读侧存在可读完整帧
frame_ready           ：输出链路已启用、同步完成且无 latched underflow
fifo_rd_underflow=0   ：输出没有向空 FIFO 取数
```

若出现 `fifo_rd_underflow`，即便 HDMI 屏幕没有完全黑掉，也不能认为该输出帧、它的 Bayer 时序或基于它的观察结论可靠。

### 16.5 第 3 步：Bayer x2 进入 Debayer，得到 RGB888 x2

每路 framebuffer 输出都进入一个 `debayer_top_2to1`：

```text
输入：ch*_vs, ch*_hs, ch*_de, raw_valid=ch*_de, ch*_bayer_2pix[15:0]
时钟：i_sysclk_div2

输出：rgb*_vs, rgb*_hs, rgb*_de, rgb*_valid, rgb*_datax2[47:0]
```

在输入端，`ch*_bayer_2pix` 由两颗 8-bit Bayer 样本组成。`CH*_BAYER_SWAP_PIXELS` 决定这对样本是否交换顺序，目的是让空间顺序与实际 Bayer 相位匹配。

Debayer 内部用行缓存取得邻近行、邻近列的 Bayer 数据，并推算每个位置缺少的两个颜色通道。它输出两颗完整 RGB888：

```text
rgb*_datax2 = {B1, G1, R1, B0, G0, R0}

P0（较左的像素）= {R0=[7:0],   G0=[15:8],  B0=[23:16]}
P1（较右的像素）= {R1=[31:24], G1=[39:32], B1=[47:40]}
```

Debayer 是有行缓存、有流水延迟的二维运算。因此后级只能使用 `rgb*_vs/hs/de/valid`，不能拿原始 CSI 或 framebuffer 输入的 `VS/HS/DE` 搭配 `rgb*_datax2`。若数据与标志相差一拍，画面会发生错位；若 Bayer 相位错误，红蓝会互换或产生伪色。

### 16.6 第 4 步：从同一 RGB x2 流分出统计支路和显示支路

Debayer 后，数据逻辑上分成两条支路。

#### A. ch1 预处理/统计旁路

当前只有 ch1 连入 `vision_preprocess_channel`：

```text
rgb1_vs/hs/de/valid + rgb1_datax2
  -> vision_stream_adapter_2ppc
  -> ROI hit0/hit1
  -> pixel_mask_2ppc
  -> feature_accumulator_2ppc
  -> feature_snapshot
  -> mark_debug snapshot wires
```

这条支路仍在 `i_sysclk_div2` 像素域：

- adapter 将 48-bit 拆成 P0/P1，生成 `x0/x1/y`；
- ROI 对 P0、P1 独立判断，因此奇数边界不会误计整拍；
- mask 生成红/蓝/黄/前景掩码和亮度和；
- accumulator 对整帧累加面积、RGB/Y 和、bbox；
- 下一帧 `VS` 上升沿发布上一帧快照。

当前这条支路有两个硬边界：

```text
1. 只取 ch1，且不反向影响 Debayer/HDMI。
2. snapshot_ack 固定为 1'b1，输出只作为 mark_debug 像素域观测；
   尚未形成正式 APB/CDC/CPU 读取接口。
```

因此，统计支路存在不等于 CPU 已经读到它，也不等于 OSD 已显示它。

#### B. ch0/ch1 可选择的显示支路

两路 Debayer RGB 流各自进入白平衡实例，但 HDMI 目前通过：

```text
HDMI_BYPASS_WHITE_BALANCE = 1'b1
```

直接选用 Debayer 的 RGB x2 输出，白平衡模块当前不在 HDMI 数据通路中。然后 ch0、ch1 各进入一个 `video_2pix_to_1pix_cdc`。

注意：顶层还把 `{B1,G1,R1,B0,G0,R0}` 重新排列成供白平衡模块使用的 `{R1,G1,B1,R0,G0,B0}`，但 bypass HDMI 直接使用的是 Debayer 原始 48-bit 打包。这个细节说明“数据是 RGB”还不够，必须同时记录其打包字节序。

### 16.7 第 5 步：2ppc RGB 跨到 HDMI 时钟域并拆为 1ppc

显示支路的写侧是 `i_sysclk_div2`，HDMI 读侧是 `hdmi_tx_slow_clk`，属于不同时钟域。每路 `video_2pix_to_1pix_cdc` 的工作如下：

```text
写侧（一拍两像素）：
  FIFO 写入 {VS, HS, DE, RGB888_P1, RGB888_P0}
  总宽度 = 1 + 1 + 1 + 48 = 51 bit

读侧（两拍一组）：
  第一个输出拍：VS/HS/DE + low 24 bit  = P0
  第二个输出拍：VS/HS/DE + high 24 bit = P1
```

当前每路 bridge 的参数为：

```text
FIFO_DEPTH = 4096
START_LEVEL = 256
```

读侧在 FIFO 内至少积累 `START_LEVEL` 个 51-bit 条目才激活，以抵抗两个时钟的相位/频率差与上游抖动。运行中 FIFO 读空则置 `o_underflow`，退出 active，后续等待重新积累水位。

顶层通过按键产生 `channel_sel`，从两路 bridge 输出中选择一条：

```text
selected_hdmi_{vs,hs,de,data}
  = channel_sel ? hdmi1_bridge_* : hdmi0_bridge_*
```

切换通道时，顶层清除上一通道的 ready 和数据历史，防止拿 ch0 的“已稳定”状态冒充 ch1 的状态。

### 16.8 第 6 步：HDMI 输入门与 DVI/TMDS 编码

顶层将选择后的 1ppc 流寄存为：

```text
rgb_vs_r, rgb_hs_r, rgb_de_r, rgb_datax1[23:0]
```

并送进 `hdmi_top`。`hdmi_top` 的职责不是再做图像处理，而是：

1. 决定使用输入视频还是 fallback 色条；
2. 将 RGB、HS、VS、DE 编成三个 TMDS 数据通道和一个 TMDS 时钟；
3. 驱动 HDMI/DVI 发射引脚。

`hdmi_top` 内部具备输入时序检查和“连续好帧”逻辑：它可检查有效宽高、总行/总帧和同步错误，并在合格帧数量达到阈值后允许使用输入画面。但当前顶层实例把：

```text
USE_INPUT_STABLE_GATE = 1'b0
```

也就是说当前顶层不强制等待该模块内部的多帧稳定门；只要 `i_video_ready` 成立，HDMI 更直接地使用输入流。`i_video_ready` 来自 selected bridge 已经活动、出现 `DE` 或数据变化后的同步状态。

因此屏幕现象的解释必须分级：

```text
色条：可能只是 hdmi_top fallback / TMDS 发射正常。
真实画面：说明选择的 bridge 至少在输出数据；不自动证明 CSI 格式、DDR、Debayer、ROI 或统计全对。
稳定真实画面 + 无 underflow + 可核对时序：才是显示支路较强的证据。
```

### 16.9 第 7 步：TMDS 输出到显示器

`dvi_encoder` 为每个输出像素时钟拍编码三个颜色通道：

```text
输入：R[7:0], G[7:0], B[7:0], HS, VS, DE
输出：TMDS data0[9:0], data1[9:0], data2[9:0], tmds_clk[9:0]
```

在 `DE=1` 时，TMDS 主要承载编码后的颜色数据；在消隐区，控制信息（包括同步）按 TMDS 规则编码。最终高速物理发射器把这些并行编码字送到 HDMI 差分线。

对后级显示器而言，看到的是 RGB 视频和同步；它不知道 ROI、统计、CPU 判断或源帧号。因此这些语义必须由 FPGA/CPU 的额外接口与 OSD 协议显式保持，不能期待 HDMI 本身替你保存。

### 16.10 同一帧在各位置的“身份”如何变化

同一源帧从 CSI 到显示器并不是同一时刻到达每一层：

```text
采集帧 N 进入 CSI
  -> 写入 DDR
  -> 被 framebuffer 读出（可能已是帧 N 或更早/更晚的可用帧，取决于缓冲调度）
  -> Debayer
  -> 统计支路在 VS(N+1) 才发布 N 的快照
  -> 显示支路再经 CDC FIFO 和 HDMI 编码后显示
```

因此，当前显示器上“正在看到”的一帧，与刚发布的统计快照不应仅凭肉眼断言为同一帧。要严格建立对应，需要沿链路携带：

```text
source_channel_id
capture_seq
snapshot_capture_seq
cfg_seq
display_capture_seq
```

当前代码主要有调试 `frame_id`，但尚未实现从 CSI 到 HDMI/OSD 的全链路 `capture_seq`。这正是后续 CPU/APB/OSD 集成前必须明确的接口边界。

### 16.11 逐段故障定位顺序

如果最终 HDMI 画面异常，不要从头修改所有模块。按照数据真正经过的顺序定位：

1. CSI 是否输出 `RAW10 + 4ppc`，并有 `VS/DE`？
2. RAW10->RAW8 的每 4 像素顺序是否符合接收器内部契约？
3. framebuffer 是否完成写帧、读帧，且没有读下溢？
4. Debayer 输出的 `VS/HS/DE/valid` 与 48-bit RGB 是否对齐？
5. 已知色目标的 R/G/B 是否正确，Bayer 相位/像素对顺序是否正确？
6. CDC FIFO 是否达到起播水位、保持 active、未 underflow？
7. selected channel、`hdmi_video_ready`、fallback 使用状态是否一致？
8. 最后才看 TMDS/显示器现象。

若统计异常而显示正常，应从第 4 步的 ch1 RGB tap 开始，依次验证坐标、ROI hit、mask、accumulator、snapshot，而不是先修改 HDMI。

## 17. 源码阅读入口

- `fpga/rtl/top/top.v`：当前顶层接线、RAW10->RAW8、ch0/ch1、预处理 tap、HDMI bridge。
- `fpga/rtl/mipi_csi/soft_mipi_rx_top.v`：CSI 输出接口和像素时钟域。
- `fpga/rtl/framebuffer/frame_buffer.v`：framebuffer、AXI、读写 FIFO 和下溢状态。
- `fpga/rtl/debayer/debayer_top_2to1.v`：Bayer 到 RGB888 x2 的接口与打包。
- `fpga/rtl/roi_crop/vision_stream_adapter_2ppc.v`：2ppc 字节序和坐标生成。
- `fpga/rtl/roi_crop/roi_window_2ppc.v`：半开 ROI 判断。
- `fpga/rtl/feature_extract/pixel_mask_2ppc.v`：亮度、颜色和前景掩码。
- `fpga/rtl/feature_extract/feature_accumulator_2ppc.v`：面积、和、bbox 累加。
- `fpga/rtl/feature_extract/feature_snapshot.v`：帧级快照、ACK、丢帧计数。
- `fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`：2ppc->1ppc 异步 FIFO bridge。
- `fpga/rtl/dvi_tx/hdmi_top.v`：输入稳定性、fallback 和 TMDS 编码。
- `integration/preprocess_apb_cdc_contract_draft_20260711.md`：正式 CPU/APB/OSD 接入前的 CDC 与帧边界约束草案。

## 18. 结论

视频流水线的核心不是“把数据接起来”，而是让每一级都能回答：这拍是什么格式、哪几颗像素、是否有效、属于哪一行哪一帧、使用哪套配置、跨域后是否仍保持同一语义。

对本项目而言，最重要的工程纪律是：统计快照、CPU 判断与 OSD 显示都必须有明确的源通道、帧身份和配置版本；任何缺少这些证据的“画面正常”都只能说明显示链路的一部分在工作，不能证明识别闭环可信。
