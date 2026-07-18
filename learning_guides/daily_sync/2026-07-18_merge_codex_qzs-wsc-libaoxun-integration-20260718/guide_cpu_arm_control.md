# 学习指南：嵌入式 CPU 与通信控制 - 终态释放 ACK 与任务二阻塞
> 生成日期: 2026-07-18 | 对应分支合并: `codex/qzs-wsc-libaoxun-integration-20260718`
> 预计阅读时间: 10-15 分钟 | 面向角色: B（嵌入式 CPU 与通信控制）

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：结果锁存后只“清空快递柜”，不再拆包

* **改动背景与问题**：旧 runtime 读取终态后的快照却不 ACK，单槽 I1 可能一直占用并跨轮 overrun。
* **源码剖析**：[release_after_latch](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/single_camera_runtime.c#L158-L181) 对同一 `frame_id` 做 release-only ACK；[终态分支](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/single_camera_runtime.c#L264-L285) 不分类、不提交第二个结果，撕裂、配置错或 ACK 错继续 fail-closed。
* **经验总结**：像快递柜里的包裹已经完成本轮判定，后来的包只负责腾空柜门，绝不能重新触发一次业务或机械臂动作。

### 📌 核心点二：圆柱/锥体 WAIT 是安全降级，不是任务二完成

* **改动背景与问题**：圆柱/锥体阈值没有真实相机标定，直接输出 `SHAPE_MISMATCH/SKIP` 会制造虚假的确定性。
* **源码剖析**：[F1 形状门](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/include/single_camera_f1.h#L17-L31) 只信任正方体；非正方体保持 WAIT，最终只能超时或人工放弃。
* **经验总结**：这是“看不清就不放行”的安检门。安全上正确，但比赛任务二仍为 `BLOCKED`，必须用固定摄像头实拍和混淆矩阵解锁。

## 2. 可迁移的系统级工程经验

* **避坑警示**：runtime `648/648`、F1 `213/213`、adapter `33/33` 已通过，但 classifier 严格 MSVC 入口因 `C4127` 仍 `FAIL`。真实 I1/APB 是 **“代码审查通过、运行时未触发”**。
* **通用方法论**：输入有效性、业务判断和动作授权必须三层分开；ACK 只解决传输占用，不能等价于分类成功，更不能等价于动作授权。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：复习四项任务的逐轮识别—判断—执行计分规则，尤其任务二混合形状池的识别要求。
* **拓展基础知识**：学习 16 位序号半区间比较、幂等事务和单槽 mailbox。把半区间规则理解为环形跑道：前半圈算“向前”，后半圈按旧数据拒绝。
