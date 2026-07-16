# M2 UART0 Hello — 操作员制品 hash 预检

> 日期：2026-07-16
> 范围：仅 `competition_project_single_camera/` 的易失性 FPGA 配置前制品身份核验。

## 操作员回传

操作员在候选工程目录运行 `Get-FileHash -Algorithm SHA256`，回传：

| 制品 | SHA-256 |
|---|---|
| `outflow_m2_cpuhello_20260716_1730/mem_test.bit` | `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347` |
| `cpu_bringup/uart_hello_onchip/build/uart_hello_onchip.elf` | `C99FD39DB437409A63A6061CD29698B5B60099B9E24A77B155B871E169BF5DA5` |

## Codex 独立复核

对相同两个现存文件重新执行 `Get-FileHash -Algorithm SHA256`，验证命令 exit 0；两项 hash 与操作员回传和 M2 当前批次均一致。

## 结论与边界

制品身份预检通过，因此仅允许进入操作卡的第 1 节“易失性 FPGA 配置”。本文件不证明 FPGA 已配置、USER2 已连通、ELF 已下载、CPU 已取指或 UART0 已输出；这些仍为 `NOT VERIFIED`。
