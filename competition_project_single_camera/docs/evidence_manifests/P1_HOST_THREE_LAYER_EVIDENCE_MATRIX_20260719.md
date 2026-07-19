# P1 Host 到 RTL TB 到板级的三层证据矩阵

> 状态：`P1-HOST-READY / HOST ONLY / WIRE ABI NOT FROZEN / BOARD NOT VERIFIED`
>
> 固定基线：`0e5ab490559c58642734b0095753c6cf8787c709`。本矩阵不定义 APB 地址、PSTRB、IRQ、CDC wire ABI、按键管脚或 OSD 寄存器布局。

| 不变量 | Host 证据（本轮 wsc） | RTL TB 复用证据（未来 H1） | 板级证据（未来） | 当前状态 |
|---|---|---|---|---|
| `P1-I1-01` | 读前后 frame_id 一致才消费 | 单槽锁存不撕裂向量 | APB/CDC 原始快照 | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I1-02` | 只 ACK 已消费的同帧 | 错 frame ACK 拒绝向量 | ACK 编码与读回 | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I1-03` | torn/config/diag/overflow/overrun/non-ch0 fail-closed | flags/overrun 负例 | 同批 feature snapshot | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I1-04` | idle-drain 只 release、无第二结果 | 终态释放 TB | 业务 ACK/flush 实装 | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I1-05` | 16-bit frame/config 回绕不复用旧结果 | 回绕向量 | 长时间板级运行 | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I2-01` | staging 必须 commit 后候选 active | frame-boundary active | VSYNC/CDC 行为 | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I3-01` | event_seq 重复/乱序/抖动/长按不重复开轮 | 输入锁存 TB | 管脚、极性、消抖 | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I3-02` | APPLY/PLACE/REMOVE/ABANDON/RESET 的 ACK/复位边界 | 事件 TB | 真实输入来源 | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I4-01` | 完整 staging 后新 round_id 原子提交 | result latch TB | result/OSD ABI | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-I4-02` | 结果语义可解释，固定 `ARM=0` | 固定结果显示表 | 像素渲染/HDMI 回归 | `HOST PASS / FUTURE LAYERS NOT RUN` |
| `P1-SAFE-01` | 目标只能为 `EXECUTE_ARM_DISABLED` | 安全负例 | 不证明 UART2/myCobot | `HOST PASS / FUTURE LAYERS NOT RUN` |

## 交换与采集工件

- JSONL 每行遵循 [p1_feature_vector.schema.json](p1_feature_vector.schema.json)，样例见 [p1_feature_vector.example.jsonl](p1_feature_vector.example.jsonl)。它是语义向量，不是寄存器镜像。
- 采集 CSV 的唯一首行模板为 [p1_calibration_capture_template.csv](../calibration/p1_calibration_capture_template.csv)。`evidence_level` 只能是 `HOST_CALIBRATION_PROVISIONAL` 或 `SAME_BATCH_FPGA_SNAPSHOT`；未取得同批原始快照前不得升级。
- 20 轮 Host bundle 按 [p1_20_round_bundle_manifest.template.json](p1_20_round_bundle_manifest.template.json) 提供；每轮保留输入、快照 hash、ACK、结果、提交次数、耗时与 `ARM_ENABLED=0`。

## OSD 与输入状态表

| 状态/事件 | 最小显示或记录 | 保持/拒绝规则 | 安全语义 |
|---|---|---|---|
| `WAIT` | `ROUND/FRAME/CONFIG`、原因、`ARM=0` | 到新有效结果 | 不动作 |
| `EXECUTE_ARM_DISABLED` | `TARGET`、理由、`ARM=0` | 到 REMOVE/下一轮确认 | 目标命中不授权机械臂 |
| `SKIP` | `NON_TARGET` 与具体 reason | 到 REMOVE/下一轮确认 | 不动作 |
| 输入拒绝 | `INPUT_FLAGS` 与拒绝原因 | 等待稳定快照/有效事件 | 不伪装识别结果 |
| `RESET` | 清旧 round/result，显示 `ARM=0` | 新一轮开始 | 不改变机械臂 Gate |
| `APPLY/PLACE/REMOVE/ABANDON` | event_seq 与 ACK | 重复、乱序、抖动、长按拒绝重复消费 | 不定义按键/寄存器 ABI |

`P1-HOST-READY` 只能在严格 Host 编译、全部协议负例、20 轮 replay、如实的标定状态和本表都完整时评定；不等于 RISC-V、APB、OSD、CDC 或板级 PASS。

## 2026-07-19 WSC 首批候选状态

`dbdbc9b84ce43e4c58a112678eadbb18ea4ef70d` 的 fresh Host 计数通过，但 JSONL 不符合本目录 schema，静态 20 行 replay 未携带本表要求的逐轮 snapshot/ACK/commit/elapsed/release 证据，reserved bit 向量还与真实 adapter 行为相反。因此本表整体状态为 `PARTIAL / CHANGES_REQUESTED`，不是 `P1-HOST-READY`。详见 `docs/review_packets/WSC_P0_A_P1_HOST_CANDIDATE_AUDIT_20260719.md`。

## 2026-07-19 WSC 整改候选状态

`328f115c1716023d2c10c4276c80cd2833291551` 已 fresh 通过 P1 model `37/37`、adapter `39/39`、classifier `54/54`、F1 `213/213`、runtime `648/648` 与 schema `11/11`；ACK、frame-boundary、result latch 和 reserved bit 阻断关闭。但 20 轮 runner 把 decision/reason、snapshot hash 与 commit count 写在预置表中，只执行输入事务模型，没有从 fake snapshot 驱动 runtime/参考模型得到 actual 结果。因此本表仍为 `PARTIAL / CHANGES_REQUESTED`，不是 `P1-HOST-READY`。详见 `docs/review_packets/WSC_P0_A_P1_HOST_REMEDIATION_AUDIT_20260719.md`。

## 2026-07-19 WSC R1/R2 快速复审状态

`a74b21d19e9a81d315456e106e9d23cc5402243a` 已关闭 runtime replay 的预置输出问题：20 轮从序列化 snapshot 经 runtime/fake transport/packer 产生 actual 结果，任务 `5+5+5+5`、submit/commit/ACK 均为 1、second result=0、`ARM=0`，篡改负例 PASS。剩余问题仅为 committed bundle 的 EOL/hash 封装：clean checkout 中三份文本 hash 与 manifest 不符，且当前 validator 未回验 manifest 文件 hash。因此本表仍为 `PARTIAL / CHANGES_REQUESTED`，修复并在 clean checkout fail-closed PASS 后即可评定 `P1-HOST-READY`。

## 2026-07-20 WSC 最终 Host 状态

`aaf2058e7e05b8cda905b8096db309c65e0da5ef` 的 clean-checkout manifest verifier、单文件 tamper、fresh bundle 与 committed-vs-fresh 四项 hash 全部通过，且相对已接受实现没有 C/H 差分。因此本表 Host 层正式评定为 `P1-HOST-READY`。RTL TB、真实 RISC-V/MMIO/APB/CDC/OSD、同批 FPGA snapshot 与板级层仍全部 `NOT RUN / NOT VERIFIED`，不得从 Host READY 外推。
