# 架构设计：comm 模块 CAN 上报周期 Kconfig 可配

> 特性 slug：`comm-report-period-kconfig`
> 阶段：敏捷-V 左腿第②层 —— 架构设计（`bms-architect`）
> 输入：`01-requirements.md`、`00-iteration-plan.md`；`app/src/bms/comm/comm.c`、`app/include/bms/comm.h`、`app/Kconfig`、`app/include/bms/channels.h`、`app/include/bms/types.h`、`app/src/bms/protection/protection.c`（纯函数范式）、`tests/bms/afe/CMakeLists.txt`（单测源复用范式）
> 交付物语言：中文。
> 本文件只到**模块边界与接口契约**为止，**不下沉**到逐函数实现/具体边界数值（那是 ③ `bms-designer` 的范围）。

---

## 0. 现状基线（架构落点扫描）

| 维度 | 现状 | 对本特性的含义 |
|---|---|---|
| 配置项 | `app/Kconfig` 已有 `config BMS_COMM_REPORT_PERIOD_MS`，含 `depends on BMS_COMM` + `default 200`，**无 `range`** | REQ-COMM-001/002/003 **现状已满足**；REQ-COMM-004 缺 `range` 待补 |
| 周期消费 | `comm_thread` 末尾 `k_msleep(CONFIG_BMS_COMM_REPORT_PERIOD_MS)`，**直接取宏，无合法化** | REQ-COMM-005/007 需引入**钳制纯函数**，运行期防御 |
| 纯逻辑函数 | comm 模块**无任何纯函数**（全为线程内联 + 桩 TX） | 需新增可单测的周期合法化纯函数（对齐 protection 范式） |
| 线程 | `K_THREAD_DEFINE(bms_comm_tid, … COMM_THREAD_PRIO=8 …)` | comm 为**最低优先级**，REQ-COMM-006「不抬升」**现状已满足**，维持不变 |
| zbus 读取超时 | 三处 `zbus_chan_read(…, K_MSEC(50))`，**有限超时，无 `K_FOREVER`** | REQ-COMM-006「有限超时」**现状已满足**，维持不变 |
| 线程优先级全景 | protection=4（安全，最高）、afe=6、soc=7、balancing=7、**comm=8** | comm(8) 为最劣，劣于全部其它模块，**已满足** REQ-COMM-006 |
| 启动日志 | `bms_comm_init()` 已 `LOG_INF("… period=%d ms …", CONFIG_BMS_COMM_REPORT_PERIOD_MS)` | REQ-COMM-007 可观测性已有锚点，需改为反映**合法化后**的生效值 |
| zbus 通道 / `types.h` | comm 仅**读** `chan_cell_meas`/`chan_soc`/`chan_prot_state`，不发布 | 本特性**零通道/零类型**变更（与需求非目标一致） |

**复用结论**：通道、线程归属、线程优先级、读取超时策略**全部复用、零新增**。本特性是 comm 模块**内部**的「周期取值合法化 + Kconfig 硬约束」纵向加固，**不改变**其在 zbus 架构中的拓扑位置，也**不触碰**任何安全关键路径。核心增量仅两处：① Kconfig 补 `range`；② 新增一个周期合法化纯函数并在线程睡眠前调用。

---

## 1. 架构决策清单（ADR 级）

> 每条：决策 + 理由 + 涉及模块/通道 + 关联需求 ID。决策号 `ADR-COMM-xx`，供下游设计/测试回链。

### ADR-COMM-01　复用既有 `CONFIG_BMS_COMM_REPORT_PERIOD_MS`，不新增配置项
- **决策**：上报周期沿用既有 `config BMS_COMM_REPORT_PERIOD_MS`（保留 `depends on BMS_COMM`、`default 200`）；**不**新增并列配置项、**不**改名。
- **理由**：现状已实现「编译期可配 + 默认 200 + 依赖模块开关」三项需求；新增配置项会与现状重复、违背「复用优先」并制造迁移负担（需求 R5：误改默认）。
- **涉及模块/通道**：`app/Kconfig`（`config BMS_COMM` 节下）；comm 模块读取。
- **关联需求**：REQ-COMM-001、REQ-COMM-002、REQ-COMM-003。

### ADR-COMM-02　为 `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 增加 `range P_min P_max` 硬约束
- **决策**：在该配置项上增加 `range <P_min> <P_max>`，构成越界配置的**编译期第一道防线**。约束须满足 `P_min > 0`、`P_min ≤ 200 ≤ P_max`、`P_max ≥ 200`（与 `default 200` 不冲突）。**`P_min`/`P_max` 的具体数值由 ③ `bms-designer` 收敛回填**，架构层仅固定「必须有 `range` 且下界严格 > 0」这一约束。
- **理由**：无 `range` 时可填 0/负值导致 `k_msleep(0/负)`，使最低优先级 comm 线程几乎不让出 CPU，间接拖累 protection/afe 安全相关线程（需求 R1 / ⚠️ 失效安全考量 1）。Kconfig `range` 在编译期即阻断该类配置错误。
- **涉及模块/通道**：`app/Kconfig`。
- **关联需求**：REQ-COMM-004、⚠️ REQ-COMM-005。

### ADR-COMM-03　新增「周期合法化」纯函数（钳制逻辑承载体，纯函数 + 线程分离范式）
- **决策**：将「把任意整数周期输入钳制到 `[P_min, P_max]`」实现为**无副作用纯函数**，签名形如 `bms_comm_clamp_period_ms(int32_t requested) -> int32_t`（精确名/签名由 ③ 定），声明于 `app/include/bms/comm.h`、实现于 `app/src/bms/comm/comm.c`（或拆出的 `comm_period.c`，由 ③ 定）。线程在 `k_msleep` 前调用它取得生效周期；该函数**导出供 ztest 直接调用**。
- **理由**：复刻既有 `bms_protection_evaluate` 的「纯逻辑/线程分离」范式（CLAUDE.md 可测试性约定），是满足 REQ-COMM-005/007 大量可量化、确定性验收准则的唯一可单测落点；comm 模块当前无纯函数，需按范式补齐（解决 R5：comm 缺测试）。
- **涉及模块/通道**：comm 模块（`comm.h` 原型、`comm.c`/`comm_period.c` 实现）。
- **关联需求**：⚠️ REQ-COMM-005、⚠️ REQ-COMM-007、REQ-COMM-004（端点构建验证）。

### ADR-COMM-04　运行期防御性钳制与编译期 `range` 双保险，保证睡眠周期恒 > 0
- **决策**：即便 Kconfig `range` 已在编译期约束（ADR-COMM-02），线程仍**必须**先经 ADR-COMM-03 纯函数合法化，再以其返回值 `k_msleep()`；纯函数对 `≤ 0`、`0 < x < P_min`、`> P_max` 全部钳制到最近合法边界，**不变式**：对任意 `int32_t` 输入，返回值恒满足 `P_min ≤ r ≤ P_max` 且 `r > 0`。**禁止**直接 `k_msleep(CONFIG_BMS_COMM_REPORT_PERIOD_MS)`。
- **理由**：⚠️ 失效安全——`range` 是编译期防线，运行期防御性钳制覆盖「调参误传 / 未来动态来源 / 宏被绕过」等残余路径，双保险确保 comm 线程实际睡眠周期任何情况下恒 > 0，不退化为忙等抢占调度（需求 ⚠️ 考量 1）。
- **涉及模块/通道**：comm 模块（`comm_thread` 调用纯函数）。
- **关联需求**：⚠️ REQ-COMM-005。

### ADR-COMM-05　钳制结果确定且可经启动日志观测（失效安全可观测）
- **决策**：钳制纯函数对同一输入产出**确定且唯一**的结果（可被单测断言）；`bms_comm_init()` 的启动 `LOG_INF` 须打印**合法化后的生效周期**（即纯函数对 `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 求值的结果），而非原始宏值，使「配了却被钳制」不再静默。可观测性以**启动期确定值 + 日志**为准（不引入运行期动态可调——属非目标）。
- **理由**：⚠️ 失效安全可观测——静默钳制会误导现场调参（以为配置生效）；确定性 + 启动日志让偏差可被发现（需求 ⚠️ 考量 3）。
- **涉及模块/通道**：comm 模块（`bms_comm_init` 日志、纯函数确定性）。
- **关联需求**：⚠️ REQ-COMM-007。

### ADR-COMM-06　comm 线程维持最低优先级 8、读取维持有限超时，均不调整（安全相对关系）
- **决策**：comm 工作线程优先级保持 `COMM_THREAD_PRIO = 8` 不变，明确其**数值最大（优先级最低）**，劣于 protection(4)/afe(6)/soc(7)/balancing(7)；三处 `zbus_chan_read(…, K_MSEC(50))` 的**有限超时复用不变**，**禁止**引入 `K_FOREVER` 或移除超时。本特性的全部改动**不得**触及线程优先级与读取超时。
- **理由**：⚠️ 失效安全——comm 为非安全关键上报流，周期改动绝不可改变优先级序或将读取改为无限阻塞，否则会拖累/阻塞安全链（需求 ⚠️ 考量 2 / 项目失效安全红线）。现状 8（最低）+ `K_MSEC(50)` 已合规，维持现值即满足，避免引入新风险。
- **涉及模块/通道**：comm 线程 vs protection/afe/soc/balancing 线程；`chan_cell_meas`/`chan_soc`/`chan_prot_state` 读路径。
- **关联需求**：⚠️ REQ-COMM-006。

### ADR-COMM-07　零 zbus 通道 / 零 `types.h` 变更（复用优先、解耦保持）
- **决策**：不新增/不修改任何 `ZBUS_CHAN_DEFINE`，不改 `app/include/bms/types.h`；comm 继续仅**读取**现有三通道、不发布。周期与上报节奏属 comm 内部行为，不进入通道载荷。
- **理由**：周期是模块私有的调度参数，与通道数据语义无关；改 `types.h` 会污染被五模块共用的共享 ABI、放大改动连锁面，且与需求非目标（不改 zbus 通道/`types.h`）一致；维持 zbus 解耦最小化原则。
- **涉及模块/通道**：comm（仅读三通道）；`channels.*`、`types.h` **不变**。
- **关联需求**：REQ-COMM-001（边界）、REQ-COMM-006（不引入新耦合）；与需求「范围对齐」非目标一致。

### ADR-COMM-08　新增 comm 单测套件 `tests/bms/comm/`，复用纯函数源
- **决策**：新增 `tests/bms/comm/`（`testcase.yaml` + `CMakeLists.txt` + `prj.conf` + `src/main.c`），`CMakeLists.txt` 仅链接承载钳制纯函数的源文件（`comm.c` 或拆出的 `comm_period.c`）与 `app/include`，**不链接线程/桩 TX**，对齐 `tests/bms/afe` 的「只链纯函数核心」范式。套件名 `bms_comm`，用例覆盖需求点名的 `test_default_period_is_200`、`test_period_range_bounds`、`test_clamp_below_lower_bound`、`test_clamp_zero_or_negative`，用例顶部 `/* Verifies REQ-COMM-NNN */` 回链。
- **理由**：需求多条验收准则要求「测试」验证（REQ-COMM-001/002/004/005/007）；comm 当前无测试（R5），架构层固定「钳制逻辑须以可单测纯函数承载 + 独立套件」这一可测性边界。具体用例实现属 ⑤ tester。
- **涉及模块/通道**：`tests/bms/comm/`（新增）；复用 comm 纯函数源。
- **关联需求**：REQ-COMM-001、REQ-COMM-002、REQ-COMM-004、⚠️ REQ-COMM-005、⚠️ REQ-COMM-007。

---

## 2. zbus 通道与数据结构变更

### 2.1 通道（`channels.c` / `channels.h`）
- **无任何变更**（ADR-COMM-07）。comm 继续仅 `zbus_chan_read` 现有 `chan_cell_meas`/`chan_soc`/`chan_prot_state`，不新增、不修改、不发布。

### 2.2 共享类型 `types.h`
- **无任何变更**（ADR-COMM-07）。上报周期是 comm 私有调度参数，不属任何通道载荷。
- **兼容影响**：因不改 `types.h`/`channels.*`，对 afe/soc/protection/balancing 的 ABI 与编译**零影响**。

### 2.3 模块私有新增（不在 `types.h`）
- 新增**周期合法化纯函数**（暂名 `bms_comm_clamp_period_ms`，最终名/签名由 ③ 定）：入参为请求周期（`int32_t`），返回钳制到 `[P_min, P_max]` 且 > 0 的生效周期。原型置于 `app/include/bms/comm.h`，实现置于 `comm.c` 或新拆 `comm_period.c`（由 ③ 裁定，以利单测只链核心源）。
- `P_min`/`P_max` 以何种形式承载（`CONFIG_*` 衍生 / 头文件常量 / 字面量）属设计决策，本文不规定。

### 2.4 配置 `Kconfig`
- 对既有 `config BMS_COMM_REPORT_PERIOD_MS` **新增 `range <P_min> <P_max>`**（ADR-COMM-02）；`depends on BMS_COMM`、`default 200` 保持不变（ADR-COMM-01）。`P_min`/`P_max` 数值由 ③ 收敛（约束：`P_min > 0`、`P_min ≤ 200 ≤ P_max`、`P_max ≥ 200`）。

---

## 3. 线程模型

| 线程 | 归属模块 | 优先级（数值） | 周期/触发 | 与安全线程的相对关系 |
|---|---|---|---|---|
| `bms_prot_tid` | protection（**安全**） | **4（最高）** | 事件驱动（订阅 chan_cell_meas） | 基准——最高优先级 |
| `bms_afe_tid` | afe | 6 | 周期 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS`=100ms | 低于 protection |
| `bms_soc_tid` | soc | 7 | 事件驱动 | 低于 protection |
| `bms_bal_tid` | balancing | 7 | 事件驱动 | 低于 protection |
| **`bms_comm_tid`** | **comm（本特性）** | **8（最低，不变）** | **周期** = 合法化后的 `CONFIG_BMS_COMM_REPORT_PERIOD_MS`（默认 200ms） | **数值 8 > 4，优先级最低，劣于全部其它模块 ✓** |

**结论（线程模型）**：
- comm 线程优先级**维持 8（最低）不变**，满足 ⚠️ REQ-COMM-006「不抬升优先级、保护/采样恒优先于上报」红线，**无需调整**（ADR-COMM-06）。
- comm 不新增线程，复用既有 `bms_comm_tid` 单一周期工作线程。
- **周期来源变更**：线程睡眠时长由「直接取宏」改为「取**合法化纯函数返回值**」（ADR-COMM-03/04），保证睡眠周期恒 > 0；触发模型仍为固定周期轮询，节拍由生效周期决定。
- 三处 `zbus_chan_read` 维持有限超时 `K_MSEC(50)`（ADR-COMM-06），无 `K_FOREVER`，读取失败即跳过本次该帧上报、继续循环，不阻塞、不拖累安全链。

---

## 4. 失效安全影响分析

| 失效安全红线 | 本特性符合性 | 落点（ADR/机制） |
|---|---|---|
| 默认接触器 OPEN，仅 NORMAL 才 CLOSED | **不触碰**——comm 不参与接触器/保护决策，仅读取上报 | ADR-COMM-07：仅读三通道，不发布保护语义 |
| 安全线程优先级更高 | **满足**——comm(8) 劣于 protection(4)/afe(6)/soc(7)/balancing(7) | ADR-COMM-06：维持 prio 8（最低） |
| 不拖累安全链 | **满足**——读取有限超时 `K_MSEC(50)`、不引入无限阻塞 | ADR-COMM-06：保持 `K_MSEC(50)`，禁 `K_FOREVER` |
| 周期恒 > 0，不退化忙等 | **满足**——编译期 `range` + 运行期钳制双保险 | ADR-COMM-02 + ADR-COMM-04：不变式 `r > 0` |

**新增风险与缓解（架构层）**：
- **忙等抢占（⚠️ 核心风险）**：误配 0/负/过小周期使 `k_msleep` 退化，最低优先级 comm 抢占 CPU、间接拖累 protection/afe。缓解：ADR-COMM-02（编译期 `range` 阻断）+ ADR-COMM-04（运行期钳制，不变式 `P_min ≤ r ≤ P_max` 且 `r > 0`）双保险（⚠️ REQ-COMM-005）。
- **静默钳制误导调参**：越界被钳制却无提示，现场误以为配置生效。缓解：ADR-COMM-05 钳制结果确定 + 启动日志反映生效值（⚠️ REQ-COMM-007）。
- **改动误触安全相对关系**：周期改动若顺手改了优先级/超时则破坏安全序。缓解：ADR-COMM-06 明令优先级与读取超时不在本特性改动范围；评审/单测留痕（⚠️ REQ-COMM-006）。
- **范式回归风险**：comm 首次引入纯函数 + 新单测套件，牵动 `tests/bms/comm/CMakeLists.txt` 源复用（R5）。缓解：ADR-COMM-03/08 严格对齐既有 protection/afe 范式，签名/源拆分在 ③/④/⑤ 同步——属下游执行项，架构层已标注。

**边界声明（评审留痕）**：comm 为**非安全关键**上报流；上报周期是其私有调度参数，**绝不**经任何路径进入过充/过放/过流/过温或接触器开合判定，亦**不**改变任何安全线程的优先级、超时与触发。该边界由 ADR-COMM-06/07 固化，protection 等安全模块的独立性与优先级序不受本特性影响。

---

## 5. 需求 → 架构决策 追溯（回填迭代计划用）

| 需求 ID | 架构决策（ADR） |
|---|---|
| REQ-COMM-001 | ADR-COMM-01、ADR-COMM-08 |
| REQ-COMM-002 | ADR-COMM-01、ADR-COMM-08 |
| REQ-COMM-003 | ADR-COMM-01 |
| REQ-COMM-004 | ADR-COMM-02、ADR-COMM-03、ADR-COMM-08 |
| ⚠️ REQ-COMM-005 | ADR-COMM-02、ADR-COMM-03、ADR-COMM-04、ADR-COMM-08 |
| ⚠️ REQ-COMM-006 | ADR-COMM-06、ADR-COMM-07 |
| ⚠️ REQ-COMM-007 | ADR-COMM-03、ADR-COMM-05、ADR-COMM-08 |

> 准出自检：每条需求均映射至少一项 ADR；ADR 均记录决策+理由+涉及模块/通道+关联需求；明确「钳制逻辑以纯函数承载、可单测」拆分（ADR-COMM-03/08）；通道/`types.h` 变更结论为零新增（复用优先，ADR-COMM-07）；线程优先级安全相对关系明确（comm 8 最低，劣于 protection 4）（ADR-COMM-06）；失效安全影响已分析（编译期 `range` + 运行期钳制双保险、优先级与超时不动）。`P_min`/`P_max` 具体数值与纯函数精确签名/源拆分留交 ③ `bms-designer`。待回填 `00-iteration-plan.md` 第 3 节「架构决策」列。

_状态：DONE（② 架构已产出）_
