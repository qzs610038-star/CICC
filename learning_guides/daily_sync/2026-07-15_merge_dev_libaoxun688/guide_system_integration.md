# 学习指南：系统集成与维护 - 把“已合并候选”与“已上板闭环”分开管理
> 生成日期: 2026-07-15 | 对应分支合并: `dev/libaoxun688@8b340a7` / PR #6
> 预计阅读时间: 10-15 分钟 | 面向角色: C

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：固定 SHA 合并，防止个人分支在验收后继续漂移

* **改动背景与问题**：lib 个人分支相对共同基线有 3 个独有提交，但 `main` 已包含更新的 CPU/myCobot 工作。直接在脏工作区合并容易混入本机配置或覆盖并行成果。
* **源码剖析**：本次先把批准分支固定到 `8b340a728c56e6defe46b6fb7b30f55bb520f787`，再通过 PR #6 合并；8 个改动文件全部限定在 `competition_project_single_camera/`。候选的连续事实总账位于 [WORK_LOG.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/WORK_LOG.md#L1-L34)。
* **经验总结**：固定 SHA 像给验收样品贴封条；分支名只是货架标签，随时可能换货。审查、合并和复现都应指向封条编号。

### 📌 核心点二：证据有保质期，源码一变就要重新盖章

* **改动背景与问题**：M0-27 曾取得 WNS `+1.674 ns`、WHS `+0.026 ns` 与 CDC PASS，但 M0-28 随后修改 HDMI 行相位和固定点白平衡。
* **源码剖析**：较早 PNR 与五色失败结论见 [WORK_LOG.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/WORK_LOG.md#L604-L623)；最终源码修改仍标记为 `MAP-PNR-BOARD NOT VERIFIED`，见 [WORK_LOG.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/WORK_LOG.md#L626-L641)。
* **经验总结**：时序报告像食品检验章，只对送检批次有效。配方又变了，就必须重新送检；不能把昨天的 WNS 贴到今天的 bitstream 上。

### 📌 核心点三：合入 main 只是共享候选，不是路线升格

* **改动背景与问题**：`competition_project_single_camera/` 是隔离候选。合入便于团队共同审查和继续构建，但不自动替代 `final_project/`。
* **源码剖析**：默认 DSI 隔离和 HDMI-only 逻辑位于 [top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L1354-L1478)；这只说明源码选择，不证明最终器件、bitstream 或板上画面。
* **经验总结**：把候选方案放进公共工具箱，不等于把它装上比赛机器。升格还需要相同 commit 的 Map/PNR、bitstream 哈希、烧录、冷启动和稳定画面证据。

## 2. 可迁移的系统级工程经验

* **避坑警示**：PR 描述必须同时写“通过什么”和“没有通过什么”。尤其不能把 `TIMING PASS`、`BOARD IMAGE FAIL` 与后续 `SOURCE PATCHED` 三个不同批次揉成一句“已上板通过”。
* **通用方法论**：维护证据链 `commit → 源码哈希 → 构建报告 → bitstream 哈希 → 板级现象`；任一环断裂，状态降为 `NOT VERIFIED`。

> **风险标注：代码审查通过、运行时未触发。** 最终 tip 未完成新 PNR/bitstream/五色板测；真实机械臂接线、烧录和动作仍由独立安全门控制。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：开发板烧录流程、Efinity Programmer、Timing/CDC 报告和赛题四任务录像签字要求。
  * **应用与意义**：让每次板测都能绑定可复现的 commit 与 bitstream，现场异常可快速回退。

* **拓展基础知识**：
  * **推荐学习内容**：Git merge-base、固定 SHA PR、可追溯构建 manifest 与软件物料清单思想。
  * **应用与意义**：把多人并行开发从“谁的分支最新”升级为“哪个固定制品经过了哪些门”。
