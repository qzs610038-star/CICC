# 生成 SoC 摘要：预处理 APB 接入门禁

> 日期：2026-07-11
> 状态：阻塞。当前缺少 SoC 生成产物；本文是基于证据的缺口报告，不是寄存器地址来源。
> 范围：FPGA 视觉预处理的 CPU/APB 集成前置检查。

## 结论

当前工程不得把预处理 RTL 接入 CPU/APB、CPU 到 OSD 的回写链路，或由 CPU 控制的快照确认链路。所需的 Efinity SoC 生成产物不存在，因此无法确认 APB 基址、实际 APB 从机端口、时钟、复位或地址宽度。

这不影响已经完成的 ch1 像素域调试旁路；它只阻塞正式 CPU/APB 与 OSD 集成。

## 已核查证据

| 项目 | 结果 | 证据 |
|---|---|---|
| 生成的 `soc.h` | 未找到 | 在 `final_project/` 下进行了针对性文件搜索 |
| 既有 `generated_soc_summary_*.md` | 未找到 | 在 `final_project/` 下进行了针对性文件搜索 |
| 正式 `mem_test.xml` 中的 SoC IP | 不存在 | XML 只列出 CSI 和 DSI IP，没有生成 SoC IP 条目 |
| APB 从机 RTL / `results_cdc` RTL | 未找到 | 对 RTL 符号进行针对性搜索，并排除 vendor IP 的伪命中 |
| CPU BSP 地址来源 | 仅有占位值 | `cpu/app/include/bsp.h` 明确要求最终数值来自 `soc.h` |
| CPU 构建行为 | 正式构建要求 `SOC_H_PATH`；测试构建使用 `0xF0000000` 占位值 | `cpu/app/Makefile` |

## 现有文档仅可作为草案输入

`integration/register_map.md`、`docs/architecture/vision_register_handbook_draft_2026-07-03.md` 与 `cpu/app/include/board_io.h` 是有价值的接口草案，但它们不能确定真实 APB 基址，也不能证明其中偏移已经在 FPGA RTL 中实现。

特别是 `board_io.h` 会在缺少生成 `soc.h` 时拒绝正式构建；其中的 `APB_VISION_BASE_PLACEHOLDER` 路径仅用于独立测试。

## 关闭门禁所需输入

1. 从准确构建配置复制或引用的 Efinity 生成 `soc.h`。
2. 与之对应的 SoC 工程/设置产物，以及生成顶层端口声明。
3. 实际 APB 用户从机名称、基址、`PADDR` 宽度、`PSEL/PENABLE/PWRITE/PWDATA/PSTRB` 语义、`PREADY` 与 `PSLVERROR` 行为。
4. APB 时钟/复位名称和频率，以及每个目标通道的像素时钟/复位。
5. 对 APB 从机槽位是否空闲、是否必须重新生成 SoC/IP 的明确结论。
6. 对应构建树的身份确认。`D:\final_project` 的 `top.v` 与 `mem_test.xml` 存在差异，审查合并前不能使用。

## 暂缓工作

以下项目是暂缓，不是取消：

- 仿真与真实 Debayer 时序验证。
- GUI Debug Wizard 采集、PNR、bitstream 生成与板级采集。
- APB 从机 RTL、配置 CDC、特征快照 CDC 和 CPU 中断连线。
- CPU 到 OSD 的 active 寄存器组及 HDMI 链路中的 OSD 插入。
- ch0 对称预处理实例。

## 恢复检查表

任何 RTL 修改前，必须将以上生成物附在新的 Review Packet 中，并完成：

- 通过生成 `soc.h` 将真实 APB 基址传入 `board_io.h`，禁止在源码中写死字面量基址。
- 将旧寄存器表草案与 `vision_preprocess_channel` 的实际字段对齐。
- 审查时钟、复位与 CDC 的所有权。
- 确认 CPU 和 FPGA 对 `frame_id` 应答、配置提交序号的理解一致。
- 将 OSD 像素域时序与特征采集分开审查。

