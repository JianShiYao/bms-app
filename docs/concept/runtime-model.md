# BMS 运行时模型 v0（设计契约）

> **定位**：本文细化 [architecture.md](architecture.md) 的 ADR-ARCH-003（运行时由 `bms_task` 集中调度），是运行时的**权威设计契约**——规定 `bms_task` / `bms_time` / `bms_sys_mon` / watchdog 的**目标形态**。**agent 据此实现或重构代码，代码向本契约对齐**；本文不描述现状实现。
>
> **现状与差距**不在此维护：见 [architecture.md](architecture.md) §11 迁移路径（本契约主要在 **M2** 与 **M5** 落地）。
>
> **规范措辞**：**必须 / 应 / 不得** 表示契约要求，可在评审、实现、测试中引用。相关：数据契约 [data-model.md](data-model.md)、接口 [module-interface.md](../standard/module-interface.md)、诊断 [diagnostics-fault-model.md](diagnostics-fault-model.md)。

## 1. 运行时设计原则

- **无自建长期线程**：业务模块不得各自创建无限循环线程；所有长期运行逻辑由 `bms_task` 的**集中任务表**声明并启动。
- **一处声明**：任务的名称、类别、周期/触发、优先级、栈预算、WCET（最大运行时间）上限、心跳超时，必须在任务表一处集中声明，供 `bms_sys_mon` 监控与审计。
- **任务体只做编排**：读 `bms_db` → 调纯函数 → 写 `bms_db` / 发通知；不得在任务体内嵌复杂业务；safety 任务不得调用可能阻塞的接口（见 §5）。
- **失效安全先行**：任务框架初始化即写入初始 `DB_BMS_STATE = INIT / 接触器 OPEN`；任何任务异常的最终反应必须是保持或回到安全态。
- **优先级铁律**：Zephyr 中优先级数值越小越高；**safety cyclic 必须严格高于 app / comm / background**。优先级是**相对顺序契约**，具体数值在任务表/Kconfig 一处声明。

## 2. 时间基准（`bms_time`）

- 全系统**唯一时间源**为 `bms_time_now_ms()`（单调递增毫秒）。业务模块与纯函数**不得**直接调用内核时间 API；时间访问必须经 `bms_time`，以便测试注入与将来切换更高精度基准。
- 所有时间比较**必须用有符号差值**以回绕安全：`(int32_t)(now - deadline) >= 0`，**不得**对时间戳做无符号大小直接比较。
- `now_ms` 必须**可注入**：周期到期、超时、stale、心跳判定都应能以注入时间脱离内核单测。

## 3. 任务表（契约）

下表是 agent 必须实现的**规范任务集**。周期为设计默认值（由 `CONFIG_BMS_TASK_*` 声明、可按板调）；栈为**预算目标**（真机以 `CONFIG_THREAD_ANALYZER` 实测收敛）；WCET 上限须实现声明并由 `bms_sys_mon` 监控。

| 任务 | 类别 | 周期/触发 | 优先级(相对) | 栈预算 | 职责（读 → 纯函数 → 写） |
|------|------|-----------|:--:|:--:|------|
| `tsk_safety` | safety cyclic | 10 ms（默认） | 最高（应用层） | ~1.5 KB | 采样消化 → `DB_CELL_MEAS`；保护判定 + 主状态机 + 接触器期望态 → `DB_PROT_STATE`/`DB_BMS_STATE` |
| `tsk_app` | app cyclic | 100 ms（默认） | 中 | ~1.5 KB | SOC/SOH 估算、均衡策略、周期上报（TX 快照）、诊断老化 |
| `tsk_comm_rx` | blocking | 队列/中断事件 | 低于 safety | 按需 | 解析 CAN/上位机命令 → `DB_COMMAND`（**不得**在此直接改 BMS 状态） |
| `tsk_background` | background | 低频（如 5 s） | 最低 | ≤1 KB | 健康打印、低优先级维护 |
| `tsk_fast`（可选） | fast cyclic | ≤1 ms | 最高 cyclic | 小 | 仅在安全需求 + 硬件支持时启用的时间敏感采样/快诊断 |
| zero-latency path | 硬件中断 | AFE ALERT/短路比较器 | 内核保留最高 | ISR | 只做接触器强制 OPEN + 置 latch/event，随后由 diag/bms 接管（M6） |

**推荐优先级顺序**（数值实现时定，须满足）：`zero-latency` > `tsk_fast` > `tsk_safety` > `tsk_app` > `tsk_comm_rx` > `tsk_background`。

## 4. 调度策略

- **绝对节拍**：安全相关周期任务必须以**绝对 deadline 推进**（基于 `bms_time_now_ms()` 或 `k_timer`），保证无累积漂移；**不得**用「工作耗时 + 相对 sleep」导致周期随负载漂移。
- **WCET 声明与监控**：每个被监控任务必须声明最大运行时间上限；进入/退出打时间戳（§6），超限记 `bms_diag`。
- **到期判定为纯函数**：`is_due(now, &next, period)` 应为无副作用纯函数（输入注入时间），可直接 ztest；落后过多时须把 `next` 重置到 `now + period` 以避免回绕后疯狂追赶。

## 5. blocking task 与 safety cyclic 隔离（契约）

- blocking task（CAN RX、NVM/flash、日志）**优先级必须低于 safety cyclic**，且只能通过**阻塞等待 / 队列 / 异步接口**让出 CPU，**不得忙等或长期占用**。
- **safety cyclic 不得调用任何可能阻塞的接口**（NVM 写、同步 CAN 发送、文件 I/O）；需要时只向 blocking task **投递请求**（队列/event）后立即返回。
- blocking task 崩溃/卡死**不得**阻止 safety 链推进或 watchdog 判定——由 `bms_sys_mon` 通过心跳独立发现。
- 外部命令通道（CAN RX）必须在 blocking task 内解析并写 `DB_COMMAND`；BMS 状态迁移只由 `bms_bms` 在 safety cyclic 内消费 `DB_COMMAND` 完成。

## 6. `bms_sys_mon`：心跳与运行时间（契约）

- 每个被监控的 cyclic 任务在**进入/退出**调用 `bms_sys_mon_enter(id)` / `bms_sys_mon_exit(id)`，记录：`last_seen_ms`（心跳）、本次运行时间、峰值运行时间。
- 判据：**心跳超时**（超过 N×周期未 enter）、**运行超时**（> 声明 WCET）、**栈余量不足** → 生成对应 `bms_diag` 条目。
- 聚合写入 **`DB_TASK_HEALTH`** entry（owner = `bms_sys_mon`，见 [data-model.md](data-model.md)），供 diag / sys / comm 消费。
- `bms_sys_mon` 自身必须高优先级、极简、不阻塞，以保证能观察到其他任务失联。

## 7. watchdog 喂狗门控（契约）

- **只有 engine/monitor 统一喂硬件 watchdog**；业务任务**不得**直接喂狗。
- **门控条件**：仅当所有**安全关键任务心跳健康**（safety cyclic 未超时、未运行超限）时才喂狗；任一安全任务失联/超时 → **停止喂狗**，让 watchdog 复位进入上电安全态。
- **软先于硬**：喂狗停止是最后一道兜底；在此之前 `bms_sys_mon → bms_diag → bms_bms` 应已尝试进入 FAULT/LOCKED（软失效安全先行）。
- watchdog 经 wrapper + devicetree 访问；native_sim/QEMU 下可桩化或关闭。

## 8. stale（数据过期）判定（契约）

- **stale 定义**：`bms_time_now_ms() − entry.timestamp_ms > 期望周期 × 容忍系数`；辅以 `sequence` 变化检测丢帧/重复。各 entry 的**期望周期与容忍系数**在 [data-model.md](data-model.md) 逐条定义。
- **消费即校验**：任何消费者读取 DB entry **必须**检查 `validity` 与 stale；**stale 或无效数据不得作为 NORMAL 依据**（失效安全）——例如电流无效/过期时 SOC 不积分、保护不据此闭合接触器。
- stale 命中必须触发 `bms_diag`（如测量过期）。
- 比较同样用有符号差值（§2）。

## 9. 可测性约束

- 任务体只做编排；核心逻辑为纯函数（`bms_*_evaluate` / `_step` / `bms_next_state` / `_compute` 等），可脱离线程/硬件/zbus 单测。
- 一切时间相关判定（到期、stale、心跳、超时）以**注入 `now_ms`** 单测；`bms_sys_mon` / watchdog 门控判定同样以注入时间可测。

## 10. 迁移

本契约的落地阶段见 [architecture.md](architecture.md) §11：
- **M2**：`bms_task` 任务表、`bms_time` 时间基准、绝对节拍、blocking 隔离。
- **M5**：`bms_sys_mon` 心跳/运行时间监控、watchdog 门控。

迁移期允许过渡实现与目标契约共存，但**目标以本契约为准**；每步须保持既有 CI 与 ztest 通过。

## 11. 参考

- [architecture.md](architecture.md) §6（ADR-ARCH-003）、§11、§12。
- foxBMS 2 FTASK / System Monitoring（链接见 concept-architecture §13）。
