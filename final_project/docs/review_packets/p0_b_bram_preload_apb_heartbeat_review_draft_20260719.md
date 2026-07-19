# P0-B BRAM 预加载与可见 APB Heartbeat Review Packet（准备稿）

> 状态：`PREPARED / HOLD / NOT APPROVED FOR IMPLEMENTATION, BUILD OR BOARD ACTION`
>
> 触发前提：`P0-A-READY`，且 USER2 选择链经过两次有新证据的修正仍不能可靠 halt/read；随后用户另行批准 P0-B。

## 2026-07-19 qzs checkpoint（保持 HOLD）

- kickoff 基线为 `0e5ab490559c58642734b0095753c6cf8787c709`。qzs 已准备不假设工具链或路径的 P0-A manifest template 与 verifier：`competition_project_single_camera/docs/evidence_manifests/p0_a_evidence_manifest.template.json`、`competition_project_single_camera/tools/p0_a_evidence_verifier.py`。
- verifier 只接受未来 wsc 提供并哈希绑定的 ELF/map/readelf/objdump/build log/TX-never-ready 负例；会检查 ELF entry/PT_LOAD、canary 符号、16 KiB RAM、stack/section 非重叠、有界 `E101` 及停止条件。模板不是 P0-A 证据。
- 本 checkpoint 可见 refs 中没有 kickoff 后的 wsc P0-A 实现 diff，故 P0-A 当前为 `BLOCKED / NO_SUBMISSION_VISIBLE`，USER2 的两次有新证据修正也未发生。所有触发条件均未满足，Packet 继续为 `HOLD`。
- `ARM_ENABLED=0`；没有构建、上板、冻结接口修改、UART2/J52 或机械臂操作。

## 任务目标与当前结论

当 P0-A 无法经 USER2 读取 PC/RAM canary 时，通过“同批 BRAM 预加载诊断固件 → CPU 周期写最小 APB heartbeat → FPGA 锁存 → 现有安全可见输出”证明 CPU 执行。当前仅完成审查输入准备；不授权修改 `APP_OVERWRITE`、固件、APB/RTL、IP、XML/peri.xml/SDC、bitstream 或板卡。

## 预期修改文件与关键 diff

实际路径必须由 libaoxun 在触发时依据 Efinity 真源填写，不能从历史工程猜测。预计原子集合至少包含：

- Hard SoC IP `settings.json` 中经 GUI/正式流程产生的应用预加载配置与路径；
- 同批 wrapper/BSP/`soc.h`/linker/固件镜像；
- 最小 heartbeat APB 从机及唯一顶层连接；
- 现有用户 LED 或固定 OSD 色块观察点；
- 完整 build manifest、输入与制品 hash、Map/Interface/PNR/STA/CDC/warning 摘要。

禁止新增顶层物理端口，禁止顺带接通 I1/I4，禁止恢复双摄/旧寄存器图，禁止直接修改生成 IP/wrapper/`soc.h`。

## 模块、信号、时钟、复位、CDC 和双通道影响

- 正式视频仍为 J48/ch0；heartbeat 不得回压 framebuffer、Debayer 或 HDMI。
- APB heartbeat 语义必须新建并审查，不能引用旧双摄地址。
- 可见输出优先使用已确认且不影响视频的现有 LED；若只能用 OSD 色块，必须证明其固定旁路不改变视频时序。
- 计数周期来自固件常量；至少用两个不同固件周期形成可区分证据，避免 FPGA 固定翻转冒充 CPU。
- 触发时必须填写 CPU/APB/像素各时钟与复位、CDC 方案、复位后起始值和 wrap 行为。

## 原子批次清单

`APP_OVERWRITE`、预加载路径、固件 ELF/hex、linker、BSP、APB/RTL、IP settings、wrapper、XML/peri.xml/SDC、bitstream 和所有构建报告必须属于同一 batch。任一输入变化都会使旧 bitstream、ELF、Map/PNR/STA/CDC、warning 和板级结果失效。

## 计划验证命令、日志与 warning

当前全部为 `NOT RUN`。触发后由 libaoxun 填写 Efinity 版本、器件/timing model、完整冷构建命令/GUI 步骤、exit code、资源、Setup/Hold、CDC 与 warning；qzs 运行 manifest/hash/freshness/diff/scope 检查。不得把 Map PASS 外推到 PNR/STA/bitstream/board。

板级判据必须同时满足：

1. 冷启动后可见输出持续按设计变化；
2. 只改变同批固件 heartbeat 周期并建立新制品后，输出周期相应改变；
3. 固件、bitstream、输入 manifest 与观察日志 hash 完整匹配；
4. 若 USER2 恢复，APB 读回仅作为第二证据，不替代前述区分实验。

## 未验证项、风险假设与回退条件

- `APP_OVERWRITE` 实际合法配置方式、预加载镜像格式与 Efinity 生成行为：`NOT VERIFIED`；
- 可用 LED/OSD 观察资源、极性、时钟、复位与管脚事实：`NOT VERIFIED`；
- heartbeat APB 地址、PSTRB、CDC、复位：`NOT DEFINED`；
- P0-A 与 USER2 实际结果：`NOT RUN`；
- 回退点：当前批准的 UART1 原子批次及其固定 hash；新批次失败不得覆盖或改写旧证据。

## 机械臂/外设状态与用户安全确认

`ARM_ENABLED=0`。UART2/J52 必须保持断开；不执行机械臂查询、发帧或动作。本 Packet 即使获批也只授权其列明的 CPU 生命证明原子批次，不授权 I1-I5 或其他板级扩展。

## 希望 Codex/用户届时裁定的问题

1. P0-A 是否已满足触发条件，且两次修正是否确有新证据？
2. 选择哪一个已确认安全的可见输出？
3. 原子差分、批次 ID、回滚制品与完整构建矩阵是否齐备？
4. 是否收到用户对本 Packet 固定差分的单独批准？若涉及冻结接口，是否收到完整口令？

上述四项关闭前，结论固定为 `HOLD`。
