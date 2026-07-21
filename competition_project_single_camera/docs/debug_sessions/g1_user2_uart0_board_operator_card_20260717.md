# G1 当前批次 USER2 / UART0 持板操作卡（2026-07-17）

状态：`OPERATION PACK OFFLINE READY / BOARD NOT VERIFIED`

适用批次：`G1-20260717-A897-E5BC`。仅允许使用同目录 manifest 中的完整 hash；不得以 2026-07-16 M2 卡或其 `2EA4.../C99...` 制品替代。

## 执行前

1. 在持板机准备一个 ASCII staging 目录，传入真实源文件的显式路径运行 `prepare_g1_user2_staging.ps1`。
2. 将脚本输出的 batch ID、源/目标 SHA-256 与大小回传。任何 preflight 或 staging 非零退出均停止，不选择其他文件、不覆盖既有文件。
3. 确认操作范围只有匹配 bitstream、`USER2`、片上 RAM 的匹配 Hello ELF，以及之后的 UART0 `115200 8N1` 只读监听。

## Checkpoint A — USER2 / RAM / PC（不得 Resume）

仅在 staging 输出为 `G1_CURRENT_BATCH_STAGING_READY_NO_HARDWARE_ACTION` 后，由持板机操作者执行：

1. 在官方工具中人工复核 staged bitstream 与 ELF 的完整 hash、大小和 batch ID。
2. 仅选择硬 TAP `USER2`，仅采用批准的安全 debug cfg 组合：`ftdi_ti.cfg` 与 `debug_ti_m2_safe.cfg`。保持 RAM-only 下载和加载后 halt。
3. 下载 ELF 后保持暂停；不得 Resume、不得打开 UART0。
4. 回传原始证据：操作者/时间、branch/HEAD、staging 路径、两份制品完整 hash/大小、USER2 选择截图、RAM 下载 console、暂停 PC 与反汇编截图、全部 warning。
5. PC 必须位于 `0xF9000000..0xF9003FFF`。否则停止；不得猜地址、改时钟、改复位、改 XML/SDC/RTL，或换 TAP 重试。

只有 qzs/Codex 审核上述原始证据并签发当前批次 checkpoint JSON 后，才能进入 B。

## Checkpoint B — Resume 后 UART0 只读监听

1. 将经审核的 checkpoint JSON 与显式 COM 号传给 `capture_g1_uart0_banner.ps1`。
2. 仅用 `115200 8N1`；首次只读监听，不发送任何字符，DTR/RTS 保持关闭。
3. 回传三次独立启动的原始字节/文本、时间、COM 号、异常与脚本 JSON。无输出、乱码或 reset 异常即停止。

UART0 结果只覆盖当前批次的 CPU 取指/UART0 子门；不证明 APB、视频、OSD、UART2/J52 或机械臂。

## 绝对禁止

- `USER1`、SoftTap、原始 `debug_ti.cfg`、手填旧地址。
- Flash/SPI/PROM、外部 DDR、UART2/J52。
- myCobot 接线、帧、动作，或任何机械臂相关操作。
- 在 Checkpoint A 获批前 Resume 或打开 UART0。
