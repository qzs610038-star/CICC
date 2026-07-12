# A14 SoC/视频 PLL 重规划决策门

## 结论

当前不能继续实施 SoC/APB 集成，也不能直接改写任何视频 PLL、`.peri.xml`、约束或顶层时钟连接。

原因不是缺少软件代码，而是硬 SoC 的系统 PLL 只能使用 `PLL_BL0/PLL_BL1/PLL_BL2`，而当前视频工程三者均承担关键时钟链路。只读核查未得到“`PLL_TR1` 可替代 `PLL_BL1`”的 Titanium/TJ375N529 官方支持证据；`TR1` 出现在 I/O bank 列表中不等价于它能承接该 PLL 实例或保持现有时序。

因此 A14 的结果是**停止猜测性重规划**，将下一动作收敛为一次受控 GUI 支持性审查。

## 已确认资源

| 资源 | 当前实例 | 输入与作用 | 是否可直接释放 |
|---|---|---|---|
| `PLL_BL0` | `lpddr4_pll` | `ddr_clk_ref`，LPDDR4/AXI | 否 |
| `PLL_BL1` | `pll_inst1` | `clk_74p25m`，视频系统与 `CLK_5M` | 否 |
| `PLL_BL2` | `pll_inst4` | 由 `PLL_TR0` 输出的 `i_fb_clk` 驱动，MIPI CSI/HDMI | 否 |
| `PLL_TR0` | `MIPI_TX_PLL` | `clk_25m`，为 `PLL_BL2` 提供 `i_fb_clk` | 否 |
| `JTAG_USER1` | `jtag_inst1` | 已占用但顶层 TDO 固定 0 | SoC 可改用 `USER2`，但不能解决 PLL 冲突 |

## 禁止的错误路径

- 手工合并 `.peri.xml`。
- 把 SoC 的 `PLL_BL0` 请求与已有视频 `PLL_BL0` 重复声明。
- 仅改 JTAG 为 `USER2` 后尝试合并 SoC。
- 假设 `PLL_TR1` 一定可替代 `PLL_BL1`。
- 为通过 PNR 临时删除 DDR、MIPI 或 HDMI 时钟链后宣称系统可集成。

## 唯一允许的下一审查

在隔离工程 `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\fpga\efinity\mem_test.xml` 中，由 Efinity Interface Designer 完成以下只读/受控操作：

1. 选中 `pll_inst1 (PLL_BL1)`，使用 GUI 的合法资源/迁移选项检查是否列出 Titanium 的非 `PLL_BL*` 资源；不手输 XML 名称。
2. 若 GUI 不提供合法替代项，记录截图和弹窗，判定 SoC 路线继续阻塞。
3. 若 GUI 提供候选项，仅生成新的隔离候选工程，不修改 A8、C 盘正式工程或 D 盘构建树。
4. 新候选必须同时证明：`clk_74p25m` 输入合法、原 `pll_inst1_CLKOUT0` 与 `CLK_5M` 名称/频率可保持、没有新增 pad 冲突、MIPI/DDR/HDMI 下游连线未断。
5. 只有以上四项均通过，才能重新生成 SoC（`JTAG_USER2`）并重新做资源交集审查；此时仍不可直接合并、PNR 或烧录。

## 当前可继续的工作

A11-A13 的合成 FPGA 特征和 CPU Host 回放可继续作为独立验证链。SoC/视频工程正式集成、MMIO、`main.c`、OSD 和机械臂仍保持禁止状态。

## 证据

- `final_project/docs/debug_sessions/a4_soc_video_resource_audit_20260712.md`
- `final_project/docs/debug_sessions/a9_gui_pll_jtag_resource_audit_20260712.md`
- `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\fpga\efinity\mem_test.peri.xml`
- `D:\Efinity\2025.2\ipm\ip\efx_hard_soc\ipm\ip_component.xml`
