# 迭代计划：SOC 库仑计数估算

> 特性 slug：`soc-coulomb`
> 编排者：bms-orchestrator（敏捷-V 混合「小 V」流程）
> 本文件只产出编排计划，不含实现。各阶段由主线程据此派发对应 subagent（subagent 不可嵌套调用）。
> 交付物语言：中文。构建/测试以 **Windows venv** 为准（`run-tests-coverage.ps1`，默认板 `mps2/an386`）。

---

## 1. 特性目标与价值、优先级

**Backlog 顶部条目（原文）**
> 补全 SOC 模块的库仑计数估算：订阅 `chan_cell_meas`，对 `pack_current_ma` 按采样周期积分估算 `soc_permille`，发布到 `chan_soc`。

**目标**
- 把 SOC 模块当前的「平均电压线性映射」桩实现（`app/src/bms/soc/soc.c` 的 `bms_soc_estimate`）升级为**库仑计数（安时积分）**估算：
  - 对每帧测量的 `pack_current_ma`（充电为正，单位 mA）按相邻两帧的时间间隔积分，累计转移电荷量；
  - 将累计电荷换算为荷电量增量，更新并夹紧 `soc_permille`（0..1000‰）后发布到 `chan_soc`。
- 采样周期/时间间隔来源：优先使用 `bms_cell_meas.timestamp_ms` 的帧间差值（鲁棒、对丢帧自适应）；以 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS`（当前 100ms）作为缺省/合理性校验。

**价值**
- 电压映射在平台期（LFP 等）严重失真；库仑计数提供短期高精度 SOC，是后续卡尔曼/混合估算与里程/可用能量上报的基础。
- 直接服务于 comm 模块的 SOC 上报与 balancing 决策质量。

**优先级**：高（backlog 顶部，单一增量特性，依赖链已就绪：`chan_cell_meas` 已含 `pack_current_ma` 与 `timestamp_ms`）。

**非目标（本迭代显式排除）**
- SOH 估算（保持 `soh_permille = 1000` 桩）。
- 卡尔曼/OCV 开路电压校正、温度补偿、自放电、库仑效率建模（仅预留接口与追溯位）。
- 上电初值的精确 OCV 标定（本迭代用电压映射做一次性初始化即可）。

---

## 2. 小 V 派发清单（①需求 → ⑥CICD）

> 规则：每阶段产出必须能回溯到上一层（见第 3 节追溯链）；失效安全相关项（标 ⚠️）必须在需求与测试阶段被显式覆盖；任一阶段准出未达标则阻塞，不得进入下一阶段。

### - [ ] ① 需求 —— `bms-requirements`
- 调用 agent：`bms-requirements`
- 输入文件：本计划 `docs/features/soc-coulomb/00-iteration-plan.md`；`app/include/bms/types.h`、`app/include/bms/soc.h`、`app/Kconfig`
- 预期产出文件：`docs/features/soc-coulomb/01-requirements.md`
- 内容要点：功能需求（积分公式、单位与符号约定、SOC 夹紧、发布触发条件）、上电初始化策略、时间间隔来源与异常（首帧、丢帧、时间戳回绕/非单调）、⚠️ 失效安全需求（见第 4 节）、可量化验收判据（精度/边界/容差）
- 准出判据：每条需求有唯一 `REQ-SOC-Cxx` ID、可测试、含明确数值容差；⚠️ 失效安全需求齐备；回填第 3 节「需求ID」列

### - [ ] ② 架构 —— `bms-architect`
- 调用 agent：`bms-architect`
- 输入文件：`01-requirements.md`；`app/src/bms/channels.c`、`app/include/bms/channels.h`、`app/src/bms/soc/soc.c`（zbus 解耦现状）
- 预期产出文件：`docs/features/soc-coulomb/02-architecture.md`（含 ADR）
- 关键架构决策（待定，需 ADR 记录）：
  - **状态归属**：库仑积分需要跨帧状态（累计电荷、上一帧时间戳、SOC 当前值）。决策其存放位置——模块内静态状态 vs 通过扩展的 `bms_soc_estimate` 入参传入（影响纯函数可单测性，对齐 protection 的「纯逻辑/线程分离」范式）。
  - **接口形态**：是否需要新增纯函数（如 `bms_soc_coulomb_step(state, meas, &out)`）以保持可单测、无副作用；现有 `bms_soc_estimate` 是否保留/改签名。
  - **初值与跨帧**：上电初始化路径、`timestamp_ms` 差值计算与异常回退到 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS`。
  - **配置项**：是否新增 `CONFIG_BMS_SOC_PACK_CAPACITY_MAH`、`CONFIG_BMS_SOC_INIT_FROM_VOLTAGE` 等。
  - 维持 zbus 解耦：订阅 `chan_cell_meas`、发布 `chan_soc` 不变，不改 channel 定义。
- 准出判据：每条需求映射到至少一项架构决策；ADR 记录取舍；明确「纯逻辑可单测」的拆分；回填第 3 节「架构决策」列

### - [ ] ③ 详细设计 —— `bms-designer`
- 调用 agent：`bms-designer`
- 输入文件：`02-architecture.md`；`app/src/bms/soc/soc.c`、`app/include/bms/soc.h`、`app/include/bms/types.h`
- 预期产出文件：`docs/features/soc-coulomb/03-design.md`
- 内容要点：函数签名与数据结构（积分状态结构体、纯函数原型）、定点积分算法与防溢出（`int64_t` 电荷累加、mA·ms→mAh 的换算与舍入）、夹紧与饱和、首帧/丢帧/时间戳异常的具体处理分支、新增 Kconfig 项的默认值与范围、伪代码、单元可测点清单
- 准出判据：设计项可直接编码；定点运算给出量纲推导与溢出边界证明；每个设计项回链需求ID 与架构决策；回填第 3 节「设计项」列

### - [ ] ④ 编码 —— `bms-coder`
- 调用 agent：`bms-coder`
- 输入文件：`03-design.md`；`app/src/bms/soc/soc.c`、`app/include/bms/soc.h`、`app/include/bms/types.h`、`app/Kconfig`
- 预期产出/修改文件：
  - `app/src/bms/soc/soc.c`（实现库仑积分、跨帧状态、发布逻辑）
  - `app/include/bms/soc.h`（新增/调整纯函数与状态结构原型）
  - `app/Kconfig`（如设计要求新增 `CONFIG_BMS_SOC_*`）
  - 若涉及共享类型：`app/include/bms/types.h`
- 准出判据：按设计实现；保持 zbus 发布/订阅范式与「纯逻辑函数可单测」；Windows venv 构建通过——`.venv\Scripts\python.exe -m west build -b mps2/an386 app`；无新增编译告警；代码注释回链设计项；回填第 3 节「代码位置」列

### - [ ] ⑤ 测试 —— `bms-tester`
- 调用 agent：`bms-tester`
- 输入文件：`01-requirements.md`、`03-design.md`；既有用例 `tests/bms/soc/src/main.c`、`tests/bms/soc/testcase.yaml`、`tests/bms/soc/CMakeLists.txt`、`tests/bms/soc/prj.conf`
- 预期产出/修改文件：`tests/bms/soc/src/main.c`（新增库仑积分用例，保留/调整既有电压映射用例）；如纯函数签名变化则同步 `CMakeLists.txt` 复用源
- 用例覆盖（至少）：恒定充/放电固定时长后的 SOC 增量精度、夹紧到 0/1000‰、首帧无前序时间戳、丢帧大间隔、时间戳非单调/回退、零电流、⚠️ 失效安全相关边界（见第 4 节）
- 准出判据：每条 `REQ-SOC-Cxx` 至少一条用例；Windows venv 全绿——`powershell -File run-tests-coverage.ps1`（默认板 `mps2/an386`）；覆盖率达项目门限；回填第 3 节「测试用例」列

### - [ ] ⑥ CICD —— `bms-cicd`
- 调用 agent：`bms-cicd`
- 输入文件：`run-tests-coverage.ps1`、`tests/bms/soc/testcase.yaml`、CI 配置（如存在）
- 预期产出文件：`docs/features/soc-coulomb/06-cicd.md`；必要时更新 CI/脚本使本特性测试纳入流水线
- 准出判据：`bms.soc` 测试在流水线（Windows venv，板 `mps2/an386`）自动执行并通过；构建+测试+覆盖率链路绿；无回归

---

## 3. 可追溯链骨架表（占位，待各阶段回填）

| 需求ID | 架构决策 | 设计项 | 代码位置 | 测试用例 |
|---|---|---|---|---|
| REQ-SOC-C01 _(积分核心/安时积分)_ | ADR-SOC-C01,C03,C06 | `bms_soc_coulomb_step`（§2.2）；量纲链 §4.1；积分伪代码 §7 | `soc.c:bms_soc_coulomb_step`（积分段 `dQ=current×dt`、`acc+=dQ`、`soc_charge_to_permille`） | `test_step_charge_integration_accuracy`、`test_step_discharge_direction` |
| REQ-SOC-C02 _(时间间隔来源/异常)_ | ADR-SOC-C05,C07 | Δt 分支 B/C/E（§3）；解析逻辑 §7；`GAP_FACTOR_N`（§5） | `soc.c:bms_soc_coulomb_step`（Δt 解析段：`<=last`回退 period / `>dt_cap`夹紧）；`Kconfig:BMS_SOC_GAP_FACTOR_N` | `test_step_nonmonotonic_ts_fallback`、`test_step_frame_drop_clamped` |
| REQ-SOC-C03 _(SOC 夹紧 0..1000‰)_ | ADR-SOC-C04,C11 | 夹紧 §4.4 + §7 clamp 0/1000 | `soc.c:soc_charge_to_permille`（pm<0→0 / pm>1000→1000） | `test_clamp_over_full`、`test_step_clamp_to_full`、`test_step_clamp_to_empty` |
| REQ-SOC-C04 _(上电初始化策略)_ | ADR-SOC-C04,C07 | `bms_soc_estimate` 语义收敛（§2.1）；首帧分支 B（§3）；自洽校验 §4.4；`INIT_FROM_VOLTAGE`（§5） | `soc.c:bms_soc_estimate`（行为不变）；`soc.c:bms_soc_coulomb_step` 首帧分支 B（acc0=permille×DEN）；`Kconfig:BMS_SOC_INIT_FROM_VOLTAGE` | `test_full_charge`、`test_empty`、`test_step_first_frame_init`、`test_step_init_only_once`、`test_reset_clears_state` |
| REQ-SOC-C05 _(发布到 chan_soc 条件)_ | ADR-SOC-C01,C09 | 返回码语义 0发布/-EAGAIN不发布（§2.2）；线程发布 §6 | `soc.c:soc_thread`（rc==0 发布、out->soh=1000、out->ts=meas->ts） | `test_step_first_frame_init`（ts/soh）、`test_step_over_range_current_skipped`（-EAGAIN 不发布） |
| ⚠️ REQ-SOC-C06 _(失效安全：异常数据隔离/安全降级)_ | ADR-SOC-C05,C11 | 分支 A/C/E/F（§3）；单帧 ΔSOC 上限 §3.1；降级闭环伪代码 §7 | `soc.c:bms_soc_coulomb_step`（分支 A 空指针-EINVAL / F 超量程-EAGAIN）；`soc.c:soc_current_in_range` | `test_step_null_returns_einval`、`test_step_over_range_current_skipped`、`test_step_over_range_negative_current_skipped`、`test_step_frame_drop_clamped`、`test_step_recovers_after_bad_frame` |
| REQ-SOC-C07 _(电流符号/积分方向一致性)_ | ADR-SOC-C03,C11 | 方向随符号 §4.1；`dQ=current×dt`（§7） | `soc.c:bms_soc_coulomb_step`（`dQ=(int64)pack_current_ma*(int64)dt_ms`） | `test_step_charge_integration_accuracy`（正）、`test_step_discharge_direction`（负）、`test_step_zero_current_no_change`（零） |
| ⚠️ REQ-SOC-C08 _(失效安全：SOC 不参与接触器/保护决策)_ | ADR-SOC-C01,C10 | 仅发 chan_soc（§6）；通道零新增（§0） | `soc.c:soc_thread`（仅 `zbus_chan_pub(&chan_soc,…)`，无保护语义通道） | 结构性约束，超出纯函数单测；见 05-test-report §4，建议集成测试/评审确认 |
| ⚠️ REQ-SOC-C09 _(失效安全：不阻塞安全链/实时性)_ | ADR-SOC-C08,C09 | prio 7 + K_MSEC(50) + 发布超时丢弃（§6） | `soc.c:SOC_THREAD_PRIO=7`；`soc.c:soc_thread`（读写 `K_MSEC(50)`、发布失败不重试） | 线程优先级/超时属性，超出纯函数单测；见 05-test-report §4，建议系统测试/评审确认 |
| REQ-SOC-C10 _(定点积分防溢出 int64)_ | ADR-SOC-C06 | `int64_t acc`（§1.1）；溢出边界证明 §4.2/4.3 | `soc.h:bms_soc_coulomb_state.acc_charge_ma_ms (int64_t)`；`soc.c` 积分先提升 int64 | `test_step_no_overflow_24h` |
| REQ-SOC-C11 _(精度与容差 ≤±1‰)_ | ADR-SOC-C03,C06 | 对称舍入 §4.4；精度边界 §4.5 | `soc.c:soc_charge_to_permille`（对称舍入 `acc±DEN/2`） | `test_step_charge_integration_accuracy`（zassert_within ±1） |
| REQ-SOC-C12 _(额定容量可配置 mAh)_ | ADR-SOC-C07 | `CONFIG_BMS_SOC_PACK_CAPACITY_MAH`（§5）；参数化测试说明 §5.1 | `Kconfig:BMS_SOC_PACK_CAPACITY_MAH`；`soc.c` DEN=`CONFIG_BMS_SOC_PACK_CAPACITY_MAH*3600`（无硬编码容量） | C12-1 由代码 DEN 不硬编码满足；C12-2（按 1/C 变化）需多 prj.conf 参数化，见 05-test-report §4 |

> 需求ID 列已由 ① bms-requirements 回填（见 `01-requirements.md`）。C01~C06 为原骨架，C07~C12 为需求阶段细化新增。
> 回填规则：每阶段完成后在对应单元格填入真实 ID/决策号/函数名/文件:行/用例名；任一行出现空链视为追溯断裂，阻塞准出。

---

## 4. 失效安全考量与本特性风险点

**失效安全红线对齐（项目级）**：默认接触器 OPEN，仅判定 NORMAL 才 CLOSED；安全相关线程优先级更高。SOC 模块为**非安全关键**信息流，但必须保证「不拖累、不污染」安全链。

**⚠️ 失效安全考量**
1. **SOC 不得参与接触器决策**：库仑计数存在累计漂移，绝不能作为过充/过放保护的唯一判据；保护仍由 protection 模块基于电压/电流/温度独立判定。需求阶段须显式声明此边界。
2. **不阻塞安全线程**：SOC 线程优先级（当前 `SOC_THREAD_PRIO 7`）须低于 protection 安全线程；`zbus_chan_pub/read` 使用有限超时（现为 `K_MSEC(50)`），不得 `K_FOREVER` 阻塞导致背压。
3. **坏数据隔离**：异常输入（NaN 不适用于定点，但含时间戳非单调、超量程电流、间隔异常大）须被检测并安全降级（跳过本帧/夹紧），不得让 SOC 跳变误导上层。

**风险点**
- **R1 定点溢出**：mA × ms 累加，长时间运行 `int32` 必溢出 → 强制 `int64_t` 电荷累加 + 量纲推导（设计阶段证明边界）。
- **R2 时间间隔不可靠**：首帧无前序、丢帧、`timestamp_ms`（`uint32` k_uptime）回绕或非单调 → 定义回退到 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS` 与合理性夹紧。
- **R3 初值与漂移**：上电 SOC 初值若错，库仑计数会长期带偏移；本迭代用一次性电压映射初始化，OCV 校正列为后续。
- **R4 容量参数**：积分换算依赖额定容量（mAh）。若硬编码不可配 → 建议 `CONFIG_BMS_SOC_PACK_CAPACITY_MAH`，由架构/设计确认。
- **R5 接口签名变更连锁**：扩展 `bms_soc_estimate`/新增纯函数会牵动既有单测与 `tests/bms/soc/CMakeLists.txt` 的源复用 → 编码与测试阶段同步。
- **R6 充电正负号**：`pack_current_ma` 充电为正，积分方向须与之一致，避免符号反转导致 SOC 反向。

---

## 5. 迭代准入 / 准出标准

**准入（进入本迭代前需满足）**
- 依赖就绪：`chan_cell_meas` 已含 `pack_current_ma`（充电为正）与 `timestamp_ms`；`chan_soc` 已定义并由 SOC 模块发布。（均已确认）
- 基线可构建可测：Windows venv 下 `.venv\Scripts\python.exe -m west build -b mps2/an386 app` 与 `powershell -File run-tests-coverage.ps1` 当前通过。
- 本计划评审通过，第 1 节非目标范围已确认。

**准出（本迭代完成判据）**
- ① ~ ⑥ 全部复选框勾选，各阶段产出文件齐备。
- 第 3 节追溯链表无空链：每条 `REQ-SOC-Cxx` 贯通「需求→架构→设计→代码→测试」。
- 所有 ⚠️ 失效安全项均有对应需求与测试用例并通过。
- Windows venv 下：构建（板 `mps2/an386`）通过；`run-tests-coverage.ps1` 全绿，覆盖率达门限；`bms.soc` 用例含库仑积分新增用例且无回归。
- 第 1 节非目标（SOH、卡尔曼、温度补偿等）未被夹带实现，范围受控。
- CICD 流水线纳入并通过本特性测试。

---

_状态：DONE（编排计划已生成，等待主线程派发 ① bms-requirements）_
