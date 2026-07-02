<!--
  活仓库（Zephyr 新固件）需求追溯矩阵 —— 权威。格式见 docs/templates/traceability-matrix-template.md。
  原则（methodology.md 原则3）：每条需求至少一个验证手段，安全相关优先自动化测试。
  与 docs/reference/legacy-requirements/ 的区别：后者是逆向旧 S16100B 固件的「移植参考」（REQ-<域>-001 起），
  本表是新固件「已实现并被测试链接」的需求。两者同域 ID 不冲突——新固件需求接续编号（SOC 从 025 起）。
-->
# 需求追溯矩阵（活仓库）

> 维护约定：新增/变更需求时同步本表（DoD「追溯链无断链」「变更已再基线」）；CI 对应测试通过即标「已验证」。
> 测试用例名格式 `bms.<模块>.<用例>`（twister id）。状态：草稿 / 已实现 / 已验证 / 部分 / 缺口。

## Engine Core 架构（`engine-core-architecture`）

> 证据包见 [features/engine-core-architecture/](features/engine-core-architecture)；首批 `DB→DIAG→BMS` 集成测试已补到 [../tests/integration/db_diag_bms/](../../tests/integration/db_diag_bms)。
> 当前 Windows 本地 `mps2/an386` 为 built-only，`native_sim` 被静态过滤；执行型验证留给 CI/WSL native_sim。

| 需求 ID | 需求摘要 | 设计 | 验证方法 | 测试用例 | 状态 |
|---|---|---|---|---|---|
| REQ-ENG-001 | `bms_db` 提供 typed snapshot 交换 | [DES-ENG-001](features/engine-core-architecture/03-design.md) | 测试 | `bms.integration.test_db_write_read_snapshot` | 部分 |
| REQ-ENG-002 | `bms_db` entry 必须有单一写入者 | [DES-ENG-001](features/engine-core-architecture/03-design.md) | 检视 | — | 已实现 / 待检视 |
| REQ-ENG-003 | `bms_diag` 集中聚合故障 | [DES-ENG-002](features/engine-core-architecture/03-design.md) | 测试 | `bms.integration.test_diag_error_blocks_normal` | 部分 |
| ⚠️ REQ-ENG-004 | `bms_bms` 主状态机集中决定接触器期望态 | [DES-ENG-003](features/engine-core-architecture/03-design.md) | 测试 | `bms.integration.test_bms_fault_opens_contactor` | 部分 |
| ⚠️ REQ-ENG-005 | engine 必须保持失效安全默认态 | [DES-ENG-003](features/engine-core-architecture/03-design.md) | 测试 | `bms.integration.test_bms_default_open` | 部分 |
| REQ-ENG-006 | `bms_task` 统一调度长期运行逻辑 | [DES-ENG-004](features/engine-core-architecture/03-design.md) | 检视 / 集成测试 | `bms.integration.test_task_pipeline_smoke`（待补） | 已实现 / 待验证 |
| REQ-ENG-007 | 兼容 `zbus` 过渡层，避免一次性大迁移 | [DES-ENG-005](features/engine-core-architecture/03-design.md) | 构建 / 检视 | 现有 `bms.*` 单测构建 | 已实现 |

## SOC 模块（库仑计数特性，`soc-coulomb`）

> **ID 规范化说明**：本特性早期用工作码 `REQ-SOC-C01..C12`；现规范为 **`REQ-SOC-025..036`**（接续遗留 `soc.md` 的 001-024，避免撞号；映射 `C0x → 0(x+24)`）。
> 活代码（`tests/bms/soc/`）已用规范 ID；`docs/work/features/soc-coulomb/` 过程文档保留原始 `Cxx` 工作码作为历史记录（不回改，另见 [agents.md §5](../process/agents.md)）。

| 需求 ID | 需求摘要 | 设计 | 验证方法 | 测试用例 | 状态 |
|---|---|---|---|---|---|
| REQ-SOC-025 | 库仑积分核心（安时积分） | `soc.c:bms_soc_coulomb_step`；设计 §3-D（[03-design](features/soc-coulomb/03-design.md)） | 测试 | `bms.soc.test_step_charge_integration_accuracy`、`bms.soc.test_step_discharge_direction` | 已验证 |
| REQ-SOC-026 | 时间间隔来源与异常处理 | `soc.c:bms_soc_coulomb_step`（Δt 解析） | 测试 | `bms.soc.test_step_nonmonotonic_ts_fallback`、`bms.soc.test_step_frame_drop_clamped` | 已验证 |
| REQ-SOC-027 | SOC 夹紧 [0,1000]‰ | `soc.c:soc_charge_to_permille` | 测试 | `bms.soc.test_clamp_over_full`、`bms.soc.test_step_clamp_to_full`、`bms.soc.test_step_clamp_to_empty` | 已验证 |
| REQ-SOC-028 | 上电初始化策略 | `soc.c:bms_soc_estimate` / `bms_soc_coulomb_step`（首帧） | 测试 | `bms.soc.test_full_charge`、`bms.soc.test_empty`、`bms.soc.test_step_first_frame_init`、`bms.soc.test_step_init_only_once`、`bms.soc.test_reset_clears_state` | 已验证 |
| REQ-SOC-029 | 发布到 `chan_soc` 触发条件 | `soc.c:soc_thread`（发布语义） | 测试 | `bms.soc.test_step_first_frame_init`、`bms.soc.test_step_over_range_current_skipped` | 已验证 |
| ⚠️ REQ-SOC-030 | 异常数据隔离与安全降级 | `soc.c:bms_soc_coulomb_step`（分支 A/F）、`soc_current_in_range` | 测试 | `bms.soc.test_step_null_returns_einval`、`bms.soc.test_step_over_range_current_skipped`、`bms.soc.test_step_over_range_negative_current_skipped`、`bms.soc.test_step_frame_drop_clamped`、`bms.soc.test_step_recovers_after_bad_frame` | 已验证 |
| REQ-SOC-031 | 电流符号与积分方向一致性 | `soc.c:bms_soc_coulomb_step`（`dQ=current×dt`） | 测试 | `bms.soc.test_step_charge_integration_accuracy`、`bms.soc.test_step_discharge_direction`、`bms.soc.test_step_zero_current_no_change` | 已验证 |
| ⚠️ REQ-SOC-032 | SOC 不参与接触器/保护决策（边界） | `soc.c:soc_thread`（仅发 `chan_soc`，无保护语义通道） | 检视/集成 | —（结构性约束，超出纯函数单测；评审 + 集成确认） | 已实现 |
| ⚠️ REQ-SOC-033 | 不阻塞安全链（实时性约束） | `soc.c:SOC_THREAD_PRIO=7`、`K_MSEC(50)` 有限超时 | 检视/系统 | —（线程优先级/超时属性，超出纯函数单测；评审 + 系统测试确认） | 已实现 |
| REQ-SOC-034 | 定点积分防溢出（int64） | `soc.h:bms_soc_coulomb_state.acc_charge_ma_ms (int64_t)` | 测试 | `bms.soc.test_step_no_overflow_24h` | 已验证 |
| REQ-SOC-035 | 精度与容差 ≤ ±1‰ | `soc.c:soc_charge_to_permille`（对称舍入） | 测试 | `bms.soc.test_step_charge_integration_accuracy`（`zassert_within ±1`） | 已验证 |
| REQ-SOC-036 | 额定容量可配置（mAh） | `Kconfig:BMS_SOC_PACK_CAPACITY_MAH`；`soc.c` DEN 不硬编码 | 测试/分析 | C12-1 由代码 DEN 不硬编码满足；C12-2（按 1/C 变化）待多 `prj.conf` 参数化 | 部分 |

> SOC 小结：12 条中 9 条「已验证」、2 条 ⚠️ 结构性约束「已实现」（评审/集成确认）、1 条「部分」（待参数化测试）。

## PROT 模块（保护，新固件需求）

> 接续遗留 `docs/reference/legacy-requirements/prot.md` 的 001-032（逆向旧固件）编号；以下为新固件实现并被测试链接的安全需求。

| 需求 ID | 需求摘要 | 设计 | 验证方法 | 测试用例 | 状态 |
|---|---|---|---|---|---|
| ⚠️ REQ-PROT-033 | 无效测量 ⇒ 失效安全（测量有效位不齐时绝不闭合，强制 FAULT→OPEN） | `protection.c:bms_protection_evaluate`（validity 早返 FAULT；对齐 `architecture.md`「测量数据纪律」） | 测试 | `bms.protection.test_invalid_validity_opens`、`bms.protection.test_partial_validity_opens`、`bms.protection.test_invariant_closed_iff_normal` | 已验证 |

> 红线不变量「接触器 CLOSED ⟺ NORMAL」由 `test_invariant_closed_iff_normal` 扫描覆盖（电压/电流/温度/有效位组合）。

## M6 引擎 / 测量-控制 / HAL 模块（`m6-hardware-bringup`）

> M6 Phase 0–2 落地的模块，设计源自 `concept/{architecture,runtime-model,data-model,hardware-abstraction,safety}` 契约（非编号 legacy REQ），故下表以**契约条款为追溯锚**、测试用例为 twister id。sim/编译级已验证；真板功能（Phase 3）待 HIL。证据包见 [features/m6-hardware-bringup/](features/m6-hardware-bringup)。

| 契约锚 | 模块 / 职责 | 设计（源） | 验证 | 测试用例 | 状态 |
|---|---|---|---|---|---|
| architecture「测量数据纪律」/ data-model raw→meas→DB | `meas` 可信化（validity + 时间戳，wrapper 只出原始帧） | `measurement-control/meas/{meas_validate,meas}.c`、`hal/afe/afe.c` | 测试 | `bms.meas.*`（7 用例） | 已验证 |
| data-model stale / diagnostics §5 | 测量过期/源失联 → `MEAS_STALE` 失效安全 | `engine/task.c`、`engine/diag.c` | 测试 | `bms.meas_stale.*` | 已验证 |
| diagnostics §4/§5 生命周期 | `diag` 去抖/锁存/严重度 + 未初始化门控 | `engine/diag.c`、`application/bms.c` | 测试 | `bms.diag.*`、`bms.state.test_uninitialized_diag_blocks_normal` | 已验证 |
| runtime-model §6 任务健康 | `sys_mon` 心跳/运行时间 + 聚合 `DB_TASK_HEALTH` + 接线 | `engine/sys_mon.c`、`engine/task.c` | 测试 | `bms.sys_mon.*`、`bms.sys_mon_agg.*`、`bms.sys_mon_task.*` | 已验证（sim） |
| runtime-model §7 watchdog 门控（软先于硬） | `sys_mon_wdt_feed_allowed` + `hal/wdt` 门控喂狗 | `engine/sys_mon.c`、`hal/wdt/{wdt_stub,wdt_stm32}.c`、`engine/task.c:bms_task_wdt_step` | 测试 / 构建 | `bms.sys_mon_wdt.*`、`bms.wdt_feed.*` | sim 已验证；真板 IWDG 复位待 HIL |
| architecture §7 PRECHARGE 时序 | `bms_next_state` 预充门控（complete→NORMAL / timeout→FAULT） | `application/bms.c` | 测试 | `bms.state.test_precharge_*` | 逻辑已验证；真板预充（经 AFE）待 Phase 3 |
| architecture §7/§8 接触器执行+反馈+诊断 | `contactor` 期望态执行 / 反馈 / 不一致（粘连/拒动）诊断 | `measurement-control/contactor/contactor.c`、`hal/contactor_io/contactor_io_fake.c`、`engine/task.c` | 测试 | `bms.contactor.*`、`bms.contactor_task.*` | sim 已验证；真板 MOS/预充经 AFE 待 Phase 3 |
| hardware-abstraction §8 / M6 Phase 2 | `bms_f405` 真板 port（时钟 + console + IWDG，进 CI 编译门） | `boards/enervenue/bms_f405/*` | 构建 | 4 板 CI build 矩阵（含 `bms_f405`） | 编译级已验证；功能待 HIL |

> M6 小结：Phase 0（契约对账）+ Phase 1（软件结构：hal/afe 拆分、meas 可信化、contactor 抽象+接线、include 三树分层）+ Phase 2（bms_f405 时钟+console+IWDG）**已完成并 sim/编译验证**；Phase 3（SH3673520 真 SPI、W25Q32 NVM、RS485/Modbus、ADC、预充/反馈/上电时序实测）**待物理板 + HIL**。引脚权威见 `reference/hardware/software-interface.md` §9（原理图 V0.4；`.ioc` 弃用）。

## 其他模块（待补 REQ 链接）

| 模块 | 现状 | 待补 |
|---|---|---|
| protection（OV/UV/OC/OT） | 6 个阈值 ztest + REQ-PROT-033 失效安全测试；OV/UV/OC/OT 用例**尚无 `REQ-PROT-NNN` 注释** | 把 OV/UV/OC/OT 等用例映射到遗留 `prot.md` 的 `REQ-PROT-NNN` 并补注释 + 入本表 |
| afe（hal 后端） | sim 后端 ztest 在 `tests/bms/hal/afe/`（可信化 validate 已迁 `tests/bms/measurement-control/meas/`，见上 M6 表），仍**无 REQ 注释** | 映射到 `REQ-AFE-NNN` |
| balancing / comm / main | 无专门单测 | 先补单测（见 [management.md](../quality/management.md) 待补齐清单） |

> 说明：protection/afe 的测试已存在但未带 REQ 注释，链接需逐用例映射到遗留需求，属独立后续项（增量 backfill，见 [methodology.md §4 原则3 立场](../concept/methodology.md)）。
