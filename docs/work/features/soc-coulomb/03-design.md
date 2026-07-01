# 详细设计：SOC 库仑计数估算

> 特性 slug：`soc-coulomb`
> 阶段：敏捷-V 左腿第③层 —— 详细设计（`bms-designer`）
> 输入：`02-architecture.md`、`01-requirements.md`、`00-iteration-plan.md`；`app/src/bms/soc/soc.c`、`app/include/bms/soc.h`、`app/include/bms/types.h`、`app/Kconfig`、`tests/bms/soc/src/main.c`（既有单测复用范式）
> 交付物语言：中文。
> 本文件给出**可直接编码**的签名/契约/数据结构/Kconfig/伪代码，**不写完整实现**（实现交 ④ `bms-coder`）。
> 范式遵循：纯逻辑与线程分离（对齐 `bms_protection_evaluate`）；输出先初始化为安全态；纯函数不依赖全局状态或硬件，可 host 单测。

---

## 0. 设计总览

| 维度 | 设计结论 |
|---|---|
| 跨帧状态 | 新增模块私有结构体 `struct bms_soc_coulomb_state`，声明于 `soc.h`，实例化（`static`）于 `soc.c`（ADR-SOC-C02） |
| 核心纯函数 | `bms_soc_coulomb_step(state, meas, &out)` —— 有状态库仑积分步进，返回 int 错误码（ADR-SOC-C03） |
| 初值映射 | `bms_soc_estimate(meas, &out)` 保留，语义收敛为「电压线性映射初值来源」，映射端点逐位不变（ADR-SOC-C04）；既有单测全部仍绿 |
| 电荷承载 | `int64_t`，量纲 **mA·ms**（ADR-SOC-C06） |
| Δt 来源 | 帧间 `timestamp_ms` 差；异常回退 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS`（ADR-SOC-C05） |
| 容量配置 | 新增 `CONFIG_BMS_SOC_PACK_CAPACITY_MAH`（必需）+ 两个可选项（ADR-SOC-C07） |
| 线程/通道 | 复用 `bms_soc_tid`(prio 7)、订阅 `chan_cell_meas`、发布 `chan_soc`，零新增（ADR-SOC-C01/C08/C09） |

**关键编码约束（来自单测复用扫描）**：`tests/bms/soc/CMakeLists.txt` 直接把 `soc.c` 编入 host 测试，而 `tests/bms/soc/prj.conf` **不提供** `CONFIG_BMS_SOC_*`。因此所有新增 `CONFIG_BMS_SOC_*` 在 `soc.c`（或 `soc.h`）中**必须带 `#ifndef … #define` 回退默认值**，与 `types.h` 对 `CONFIG_BMS_CELL_COUNT` 的处理一致，否则纯函数在测试构建下无法编译/取值。详见 §5。

---

## 1. 数据结构设计

### 1.1 跨帧积分状态结构体（声明于 `app/include/bms/soc.h`）

```c
/**
 * @brief SOC 库仑积分跨帧状态（模块私有；不进 types.h，非 zbus 载荷）。
 *
 * 不变式（invariant，任意时刻成立）：
 *  - initialized==false 时，acc_charge_ma_ms 与 last_ts_ms 内容无意义（未使用）。
 *  - initialized==true  时，soc_permille ∈ [0,1000]，last_ts_ms 为最近一次有效更新所用帧的时间戳。
 *  - acc_charge_ma_ms 是「相对初值起点」的累计净转移电荷（充电为正），量纲 mA·ms。
 */
struct bms_soc_coulomb_state {
    int64_t  acc_charge_ma_ms;  /**< 累计净转移电荷，量纲 mA·ms，充电为正。int64 防溢出。 */
    uint32_t last_ts_ms;        /**< 上一帧已积分的 timestamp_ms（k_uptime, ms）。 */
    uint16_t soc_permille;      /**< 当前 SOC 估值 ‰，恒夹紧于 [0,1000]。 */
    bool     initialized;       /**< 是否已完成上电一次性初始化（电压映射）。 */
};
```

> 字段顺序：8 字节对齐量在前，避免填充浪费。`soc.c` 中以 `static struct bms_soc_coulomb_state soc_state;`（BSS 零初始化 → `initialized=false`，天然首帧安全态）实例化。

设计取舍：`acc_charge_ma_ms` 不直接存「SOC 浮点/定点比例」而存「原始累计电荷」，是为了把唯一的有损换算（mA·ms→‰）集中在一处、便于精度证明（§4.3），并使 `soc_permille` 始终可由 `初值对应电荷 + acc_charge_ma_ms` 重算——异常帧只要不破坏 `acc_charge_ma_ms` 即可恢复（REQ-SOC-C06 验收 3）。

> 实现说明（供 ④）：为支持「初值 + 累计电荷重算」，初始化时把电压映射初值换算成等效起始电荷写入 `acc_charge_ma_ms`，后续 ‰ 完全由 `acc_charge_ma_ms` 推出（§4.4 方案 A）。或保留初值 ‰ 单独存储、`acc_charge_ma_ms` 仅存增量（方案 B）。本设计采用**方案 A**（单一真值源，避免双状态漂移），伪代码以方案 A 给出。

### 1.2 输出载荷 `struct bms_soc`（`types.h`，不变）

复用现有字段：`timestamp_ms`、`soc_permille`(uint16)、`soh_permille`(uint16，恒 1000)。**不修改 `types.h`**（ADR-SOC-C02）。

---

## 2. 纯逻辑函数签名与契约（可直接编码）

> 命名对齐 `bms_<module>_<verb>`；返回 `int` 错误码（0 成功，负 errno）；输出先置安全态。

### 2.1 `bms_soc_estimate` —— 电压映射初值器（保留，语义收敛）

```c
int bms_soc_estimate(const struct bms_cell_meas *meas, struct bms_soc *out);
```

| 项 | 契约 |
|---|---|
| 职责 | 由一帧测量的平均电压线性映射出 ‰ 初值（3000mV→0‰，4200mV→1000‰，线性，端点外夹紧）。**行为与现实现逐位一致**（REQ-SOC-C04 验收 1：误差 0‰）。 |
| 入参 `meas` | 非空；读取 `cell_mv[0..BMS_CELL_COUNT-1]`、`timestamp_ms`。 |
| 出参 `out` | 非空；写 `timestamp_ms=meas->timestamp_ms`、`soc_permille=映射夹紧值`、`soh_permille=1000`。 |
| 返回 | `0` 成功；`-EINVAL`（meas/out 任一为 NULL）。 |
| 前置 | 无跨帧依赖（无状态）。 |
| 后置 | `0 ≤ out->soc_permille ≤ 1000`。失败时（NULL）不写 out。 |
| 纯度 | 无副作用、不依赖全局/硬件。**单测目标 T-EST**。 |

> 实现保持现 `soc.c:27-53` 逻辑不动（含 `SOC_EMPTY_MV/SOC_FULL_MV`、整型映射、夹紧）。既有 5 条单测（test_full_charge / test_empty / test_mid_in_range / test_clamp_over_full / test_null_args）**必须继续通过**，不得行为漂移（R5）。

### 2.2 `bms_soc_coulomb_step` —— 库仑积分步进核心（新增）

```c
int bms_soc_coulomb_step(struct bms_soc_coulomb_state *state,
                         const struct bms_cell_meas   *meas,
                         struct bms_soc               *out);
```

| 项 | 契约 |
|---|---|
| 职责 | 有状态库仑积分一步：取 Δt、对 `pack_current_ma` 积分、更新 `state` 与 `out->soc_permille`（夹紧）；首帧执行电压映射初始化；异常输入安全降级。 |
| 入参 `state` | 非空，跨帧积分状态（调用方持有，单测可栈上构造）。本函数**唯一**读写它的入口。 |
| 入参 `meas` | 非空，本帧测量。读取 `pack_current_ma`、`timestamp_ms`、`cell_mv[]`（仅初始化时）。 |
| 出参 `out` | 非空，发布载荷。**进入即先置安全态**（见后置/伪代码）。 |
| 返回 | `0` 已产生有效输出（含初始化帧、含降级后仍更新的帧）；`-EINVAL`（任一指针 NULL）；`-EAGAIN`（坏数据被跳过、本帧不应发布，见 §3 分支 D/E 的「跳过」语义）。 |
| 前置 | `state` 由 `bms_soc_coulomb_state_reset()` 或零初始化过（`initialized` 字段有定义值）。 |
| 后置（返回 0） | `0 ≤ out->soc_permille ≤ 1000`；`out->timestamp_ms == meas->timestamp_ms`；`out->soh_permille == 1000`；`state->initialized == true`；`state->soc_permille == out->soc_permille`；`state->last_ts_ms == meas->timestamp_ms`（除时间戳非单调分支按 §3-C 处理）。 |
| 后置（返回 < 0） | `state` **不被破坏**到不可恢复（`acc_charge_ma_ms` 不被异常值污染）；调用方不应发布 `out`（REQ-SOC-C06 验收 1/3）。 |
| 纯度 | 不依赖全局/硬件；容量等配置通过编译期宏读取（§5 回退默认保证 host 可测）。**单测目标 T-STEP（主），覆盖 C01/C02/C03/C04/C06/C07/C10/C11**。 |

> 取舍：是否「跳过帧发布」由返回码区分（`-EAGAIN`=跳过不发布；`0`=发布）。这把 REQ-SOC-C05 验收 2「被跳过帧不发布」的择一决策**定为「不发布」**（线程据返回码决定 pub，见 §6），语义确定、可测。空指针返回 `-EINVAL`（REQ-SOC-C06 验收 1）。

### 2.3 `bms_soc_coulomb_state_reset` —— 状态复位（新增，便于单测/上电）

```c
void bms_soc_coulomb_state_reset(struct bms_soc_coulomb_state *state);
```

| 项 | 契约 |
|---|---|
| 职责 | 把 `state` 置为「未初始化安全态」：`acc_charge_ma_ms=0`、`last_ts_ms=0`、`soc_permille=0`、`initialized=false`。 |
| 入参 | `state` 为 NULL 时安全返回（无操作）。 |
| 返回 | `void`。 |
| 纯度 | 无副作用（仅写入参）。**单测目标 T-RESET**（也作为其它用例的 setup）。 |

### 2.4 内部辅助（建议 `static`，纯函数，可选单独单测）

> 以下为 `soc.c` 内部 `static` 辅助，签名供实现参考；若 ④ 内联进 `bms_soc_coulomb_step` 亦可，但**建议拆出以便单测确定性注入**（C02/C11）。若拆出，需在 `soc.h` 暴露或用测试专用编译单元复用（参照 CMakeLists 复用 `soc.c` 的方式，函数可设为非 static 并加 `soc.h` 原型）。

```c
/** 由帧间时间戳算出本帧有效 Δt（ms），并标注异常类别。
 *  返回有效 Δt(>0)；通过 *reason 输出分支（正常/回退/夹紧/跳过）。 */
static uint32_t soc_resolve_dt_ms(const struct bms_soc_coulomb_state *state,
                                  uint32_t now_ts_ms, enum soc_dt_reason *reason);

/** 量程合理性检查：|pack_current_ma| 是否在 [0, CONFIG_BMS_SOC_MAX_CURRENT_MA]。 */
static bool soc_current_in_range(int32_t pack_current_ma);

/** 由累计电荷 acc(mA·ms) 换算并夹紧为 ‰（含定点舍入，§4.4）。 */
static uint16_t soc_charge_to_permille(int64_t acc_charge_ma_ms);
```

`enum soc_dt_reason { SOC_DT_NORMAL, SOC_DT_FALLBACK_FIRST, SOC_DT_FALLBACK_NONMONO, SOC_DT_CLAMP_GAP };`

---

## 3. 分支处理设计（首帧/丢帧/时间戳非单调/超量程）

> 全部在 `bms_soc_coulomb_step` 纯函数内闭环（ADR-SOC-C05/C11），可被测试确定性注入。下表「动作」即伪代码分支。

| 分支 | 触发条件 | 动作 | 返回 | 关联需求 |
|---|---|---|---|---|
| A 空指针 | `state==NULL ‖ meas==NULL ‖ out==NULL` | 不触 state、不写 out | `-EINVAL` | C06-1 |
| B 首帧（初始化） | `state->initialized==false` | 用 `bms_soc_estimate` 取电压映射初值 → 换算等效起始电荷写入 `acc_charge_ma_ms`；`soc_permille=初值`；`last_ts_ms=meas->ts`；`initialized=true`；**首帧不积分**（Δt 未知） | `0`（发布初始化帧，C05 含初始化帧） | C04-1/2, C02-1 |
| C 时间戳非单调/回绕 | `initialized && meas->ts ≤ state->last_ts_ms` | Δt 回退 `CONFIG_BMS_AFE_SAMPLE_PERIOD_MS`；**仍按回退 Δt 正常积分**（不产生负 Δt、不反向跳变）；更新 `last_ts_ms = meas->ts`（采纳新基准，防卡死） | `0` | C02-2, C06-2 |
| D 正常 | `initialized && 0 < (ts-last) ≤ N×period` | `dt = ts - last`；正常积分 | `0` | C01, C07 |
| E 丢帧（大间隔） | `initialized && (ts-last) > N×period` | Δt **夹紧到上限** `N×period`（不取真实大差值），按夹紧 Δt 积分；保证单帧 \|ΔSOC\| ≤ 设计上限（§3.1）；`last_ts_ms=meas->ts` | `0` | C02-3, C06-2 |
| F 电流超量程 | `\|pack_current_ma\| > CONFIG_BMS_SOC_MAX_CURRENT_MA` | **跳过本帧积分**（不改 `acc_charge_ma_ms`）；`out` 取上一稳定 `state->soc_permille`；`last_ts_ms` 仍推进到 `meas->ts`（避免下帧把跳过期算成丢帧） | `-EAGAIN`（不发布，C05-2 定为不发布） | C06-2/3 |

> 说明：
> - 分支 C 选择「回退 Δt 后**仍积分**」而非「跳过」，与 REQ-SOC-C02-2 一致（用缺省周期当 Δt，且 SOC 不反向跳变）；电流符号决定方向，回退 Δt 为正，故不会反向。
> - 分支 E/F 的 `last_ts_ms` 都推进到本帧 ts，确保「异常帧不污染后续判定」（C06-3）：下一正常帧 Δt 重新以本帧为基准、回到分支 D。
> - 分支 F 跳过积分是「绝不让超量程电流放大单帧跳变」的最强保证；分支 E 用夹紧而非跳过，是因为丢帧期间电荷确实转移，夹紧到上限兼顾不失真与不爆冲。

### 3.1 单帧 ΔSOC 上限（设计给定，量化）

单帧最大允许积分时间 `dt_max = N × period`（N=`CONFIG_BMS_SOC_GAP_FACTOR_N` 默认 10，period 默认 100ms → 1000ms）。
单帧最大电荷 = `MAX_CURRENT_MA × dt_max`。以默认 `MAX_CURRENT_MA=200000`(±200A)、`dt_max=1000ms`、`C=容量 mAh`：

```
|ΔSOC|_max(‰) = MAX_CURRENT_MA × dt_max / (3600 × 1000 × C) × 1000   (ms→h: /3600000)
             = 200000 × 1000 / (3600000 × C) × 1000
             = 55556 / C   (‰)
```

例 C=100000mAh：\|ΔSOC\|_max ≈ 0.56‰/帧；C=10000mAh：≈ 5.6‰/帧。即「设计给定上限」= 由 `MAX_CURRENT_MA × N × period` 推出的解析上界，测试据此断言（REQ-SOC-C06-2、C02-3）。

---

## 4. int64 防溢出与量纲推导（可编码、可证明）

### 4.1 量纲链

```
电荷增量 dQ [mA·ms] = pack_current_ma [mA] × dt [ms]
累计电荷 acc        [mA·ms] = Σ dQ
SOC 比例 [‰] = acc [mA·ms] / 容量[mA·h] / (3600×1000 [ms/h]) × 1000[‰]
            = acc / (C_mAh × 3600 × 1000) × 1000
            = acc / (C_mAh × 3600)             [‰]   ← 1000 与分母 1000 约去
```

即换算系数：**`permille_delta = acc_charge_ma_ms / (C_mAh × 3600)`**（理想实数；定点实现见 §4.4）。

### 4.2 单步 dQ 溢出边界（int32 不够、int64 充裕）

- 单步 `dQ = pack_current_ma × dt`：最坏 `200000 mA × 1000 ms = 2.0×10^8`。`int32` 上限 ≈ 2.147×10^9，单步**不溢**；但累加必溢（见 4.3）。乘法**必须先提升到 int64** 再乘：`(int64_t)pack_current_ma * (int64_t)dt`，避免 `int32×int32` 中间溢出（虽本例不溢，仍按规范强制提升，防未来量程放大）。

### 4.3 累计 acc 溢出边界（24h 连续，REQ-SOC-C10 验收 1）

- 最坏恒定 ±200A 连续 24h：
  `acc_max = 200000 mA × (24×3600×1000) ms = 200000 × 8.64×10^7 = 1.728×10^13 mA·ms`。
- `int64` 上限 ≈ 9.22×10^18。**余量 ≈ 5.3×10^5 倍**。即便 ±200A 连续运行约 **1.5 万年** 才接近 int64 上限。→ `int64_t` 承载 acc **绝对充裕**，REQ-SOC-C10 满足，无需周期性归一。
- 注：实际 `acc` 会随充放电正负抵消，且换算后被夹紧；但即便单调累加（最坏假设）也不溢。

### 4.4 定点换算与舍入（方案 A：acc 为单一真值源）

`soc_permille = clamp( round( acc_charge_ma_ms / (C_mAh × 3600) ), 0, 1000 )`

- 分母 `DEN = (int64_t)C_mAh × 3600`（C_mAh≤10^7 → DEN≤3.6×10^10，int64 安全）。
- **四舍五入**（避免小增量被整数除截断、累积单调偏移，REQ-SOC-C11-2）：
  `permille_raw = (acc + (acc>=0 ? DEN/2 : -DEN/2)) / DEN;`（对称舍入，正负一致）。
- 夹紧：`<0→0`，`>1000→1000`，转 `uint16_t`。
- **初始化等效起始电荷**（分支 B）：由初值 ‰ 反算 `acc0 = (int64_t)init_permille × DEN`，写入 `acc_charge_ma_ms`。此后 ‰ 恒由 acc 推出 → 单一真值源，无双状态漂移。
  - 自洽校验：`soc_charge_to_permille(init_permille × DEN) == init_permille`（舍入项 DEN/2 < DEN，整除回原值）→ 初始化帧 ‰ 与电压映射逐位一致（REQ-SOC-C04-1 误差 0‰）。

### 4.5 精度边界（REQ-SOC-C11）

- 量化步进 1‰ = `DEN` 个 mA·ms 的电荷。恒流 I、时长 T：理论 `ΔSOC=I×T/DEN`，实现用四舍五入，单帧误差 ≤ 0.5‰，多帧因对称舍入不单调累积 → 稳态误差 ≤ ±1‰（满足 C11-1/2、C01-2）。

---

## 5. Kconfig 变更草案（可直接抄进 `app/Kconfig`）

> 位置：`menu "BMS application"` 内、`config BMS_SOC` 节之后；新增项 `depends on BMS_SOC`，风格对齐既有 `CONFIG_BMS_*`。

```kconfig
config BMS_SOC_PACK_CAPACITY_MAH
	int "电池组额定容量 (mAh)"
	depends on BMS_SOC
	range 1 10000000
	default 100000
	help
	  库仑积分换算基准（额定容量，单位 mAh）。换算公式
	  ΔSOC(‰)=acc(mA·ms)/(容量×3600)。多板型适配请覆写，
	  禁止在算法中硬编码（REQ-SOC-C12）。

config BMS_SOC_INIT_FROM_VOLTAGE
	bool "上电用电压线性映射初始化 SOC 初值"
	depends on BMS_SOC
	default y
	help
	  y：首帧用 bms_soc_estimate 的电压映射(3000mV→0‰,4200mV→1000‰)
	  作为库仑积分起点（REQ-SOC-C04）。n：从 0‰ 起算（不推荐）。

config BMS_SOC_GAP_FACTOR_N
	int "丢帧判定因子 N（Δt 上限 = N × AFE 采样周期）"
	depends on BMS_SOC
	range 2 1000
	default 10
	help
	  帧间差 > N×CONFIG_BMS_AFE_SAMPLE_PERIOD_MS 判为丢帧，Δt 夹紧到
	  该上限，避免单帧 SOC 大幅跳变（REQ-SOC-C02-3、C06）。

config BMS_SOC_MAX_CURRENT_MA
	int "合理电流量程上限绝对值 (mA)"
	depends on BMS_SOC
	range 1 2000000
	default 200000
	help
	  |pack_current_ma| 超过此值判为坏数据，跳过本帧积分（REQ-SOC-C06）。
	  默认 ±200A，覆盖 C10 建议的 ±100A 测试量程并留裕量。
```

### 5.1 单测构建回退默认（**编码必做**，否则 host 单测不可编译）

`tests/bms/soc/prj.conf` 不提供上述符号、且 `soc.c` 被直接编入测试。须在 `soc.c` 顶部（include 之后）按 `types.h` 范式补回退默认：

```c
#ifndef CONFIG_BMS_SOC_PACK_CAPACITY_MAH
#define CONFIG_BMS_SOC_PACK_CAPACITY_MAH 100000
#endif
#ifndef CONFIG_BMS_SOC_GAP_FACTOR_N
#define CONFIG_BMS_SOC_GAP_FACTOR_N 10
#endif
#ifndef CONFIG_BMS_SOC_MAX_CURRENT_MA
#define CONFIG_BMS_SOC_MAX_CURRENT_MA 200000
#endif
#ifndef CONFIG_BMS_AFE_SAMPLE_PERIOD_MS   /* 测试 prj.conf 也未开 AFE */
#define CONFIG_BMS_AFE_SAMPLE_PERIOD_MS 100
#endif
/* CONFIG_BMS_SOC_INIT_FROM_VOLTAGE 为 bool，未定义即视为关闭；
   测试若需验证 C04 初始化，应在 prj.conf 显式 CONFIG_BMS_SOC_INIT_FROM_VOLTAGE=y，
   或将其默认行为设计为「宏未定义时也按电压映射初始化」——本设计取后者：
   首帧初始化恒走电压映射（与 C04 默认 y 一致），该 Kconfig 仅作显式关闭开关。 */
```

> 测试若要验证「改容量→ΔSOC 按 1/容量 变化」（REQ-SOC-C12-2），建议把容量作为 `bms_soc_coulomb_step` 可注入参数考虑——但本设计为对齐既有「编译期常量」范式，容量取宏值；C12-2 的测试可通过在 `tests/bms/soc/prj.conf` 设不同 `CONFIG_BMS_SOC_PACK_CAPACITY_MAH` 跑参数化用例，或新增一个测试编译单元。④/⑤ 协调，本设计标注此点（见 §8 风险）。

---

## 6. 线程集成（复用，最小改动）

线程 `soc_thread` 仅替换「调用纯函数 + 发布」一段，其余（订阅、读、超时）不变（ADR-SOC-C01/C08/C09）。

```
soc_thread():
  state 复位（bms_soc_coulomb_state_reset(&soc_state)）  // 或依赖 BSS 零初始化
  while zbus_sub_wait(soc_sub, &chan, K_FOREVER)==0:
      if chan != &chan_cell_meas: continue
      if zbus_chan_read(chan_cell_meas, &meas, K_MSEC(50)) != 0: continue
      rc = bms_soc_coulomb_step(&soc_state, &meas, &soc)
      if rc == 0:
          zbus_chan_pub(chan_soc, &soc, K_MSEC(50))   // 发布超时即丢弃，不重试
      // rc == -EAGAIN（跳过帧）/ -EINVAL：不发布，继续下一帧
```

- 优先级保持 `SOC_THREAD_PRIO 7`（> protection 4，合规 REQ-SOC-C09-2）。
- 读/写均 `K_MSEC(50)` 有限超时；发布失败丢弃（REQ-SOC-C09-1/3）。`zbus_sub_wait` 的 `K_FOREVER` 为空闲等待，非数据读写背压路径，沿用（ADR-SOC-C09）。
- SOC 仍仅发 `chan_soc`，不触保护语义（REQ-SOC-C08）。

---

## 7. 伪代码（核心纯函数，方案 A）

```c
int bms_soc_coulomb_step(struct bms_soc_coulomb_state *state,
                         const struct bms_cell_meas *meas,
                         struct bms_soc *out)
{
    /* 分支 A：空指针 —— 不触 state、不写 out */
    if (state == NULL || meas == NULL || out == NULL)
        return -EINVAL;

    /* 输出先置安全态：用上一稳定 SOC（未初始化则 0），ts/soh 先填 */
    out->timestamp_ms = meas->timestamp_ms;
    out->soh_permille = 1000;
    out->soc_permille = state->initialized ? state->soc_permille : 0;

    const int64_t DEN = (int64_t)CONFIG_BMS_SOC_PACK_CAPACITY_MAH * 3600; /* mA·ms / ‰ */

    /* 分支 B：首帧初始化（电压映射 → 等效起始电荷），不积分 */
    if (!state->initialized) {
        struct bms_soc init;
        (void)bms_soc_estimate(meas, &init);          /* 取夹紧后初值 ‰ */
        state->acc_charge_ma_ms = (int64_t)init.soc_permille * DEN;
        state->soc_permille     = init.soc_permille;
        state->last_ts_ms       = meas->timestamp_ms;
        state->initialized      = true;
        out->soc_permille       = init.soc_permille;
        return 0;                                       /* 发布初始化帧 */
    }

    /* 分支 F：电流超量程 —— 跳过积分，状态电荷不污染，ts 推进，不发布 */
    if (!soc_current_in_range(meas->pack_current_ma)) {
        state->last_ts_ms = meas->timestamp_ms;         /* 防下帧误判丢帧 */
        return -EAGAIN;                                 /* out 保持上一稳定值 */
    }

    /* Δt 解析：分支 C(非单调回退) / D(正常) / E(丢帧夹紧) */
    uint32_t dt_ms;
    const uint32_t period = CONFIG_BMS_AFE_SAMPLE_PERIOD_MS;
    const uint32_t dt_cap = (uint32_t)CONFIG_BMS_SOC_GAP_FACTOR_N * period;

    if (meas->timestamp_ms <= state->last_ts_ms) {      /* C：非单调/回绕 */
        dt_ms = period;                                 /* 回退缺省周期 */
    } else {
        uint32_t diff = meas->timestamp_ms - state->last_ts_ms;
        dt_ms = (diff > dt_cap) ? dt_cap : diff;        /* E：丢帧夹紧 / D：正常 */
    }

    /* 积分（先提升 int64，防中间溢出）；方向随电流符号（C07） */
    int64_t dQ = (int64_t)meas->pack_current_ma * (int64_t)dt_ms; /* mA·ms */
    state->acc_charge_ma_ms += dQ;

    /* 换算 + 对称舍入 + 夹紧（§4.4） */
    int64_t acc = state->acc_charge_ma_ms;
    int64_t pm  = (acc + (acc >= 0 ? DEN/2 : -DEN/2)) / DEN;
    if (pm < 0)    pm = 0;
    if (pm > 1000) pm = 1000;

    state->soc_permille = (uint16_t)pm;
    state->last_ts_ms   = meas->timestamp_ms;
    out->soc_permille   = (uint16_t)pm;
    return 0;
}
```

> 注：方案 A 下 `acc_charge_ma_ms` 在长期单调充/放且已夹紧后仍会继续累加（acc 可超出 [0,DEN×1000]），但 §4.3 证明 int64 不溢，且换算后恒夹紧，发布值始终合法（REQ-SOC-C03）。若 ④ 担心 acc 远离物理区间，可在夹紧饱和时把 acc 回拉至边界（`acc = clamp_permille × DEN`）——**可选优化**，不影响正确性，本设计不强制。

---

## 8. 纯逻辑函数清单（单测目标）

| 标签 | 函数 | 单测重点 | 覆盖需求 |
|---|---|---|---|
| **T-EST** | `bms_soc_estimate` | 端点 0‰/1000‰、区间内、过量夹紧、NULL（**既有 5 用例保留**，行为 0‰ 漂移） | C04-1, C03 |
| **T-RESET** | `bms_soc_coulomb_state_reset` | 复位后 `initialized==false`、字段归零；NULL 安全 | C04-2 |
| **T-STEP** | `bms_soc_coulomb_step` | 恒流充/放电 T 后 ΔSOC 精度≤±1‰；方向（正/负/零电流）；夹紧 0/1000‰；首帧初始化值；非单调回退；丢帧夹紧；超量程跳过；空指针 -EINVAL；异常帧后正常帧可恢复；24h 等效序列不溢出 | C01,C02,C03,C04,C06,C07,C10,C11 |
| （辅助）| `soc_resolve_dt_ms`/`soc_current_in_range`/`soc_charge_to_permille` | 若拆出为非 static，可单独参数化测 Δt 分支与换算舍入 | C02,C06,C11 |

> 全部为纯函数、无全局/硬件依赖，可在 host（native_sim / mps2-an386）ztest 直接构造 `state`/`meas` 注入断言（对齐 `bms_protection_evaluate` 范式）。`bms_soc_init`/`soc_thread` 含 zbus/线程副作用，**非纯函数、不在单测目标内**。

---

## 9. 设计 → 需求 / 架构 追溯（回填迭代计划「设计项」列）

| 需求 ID | 架构决策 | 设计项（本文件） |
|---|---|---|
| REQ-SOC-C01 | ADR-SOC-C01,C03,C06 | §2.2 `bms_soc_coulomb_step`；§4.1 量纲；§7 积分伪代码 |
| REQ-SOC-C02 | ADR-SOC-C05,C07 | §3 分支 B/C/E；§7 Δt 解析；§5 `GAP_FACTOR_N` |
| REQ-SOC-C03 | ADR-SOC-C04,C11 | §4.4 夹紧；§7 clamp 0/1000 |
| REQ-SOC-C04 | ADR-SOC-C04,C07 | §2.1 `bms_soc_estimate`（语义收敛）；§3-B 首帧；§4.4 自洽校验；§5 `INIT_FROM_VOLTAGE` |
| REQ-SOC-C05 | ADR-SOC-C01,C09 | §2.2 返回码语义（0 发布/-EAGAIN 不发布）；§6 线程发布逻辑 |
| ⚠️ REQ-SOC-C06 | ADR-SOC-C05,C11 | §3 分支 A/C/E/F；§3.1 单帧 ΔSOC 上限；§7 降级闭环 |
| REQ-SOC-C07 | ADR-SOC-C03,C11 | §4.1 方向随符号；§7 `dQ=current×dt`；T-STEP 方向用例 |
| ⚠️ REQ-SOC-C08 | ADR-SOC-C01,C10 | §6 仅发 `chan_soc`；§0 通道零新增 |
| ⚠️ REQ-SOC-C09 | ADR-SOC-C08,C09 | §6 prio 7、K_MSEC(50)、发布超时丢弃 |
| REQ-SOC-C10 | ADR-SOC-C06 | §1.1 `int64_t acc`；§4.2/4.3 溢出边界证明 |
| REQ-SOC-C11 | ADR-SOC-C03,C06 | §4.4 对称舍入；§4.5 精度边界 |
| REQ-SOC-C12 | ADR-SOC-C07 | §5 `CONFIG_BMS_SOC_PACK_CAPACITY_MAH`；§5.1 容量参数化测试说明 |

**风险/留痕（交 ④/⑤）**：
- R5（签名连锁）：新增 `bms_soc_coulomb_step`/状态结构需在 `soc.h` 加原型；`tests/bms/soc/CMakeLists.txt` 已复用 `soc.c`，无需改源列表，但 `prj.conf` 须按 §5.1 加 `CONFIG_BMS_SOC_*=…`（或依赖 `soc.c` 内回退默认）。
- C12-2 参数化：容量取编译期宏，验证「改容量→ΔSOC 按 1/C 变化」需多 prj.conf 或独立编译单元，④/⑤ 协调。
- 既有 5 条 `bms_soc_estimate` 用例必须保持全绿（行为零漂移）。

_状态：DONE（③ 详细设计已产出，已回填迭代计划「设计项」列）_
```
