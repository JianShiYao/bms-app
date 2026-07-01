# Engine Core 架构说明

| 字段 | 值 |
|---|---|
| 文档 ID | ARCH-ENG-001 |
| 版本 | 0.1（草稿） |
| 覆盖需求 | REQ-ENG-001..007 |
| 状态 | 草稿 |

## 1. 范围

本特性定义 engine core 的目标架构：

- `bms_db`：typed snapshot database。
- `bms_diag`：诊断中心。
- `bms_bms`：主状态机纯逻辑。
- `bms_task`：Zephyr 上的任务框架。

不覆盖：

- 接触器 GPIO 真机驱动。
- WDT/HIL/NVM 持久化。
- 完整 FMEA/HARA。

## 2. 架构上下文

```
Measurement / Application modules
        │
        ▼
     bms_task
        │
        ├── calls pure/service functions
        │
        ├── writes/reads bms_db
        │
        ├── reports bms_diag
        │
        └── runs bms_bms state transition
```

`bms_task` 是运行时编排者；`bms_db` 是数据交换中心；`bms_diag` 是故障聚合中心；`bms_bms` 是接触器允许条件的唯一软件 owner。

## 3. 架构决策

### ADR-ENG-001 database 作为模块契约

**决策**：目标模块契约使用 `bms_db_read/write`，zbus 仅作为兼容/通知层。

**理由**：

- database entry 能明确 owner 与消费者。
- 读写 API 比散落 channel 更容易做检视、追溯和集成测试。
- 贴近 foxBMS 2 database 思想。

**影响**：

- 新模块设计必须先声明 DB entry owner。
- 旧 zbus channel 逐步退化为兼容层。

### ADR-ENG-002 诊断集中登记

**决策**：安全相关故障进入 `bms_diag`，不得只在模块内部消化或只打日志。

**理由**：

- BMS 状态机需要统一故障视图。
- 安全案例证据需要诊断来源、严重度、锁存策略可追溯。

### ADR-ENG-003 BMS 状态机集中拥有接触器期望态

**决策**：`bms_bms` 根据诊断、保护、命令、硬件 latch 等输入计算主状态和接触器期望态。

**理由**：

- 接触器是安全关键执行器，必须避免多模块分布式决策。
- `bms_next_state()` 可做纯函数测试。

### ADR-ENG-004 Zephyr task framework 而非业务模块自启动线程

**决策**：长期运行逻辑集中到 `bms_task`，用 Zephyr 静态线程实现 safety/app/background task。

**理由**：

- 保留 Zephyr 原生调度与 Kconfig 能力。
- 任务周期、优先级、栈和健康监控可以集中审计。
- 贴近 foxBMS 2 FTASK 的工程组织方式。

### ADR-ENG-005 过渡期保留 zbus 兼容发布

**决策**：在 engine pipeline 写 DB 后，可继续发布现有 `chan_cell_meas` / `chan_soc` / `chan_prot_state`。

**理由**：

- 降低迁移风险。
- 保持现有测试/调试路径可用。
- 给后续逐步删除 zbus 契约留出空间。

## 4. 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| DB 与 zbus 双写期间出现双真相 | 消费者读到不同来源 | 文档规定 DB 为目标契约；新增模块不得依赖 zbus |
| `bms_task` 无限循环难以单测 | 集成测试困难 | 后续抽出 `run_once` 内部函数或 test seam |
| 诊断 ID 过少 | 故障语义不够 | 后续补 `standard-diagnostics.md` |
| BMS 状态机过于简化 | PRECHARGE/LOCKED 恢复不完整 | 先以安全默认态闭环，后续小 V 扩展 |

## 5. 验证策略

- 单元测试：`bms_next_state()`、`bms_diag_report()`、`bms_db_read/write()`。
- 组件集成：DB 写入测量 → protection fault → diag ERROR → BMS FAULT/LOCKED → contactor OPEN。
- 构建检视：确认业务模块不再自启动长期线程。

