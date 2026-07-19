# qzs + wsc F1 Preboard 接口与接板合同

> 状态：`IMPLEMENTATION HANDOFF CONTRACT / NOT FPGA WIRE ABI / NOT BOARD APPROVAL`
>
> 基线：`c296442a811fffdf103625a3883eb36e16aeac0c`

## 1. 合同定位

本文件只规定 qzs 与 wsc 在 Host/离线阶段交换什么、如何验收，以及 UART1
全链路通过后如何快速接入板上验证。它不修改或替代冻结的
`single_camera_feature_contract.md`，不定义 APB 地址、寄存器布局、CDC、IRQ、
PSTRB、管脚或 OSD wire ABI。

既有 C seam 保持不变：

```text
sc_feature_snapshot_t
  -> sc_feature_snapshot_to_observation()
  -> sc_runtime_process_one()
  -> sc_round_result_t
```

`sc_runtime_transport_t` 的 `read_feature_snapshot / ack_feature_frame /
submit_round_result / emit_diagnostic_event` 是唯一允许的板前适配面。
`single_camera_mmio_transport.c` 在真实 ABI 审查前必须继续 fail-closed。

## 2. qzs -> wsc：`QW-CALIBRATION-SAMPLE-v1`

### 2.1 目录和文件

qzs 为每个 batch 建立：

```text
competition_project_single_camera/docs/calibration/f1_preboard/<batch_id>/
  capture_profile.json
  sample_manifest.jsonl
  feature_rows.jsonl
  data_quality_summary.json
  sha256sums.json
```

原始图片/视频不默认提交 Git；manifest 只记录逻辑相对标识、字节数和 SHA-256，
禁止提交本机绝对路径。小型脱敏裁剪样例只有在 `.gitignore`、体积和授权检查
通过后才可单独评审。

### 2.2 每个 `feature_rows.jsonl` 对象的必需字段

| 字段 | 约束 |
|---|---|
| `schema` | 固定 `QW-CALIBRATION-SAMPLE-v1` |
| `batch_id`, `sample_id`, `object_id` | 非空；batch 内 `sample_id` 唯一 |
| `artifact_sha256` | 对原始帧或裁剪图的 64 位十六进制 SHA-256 |
| `evidence_level` | 板前固定 `HOST_CALIBRATION_PROVISIONAL` |
| `color` | `WHITE/BLACK/RED/BLUE/YELLOW` |
| `shape` | `CUBE/CYLINDER/CONE` |
| `nominal_size_cm_x10` | `20/25/30` |
| `lighting_id`, `position_id`, `capture_profile_id` | 必须能追溯固定相机与光照条件 |
| `frame_id`, `config_seq` | Host 生成时也必须显式赋值，不得缺省 |
| `source_flags` | 合法候选固定含 stable/ROI/stats/ch0，保留位为 0 |
| `red_area`, `blue_area`, `yellow_area` | 与冻结 mask 语义一致的像素数 |
| `foreground_area`, `roi_pixel_count` | 逐像素实际统计，不由 bbox 或配置面积猜测 |
| `sum_luma` | 逐像素 `R+G+B` 之和，不改成三通道均值 |
| `bbox_width`, `bbox_height` | 包含端点尺寸；无前景均为 0 |
| `expected_use` | `TRAIN/CALIBRATE/HOLDOUT/NEGATIVE`，同一 object 不跨 train/holdout 泄漏 |

qzs 工具必须拒绝缺字段、额外字段、重复 ID、未知枚举、非法 flags、hash 不匹配、
`foreground_area > roi_pixel_count`、无前景但 bbox 非零等记录。

### 2.3 最小数据量

1. 优先包：5 色 x 3 尺寸 x 正方体 x 3 次 = 45 个稳定样本；
2. 完整包：5 色 x 3 形状 x 3 尺寸 x 3 次 = 135 个稳定样本；
3. 负例：空场、偏位、遮挡、过曝/欠曝、运动模糊、ROI 越界模拟，各至少 3 条。

数据不足时只允许发布实际覆盖范围，不得把 45 样本写成任务二完整标定。

## 3. wsc -> qzs：`QW-CALIBRATION-RESULT-v1`

wsc 在其 CPU 测试范围输出：

```text
competition_project_single_camera/cpu/tests/evidence/f1_preboard/<batch_id>/
  input_identity.json
  classifier_profile.json
  confusion_matrix.csv
  size_calibration.csv
  holdout_predictions.jsonl
  replay_summary.json
  sha256sums.json
```

必需内容：

- `input_identity.json` 绑定 qzs batch、qzs 完整 SHA、五个输入文件 hash；
- 颜色/形状逐类 precision、recall、support，另列 cube vs non-cube；
- 尺寸使用的整数特征、阈值、2/2.5/3 cm 分布、不可判定区间；
- 按 `object_id` 分组的 holdout，禁止同一物体的重复帧泄漏到训练与验证两侧；
- 每个误判的 `sample_id` 和原始特征；
- 对未达标能力输出 `WAIT/SIZE_UNAVAILABLE/PROVISIONAL`，不得强行分类；
- `ARM_ENABLED=0`、`arm_request_count=0`、`arm_send_count=0`。

任何生产阈值都必须来自该固定 batch；Host 合成阈值只能标记 baseline。

## 4. 自检固件 seam：`QW-F1-BOARD-SELFTEST-v1`

wsc 预先实现无 MMIO 地址、无 UART 寄存器知识的板上自检核心：

```text
固定内置 snapshot cases
  -> 既有 classifier/adapter/runtime
  -> callback event sink
  -> 累积确定性 case count/result digest
```

核心不得直接 include `soc.h`，不得写寄存器，不使用堆，不发送机械臂帧。
平台绑定只允许在 UART1 已通过后新增一个小适配层，把 `event sink` 绑定到
同批已验证 UART1 API。建议输出：

```text
@F1SELFTEST|v=1|build=<id>|cases=<n>|pass=<n>|digest=<sha-prefix>|arm=0
```

qzs 的捕获 verifier 按 `build/cases/pass/digest/arm` fail-closed 验证。它最多证明
“同一板上 CPU 能执行 F1 业务核心并通过 UART1 输出”，不证明真实 feature、
APB、CDC、输入或 OSD。

## 5. UART1 成功后的三阶接板梯子

### B0：CPU F1 自检（目标：数小时内）

触发输入必须是 libaoxun 主动交付的固定 SHA/批次证据，且同批已经实际通过：

```text
USER2 -> RAM Hello -> UART1 三行输出/单字节回显 -> APB probe
```

执行顺序：

1. qzs 只读核对固定硬件 SHA、batch、bitstream、`soc.h`、linker、Hello ELF 与日志 hash；
2. wsc 用同批生成 BSP/linker 构建 `QW-F1-BOARD-SELFTEST-v1` 新 ELF；
3. 新 ELF 作为独立固件批次审查，不能继承 Hello ELF 的板级 PASS；
4. 在无机械臂窗口经 USER2 加载，qzs 捕获/校验唯一 summary；
5. 允许状态仅为 `BOARD_CPU_F1_SELFTEST=PASS`，其余仍 `NOT_VERIFIED`。

### B1：feature snapshot/ACK

必须另开 H1 Review Packet，由 libaoxun 负责 RTL/SoC 原子硬件修改：tap 只读接通、
单槽快照、same-frame ACK、overrun、multi-bit CDC、真实 BSP 地址。wsc 仅在已生成
ABI 上实现 `sc_runtime_transport_t` backend；qzs 用同一 qzs batch 的期望特征做
same-batch FPGA snapshot 对照。未收到冻结接口口令不得开始 ABI 文件修改。

### B2：result/OSD 与输入

先固定结果 staging/commit/round_id 和 OSD，再接配置/事件；每一级独立执行
Map/PNR/STA/CDC/warning/HDMI/板级证据。始终 `ARM_ENABLED=0`，UART2/J52 不进入范围。

## 6. libaoxun 隔离规则

- 本合同发布与两人开发不读取、不切换、不合并 libaoxun 活动工作区。
- 不向其活动分支推送任何提交，不复用其未固定中间产物。
- UART1 未 PASS 前，qzs/wsc 只准备 B0/B1 输入，不要求 libaoxun执行新构建。
- UART1 PASS 后也只消费其主动给出的完整 SHA 和证据包；三方集成另建工作区。
- 任何 H1 硬件 diff 仍由 libaoxun 实施，qzs/wsc 不越权代写。

## 7. 版本与变更规则

上述两个 `QW-*` JSON 合同是新增的双人文件交换格式，不是 FPGA wire ABI。
若只增 optional 字段，升级 minor；删除/改义/改单位必须升级 major 并同步修改
qzs schema、wsc parser、正负例与本文件。任何需要触碰冻结接口的变更必须停下，
走完整口令和 Review Packet。
