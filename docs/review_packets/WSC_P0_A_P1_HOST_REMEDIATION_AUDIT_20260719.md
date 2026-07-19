# WSC P0-A / P1 Host 整改固定 SHA 复审

> 复审对象：`codex/wsc-p0a-p1-host-20260719@328f115c1716023d2c10c4276c80cd2833291551`
>
> 固定实现 SHA：`6e9475ff6678c289160b3374cd98b6fda88c8455`
>
> 治理输入 SHA：`f027c6ff7b087e4dff5c23b5d76edd8286172f1f`
>
> qzs 裁定：`CHANGES_REQUESTED / DO NOT MERGE / P0-A IMPLEMENTATION ACCEPTED / P0-A READY NO / P1-HOST READY NO`

## 1. 固定来源、范围与安全边界

- `git ls-remote` 实读远端分支为 `328f115c1716023d2c10c4276c80cd2833291551`；首次候选 `dbdbc9b84ce43e4c58a112678eadbb18ea4ef70d` 是其祖先，其后共有 5 个整改提交。
- `6e9475ff6678c289160b3374cd98b6fda88c8455..328f115c1716023d2c10c4276c80cd2833291551` 只有结构化证据文件，没有新的实现输入变化。
- `team_scope_check -Role wsc`：`PASS`，37 个 ACMR 文件均位于 wsc 授权范围；冻结 `cpu/include/**`、`integration/**`、RTL、XML/peri.xml、SDC、IP 零差异。
- `git diff --check` PASS；相对 kickoff 没有新增 ELF/map/log/bit/bin 制品。
- 未执行 USER2、上板、P0-B、UART2/J52 或机械臂操作；`ARM=0`、`BOARD NOT VERIFIED` 保持。

## 2. qzs 独立 fresh 复验

| 检查 | fresh 结果 | 结论边界 |
|---|---|---|
| P0-A Host TX-never-ready | `10/10 PASS` | 有界 TX、E101 与 heartbeat Host witness 通过 |
| P0-A RISC-V 严格构建 | exit 0；`2976/16384 bytes` | 固件可构建，不证明板上执行 |
| qzs P0-A verifier | `P0_A_EVIDENCE=PASS`；entry `0xF9000000`；canary `0xF9000370` | manifest 主链通过，`board_status=NOT_VERIFIED` |
| 两个独立工作树 fresh 构建 | ELF/map/readelf/objdump/build log/TX witness/manifest 均逐项同 hash | 实际可复现身份策略成立 |
| P1 Host model / adapter | `37/37`、`39/39` | ACK、pending/frame-boundary、result latch 与 reserved bit 单元层通过 |
| classifier / F1 / runtime | `54/54`、`213/213`、`648/648` | 既有 Host/fake seam 无回归 |
| P1 schema / runner 外形 | `11/11`、`20/20`、`ARM=0` | 只证明 schema 与 runner 自身断言通过，不证明四任务结果由快照执行得出 |

两次独立 P0-A fresh 构建的关键 hash 完全一致：

```text
diagnostic.elf     341D902E00A44B099D6BDBC0872873153E37DDB80D22D04FAFADF3F65E490C3A
diagnostic-map.txt 69443007F8DAAC349E123785777F280CF2F84ACB72564861D69D582AB762B6E3
readelf-lW.txt     9EF233C951E93079E332F1450B5E81E755A21E6EB72F8F825B11882DDCE78740
objdump-d.txt      4986D71AE5C2C50C385E33D0C7F8BF45C28762F23A7EDD155C1C792CC2135EEF
strict-build.txt   19FCB034DE11BAEED91F2C6C6C7B1223DAE5ECCBFD195AEB202C81E2D2A16731
```

## 3. 已关闭的首次阻断项

- `P0-A-1`：源码和 objdump 均满足 `C003 < UART config < C004 < C005 < first TX poll`。
- `P0-A-2`：规范化输入 SHA 为 `14D5F966...C89CA5C`，build_id 按确定性规则派生为 `0x14D5F966`。
- `P0-A-3`：`p0-a-evidence-v1`、TX witness、stop/safety 字段齐全，qzs verifier fresh PASS。
- `P0-A-4` 实现与制品部分：已移除入库 ELF/map/log，strip-debug 身份 ELF 在两个独立工作树一致。
- `P1-1`：11 条 JSONL 均通过 qzs schema、唯一 ID、边界和 reserved-bit 检查。
- `P1-3`：`0xC7` reserved bit 已 fail-closed，并有 Host 回归。
- `P1-4`：ACK-on-success、pending 到 frame-boundary active、result latch/release 生命周期已实现并测试。

上述项目下一轮不要求重写；只需保持回归通过。

## 4. 剩余阻断项

### R1：P0-A reproducibility 记录仍含旧 objdump hash

提交中的 `evidence/manifest.json` 与 qzs 两个 fresh 工作树都给出 objdump SHA-256 `4986D71A...35EEF`，但提交中的 `evidence/reproducibility.json` 第 12 行仍写 `8820B8D1...056F30`，同时第 13 行声明 `all_compared_fields_match=true`。这使同一 evidence bundle 内部自相矛盾；当前 verifier 没有消费该旁路文件，因此 verifier PASS 不能消除矛盾。

最小修复：从两次 fresh 结果自动重生成 `reproducibility.json`，使每个 hash 与 manifest/实际文件一致；增加一个 fail-closed 校验，禁止手写 `all_compared_fields_match=true`。不需要修改 P0-A 固件实现。

### R2：P1 20 轮 runner 仍是预置输出，不是四任务执行回放

`p1_replay_runner.c` 第 15-35 行在 `cases[]` 中直接写死每轮 `decision`、`reason`、`release` 与 `commit_count`；第 68-73 行写死四个 snapshot hash；第 96-107 行把这些常量直接打印进 JSONL。runner 只实际驱动了 `p1_host_model` 的 APPLY/PLACE/latch/release 生命周期，没有构造或序列化 fake snapshot，也没有调用 classifier/F1/runtime/参考模型来计算 decision/reason，snapshot hash 也没有绑定任何真实输入字节。

此外，`TIMEOUT` 分支实际调用 `P1_INPUT_ABANDON`，输出却只记录 `terminal_release=TIMEOUT`；若这是设计映射，记录必须同时保留“外部终止原因”和“实际消费的 Host event”，不能把两者伪装成同一事务。

因此当前 `P1_REPLAY_BUNDLE=PASS` 只能证明 runner 自身预置表与事务模型通过，尚未关闭首次审查要求的“同一个 runner 驱动 fake input + fake snapshot + runtime/参考模型”。

最小修复：

1. 每轮从真实序列化的 fake snapshot/input 计算 `snapshot_hash`，不得从常量表取 hash。
2. decision/reason 必须由被测 runtime 或独立参考模型返回，再与 expected 断言比较；禁止直接把 expected 打印为 actual。
3. `result_commit_count`、second-result 与 ACK 必须由实际调用计数产生。
4. 对 TIMEOUT 同时记录 terminal cause 与实际 Host release event；保留 ABANDON、RESET、duplicate/stale/invalid ACK 负例。
5. manifest 继续绑定 runner、输入向量、输出、编译器与治理 SHA，并增加篡改一个 snapshot 后 hash/结果校验必失败的负例。

## 5. 裁定与下一 Gate

```text
WSC_FIXED_SHA=328f115c1716023d2c10c4276c80cd2833291551
IMPLEMENTATION_SHA=6e9475ff6678c289160b3374cd98b6fda88c8455
VERDICT=CHANGES_REQUESTED
MERGE=NO
P0_A_IMPLEMENTATION=ACCEPTED
P0_A_READY=NO
P1_HOST_READY=NO
BOARD_VERIFIED=NO
USER2_BOARD_GATE=NOT_OPENED
P0_B=HOLD
ARM_ENABLED=0
```

wsc 下一轮只需关闭 R1、R2 并保持既有回归，不得修改冻结接口、RTL/Hard SoC，也不得执行 USER2、上板、P0-B、UART2/J52 或机械臂操作。qzs 收到新固定 SHA 后只复核这两个差分；libaoxun 不接收本轮任务、不需要拉取或审阅。
