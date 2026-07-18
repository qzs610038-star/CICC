# 4. Libaoxun 视角：视频旁路、I1/I2 硬件契约与 F1 接入边界

本指南是 [主图：项目数据链路与三人分工](1_project_data_link_and_roles_guide.md#2-项目整体数据传输链路与队员分工图示) 的 **FPGA 前端细节补充**。它放大主图中的 `视频主链 → Tap → I1` 和 `I2 active config → Tap` 支路，并连接到未来 `I4 → OSD → HDMI` 的硬件侧边界。

> **当前状态（2026-07-18 实读）**：ch0 Debayer 后 RGB 旁路位置和 `feature_stats_tap` RTL/testbench 已存在；顶层把 `i_capture_enable` 固定为 `0`、ACK 固定关闭、特征输出接到 unused 信号。因此统计 RTL 目前**没有**成为 CPU 可读接口，业务 CDC/APB/ACK/I2/I4/OSD 均未实现或未冻结，板级视频/特征/OSD 均未验证。

事实源优先级： [CURRENT_STATE.md](../../CURRENT_STATE.md) → [单摄特征快照契约](../../competition_project_single_camera/integration/single_camera_feature_contract.md) → [F1 接口对齐总账](../../competition_project_single_camera/integration/F1_INTERFACE_ALIGNMENT_DRAFT.md) → 同批 RTL/XML/SoC/BSP/构建证据。

---

## 1. 这份图放大主图的哪一段？

以下图示将主路线图对应的 FPGA 视频旁路截出，并区分两层内容：已存在的视频只读旁路与隔离使能；以及待 F1-ABI 审查的帧末快照、CDC/APB 和完整帧边界配置生效模型。

```mermaid
flowchart TB
    classDef mainTrack fill:#f5f6fa,stroke:#2f3640,stroke-width:2px,color:#2d3436;
    classDef detailTrack fill:#ebf3ff,stroke:#0984e3,stroke-width:2px,color:#2d3436;
    classDef offStyle fill:#fff1f0,stroke:#cf1322,stroke-width:2px,color:#cf1322;
    classDef proposed fill:#fff8e7,stroke:#d69e2e,stroke-width:2px,stroke-dasharray: 6 4,color:#2d3436;

    %% 上半部分：从1中主路线图截出来的对应片段
    subgraph MAIN_PIECE["【主路线图对应片段】(1号总图中的 FPGA 视频旁路与接口链路)"]
        M_CAM["CAM"] --> M_VID["视频主链"] --> M_HDMI["HDMI"]
        M_VID -.->|只读像素旁路| M_TAP["统计 Tap"] -.->|未来 I1 语义| M_I1[["未来 I1 ABI"]]
        M_I2[["未来 I2 配置 ABI"]] -.-> M_TAP
        M_I4[["未来 I4 结果 ABI"]] -.-> M_OSD["未来 OSD Renderer"] -.-> M_HDMI
    end

    %% 下半部分：在此基础上拓展画出的细致逻辑图
    subgraph DETAIL_LOGIC["【libaoxun 负责的视频旁路、隔离门与帧末原子锁存细致链路】"]
        RGB["ch0 Debayer 后 RGB 双像素总线<br/>(rgb0_data_rgb[47:0])"]
        HDMI_OUT["HDMI 实时无延迟显示路径"]

        TAP["feature_stats_tap RTL 模块<br/>(在旁路进行 RGB/ROI/mask/bbox 累加)"]
        DISABLED["当前顶层隔离：i_capture_enable = 0<br/>(i_ack_valid=0，特征输出接 unused)"]

        I2_ACTIVE["拟议 I2 active 配置模型<br/>(完整帧边界整体切换)"]
        SNAP["拟议 I1 原子快照模型<br/>(frame_id + fields + flags)"]
        CDC_BUS["拟议 CDC/APB 实现<br/>(待冻结双读与 ACK 线 ABI)"]

        OSD_DEV["OSD 渲染引擎<br/>(只在收到 valid 且轮次更新时同步显示)"]
    end

    %% 主图与分图拓展关联
    M_CAM ==> RGB
    M_VID ===> HDMI_OUT
    M_TAP ===> TAP
    M_I1 ===> SNAP
    M_I2 ===> I2_ACTIVE
    M_OSD ===> OSD_DEV

    %% 细节流向
    RGB -->|1. 主显示流，零延迟| HDMI_OUT
    RGB -.->|2. 只读旁路像素流| TAP
    DISABLED -.->|3. 强行拉低使能，模块休眠| TAP
    I2_ACTIVE -.->|4. 为整帧累加提供边界与阈值| TAP
    TAP -->|5. 帧消隐开始时原子锁存| SNAP
    SNAP -->|6. 握手同步 CDC 并拉起 valid| CDC_BUS
    OSD_DEV -.->|7. 字符像素物理叠加| HDMI_OUT

    class M_CAM,M_VID,M_HDMI,M_TAP,M_I1,M_I2,M_I4,M_OSD mainTrack;
    class RGB,HDMI_OUT,TAP,SNAP,CDC_BUS,OSD_DEV detailTrack;
    class DISABLED offStyle;
    class I2_ACTIVE proposed;
```

<details>
<summary>🔍 点击展开/收起：Libaoxun FPGA旁路图专有名词释义</summary>

*   **rgb0_data_rgb[47:0]**：视频处理流水线中经过 Debayer 图像还原后产生的 48 位并行双像素 RGB 彩色信号。
*   **i_capture_enable**：顶层 RTL 内部用于控制特征统计模块是否对输入像素进行累加处理的物理使能信号，当前固定为 0。
*   **Active Config**：拟议协议中，完整帧统计时硬件引用的一致配置版本（如 ROI 边界、颜色阈值）；当前没有已冻结的业务寄存器实现。
*   **frame_id**：FPGA 统计引擎对所处视频流帧的流水计数值。
*   **feature_stats_tap**：libaoxun 编写的特征累加统计 RTL 模块，挂载在视频主链路的旁路。
*   **OSD Renderer**：负责在像素流输出到 HDMI 前进行字符图形叠加的专用硬件渲染贴图块。

</details>

<details>
<summary>📖 点击展开/收起：Libaoxun FPGA旁路图图文对照生动表述</summary>

本图展示了**“高速公路旁路称重超载检查站的工作机制”**：
1.  **直行通道（HDMI 主链）**：从摄像头（CAM）开来的大货车源源不断行驶在高速路主干道（VID 视频主链）上，直接出港驶向显示器（HDMI），主干道上没有任何收费站或红绿灯阻挡，保证货车行进绝对零延迟。
2.  **称重旁路（Tap 统计）**：在直行通道旁边，我们开了一个极窄的分流称重车道（只读旁路）。大货车快速通过主干道时，旁路上的称重拍照设备（TAP 统计）默默对货车长宽高、货色（像素）进行抓拍称重，但不要求货车在主干道上刹车减速。目前由于顶层控制室进行了演习安全物理封闭（i_capture_enable=0），分流入口处于上锁隔离状态，所以称重数据目前并不能流入 CPU。
3.  **上传与确认（I1 & ACK）**：未来若通过 F1-ABI 审查，快照可经受审的 CDC/APB 模型交给 CPU，CPU 成功消费同帧后再提出 ACK 语义。地址、ACK 位点与具体跨域实现仍未冻结。
4.  **电子屏广播显示（I4 & OSD）**：I4 到 OSD 是未来目标链路；分类结果的布局、字体、像素叠加和 HDMI 上屏证据均尚不存在，不能将示意图当作现成功能。

</details>

图中唯一不能妥协的事实是：Tap 是只读旁路，不得反向驱动、阻塞、替换或延迟 HDMI 主像素路径。虚线表示未来 F1 原子批次才可接入的业务链，不是当前可连线或可烧写的实现。

---

## 2. Libaoxun 的职责边界

Libaoxun 负责硬件架构与总线接口物理层的稳定，确保硬件不会对 CPU 造成总线对齐死机。

| 模块/接口 | Libaoxun 当前负责的事实与产出 | 必须与谁对齐 | 当前不得做 |
|---|---|---|---|
| 视频主链 | 维护 ch0 视频、Debayer 后 RGB 与 HDMI 显示链的独立性 | qzs 审核任何顶层/XML/SDC/IP 原子改动 | 为接 feature 改坏 framebuffer、Debayer 或 HDMI；用“旁路”掩盖时序/资源风险 |
| Tap 统计 | 维护 Tap 输入时钟、复位、`rgb_vs/rgb_de/rgb0_data_rgb`、ROI/mask/前景/bbox 的字段语义 | wsc 确认字段/flags/ACK 时序；qzs 确认 Gate/Review Packet | 让 RTL 给出颜色/形状/任务判断，或把统计结果称为 CPU 可读 |
| I1 | 设计经审查的帧末锁存、multi-bit snapshot CDC 或异步 FIFO、发布/ACK/overrun 行为 | wsc 的消费/双读/ACK 规则；同批 SoC/BSP | 用逐位两拍同步拼多位数据；未冻结 ABI 前手填 APB 地址 |
| I2 | 实现 `staging → commit(config_seq) → frame-boundary active` 后，Tap 只使用 active 配置 | wsc 的配置语义；qzs 的接口审查 | 写一个配置立即影响半帧统计；让配置反向影响视频显示 |
| I4 / OSD | 仅提供将来像素渲染的硬件实现条件 | wsc 的结果语义与 qzs 的展示要求 | 先做 result/OSD APB、显示裸寄存器值，或宣称 OSD 已板级闭环 |

<details>
<summary>💡 点击展开/收起：I1 接口硬件侧生动形象中文介绍</summary>

I1 的**拟议硬件模型**是“原子卷帘快门与防撕裂保险柜”：帧结束时将同一帧统计字段与 `frame_id` 一并冻结，CPU 仅在成功消费该帧后提出匹配 ACK。具体帧边界信号、寄存器、ACK 位点和 CDC 还未实现或冻结；当前 Tap 输出仍未连接到 CPU。

</details>

<details>
<summary>💡 点击展开/收起：I2 接口硬件侧生动形象中文介绍</summary>

I2 的**拟议硬件模型**是“暂存配置与影子版本”：若未来 CPU 经受审的 APB 写入 ROI/阈值，硬件应以完整帧为界原子切换 staging 配置，避免一帧统计混用两套参数。当前并没有业务 APB、Active Shadow 或可验证的 VSYNC 切换实现。

</details>

<details>
<summary>💡 点击展开/收起：I4 接口硬件侧生动形象中文介绍</summary>

I4 在硬件侧是**“半透明像素图层叠加发生器（OSD Renderer）”**。它只读取 CPU 写完结果并提交了 valid 信号的那一组寄存器，在视频时序控制下，把“第 3 轮”、“红色正方体”以及识别理由的字体像素，在 HDMI 场同步信号下“半透明贴图”在主画面之上。未收到 valid 信号前，OSD 应当渲染为等待或清屏，严禁擅自渲染乱码或者错位的历史框图。

</details>

---

## 3. 当前真实 Tap 与未来 I1 的边界

### 已存在且可核对

- [feature_stats_tap.v](../../competition_project_single_camera/src/feature_stats/feature_stats_tap.v) 将每拍 48-bit RGB 视为两个像素，只累积 ROI 内的红/蓝/黄 mask、前景面积、ROI 像素数、亮度和 bbox；它不驱动视频主链。
- [top.v](../../competition_project_single_camera/src/top.v) 把 Tap 放在 ch0 Debayer 后的 `rgb0_data_rgb` 旁路上，但当前 `i_capture_enable=1'b0`、`i_ack_valid=1'b0`，全部特征输出为 unused。
- [feature_stats_tap_tb.v](../../competition_project_single_camera/tests/rtl/feature_stats_tap_tb.v) 已覆盖双像素计数、匹配/不匹配 ACK、诊断拒绝与计数溢出拒绝。

### 尚未实现，不能由图补成事实

1. Tap 到 APB/CPU 的 CDC 实现、寄存器布局、读取次序、PSTRB、IRQ、复位行为；
2. CPU 对 snapshot 的真实总线读取与相同 `frame_id` ACK；
3. I2 staging/commit/active 寄存器和 `active_seq` 回读；
4. I4 result/OSD 寄存器、OSD renderer 和 HDMI 叠加；
5. 上述任一改动后的同批 Map/PNR/STA/CDC、bitstream、视频回归和板级证据。

---

## 4. I1/I2 硬件必须共同确认的协议

| 接口 | 硬件必须保证 | CPU/系统依赖它做什么 | 尚待冻结 |
|---|---|---|---|
| I1发布 | 一帧结束时整体锁存 `frame_id`、`config_seq`、统计字段与 flags；有效快照在匹配 ACK 前保持一致 | wsc 双读、拒绝撕裂/过载/诊断帧，再对同帧 ACK | CDC 实现、APB 地址/读序、ACK 位点/复位 |
| I1 overrun | 上一快照未 ACK 时新帧不得伪装为正常稳定快照；标记 `SNAPSHOT_OVERRUN`，不阻塞 HDMI | CPU 将该候选视为不稳定，不生成稳定识别 | 物理跨域和状态清除实现 |
| I2 active | ROI/background/mask/`config_seq` 先完整 staging，提交后仅在完整帧边界变为 active | CPU 能以返回的 active `config_seq` 追溯本帧依据 | 寄存器、commit/active 回执、CDC |
| I4 renderer | 只渲染 CPU 提供的可解释颜色/形状/decision/reason 语义 | qzs 能审查屏幕语义与轮次追溯 | payload ABI、字体、像素接口、锁存时序 |

**禁止偷换概念**：颜色/形状/尺寸分类、四任务关系和机械臂状态机属于 CPU；FPGA 只提供受审的基础统计、通道和像素渲染。

---

## 5. Libaoxun 与队友的接口核对清单

| 先核对谁 | 必须一起读的文件 | 要达成的一句话结论 |
|---|---|---|
| wsc | [single_camera_feature_contract.md](../../competition_project_single_camera/integration/single_camera_feature_contract.md)、[single_camera_feature_adapter.h](../../competition_project_single_camera/cpu/include/single_camera_feature_adapter.h)、[single_camera_runtime.h](../../competition_project_single_camera/cpu/include/single_camera_runtime.h) | “Tap 发布什么、CPU 何时拒绝、ACK 对哪个 frame_id、`config_seq` 怎样追溯。” |
| qzs | [F1_INTERFACE_ALIGNMENT_DRAFT.md](../../competition_project_single_camera/integration/F1_INTERFACE_ALIGNMENT_DRAFT.md)、[2_qzs_role_data_link_and_control_logic.md](2_qzs_role_data_link_and_control_logic.md) | “I4 仍是拟议，I5 保持阻断，哪些证据/Review Packet 后才可接线。” |
| 全队 | [top.v](../../competition_project_single_camera/src/top.v)、[mem_test.xml](../../competition_project_single_camera/mem_test.xml)、[mem_test.peri.xml](../../competition_project_single_camera/mem_test.peri.xml)、[constrain.sdc](../../competition_project_single_camera/constrain.sdc) | “任何业务接入是同一硬件原子批次，旧构建/bitstream 不能继承。” |
| 全队 | [CURRENT_STATE.md](../../CURRENT_STATE.md) 与当前 G1 操作卡 | “当前板级 Gate 未关闭；只允许操作卡明确写出的范围。” |

### 本人应先学会的四件事

1. **双像素统计与 ROI 边界**：48-bit RGB 的两个像素怎样在同一有效周期累计，bbox/溢出怎样保持一致。
2. **多位 CDC 与 backpressure 隔离**：为什么 snapshot CDC/异步 FIFO 不能用逐位同步，为什么 overrun 不能回压 HDMI。
3. **配置原子性**：`staging → commit → active` 如何防止 ROI 和阈值在一帧中途变化。
4. **硬件原子批次**：顶层、XML/peri、SDC、IP、BSP/`soc.h` 任一变更为何会使旧构建和板级证据失效。

---

## 6. 交给 Gemini 的编辑边界

Gemini 可以重排版、为这张分图添加直觉类比、波形示意或知识讲解，但**不得**：

- 修改 Tap 的只读旁路原则、`i_capture_enable=0`/ACK 关闭/输出 unused 的当前事实，或把统计 RTL 写成已接 APB/CPU；
- 改写 I1/I2/I4 的方向、字段语义、所有权、未冻结项、Gate 或“板级未验证”状态；
- 把分类、四任务关系、阈值管理、逐轮事务或机械臂状态机迁回 FPGA；
- 修改 `top.v`、XML/peri、SDC McKinney、IP、BSP、构建制品、操作卡或任何硬件配置；
- 添加未经同批 `soc.h` 和 Review Packet 证明的地址、端口、时钟、复位、CDC、OSD 或 UART2 细节。

任何事实性修改必须先由 wsc、libaoxun、qzs 对齐并经 Codex Review Packet 审查。
