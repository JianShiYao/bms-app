# BMS 诊断与故障模型 v0（设计契约）

> **定位**：本文细化 [concept-architecture.md](concept-architecture.md) 的 ADR-ARCH-005（诊断中心化），是诊断子系统的**权威设计契约**——规定 `bms_diag` 的**诊断条目（登记表）结构、severity 语义、去抖/锁存/老化生命周期、聚合到 `DB_DIAG_STATE`、以及故障到 `bms_bms` 状态机的路径**。**agent 据此实现或重构 `bms_diag` 代码，代码向本契约对齐**；本文不描述现状实现。
>
> **现状与差距**不在此维护：见 [concept-architecture.md](concept-architecture.md) §11 迁移路径（本契约在 **M3** 落地）。
>
> **规范措辞**：**必须 / 应 / 不得** 表示契约要求，可在评审、实现、测试中引用。相关：架构决策 [concept-architecture.md](concept-architecture.md) §8·§9，数据契约 [concept-data-model.md](concept-data-model.md)（`DB_DIAG_STATE`/`DB_PROT_STATE`），运行时 [concept-runtime-model.md](concept-runtime-model.md)（诊断在哪个任务消化），安全目标 [concept-safety.md](concept-safety.md)。

## 1. 诊断设计原则

- **单一登记中心**：`bms_diag` 是系统唯一的诊断登记与裁决中心。安全相关异常**不得**只用日志、局部变量或模块私有状态处理（ADR-ARCH-005）。
- **感知与裁决分离**：**源模块只报告原始判定**（某条件本周期通过/失败 + 是否可信）；**去抖、锁存、老化、聚合由 `bms_diag` 统一负责**。源模块**不得**自行实现故障锁存或直接据此驱动执行器。
- **诊断不驱动执行器**：`bms_diag` 只登记与聚合、写 `DB_DIAG_STATE`；接触器由 `bms_bms` 依据聚合结果决策（ADR-ARCH-004）。诊断**不得**直接闭合/断开接触器（硬件 zero-latency 例外见 §10）。
- **失效安全**：诊断状态未初始化、过期（stale）、或源信号缺失时，**必须按不安全处理**（不得据此进入/保持 NORMAL）。缺数据**不得**被当作"无故障"。
- **可追溯**：每个诊断条目有稳定 ID，可回指安全目标（[concept-safety.md](concept-safety.md) SG）与需求（追溯矩阵）。

## 2. 诊断条目（登记表）契约

诊断以**静态声明的条目表**存在（foxBMS 2 diagnosis registry 思路，Zephyr 原生落地）。每个条目**必须**声明下列字段：

| 字段 | 含义 | 约束 |
|------|------|------|
| `id` | 稳定枚举（如 `DIAG_CELL_OV`） | 全局唯一、不复用、变更即架构变更 |
| `severity` | INFO / WARNING / ERROR / CRITICAL | 决定反应类别（§3）；条目声明 severity，反应由 severity 派生 |
| `group` | 归属域（meas/prot/contactor/comm/sys_mon…） | 用于聚合与上报归类 |
| `confirm_time_ms` | 置位去抖：原始失败须**持续**多久才 confirm 为 active | 安全条目不得为 0 之外未经评审的短值 |
| `clear_time_ms` | 恢复去抖：原始恢复须**持续**多久才允许 clear | 安全条目 `clear_time_ms ≥ confirm_time_ms` |
| `latch` | 是否锁存：active 后即便条件消失也保持，直至授权复位 | CRITICAL 默认 latch |
| `aging` | 锁存条目的老化/清除策略（授权复位 / 上电 / 计数老化 / 不可自动老化） | 安全锁存默认"仅授权复位"，见 §8 |
| `reaction`（派生） | 对 `bms_bms` 的影响 | **不得**弱于该 severity 的默认反应（§3） |

> 时间/阈值/去抖计数是**标定参数**，其治理（版本、单位、合法范围、默认值）见 `concept-configuration-calibration.md`（待建）；本文只规定字段语义与不变量。

## 3. severity 语义与反应映射（契约）

severity 是反应的**唯一权威**；聚合反应由**当前所有 active/latched 条目中的最高 severity** 决定。

| Severity | 含义 | 对 `bms_bms` 的强制反应 |
|----------|------|------------------------|
| `INFO` | 非安全信息 | 无；仅记录/上报 |
| `WARNING` | 可继续运行但需关注 | 无强制断开；应上报，可触发降额/限流策略 |
| `ERROR` | 不允许运行于 NORMAL | **不得进入 NORMAL**；若已在 NORMAL 必须退出到安全态（STANDBY/FAULT，接触器 OPEN） |
| `CRITICAL` | 必须立即断开 | **立即 OPEN 并进入 FAULT**；默认 `latch` → 通常进入/停留 LOCKED，须授权复位（§8） |

- 反应**只能加严不得放宽**：条目不得声明弱于其 severity 默认反应的行为。
- WARNING 的降额策略属应用层，**不得**替代 ERROR/CRITICAL 的断开语义。

## 4. 诊断条目生命周期（状态机·纯函数）

每个条目的生命周期是一个**纯函数状态机**，由 `bms_diag` 每诊断周期对每条目推进，输入为"源模块本周期原始判定 + 可信位 + 注入 `now_ms`"：

```
INACTIVE --(raw=fail 持续≥confirm_time)--> ACTIVE
ACTIVE   --(raw=ok  持续≥clear_time)-----> (latch? LATCHED : INACTIVE)
LATCHED  --(授权复位 + 前置条件满足)------> INACTIVE
```

- 置位/复位均**必须去抖**：瞬时抖动不得直接改变 active（`confirm_time_ms` / `clear_time_ms`）。
- **安全条目的 clear 不得因信号瞬间恢复而自动完成**（ADR-ARCH-005）：非锁存条目须满足 `clear_time_ms` 持续恢复；锁存条目还须 §8 的授权。
- 该状态机**必须**实现为无副作用纯函数（如 `diag_entry_step(cfg, prev, raw, valid, now_ms) → next`），可注入时间单测（§11）。
- 时间比较**必须用有符号差值**以回绕安全（对齐 [concept-runtime-model.md](concept-runtime-model.md) §2）。

## 5. 去抖、恢复与源失联（契约）

- **置位去抖**：原始失败须持续 `confirm_time_ms`（或连续 N 次）才 confirm；未达则保持 INACTIVE 但**应**可观测"pending"。
- **恢复去抖**：原始恢复须持续 `clear_time_ms` 才允许离开 ACTIVE；期间任一次失败重置恢复计时。
- **源失联 fail-safe**：若源模块本周期**未提供新鲜原始判定**（对应 `DB_*` entry stale 或缺失，见 [concept-data-model.md](concept-data-model.md) §stale），`bms_diag`**必须**：(a) **保持**该条目已 active 状态、**不得**自动 clear；(b) 置位一条"报告源失联/测量过期"诊断（其自身 severity 至少 ERROR）。
- 判 stale 复用统一时间基准 `bms_time_now_ms()`（[concept-runtime-model.md](concept-runtime-model.md) §2·§8）。

## 6. 聚合与 `DB_DIAG_STATE` 契约

`bms_diag` 每诊断周期把全部条目聚合为单一 entry，owner=`bms_diag`（[concept-data-model.md](concept-data-model.md)）。`DB_DIAG_STATE` **至少**表达：

| 字段 | 含义 |
|------|------|
| `initialized` | 诊断子系统是否已完成初始化（未初始化=不安全，见 §7） |
| `worst_severity` | 当前 active/latched 条目的最高 severity（反应权威，§3） |
| `active_summary` | 按 group/severity 的 active 计数或位图 |
| `latched` | 是否存在锁存条目（或锁存位图） |
| `first_fault` / `latest_fault` | 首个与最近置位的条目 id + 时间戳（诊断/上报用） |
| header | `timestamp_ms` / `sequence` / `validity`（统一 entry header，[concept-data-model.md](concept-data-model.md)） |

- 读者拿值拷贝，**不得**缓存 `bms_diag` 内部可变指针。
- 消费者：`bms_bms`（状态迁移，§7）、`bms_sys`（系统模式/授权）、`bms_comm`（上报）。
- `DB_DIAG_STATE` 的期望周期与 stale 容忍见 [concept-data-model.md](concept-data-model.md)（约 1–2 个诊断周期）。

## 7. 诊断 → `bms_bms` 迁移规则（契约）

`bms_bms` 把 `DB_DIAG_STATE` 作为状态机输入之一（ADR-ARCH-004：`bms_bms` 是接触器期望态 owner）。规则（**加严方向**）：

- `initialized == false` **或** `DB_DIAG_STATE` stale/invalid ⇒ 视为不安全 ⇒ **不得进入 NORMAL**（失效安全，[concept-data-model.md](concept-data-model.md)）。
- `worst_severity ≥ ERROR` ⇒ **不得进入或保持 NORMAL**；若在 NORMAL 必须退出到 STANDBY/FAULT（OPEN）。
- `worst_severity == CRITICAL` ⇒ **强制 OPEN + 进入 FAULT**；若条目 latch，则进入/停留 LOCKED。
- 该路径**单向**：`bms_diag` 只提供聚合信号，**不得**代替 `bms_bms` 决策或直接操作接触器。

## 8. latch 与 LOCKED 退出授权（契约）

- **锁存语义**：`latch` 条目一旦 active，即便原始条件消失也**保持有效**，直至满足全部授权条件；CRITICAL 默认 latch。
- **清除锁存的必要条件**（全部满足）：(a) 原始条件已持续恢复 `clear_time_ms`；(b) 收到**显式授权复位**（维护命令 / 上电复位 / 明确的清障命令，经 `bms_sys` 合法性校验）；(c) 无其他阻断性 active 故障。
- **`bms_bms` 的 LOCKED 退出**由 `bms_bms` 拥有；`bms_diag` 只提供"锁存条目是否已全部可清"的门信号。LOCKED**不得**因信号瞬时恢复自动退出。
- **老化**：仅非安全或明确允许老化的条目可按 `aging` 计数/时间自动清；安全锁存默认**不可自动老化**。

## 9. 诊断来源登记（契约）

下列模块**必须**将对应异常报告为诊断条目（原始判定），由 `bms_diag` 裁决（对齐 [concept-architecture.md](concept-architecture.md) §8）：

- `bms_meas`：测量无效、过期、冗余不一致、AFE 通信错误。
- `bms_protection`：过压/欠压/过流/过温/绝缘/互锁等阈值判定（写 `DB_PROT_STATE`，diag 据其原始判定登记）。
- `bms_contactor`：反馈不一致、预充超时、粘连检测失败。
- `bms_comm`：非法命令、通信超时、CAN bus-off。
- `bms_sys_mon`：任务心跳超时、运行超时、栈余量不足、watchdog 门控异常（[concept-runtime-model.md](concept-runtime-model.md) §6·§7）。

## 10. 硬件 latch / zero-latency 集成（契约）

- 硬件严重故障（短路比较器、AFE ALERT）经 zero-latency ISR 只做最小安全动作（接触器强制 OPEN + 置硬件/软件 latch/event），**随后**由 `bms_diag` 将其登记为 CRITICAL 条目（[concept-runtime-model.md](concept-runtime-model.md) §3 zero-latency path、[concept-architecture.md](concept-architecture.md) §9）。
- 此类条目的 clear **必须**同时满足"硬件 latch 已解除"与 §8 授权；ISR 默认**不直接写** `bms_db`（[concept-data-model.md](concept-data-model.md)）。

## 11. 可测性约束

- 条目生命周期（§4）、聚合（§6）、迁移判定（§7）**必须**为纯函数（如 `diag_entry_step` / `diag_aggregate` / `bms_bms` 消费侧判定），可脱离线程/硬件/zbus 单测。
- 一切时间相关判定（confirm/clear/aging/stale）以**注入 `now_ms`** 单测；去抖、锁存、授权复位、源失联 fail-safe 均须有 ztest 覆盖。
- 至少一条 `源模块原始判定 → DIAG 聚合 → BMS 迁移` 的集成测试（对齐架构 §11 M3 与 [quality-integration-test-strategy.md](../quality-integration-test-strategy.md)）。

## 12. 迁移

本契约落地阶段见 [concept-architecture.md](concept-architecture.md) §11：**M3 诊断中心化到 `bms_diag`**（完成判据：protection/meas/comm/sys_mon 故障进入诊断条目）。迁移期允许过渡实现与目标契约共存，但**目标以本契约为准**；每步须保持既有 CI 与 ztest 通过。

## 13. 参考

- [concept-architecture.md](concept-architecture.md) §8（ADR-ARCH-005）、§9、§11。
- [concept-data-model.md](concept-data-model.md)（`DB_DIAG_STATE` / `DB_PROT_STATE` / entry header / stale）。
- [concept-runtime-model.md](concept-runtime-model.md) §2（时间基准）、§6·§7（sys_mon / watchdog）、§8（stale）。
- foxBMS 2 Diagnosis / BMS Module（链接见 [concept-architecture.md](concept-architecture.md) §13）。
