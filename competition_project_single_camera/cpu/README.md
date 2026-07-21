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
- `include/single_camera_runtime.h` 与 `src/single_camera_runtime.c`：平台无关的
  `read/ack/submit/emit` runtime seam；事件采用版本化 `@E|v=1|...` 键值行，ARM 永久禁用。
- `include/single_camera_fake_transport.h` 与 `src/single_camera_fake_transport.c`：确定性 Host
  fake transport，不含 COM、UART、MMIO 或机械臂协议。
- `include/single_camera_mmio_transport.h` 与 `src/single_camera_mmio_transport.c`：地址/ABI
  未确认前的 fail-closed 占位 backend，全部操作返回 `SC_TRANSPORT_UNAVAILABLE`。
- `tests/run_g2_host_evidence.ps1`：自动定位 VS2022/MSVC 的非交互 G2 Host runner；对象文件只落在系统临时目录。
- `../integration/single_camera_feature_contract.md`：分类器唯一允许的未来 FPGA
  特征来源；当前是无地址的 Host 契约，不能据此编写 MMIO。

当前边界：

- 这是 `HOST VERIFIED` 的纯软件核心，尚不是板上 CPU、UART、APB 或 OSD。
- 尺寸固定为 `SIZE_UNAVAILABLE`，任务三/四只能显示不可用或人工放弃，不得
  产生执行授权。
- 圆柱/锥体细分门限仅为未标定诊断标签；F1 只信任 `CUBE`，圆柱/锥体保持
  `WAIT`，只能由超时或人工放弃结束。任务二完整能力保持 `BLOCKED`，现场
  标定和混淆矩阵完成前不得将圆柱/锥体细分作为得分能力宣称。
- `ARM_ENABLED=0`；任何 `EXECUTE` 都只表示 F1 应显示 `ARM_DISABLED`，绝不
  发送机械臂请求。
- runtime seam 的 `source=fake_transport` 仅证明 Host/fake 事务；它不证明 RISC-V ELF、
  APB、UART 或板级能力。真实 APB ABI 经过独立审核后，才可新增 production backend。
- 同次 Efinity GUI 生成 `soc.h`、linker 和合法 SoC 资源后，才添加平台适配层。
