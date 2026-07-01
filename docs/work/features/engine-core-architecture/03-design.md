# Engine Core 详细设计

| 字段 | 值 |
|---|---|
| 设计 ID | DES-DOC-ENG |
| 版本 | 0.1（草稿） |
| 满足需求 | REQ-ENG-001..007 |
| 状态 | 草稿 |

## DES-ENG-001 `bms_db` typed snapshot database

满足：REQ-ENG-001、REQ-ENG-002、REQ-ENG-007

接口：

```c
int bms_db_write_cell_meas(const struct bms_cell_meas *meas);
int bms_db_read_cell_meas(struct bms_cell_meas *meas, struct bms_db_meta *meta);
int bms_db_write_soc(const struct bms_soc *soc);
int bms_db_read_soc(struct bms_soc *soc, struct bms_db_meta *meta);
int bms_db_write_prot(const struct bms_prot_evt *prot);
int bms_db_read_prot(struct bms_prot_evt *prot, struct bms_db_meta *meta);
int bms_db_write_bms_state(const struct bms_state_snapshot *state);
int bms_db_read_bms_state(struct bms_state_snapshot *state, struct bms_db_meta *meta);
```

设计：

- 每个 entry 保存一个结构体值拷贝。
- 每个 entry 有 `sequence` 与 `valid` meta。
- API 对 NULL 返回 `-EINVAL`。
- 内部用 Zephyr mutex 保护读写。

后续扩展：

- entry table 化，减少重复代码。
- 增加 source / timestamp header 的统一包装。
- 增加 DB change notification。

## DES-ENG-002 `bms_diag` 诊断中心

满足：REQ-ENG-003、REQ-ENG-005

接口：

```c
int bms_diag_report(enum bms_diag_id id,
		    enum bms_diag_severity severity,
		    bool active,
		    bool latch);
int bms_diag_get_state(struct bms_diag_state *out);
bool bms_diag_has_error(void);
```

设计：

- 用 bit mask 记录 active 和 latched fault。
- `max_severity` 表示当前聚合严重度。
- CRITICAL/latch 诊断进入 BMS 状态机后应导致 LOCKED。
- ERROR 诊断应禁止 NORMAL。

后续扩展：

- 每个诊断条目独立 occurrence count / first_seen / last_seen。
- 增加老化/恢复条件。
- 增加 NVM 持久化故障记录。

## DES-ENG-003 `bms_bms` 主状态机

满足：REQ-ENG-004、REQ-ENG-005

接口：

```c
enum bms_state bms_next_state(enum bms_state cur,
			      const struct bms_state_inputs *in);
enum bms_contactor bms_contactor_for_state(enum bms_state state);
```

设计：

- NULL 输入返回 `BMS_STATE_FAULT`。
- `hw_fault_latched` 或 latched/CRITICAL 诊断优先进入 `LOCKED`。
- ERROR 诊断、protection 非 NORMAL、open request 进入 `FAULT`。
- 只有 `BMS_STATE_NORMAL` 对应 contactor `CLOSED`。
- 其它状态均为 `OPEN`。

后续扩展：

- 完整 PRECHARGE 时序。
- LOCKED 显式 reset/re-arm 条件。
- 接触器反馈校验。

## DES-ENG-004 `bms_task` Zephyr 任务框架

满足：REQ-ENG-006

任务：

| Task | 周期 | 职责 |
|------|------|------|
| safety task | `CONFIG_BMS_TASK_SAFETY_PERIOD_MS` | 采样、protection、diag、BMS state |
| app task | `CONFIG_BMS_TASK_APP_PERIOD_MS` | SOC、balancing、comm TX |
| background task | 5s | health log |

设计：

- 线程由 `K_THREAD_DEFINE(..., SYS_FOREVER_MS)` 静态定义，`bms_task_init()` 统一启动。
- `bms_task` 持有 SOC 跨帧状态与 protection 默认阈值。
- safety task 不做长时间阻塞。
- app task 读取 DB 快照后调用慢速算法和上报逻辑。

后续扩展：

- 抽出 `bms_task_run_safety_once()` / `bms_task_run_app_once()` 便于集成测试。
- 增加 `bms_sys_mon_enter/exit` 记录运行时间。
- 增加 watchdog 喂狗门控。

## DES-ENG-005 zbus 兼容层

满足：REQ-ENG-007

设计：

- engine pipeline 写入 DB 后，同步发布旧 channel：
  - `chan_cell_meas`
  - `chan_soc`
  - `chan_prot_state`
- 旧 channel 不再作为新增模块的目标契约。
- 未来删除 zbus 契约前，必须先确认所有消费者已迁移到 DB。

## 失效处理

| 情况 | 行为 |
|------|------|
| DB read 空指针 | 返回 `-EINVAL` |
| DB entry 未写入 | meta.valid=false |
| 诊断 ERROR | BMS 不进入 NORMAL |
| 诊断 CRITICAL / latch | BMS 进入 LOCKED |
| protection 非 NORMAL | BMS 进入 FAULT |
| 状态机 NULL 输入 | FAULT |

## 验证要点

- DB read/write value copy 与 sequence。
- diag active/latched/severity 聚合。
- `bms_next_state()` 安全优先级。
- `bms_contactor_for_state()` CLOSED iff NORMAL。
- task framework 初始化后线程由 `bms_task_init()` 启动。
- 旧业务模块不再自启动长期线程。

