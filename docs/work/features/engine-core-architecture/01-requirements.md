# Engine Core 需求规格

| 字段 | 值 |
|---|---|
| 文档 ID | REQ-DOC-ENG |
| 版本 | 0.1（草稿） |
| 范围 | `bms_task` / `bms_db` / `bms_diag` / `bms_bms` |
| 状态 | 草稿 |

## 需求列表

### REQ-ENG-001 database typed snapshot 交换

| 属性 | 内容 |
|---|---|
| 类型 | 接口 / 架构 |
| 优先级 / 安全等级 | 高 |
| 来源 | `architecture.md` database 原则 |
| 验证方法 | 测试 |
| 关联设计 | DES-ENG-001 |
| 关联测试 | `bms.integration.test_db_write_read_snapshot`（已补；待执行型验证） |
| 状态 | 已实现 / 待验证 |

**需求描述**
> 系统应通过 `bms_db` 以 typed snapshot 的方式交换 cell measurement、SOC、protection、BMS state 等核心数据。

**理由**
> foxBMS 2 inspired 架构要求 database 成为模块间数据交换中心，避免模块互相直接依赖或共享裸全局变量。

**验收标准**
- Given 一个 `struct bms_cell_meas`，When 写入并读取 `bms_db`，Then 读回值应与写入值一致。
- Given 任一 entry 第一次写入，When 读取 meta，Then `valid == true` 且 `sequence > 0`。

### REQ-ENG-002 database entry 单一写入者

| 属性 | 内容 |
|---|---|
| 类型 | 架构约束 |
| 优先级 / 安全等级 | 高 |
| 来源 | `module-interface.md` |
| 验证方法 | 检视 |
| 关联设计 | DES-ENG-001 |
| 关联测试 | — |
| 状态 | 已实现 / 待检视 |

**需求描述**
> 每个 `bms_db` entry 应有唯一逻辑 owner，消费者只能读取值拷贝。

**理由**
> 多写入者会导致状态来源不清，安全诊断和 BMS 状态机难以审计。

**验收标准**
- Given `DB_CELL_MEAS`，When 检视设计和代码，Then 只有 measurement/task pipeline 负责写入。
- Given `DB_BMS_STATE`，When 检视设计和代码，Then 只有 BMS state machine pipeline 负责写入。

### REQ-ENG-003 诊断中心聚合故障

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 优先级 / 安全等级 | 高 |
| 来源 | `safety.md` / `architecture.md` |
| 验证方法 | 测试 |
| 关联设计 | DES-ENG-002 |
| 关联测试 | `bms.integration.test_diag_error_blocks_normal`（已补；待执行型验证） |
| 状态 | 已实现 / 待验证 |

**需求描述**
> 当任一模块报告 ERROR 或 CRITICAL 诊断时，`bms_diag` 应保存 active/latched 状态并对 BMS 状态机可见。

**理由**
> 安全相关故障不得只停留在日志或模块私有状态，必须成为系统状态机输入。

**验收标准**
- Given 无诊断，When 调用 `bms_diag_has_error()`，Then 返回 false。
- Given 报告 ERROR active，When 读取诊断状态，Then `max_severity >= ERROR` 且 `active_mask` 包含对应 bit。
- Given 报告 CRITICAL latch，When 读取诊断状态，Then `latched_mask` 包含对应 bit。

### REQ-ENG-004 BMS 主状态机集中决定接触器期望态

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 优先级 / 安全等级 | 高 |
| 来源 | `safety.md` SG-06/SG-12，`architecture.md` BMS/SYS 状态机 |
| 验证方法 | 测试 |
| 关联设计 | DES-ENG-003 |
| 关联测试 | `bms.integration.test_bms_fault_opens_contactor`（已补；待执行型验证） |
| 状态 | 已实现 / 待验证 |

**需求描述**
> BMS 主状态机应集中决定系统状态和接触器期望态；除硬件零延迟安全路径外，业务模块不得直接闭合接触器。

**理由**
> 接触器是安全关键执行器，必须有唯一 owner，避免分布式决策导致不可验证行为。

**验收标准**
- Given 无故障且 close_allowed=true，When 状态机从 INIT/STANDBY 运行，Then 可进入 NORMAL，接触器期望态为 CLOSED。
- Given protection 非 NORMAL 或诊断 ERROR，When 状态机运行，Then 状态为 FAULT/LOCKED，接触器期望态为 OPEN。

### REQ-ENG-005 失效安全默认态

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 优先级 / 安全等级 | 高 |
| 来源 | `safety.md` 运行模式与安全态 |
| 验证方法 | 测试 |
| 关联设计 | DES-ENG-003 |
| 关联测试 | `bms.integration.test_bms_default_open`（已补；待执行型验证） |
| 状态 | 已实现 / 待验证 |

**需求描述**
> 当输入无效、诊断达到 ERROR/CRITICAL、硬件故障锁存或 protection 非 NORMAL 时，BMS 应禁止进入 NORMAL 并保持接触器 OPEN。

**理由**
> BMS 安全默认态是接触器 OPEN；任何不确定性都不得导致闭合。

**验收标准**
- Given `bms_next_state()` 输入为 NULL，When 调用状态转移，Then 返回 FAULT。
- Given `diag.max_severity >= ERROR`，When 计算下一状态，Then 不得返回 NORMAL。
- Given `hw_fault_latched=true`，When 计算下一状态，Then 返回 LOCKED。

### REQ-ENG-006 任务框架统一调度长期运行逻辑

| 属性 | 内容 |
|---|---|
| 类型 | 架构 / 实时性 |
| 优先级 / 安全等级 | 高 |
| 来源 | foxBMS 2 FTASK 思路，`module-interface.md` |
| 验证方法 | 检视 / 集成测试 |
| 关联设计 | DES-ENG-004 |
| 关联测试 | `bms.integration.test_task_pipeline_smoke`（待补） |
| 状态 | 已实现 / 待验证 |

**需求描述**
> 长期运行逻辑应由 `bms_task` 统一调度，业务模块不得自行创建未登记的长期线程。

**理由**
> 统一任务框架才能集中审计周期、优先级、阻塞行为和 watchdog 心跳。

**验收标准**
- Given 业务模块 `afe/soc/protection/balancing/comm`，When 检视代码，Then 不应存在模块私有 `K_THREAD_DEFINE` 长期循环。
- Given task framework 初始化，When 系统运行，Then safety/app/background task 由 `bms_task_init()` 统一启动。

### REQ-ENG-007 兼容 zbus 过渡层

| 属性 | 内容 |
|---|---|
| 类型 | 兼容 / 迁移 |
| 优先级 / 安全等级 | 中 |
| 来源 | 当前代码迁移路径 |
| 验证方法 | 构建 / 检视 |
| 关联设计 | DES-ENG-005 |
| 关联测试 | 现有 `bms.*` 单测构建 |
| 状态 | 已实现 |

**需求描述**
> 迁移期间系统可继续发布现有 zbus channel 作为兼容层，但目标模块契约应以 `bms_db` 为准。

**理由**
> 保留 zbus 兼容层可降低迁移风险，避免现有测试和临时代码一次性失效。

**验收标准**
- Given 新 engine pipeline 写入 DB，When 需要兼容旧消费者，Then 可同步发布 `chan_cell_meas`、`chan_soc`、`chan_prot_state`。
- Given 新增目标架构模块，When 设计接口，Then 不应把 zbus 作为主要模块契约。
