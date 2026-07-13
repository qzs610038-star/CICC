# 学习指南：FPGA 与视频前端 - 默认走真路，调试源必须持证上岗
> 生成日期: 2026-07-14 | 对应分支合并: `dev/wsc6090-CPU@885e97a` + `fable5-remediation@758e864`
> 预计阅读时间: 10-15 分钟 | 面向角色: A

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：production 与 synthetic 不再共用一把默认开关
* **改动背景与问题**：历史顶层曾默认选中合成视频，map 即使通过也可能只证明“演习通道”可用，不能证明真实摄像头链路。
* **源码剖析**：[top.v](file:///D:/第十届集创赛-雄芯院材料/final_project/fpga/rtl/top/top.v#L2088-L2097) 规定只有显式定义 `COMPETITION_DEBUG_SYNTHETIC` 才把两路选择器置 1；未定义时 production 默认走真实输入。[后续 mux](file:///D:/第十届集创赛-雄芯院材料/final_project/fpga/rtl/top/top.v#L2120-L2141) 再由这两个常量选择数据流。
* **经验总结**：像机场登机口一样，真实乘客走默认通道；测试假人必须拿着明确的“DEBUG 通行证”才能进入。这样不会因为忘记改一个常量，把合成色条当成摄像头验收。

### 📌 核心点二：map PASS 与 PNR FAIL 必须同时写进结论
* **改动背景与问题**：本轮 production/default map 通过，资源为 ADD 2081、LUT4 11939、FF 10492、RAM10 250、DPRAM10 8；但 PNR 报告 2288 个 IO 随机放置并在 `outpad` 断言处失败。
* **证据剖析**：[合并适配说明](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/review_packets/cpu_branch_merge_adaptation_20260714.md#L84-L103) 保存了构建结果和不可越过的边界。[CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L32-L40) 把 PNR 阻塞登记为当前事实。
* **经验总结**：map 像“零件清点合格”，PNR 才是“零件真的装进机箱”。零件数量算得出来，不等于机箱已经合上，更不等于 bitstream 能上板。

### 📌 核心点三：不要给 2288 个导出 IO 盲补引脚
* **改动背景与问题**：大量 AXI/内部接口被当成顶层 IO 导出，首先应检查 Interface Designer/periphery 与顶层导出边界。
* **源码剖析**：[工程边界说明](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md#L620-L634) 要求先审查生产构建与 periphery，再关闭 PNR 阻塞。
* **经验总结**：这像建筑图把每根室内网线都误画成了外墙插座。正确动作是修正图纸边界，不是给几千根线各找一块外墙打孔。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：debug map 即使通过，也只能证明显式合成入口可编译；真实摄像头、CDC、HDMI、PNR、STA、bitstream 和上板都要单独给证据。
* **通用方法论**：对有“演习/生产”两种数据源的工程，生产路径必须是安全默认，调试路径必须显式 opt-in，并让构建日志可搜索到宏名。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：Efinity Interface Designer、TJ375 periphery 生成流程、正式 `mem_test.peri.xml` 与 `constrain.sdc` 的职责边界。
  * **应用与意义**：判断哪些端口应由硬核/periphery 消化，哪些才是真正需要封装引脚约束的顶层 IO。

* **拓展基础知识**：
  * **推荐学习内容**：综合、布局布线、静态时序和 bitstream 四阶段的证据边界，以及编译期 feature flag。
  * **应用与意义**：避免把 map PASS 误写成全流程 PASS，也避免 debug 构建污染比赛 production 镜像。
