# integration

新单摄工程的机器可读接口契约目录。

当前M0未实现SoC/APB/CDC或业务按键。后续只保留一套正式运行契约：

```text
TARGET_CFG
OPERATOR_EVENT
EVENT_ACK
FEATURE_SNAPSHOT
ROUND_RESULT
round_controller
```

所有正式地址、UART、GPIO、IRQ、时钟和复位必须来自新Demo同一次Efinity SoC生成物。不得复制`final_project`的候选MMIO地址。

