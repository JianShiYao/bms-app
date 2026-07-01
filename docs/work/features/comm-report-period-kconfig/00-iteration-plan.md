# 迭代计划：comm 模块 CAN 上报周期 Kconfig 可配

> 特性 slug：`comm-report-period-kconfig`
> 编排者：bms-orchestrator（敏捷-V 混合「小 V」流程）
> 本文件只产出编排计划，不含实现。各阶段由主线程据此派发对应 subagent（subagent 不可嵌套调用）。
> 交付物语言：中文。构建/测试以 **Windows venv** 为准（`run-tests-coverage.ps1`，默认板 `mps2/an386`）。
> 追溯链独立成文于 [`traceability.md`](traceability.md)（套 traceability-matrix-template），本计划仅引用、不内嵌追溯表。

---

## 1. 特性目标与价值、优先级

**Backlog 顶部条目（原文）**
> 把 comm 模块 CAN 上报周期改为由 Kconfig (`CONFIG_BMS_COMM_REPORT_PERIOD_MS`) 可配，默认 200ms。

**现状基线（brownfield，进入迭代前已勘查）**
- `app/Kconfig` 已存在 `config BMS_COMM_REPORT_PERIOD_MS`（`int`，`depends on BMS_COMM`，`default 200`）。
- `app/src/bms/comm/comm.c` 的 `comm_thread` 已用 `k_msleep(CONFIG_BMS_COMM_REPORT_PERIOD_MS)` 驱动上报周期；`bms_comm_init()` 已在启动日志打印该周期。
- **缺口**：① 该 Kconfig 项**无取值范围/合理性约束**（无 `range`，可填 0 或负值导致忙等/退化）；② **无任何 comm 模块单元测试**（comm 当前为已知测试缺口，见根 CLAUDE.md「afe/balancing/comm 尚缺测试」）；③ 该「可配上报周期」**无需求/设计/追溯记录**，配置语义（默认值、边界、失效安全含义）未被流程固化。

**目标（本迭代收敛为「形式化 + 加固 + 验证」既有实现，而非从零开发）**
- 形式化需求与设计：将「上报周期可配、默认 200ms」补成显式 `REQ-COMM-*` / `DES-COMM-*`，建立完整追溯链。
- 加固配置项：为 `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 增加合理 `range`（下限 > 0，避免 0/负值；上限给出工程上界），并校验依赖与默认值。
- 把驱动周期所依赖的**纯逻辑**（周期取值的钳制/合法化）抽成可单测纯函数，延续项目「纯函数 + 薄线程包装」范式，补齐 comm 模块**首个**单元测试套件。
- 验证：构建通过、`bms.comm` 新测试套件全绿、覆盖率不回退、CI 门全绿。

**价值**
- 不同部署（实车 CAN 总线负载 / 仿真 / 调参）需要不同上报节奏；编译期可配避免改源码。
- 把已存在但「无主」的配置项纳入需求→设计→测试追溯，消除流程债与测试缺口，提升 comm 模块从 0 到 1 的可测试性基线。

**优先级**：中（backlog 顶部；增量小、依赖已就绪；主要价值在补流程合规与测试缺口，而非新功能）。

**非目标（本迭代显式排除）**
- **不**实现真实 CAN 收发（`comm_tx_meas` / `can_send` 仍为桩，保持 TODO）。
- **不**改 zbus 通道定义、不改其它模块周期（afe/protection/soc/balancing 周期不动）。
- **不**做运行期（运行时）动态可调周期（仅编译期 Kconfig；运行期可调列为后续）。
- **不**改 comm 的「变化时才 INF、逐帧 DBG」日志策略。
- **不**引入新 zbus 通道或新数据结构（`types.h` 不动）。

---

## 2. 小 V 派发清单（①需求 → ⑥CICD）

> 规则：每阶段产出必须能回溯到上一层（追溯链见 [`traceability.md`](traceability.md)）；失效安全相关项（标 ⚠️）必须在需求与测试阶段被显式覆盖；任一阶段准出未达标则阻塞，不得进入下一阶段。
> ID 规范：需求 `REQ-COMM-NNN`、设计 `DES-COMM-NNN`（域 = COMM，三位数字，不加临时式后缀）。

### - [ ] ① 需求 —— `bms-requirements`
- 调用 agent：`bms-requirements`
- 输入文件：本计划 `docs/work/features/comm-report-period-kconfig/00-iteration-plan.md`；`app/Kconfig`（§BMS_COMM 段）、`app/src/bms/comm/comm.c`、`app/include/bms/comm.h`
- 预期产出文件：`docs/work/features/comm-report-period-kconfig/01-requirements.md`
- 内容要点：功能需求（上报周期由 `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 决定、默认 200ms、`depends on BMS_COMM`）、取值范围与边界（下限 > 0 的具体下界、工程上界、越界时的编译期/运行期行为）、周期合法化/钳制的逻辑语义、⚠️ 失效安全需求（见第 4 节：周期配置不得阻塞或拖累安全链、不得退化为忙等）、可量化验收判据（默认值、边界值、钳制结果）
- 准出判据：每条需求有唯一 `REQ-COMM-NNN` ID、可测试、含明确数值（默认 200、range 上下界）；⚠️ 失效安全需求齐备；同步把「需求ID/需求摘要」回填 [`traceability.md`](traceability.md)

### - [ ] ② 架构 —— `bms-architect`
- 调用 agent：`bms-architect`
- 输入文件：`01-requirements.md`；`app/src/bms/comm/comm.c`、`app/include/bms/comm.h`、`docs/concept/architecture.md`（comm 模块章节、线程模型/优先级）
- 预期产出文件：`docs/work/features/comm-report-period-kconfig/02-architecture.md`（含 ADR）
- 关键架构决策（待定，需 ADR 记录）：
  - **可测性拆分**：周期取值的合法化逻辑是否抽成纯函数（如 `bms_comm_report_period_ms(void)` 或 `bms_comm_clamp_period(int)`），以脱离线程在 ztest 直测——对齐项目「纯逻辑 + 薄线程包装」范式（参照 `bms_protection_evaluate`）。
  - **越界处理归属**：`range` 在 Kconfig 层硬约束 vs 运行期纯函数再钳制（防御性）——决策二者边界，避免双重真相。
  - **线程影响面**：`comm_thread` 仍以 `k_msleep(周期)` 驱动；确认不改线程优先级（comm=8，最低）、不改 zbus 读取超时语义，维持「不拖累安全链」。
  - **配置面**：是否保留单一 `CONFIG_BMS_COMM_REPORT_PERIOD_MS`（倾向是）、`range` 的具体上下界取值依据。
- 准出判据：每条需求映射到至少一项架构决策；ADR 记录取舍（尤其纯函数拆分 vs 直接读宏）；明确「纯逻辑可单测」的拆分点；回填 [`traceability.md`](traceability.md)「设计」列上游对应关系

### - [ ] ③ 详细设计 —— `bms-designer`
- 调用 agent：`bms-designer`
- 输入文件：`02-architecture.md`；`app/src/bms/comm/comm.c`、`app/include/bms/comm.h`、`app/Kconfig`
- 预期产出文件：`docs/work/features/comm-report-period-kconfig/03-design.md`
- 内容要点：`app/Kconfig` 中 `BMS_COMM_REPORT_PERIOD_MS` 的最终定义（`range <下界> <上界>`、`default 200`、`depends on BMS_COMM`、help 文案）；周期合法化纯函数签名与原型（入参/返回/钳制规则）；`comm_thread` 调用点改造（调用纯函数取周期）；`bms_comm_init` 日志；边界与异常分支（越界值如何钳制、下界保证 > 0）；单元可测点清单；每个设计项分配 `DES-COMM-NNN` 并回链 `REQ-COMM-NNN`
- 准出判据：设计项可直接编码；`range` 上下界有工程依据；周期合法化逻辑给出确定性钳制规则（可断言）；回填 [`traceability.md`](traceability.md)「设计」列（`DES-COMM-NNN`）

### - [ ] ④ 编码 —— `bms-coder`
- 调用 agent：`bms-coder`
- 输入文件：`03-design.md`；`app/src/bms/comm/comm.c`、`app/include/bms/comm.h`、`app/Kconfig`
- 预期产出/修改文件：
  - `app/Kconfig`（为 `BMS_COMM_REPORT_PERIOD_MS` 增加 `range` 与 help）
  - `app/src/bms/comm/comm.c`（周期合法化纯函数 + `comm_thread` 调用点改造）
  - `app/include/bms/comm.h`（导出周期合法化纯函数原型，供 ztest 复用）
- 准出判据：按设计实现；保持 zbus 读写超时与日志策略不变、线程优先级不变；Windows venv 构建通过——`..\.venv\Scripts\python.exe -m west build -b mps2/an386 app -p always`；无新增编译告警；代码注释回链 `DES-COMM-NNN`；回填 [`traceability.md`](traceability.md)「代码位置」（隐含于设计列/测试列对应实现）

### - [ ] ⑤ 测试 —— `bms-tester`
- 调用 agent：`bms-tester`
- 输入文件：`01-requirements.md`、`03-design.md`；既有测试套件范例 `tests/bms/soc/`（`testcase.yaml`/`CMakeLists.txt`/`prj.conf`/`src/main.c` 结构）；被测源 `app/src/bms/comm/comm.c`、`app/include/bms/comm.h`
- 预期产出/新增文件（comm 模块**首个**测试套件）：
  - `tests/bms/comm/testcase.yaml`（`platform_allow: mps2/an386, native_sim` + tags）
  - `tests/bms/comm/CMakeLists.txt`（链接 `app/src/bms/comm/*.c` 与 `app/include`）
  - `tests/bms/comm/prj.conf`（`CONFIG_ZTEST=y`，按需设/覆盖 `CONFIG_BMS_COMM_REPORT_PERIOD_MS`）
  - `tests/bms/comm/src/main.c`（`ZTEST_SUITE(bms_comm, …)`，用例顶部 `/* Verifies REQ-COMM-NNN */` 回链）
- 用例覆盖（至少）：默认周期为 200ms、周期合法化纯函数对下界以下值的钳制、对上界以上值的钳制、合法范围内原值透传、⚠️ 周期不退化为 0/负（失效安全边界，见第 4 节）；如可行用多 `prj.conf` 或参数化验证不同 `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 取值
- 准出判据：每条 `REQ-COMM-NNN` 至少一条用例（或显式标注替代验证：分析/检视/演示，并在追溯表注明）；Windows venv 全绿——`powershell -ExecutionPolicy Bypass -File ..\run-tests-coverage.ps1 -Board mps2/an386`；覆盖率不回退；回填 [`traceability.md`](traceability.md)「验证方法/测试用例/状态」列

### - [ ] ⑥ CICD —— `bms-cicd`
- 调用 agent：`bms-cicd`
- 输入文件：`run-tests-coverage.ps1`、`tests/bms/comm/testcase.yaml`、`.github/workflows/ci.yml`
- 预期产出文件：`docs/work/features/comm-report-period-kconfig/06-cicd.md`；确认新 `bms.comm` 套件被 Twister 自动发现并纳入 CI（`tests/` 通配，通常无需改 CI，验证即可）
- 准出判据：`bms.comm` 测试在流水线（CI native_sim / 本地 Windows venv 板 `mps2/an386`）自动执行并通过；构建 + 测试 + 覆盖率链路绿；CI 6 门全绿；无回归

---

## 3. 可追溯链

本特性追溯矩阵独立维护于 [`traceability.md`](traceability.md)（套用 traceability-matrix-template，列：需求ID | 需求摘要 | 设计 | 验证方法 | 测试用例 | 状态），各阶段按本计划准出判据回填。本计划不内嵌追溯表。

**回填规则**：每阶段完成后在对应单元格填入真实 ID / `DES-COMM-NNN` / 用例名，并推进「状态」（草稿 → 已实现 → 已验证）；任一行出现空链或停在「缺口」无替代验证说明，视为追溯断裂，阻塞 DoD 准出。

---

## 4. 失效安全考量与本特性风险点

**失效安全红线对齐（项目级）**：默认接触器 OPEN，仅判定 NORMAL 才 CLOSED；安全相关线程优先级更高。comm 为**非安全关键**信息上报流（优先级最低 = 8），但本特性改动「周期」直接影响线程节奏，必须保证「不拖累、不阻塞」安全链。

**⚠️ 失效安全考量**
1. **周期不得退化为忙等**：若 `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 被配为 0 或负，`k_msleep(0/负)` 会让 comm 线程几乎不让出 CPU，挤占调度、间接拖累其它线程。须用 Kconfig `range`（下界 > 0）+ 运行期防御性钳制双保险，保证实际睡眠周期恒 > 0。需求阶段须显式声明此边界。
2. **不改变线程优先级与超时语义**：comm 线程仍为最低优先级 8，`zbus_chan_read` 仍用有限超时（现 `K_MSEC(50)`），不得因本改动引入 `K_FOREVER` 或抬升优先级，确保保护/采样线程始终优先。
3. **配置无声退化的可观测性**：越界配置被钳制时应有日志/确定性行为，避免「配了却没生效」的静默偏差误导现场调参。

**风险点**
- **R1 周期边界配置**：0/负/极小值导致忙等或刷屏 → Kconfig `range` + 纯函数钳制（设计阶段定下界，如 ≥ 10ms 之类工程下界）。
- **R2 comm 零测试基线**：comm 模块此前无任何 ztest，新建套件需打通 `CMakeLists.txt` 源链接、`prj.conf`、Twister 发现链路（参照 `tests/bms/soc/`），首次搭建有踩坑成本。
- **R3 brownfield 双真相**：Kconfig `range` 与运行期钳制若规则不一致，会出现「Kconfig 允许但代码又钳制」的矛盾 → 架构 ADR 须明确二者边界（建议 Kconfig 硬约束为主、运行期钳制为防御性兜底，规则一致）。
- **R4 范围蔓延**：易顺手去实现真实 CAN 收发或运行期可调 → 严守第 1 节非目标，仅做周期可配 + 加固 + 测试。
- **R5 默认值回归**：改 Kconfig 时误改 `default 200` 或破坏 `depends on BMS_COMM` → 需求/测试显式断言「默认 = 200」与依赖关系。

---

## 5. 迭代准入 / 准出标准

**准入（DoR，进入本迭代前需满足）**
- 依赖就绪：`CONFIG_BMS_COMM_REPORT_PERIOD_MS` 已存在（默认 200）、`comm.c` 已消费该宏；`BMS_COMM` 模块开关存在。（均已确认）
- 基线可构建可测：Windows venv 下 `..\.venv\Scripts\python.exe -m west build -b mps2/an386 app -p always` 与 `powershell -ExecutionPolicy Bypass -File ..\run-tests-coverage.ps1 -Board mps2/an386` 当前通过（基线 11/11）。
- 本计划评审通过，第 1 节非目标范围已确认。

**准出（DoD，本迭代完成判据；不低于 workflow.md §1.3 通用下限）**
- ① ~ ⑥ 全部复选框勾选，各阶段产出文件齐备。
- [`traceability.md`](traceability.md) 无空链：每条 `REQ-COMM-NNN` 贯通「需求→设计→验证/测试」，状态推进至「已验证」（或缺口附替代验证说明）。
- 所有 ⚠️ 失效安全项（周期 > 0 不忙等、不抬优先级、不阻塞安全链）均有对应需求与测试/分析并通过。
- Windows venv 下：构建（板 `mps2/an386`）通过；`run-tests-coverage.ps1` 全绿（含**新增** `bms.comm` 套件），覆盖率不回退；无回归。
- 第 1 节非目标（真实 CAN 收发、运行期可调、改其它模块周期等）未被夹带实现，范围受控。
- CI 6 门全绿，新 `bms.comm` 套件纳入流水线并通过。

---

_状态：DONE（编排计划已生成，等待主线程派发 ① bms-requirements）_
