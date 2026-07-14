# M0人工构建、烧录与画面复现记录

> 状态：**等待用户执行**
>
> 工程：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml`
>
> Efinity：`2025.2.288.4.15`
>
> 说明：仓库与D盘75个白名单文件当前哈希完全一致，无需再次覆盖D盘源码。

## 1. 构建前

- [ ] 确认没有其他成员修改D盘源码。
- [ ] 关闭或确认无旧Efinity构建仍在运行。
- [ ] 打开`D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml`。
- [ ] 确认器件为`TJ375N529`、Timing Model为`I3`。
- [ ] 不修改RTL、IP、periphery、SDC或Debugger设置。

## 2. 完整构建

由用户在Efinity GUI中运行完整flow至bitstream。请填写：

```text
开始时间：
结束时间：
Map：PASS / FAIL
PNR：PASS / FAIL
Bitstream：PASS / FAIL
新bitstream路径：
```

请保留以下原始产物，不需要手工摘抄全部warning：

```text
outflow/mem_test.map.rpt
outflow/mem_test.warn.log
outflow/mem_test.route.out
outflow/mem_test.timing.rpt
outflow/mem_test.cdc.rpt
outflow/mem_test.pgm.out
outflow/mem_test.bit
```

## 3. 烧录与画面

连接保持为：摄像头接J48/ch0，HDMI接电脑。

| 检查 | 结果 | 备注 |
|---|---|---|
| 第1次完全断电冷启动 | PASS / FAIL | |
| 第2次完全断电冷启动 | PASS / FAIL | |
| 第3次完全断电冷启动 | PASS / FAIL | |
| 连续运行10分钟 | PASS / FAIL | |
| 实时摄像头画面 | PASS / FAIL | |
| 无fallback纯色 | PASS / FAIL | |
| 无明显花屏/条纹 | PASS / FAIL | |
| 无冻结 | PASS / FAIL | |
| 分辨率与旧基线一致 | PASS / FAIL / UNKNOWN | |
| 帧率与旧基线一致 | PASS / FAIL / UNKNOWN | |

## 4. 请反馈

最少反馈以下内容：

1. Map、PNR和bitstream是否成功。
2. 烧录后是否是J48真实画面。
3. 三次冷启动和10分钟运行结果。
4. 若失败，失败阶段、工具原始错误和当前HDMI现象。

不要为了通过M0修改工程。任何失败先保留原始日志，由Codex核查后决定最小动作。

## 5. Gate判定

- 构建、烧录、3次冷启动和10分钟稳定全部通过：M0 `PASS`，允许进入M1A“只禁用ch1实例”。
- 构建通过但画面失败：M0 `FAIL`，只核查构建身份和板级差异。
- 构建失败：M0 `FAIL`，不烧录、不修改视频链猜测修复。

