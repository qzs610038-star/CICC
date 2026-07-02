我没有在当前目录顶层找到明确的 `FPGA + myCobot 280 + MCP` 实施方案文件；下面按“用 MCP 暴露工具，由 AI/上位机协调 FPGA 与 myCobot 280”的典型方案做差距分析。MCP 本身是连接 AI 应用与外部工具/数据源的协议，不是实时控制总线或安全控制层；官方定位也是让 AI 应用连接工具、数据源和工作流。参考：MCP 官方介绍：https://modelcontextprotocol.io/docs/getting-started/intro

**核心结论**
当前方案若把 MCP 当作“机械臂控制协议”，风险偏高；若把 MCP 定位为“任务编排/诊断/参数下发/状态查询接口”，再由本地确定性控制层负责运动、安全和 FPGA I/O，则方向可行。

**主要差距**

1. **实时控制边界不清**
   MCP/LLM 调用链路延迟不可控，不能承担闭环伺服、急停、轨迹插补、碰撞保护。  
   需要明确：MCP 只发高层命令，如 `plan_motion`、`validate_motion`、`execute_trajectory_id`；实时环放在 myCobot 控制器、FPGA 或本地控制进程内。

2. **FPGA 角色定义不足**
   方案必须说明 FPGA 是做传感器采集、脉冲/串口桥接、视觉预处理、硬件互锁，还是直接参与运动控制。  
   如果只是控制 myCobot 280，通常不应绕过原厂控制器直接驱动关节；FPGA 更适合作为低延迟 I/O、安全互锁、同步触发和状态采集层。

3. **机械臂运动安全缺口**
   需要补齐关节限位、速度/加速度限制、工作空间围栏、负载限制、软急停、硬急停、看门狗、通信丢失停机策略。  
   MCP 工具必须默认 `dry_run`，危险动作需要二阶段确认：先生成轨迹和风险报告，再执行。

4. **工具接口粒度过粗或过危险**
   不建议暴露 `send_raw_command`、`set_joint_angles` 这类裸工具给模型直接调用。  
   推荐工具分层：
   - `get_robot_state`
   - `get_fpga_status`
   - `validate_pose`
   - `plan_joint_trajectory`
   - `simulate_trajectory`
   - `arm_enable`
   - `execute_approved_trajectory`
   - `stop_motion`

5. **坐标系与标定缺失**
   必须定义 base frame、tool frame、camera frame、FPGA 传感器坐标、单位、角度制、右手系/左手系、TCP 偏置。  
   没有手眼标定、TCP 标定和坐标转换测试，MCP 层再漂亮也无法保证真实运动正确。

6. **状态机缺失**
   机械臂系统需要显式状态机：`INIT -> HOMED -> IDLE -> PLANNING -> ARMED -> EXECUTING -> PAUSED/FAULT -> RECOVERY`。  
   MCP 工具应检查状态，不允许跨状态执行，例如未归零、未使能、故障未清除时禁止运动。

7. **错误语义不够工程化**
   MCP 返回不能只写“失败”。需要结构化错误：
   - `SAFETY_LIMIT`
   - `IK_UNREACHABLE`
   - `SERIAL_TIMEOUT`
   - `FPGA_WATCHDOG_EXPIRED`
   - `JOINT_LIMIT_EXCEEDED`
   - `ESTOP_ACTIVE`
   并给出可恢复/不可恢复、是否需要人工确认。

8. **FPGA 工程验证不足**
   需要补齐 HDL testbench、时序约束、CDC 检查、复位策略、串口/SPI/CAN 协议仿真、硬件在环测试。  
   如果 FPGA 参与安全互锁，必须证明故障默认输出是安全态，而不是依赖上位机正常退出。

9. **观测与审计不足**
   每次 MCP 调用应记录：调用者、参数、审批状态、轨迹摘要、执行时间、机器人状态快照、FPGA 状态、异常码。  
   运动执行日志要能 replay，用于事故复盘和调参。

10. **安全与权限模型不足**
   MCP 工具面向 AI 调用，必须考虑提示注入和越权调用。  
   建议本地白名单、最小权限、危险工具人工确认、参数范围校验、工具描述避免诱导模型绕过安全流程。

**建议的目标架构**

`MCP Client / AI Agent`  
-> `MCP Server: task tools, validation tools, read-only resources`  
-> `Safety Broker: schema check, state check, approval, rate limit, workspace limit`  
-> `Robot Controller Service: IK, trajectory planning, pymycobot/serial control`  
-> `FPGA Service: low-latency I/O, interlock, sync, watchdog`  
-> `myCobot 280 + sensors + E-stop`

**验收标准**
最低可验收版本应做到：

- MCP 只能执行白名单工具，不能发送裸硬件命令。
- 所有运动命令先 dry-run，输出轨迹、预计耗时、关节范围、风险判断。
- 急停、串口断连、FPGA watchdog 超时均能进入安全态。
- 有 10 条以上 HIL 测试用例覆盖正常运动、越界、断连、急停、错误恢复。
- 日志能完整还原一次动作从 MCP 调用到机械臂响应的全过程。

整体判断：作为“AI 编排 + 本地控制”的 MCP 方案可行；作为“AI/MCP 直接控制机械臂和 FPGA 实时动作”的方案不合格，需要重构边界。
