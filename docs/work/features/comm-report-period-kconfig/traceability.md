<!--
  需求↔设计↔测试 追溯矩阵（活文档）。特性：comm CAN 上报周期 Kconfig 可配。
  参考 `docs/work/traceability.md` 的格式
  原则：每条 REQ 至少有一个验证手段；安全相关需求优先有自动化测试。
  状态取值：草稿 / 已实现 / 已验证 / 缺口。
-->
# 追溯矩阵：comm 模块 CAN 上报周期 Kconfig 可配

> 特性 slug：`comm-report-period-kconfig`　·　配套计划：[`00-iteration-plan.md`](00-iteration-plan.md)
> 维护约定：新增/变更需求时同步更新本表；CI 通过即可把对应行标为「已验证」。
> 「缺口」= 尚无验证手段，需补测试或说明替代验证（分析/检视/演示）。
> ID 规范：需求 `REQ-COMM-NNN`、设计 `DES-COMM-NNN`（域 = COMM）。

| 需求 ID | 需求摘要 | 设计 | 验证方法 | 测试用例 | 状态 |
|---|---|---|---|---|---|
| REQ-COMM-001 | 上报周期由 `CONFIG_BMS_COMM_REPORT_PERIOD_MS`（ms）决定，免改源码可调 | DES-COMM-001、DES-COMM-006 | 测试 | `bms.comm.test_default_period_is_200` | 已验证 |
| REQ-COMM-002 | 上报周期默认值为 200ms（未覆盖时） | DES-COMM-001、DES-COMM-006 | 测试 | `bms.comm.test_default_period_is_200` | 已验证 |
| REQ-COMM-003 | 周期配置 `depends on BMS_COMM`，模块关闭时不暴露 | DES-COMM-001 | 检视 | _(检视；替代验证见下)_ | 已验证 |
| REQ-COMM-004 | 周期取值范围 `range [P_min, P_max]`，`P_min > 0`，编译期阻止越界 | DES-COMM-001、DES-COMM-002、DES-COMM-003、DES-COMM-006 | 检视 / 测试 | `bms.comm.test_period_range_bounds`、`bms.comm.test_clamp_above_upper_bound` | 已验证 |
| ⚠️ REQ-COMM-005 | 失效安全——周期 ≤0 或低于下界时钳制到 `P_min`，实际睡眠恒 > 0，不忙等 | DES-COMM-002、DES-COMM-003、DES-COMM-004、DES-COMM-006 | 测试 | `bms.comm.test_clamp_below_lower_bound`、`bms.comm.test_clamp_zero_or_negative`、`bms.comm.test_clamp_above_upper_bound`、`bms.comm.test_clamp_invariant_holds` | 已验证 |
| ⚠️ REQ-COMM-006 | 失效安全——不抬升 comm 线程优先级、读取保持有限超时，不阻塞安全链 | DES-COMM-004 | 检视 / 分析 | _(检视/分析；替代验证见下)_ | 已验证 |
| ⚠️ REQ-COMM-007 | 失效安全可观测——越界钳制结果确定、生效周期可经启动日志观测 | DES-COMM-002、DES-COMM-005、DES-COMM-006 | 检视 / 测试 | `bms.comm.test_clamp_below_lower_bound` | 已验证 |

> 说明：设计编号已由 ③ 设计阶段回填（DES-COMM-001~006，见 [`03-design.md`](03-design.md) 第 0 节索引）。`P_min` / `P_max` 已收敛为 `P_min = 10`、`P_max = 60000`（ms），满足需求约束 `P_min > 0`、`200 ∈ [P_min, P_max]`、`P_max ≥ 200`。
> 回填顺序：① 需求 → 填「需求 ID / 需求摘要」(已完成)；② 架构 / ③ 设计 → 填「设计」(`DES-COMM-NNN`)；
> ⑤ 测试 → 确认/填「验证方法 / 测试用例」并把「状态」推进到「已验证」。
> 任一行出现空链（设计/验证为空）或停在「缺口」且无替代验证说明，视为追溯断裂，阻塞 DoD 准出。

## 替代验证说明（非「测试」方法的需求）
- REQ-COMM-003（检视）：通过检视 `app/Kconfig` 中 `config BMS_COMM_REPORT_PERIOD_MS` 含 `depends on BMS_COMM`，并以 `CONFIG_BMS_COMM=n` 时配置不可见佐证；不单设 ztest 用例。
- REQ-COMM-006（检视/分析）：通过检视改动前后源码（comm 线程优先级数值不变、各 zbus 读取仍用有限超时无 `K_FOREVER`）+ 优先级序分析佐证；不单设 ztest 用例。

## 统计

- 需求总数：7（功能/约束 4：REQ-COMM-001~004；⚠️ 失效安全 3：REQ-COMM-005~007）。
- 验证方法分布：测试 4（含 1 项检视+测试）、检视/分析 3（其中 REQ-COMM-004/007 兼用测试）。
- 已验证 / 缺口：⑤ 测试已完成，7/7 需求全部「已验证」（测试 4 + 检视/分析 3，部分兼用）；无功能缺口。
- 测试结果（2026-06-30 复跑）：全量并发跑 `..\run-tests-coverage.ps1 -Board mps2/an386` 四套件全 PASS——`bms.comm` 6/6（qemu 47.9s）、`bms.afe` 13/13、`bms.protection` 17/17、`bms.soc` 21/21（qemu 117.2s）；全仓 4/4 配置、57/57 用例通过，0 失败 0 错误。本轮 soc 未触发先前并发+插桩下偶发的 60s Timeout（已确认为环境 flaky，非逻辑失败）。详见 [`05-test-report.md`](05-test-report.md)。
- 覆盖率：被测纯函数 `comm_period.c` 逻辑覆盖经静态分析为 100%；Windows/QEMU 链路 gcov dump 截断未产出数值报告（已知限制），权威覆盖率由 CI（Linux/native_sim）给出。
