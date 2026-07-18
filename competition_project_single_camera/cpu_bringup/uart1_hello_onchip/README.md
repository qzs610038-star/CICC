# UART1 片上 RAM Hello（待生成接口骨架）

> 状态：`SCAFFOLD ONLY / GENERATED SOC.H REQUIRED / NOT BUILT / BOARD NOT VERIFIED`

本目录预留给 wsc 的 I0 UART1 Hello 固件。接口固定为 SoC UART1 → 板载 Type-C
UART1（RX=`GPIOR_96/B12`、TX=`GPIOR_100/D12`），`115200 8N1`。

当前不提供 `main.c`、硬编码基址或可执行构建脚本，因为仓库现有 `soc.h` 只有
`SYSTEM_UART_0_*`。libaoxun 完成 Efinity UART1 原子生成后，wsc 必须：

1. 只引用新生成 `SYSTEM_UART_1_*` 宏；
2. 保持 ELF 唯一 LOAD 位于片上 RAM 合法范围；
3. 输出批次标识、完整 UART1 Hello 和单字符回显；
4. 不初始化 UART2/J52，不包含任何机械臂 transport；
5. 将 ELF hash 与同批 bitstream、`soc.h` 和 Review Packet 绑定。

旧 `../uart_hello_onchip/` 属于 UART0/R0 历史批次，不复制其地址或制品结论。
