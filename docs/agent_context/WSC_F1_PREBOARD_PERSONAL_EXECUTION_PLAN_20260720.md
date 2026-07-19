# wsc F1 Preboard 个人执行方案

> 负责人：wsc
>
> 目标分支：`codex/wsc-f1-preboard-runtime-calibration-20260720`
>
> 状态上限：`WSC_PREBOARD_CORE_READY`；不得宣称板级 PASS

## 一、目标与交付边界

wsc 负责把已有 Host runtime 变成可快速绑定板卡的业务核心：小体积 F1 自检、
RISC-V freestanding profile、真实特征 batch 回放、任务二阈值和尺寸 fail-closed
标定。当前不实现真实 MMIO backend，不触碰 libaoxun 的 RTL/SoC/BSP，不接机械臂。

允许写入：`competition_project_single_camera/cpu/src/**`、`cpu/tests/**`、
`cpu/README.md`、既有允许的 `cpu_bringup/uart1_hello_onchip/**`。本轮不修改
`cpu/include/**`；如确需公共类型或冻结接口变更，先停下提交 Review Packet，等待
临时所有权和完整冻结口令。

## 二、任务分解

| 步骤 | 内容 | 依赖 | 预估有效工时 | 环境/空间 | 完成判据 |
|---|---|---|---:|---|---|
| W0 | 精确拉取发布 SHA，建独立工作区/个人分支 | 计划已推送 | 0.5h | Git/PowerShell；<100MB | HEAD 精确、worktree clean、scope 基线记录 |
| W1 | 实现无寄存器知识的 `QW-F1-BOARD-SELFTEST-v1` 核心和 Host 正负例 | W0 | 3h | MSVC/Clang；<200MB | 固定 cases/result digest、ARM=0、无堆/无 MMIO/无重复结果 |
| W2 | 建 RISC-V freestanding profile、map/readelf/objdump/16KiB budget 检查 | W1 | 3h | RISC-V GCC/QEMU 可选；<500MB | profile ELF 可复现、无未解析平台符号、预算 PASS |
| W3 | 消费 qzs batch，建立真实特征 -> snapshot -> runtime -> result 回放 | qzs Q4；W1 | 3h | C/PowerShell/Python reader；<300MB | 输入 hash 绑定、逐样本结果、tamper fail-closed |
| W4 | 数据驱动任务二阈值和尺寸估计模块 | qzs Q4；W3 | 4h | Host C；<300MB | object-level holdout 指标、歧义区 WAIT、size=0 fail-closed |
| W5 | 准备 UART1 PASS 后的同批 BSP/linker 绑定说明与 B0 固件包模板 | W1,W2 | 1.5h | 文档写入仅限 CPU README/tests evidence | 不含地址；明确新 ELF 独立 hash/Gate |
| W6 | 全量 fresh 回归、固定 SHA、交给 qzs | W1-W5 | 1.5h | 既有 Host runner | scope/diff/tests/evidence/tamper 全 PASS |

## 三、具体实施要求

### W0：一次精确拉取

只 fetch 本双人发布 ref，先用 `git ls-remote` 获取权威 SHA；Windows 遇到大小写
ref 冲突时禁止全量 fetch。若目标 worktree/branch 已存在，检查后继续，不 reset、
clean、stash 或覆盖现有修改。

### W1：板上 F1 自检核心

建议在 `cpu/src/**` 放平台无关核心，在 `cpu/tests/f1_board_selftest/**` 放固定向量、
Host runner 和未来 BSP 适配模板。约束：

- 复用现有 classifier/feature adapter/runtime 语义，不复制第二套业务判定；
- 不直接 include `soc.h`，不写 UART/APB 寄存器，不猜地址；
- 事件输出通过 callback，Host 用内存 sink，未来板上用 UART1 sink；
- 不使用 malloc；避免 `snprintf`/大 libc 依赖进入 freestanding 核心；
- 固定向量包含四任务正常/等待/超时/重复/旧帧/flags 拒绝，结果 digest 确定；
- `arm_enabled=0`、`arm_request_count=0`、`arm_send_count=0` 是硬断言；
- B0 只需代表性 cases，不重复替代既有 648/648 与完整 20 轮证据。

### W2：RISC-V profile

在测试目录提供可复现脚本，自动探测工具链。若工具链不可用，Host 工作继续并
把 W2 标为 `TOOLCHAIN_BLOCKED`，不得伪造输出。profile 必须生成本地忽略的 ELF、
map、readelf、objdump 和 JSON identity，检查：

- 入口/LOAD 段/栈/静态数据范围；
- 代码+数据+预留栈不超过同批 16 KiB 约束；
- 不引用 MMIO/UART 地址，不含机械臂协议；
- canary/自检摘要写入发生在任何平台 sink 失败之前；
- 两个 clean/temp build 的输入和结构化输出 hash 一致。

此 profile 不是板卡最终 ELF；UART1 PASS 后必须用 libaoxun 固定批次的真实 BSP/linker
重新构建并生成新的固件 identity。

### W3：真实特征回放

只接受接口合同定义的 qzs 固定 batch。parser 必须验证 qzs SHA 和全部输入 hash，
再逐行构造现有 `sc_feature_snapshot_t`，通过 fake transport 驱动真实 runtime；禁止
直接调用分类器后手写期望结果绕过 ACK/结果提交。输出每条 sample 的 snapshot、
observation、decision、reason、ACK、result count 和输入 hash。

### W4：任务二与尺寸

- 任务二优先利用现有 `sc_classifier_cfg_t` 的整数阈值，不改公共头；
- 用 object-level holdout 选择阈值，分别报告 3 类混淆矩阵与 cube/non-cube；
- 非正方体未达门时继续保持诊断/WAIT，不改为业务终态；
- 尺寸模块放 `cpu/src/**` 内部头/实现，输入只使用现有 bbox/foreground 字段；
- 比较可审计整数特征（如 bbox 短边、长边、面积代理），选取 holdout 最稳者；
- 2/2.5/3 cm 分布重叠区域必须返回 `0=SIZE_UNAVAILABLE`；不得最近邻强判；
- 不把 PC 图像像素直接喂给板上 CPU，正式运行仍只消费 FPGA 基础统计。

若必须增加公共配置字段或改变 `sc_features_t/sc_observation_t`，停止实现并提交接口
变更 Review Packet；没有完整口令不得修改 `cpu/include/**` 或冻结合同。

### W5：UART1 成功后的 B0 绑定

预先准备清单，不执行板卡操作：

1. 输入：libaoxun 固定 UART1 PASS SHA、batch、bitstream、同批 `soc.h`/linker/BSP hash；
2. 仅新增小 UART sink/启动适配，业务核心不改；
3. 产出新 selftest ELF/map/readelf/objdump/hash，作为独立固件批次；
4. 输出唯一 `@F1SELFTEST` summary，禁止三行 Hello 被误判为业务 PASS；
5. 经 qzs verifier 后只升级 `BOARD_CPU_F1_SELFTEST`，feature/APB/OSD 仍保持未验证。

### W6：回传包

回传个人分支、完整 SHA、seed SHA、文件范围、所有命令/exit code、evidence 目录、
qzs batch/hash、未验证项和 `ARM_ENABLED=0`。不直接 merge 双人种子分支。

## 四、依赖图

```text
W0 -> W1 -> W2 -----> W5 -> W6
       \-> qzs Q4 -> W3 -> W4 -> W6
```

W1/W2 不等待 qzs 数据；W3/W4 严格等待 qzs 固定 batch，不能使用聊天粘贴值。

## 五、验收命令框架

在实现脚本落地后，至少保留这些入口或等价命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File competition_project_single_camera/cpu/tests/run_f1_board_selftest_host.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File competition_project_single_camera/cpu/tests/run_f1_riscv_profile.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File competition_project_single_camera/cpu/tests/run_real_feature_replay.ps1 -Batch <qzs-batch>
powershell -NoProfile -ExecutionPolicy Bypass -File competition_project_single_camera/cpu/tests/run_g2_host_evidence.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/interface_freeze_check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/team_scope_check.ps1 -Role wsc -BaseRef <seed> -TargetRef HEAD
git diff --check <seed>..HEAD
```

负例至少覆盖 qzs input hash 篡改、重复 sample、错误 flags、旧 frame、ACK 失败、
结果重复提交、尺寸歧义、工具链缺失。所有生成 ELF/map/log 默认忽略，不提交二进制。

## 六、风险矩阵

| 风险 | 概率 | 影响 | 缓解/停止条件 |
|---|---|---|---|
| 自检核心引入 libc 导致超过 16KiB | 中 | B0 无法快速绑定 | callback/定长整数序列化；map budget 门 |
| Host batch 与板上特征分布漂移 | 高 | 阈值上板失效 | 保持 provisional；B1 用 same-batch FPGA snapshot 再升级 |
| 尺寸分布重叠 | 中 | 任务三/四仍阻塞 | 歧义区返回 0；改善相机治具/ROI 后新 batch |
| 修改公共头触发冻结接口 | 中 | 合并阻断 | 内部模块优先；需要时停止并走口令/Packet |
| 误写真实 MMIO | 低 | 破坏原子硬件边界 | `single_camera_mmio_transport` 继续 fail-closed |
| 干扰 libaoxun UART 排障 | 低 | 主线延误 | 只消费未来固定 PASS 包，不进入活动树/中间 SHA |

## 七、wsc 完成口径

W1-W6 通过后可写：

```text
WSC_PREBOARD_CORE_READY=YES
RISC_V_F1_PROFILE_READY=YES
REAL_FEATURE_REPLAY_READY=YES
ARM_ENABLED=0
BOARD_VERIFIED=NO
```

任务二/尺寸按实际指标分别写 `PROVISIONAL_READY` 或 `BLOCKED`。不得因自检 ELF、
QEMU、Host replay 通过而宣称真实 feature/APB/OSD 可用。
