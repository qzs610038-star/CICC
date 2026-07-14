# myCobot G3 之后下一步简要说明

日期：2026-07-14

## 当前状态

- G0–G3 纯软件门：PASS。
- 当前分支：`codex/mycobot-g0-g3-bringup-20260714@d433bca`，修改仍未提交。
- G4 正式 SoC/PNR/部署、烧录、J52、UART2 和真实机械臂：NO-GO。
- 当前制品全部是 `NOT_FOR_FLASH`，不能直接烧录。

## 现在先做什么

只有在用户确认以下条件都具备后，才准备 G4：

1. 开发板在手，能安全供电；
2. Efinity 2025.2、许可证和 Programmer/JTAG 可用；
3. UART0 端口能被电脑识别并可保存原始日志；
4. 有足够的磁盘空间保存 SoC、map/PNR/STA、bitstream 和日志；
5. 机械臂与 J52 全程断开，J52 的 VCC 保持悬空。

任一条件不满足，就继续停在 G3，不烧录、不接线。

## G4：生成并审查正式 SoC 制品

1. 用户在 Efinity/Interface Designer 中生成同一批次的 SoC/BSP、`soc.h`、linker、startup/trap 和调试配置。
2. 用户完成正式工程的 map、PNR、STA，并保存完整日志；同时生成 bitstream，但先不要烧录。
3. Codex 核对 `soc.h`、linker、startup、bitstream、CPU 地址/时钟/复位、UART0、CLINT、内存布局和 SHA-256 是否来自同一批次。
4. 若 PNR/STA、启动入口、异常路径或来源追溯有任何 FAIL，停止 G4，不进入烧录。
5. 只有 G4 Review Packet 明确 PASS，才允许安排 G5 的板上无臂模拟。

## G5：机械臂断开的板上模拟

1. 用户确认 J52 和机械臂完全断开，只连接开发板供电、JTAG/Programmer 和 UART0。
2. 用户按审查后的命令烧录对应的板上 simulated 制品，并保存 Programmer/JTAG 日志。
3. 观察 UART0 启动信息，至少完成 3 次复位启动检查。
4. 运行板上 20 轮模拟自检：目标请求数量、非目标零请求、无死锁；同时确认 J52/F12 没有 UART2 活动。
5. 连续运行观察窗口并保存 UART0 原始日志、时间戳和结果截图，形成 G5 Review Packet。

G5 通过后，仍不能直接接机械臂；下一门必须先单独完成 UART2/J52 断电复核、3.3V 电气测量和无臂回环，再进入只读协议门。

## 明确禁止

- 不要把当前 `NOT_FOR_FLASH` ELF、临时 linker 或候选 `soc.h` 当成正式板上制品。
- 不要在 G4/G5 期间连接 J52 或机械臂。
- 不要把 UART0 启动日志、模拟后端结果或 TX 成功解释成真实机械臂响应。
- 不要在只读协议、电平、Basic=`transponder`、Atom=`atomMain` 和安全 Review Packet 未通过前执行任何动作。

## 交付顺序

`用户确认 G4 条件 → 生成同批次 SoC/BSP → Codex 审查 G4 → 用户断臂烧录 → 板上 simulated → G5 Review Packet → 再决定 UART2/只读`。

本说明不授权烧录、接线或机械臂动作；下一次继续前，应由用户明确回复是否具备 G4 条件。
