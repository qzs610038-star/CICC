# integration

新单摄工程的机器可读接口契约目录。

当前源码已包含 Hard SoC、同批生成的 `soc.h`、APB0 只读 MAGIC 和 feature 统计 RTL；但当前 feature capture 被禁用、输出未接 APB，业务 CDC/ACK、目标/事件、结果写回和 OSD 均未实现。CPU 取指、UART0、APB 实读和板级 feature 仍为 `NOT VERIFIED`。后续只保留一套正式运行契约：

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
