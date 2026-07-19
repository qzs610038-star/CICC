# WSC P0-A / P1 Host R1-R2 固定 SHA 快速复审

> 复审对象：`codex/wsc-p0a-p1-host-20260719@a74b21d19e9a81d315456e106e9d23cc5402243a`
>
> 实现 SHA：`2564f146fede84383be0ff607aa63408b2889f37`
>
> qzs 审计输入：`e282a95fcef306c87e4015385c9fcd0041f5a722`
>
> 裁定：`PARTIAL ACCEPT / P0-A-READY / P1-HOST CHANGES_REQUESTED / DO NOT MERGE`

> 后续状态：wsc 已在 `aaf2058e7e05b8cda905b8096db309c65e0da5ef` 关闭最后的 clean-checkout hash 封装问题，qzs 已签发 `P1-HOST-READY`。最终验收见 `WSC_P0_A_P1_HOST_FINAL_ACCEPTANCE_20260720.md`。

## 1. 固定范围

- 远端 ref 实读为 `a74b21d19e9a81d315456e106e9d23cc5402243a`，且 `328f115c1716023d2c10c4276c80cd2833291551` 是其祖先。
- 本轮只有 `2564f14` 实现提交和 `a74b21d` 证据提交，共 12 个 R1/R2 文件。
- wsc scope PASS；冻结 `cpu/include/**`、`integration/**`、RTL、XML/peri.xml、SDC、IP 零差异；`git diff --check` PASS。
- 未执行 USER2、上板、P0-B、UART2/J52 或机械臂操作，`ARM=0`。

## 2. R1 独立复验与裁定

qzs 在两个独立、clean、detached 的 `2564f146...` 工作树中并行 fresh 构建：

- 两次严格 RISC-V 构建均 exit 0，RAM `2976/16384 bytes`；
- 两次 qzs verifier 均 `P0_A_EVIDENCE=PASS`，entry `0xF9000000`，canary `0xF9000370`，`board_status=NOT_VERIFIED`；
- 用提交中的 `compare_reproducibility.ps1` 比较实际 manifest，得到 `P0_A_REPRODUCIBILITY=PASS compared=12`；
- ELF、map、readelf、objdump、build log、TX witness、内存布局、canary 与阶段顺序 12 项全部一致；
- 篡改 objdump 后比较器 exit 1，并给出 `P0A_REPRO_ARTIFACT_HASH`。

因此 R1 实质闭环，qzs 签发：

```text
P0_A_READY=YES
BOARD_VERIFIED=NO
CPU_EXECUTION_PROVEN=NO
```

这只表示 P0-A 固件与 Host/构建证据包 READY，不表示 USER2、UART1 终端或板上 CPU 执行已经通过。

## 3. R2 功能闭环

源码与 fresh runner 已确认：

- 每轮构造 `sc_feature_snapshot_t`，按 `p1-fake-snapshot-le-v1` 序列化为 33 字节并计算 SHA-256；
- actual decision/reason 来自 `single_camera_runtime`，submit/ACK 来自 fake transport 计数，commit 来自 packer 调用；
- 20 轮按任务 `5+5+5+5`，每轮 runtime submit=1、commit=1、feature ACK=1、second result=0、`ARM=0`；
- TIMEOUT 记录为 `terminal_cause=TIMEOUT`、`host_release_event=ABANDON`；
- strict MSVC C11 `/W4 /WX`、schema `11/11`、replay `20/20`、runner tamper negative 均 PASS；
- 外部篡改第 1 轮 `snapshot_bytes_hex` 后 validator exit 1，标记 `P1_SNAPSHOT_HASH_MISMATCH round=1`；
- P1 model `37/37`、adapter `39/39`、classifier `54/54`、F1 `213/213` fresh PASS。

因此先前 R2 的“预置输出”阻断已关闭。

## 4. 唯一剩余 P1 evidence-packaging 阻断

在未运行生成器的 clean checkout 中，提交的 manifest 与提交文件原始字节不一致：

| 文件 | manifest SHA-256 | clean checkout 实际 SHA-256 |
|---|---|---|
| `rounds.jsonl` | `2D32393E...56A6BC1` | `18E764A9...0519878` |
| `compile.txt` | `04FA4D3E...2E3801B` | `DFABC326...F520065` |
| `tamper.jsonl` | `0C830B89...F5CA9` | `2A081504...37F1D9` |

根因是 `run_p1_replay_bundle.ps1` 的 manifest 声明 `UTF-8 with CRLF normalized to LF`，但对这四个 bundle 输出仍直接使用 `Get-FileHash`；Git 签出换行转换后，三项 raw hash 失配。当前 validator 也不回验 manifest 的 `files_sha256`，所以 `P1_REPLAY_BUNDLE=PASS` 没有捕获该问题。

另有一个命令卡 WARN：`WSC_P0A_P1_REPRO_COMMANDS_20260719.txt` 给 `compare_reproducibility.ps1` 传入不存在的 `BundleLabelA/BundleLabelB`；原样执行 exit 1。qzs 去掉这两个参数后 R1 正常 PASS。提交的 `reproducibility.json` 使用 `label`，而当前脚本实际输出 `path`，需要同步为可由提交脚本原样再生的格式。

## 5. 最小修复与下一步

wsc 下一轮仅修证据封装，不改 P0/P1 C 实现：

1. 对 `rounds.jsonl`、`runner.txt`、`compile.txt`、`tamper.jsonl` 统一计算规范化 LF hash，或显式采用不会被 checkout 改写的逐文件策略；manifest 必须准确描述实际算法。
2. 增加 manifest file-hash verifier，并在 clean checkout 对已提交 bundle 执行；三项 mismatch 必须变成 PASS，篡改任一文件必须 exit 1。
3. 删除 R1 命令卡的两个无效参数，并让 committed `reproducibility.json` 可由 committed script 原样生成；不得事后手改 `path` 为 `label`。
4. 保持现有 R1/R2 功能回归；qzs 下一轮不再要求双 P0 构建，只跑命令卡、P1 bundle、clean-checkout manifest verifier 与 tamper。

```text
WSC_FIXED_SHA=a74b21d19e9a81d315456e106e9d23cc5402243a
VERDICT=PARTIAL_ACCEPT
P0_A_READY=YES
P1_RUNTIME_REPLAY=PASS
P1_HOST_READY=NO
MERGE=NO
BOARD_VERIFIED=NO
USER2_BOARD_GATE=READY_FOR_SEPARATE_SCHEDULING_NOT_RUN
P0_B=HOLD
ARM_ENABLED=0
```

libaoxun 不接收本轮任务、不需要拉取或审阅；本裁定不改变其 UART1/USER2 工作。
