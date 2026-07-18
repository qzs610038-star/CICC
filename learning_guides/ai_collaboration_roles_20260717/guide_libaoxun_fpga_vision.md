# 岗位学习指南：libaoxun——FPGA 视频前端与 Hard SoC 集成

> 生成日期：2026-07-17
> 面向角色：libaoxun / `@libaoxun688`
> 预计阅读时间：10–15 分钟
> 责任边界来源：[BRANCH_MERGE_GOVERNANCE.md](file:///D:/第十届集创赛-雄芯院材料/docs/merge_governance/BRANCH_MERGE_GOVERNANCE.md#L41-L67)

## 先理解你的“原子批次”责任

你的工作不是只让画面亮起来，而是维护一组必须一起成立的硬件事实：视频前端、顶层连接、Hard SoC/IP、XML、`.peri.xml`、SDC、BSP/`soc.h`、构建产物和板测证据。像修一座桥：换了桥墩，旧的承重报告不能自动继续有效。

当前 `competition_project_single_camera/` 的 G1 离线构建和匹配 hash 是当前事实，但 `USER2`、CPU 取指、UART0 Hello、APB 实读和 CPU→OSD 仍未验证。学习和排障时，必须先把“工程可生成”和“板上可运行”分开，见 [CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L16-L57)。

## 1. 你必须补齐的知识

### 第一优先级：视频流水线与时序

理解像素如何经过采集、RAW/RGB、Debayer/Gamma、ROI、统计、OSD 和显示；每一级要知道输入输出时序、数据宽度、valid/ready 或帧边界。不要只看“屏幕有没有图”，还要知道统计快照是否与同一帧、同一 ROI 对齐。

### 第二优先级：时钟、复位与 CDC

CDC 就像“隔洋通话”：两个时钟域不同，直接把脉冲线接过去可能漏采样或产生亚稳态。要会问：跨域的是脉冲、计数、总线还是快照？采用双锁存器、握手、异步 FIFO 还是锁存后提交？复位释放是否同步？

### 第三优先级：Hard SoC 与构建证据

掌握 Map、Interface、PNR、STA、CDC、bitstream、ELF 的区别；能读出 warning 分类、输入 SHA、工具版本、LOAD/entry、USER2 和板测结果。Map PASS 不能替代 PNR/STA/bitstream，更不能替代 CPU 取指和视频/串口现象；仓库红线见 [AGENTS.md](file:///D:/第十届集创赛-雄芯院材料/AGENTS.md#L32-L38)。

## 2. 你应该怎样使用 Agent

### 用法 A：先做“变量隔离表”

针对“纯 FPGA 视频正常，加入 CPU 后屏幕黑/烧录失败/无输出”这类问题，先要求 Agent 只读建立表格：

| 变量 | 当前证据 | 未知点 | 最小实验 |
|---|---|---|---|
| 视频裸链路 | 真实板测/日志 | 是否仍使用同一输入 | 复现纯 FPGA 基线 |
| CPU 时钟/复位 | XML/RTL/日志 | 是否取指 | 当前批次 UART0 Hello |
| SoC/IP/顶层 | 固定 SHA | 是否原子匹配 | 输入文件 hash 清单 |
| APB/快照 | 契约/RTL | 是否实读 | 固定 MAGIC/只读寄存器 |
| OSD/显示 | 画面/波形 | 是否被 CPU 支路影响 | 分离显示与 CPU 支路 |

不要让 Agent 一上来同时改时钟、地址、顶层和软件；那会失去因果关系。

### 用法 B：要求 Agent 维护“硬件原子清单”

```text
只读核对当前候选工程。
列出 XML、peri.xml、SDC、顶层、SoC/IP、wrapper、BSP、soc.h、ELF、bitstream 的路径、SHA、生成关系和证据批次。
任何缺失或无法匹配的项标为 BLOCKED，不要从历史批次拼接。
不要修改生成 IP、ipm、outflow 或历史制品。
```

### 用法 C：按最小 bring-up 阶梯推进

建议顺序：

1. 纯 FPGA 视频基线不变，确认 CPU 支路没有被误认为已验证。
2. 当前批次 CPU Hello：只验证 FPGA `USER2`、片上 RAM 和 UART0 `115200`。
3. 固定 APB MAGIC/只读寄存器：只证明 CPU→APB 的一条路径。
4. 单个冻结视频快照字段：先验证 `valid`、位置、面积或已定统计量。
5. 再接 CPU 分类、OSD 语义和后续系统链路。

这不是保守，而是给故障定位留下“单变量实验台”。

### 用法 D：用第二个 Agent 做“输入一致性审查”

```text
只读审查，不改硬件。
请比较当前 XML/peri.xml/SDC/顶层/BSP/soc.h/ELF/bitstream 是否属于同一批次。
检查时钟、复位、CDC、位宽、地址窗口、快照锁存和 OSD 语义是否一致。
把静态推断、历史证据和当前真实证据分开。
```

## 3. 你与队友的交付关系

交给 wsc：

- 真实存在且已定版的字段语义、单位、有效位、快照时刻、清除/提交行为；
- 同批次 `soc.h`、硬件窗口、BSP/ELF 依赖；
- 未定版地址和不可读取项清单。

交给 qzs：

- 工程输入集合、完整 commit/SHA、Efinity 版本、Map/PNR/STA/CDC/warning；
- bitstream/ELF hash、LOAD/entry、USER2 操作前置条件；
- 本批次仍未验证的板级项目和重新生成后的证据失效范围。

如果 XML、IP、顶层、约束、BSP ABI 或工具版本变化，要主动告诉全队：旧 bitstream、ELF、slack、CDC、warning 和板级记录不能继承。

## 4. FPGA/SoC 合并验收清单

- [ ] 纯视频基线和 CPU 集成实验有明确区分。
- [ ] Hard SoC、IP、XML、`.peri.xml`、SDC、顶层、BSP/`soc.h` 作为同一原子批次审查。
- [ ] 时钟、复位、CDC、双通道、位宽和帧/快照边界有记录。
- [ ] Map、Interface、PNR、STA、CDC、bitstream 分别报告。
- [ ] warning 按类别记录，没有用总数或一句“无影响”掩盖。
- [ ] bitstream/ELF hash、大小、LOAD/entry 和 USER2 目标可复核。
- [ ] 没有改生成 IP、`ipm/`、outflow、历史波形或本机临时制品。
- [ ] 缺少真实板测时写 `NOT VERIFIED`，没有把离线 PASS 升级为板级 PASS。

## 5. 推荐学习顺序

优先阅读：`AGENTS.md` 的 FPGA/SoC 红线、[CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L16-L25)、`docs/merge_governance/BRANCH_MERGE_GOVERNANCE.md` 第 3–5 节、[operations_runbook.md](file:///D:/第十届集创赛-雄芯院材料/docs/agent_context/operations_runbook.md#L1-L18)。然后阅读 [register_map.md](file:///D:/第十届集创赛-雄芯院材料/final_project/integration/register_map.md#L1-L70) 理解 CPU 所需的是“可解释字段”，不是裸寄存器值。

拓展基础：同步时序、CDC/FIFO、APB/AXI 基本握手、SDC 约束、FPGA PNR/STA、RISC-V 片上 RAM 启动路径。

你的学习验收标准是：能说明“画面正常但 CPU 仍可能未运行”的原因；能给 wsc 一份完整、可实现、不可猜测的字段契约；能给 qzs 一份可复核的原子构建和证据包。
