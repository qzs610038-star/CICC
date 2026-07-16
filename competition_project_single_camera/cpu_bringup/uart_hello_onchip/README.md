# UART0 片上 RAM Hello

这个独立应用只用于证明 QCRV32 能取指运行、UART0 能收发，不使用外部 DDR，也不接入机械臂控制。

## 固定契约

- BSP：生成的 `efinix/EfxSapphireSoc`
- 片上 RAM：`0xF9000000`，16 KiB
- Linker：生成的 `default_i.ld`
- UART0：`0xE8010000`，115200 baud，8N1
- 启动：保留 `-DSMP`，只有 hart 0 执行 `main`，其余核心安全等待
- CPU JTAG：FPGA `JTAG_USER2`
- 构建可复现：编译时传入显式 `BUILD_ID`，禁止 `__DATE__`/`__TIME__`

禁止使用 `default.ld`。它指向旧的 `0x00001000` 外部存储地址，不适合本次片上 RAM Hello。

## 构建

本目录提交了 Hello 构建所需的最小 BSP 集：

```text
embedded_sw/efx_hard_soc/software/standalone/common/bsp.mk
embedded_sw/efx_hard_soc/software/standalone/common/riscv64-unknown-elf.mk
embedded_sw/efx_hard_soc/software/standalone/common/standalone.mk
embedded_sw/efx_hard_soc/software/standalone/common/start.S
embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.mk
embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h
embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/linker/default_i.ld
```

这些文件与 `ip/EfxSapphireHpSoc_slb/settings.json` 属于同一批 Efinity `2025.2.288.4.15` Hard SoC 生成物。脚本会在构建前逐项检查，缺少任何文件都会明确失败，不会退回旧地址或其他 BSP。

安装路径不写入仓库。选择以下任一种方式指定 Efinity RISC-V IDE 根目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Clean -ToolchainRoot <Efinity-RISC-V-IDE根目录> -BuildId <标识符>
```

或：

```powershell
$env:EFINITY_RISCV_IDE = '<Efinity-RISC-V-IDE根目录>'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Clean -BuildId <标识符>
```

`-BuildId` 为必填参数，只接受 `[A-Za-z0-9._-]+` 的 ASCII 字符（最长 64 字符），空值或非法字符会导致构建失败。示例：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Clean -ToolchainRoot D:\Efinity\efinity-riscv-ide-2025.2 -BuildId g4a-0737304-r1
```

如果工具链已经加入 `PATH`，也可以不指定 `-ToolchainRoot`。脚本固定要求 Efinity 2025.2 RISC-V 工具名，并自动检查：

- ELF入口必须精确为 `0xF9000000`，并通过 `nm` 确认 `_start` 位于同一地址。
- 唯一 LOAD 段必须从 `0xF9000000` 开始且末端不得超过 `0xF9003FFF`。
- ELF不得含未解析符号。
- 在相同构建路径和环境下，相同 `BUILD_ID`、源码及工具链的连续两次 clean build 应产生完全一致的 ELF；当前构建包含 DWARF 路径信息，不承诺跨绝对路径或跨机器 bit-for-bit 一致。
- 构建或审计失败时自动清理 ELF/HEX/BIN/ASM/map 等发布制品。
- 输出 ELF、HEX、BIN、反汇编和内存占用。

编译已启用 `-Wdate-time`（配合 `-Werror`），源码中禁止使用 `__DATE__`/`__TIME__`。

如需重生 BSP，使用 Efinity `2025.2.288.4.15` 打开本工程，并以 `ip/EfxSapphireHpSoc_slb/settings.json` 重新生成 Hard SoC 的 Embedded Software。禁止使用其他工程的 `soc.h` 或 linker 覆盖本目录；重生后必须重新核对 `soc.h`、`default_i.ld` 和本 Review Packet 中记录的 SHA-256。

## 预期 UART 输出

```text
TJ375 CPU+VIDEO UART0 HELLO
PROFILE=UART0_HELLO_ONCHIP BACKEND=NONE
BUILD_ID=g4a-0737304-r1
Type characters to verify echo.
```

串口终端输入的字符应逐字节回显。空闲时周期性输出 `'.'` 作为 activity marker（非精确定时，不能替代独立 liveness 证明）。该程序不访问特征寄存器、OSD、外部 DDR、UART2 或 myCobot。

## 上板边界

1. 先烧录与该工程匹配的联合 `mem_test.bit`。
2. 通过 QCRV32 `JTAG_USER2` 将 `build/uart_hello_onchip.elf` 下载到片上 RAM。
3. UART终端设置为115200、8N1、无流控。
4. 首次仅连接、下载和运行，不写CPU固件到Flash。
5. 未确认COM口前只允许监听候选串口，不得向机械臂串口发送测试数据。
