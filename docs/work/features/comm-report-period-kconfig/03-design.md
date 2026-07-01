<!--
  详细设计：comm 模块 CAN 上报周期 Kconfig 可配。
  套用模板：docs/templates/design-spec-template.md。
  阶段：敏捷-V 左腿第③层 —— 详细设计（bms-designer）。
  输入：02-architecture.md（ADR-COMM-01~08）、01-requirements.md（REQ-COMM-001~007）。
  本文把架构 ADR 细化到可直接编码：函数签名/契约/错误码、状态机、Kconfig/dts、纯逻辑单测目标。
  交付物语言：中文。
-->
# comm 模块 CAN 上报周期 详细设计

| 字段 | 值 |
|---|---|
| 设计 ID | DES-COMM-001 … DES-COMM-006（见第 0 节索引） |
| 版本 | 0.1（草稿） |
| 满足需求 | REQ-COMM-001, REQ-COMM-002, REQ-COMM-003, REQ-COMM-004, REQ-COMM-005, REQ-COMM-006, REQ-COMM-007 |
| 输入架构 | [`02-architecture.md`](02-architecture.md)（ADR-COMM-01~08） |
| 状态 | 草稿 |

---

## 0. 设计项索引（DES-COMM-NNN）

| 设计 ID | 标题 | 落点 | 关联 ADR | 满足需求 |
|---|---|---|---|---|
| **DES-COMM-001** | `CONFIG_BMS_COMM_REPORT_PERIOD_MS` 增加 `range`，固定 `P_min=10`、`P_max=60000` | `app/Kconfig` | ADR-COMM-01/02 | REQ-COMM-001/002/003/004 |
| **DES-COMM-002** | 周期合法化纯函数 `bms_comm_clamp_period_ms(req, lo, hi)` 签名与契约 | `app/include/bms/comm.h`、`app/src/bms/comm/comm_period.c` | ADR-COMM-03/04 | REQ-COMM-004/005/007 |
| **DES-COMM-003** | 生效周期取值常量 `BMS_COMM_PERIOD_MIN_MS`/`MAX_MS` 与 `comm_effective_period_ms()` 内部封装 | `app/src/bms/comm/comm.c`（或 `comm_period.c`） | ADR-COMM-03/04 | REQ-COMM-004/005 |
| **DES-COMM-004** | `comm_thread` 睡眠改为取合法化返回值（删除直接取宏） | `app/src/bms/comm/comm.c` | ADR-COMM-04/06 | REQ-COMM-005/006 |
| **DES-COMM-005** | `bms_comm_init()` 启动日志打印合法化后生效周期 | `app/src/bms/comm/comm.c` | ADR-COMM-05 | REQ-COMM-007 |
| **DES-COMM-006** | 新增单测套件 `tests/bms/comm/`，只链纯函数源 | `tests/bms/comm/*` | ADR-COMM-08 | REQ-COMM-001/002/004/005/007 |

> 边界值收敛（履行架构移交项）：本设计将架构留空的 `P_min`/`P_max` 收敛为 **`P_min = 10`、`P_max = 60000`**（ms）。理由见第 4 节决策表 D1。满足架构约束 `P_min > 0`、`P_min ≤ 200 ≤ P_max`、`P_max ≥ 200`，且与 `default 200` 不冲突。

---

## 1. 概述与范围

- **解决什么**：把 comm 模块对外 CAN 上报周期做成「编译期 Kconfig 可配 + 运行期防御性钳制」，默认 200ms；保证任意配置/输入下 comm 线程实际睡眠周期恒 > 0，不退化为忙等抢占调度。
- **边界**：仅 comm 模块内部改动 —— 给既有 Kconfig 项加 `range`、新增一个周期合法化纯函数、线程睡眠改用其返回值、启动日志反映生效值、补一套纯函数单测。
- **不做**（与需求非目标一致）：不改 zbus 通道 / `types.h`（ADR-COMM-07）；不抬升线程优先级、不引入无限阻塞（ADR-COMM-06）；不引入运行期动态可调周期；不实现真实 CAN 收发（维持桩）；不改 comm 日志「变化才 INF」策略。

---

## 2. 接口

### 2.1 zbus 通道
- **无变更**（ADR-COMM-07）。comm 继续仅 `zbus_chan_read` 现有 `chan_cell_meas` / `chan_soc` / `chan_prot_state`，不新增、不修改、不发布。三处读取维持有限超时 `K_MSEC(50)`。

### 2.2 公共 API（新增，声明于 `app/include/bms/comm.h`）

#### DES-COMM-002　`bms_comm_clamp_period_ms` —— 周期合法化纯函数

```c
/**
 * @brief 纯函数：把请求的上报周期(ms)钳制到合法闭区间 [lo, hi]（供线程与单测复用）。
 *
 * 无副作用、不依赖全局状态/硬件/Kconfig 宏；边界以入参注入，便于 host 单测
 * （对齐 bms_afe_validate(m, limits) 的「边界入参注入」范式）。
 *
 * 钳制规则（确定且唯一）：
 *   requested <  lo  -> 返回 lo   （含 requested <= 0 的全部情形，因 lo > 0）
 *   requested >  hi  -> 返回 hi
 *   否则             -> 返回 requested 原值
 *
 * @param requested 请求周期，单位 ms（允许任意 int32_t，含 0/负数/越界）
 * @param lo        合法下界，单位 ms；调用方须保证 lo > 0 且 lo <= hi
 * @param hi        合法上界，单位 ms
 * @return 钳制后的生效周期，单位 ms。
 *
 * 不变式（对任意 int32_t requested，在前置条件成立时）：
 *   lo <= 返回值 <= hi   且   返回值 >= lo > 0   （即返回值恒 > 0）
 *
 * 前置条件：0 < lo <= hi（由调用方/编译期 range 共同保证；见 D2「前置条件防御」）。
 * 后置条件：返回值满足上述不变式；不修改任何外部状态。
 * 错误处理：纯函数无指针参数，不返回错误码；非法前置（lo<=0 或 lo>hi）属调用方契约违反，
 *           其行为见 D2（设计选择 clamp 自身对 lo/hi 不做纠正，依赖编译期 range 与单一调用点保证）。
 */
int32_t bms_comm_clamp_period_ms(int32_t requested, int32_t lo, int32_t hi);
```

- **签名取舍**：返回 `int32_t`（非 `int 错误码 + out 指针`）—— 因本函数为「取值映射」而非「带失败语义的求值」，无指针入参故无 `-EINVAL` 路径，返回值即结果，最利于 ztest 直接断言（`zassert_equal(bms_comm_clamp_period_ms(0, 10, 60000), 10, …)`）。这是与 `bms_protection_evaluate`（有指针、需 `-EINVAL`）的合理差异，仍遵循「纯逻辑/线程分离」范式。
- **`bms_comm_init` 契约不变**：仍 `int bms_comm_init(void)`，返回 0 成功 / 负 errno。

### 2.3 模块内部 API（不导出，`static`，定义于 `comm.c`）

#### DES-COMM-003　`comm_effective_period_ms` —— 生效周期求值（Kconfig → 纯函数的薄包装）

```c
/* 生效上报周期 = 对编译期配置值施加运行期防御性钳制后的确定值。
 * 把 CONFIG_* 注入点收敛到唯一一处，线程与 init 共用，避免散落的 k_msleep(CONFIG_*)。 */
static inline int32_t comm_effective_period_ms(void)
{
	return bms_comm_clamp_period_ms((int32_t)CONFIG_BMS_COMM_REPORT_PERIOD_MS,
					BMS_COMM_PERIOD_MIN_MS, BMS_COMM_PERIOD_MAX_MS);
}
```

- **`BMS_COMM_PERIOD_MIN_MS` / `BMS_COMM_PERIOD_MAX_MS`**：定义为 `comm.c` 内 `#define` 常量，**数值必须与 Kconfig `range` 端点一致**（`10` / `60000`），是「编译期 range + 运行期 clamp 双保险」中运行期那一侧的边界源。设计要求：二者与 Kconfig `range` 同步修改（见 D3 风险）。
- **错误码**：无（`static inline`，纯取值）。

### 2.4 Kconfig（DES-COMM-001）

对既有 `config BMS_COMM_REPORT_PERIOD_MS` **新增 `range 10 60000`**；`depends on BMS_COMM`、`default 200` 保持不变。可直接抄入 `app/Kconfig`（替换现有 4 行块）：

```kconfig
config BMS_COMM_REPORT_PERIOD_MS
	int "CAN 上报周期 (ms)"
	depends on BMS_COMM
	range 10 60000
	default 200
	help
	  comm 模块对外 CAN 上报的固定周期（毫秒）。
	  下界 10ms 防止过密上报使最低优先级 comm 线程忙等、间接拖累
	  protection/afe 等安全相关线程（失效安全，REQ-COMM-005）；
	  上界 60000ms(60s) 为工程合理上限。默认 200ms 为现状基线。
	  运行期另有防御性钳制（bms_comm_clamp_period_ms）作第二道防线。
```

- **越界编译期行为**：`CONFIG_BMS_COMM_REPORT_PERIOD_MS` 被赋 `< 10` 或 `> 60000` 时，Kconfig `range` 在配置求值期即拒绝/钳制（编译期第一道防线，REQ-COMM-004）。
- **`depends on BMS_COMM`**：`CONFIG_BMS_COMM=n` 时该项不可见（REQ-COMM-003）。

### 2.5 设备树 / overlay
- **无变更**。本特性不涉及 GPIO/CAN/ADC 节点（仿真阶段 CAN 为日志桩，真实 CAN overlay 属另一特性）。无 `DEVICE_DT_GET` 新增。

---

## 3. 设计与数据流

### 3.1 源文件拆分（DES-COMM-002 落点裁定）

履行架构「`comm.c` 或拆出 `comm_period.c` 由 ③ 裁定」的移交：**裁定拆出独立纯函数源 `app/src/bms/comm/comm_period.c`**，仅含 `bms_comm_clamp_period_ms`。

| 文件 | 内容 | 是否进单测链接 |
|---|---|---|
| `app/src/bms/comm/comm_period.c`（**新增**） | `bms_comm_clamp_period_ms` 纯函数实现 | **是**（单测只链此源） |
| `app/src/bms/comm/comm.c`（改） | 线程、桩 TX、`comm_effective_period_ms`、`bms_comm_init`、`#define MIN/MAX` | 否（含线程/桩，不进单测） |
| `app/include/bms/comm.h`（改） | 新增 `bms_comm_clamp_period_ms` 原型 | —（头，单测 include） |

- **理由**：对齐 `tests/bms/afe` 只链 `afe_sim.c + afe_validate.c`（纯函数核心）、不链线程的范式（ADR-COMM-08）。把纯函数独立成源，使 `tests/bms/comm/CMakeLists.txt` 能只链 `comm_period.c`，避免把 `K_THREAD_DEFINE`/桩 TX/`LOG_MODULE_REGISTER` 拖进 host 单测。
- **构建影响**：应用构建侧需把 `comm_period.c` 加入 comm 模块的 `CMakeLists.txt`（与 `comm.c` 同级编译）。具体 build 接线属 ④ coder，本设计标注该新增源须纳入编译。

### 3.2 线程模型（维持现状，仅改睡眠取值来源）

| 线程 | 优先级 | 周期/触发 | 本特性改动 |
|---|---|---|---|
| `bms_comm_tid` | **8（最低，不变）** | 周期 = `comm_effective_period_ms()` 返回值 | 仅把 `k_msleep(CONFIG_BMS_COMM_REPORT_PERIOD_MS)` 改为 `k_msleep(comm_effective_period_ms())` |

- comm 不新增线程，复用既有单一周期工作线程（ADR-COMM-06）。
- 优先级常量 `COMM_THREAD_PRIO = 8` **不动**；三处 `zbus_chan_read(…, K_MSEC(50))` **不动**，无 `K_FOREVER`（REQ-COMM-006）。
- **优化（可选，推荐）**：线程进入 `while(1)` 前一次性求值生效周期存入局部 `const int32_t period_ms = comm_effective_period_ms();`，循环内 `k_msleep(period_ms)`。因 `CONFIG_*` 编译期固定、clamp 确定，生效周期不随运行变化，无需每圈重算。该优化不改变语义与不变式。

### 3.3 钳制状态机 / 取值映射（DES-COMM-002 核心逻辑）

本特性无时序状态机；钳制为**无状态确定映射**，以「输入区间 → 输出」表表达（`lo=10, hi=60000` 代入）：

| 输入 `requested` 区间 | 返回值 | 触发的需求验收点 |
|---|---|---|
| `requested ≤ 0`（含 0、负数） | `lo`（=10） | REQ-COMM-005：≤0 钳到 P_min 且 > 0 |
| `0 < requested < lo`（如 1..9） | `lo`（=10） | REQ-COMM-005：低于下界钳到 P_min |
| `lo ≤ requested ≤ hi`（如 200） | `requested` 原值 | REQ-COMM-001/002：合法值原样生效 |
| `requested > hi`（> 60000） | `hi`（=60000） | REQ-COMM-004：越上界钳到 P_max |

- **不变式（机器可验证，单测断言）**：`∀ requested ∈ int32_t, lo ≤ ret ≤ hi ∧ ret ≥ lo > 0`。
- **确定性**：同一 `(requested, lo, hi)` 必得同一 `ret`，无随机/无时间依赖（REQ-COMM-007 可观测前提）。
- **参考实现骨架**（供 ④ coder，非完整实现）：

```c
int32_t bms_comm_clamp_period_ms(int32_t requested, int32_t lo, int32_t hi)
{
	if (requested < lo) {
		return lo;   /* 覆盖 <=0 与 0<req<lo 两类，因 lo>0 故结果恒>0 */
	}
	if (requested > hi) {
		return hi;
	}
	return requested;
}
```

### 3.4 启动可观测性（DES-COMM-005）

`bms_comm_init()` 的 `LOG_INF` 改为打印**合法化后**生效周期（而非原始宏），使「配了却被钳制」不再静默（REQ-COMM-007）：

```c
int bms_comm_init(void)
{
	int32_t period = comm_effective_period_ms();

	LOG_INF("Comm init: CAN report stub, period=%d ms (configured=%d, no CAN HW in sim)",
		period, CONFIG_BMS_COMM_REPORT_PERIOD_MS);
	return 0;
}
```

- 同时打印生效值与原始配置值，便于现场对比发现「被钳制」偏差（满足 REQ-COMM-007「生效周期可被观测」且暴露偏差）。

---

## 4. 设计决策与权衡

| 决策 | 备选 | 选择理由 |
|---|---|---|
| **D1** `P_min=10`、`P_max=60000` | P_min=1 / P_min=50；P_max=1000 / 10000 | `10ms`：满足 `>0` 红线，留足忙等防护余量（1ms 在 QEMU 快进下仍偏密、抗误配余量小）；又不高到压住合理高频上报。`60000ms(60s)`：覆盖「低速诊断上报」工程上限，远大于默认 200。满足约束 `10 ≤ 200 ≤ 60000`。 |
| **D2** clamp 不自纠 `lo/hi` 前置 | 函数内 `if (lo>hi) swap` / 返回错误码 | 保持纯函数最简、单一职责；`lo/hi` 由编译期 `range` + 单一调用点（`comm_effective_period_ms`）注入常量保证，不存在运行期可变非法前置；自纠会增复杂度且掩盖契约违反。 |
| **D3** MIN/MAX 用 `#define` 镜像 Kconfig `range` | 直接在 clamp 内硬编码；或从 `CONFIG_*` 推导 | 边界以入参注入纯函数（对齐 afe `limits` 范式），使单测可独立传边界；`#define` 集中于 `comm.c` 一处、注释要求与 Kconfig `range` 同步，避免散落魔数。 |
| **D4** 拆 `comm_period.c` | 纯函数留在 `comm.c` | 单测只链纯函数源、不拖入线程/桩（afe 范式）；隔离 `LOG_MODULE_REGISTER`/`K_THREAD_DEFINE`，host 单测干净。 |
| **D5** 返回 `int32_t` 非 `int+out 指针` | `int bms_comm_clamp_period_ms(int32_t, int32_t, int32_t, int32_t*)` | 无失败语义、无指针入参，返回值即结果最简、最利断言；与「带 `-EINVAL` 的求值类纯函数」语义不同。 |
| **D6** 生效周期循环外求值一次 | 每圈 `k_msleep(comm_effective_period_ms())` | 编译期固定 + 确定映射 → 值不变，循环外算一次更省；语义/不变式不变。 |

---

## 5. 失效处理 / Fail-safe

| 场景 | 处理 | 落点 |
|---|---|---|
| 配置 `< P_min`（含 0/负、1..9） | 编译期：`range` 拒绝；运行期：clamp → `P_min=10`（恒 > 0），comm 线程睡眠 ≥ 10ms，不忙等 | DES-COMM-001 + 002 双保险（REQ-COMM-005） |
| 配置 `> P_max` | 编译期：`range` 拒绝；运行期：clamp → `P_max=60000` | DES-COMM-001 + 002（REQ-COMM-004） |
| 宏被绕过 / 未来动态来源传入越界值 | 运行期 clamp 仍兜底，不变式保证 `> 0` | DES-COMM-002/003（REQ-COMM-005） |
| 越界被静默钳制误导调参 | 启动日志同时打印生效值与配置值，偏差可见 | DES-COMM-005（REQ-COMM-007） |
| zbus 读取超时/失败 | 维持现状：跳过本帧上报、继续循环，不阻塞（有限超时 `K_MSEC(50)`） | 不改（REQ-COMM-006） |
| 线程优先级被误抬升 | 设计明令 `COMM_THREAD_PRIO=8` 不动；评审 + 检视留痕 | 不改（REQ-COMM-006） |

- **安全默认态**：comm 非安全关键、不参与接触器/保护决策；本特性不触碰接触器默认 OPEN 语义。失效安全核心是「睡眠周期恒 > 0」，由 `lo=10>0` + 不变式 `ret ≥ lo` 写死保证。

---

## 6. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| `#define MIN/MAX` 与 Kconfig `range` 端点漂移（改一处忘改另一处） | 编译期与运行期边界不一致，钳制行为偏离预期 | 注释强约束二者同步（D3）；可加 `BUILD_ASSERT` 或在单测中以同样常量断言端点（DES-COMM-006 `test_period_range_bounds`） |
| comm 首次引入纯函数 + 新单测套件，CMakeLists 源复用接线出错 | 单测编译失败 / 误链线程 | 严格照搬 `tests/bms/afe/CMakeLists.txt` 只链纯函数源范式（DES-COMM-006）；④/⑤ 同步 |
| 改动顺手动了优先级/超时 | 破坏安全线程优先级序 | D2/线程模型明令不动；REQ-COMM-006 检视/分析留痕 |
| `range` 下界选 10 仍偏密（QEMU 时钟快进刷屏） | 仿真日志噪声 | comm 日志「变化才 INF」策略未改，逐帧 meas 走 DBG；下界仅防忙等，非控制日志量 |

---

## 7. 验证要点（纯逻辑单测目标 + 检视）

### 7.1 纯逻辑函数清单（单测目标，host 可测）

| 纯函数 | 源 | 可测性 | 对应用例 |
|---|---|---|---|
| `bms_comm_clamp_period_ms(req, lo, hi)` | `comm_period.c` | 无副作用、无全局/硬件依赖、边界入参注入 → host 直接调用断言 | 见 7.2 全部用例 |

### 7.2 单测套件 `tests/bms/comm/`（DES-COMM-006，套件名 `bms_comm`）

新增 `tests/bms/comm/`：`testcase.yaml`（`platform_allow: mps2/an386, native_sim`；tags `bms`、`comm`）、`CMakeLists.txt`（只链 `${APP_DIR}/src/bms/comm/comm_period.c` + `app/include`，**不链** `comm.c`）、`prj.conf`（`CONFIG_ZTEST=y`）、`src/main.c`。用例顶部 `/* Verifies REQ-COMM-NNN */` 回链。建议测试侧本地常量 `TEST_P_MIN=10`、`TEST_P_MAX=60000`（镜像 Kconfig range，供断言）。

| 用例（`ZTEST(bms_comm, …)`） | 验证内容 | 回链需求 |
|---|---|---|
| `test_default_period_is_200` | `clamp(200, 10, 60000) == 200`（默认 200 落区间内原样生效） | REQ-COMM-001/002 |
| `test_period_range_bounds` | `clamp(10,…)==10`、`clamp(60000,…)==60000`（端点合法、原样返回）；并断言 `TEST_P_MIN>0`、`TEST_P_MAX>=200`、`TEST_P_MIN<=200<=TEST_P_MAX` | REQ-COMM-004 |
| `test_clamp_below_lower_bound` | `clamp(1)==10`、`clamp(9)==10`（`0<x<P_min` → `P_min`）；结果确定唯一 | REQ-COMM-005/007 |
| `test_clamp_zero_or_negative` | `clamp(0)==10`、`clamp(-1)==10`、`clamp(-1000)==10`（≤0 → `P_min` 且 > 0） | REQ-COMM-005 |
| `test_clamp_above_upper_bound`（补强，推荐） | `clamp(60001)==60000`、`clamp(INT32_MAX)==60000` | REQ-COMM-004/005 |
| `test_clamp_invariant_holds`（补强，推荐） | 对一组覆盖样本（`INT32_MIN,-1,0,9,10,200,60000,60001,INT32_MAX`）断言 `10 ≤ ret ≤ 60000 ∧ ret>0` | REQ-COMM-005（不变式） |

### 7.3 检视 / 分析（非测试方法，替代验证）

| 需求 | 检视点 |
|---|---|
| REQ-COMM-003 | 检视 `app/Kconfig`：`config BMS_COMM_REPORT_PERIOD_MS` 含 `depends on BMS_COMM`；`CONFIG_BMS_COMM=n` 时不可见 |
| REQ-COMM-004（编译期侧） | 检视 `app/Kconfig` 含 `range 10 60000`、`default 200`，`10>0`、`10≤200≤60000` |
| REQ-COMM-006 | 检视改动前后 `comm.c`：`COMM_THREAD_PRIO` 仍为 8（最低）；三处 `zbus_chan_read` 仍 `K_MSEC(50)`，无 `K_FOREVER` |
| REQ-COMM-007（日志侧） | 检视 `bms_comm_init` 启动日志打印生效周期（= clamp 结果），与配置值并列 |

---

_状态：DONE（③ 设计已产出，已收敛 P_min=10/P_max=60000，已回填 traceability.md 设计列）_
