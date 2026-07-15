# cpu

单摄 F1 CPU 核心。此目录不包含 SoC 地址、UART 驱动、APB、GPIO 或机械臂
传输，因此可以在 Hard SoC 资源冲突关闭前继续开发并做 Host 回归。

当前实现：

- `include/single_camera_f1.h`：单摄识别结果、四任务规则、PLACE/ABANDON
  事件与结果语义。
- `src/single_camera_f1.c`：一轮只锁存一次最终识别/判断；下一次 PLACE
  自动进入下一轮，不要求 REMOVE。
- `include/single_camera_classifier.h` 与 `src/single_camera_classifier.c`：将未来 FPGA 的
  单摄 ROI 统计值转换为 F1 `sc_observation_t`，不包含 APB 地址或双摄依赖。
- `include/single_camera_feature_adapter.h` 与 `src/single_camera_feature_adapter.c`：按特征
  契约拒绝诊断、溢出、过载和非 ch0 帧后才调用分类器。
- `tests/test_single_camera_f1.c`：任务一至四、20轮、重复 PLACE、ABANDON、
  超时和尺寸未标定安全门。
- `tests/run_single_camera_f1_host.ps1`：严格 Host 编译与执行入口。
- `tests/run_single_camera_classifier_host.ps1`：五色和正方体优先分类的 Host 回归入口。
- `tests/run_single_camera_feature_adapter_host.ps1`：特征快照 flags 的 fail-closed Host 回归入口。
- `../integration/single_camera_feature_contract.md`：分类器唯一允许的未来 FPGA
  特征来源；当前是无地址的 Host 契约，不能据此编写 MMIO。

当前边界：

- 这是 `HOST VERIFIED` 的纯软件核心，尚不是板上 CPU、UART、APB 或 OSD。
- 尺寸固定为 `SIZE_UNAVAILABLE`，任务三/四只能显示不可用或人工放弃，不得
  产生执行授权。
- 圆柱/锥体细分门限仅为未标定启发式；F1 只将可靠的 `CUBE/NON_CUBE` 用于
  任务判定，现场标定前不得将圆柱/锥体细分作为得分能力宣称。
- `ARM_ENABLED=0`；任何 `EXECUTE` 都只表示 F1 应显示 `ARM_DISABLED`，绝不
  发送机械臂请求。
- 同次 Efinity GUI 生成 `soc.h`、linker 和合法 SoC 资源后，才添加平台适配层。
