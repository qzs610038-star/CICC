# Review Packet: A6 CPU 接口契约与尺寸暂缓

> 日期：2026-07-12  
> 审查结论：寄存器无关 CPU 契约可冻结；尺寸标定暂缓时禁止任务三、四产生执行语义。

## 已冻结的语义

- `target_config_t`：目标先 staging、再 apply、轮内锁存。
- `operator_event_t`：`PLACE/REMOVE/ABANDON/RESET + event_seq`；重复同序号幂等、旧序号拒绝。
- `result_status_t`：输出识别结果、目标判断、执行/跳过/等待、理由码、轮状态和关联序号。
- `SIZE_STATE_UNAVAILABLE`：任务三、四固定 `WAIT + SIZE_UNAVAILABLE`，不得使用未标定尺寸，任务一、二可继续。

## 测试

- 契约：`35/35` 通过。
- 四任务/20 轮：`135/135` 通过。
- 旧 matcher：`82/82` 通过。

## 禁止项

- 不将该 API 描述为已分配的寄存器地址或 FPGA bitfield。
- 不将 Mock 尺寸值视为尺寸标定结果。
- 不接入 `main.c`、OSD、SoC 或 `arm_controller`。

## 下一步门禁

待 SoC/FPGA 接口恢复后，先将这三个结构逐字段映射为一次性快照和 commit/ACK 规则，再进行工程级 map。尺寸标定完成并以真实数据通过验收前，`SIZE_STATE` 必须保持 `UNAVAILABLE`。
