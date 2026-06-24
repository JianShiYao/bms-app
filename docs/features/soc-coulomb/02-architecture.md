# 架构设计：SOC 库仑计数估算

> 特性 slug：`soc-coulomb`
> 阶段：敏捷-V 左腿第②层 —— 架构设计（`bms-architect`）
> 输入：`01-requirements.md`、`00-iteration-plan.md`；`app/src/bms/soc/soc.c`、`app/include/bms/soc.h`、`app/src/bms/channels.c`、`app/include/bms/channels.h`、`app/include/bms/types.h`、`app/Kconfig`、`app/src/bms/afe/afe.c`、`app/src/bms/protection/protection.c`（既有范式）
> 交付物语言：中文。
> 本文件只到**模块边界与接口契约**为止，**不下沉**到状态机/逐函数实现（那是 ③ `bms-designer` 的范围）。

---

## 0. 现状基线（架构落点扫描）

| 维度 | 现状 | 对本特性的含义 |
|---|---|---|
| 通道（订阅） | SOC 模块已 `ZBUS_CHAN_ADD_OBS(chan_cell_meas, soc_sub, 3)` | **复用**，输入不变 |
| 通道（发布） | SOC 模块已发布 `chan_soc`（`struct bms_soc`） | **复用**，输出不变 |
| 纯逻辑函数 | `bms_soc_estimate(meas, out)` 无副作用、被线程与单测复用 | 当前为**无状态**（仅当帧电压映射）；库仑积分需**跨帧状态** |
| 线程 | `K_THREAD_DEFINE(bms_soc_tid, … SOC_THREAD_PRIO=7 …)` | 复用同一线程，优先级无需改动 |
| zbus 读写超时 | 读 `K_MSEC(50)`、写 `K_MSEC(50)` | 已满足 REQ-SOC-C09 有限超时红线，复用 |
| 线程优先级全景 | protection=4（安全，最高）、afe=6、**soc=7**、balancing=7、comm=8 | soc(7) 数值 > protection(4)，**已满足** REQ-SOC-C09 |
| 数据类型 | `bms_cell_meas`（输入）、`bms_soc`（输出）均已就绪 | 输出字段足够；**跨帧积分状态不属于通道载荷** |
| 配置 | 有 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS=100`；无容量配置 | 需新增容量等 `CONFIG_BMS_SOC_*` |

**复用结论**：通道、线程、输出数据结构、超时策略**全部复用**，无新增。本特性是 SOC 模块**内部算法**从「电压映射」到「库仑积分」的纵向升级，不改变其在 zbus 架构中的拓扑位置。

---

## 1. 架构决策清单（ADR 级）

> 每条：决策 + 理由 + 涉及模块/通道 + 关联需求 ID。决策号 `ADR-SOC-Cxx`，供下游设计/测试回链。

### ADR-SOC-C01　复用 `chan_cell_meas` / `chan_soc`，零新增通道
- **决策**：输入沿用订阅 `chan_cell_meas`，输出沿用发布 `chan_soc`；不新增、不修改任何 `ZBUS_CHAN_DEFINE`。
- **理由**：依赖链已就绪（`bms_cell_meas` 已含 `pack_current_ma` 充电为正、`timestamp_ms`）；新增通道违背「复用优先」与 zbus 解耦最小化原则。
- **涉及模块/通道**：soc 模块；`chan_cell_meas`（订阅）、`chan_soc`（发布）。
- **关联需求**：REQ-SOC-C01、C05、C08。

### ADR-SOC-C02　跨帧积分状态为模块私有，**不进 `types.h`**
- **决策**：库仑积分所需跨帧状态（累计电荷 `int64`、上一帧 `timestamp_ms`、是否已初始化标志、当前 SOC）封装为一个**模块私有状态结构体**，声明位置在 `app/include/bms/soc.h`（供纯函数与单测复用），定义/实例化在 `app/src/bms/soc.c`；**绝不**加入 `app/include/bms/types.h`。
- **理由**：`types.h` 是 zbus 通道载荷的共享类型，被 afe/protection/balancing/comm 共用；积分状态是 SOC 内部实现细节，放入 `types.h` 会污染共享 ABI、违背封装、放大改动连锁面（迭代计划 R5）。`bms_soc` 通道载荷已含输出所需字段，无需扩展。
- **涉及模块/通道**：soc 模块（`soc.h`/`soc.c`）；`types.h` **不变**。
- **关联需求**：REQ-SOC-C01、C02、C04、C10。

### ADR-SOC-C03　纯函数 + 线程分离范式（对齐 protection）
- **决策**：库仑积分逻辑实现为**无副作用纯函数**，签名形如 `bms_soc_coulomb_step(state*, meas*, out*)`（精确签名由 ③ 设计定），把「积分计算」与「线程/zbus I/O」分离；线程仅做 `zbus_sub_wait → zbus_chan_read → 调纯函数 → zbus_chan_pub`。
- **理由**：复刻既有 `bms_protection_evaluate` 的「纯逻辑/线程分离」范式，保证可单测（ztest 直接喂状态与测量帧、断言输出），满足需求大量可量化验收准则（C01/C07/C11）。
- **涉及模块/通道**：soc 模块（`soc.h` 原型、`soc.c` 实现）。
- **关联需求**：REQ-SOC-C01、C07、C11（可测性贯穿全部）。

### ADR-SOC-C04　`bms_soc_estimate` 现签名定位为「电压映射初始化器」
- **决策**：保留现有 `bms_soc_estimate(meas, out)` 的电压线性映射逻辑，将其**语义收敛为「上电一次性初值来源」**（C04 的 3000mV→0‰、4200mV→1000‰ 映射由它提供）；稳态逐帧更新走新的库仑积分纯函数。是否对外保留该符号 / 内部化，由 ③ 设计裁定，但**映射端点与夹紧行为须与现实现逐位一致**（C04 验收要求误差 0‰）。
- **理由**：C04 明确要求初值「与现桩实现一致」；复用现有映射避免重复实现与行为漂移；同时把无状态映射与有状态积分职责分清。
- **涉及模块/通道**：soc 模块；`soc.h`/`soc.c`。
- **关联需求**：REQ-SOC-C04、C03。

### ADR-SOC-C05　时间间隔来源与异常回退策略落在纯函数内
- **决策**：Δt 计算（本帧与上一帧 `timestamp_ms` 差）、首帧无前序、帧间差 ≤ 0（非单调/回绕）、丢帧（差 > N×缺省周期）的判定与回退/夹紧，全部封装在库仑积分纯函数内，依赖入参状态中的「上一帧时间戳」。回退基准为 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS`。N 因子作为可配置/常量由 ③ 设计给出（需求建议默认 N=10）。
- **理由**：时间间隔异常是坏数据主因（R2）；放在纯函数内可被测试确定性注入与验证（C02 验收要求可测）。复用 AFE 已有周期配置，不新增重复语义配置。
- **涉及模块/通道**：soc 模块；复用 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS`。
- **关联需求**：REQ-SOC-C02、C06（异常间隔）。

### ADR-SOC-C06　电荷累计采用 `int64` 承载（mA·ms 量纲）
- **决策**：积分中间量以**至少 64 位有符号整型**（`int64_t`）承载累计电荷（量纲 mA·ms），换算到 mAh / ‰ 的定点策略与舍入由 ③ 设计给出量纲推导与溢出边界证明。
- **理由**：mA×ms 累加在小时级即超 `int32`（R1）；架构层固化承载位宽，避免设计/编码阶段误用 32 位。
- **涉及模块/通道**：soc 模块（私有状态结构体字段，见 ADR-SOC-C02）。
- **关联需求**：REQ-SOC-C10、C11。

### ADR-SOC-C07　新增 `CONFIG_BMS_SOC_*` 配置项（容量必需）
- **决策**：在 `app/Kconfig`（`config BMS_SOC` 节下、`depends on BMS_SOC`）新增：
  - `CONFIG_BMS_SOC_PACK_CAPACITY_MAH`（额定容量 mAh，**必需**，无硬编码容量）；
  - 可选 `CONFIG_BMS_SOC_INIT_FROM_VOLTAGE`（上电电压映射初始化开关，默认 y）；
  - 可选 `CONFIG_BMS_SOC_GAP_FACTOR_N`（丢帧判定 N 因子，默认 10）。
  默认值/范围由 ③ 设计定稿；架构层只固定「容量必须可配」这一约束。
- **理由**：换算依赖额定容量，硬编码不利多板型适配（R4、C12）；用 Kconfig 与既有 `CONFIG_BMS_*` 风格一致。
- **涉及模块/通道**：`app/Kconfig`；soc 模块读取。
- **关联需求**：REQ-SOC-C12、C04、C02。

### ADR-SOC-C08　SOC 线程优先级维持 7，不调整（安全相对关系）
- **决策**：SOC 工作线程优先级保持 `SOC_THREAD_PRIO = 7` 不变；明确其**数值大于（优先级低于）** protection 安全线程的 `PROT_THREAD_PRIO = 4`。
- **理由**：REQ-SOC-C09 红线要求 soc 优先级数值 > protection；现状 7 > 4 已满足，**无需改动**即合规；维持现值避免引入新风险。
- **涉及模块/通道**：soc 线程 vs protection 线程。
- **关联需求**：⚠️ REQ-SOC-C09。

### ADR-SOC-C09　zbus 读/写维持有限超时 `K_MSEC(50)`，禁用无限阻塞
- **决策**：`zbus_chan_read(chan_cell_meas, …, K_MSEC(50))` 与 `zbus_chan_pub(chan_soc, …, K_MSEC(50))` 的有限超时**复用不变**；发布超时即丢弃本次发布并继续处理后续帧，不重试、不死锁。（`zbus_sub_wait` 在订阅等待路径上沿用 `K_FOREVER` 属可接受——它是空闲等待新帧，非数据读写背压路径。）
- **理由**：REQ-SOC-C09 要求数据读写路径有限超时、不拖累安全链；现状已合规，复用即可。
- **涉及模块/通道**：soc 线程；`chan_cell_meas` 读、`chan_soc` 写。
- **关联需求**：⚠️ REQ-SOC-C09、C05（被跳过帧不发布）。

### ADR-SOC-C10　输出通道唯一性边界：SOC 不触达保护/接触器语义
- **决策**：SOC 模块的**唯一输出通道**为 `chan_soc`；严禁向 `chan_prot_state` 或任何接触器控制语义通道发布；SOC 不作为过充/过放/接触器判据。protection 模块继续基于电压/电流/温度独立判定（其对 `chan_soc` 的订阅当前仅预留、不参与判定，本特性不改变该现状）。
- **理由**：库仑计数有累计漂移，不可作安全判据（失效安全 1、C08）；架构层固化职责边界防误用。
- **涉及模块/通道**：soc（仅发 `chan_soc`）；protection（独立判定，边界声明）。
- **关联需求**：⚠️ REQ-SOC-C08。

### ADR-SOC-C11　坏数据安全降级在纯函数内闭环，状态不被污染
- **决策**：坏数据（空指针、时间戳非单调/回绕、电流超量程、间隔异常大）的检测与降级（空指针返回负 errno 且不更新状态/不发布；其余跳过本帧积分或对单帧 ΔSOC 夹紧到设计上限）在纯函数内闭环；异常帧**不得**破坏累计电荷状态使其不可恢复（后续正常帧仍能正确积分）。
- **理由**：坏数据敏感性是库仑计数固有风险（失效安全 3、R1/R2）；与 ADR-SOC-C03 纯函数边界一致，便于注入测试。
- **涉及模块/通道**：soc 模块（纯函数 + 私有状态）。
- **关联需求**：⚠️ REQ-SOC-C06、C03、C07。

---

## 2. zbus 通道与数据结构变更

### 2.1 通道（`channels.c` / `channels.h`）
- **无任何变更**。订阅 `chan_cell_meas`、发布 `chan_soc` 全部复用现有定义（ADR-SOC-C01）。`ZBUS_OBSERVERS_EMPTY` + 模块侧 `ZBUS_CHAN_ADD_OBS` 的解耦范式保持。

### 2.2 共享类型 `types.h`
- **无任何变更**（ADR-SOC-C02）。
  - 输入 `struct bms_cell_meas`：字段已满足（`timestamp_ms`、`pack_current_ma`、`cell_mv[]`）。
  - 输出 `struct bms_soc`：字段已满足（`timestamp_ms`、`soc_permille`、`soh_permille`）。`soh_permille` 维持 1000‰（非目标）。
- **兼容影响**：因不改 `types.h`，对 afe/protection/balancing/comm 的 ABI 与编译**零影响**。

### 2.3 模块私有新增（不在 `types.h`）
- 新增**库仑积分状态结构体**（暂名 `bms_soc_coulomb_state`，最终名由 ③ 定）：含累计电荷（`int64`）、上一帧 `timestamp_ms`、初始化标志、当前 SOC。声明于 `soc.h`、实例化于 `soc.c`。
- 新增/调整**纯函数原型**于 `soc.h`：库仑积分步进函数（接收状态指针 + 测量 + 输出）；`bms_soc_estimate` 语义收敛为初值映射（ADR-SOC-C04）。
- 精确字段、签名、初始化时机属于 ③ `bms-designer` 范围，本文件不下沉。

### 2.4 配置 `Kconfig`
- 新增 `CONFIG_BMS_SOC_PACK_CAPACITY_MAH`（必需）及可选 `CONFIG_BMS_SOC_INIT_FROM_VOLTAGE`、`CONFIG_BMS_SOC_GAP_FACTOR_N`（ADR-SOC-C07）；复用 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS` 作 Δt 回退基准。

---

## 3. 线程模型

| 线程 | 归属模块 | 优先级（数值） | 周期/触发 | 与安全线程的相对关系 |
|---|---|---|---|---|
| `bms_prot_tid` | protection（**安全**） | **4（最高）** | 事件驱动（订阅 chan_cell_meas） | 基准——最高优先级 |
| `bms_afe_tid` | afe | 6 | 周期 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS`=100ms | 低于 protection |
| **`bms_soc_tid`** | **soc（本特性）** | **7（不变）** | **事件驱动**：每收到一帧 `chan_cell_meas` 触发一次积分+发布 | **数值 7 > 4，优先级低于 protection ✓** |
| `bms_bal_tid` | balancing | 7 | 事件驱动 | 低于 protection |
| `bms_comm_tid` | comm | 8 | 周期 200ms | 低于 protection |

**结论（线程优先级）**：
- SOC 线程优先级**维持 7 不变**，满足 REQ-SOC-C09「soc 数值 > protection(4)」红线，**无需调整**。
- SOC 不新增线程，复用既有 `bms_soc_tid`，单一工作线程承载积分与发布。
- 触发模型为**事件驱动**（随 AFE 发帧节拍，等效约 100ms/帧），Δt 由帧间 `timestamp_ms` 差自适应，而非定时器轮询（ADR-SOC-C05）。
- I/O 路径有限超时 `K_MSEC(50)`，发布超时丢弃不重试（ADR-SOC-C09），保证不因背压抢占/拖延 protection。

---

## 4. 失效安全影响分析

| 失效安全红线 | 本特性符合性 | 落点（ADR/机制） |
|---|---|---|
| 默认接触器 OPEN，仅 NORMAL 才 CLOSED | **不触碰**——SOC 不参与接触器决策 | ADR-SOC-C10：唯一输出 `chan_soc`，不发保护语义 |
| 安全线程优先级更高 | **满足**——soc(7) > protection(4) | ADR-SOC-C08：维持 prio 7 |
| 不拖累安全链 | **满足**——读写有限超时、发布超时即丢弃 | ADR-SOC-C09：`K_MSEC(50)`，不重试不死锁 |
| 不污染安全链 | **满足**——SOC 输出不被 protection 当判据 | ADR-SOC-C10：protection 对 chan_soc 订阅仅预留 |

**新增风险与缓解（架构层）**：
- **坏数据传导**：异常帧若进积分会令 SOC 跳变误导上层（balancing/comm）。缓解：ADR-SOC-C11 纯函数内检测+降级，单帧 |ΔSOC| 夹紧到设计上限，状态可恢复（REQ-SOC-C06）。SOC 不进保护链，故即便 SOC 错也**不影响接触器安全态**——失效安全隔离成立。
- **定点溢出**：长时运行累计溢出可致 SOC 错乱。缓解：ADR-SOC-C06 强制 `int64` 承载 + ③ 设计溢出边界证明（REQ-SOC-C10）。
- **背压死锁**：SOC 写 `chan_soc` 阻塞拖累系统。缓解：ADR-SOC-C09 有限超时 + 超时丢弃（REQ-SOC-C09）。
- **范式回归风险**：纯函数签名变更牵动既有单测与 `tests/bms/soc/CMakeLists.txt` 源复用（R5）。缓解：ADR-SOC-C03 保持「纯逻辑/线程分离」范式，签名变更在 ④/⑤ 阶段同步——属下游执行项，架构层已标注。

**边界声明（评审留痕）**：SOC 为**非安全关键**信息流；其漂移/异常**绝不**经由任何路径进入过充/过放/过流/过温或接触器开合判定。该边界由 ADR-SOC-C10 固化，protection 模块独立性不受本特性影响。

---

## 5. 需求 → 架构决策 追溯（回填迭代计划用）

| 需求 ID | 架构决策（ADR） |
|---|---|
| REQ-SOC-C01 | ADR-SOC-C01、C03、C06 |
| REQ-SOC-C02 | ADR-SOC-C05、C07 |
| REQ-SOC-C03 | ADR-SOC-C04、C11 |
| REQ-SOC-C04 | ADR-SOC-C04、C07 |
| REQ-SOC-C05 | ADR-SOC-C01、C09 |
| ⚠️ REQ-SOC-C06 | ADR-SOC-C05、C11 |
| REQ-SOC-C07 | ADR-SOC-C03、C11 |
| ⚠️ REQ-SOC-C08 | ADR-SOC-C01、C10 |
| ⚠️ REQ-SOC-C09 | ADR-SOC-C08、C09 |
| REQ-SOC-C10 | ADR-SOC-C06 |
| REQ-SOC-C11 | ADR-SOC-C03、C06 |
| REQ-SOC-C12 | ADR-SOC-C07 |

> 准出自检：每条需求均映射至少一项 ADR；ADR 均记录决策+理由+涉及模块/通道+关联需求；明确「纯逻辑可单测」拆分（ADR-SOC-C03）；通道/`types.h` 变更结论为零新增（复用优先）；线程优先级安全相对关系明确（soc 7 > protection 4）；失效安全影响已分析。已回填 `00-iteration-plan.md` 第 3 节「架构决策」列。

_状态：DONE（② 架构已产出，已回填迭代计划追溯链「架构决策」列）_
