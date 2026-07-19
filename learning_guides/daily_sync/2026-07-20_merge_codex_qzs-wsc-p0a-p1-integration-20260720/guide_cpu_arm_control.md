# 角色 B：P0-A 生命证明与 P1 Host 事务闭环

> 10–15 分钟目标：掌握 WSC 固定实现的可用能力和禁区。

## P0-A：给 CPU 留黑匣子轨迹

阶段码像沿路盖章：写 canary，再进入 UART 配置、首字节发送与有限重试。UART 永远
not-ready 时也必须有界退出并继续 heartbeat，不能死锁。Host `10/10` 已通过，见
[p0a_diag.c](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/cpu_bringup/uart1_hello_onchip/src/p0a_diag.c)
和
[linker script](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/cpu_bringup/uart1_hello_onchip/linker/p0a_diag.ld)。

黑匣子设计正确不等于飞机已起飞；板上仍需 PC/断点或 RAM canary 证明 CPU 执行。

## P1：把一轮判断当成银行转账

P1 使用 32-bit staging/commit、`round_id` 去旧、feature ACK 和 result latch/release。
先填单再一次性 commit；重复/过期流水号拒绝；成功读取结果后才释放，不能重复扣款。

阅读顺序：

1. [p1_host_model.h](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/cpu/src/p1_host_model.h)
2. [model tests](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/cpu/tests/test_p1_host_model.c)
3. [20 轮 runner](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/cpu/tests/p1_replay_runner.c)

fresh 结果：model `37/37`、adapter `39/39`、classifier `54/54`、F1 `213/213`、
runtime `648/648`、四任务 replay `20/20` 且 `ARM=0`。

## 证据为什么必须能识别篡改

每轮保存真实 33-byte little-endian snapshot，validator 独立重算 SHA-256；manifest 绑定
CRLF→LF 规范化后的四个文本。单独改 `rounds.jsonl` 后 verifier 必须 exit 1。这相当于
封条不仅贴上，还证明撕开就报警。

## 当前缺口

- Task 2 非正方体仍 `BLOCKED/PROVISIONAL`。
- Task 3/4 仍受真实标定与输入可用性约束。
- 未定义真实 APB 偏移、PSTRB、IRQ 或 CDC 实装。
- `ARM_ENABLED=0`；无 UART2/J52、接线或动作授权。

## 自学入口

### 优先赛方资料

- [P1 三层证据矩阵](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/docs/evidence_manifests/P1_HOST_THREE_LAYER_EVIDENCE_MATRIX_20260719.md)
- [CPU README](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/competition_project_single_camera/cpu/README.md)

### 拓展基础知识

- 幂等事务、单调序号与序号回绕比较。
- golden model、negative cases 与内容寻址证据。
