# QZS 最终静态集成 Review Packet — 2026-07-19

## 结论

`VERDICT=NOT_READY_FOR_NEW_WINDOW_REQUEST`

本 Packet 关闭了治理、EOL、所有权与静态集成核验的可重复部分；它**不**构成硬件
授权。两个静态目标通过，但 libaoxun 完整 verifier 所需的原始 `evidence/work/Efinity`
根目录不在本集合 checkout，故第三个静态目标不能在最终 HEAD 复跑。远端同名分支也仍为
`e72fb6a`，未回读到本次 qzs 提交。两项均关闭前，不得申请新硬件窗口。

## 固定来源、归属与计数纠正

| 来源角色 | 固定 SHA | 审查范围 | 结果 |
|---|---|---|---|
| libaoxun | `72cc281bd104726d9db1e88cb2894facb1d5fd1a` | UART1 Hard SoC 原子批次与 5 个 build-evidence 文件 | `TEAM_SCOPE=PASS`（`f47af29..72cc281`，20 项） |
| wsc | `13419d9922f3f8e7585bd43b77491b81b4bc0681` | CPU Host probe 的 runner 与 classifier 修复 | `TEAM_SCOPE=PASS`（`f47af29..13419d9`，2 项） |
| qzs | `018ced2`、`bb34856` 与本次治理补丁 | Gate、manifest、EOL、状态、handoff 与 Packet | 三个独立 qzs 片段均 `TEAM_SCOPE=PASS` |

旧 Packet 的“7 项例外”不是完整 ACMR 清单。重新冻结后，真实的跨所有者输入数为
**11**：libaoxun 的 `I0_UART1_BUILD_{EVIDENCE,INPUTS,MANIFEST}` 和两份 verifier/rebuild
脚本（5），WSC 的 `run_g2_host_evidence.ps1` 与
`test_single_camera_classifier.c`（2），以及 qzs 的 I0 operation card、interface-freeze
checker、team-scope checker、ownership policy（4）。它们被按来源 SHA 审查并被最终 manifest
只读引用，**不是** qzs 获得了 11 项临时源码写权限。

WSC probe 的 Git 来源保持为 `dev/wsc6090-uart1-cpu-20260719@13419d9`；本 Packet 没有把
WSC 源码复制到 libaoxun 路径，也没有改写其作者/提交归属。libaoxun 的 build manifest 同样
保持在 `embedded_sw/uart1_hello_onchip/**`，qzs 只拥有最终集成 manifest。

## EOL 与 hash 策略

- `.gitattributes` 已冻结 `*.ps1=CRLF`、`*.gdb=LF`、`*.cfg=LF`；docs、JSON 与 source
  延续仓库既有规则。
- 从干净 `8f0f6184cbba7ab7efcf8c649beceb1a6b90fea9` checkout 生成
  [`FINAL_STATIC_INTEGRATION_MANIFEST_20260719.json`](../../competition_project_single_camera/integration/FINAL_STATIC_INTEGRATION_MANIFEST_20260719.json)。
- manifest 逐项记录实际 checkout bytes 的 SHA-256 与 Git blob SHA-1；前者是执行身份，后者
  仅作 Git 可追溯性。manifest 自身 SHA-256 为
  `B2D402F77CDFB14C0841D164CF6B647F4763282FAE953B5FB67388521D6E55C4`。

## 最终静态检查

| 检查 | 结果 | 说明 |
|---|---|---|
| `git diff --check` | PASS | 无空白错误 |
| `team_scope_check` 三角色 | PASS | 使用 `-BaseRef/-TargetRef` 分来源审查；不再把集合树误归单角色 |
| interface freeze | PASS | `files=8`、`surfaces=1`、`route=UART1_TYPEC` |
| WSC probe verifier | PASS | G2 runtime `648/648`，classifier `54/54` |
| 最终 manifest | PASS | 12 个执行/门控输入、clean checkout、EOL 与双 hash 已绑定；独立 verifier 已在 `8f0f618` PASS |
| libaoxun 总 verifier | `NOT_RERUN_LOCAL` | 原始 evidence/work/Efinity roots 缺失；历史固定 SHA 记录为 `PASS inputs=82 artifacts=21`，不可提升为本机复跑 PASS |
| 危险路由扫描 | PASS_WITH_BOUNDARY | USER1 仅为既有 JTAG 定义；DDR 仍由视频状态机驱动；APB `PWDATA` 未接、`pwrite` 只进入只读 MAGIC 外设；未发现 Flash 路由 |
| qzs worktree / 远端 SHA | PENDING_COMMIT_AND_PUSH | 本 Packet 提交后需 worktree clean；远端当前回读为 `e72fb6a`，尚未等于本次最终 HEAD |

## Gate 状态（不得改写）

```text
USER2=NOT_VERIFIED
PC=NOT_VERIFIED
UART1_HELLO_ECHO=NOT_VERIFIED
APB_MAGIC=NOT_VERIFIED
I3=BLOCKED_CONTRACT_NOT_FROZEN
```

UART0、USER1、Flash、DDR 与 direct-APB 搜索只是静态风险扫描；它不授权 USER TAP、烧录、
Flash、DDR、UART、APB 写入或机械臂动作。

## 解除条件与后续裁定

1. 在 libaoxun 证据主机或带完整原始 roots 的 clean fixed-SHA worktree 复跑
   `verify_i0_uart1_build_evidence.ps1`，取得当前命令、root、exit 和 `82/21` 输出。
2. 提交本 Packet/manifest/状态，确认 qzs worktree clean，并将同名分支推送后用
   `git ls-remote --heads origin` 回读最终 SHA。
3. 上述三角色静态目标均 PASS 后，qzs 才可改为
   `VERDICT=READY_FOR_NEW_WINDOW_REQUEST`；该结论仍不等于硬件授权。实际 USER2 →
   UART1 Hello/echo → APB MAGIC 窗口必须由用户再次明确批准。
