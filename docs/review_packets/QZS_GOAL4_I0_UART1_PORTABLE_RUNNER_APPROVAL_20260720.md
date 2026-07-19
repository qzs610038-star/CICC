# QZS Goal 4 I0 UART1 可移植执行器批准记录

## 结论

`VERDICT=APPROVED_FOR_LIBAOXUN_HARDWARE_WINDOW`

新执行提交为 `2434f013aaa57d98ee3eda040da2414ff1092b59`，基于 `eda235a1a0a1470cab2e166b1a388e87954b922b`，只修改 runner、verifier 和 manifest。

新授权 JSON 不含任何工具文件系统路径。所有实际路径由连接开发板的 libaoxun 主机在运行时提供；runner 只解析该主机上的实际路径并核验 SHA-256 与版本。本次 qzs 审查没有枚举 qzs 本机端口。

窗口为 `2026-07-20 02:15:00+08:00` 至 `2026-07-20 05:30:00+08:00`，共 195 分钟；窗口内不限制独立 fail-closed attempt 数量。

## 已修复阻塞

1. approval JSON 通过 `-Encoding UTF8` 显式读取，兼容 Windows PowerShell 5.1 的无 BOM UTF-8 文件。
2. tool approval 禁止 `normalized_path` 字段，仅绑定 SHA-256 与版本；同 hash 工具移动到其他路径的 fixture PASS。
3. UART1 先使用 `Get-PnpDevice -Class Ports -PresentOnly`，仅在无结果时回退 `Win32_SerialPort`。
4. PnP 解析支持实际 `FTDIBUS\\VID_0403+PID_6011+FTBI7G42C\\0000` 格式，并从中提取 `FTBI7G42C`，而不是错误使用末尾接口号 `0000` 作为 serial。
5. COM17、CH340、J44/UART0 排除规则保持不变。

## 静态证据

- runner SHA-256：`4A69D1951E2D5C3E971AA3E75E6E90C0BA71D337B368C486C5292A9940B53FDD`。
- verifier SHA-256：`551D80B5D7BEA21B6EE63B94F07B7ACCDDBB244D0C9CF8E8010DE7C38C707136`。
- manifest SHA-256：`DFFE3AA0B9AFBA0FDBDB0D9E9B671D1AF5CA627A4D273D2DF285040CBB4EE0D1`。
- `PORTABLE_OPENOCD_PATH=PASS`。
- `NONPORTABLE_OPENOCD_PATH_BINDING=PASS`。
- `UART1_PNP_ALLOWLIST=PASS`。
- `I0_UART1_EXECUTION_CONFIG_STATIC=PASS`。
- `git diff --check`：PASS。
- `HARDWARE_ACTIONS=NONE`。

先前两个 fail-closed 日志证明 Gate 在所有硬件动作前生效，不构成板级失败。USER2、PC、UART1 Hello/Echo 与 APB MAGIC 继续为 `NOT_VERIFIED`，等待 libaoxun 主机执行。
