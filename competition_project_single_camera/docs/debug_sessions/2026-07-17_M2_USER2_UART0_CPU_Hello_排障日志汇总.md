# 2026-07-17 M2 USER2/UART0 CPU Hello 排障日志汇总

> 范围：`competition_project_single_camera/` 的隔离 CPU Hello 候选工程。
>
> 边界：本汇总只沉淀 2026-07-16 的离线预检、配置链修正与安全门结论；不代表 `final_project/` A0/G4 闭环，不开放 UART2/J52/myCobot。
>
> 原始 JSON：`docs/debug_sessions/evidence/*.json` 是本机调试记录，含机器路径和中间输出，按 `.gitignore` 仅本地保留；本文件是供队友共享的脱敏索引。

## 当前结论

- `VOLATILE_BITSTREAM_CONFIG_PASS`：批准 bitstream 的专属 ASCII 暂存镜像已完成一次易失性 JTAG 配置，完整 SHA-256 与批准批次一致。
- FTDI `0403:6011` 已可见三条通道，但 UART0 通道映射仍未确认。
- USER2 调试配置已静态收敛到硬 TAP、`CPUTAPID=0x006A0EF3` 覆盖、片上 RAM 安全 work area 和运行时 PC 门。
- USER2 连通、CPU 实际取指、UART0 115200 横幅和 `x` 回显仍为 `NOT VERIFIED`。在 PC 范围取证获准前，禁止 Resume、打开 UART 或发送字节。
- 本批所有原始 JSON 都记录 `serial_port_opened=false`、`uart_bytes_sent=0`、`programmer_invoked=false`、`flash_operation_invoked=false`；含 OpenOCD/GDB 字段的记录同样为 `false`。BSP 的 `-Apply` 仅生成本机调试支持文件，不是连板动作。

## 排障主线

1. 初始 FTDI 采集误判为未连接：Windows 实例使用 `VID_0403+PID_6011`，旧正则只接受 `&`。修正为 `[&+]` 后，三条 FTDI 通道均被枚举。
2. 首次 Programmer 虽传输完成，却选中了暂存目录遗留的 `outflow/mem_test.bit`；其 SHA-256 为 `9515FAC58CBE4DC07ADED28B6038C64485989C6BE650D09E5C07F6D5C4F1A169`，与批准批次不一致，因此拒绝计入 M2 成功。
3. 重新以批准 bitstream 的专属暂存镜像配置后，源/目标完整 SHA-256 均为 `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347`，才关闭易失性配置门。
4. USER2 生成链先后修正了：运行时 PC 门、TJ375 与 Ti375 的 CPUTAPID 差异、Hello 入口与 OpenOCD work area 重叠。v4 的后加载 work-area 方案已废弃；v5 最终安全配置仅将 target-create 的 work area 前移到 `0xF9000C00..0xF9000FFF`，并保留硬 TAP/USER2/PC 门。
5. UART 横幅采集器已验证 fail-closed：没有合格的 PC-gate approval 时只能返回 HOLD，绝不打开串口。

## 原始记录逐份索引

| 文件 | 结果 | 关键结论 | 归类 |
|---|---|---|---|
| `m2_ftdi_enumeration_20260716_151807.json` | `HOLD_NO_CONNECTED_FTDI` | 初始计数为 0，定位匹配缺陷的起点。 | 保留 |
| `m2_ftdi_enumeration_20260716_151908.json` | `FTDI_ENUMERATED` | 修正后计数为 3。 | 中间 |
| `m2_ftdi_enumeration_20260716_152115.json` | `FTDI_ENUMERATED` | CH340 字段改为中性命名。 | 中间 |
| `m2_ftdi_live_verify_20260716_152234.json` | `FTDI_ENUMERATED` | 三条 FTDI 通道可见。 | 中间 |
| `m2_ftdi_card_verify_20260716_152433.json` | `FTDI_ENUMERATED` | 最终 FTDI 预检；仍禁止配置 FPGA。 | 保留 |
| `m2_user2_elf_preflight_20260716_160707.json` | `HOLD_ELF_MIRROR_MISSING` | 发现 ELF 暂存镜像缺失。 | 保留 |
| `m2_user2_elf_preflight_20260716_160718.json` | `HOLD_ELF_MIRROR_MISSING` | 与上一条重复。 | 中间 |
| `m2_user2_elf_preflight_20260716_161436.json` | `ELF_MIRROR_READY_FOR_USER2_RAM_LOAD` | 镜像首次就绪。 | 中间 |
| `m2_user2_elf_preflight_20260716_163439.json` | `ELF_MIRROR_READY_FOR_USER2_RAM_LOAD` | 重复就绪记录。 | 中间 |
| `m2_user2_elf_preflight_20260716_163454.json` | `ELF_MIRROR_READY_FOR_USER2_RAM_LOAD` | v2 最终 ELF 身份预检；仍禁止 Resume/UART。 | 保留 |
| `m2_user2_debug_bsp_20260716_161226.json` | `DRY_RUN_NO_BSP_GENERATION` | v1 dry-run。 | 中间 |
| `m2_user2_debug_bsp_20260716_161239.json` | `USER2_DEBUG_BSP_READY` | v1 生成器 exit 0。 | 中间 |
| `m2_user2_debug_bsp_20260716_161318.json` | `USER2_DEBUG_BSP_READY` | 补充初始生成产物 hash。 | 保留 |
| `m2_user2_debug_bsp_20260716_162117.json` | v2 dry-run | 官方 Titanium 模板为 RAM debug；legacy `instr_addr` 未被消费。 | 中间 |
| `m2_user2_debug_bsp_20260716_162132.json` | v2 READY | 引入运行时 PC 必须落在片上 RAM 的门。 | 保留 |
| `m2_user2_debug_bsp_20260716_162726.json` | v3 dry-run | 发现默认 CPUTAPID 属于 Ti375。 | 中间 |
| `m2_user2_debug_bsp_20260716_162733.json` | v3 READY | 加入 TJ375 `0x006A0EF3` 覆盖和 PC 门。 | 保留 |
| `m2_user2_debug_bsp_20260716_163248.json` | v4 dry-run | 审查 Hello 映像与安全 work area。 | 中间 |
| `m2_user2_debug_bsp_20260716_163255.json` | v4 READY | 后加载 `m2_user2_safe_workarea.cfg`，后续已废弃。 | 保留废弃原因 |
| `m2_user2_debug_bsp_20260716_163912.json` | v5 dry-run | safe debug cfg 尚不存在。 | 中间 |
| `m2_user2_debug_bsp_20260716_163949.json` | `HOLD_GENERATED_DEBUG_BSP_VALIDATION_FAILED` | 拒绝并非只替换 work area 的中间 cfg。 | 保留 |
| `m2_user2_debug_bsp_20260716_164013.json` | v5 READY | 最终安全 cfg：仅前移 work area，保留 CPUTAPID/PC 门。 | 保留 |
| `m2_uart0_banner_capture_dryrun_20260716_164622.json` | `DRY_RUN_NO_SERIAL_OPEN` | 缺少 approval JSON 时的 dry-run。 | 中间 |
| `m2_uart0_banner_capture_unapproved_guard_20260716_164635.json` | `HOLD_PC_GATE_APPROVAL_REQUIRED_NO_SERIAL_OPEN` | 首个 UART 拒绝门。 | 保留 |
| `m2_uart0_banner_capture_postpatch_dryrun_20260716_164813.json` | `DRY_RUN_NO_SERIAL_OPEN` | 补丁后 dry-run。 | 中间 |
| `m2_uart0_banner_capture_postpatch_unapproved_20260716_164814.json` | `HOLD_PC_GATE_APPROVAL_REQUIRED_NO_SERIAL_OPEN` | 最终 guard：禁止换 UART 类、CH340、UART2/J52/myCobot。 | 保留 |

## 队友接力的唯一下一步

只使用 `m2_uart0_user2_ram_download_operator_card_20260716.md` 的硬 TAP/USER2/RAM 流程。加载固定 Hello ELF 后，在任何 Resume 或串口动作前，先截图证明 PC/反汇编落在 `0xF9000000..0xF9003FFF`。不满足即 STOP；禁止 USER1、SoftTap、External、Flash、DDR、UART2/J52 和 myCobot。
