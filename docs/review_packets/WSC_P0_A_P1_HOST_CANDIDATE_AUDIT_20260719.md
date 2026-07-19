# WSC P0-A / P1 Host 固定 SHA 候选审查

> 审查对象：`codex/wsc-p0a-p1-host-20260719@dbdbc9b84ce43e4c58a112678eadbb18ea4ef70d`
>
> 实现提交：`c4b0756ba4cd1cca713c6a1276f377a1ec72e038`
>
> kickoff：`0e5ab490559c58642734b0095753c6cf8787c709`
>
> qzs 裁定：`CHANGES_REQUESTED / DO NOT MERGE / P0-A NOT READY / P1-HOST NOT READY`

## 1. 范围与安全边界

- 远端 ref 实读为 `dbdbc9b84ce43e4c58a112678eadbb18ea4ef70d`；该提交以 kickoff 为 merge-base，仅领先 2 个提交。
- `team_scope_check -Role wsc`：`PASS`，30 个文件全部位于授权范围。
- `cpu/include/**`、`integration/**`、RTL、XML/peri.xml、SDC、IP 零差异；`git diff --check` PASS。
- 没有 P0-B、板级动作、UART2/J52 或机械臂操作；`ARM=0` 与 `BOARD NOT VERIFIED` 标注保持。

## 2. qzs 独立复跑通过项

| 检查 | fresh 结果 | 允许结论 |
|---|---|---|
| P0-A TX never-ready Host | `7/7 PASS` | 有界轮询代码可在 Host 负例退出并继续 heartbeat |
| P0-A RISC-V 严格构建 | exit 0；`2912/16384 bytes` | 当前源码可生成 RISC-V ELF；不证明板上执行 |
| ELF 布局 | entry `0xF9000000`；LOAD 到 `0xF9000B60`；canary `0xF9000330..0xF9000358`；stack `0xF9000360..0xF9000B60` | 当前链接布局在 16 KiB RAM 内且 canary/stack 不重叠 |
| P1 Host model | `28/28 PASS` | 当前 packer/event 单元测试通过 |
| classifier / F1 / adapter / runtime | `54/54`、`213/213`、`33/33`、`648/648` | 既有 Host/fake seam 未回归 |
| 当前向量脚本 | `11` 条、静态 replay `20/20`、`ARM=0` | 只证明脚本的现有浅层字段检查通过 |

## 3. P0-A 阻断项

### P0-A-1 阶段码语义与批准方案不一致

批准方案要求：`C003=UART 初始化前`、`C004=配置写入后`、`C005=第一次 TX 尝试前`。当前 `main.c` 先调用 `uart1_init()` 再写 `C003`，并在第一次 TX 前写 `C004`；`C005` 只在 TX 成功后写入。阶段常量存在，但板级 canary 无法按批准表定位“初始化前/第一次 TX 前”。

修复要求：按批准语义重排阶段写入，并增加 Host/objdump witness，明确 `C003 < UART config writes < C004 < C005 < first TX poll`。

### P0-A-2 `build_id` 未绑定固件输入 hash

`P0A_BUILD_ID` 固定为 `0x0719A001`，不是固件输入 hash 的短标识，无法用 RAM canary 区分同日不同固件。修复后 manifest 必须记录完整输入 SHA-256、短 build_id 的确定性派生规则与期望值。

### P0-A-3 正式 evidence manifest/负例 witness 缺失

候选没有 `p0-a-evidence-v1` manifest，也没有 qzs verifier 所需的 `tx-never-ready.json`、disassembly order offsets 和 stop/safety 字段，因此 `competition_project_single_camera/tools/p0_a_evidence_verifier.py` 无法对该 bundle 执行。控制台 `7/7` 不能替代结构化的 poll count、E101、timeout 前后 heartbeat 和 artifact hash 关系。

修复要求：按 qzs 模板生成 bundle-local manifest 与 JSON witness，在 clean worktree 对固定 SHA运行 verifier，要求 `P0_A_EVIDENCE=PASS`。

### P0-A-4 制品治理与可复现性不合格

- 候选把 `evidence/p0a_uart1_diag.elf` 提交进 Git，违反仓库“不得提交 ELF/bitstream/机器制品”的规则；合并前必须从提交历史中移除，只保留 hash、布局、脱敏文本与外部证据索引。
- qzs 在独立 worktree fresh 构建成功，但 ELF SHA-256 从提交声称的 `60D8...1860` 变为 `B000...00FE`；map/readelf 的 debug 字符串/section offset 也变化。当前构建包含工作树相关 debug 信息，尚不能从相同源码输入复现同一 artifact hash。

修复要求：用确定性 debug-prefix 映射、可复现 strip 规则或“loadable image hash + 明确非确定性 debug ELF”方案定版；不得继续把某一工作树的完整 ELF hash当跨机可复现身份。

## 4. P1-HOST 阻断项

### P1-1 向量不符合 qzs 已发布 schema

对 11 条 JSONL 逐条检查：`11/11` 缺 `evidence_level`；`classification` 不在允许 category 中；多个 input/result 向量也缺 schema 要求的核心字段。现有 validator 只检查数量，没有执行 schema 或 expected 语义。

修复要求：使全部向量通过 `p1_feature_vector.schema.json`；validator 必须 fail-closed 地校验 schema、case_id 唯一性、reserved bit、边界值和 expected 结果，而不是只检查条数。

### P1-2 静态 20 行 JSON 不等于四任务可执行回放

`twenty_round_arm0_replay.jsonl` 的 20 行都缺 qzs bundle 要求的 `snapshot_hash`、`ack_sequence`、`result_commit_count`、`elapsed_ms` 与 `terminal_release`。当前脚本只验证 4×5 行与 `ARM=0`。另一个 `raw.log` 虽含 20 轮 runtime 事件，却没有 task 字段，内容是颜色正方体 fake seam，未与四任务静态 JSON 逐轮绑定。

修复要求：用同一个可执行 runner 驱动 fake input + fake snapshot + runtime/参考模型，逐轮生成任务、event_seq、snapshot hash、ACK、decision/reason、commit 次数、耗时与 terminal release，并由 manifest 绑定 runner log/round records/vector/compiler hashes。必须覆盖 ABANDON、TIMEOUT、RESET 与无第二结果等负例。

### P1-3 reserved bit 向量与真实 adapter 行为相反

向量 `snapshot_bad_flags_001` 用 `source_flags=0xC7` 期待拒绝；冻结契约也要求 bit7 为 0。但当前 adapter 只要求 `0x47` 并拒绝 `0x38`，实际会接受 `0xC7`。`33/33` 没有覆盖 reserved bit，因此声明向量并未验证实现。

修复要求：在允许的 `cpu/src/**` 与 `cpu/tests/**` 内使 bit7 fail-closed，并增加 red/green 负例；不需要修改冻结头文件。

### P1-4 输入 ACK 与配置激活边界未关闭

`p1_input_event()` 在判定 duplicate/stale/invalid state 前就写 `*acked_event_seq=event_seq`；这与“只 ACK 成功消费事件”要求不一致或至少未定义。`APPLY` 还会立即设置 `config_active=1`，没有实现 `commit -> frame-boundary active` 的 Host 不变量。`result_latched` 只被清零，从未在模型中置位并参与完整生命周期。

修复要求：明确 ACK-on-success 规则并测试无效事件不产生新 ACK；增加 pending/commit/frame-boundary active 模型和结果锁存/REMOVE/RESET 生命周期，随后接入可执行 20 轮 runner。

## 5. 最终裁定与下一步

当前分支的实现方向和大部分安全边界正确，且 fresh 单元测试/严格构建通过；但它尚未满足批准方案的证据契约和可执行回放要求，并包含禁止入库的 ELF。因此：

```text
WSC_FIXED_SHA=dbdbc9b84ce43e4c58a112678eadbb18ea4ef70d
VERDICT=CHANGES_REQUESTED
P0_A_READY=NO
P1_HOST_READY=NO
MERGE=NO
P0_B=HOLD
USER2_BOARD_GATE=NOT_OPENED
```

wsc 应在同一个人分支追加修复提交，不重写 kickoff、不接触冻结接口/RTL/Hard SoC，也不联系或阻塞 libaoxun。qzs 收到新固定 SHA 后只复核上述差分并 fresh 重跑；通过后再将 USER2/板级 canary 读取列为未来独立 Gate。
