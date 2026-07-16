# 学习指南：FPGA 与视频前端 - Hard SoC 真源落库与证据批次
> 生成日期: 2026-07-16 | 对应分支合并: `dev/libaoxun688-hard-soc-source-sync-20260716@0604d33`
> 预计阅读时间: 10-15 分钟 | 面向角色: A

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：工程从“拿到一张照片”变成“拿到可复建的施工图”

* **改动背景与问题**：旧提交有来源副本的 PNR、bitstream 和 J48/ch0 视频证据，却缺少仓库内 Hard SoC IP、`.peri.xml`、BSP 和 Hello。它像只交付一栋房子的验收照片，却没交施工图，其他人无法确认重建的是不是同一栋楼。
* **源码剖析**：工程 XML 现在同时登记 [feature_stats_tap 与 Hard SoC IP](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/mem_test.xml#L43)，并关闭 [debugger auto_instantiation](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/mem_test.xml#L165)。Hard SoC 的可复现参数入口是 [settings.json](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/ip/EfxSapphireHpSoc_slb/settings.json#L1)。
* **经验总结**：对 FPGA 工程，“能运行的 bitstream”与“能复建的真源”是两种证据。前者证明某一次运行，后者才允许团队持续维护。

### 📌 核心点二：DDR 配置只有一名司机，SoC 只看红绿灯

* **改动背景与问题**：若视频状态机和 SoC wrapper 同时驱动 DDR `CFG_START/RST/SEL`，就像两名司机同时抢方向盘，仿真不一定暴露，上板却可能随机失效。
* **源码剖析**：[顶层注释与 Hard SoC 实例](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L621)明确视频状态机仍是配置端口唯一驱动者，SoC 只读取 `CFG_DONE`；[最终连续赋值](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L769)保持该所有权。
* **经验总结**：系统级控制线必须有唯一 owner。跨子系统集成时，先画“谁写、谁读”，再看功能是否正确。

### 📌 核心点三：旧板测与新构建不能混成一个 PASS

* **改动背景与问题**：来源 bitstream `AA1338...` 有真实视频证据；Codex 从仓库真源重新生成的是另一个 bitstream。二者即使时序数值相同，也不能共享板测身份。
* **源码剖析**：[UART0 引脚与 USER1/USER2 资源](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/mem_test.peri.xml#L178)和 [JTAG USER2](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/mem_test.peri.xml#L427)已进入工程真源，但真实连接仍需板测。
* **经验总结**：证据像血型配对——工具版本、源码提交、bitstream 哈希和板级现象必须属于同一批次。当前结论是“离线构建 PASS，新 bitstream 板测 NOT VERIFIED”。

## 2. 可迁移的系统级工程经验

* **避坑警示**：Interface Check 的 4 个物理距离 warning，与 post-synthesis netlist 的 118 个 warning 是不同集合。不能用前者替代全工程 warning 统计，也不能把“无 CDC warning”写成“无任何 warning”。
* **通用方法论**：采用四层证据链：真源齐备 → 可重复构建 → 匹配 bitstream 上板 → 目标功能复现。任何层失败就停在该层，不向后借证据。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：复习 Efinity Interface Designer、Hard SoC 生成与 TJ375N529 JTAG USER TAP 配置，重点理解 `.peri.xml`、IP `settings.json` 与顶层端口的对应关系。
* **应用与意义**：下一次改 PLL、UART GPIO 或 USER TAP 时，可先核对资源所有权，避免把视频调试 USER1 和 CPU USER2 混用。
* **拓展基础知识**：学习可复现构建和制品 provenance（来源追踪），把 commit、工具版本、参数、哈希和板测日志看作一条不可拆分的证据链。
