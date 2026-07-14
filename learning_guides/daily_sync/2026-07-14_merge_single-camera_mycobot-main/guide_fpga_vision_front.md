# 学习指南：FPGA 与视频前端 - 单摄候选不是历史 bitstream 的替身
> 生成日期: 2026-07-14 | 对应分支合并: `b48973b + edd5328 -> main@45c790c`
> 预计阅读时间: 10-15 分钟 | 面向角色: A

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：把“候选工程”和“正式主线”分开
* **改动背景与问题**：单摄 Demo 的历史 bitstream 曾显示 J48/ch0 真实画面，但候选仓库源码后来修改了白平衡。历史车票不能证明今天这趟车也到站。
* **源码剖析**：[CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L43-L55) 将 `competition_project_single_camera/` 定义为隔离候选；[主方案](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md#L34-L41) 明确了新构建、匹配 bitstream、烧录与板级复现升格门。
* **经验总结**：像换过发动机的赛车，旧圈速只能证明旧配置；源码、bitstream 和板上现象必须三者同批次绑定。

### 📌 核心点二：白平衡除法必须在空帧时安全
* **改动背景与问题**：`avg = sum / pixel_cnt` 即使只在有效帧使用，也不应让组合除法器面对零分母。
* **源码剖析**：[white_balance.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/uvc_src/white_balance.v#L95-L97) 对 R/G/B 三路都加入 `pixel_cnt == 0` 防护；[delta manifest](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/docs/baseline/m0_post_baseline_delta_20260714.csv#L1-L2) 把初始快照、M0-09 和合并后状态分开记录。
* **经验总结**：这是给除法器装“空载保护开关”；没有像素时输出零，不让未定义行为污染综合或仿真。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：`constrain.sdc` 是 Interface Designer 自动生成文件，不能为了让 diff 好看而格式化；IP `settings.json` 虽已改成相对 `base_path`，下次重新生成前仍须在 GUI 核对目录。
* **通用方法论**：约束清点分三层：文件存在、工程引用可解析、真实 Efinity flow/板级现象。前两层通过不能替代第三层。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：Efinity Interface Designer、SDC 时钟约束和现有 M0 人工构建反馈表。
  * **应用与意义**：按同一源码提交生成并记录 Map/PNR/STA/bitstream，避免历史产物错配。
* **拓展基础知识**：
  * **推荐学习内容**：Verilog 除零语义、自动白平衡帧统计与 CDC。
  * **应用与意义**：理解为什么静态防护、帧结束锁存和板级画面必须同时验证。
