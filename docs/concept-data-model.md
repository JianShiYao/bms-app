# BMS 数据模型基线 v0

> **定位**：本文定义 `bms_db` 的数据契约基线，承接 [concept-architecture.md](concept-architecture.md) 的 ADR-ARCH-002。它先冻结 entry、owner、header、validity、sequence、stale 和 copy-by-value 规则；具体 C API 可在后续实现中逐步贴合本文。
>
> **当前代码状态**：`app/include/bms/db.h` / `app/src/bms/engine/db.c` 已有 `bms_cell_meas`、`bms_soc`、`bms_prot_evt`、`bms_state_snapshot` 的线程安全读写、`sequence` 和 `valid` 元数据。本文是 M1 迁移前的目标契约，不要求一次提交完成全部实现。

## 1. 数据模型目标

- **单一 owner**：每个 database entry 只有一个写入者。
- **值快照**：读者只拿 copy-by-value，不持有 DB 内部可变指针。
- **元数据统一**：每个 entry 带时间、序号、有效性和来源信息。
- **过期可判定**：消费者能判断数据是否 stale，而不是默默使用旧值。
- **失效安全**：无效、过期、部分有效的数据不得成为闭合接触器的依据。
- **迁移兼容**：zbus 可作为通知/过渡层，但模块契约以 `bms_db` entry 为准。

## 2. v0 entry 集

| Entry | Owner | 主要消费者 | 当前对应类型/状态 | 说明 |
|---|---|---|---|---|
| `DB_CELL_MEAS` | `bms_meas` | protection, algorithm, balancing, comm, bms | 当前 `struct bms_cell_meas`，由 afe 过渡写入 | 可信化后的测量快照 |
| `DB_SOC_STATE` | `bms_algorithm` | bms, comm, diag | 当前 `struct bms_soc`，由 soc 过渡写入 | SOC/SOH/SOE 估算状态 |
| `DB_PROT_STATE` | `bms_protection` | diag, bms, comm | 当前 `struct bms_prot_evt` | 保护阈值判定，不直接代表接触器最终 owner |
| `DB_DIAG_STATE` | `bms_diag` | bms, sys, comm | 待补 | 聚合故障、严重度、锁存 |
| `DB_BMS_STATE` | `bms_bms` | comm, balancing, sys_mon | 当前 `struct bms_state_snapshot` | 主状态机状态与接触器期望态 |
| `DB_COMMAND` | `bms_comm_rx` / system input adapter | bms, sys | 待补 | 外部命令、维护复位、闭合/断开请求 |
| `DB_CONTACTOR_FB` | `bms_contactor` | bms, diag | 待补 | 接触器反馈、预充状态 |
| `DB_TASK_HEALTH` | `bms_sys_mon` | diag, sys, comm | 待补 | 任务心跳、运行时间、超时标志 |

### owner 规则

- 只有 owner 可以调用对应 write API。
- 非 owner 需要改变数据时，必须通过自己的 entry 表达请求或状态，由 owner 消化。
- owner 变更属于架构变更，必须更新本文、`standard-module-interface.md` 和追溯矩阵。
- 过渡期若由 `bms_task` 代写某 entry，必须在代码注释或设计文档中说明“代 owner”关系和退出条件。

## 3. Entry header

v0 目标 header：

```c
struct bms_db_header {
    uint32_t timestamp_ms;
    uint32_t sequence;
    uint32_t validity;
    uint16_t source;
    uint16_t flags;
};
```

字段语义：

| 字段 | 语义 | 规则 |
|---|---|---|
| `timestamp_ms` | 数据产生或 owner 确认时刻 | 用于 stale 判断；不是读取时刻 |
| `sequence` | 成功写入序号 | 每次成功写入递增；允许回绕，消费者只比较是否变化 |
| `validity` | 数据有效性位 | 0 表示无有效子项；各 entry 定义自己的 bit |
| `source` | 数据来源 | 例如 sim、adc、can、default、fallback、maintenance |
| `flags` | 通用标志 | stale hint、degraded、latched、estimated 等可后续分配 |

### 与当前实现的关系

当前 `struct bms_db_meta` 只有 `sequence` 和 `valid`。M1 可以先保持现状，但新增 entry 时应朝统一 header 靠拢。`valid` 可视为 `validity != 0` 的过渡布尔值。

## 4. Validity 规则

### 通用规则

- `validity == 0` 表示该 entry 不能作为安全正向依据。
- partial validity 必须显式表达；消费者只能使用自己依赖的有效位。
- 安全链消费者必须把缺失关键有效位解释为 fail-safe，而不是默认正常。
- validity 只表达“这份数据是否可信”，不表达“系统是否安全”；系统安全由 `bms_diag` / `bms_bms` 决定。

### `DB_CELL_MEAS` validity

当前测量有效位已经存在：

| Bit | 含义 | 消费者约束 |
|---|---|---|
| `BMS_MEAS_VALID_VOLTAGE` | 电压量可信 | protection 可做 OV/UV；balancing 可计算均衡 |
| `BMS_MEAS_VALID_CURRENT` | 电流量可信 | SOC 可积分；protection 可做 OC |
| `BMS_MEAS_VALID_TEMP` | 温度量可信 | protection 可做 OT |

`bms_bms` 或 `bms_protection` 不得因某些测量位缺失而闭合接触器。若闭合条件依赖该测量域，则对应 validity 必须有效。

### 其他 entry validity

- `DB_SOC_STATE`：应至少区分 SOC 数值有效、SOH 数值有效、估算处于 fallback/degraded。
- `DB_PROT_STATE`：应表达保护判定是否基于完整输入；输入不完整时必须输出 FAULT 或等价 fail-safe 状态。
- `DB_DIAG_STATE`：应表达诊断聚合状态是否已初始化。
- `DB_BMS_STATE`：应表达状态机是否已初始化、接触器期望态是否可信。

## 5. Sequence 规则

- 每个 entry 独立维护 `sequence`。
- `sequence` 只表示该 entry 成功写入次数，不表示全局时间顺序。
- 消费者可以缓存上次读到的 `sequence`，用于判断是否有新数据。
- `sequence` 回绕时仍允许使用“不等于上次值”判断变化；不得依赖单调差值做长期计数。
- 初始化后未写入的 entry 必须通过 meta/header 标识 invalid，不能靠 `sequence == 0` 推断。

## 6. Stale 规则

stale 表示数据曾经有效，但已超过消费者可接受年龄。

### 判断公式

消费者按自身需求判断：

```text
age_ms = now_ms - entry.header.timestamp_ms
stale = !entry_valid || age_ms > max_age_ms
```

### max age 来源

| Entry | 初始建议 | 说明 |
|---|---|---|
| `DB_CELL_MEAS` | 2-3 个测量周期 | protection 可更严格 |
| `DB_SOC_STATE` | 2-3 个算法周期 | 不应直接影响接触器安全闭合 |
| `DB_PROT_STATE` | 1-2 个 safety 周期 | BMS 状态机依赖它做安全决策 |
| `DB_DIAG_STATE` | 1-2 个 diag 周期 | 未初始化或 stale 时不得进入 NORMAL |
| `DB_BMS_STATE` | 1 个 BMS 周期 | 用于上报/监控 |
| `DB_COMMAND` | 按命令类型 | close request 应有短超时，maintenance 可不同 |
| `DB_CONTACTOR_FB` | 1-2 个 feedback 周期 | stale 视为诊断故障 |
| `DB_TASK_HEALTH` | 1 个 monitor 周期 | stale 视为任务健康未知 |

具体阈值属于配置/运行时模型，后续在 `concept-runtime-model.md` 或配置文档中细化。

### stale 的安全含义

- 安全链关键 entry stale 时，`bms_bms` 不得进入或保持 NORMAL。
- `DB_CELL_MEAS` stale 时，protection 必须输出 fail-safe 结果或触发诊断。
- comm 可以上报 stale 状态，但不得把 stale 数据包装成正常值。

## 7. Copy-by-value 与并发

### API 原则

- `bms_db_read_*()` 把完整 entry 拷贝到调用方提供的结构体。
- `bms_db_write_*()` 在临界区内一次性替换完整 entry 并更新 header/meta。
- DB 不返回内部指针。
- DB 不允许消费者在锁内执行回调或复杂逻辑。

### 锁与 ISR

- 常规 task 上下文可使用 mutex / spinlock / irq lock 等实现，但 API 必须保证读者拿到一致快照。
- v0 默认 **ISR 不直接写 DB**。硬件中断只设置最小 event/latch，后续由 task 写入 DB 和 diag。
- 若未来确需 ISR 写入某 entry，必须为该 entry 设计专用 lock-free 或 irq-safe API，并在本文登记。

## 8. DB 与 zbus 的关系

- DB 是模块契约；zbus 是过渡/通知机制。
- 过渡期允许 DB write 后发布 zbus 通知，或 zbus consumer 将旧 channel 映射到 DB entry。
- 新模块不得只定义 zbus channel 而不定义 DB entry owner。
- 迁移完成后，业务逻辑不得依赖“订阅某模块内部状态”作为主要数据流。

## 9. Fail-safe 数据纪律

- 未初始化 entry：视为 invalid。
- stale entry：视为 invalid 或 degraded，具体由消费者安全需求决定；安全闭合路径按 invalid 处理。
- partial validity：只允许使用有效子域；缺失安全关键子域时 fail-safe。
- fallback / estimated 数据必须通过 `source` 或 `flags` 标识，不得伪装成实测可信数据。
- 日志不是状态；诊断与状态机只能依赖 DB entry、硬件 latch 或明确的 event。

## 10. M1 落地步骤

1. 在 `bms_db` 中登记 v0 entry enum / table，保留现有 typed API。
2. 为已有四类 entry 补齐 header 语义，至少保持 `sequence`、`valid`、`timestamp_ms` 一致。
3. 明确 `DB_CELL_MEAS`、`DB_SOC_STATE`、`DB_PROT_STATE`、`DB_BMS_STATE` 的 owner 注释和测试。
4. 增加 stale 判断 helper，先服务 protection / bms 状态机。
5. 将 zbus channel 映射关系写入 DB 迁移注释或适配层。
6. 补 `tests/integration`，验证 copy-by-value、sequence 递增、invalid/stale fail-safe 行为。

## 11. 待决问题

| 问题 | 默认立场 | 何时决策 |
|---|---|---|
| `source` 枚举如何命名 | 先用模块私有 enum 或通用 `BMS_DB_SOURCE_*` | M1 实现 header 时 |
| stale 阈值放哪里 | 先 Kconfig，后续纳入配置/标定治理 | 引入 stale helper 时 |
| `DB_COMMAND` owner 是 comm 还是 sys adapter | 外部通信由 `bms_comm_rx` 写，内部维护命令由 system adapter 写 | 实现命令通路前 |
| ISR 是否允许写 DB | 默认不允许 | 出现硬件 ALERT fast path 时重新评审 |
| DB 是否提供泛型 API | v0 保留 typed API；泛型 table 后续再定 | entry 数量明显增长时 |

## 12. 参考

- [concept-architecture.md](concept-architecture.md)
- [standard-module-interface.md](standard-module-interface.md)
- [quality-integration-test-strategy.md](quality-integration-test-strategy.md)
- [docs/traceability.md](traceability.md)