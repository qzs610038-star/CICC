# WSC P0-A / P1 Host 最终固定 SHA 验收

> 验收对象：`codex/wsc-p1-evidence-packaging-20260719@aaf2058e7e05b8cda905b8096db309c65e0da5ef`
>
> 已接受实现 SHA：`2564f146fede84383be0ff607aa63408b2889f37`
>
> 前序 qzs 裁定：`df247dc8917c2293897d0ba8131134609c8ae9b3`
>
> 最终裁定：`APPROVE / P0-A-READY / P1-HOST-READY / SOURCE READY FOR INTEGRATION REVIEW`

## 1. 固定范围

- `git ls-remote` 实读远端为 `aaf2058e7e05b8cda905b8096db309c65e0da5ef`；前序固定 SHA `a74b21d19e9a81d315456e106e9d23cc5402243a` 是其祖先。
- 本轮只有 1 个 evidence-packaging 提交、7 个 PowerShell/manifest/命令卡文件。
- wsc scope PASS；相对 `a74b21d` 的 C/H、冻结接口、integration、RTL、XML/peri.xml、SDC、IP 差异为空；`git diff --check` PASS。
- 未重跑 P0 双构建，未执行 USER2、上板、P0-B、UART2/J52 或机械臂操作。

## 2. qzs clean-checkout 复验

在未运行任何生成器的 clean detached checkout 上：

```text
P1_MANIFEST_HASHES=PASS files=4 policy=LF_NORMALIZED
P1_MANIFEST_TAMPER_NEGATIVE=PASS file=rounds.jsonl expected_exit=1
```

随后在独立输出目录 fresh 运行 P1 bundle：

```text
P1_VECTOR_SCHEMA=PASS count=11
P1_REPLAY_SCHEMA=PASS rounds=20 ARM=0
P1_TAMPER_NEGATIVE=PASS hash_mismatch=1 result_mismatch=1
P1_REPLAY_BUNDLE=PASS rounds=20 ARM=0 status=AWAITING_QZS_REVIEW
P1_MANIFEST_HASHES=PASS files=4 policy=LF_NORMALIZED
```

fresh 与 committed manifest 的四项规范化 hash 全部相同：

| 文件 | SHA-256 |
|---|---|
| `rounds.jsonl` | `2D32393E9F20B7B01137DCFCADC3BD93AF152A77674F4242B9ABC3CDA56A6BC1` |
| `runner.txt` | `8C4CCF1071B134326445B9BDC3193BC5A19787E442243147FB9DE1CF7D66E73F` |
| `compile.txt` | `DFABC3264C3062CB6A3019BE66BECBE030E35EA67E3CA1F4CCF5B6232F520065` |
| `tamper.jsonl` | `0C830B89CBBDBCB21A7FFA75D7BFEC404C9A01A69BA0A15FF07D6E825F8F5CA9` |

R1 命令卡已删除无效 label 参数；`compare_reproducibility.ps1` 现在自行产生稳定的 `fresh_detached_a/b` 标签、验证输出 round-trip，不需要事后手改。

## 3. 最终状态边界

前序 P0-A 与 P1 功能测试证据继续有效，本轮 evidence-only 差分没有改变任何 C 输入。最终签发：

```text
WSC_FIXED_SHA=aaf2058e7e05b8cda905b8096db309c65e0da5ef
P0_A_READY=YES
P1_HOST_READY=YES
SOURCE_READY_FOR_INTEGRATION_REVIEW=YES
MERGE_PERFORMED=NO
BOARD_VERIFIED=NO
CPU_EXECUTION_PROVEN=NO
USER2_BOARD_GATE=READY_FOR_SEPARATE_SCHEDULING_NOT_RUN
P0_B=HOLD
ARM_ENABLED=0
```

`P1-HOST-READY` 只表示 Host 模型、协议负例、四任务 20 轮 runtime replay、结构化证据与 clean-checkout hash Gate 通过；不等于 RISC-V 业务 ELF、真实 MMIO/APB/CDC/OSD、同批 FPGA snapshot、板级 CPU 执行或机械臂闭环完成。Task 2 非正方体仍为 provisional，Task 3/4 仍为 size unavailable；这些限制已被如实编码为 WAIT/BLOCKED，而不是伪装成功。

下一步是固定 SHA 的跨成员集成审查；USER2/板级 canary 是独立 Gate，本验收不授权执行。P0-B 必须继续 HOLD，直到 USER2 满足既定失败触发条件并另获批准。libaoxun 不需要为本验收拉取、审阅或暂停其工作。
