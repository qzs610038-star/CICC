# integration

正式单摄视频/识别工程的机器可读接口契约目录。原双摄方案已取消，只保留历史文档和代码供追溯，不得重新成为活动闭环。

当前源码已包含 Hard SoC、同批生成的 `soc.h`、APB0 只读 MAGIC 和 feature 统计 RTL；但当前 feature capture 被禁用、输出未接 APB，业务 CDC/ACK、目标/事件、结果写回和 OSD 均未实现。I0 已冻结为 SoC UART1 路由到板载 Type-C UART1（RX=`GPIOR_96/B12`、TX=`GPIOR_100/D12`，`115200 8N1`），但当前 Hard SoC 尚未重新生成，因此 CPU 取指、UART1、APB 实读和板级 feature 仍为 `NOT VERIFIED`。UART0/R0 只保留为历史证据。后续只保留一套正式运行契约：

```text
TARGET_CFG
OPERATOR_EVENT
EVENT_ACK
FEATURE_SNAPSHOT
ROUND_RESULT
round_controller
```

所有正式地址、UART、GPIO、IRQ、时钟和复位必须来自新Demo同一次Efinity SoC生成物。不得复制`final_project`的候选MMIO地址。

`single_camera_feature_contract.md` 冻结 `FEATURE_SNAPSHOT` 的字段、帧原子性和
CDC/ACK 规则。它不定义业务地址；源码存在也不表示 feature 已经 CPU 可读或 OSD 已板级实现。

I0 的固定方向、生成要求和旧批次处置见 `I0_UART1_INTERFACE_FREEZE.md`；三人文件范围见 `../../docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`。
