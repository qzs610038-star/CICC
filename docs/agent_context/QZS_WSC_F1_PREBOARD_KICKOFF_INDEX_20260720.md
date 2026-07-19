# qzs + wsc F1 Preboard 并行执行索引

> 日期：2026-07-20（Asia/Shanghai）
>
> 发布分支：`codex/qzs-wsc-p0a-p1-integration-20260720`
>
> 发布前基线：`c296442a811fffdf103625a3883eb36e16aeac0c`
>
> 状态：`EXECUTION PLAN READY / BOARD_VERIFIED=NO / P0_B=HOLD / ARM_ENABLED=0`

## 1. 本轮目标

在不依赖 `USER2 -> RAM Hello -> UART1 -> APB probe` 当前阻塞链的前提下，
把双人工作从已经完成的 `P0_A_READY + P1_HOST_READY` 推进到
`F1_PREBOARD_RC`：真实采样格式、Host 黄金特征、任务二标定、尺寸标定、
真实特征回放、RISC-V/F1 自检固件轮廓、H1 接板包都准备好。

本轮不重复制造已经通过的 `37/39/54/213`、`648/648`、20 轮合成证据；
这些测试只作为修改后的回归门。

## 2. 必读与执行入口

1. [qzs 个人执行方案](QZS_F1_PREBOARD_PERSONAL_EXECUTION_PLAN_20260720.md)
2. [wsc 个人执行方案](WSC_F1_PREBOARD_PERSONAL_EXECUTION_PLAN_20260720.md)
3. [双人接口与 UART1 成功后接板协议](QZS_WSC_F1_PREBOARD_INTERFACE_CONTRACT_20260720.md)
4. [qzs 新对话启动提示词](prompts/QZS_F1_PREBOARD_START_PROMPT_20260720.md)
5. [wsc 新对话启动提示词](prompts/WSC_F1_PREBOARD_START_PROMPT_20260720.md)
6. 既有冻结语义：
   `competition_project_single_camera/integration/single_camera_feature_contract.md`
7. 所有权与冻结门：
   `docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`

## 3. 分支模型

本发布分支只作为共同只读种子，不允许 qzs 与 wsc 同时在其上开发。

| 角色 | 个人分支 | 建议独立工作区 | 写入范围 |
|---|---|---|---|
| qzs | `codex/qzs-f1-preboard-data-evidence-20260720` | `D:\\CICC-qzs-f1-preboard-data-evidence-20260720` | `docs/**`、`competition_project_single_camera/docs/**`、`competition_project_single_camera/tools/**`、`tools/**` |
| wsc | `codex/wsc-f1-preboard-runtime-calibration-20260720` | `D:\\CICC-wsc-f1-preboard-runtime-calibration-20260720` | `competition_project_single_camera/cpu/src/**`、`cpu/tests/**`、`cpu/README.md`、既有允许的 `cpu_bringup/uart1_hello_onchip/**` |

两人都从本次最终发布 SHA 新建个人分支。不得从旧个人分支继续叠加，
不得直接推送本双人种子分支。最终由 qzs 固定两边完整 SHA 后建立新的
`codex/qzs-wsc-f1-preboard-integration-20260720` 集成分支；不得改正式 `main`。

## 4. 并行依赖图

```text
共同种子 SHA
  |-- qzs Q1 数据契约 -> Q2 采集/黄金特征 -> Q3 数据包/证据 -> Q4 H1 Packet --|
  |                                                                               |-> 固定 SHA 交叉审查 -> F1_PREBOARD_RC
  |-- wsc W1 自检核心/RISC-V 轮廓 -> W2 真实特征回放 ------------------------------|
                               \-> W3 任务二/尺寸标定（消费 Q2 数据） -------------|
```

qzs 的契约骨架与 wsc 的自检核心可立即并行；wsc 的真实标定参数只能消费 qzs
通过 `QW-CALIBRATION-SAMPLE-v1` 发布的固定 batch，不得使用聊天中手抄数据。

## 5. 共同红线

- 不切换、不 fetch 到、不 merge 到、不写入 libaoxun 的活动分支或工作区。
- 不联系 libaoxun 要求暂停、拉取、审阅或改 RTL；只在其主动形成 UART1 固定
  PASS SHA 后消费交接包。
- 不改 `src/top.v`、RTL、XML/peri.xml、SDC、IP、wrapper、生成 BSP、`soc.h`、
  `embedded_sw/**` 或任何 bitstream 输入。
- 不修改冻结接口文件；如确需修改，先停下并等待用户发送完整口令：
  `确认接口文件修改，已经和wsc、libaoxun、qzs沟通。`
- 不猜 APB 地址、位宽、PSTRB、IRQ、CDC、复位或 OSD wire ABI。
- `ARM_ENABLED=0`；不连接 UART2/J52，不查询、不发 myCobot 帧、不执行动作。
- Host、交叉编译、QEMU 或 UART 自检结果都不能写成 feature/APB/OSD 板级通过。

## 6. 联合完成状态

只有两份个人方案的必做 Gate 都通过，且固定 SHA 交叉审查完成，才允许写：

```text
F1_PREBOARD_RC=YES
RISC_V_F1_PROFILE_READY=YES
REAL_FEATURE_REPLAY_READY=YES
P1_CALIBRATION_PROVISIONAL_READY=YES
H1_PACKET_READY=YES
BOARD_VERIFIED=NO
P0_B=HOLD
ARM_ENABLED=0
```

任何实拍数据不足、尺寸区间重叠或非正方体混淆不达门时，保留对应
`BLOCKED/PROVISIONAL`，但不阻塞其他已独立完成的 Preboard 交付。
