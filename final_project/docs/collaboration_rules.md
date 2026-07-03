# 团队协同与代码管理规范 (Vision & Arm)

为了减少并行开发时的代码冲突，并保证项目能够高效、稳定地在 GitHub 上推进，特制定以下协同规则。我们的核心思想是：**物理目录隔离、接口提前约定、分支独立开发、合并必须审查**。

## 1. 物理结构与架构解耦

最有效的防冲突手段是“各写各的文件”。

*   **目录隔离：** 严格划分视觉与机械臂的代码存放位置。
    *   视觉相关（FPGA图像处理、CPU端识别算法）：统一放在 `src/vision/`（或对应工程目录）。
    *   机械臂相关（通信协议、运动学、动作控制）：统一放在 `src/arm/`（或对应工程目录）。
*   **公共模块的修改：** 
    *   像 `top.v`（顶层连线）、`constrain.sdc`（管脚约束）、`mem_test.xml`（工程配置）、主控脚本（如 `main.py` 或 `main.c`）等公共文件，尽量保持精简。
    *   **谁要改公共文件，必须提前在交流群里通知对方！**

## 2. 接口契约先行 (Interface Contract)

在各自开始闷头写代码之前，**必须先坐下来把双方的接口定死**。

*   **数据结构一致：** 明确视觉模块传给机械臂模块的数据格式。例如：是直接传像素坐标 `(x, y)` 还是物理坐标 `(X, Y, Z)`？是否附带类别信息、置信度？使用什么协议（UART、共享内存等）？
*   **启用 Mock（桩数据）开发：**
    *   **机械臂负责同学：** 不要等视觉识别做好了才开始调试动作。自己写一个生成随机或固定坐标的 `dummy_vision_node`，用来测试机械臂的抓取。
    *   **视觉负责同学：** 不要等机械臂连通了才开始测识别。将识别结果打印到终端或保存到日志，验证精度即可。

## 3. Git 分支管理策略

采用简化的 Git Flow 策略。严禁任何人在 `main` (或 `master`) 分支上直接写代码。

*   **`main` 分支：** 稳定、可演示的版本。任何时候从 `main` clone 下来的代码都必须是能编译通过并运行的。
*   **功能分支 (Feature Branch)：**
    *   所有的开发都在功能分支上进行。
    *   命名规范：
        *   视觉同学：`feature/vision-[功能描述]` (例如：`feature/vision-color-detect`)
        *   机械臂同学：`feature/arm-[功能描述]` (例如：`feature/arm-uart-comm`)
        *   公共架构调整：`chore/[功能描述]` 或 `refactor/[功能描述]`
*   **工作流：**
    1. 每天开始工作时：`git checkout main` -> `git pull` -> `git checkout -b feature/your-branch-name`
    2. 开发并提交：`git add .` -> `git commit -m "feat: add uart driver for arm"`
    3. 推送到远端：`git push origin feature/your-branch-name`

## 4. 合并与避免冲突规范

*   **拉取合并请求 (Pull Request / PR)：** 当你的功能分支开发完成并在本地测试通过后，在 GitHub 上向 `main` 分支发起 Pull Request。
*   **强制 Code Review：** 
    *   **视觉的 PR 必须由机械臂同学 approve 才能合并，反之亦然。** 
    *   Review 的重点不是去死抠对方的业务逻辑，而是看**是否修改了公共文件**、**是否破坏了约定的接口**。
*   **解决冲突：** 如果发起 PR 时发现与 `main` 分支冲突，**由发起 PR 的人负责解决冲突**。先在本地 `git pull origin main` 合并到自己的分支，解决完冲突再 push。

## 5. 小步快跑与环境同步

*   **高频提交：** 每天至少保证有可以运行的阶段性成果 push 到 GitHub，不要攒着一个星期才提一个巨大的 PR，那样一旦冲突将非常痛苦。
*   **环境一致性：** Efinity 版本、Python 版本及第三方库（如 `pymycobot`）版本必须在 README 或 `requirements.txt` 中严格写明。不要因为两人环境不一致导致互相覆盖配置。
