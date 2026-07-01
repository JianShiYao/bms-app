<!--
  需求规格：comm 模块 CAN 上报周期 Kconfig 可配。
  套用模板：docs/templates/requirements-template.md（EARS 句式，验收标准可度量）。
  本阶段（敏捷-V 左腿①需求）只定义「做什么 / 如何验收」，不含实现或架构决策。
  追溯链见同目录 traceability.md。
-->
# comm 模块 CAN 上报周期 需求规格

| 字段 | 值 |
|---|---|
| 文档 ID | REQ-DOC-COMM |
| 特性 slug | comm-report-period-kconfig |
| 版本 | 0.1（草稿） |
| 关联计划 | [`00-iteration-plan.md`](00-iteration-plan.md) |
| 关联设计 | DES-COMM-NNN（待 ②/③ 回填） |
| 状态 | 草稿 |

## 约定与符号

- 周期配置项指 `CONFIG_BMS_COMM_REPORT_PERIOD_MS`（单位 ms，整数），下称「上报周期」。
- 数值与单位以 `app/include/bms/types.h`/Kconfig 为准：周期单位 ms、默认 200ms。
- `P_min` / `P_max`：上报周期的合法下界 / 上界，单位 ms，**具体取值由 ②架构 / ③设计阶段确定**（需求层仅约束：`P_min` 必须严格 > 0，`P_max` 为工程上界且 `P_max ≥ 200`）。本文凡涉具体边界值处均以符号引用，便于设计阶段收敛后回填。
- 失效安全相关需求以 ⚠️ 标注（对齐 docs/process/workflow.md §2 与计划第 4 节）。
- 「钳制（clamp）」：当输入越界时映射到最近的合法边界值（< `P_min` → `P_min`；> `P_max` → `P_max`），并保证结果落在 `[P_min, P_max]`。

## 需求列表

---

### REQ-COMM-001 上报周期由 Kconfig 配置驱动

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 优先级 / 安全等级 | 中 |
| 来源 | Backlog 条目（计划第 1 节） |
| 验证方法 | 测试 |
| 关联设计 | DES-COMM-NNN |
| 关联测试 | `bms.comm.test_default_period_is_200` |
| 状态 | 草稿 |

**需求描述（EARS — 普遍型）**
> 系统应使 comm 模块对外 CAN 上报的周期由编译期配置项 `CONFIG_BMS_COMM_REPORT_PERIOD_MS`（单位 ms）决定，无需修改源码即可调整上报节奏。

**理由**
> 不同部署（实车 CAN 总线负载 / 仿真 / 调参）需要不同上报节奏；编译期可配避免改源码、避免分叉。

**验收标准（可度量）**
- Given 构建配置 `CONFIG_BMS_COMM_REPORT_PERIOD_MS = N`（N 在合法区间内），When comm 模块取用上报周期，Then 实际生效周期 = N ms。
- Given 同上配置为两个不同合法值 N1 ≠ N2，When 分别构建，Then 生效周期分别为 N1、N2（配置改变可观察地改变周期）。

---

### REQ-COMM-002 上报周期默认值为 200ms

| 属性 | 内容 |
|---|---|
| 类型 | 功能 / 约束 |
| 优先级 / 安全等级 | 中 |
| 来源 | Backlog 条目（默认 200ms） |
| 验证方法 | 测试 |
| 关联设计 | DES-COMM-NNN |
| 关联测试 | `bms.comm.test_default_period_is_200` |
| 状态 | 草稿 |

**需求描述（EARS — 不期望/缺省型）**
> 如果未显式覆盖 `CONFIG_BMS_COMM_REPORT_PERIOD_MS`，则系统应采用默认上报周期 200ms。

**理由**
> 固化默认值，防止改 Kconfig 时误改默认（计划风险 R5）；200ms 为现状基线，向后兼容。

**验收标准（可度量）**
- Given 不在工程中覆盖该配置项，When 读取生效上报周期，Then 其值 = 200ms。
- Given Kconfig 定义，When 检视 `config BMS_COMM_REPORT_PERIOD_MS`，Then 存在 `default 200`。

---

### REQ-COMM-003 上报周期配置依赖 BMS_COMM 模块开关

| 属性 | 内容 |
|---|---|
| 类型 | 约束 / 接口 |
| 优先级 / 安全等级 | 中 |
| 来源 | 现状基线 + 模块解耦约定 |
| 验证方法 | 检视 |
| 关联设计 | DES-COMM-NNN |
| 关联测试 | （检视，无独立用例；见追溯表替代验证说明） |
| 状态 | 草稿 |

**需求描述（EARS — 状态型）**
> 在 comm 模块（`CONFIG_BMS_COMM`）未启用期间，系统应不暴露上报周期配置项；上报周期配置应 `depends on BMS_COMM`。

**理由**
> 配置随模块按需开关，避免在 comm 关闭时出现无意义配置项；维持各模块 Kconfig 单独开关的解耦约定。

**验收标准（可度量）**
- Given Kconfig 定义，When 检视 `config BMS_COMM_REPORT_PERIOD_MS`，Then 含 `depends on BMS_COMM`。
- Given `CONFIG_BMS_COMM=n`，When 求值配置树，Then `BMS_COMM_REPORT_PERIOD_MS` 不可见/不生效。

---

### REQ-COMM-004 上报周期取值范围与编译期约束

| 属性 | 内容 |
|---|---|
| 类型 | 约束 / 性能 |
| 优先级 / 安全等级 | 中 |
| 来源 | 计划第 4 节（R1 周期边界配置） |
| 验证方法 | 检视 / 测试 |
| 关联设计 | DES-COMM-NNN |
| 关联测试 | `bms.comm.test_period_range_bounds` |
| 状态 | 草稿 |

**需求描述（EARS — 普遍型）**
> 系统应为 `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 施加取值范围约束 `[P_min, P_max]`（`P_min` 严格 > 0，`P_max` 为工程上界），使越界配置在编译期即被阻止。

**理由**
> 无约束时可填 0/负值导致忙等或退化（计划 R1）；在编译期硬约束是消除该类配置错误的第一道防线。

**验收标准（可度量）**
- Given Kconfig 定义，When 检视 `config BMS_COMM_REPORT_PERIOD_MS`，Then 含 `range <P_min> <P_max>` 且 `P_min > 0`、`P_max ≥ 200`。
- Given `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 被赋值为 `P_min` 与 `P_max`（含端点），When 构建，Then 构建成功且生效周期分别为 `P_min`、`P_max`。
- Given 默认值 200 落在 `[P_min, P_max]` 内，When 求值范围约束，Then 不与 REQ-COMM-002 冲突（`P_min ≤ 200 ≤ P_max`）。

---

### ⚠️ REQ-COMM-005 上报周期恒为正，不退化为忙等（失效安全）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 优先级 / 安全等级 | 高 |
| 来源 | 计划第 4 节 ⚠️ 失效安全考量 1 / R1 |
| 验证方法 | 测试 |
| 关联设计 | DES-COMM-NNN |
| 关联测试 | `bms.comm.test_clamp_below_lower_bound`、`bms.comm.test_clamp_zero_or_negative` |
| 状态 | 草稿 |

**需求描述（EARS — 不期望型）**
> 如果上报周期取值 ≤ 0 或低于合法下界 `P_min`，则系统应将实际生效周期钳制到 `P_min`（严格 > 0），使 comm 线程的实际睡眠周期恒 > 0。

**理由**
> ⚠️ 失效安全：`k_msleep(0/负)` 会使最低优先级的 comm 线程几乎不让出 CPU，挤占调度、间接拖累保护/采样等安全相关线程。需以「Kconfig `range` 硬约束 + 运行期防御性钳制」双保险，保证任何情况下睡眠周期 > 0（计划 ⚠️ 考量 1）。

**验收标准（可度量）**
- Given 周期合法化逻辑输入 = 0，When 求合法化结果，Then 结果 = `P_min` 且 > 0。
- Given 输入为任一负值（如 -1、-1000），When 求合法化结果，Then 结果 = `P_min` 且 > 0。
- Given 输入为 `0 < x < P_min`，When 求合法化结果，Then 结果 = `P_min`。
- 不变式：对任意整数输入，合法化结果恒满足 `P_min ≤ 结果 ≤ P_max` 且 `结果 > 0`。

---

### ⚠️ REQ-COMM-006 周期改动不抬升优先级、不引入无限阻塞（失效安全）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 / 约束 |
| 优先级 / 安全等级 | 高 |
| 来源 | 计划第 4 节 ⚠️ 失效安全考量 2 |
| 验证方法 | 检视 / 分析 |
| 关联设计 | DES-COMM-NNN |
| 关联测试 | （检视/分析，见追溯表替代验证说明） |
| 状态 | 草稿 |

**需求描述（EARS — 状态型）**
> 在本特性的全部改动期间，系统应保持 comm 线程为最低优先级（不抬升），并保持其 zbus 读取使用有限超时（不得引入 `K_FOREVER` 或其它无限阻塞），确保保护/采样线程始终优先于上报流。

**理由**
> ⚠️ 失效安全：comm 为非安全关键上报流，其周期改动绝不可改变线程优先级序（protection > afe > soc/balancing > comm）或将读取改为无限等待，否则会拖累或阻塞安全链（计划 ⚠️ 考量 2 / 红线对齐）。

**验收标准（可度量）**
- Given 改动前后源码，When 检视 comm 线程优先级，Then 优先级数值不变（仍为最低，劣于 protection/afe/soc/balancing）。
- Given 改动前后源码，When 检视各 zbus 读取调用，Then 全部使用有限超时（无 `K_FOREVER`、无去除超时）。

---

### ⚠️ REQ-COMM-007 越界配置被钳制时具备可观测性（失效安全可观测）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 / 接口 |
| 优先级 / 安全等级 | 中 |
| 来源 | 计划第 4 节 ⚠️ 失效安全考量 3 |
| 验证方法 | 检视 / 测试 |
| 关联设计 | DES-COMM-NNN |
| 关联测试 | `bms.comm.test_clamp_below_lower_bound`（断言钳制结果确定性） |
| 状态 | 草稿 |

**需求描述（EARS — 事件型）**
> 当生效上报周期来自对越界配置的钳制时，系统应使该钳制具有确定性结果（落在 `[P_min, P_max]`），并使生效周期可被观测（如启动日志反映实际生效周期），避免「配了却没生效」的静默偏差。

**理由**
> ⚠️ 失效安全可观测：钳制若静默发生，现场调参会被误导（以为配置生效）。确定性 + 可观测可让偏差被发现（计划 ⚠️ 考量 3）。注：本特性不引入运行期动态可调（非目标），可观测性以编译期/启动期的确定值与日志为准。

**验收标准（可度量）**
- Given 任一越界输入，When 求合法化结果，Then 结果确定且唯一（可被单元测试断言）。
- Given 启动初始化，When 检视启动日志，Then 其反映实际生效的上报周期值（与合法化结果一致）。

---

## 非功能 / 边界需求汇总

- **时序边界**：生效上报周期 ∈ `[P_min, P_max]` ms，`P_min > 0`（REQ-COMM-004/005）。
- **默认值**：200ms（REQ-COMM-002）。
- **失效安全不变式**：任意配置/输入下实际睡眠周期恒 > 0；线程优先级序不变；读取超时有限（REQ-COMM-005/006）。
- **可观测性**：越界钳制结果确定且可经启动日志观测（REQ-COMM-007）。

## 范围对齐（与计划第 1 节非目标一致，仅作边界声明，不产出需求）

- 不要求真实 CAN 收发（保持桩）；不要求运行期动态可调周期（仅编译期）；不改其它模块周期；不改 comm 日志策略；不改 zbus 通道/`types.h`。上述均为本特性显式非目标，相关行为不在本需求规格的验收范围内。

## 待设计阶段确定的参数（移交 ②/③）

- `P_min`、`P_max` 的具体数值（需求层约束：`P_min > 0`、`200 ∈ [P_min, P_max]`、`P_max ≥ 200`）。
- 钳制逻辑的承载形式（纯函数签名 / 是否导出供 ztest）——属设计决策，本文不规定实现。
