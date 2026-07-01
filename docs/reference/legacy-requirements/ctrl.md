# REQ-CTRL：系统状态机 / MOS 控制 / 启动自检 / 主循环编排 需求规格

> 覆盖源文件：`BMS_Control.c`、`BMS_Control.h`、`BMS_Info.c`、`BMS_Info.h`、
> `common.c`、`common.h`、`Core/Src/main.c`、`WarningTask.c`（`IsBmsWarning()`、`needsleep()`）、
> `UpperComTask.h`（`UpperCMD_TypeDef`、控制枚举）、`AppPower.c`

---

## 需求列表

---

### REQ-CTRL-001  上电立即关断所有 MOS

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `main.c:main()` 第 193 行 `BmsControl_MosAllOffNow()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统完成外设初始化（GPIO、AFE 重初始化、参数加载）之后、进入主循环之前，系统应立即将充电 MOS、放电 MOS、预充 MOS 全部关断，以确保起始为安全态。

**理由 / 代码依据**
> `main()` 在完成 `AppAfe_Reinit()`、`initParameter()` 等初始化后，显式调用 `BmsControl_MosAllOffNow()`（直接调用 AFE 驱动层 `AppAfe_SetChgMos(0)` / `AppAfe_SetDsgMos(0)` / `AppAfe_SetPreDsgMos(0)`），绕过请求队列，直接写硬件，保证初始安全态。

**验收准则（可度量）**
- Given 系统刚完成硬件初始化，When 代码执行到 `BmsControl_MosAllOffNow()`，Then 充电 MOS = OFF、放电 MOS = OFF、预充 MOS = OFF（可通过 AFE 寄存器读回或电流为零确认）。

---

### REQ-CTRL-002  系统工作状态枚举

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.h:BMS_WORK_STA_S` |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应始终维护一个工作状态变量 `Bms.sta`，其合法值为以下 7 个枚举：`BMS_STA_DISCONNET`（上电断开）、`BMS_STA_PRECHG`（预充）、`BMS_STA_STANDBY`（待机）、`BMS_STA_POWEROFF`（关机）、`BMS_STA_CHARGING`（充电）、`BMS_STA_DISCHARGING`（放电）、`BMS_STA_PROTECT`（保护）。

**理由 / 代码依据**
> `BMS_Info.h` 定义 `BMS_WORK_STA_S`，`BmsControlTask()` 以 `Bms.sta` 为 switch 分支键驱动全部状态转移逻辑。

**验收准则（可度量）**
- Given 任意时刻，When 读取 `Bms.sta`，Then 值落在上述 7 个枚举之内，不出现非法数值。

---

### REQ-CTRL-003  上电默认进入 DISCONNET 状态

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Info.c:BMS_PARA_S Bms` 零初始化；`BmsControlTask()` `BMS_STA_DISCONNET` 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统上电后 `Bms` 结构体零初始化时，系统应以 `BMS_STA_DISCONNET`（值 = 0）作为初始工作状态，并在该状态下保持所有 MOS 断开。

**理由 / 代码依据**
> `Bms` 为全局变量（静态零初始化），`BMS_STA_DISCONNET = 0`。`BmsControlTask()` 中 `BMS_STA_DISCONNET` 分支首先调用 `BmsMos_AllOff()`。

**验收准则（可度量）**
- Given 系统刚上电（未执行任何主循环迭代），When 读取 `Bms.sta`，Then 值 = 0（`BMS_STA_DISCONNET`）。
- Given 系统处于 `BMS_STA_DISCONNET` 状态，When `BmsControlTask()` 执行到该分支，Then `BmsMos_AllOff()` 被调用，充电/放电/预充 MOS 请求均为 0。

---

### REQ-CTRL-004  上电 5 s 稳定等待

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Control.c:BmsControlTask()` `BMS_STA_DISCONNET` 分支，`u32SysTime <= 5000` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统处于 `BMS_STA_DISCONNET` 状态时，系统应等待系统运行时间超过 5 000 ms 后，才开始评估告警状态并进行状态转移；在此等待窗口内，系统应保持所有 MOS 断开。

**理由 / 代码依据**
> `if(u32SysTime <= 5000) { break; }` 确保 AFE 采集数据、告警评估在 5 s 内稳定后再决策，防止上电瞬间误保护或误开 MOS。

**验收准则（可度量）**
- Given 系统处于 `BMS_STA_DISCONNET` 且 `u32SysTime ≤ 5 000 ms`，When `BmsControlTask()` 执行，Then 不进行状态转移，所有 MOS 保持断开。
- Given `u32SysTime > 5 000 ms`，When `BmsControlTask()` 执行 `BMS_STA_DISCONNET` 分支，Then 开始执行保护判断和状态转移逻辑。

---

### REQ-CTRL-005  DISCONNET 状态下的保护优先转移

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:BmsControlTask()` `BMS_STA_DISCONNET` 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统处于 `BMS_STA_DISCONNET` 且运行时间已超过 5 000 ms 时，如果 `IsBmsWarning()` 返回非零（存在任意故障），系统应立即转移到 `BMS_STA_PROTECT` 状态，不开启任何 MOS。

**理由 / 代码依据**
> `if(ErrorSta != 0) { Bms.sta = BMS_STA_PROTECT; break; }` 在 DISCONNET 分支优先处理故障，避免带故障开启回路。

**验收准则（可度量）**
- Given 系统处于 `BMS_STA_DISCONNET`，`u32SysTime > 5 000 ms`，且 `ErrorSta ≠ 0`，When `BmsControlTask()` 执行，Then `Bms.sta` 转为 `BMS_STA_PROTECT`，不触发 MOS 开启。

---

### REQ-CTRL-006  DISCONNET 状态向 STANDBY / PRECHG 的正常转移

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Control.c:BmsControlTask()` `BMS_STA_DISCONNET` 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统处于 `BMS_STA_DISCONNET`、运行时间超过 5 000 ms、且无故障时，如果检测到负载（`AppAfe_HasLoad() == 1`），系统应进入预充状态 `BMS_STA_PRECHG`；否则应进入 `BMS_STA_STANDBY`。

**理由 / 代码依据**
> `BmsControl_RequestChgByParam()` 先尝试开充电 MOS；随后判断 `AppAfe_HasLoad()`，有负载则调用 `BmsControl_EnterPreDsg()` 切换到 PRECHG，无负载则直接切 STANDBY。

**验收准则（可度量）**
- Given 无故障、`u32SysTime > 5 000 ms`、`AppAfe_HasLoad() == 1`，When `BmsControlTask()` 执行 DISCONNET 分支，Then `Bms.sta` = `BMS_STA_PRECHG`，预充状态机复位启动。
- Given 无故障、`u32SysTime > 5 000 ms`、`AppAfe_HasLoad() == 0`，Then `Bms.sta` = `BMS_STA_STANDBY`。

---

### REQ-CTRL-007  预充序列：先预充 MOS 后等待 Vmos 收敛

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:PreDischg()`，`PRE_DSG_WAIT_VMOS` 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统开始预充序列时，系统应先置预充 MOS = ON、放电 MOS = OFF，等待 100 ms 后，当 MOS 两端电压 `Bms.Vmos_mv` 非零且绝对值 < 10 000 mV 时，才将放电 MOS 置 ON，随后再保持 100 ms 延时，完成预充。

**理由 / 代码依据**
> 宏 `PRE_DSG_STEP_WAIT_MS = 100u`，`PRE_DSG_VMOS_CLOSE_MV = 10000`。状态机步骤：`PRE_DSG_PD_ON` → `PRE_DSG_WAIT_VMOS`（等 Vmos < 10 000 mV）→ `PRE_DSG_DSG_ON_DELAY`（等 100 ms）→ Done。

**验收准则（可度量）**
- Given 预充开始，When `u32SysTime` 经过 100 ms 且 `|Bms.Vmos_mv| < 10 000 mV`，Then 放电 MOS 置 ON。
- Given `|Bms.Vmos_mv| ≥ 10 000 mV` 且超时未收敛，Then 预充在总超时（`Bms.Para.PreDiscTime`）后强制完成或失败。

---

### REQ-CTRL-008  预充过电流保护

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:PreDischg_CheckFault()`，`PRE_DSG_CURRENT_LIMIT_MA = -3000`，`PRE_DSG_CURRENT_TIMEOUT_MS = 1000u` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 在预充过程中，如果放电电流 `Bms.current_mA < -3 000 mA` 的状态持续超过 1 000 ms，系统应置 `warn2_level.PreDiscF = WARNING_LEVLE_3` 并终止预充（放电 MOS 和预充 MOS 均置 OFF），转为预充失败结果。

**理由 / 代码依据**
> `PreDischg_CheckFault()` 检查 `Bms.current_mA < PRE_DSG_CURRENT_LIMIT_MA`（-3 000 mA），若连续超过 `PRE_DSG_CURRENT_TIMEOUT_MS`（1 000 ms），调用 `PreDischg_Fail()`。

**验收准则（可度量）**
- Given 预充进行中，When 电流 < -3 000 mA 持续 ≥ 1 000 ms，Then 预充 MOS = OFF，放电 MOS = OFF，`warn2_level.PreDiscF` = 3，函数返回 `PRE_DSG_RESULT_FAIL`。

---

### REQ-CTRL-009  预充期间故障即失败

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:PreDischg_CheckFault()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 在预充过程中，如果 `IsBmsWarning()` 包含 `DISC_FAULT` 或 `SYS_FAULT` 位，系统应立即中断预充，关断放电 MOS 和预充 MOS，返回预充失败。

**理由 / 代码依据**
> `if(IsBmsWarning() & (BIT(DISC_FAULT) | BIT(SYS_FAULT))) { return PreDischg_Fail(); }`。

**验收准则（可度量）**
- Given 预充进行中，When `IsBmsWarning()` 中 `DISC_FAULT` 或 `SYS_FAULT` 任一置位，Then 立即调用 `PreDischg_Fail()`，放电 MOS = OFF，预充 MOS = OFF。

---

### REQ-CTRL-010  预充超时强制完成

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Control.c:PreDischg()`，`(system_run_time_100ms - s_pre_start_time) >= timeout` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当预充已运行时间（以 100 ms 计）达到参数 `Bms.Para.PreDiscTime`（单位：100 ms 计数）时，无论 Vmos 是否收敛，系统应强制置预充完成（放电 MOS = ON，预充 MOS = OFF）。

**理由 / 代码依据**
> `if((system_run_time_100ms - s_pre_start_time) >= timeout) { return PreDischg_Done(); }` 先于状态机分支判断，保证最终超时都能完成。`timeout` 由调用方传入 `Bms.Para.PreDiscTime`。

**验收准则（可度量）**
- Given 预充开始，When 经过 `Bms.Para.PreDiscTime × 100 ms`（具体值由参数决定），Then `PreDischg_Done()` 被调用，放电 MOS = ON，预充 MOS = OFF，`PreDone = 1`。

---

### REQ-CTRL-011  如放电 MOS 已开则跳过预充

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Control.c:PreDischg()`，`PRE_DSG_IDLE` 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当预充状态机处于初始状态（`PRE_DSG_IDLE`）且放电 MOS 或预充 MOS 已经导通时，系统应直接跳过预充序列，立即返回预充完成结果。

**理由 / 代码依据**
> `if((AppAfe_IsDsgMosOn() != 0) || (AppAfe_IsPreDsgMosOn() != 0)) { return PreDischg_Done(); }` 防止重复预充。

**验收准则（可度量）**
- Given `s_pre_dsg_step == PRE_DSG_IDLE` 且 `AppAfe_IsDsgMosOn() || AppAfe_IsPreDsgMosOn()`，When `PreDischg()` 调用，Then 直接返回 `PRE_DSG_RESULT_DONE`，不启动预充时序。

---

### REQ-CTRL-012  预充成功后转 STANDBY，失败后转 PROTECT

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:BmsControlTask()` `BMS_STA_PRECHG` 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统处于 `BMS_STA_PRECHG` 状态时，如果 `PreDischg()` 返回成功，系统应复位预充状态机并转移到 `BMS_STA_STANDBY`；如果返回失败，系统应复位预充状态机并转移到 `BMS_STA_PROTECT`。

**理由 / 代码依据**
> PRECHG 分支：`if(pre_result == PRE_DSG_RESULT_DONE) { Bms.sta = BMS_STA_STANDBY; } else if(pre_result == PRE_DSG_RESULT_FAIL) { Bms.sta = BMS_STA_PROTECT; }`。

**验收准则（可度量）**
- Given `Bms.sta == BMS_STA_PRECHG`，When `PreDischg()` 返回 `PRE_DSG_RESULT_DONE`，Then `Bms.sta` = `BMS_STA_STANDBY`。
- Given `Bms.sta == BMS_STA_PRECHG`，When `PreDischg()` 返回 `PRE_DSG_RESULT_FAIL`，Then `Bms.sta` = `BMS_STA_PROTECT`。

---

### REQ-CTRL-013  故障立即触发保护状态

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:BmsControlTask()` STANDBY/CHARGING/DISCHARGING 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统处于 `BMS_STA_STANDBY`、`BMS_STA_CHARGING` 或 `BMS_STA_DISCHARGING` 状态时，如果 `IsBmsWarning()` 返回非零，系统应立即转移到 `BMS_STA_PROTECT` 状态。

**理由 / 代码依据**
> 三个状态共用分支：`if(ErrorSta != 0) { Bms.sta = BMS_STA_PROTECT; break; }`。

**验收准则（可度量）**
- Given 系统处于运行态（STANDBY/CHARGING/DISCHARGING），When `ErrorSta ≠ 0`，Then `Bms.sta` 在本次 `BmsControlTask()` 迭代内设为 `BMS_STA_PROTECT`。

---

### REQ-CTRL-014  正常运行态下的充放电状态更新

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Control.c:BmsControl_UpdateRunState()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 在 STANDBY/CHARGING/DISCHARGING 状态且无故障时，系统应根据 `Bms.CurrentState` 更新工作状态：`CURRENT_DISC` → `BMS_STA_DISCHARGING`；`CURRENT_CHG` → `BMS_STA_CHARGING`（并同时置放电 MOS ON）；其余 → `BMS_STA_STANDBY`。

**理由 / 代码依据**
> `BmsControl_UpdateRunState()` 中三路判断，充电态下额外调用 `BmsMos_SetDsg(1)`（充电时同步开放电 MOS，允许双向流通）。

**验收准则（可度量）**
- Given `Bms.CurrentState == CURRENT_DISC`，When `BmsControl_UpdateRunState()`，Then `Bms.sta = BMS_STA_DISCHARGING`。
- Given `Bms.CurrentState == CURRENT_CHG`，When `BmsControl_UpdateRunState()`，Then `Bms.sta = BMS_STA_CHARGING` 且放电 MOS 请求 = 1。
- Given `Bms.CurrentState` 为其他值，Then `Bms.sta = BMS_STA_STANDBY`。

---

### REQ-CTRL-015  电流状态判定阈值

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.c:BmsinfoStatistics()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应在每次 `BmsinfoStatistics()` 执行时，根据 `Bms.current_mA` 与参数阈值更新 `Bms.CurrentState`：当 `current_mA ≥ Bms.Para.chgthdmA` 时为 `CURRENT_CHG`；当 `current_mA ≤ -Bms.Para.dscthdmA` 时为 `CURRENT_DISC`；其余为 `CURRENT_NONE`。

**理由 / 代码依据**
> `BMS_Info.c` 第 63–74 行，阈值 `chgthdmA`（mA）与 `dscthdmA`（mA）均为可配置参数（`USER_PARA_S`），由参数加载决定默认值（具体值需查 `ParaSet.c`，此处标为存疑）。

**验收准则（可度量）**
- Given `Bms.current_mA ≥ Bms.Para.chgthdmA`，Then `Bms.CurrentState = CURRENT_CHG`。
- Given `Bms.current_mA ≤ -(Bms.Para.dscthdmA)`，Then `Bms.CurrentState = CURRENT_DISC`。
- Given 两条件均不满足，Then `Bms.CurrentState = CURRENT_NONE`。

---

### REQ-CTRL-016  SYS_FAULT 触发条件（全体 MOS 断开）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:IsBmsWarning()` 第 1993–2014 行 |
| 验证方法 | 分析 + 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当以下任一三级（Level-3）保护位置位时，`IsBmsWarning()` 应设置 `SYS_FAULT` 位，导致 `DealFault()` 关断全部 MOS：单体压差高、单体温差高、MOS 温度高、环境温度高、环境温度低、单体温度高、预充故障（`PreDiscF`）、短路故障（`DiscCurShort`）、二级过流（`SecondOverCur`）、进水检测（`WaterCheck`）。

**理由 / 代码依据**
> `IsBmsWarning()` 中 `SYS_FAULT` 判断使用 `WARN3CODE0_BIT` / `WARN3CODE1_BIT` 宏逐位检查，任一为真则 `fault_type |= BIT(SYS_FAULT)`。

**验收准则（可度量）**
- Given 上述任一三级保护位置位，When `IsBmsWarning()` 返回，Then 返回值 `& BIT(SYS_FAULT)` 非零。
- Given `SYS_FAULT` 置位，When `DealFault()` 执行，Then 充电 MOS = OFF，放电 MOS = OFF，预充 MOS = OFF。

---

### REQ-CTRL-017  SYS_FAULT 时断开所有 MOS

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:DealFault()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `DealFault()` 检测到 `ErrorSta` 包含 `SYS_FAULT` 位时，系统应立即将充电 MOS、放电 MOS、预充 MOS 请求全部置 0（通过 `BmsMos_AllOff()`）。

**理由 / 代码依据**
> `DealFault()` 第 481–483 行：`if(ErrorSta & BIT(SYS_FAULT)) { BmsMos_AllOff(); }`。`BmsMos_AllOff()` 内部调用三路 `BmsMos_Set*(0)`，但仅在 `ControlByBms` 模式下生效（上位机强制控制时无效——见 REQ-CTRL-020）。

**验收准则（可度量）**
- Given `ErrorSta & BIT(SYS_FAULT)` 非零，When `DealFault()` 执行，Then 充电 MOS 请求 = 0，放电 MOS 请求 = 0，预充 MOS 请求 = 0。

---

### REQ-CTRL-018  DISC_FAULT 时断开放电 MOS

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:DealFault()` 第 485–488 行 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `DealFault()` 检测到 `ErrorSta` 包含 `DISC_FAULT` 位时，系统应立即将放电 MOS 和预充 MOS 请求置 0，同时保持充电 MOS 状态不变。

**理由 / 代码依据**
> `if(ErrorSta & BIT(DISC_FAULT)) { BmsMos_SetDsg(0); BmsMos_SetPreDsg(0); }`。DISC_FAULT 触发条件：总压低、单体电压低、放电温度低、放电过流、SOC 低（三级）。

**验收准则（可度量）**
- Given `ErrorSta & BIT(DISC_FAULT)` 非零，When `DealFault()` 执行，Then 放电 MOS 请求 = 0，预充 MOS 请求 = 0，充电 MOS 请求维持不变。

---

### REQ-CTRL-019  CHG_FAULT 时断开充电 MOS

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:DealFault()` 第 489–492 行 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `DealFault()` 检测到 `ErrorSta` 包含 `CHG_FAULT` 位时，系统应立即将充电 MOS 请求置 0，同时保持放电 MOS 和预充 MOS 状态不变。

**理由 / 代码依据**
> `if(ErrorSta & BIT(CHG_FAULT)) { BmsMos_SetChg(0); }`。CHG_FAULT 触发条件：总压高、单体电压高、充电温度低、充电过流（三级）。

**验收准则（可度量）**
- Given `ErrorSta & BIT(CHG_FAULT)` 非零，When `DealFault()` 执行，Then 充电 MOS 请求 = 0，放电/预充 MOS 不受影响。

---

### REQ-CTRL-020  上位机可强制覆盖各 MOS 控制权

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:BmsMos_GetFinalState()`、`BmsMos_SetChg/Dsg/PreDsg()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应支持上位机通过 `UpperCmd.ChgMosCtrl` / `UpperCmd.DiscMosCtrl` / `UpperCmd.PrecMosCtrl` 对每路 MOS 独立设置三种控制模式：`ControlByBms`（BMS 自主控制）、`Force_On`（强制导通）、`Force_Off`（强制关断）；当模式为 `Force_On` 或 `Force_Off` 时，BMS 的 MOS 请求被忽略，以上位机指令为准。

**理由 / 代码依据**
> `BmsMos_GetFinalState()` 先判断 `ctrl == Force_On` 返回 1，`ctrl == Force_Off` 返回 0，`ControlByBms` 才使用 `bms_req`。`BmsMos_SetChg()` 等函数在非 `ControlByBms` 时提前 `return`，不写请求。

**验收准则（可度量）**
- Given `UpperCmd.ChgMosCtrl == Force_On`，When `BmsMos_Apply()` 执行，Then 充电 MOS 导通，无论 BMS 内部是否有 CHG_FAULT。
- Given `UpperCmd.DiscMosCtrl == Force_Off`，When `BmsMos_Apply()` 执行，Then 放电 MOS 关断，无论 BMS 内部状态。

---

### REQ-CTRL-021  限流激活时阻止充电 MOS 开启

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:BmsMos_SetChg()` 第 47–50 行 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 BMS 请求开启充电 MOS（`enable = 1`）且限流功能当前处于激活状态（`Bms.lim_state != 0`）时，系统应拒绝开启充电 MOS（请求被静默丢弃）。

**理由 / 代码依据**
> `BmsMos_SetChg()` 中：`if(enable && (Bms.lim_state != 0)) { return; }` 防止限流期间充电 MOS 重开。

**验收准则（可度量）**
- Given `Bms.lim_state = 1` 且 `ControlByBms` 模式，When `BmsMos_SetChg(1)` 被调用，Then `s_bms_mos_req.chg` 保持不变（不被置 1）。

---

### REQ-CTRL-022  CHG_FAULT 或 SYS_FAULT 时强制关断限流器

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:BmsControlTask()` 第 464–474 行 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `ErrorSta` 包含 `CHG_FAULT` 或 `SYS_FAULT` 位时，系统应调用 `LimitOff()` 关断限流器并将 `Bms.lim_state` 清零；否则执行 `Limit_Task()` 进行正常限流管理。

**理由 / 代码依据**
> `BmsControlTask()` 末尾：`if((ErrorSta & BIT(CHG_FAULT)) || (ErrorSta & BIT(SYS_FAULT))) { LimitOff(); Bms.lim_state = 0; } else { Limit_Task(); }`。

**验收准则（可度量）**
- Given `ErrorSta` 含 `CHG_FAULT` 或 `SYS_FAULT`，When `BmsControlTask()` 末尾执行，Then `LimitOff()` 被调用，`Bms.lim_state = 0`。
- Given 两者均无，Then `Limit_Task()` 被调用。

---

### REQ-CTRL-023  充电限流功能（Limit_Task）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Control.c:Limit_Task()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当充电限流使能（`Bms.Para.chg_limiten == 1`）且电池未满充（`Bms.Para.Waite_selfConsumption == FALSE`）时，如果充电电流 `Bms.current_mA > Bms.Para.chg_limiten_th`（单位 mA）且持续时间超过 `Bms.Para.chg_limiten_time_th`（单位 100 ms），系统应开启限流器（`LimitOn()`），置 `Bms.lim_state = 1`，延时 3 000 ms 后关断充电 MOS；当充电电流降至 `Bms.Para.chg_limiten_recover`（mA）以下且持续 20 × 100 ms（2 000 ms）后，系统应关闭限流器并重新允许充电 MOS 开启。

**理由 / 代码依据**
> `Limit_Task()` 完整逻辑：触发条件含时间滞后（`system_run_time_100ms - overtime > chg_limiten_time_th`），触发后 `delay_ms(3000)` 再断 MOS；恢复条件含 20 × 100 ms（`system_run_time_100ms - starttime > 20`）加 `delay_ms(2000)` 后 `LimitOff()`。

**验收准则（可度量）**
- Given `chg_limiten == 1`、`current_mA > chg_limiten_th` 持续超过 `chg_limiten_time_th × 100 ms`，Then `Bms.lim_state = 1`，限流器开启，3 s 后充电 MOS 断开。
- Given `lim_state == 1`、`current_mA < chg_limiten_recover` 持续 ≥ 2 000 ms，Then `LimitOff()`，`lim_state = 0`，充电 MOS 可重新开启。

---

### REQ-CTRL-024  满充状态下关闭限流器

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Control.c:Limit_Task()` 末尾 `Waite_selfConsumption == TRUE` 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当电池已满充（`Bms.Para.Waite_selfConsumption != FALSE`）时，如果当前限流控制权为 BMS（`UpperCmd.LimitCtrl == ControlByBms`），系统应关闭限流器并清零 `Bms.lim_state`。

**理由 / 代码依据**
> `Limit_Task()` else 分支：满充 → `if(UpperCmd.LimitCtrl == ControlByBms) { LimitOff(); Bms.lim_state = 0; }`。

**验收准则（可度量）**
- Given `Waite_selfConsumption = TRUE` 且 `LimitCtrl == ControlByBms`，When `Limit_Task()` 执行，Then `LimitOff()` 调用，`Bms.lim_state = 0`。

---

### REQ-CTRL-025  PROTECT 状态下的分级 MOS 策略

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:BmsControlTask()` `BMS_STA_PROTECT` 分支 |
| 验证方法 | 分析 + 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 在 `BMS_STA_PROTECT` 状态下，系统应按以下优先级处理 MOS：
> 1. 若存在 `SYS_FAULT` → 全体 MOS 断开，不进行状态退出（`break`）；
> 2. 若同时存在 `DISC_FAULT` 和 `CHG_FAULT` → 全体 MOS 断开，不进行状态退出；
> 3. 若仅有 `DISC_FAULT` → 仅允许充电 MOS 开启（`BmsControl_RequestChgByParam()`）；
> 4. 若仅有 `CHG_FAULT` → 尝试放电预充序列（`BmsControl_RequestDsgWithPreDsg()`）；
> 5. 若无任何故障（`ErrorSta == 0`）→ 转移到 `BMS_STA_STANDBY`。

**理由 / 代码依据**
> `BMS_STA_PROTECT` 分支的 `else if` 链，精确实现充电故障时可放电、放电故障时可充电的分级降级策略。同时若检测到实际充电/放电电流则更新相应状态（`AppAfe_IsCharging()`/`AppAfe_IsDischarging()`）。

**验收准则（可度量）**
- Given `SYS_FAULT` 置位，Then 不开任何 MOS，状态停留在 PROTECT。
- Given 仅 `DISC_FAULT`，Then 允许充电 MOS 开启，放电 MOS 保持关闭。
- Given 仅 `CHG_FAULT`，Then 允许启动预充序列，充电 MOS 保持关闭。
- Given `ErrorSta == 0`，Then `Bms.sta = BMS_STA_STANDBY`。

---

### REQ-CTRL-026  充电状态屏蔽放电故障、放电/预充状态屏蔽充电故障

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:IsBmsWarning()` 第 2056–2064 行 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统处于 `BMS_STA_CHARGING` 状态时，`IsBmsWarning()` 应清除 `DISC_FAULT` 位（避免充电器供电情况下的误保护）；当系统处于 `BMS_STA_DISCHARGING` 或 `BMS_STA_PRECHG` 状态时，应清除 `CHG_FAULT` 位。

**理由 / 代码依据**
> 代码注释原文"充电状态无视放电故障，放电状态无视充电故障"，对应第 2056–2064 行。

**验收准则（可度量）**
- Given `Bms.sta == BMS_STA_CHARGING` 且 DISC_FAULT 原本应置位，When `IsBmsWarning()` 返回，Then 返回值 `& BIT(DISC_FAULT) == 0`。
- Given `Bms.sta == BMS_STA_DISCHARGING` 且 CHG_FAULT 原本应置位，Then 返回值 `& BIT(CHG_FAULT) == 0`。

---

### REQ-CTRL-027  低电量自动休眠（`needsleep`）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `WarningTask.c:needsleep()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当总压低预警等级 ≥ `WARNING_LEVLE_2` 或单体电压低预警等级 ≥ `WARNING_LEVLE_2` 时，系统应启动休眠计时；若此状态持续超过 6 000 × 100 ms（600 s / 10 分钟），系统应进入低功耗休眠（`AppPower_EnterNormalSleep()`）。

**理由 / 代码依据**
> `needsleep()` 内：条件满足则 `needsleep` 位置 1，计时 `system_run_time_100ms - time > 6000`（100 ms 单位，6 000 × 100 ms = 600 s）后调用休眠。

**验收准则（可度量）**
- Given `warn_level.TotalVoltLow ≥ 2` 或 `warn_level.CellVoltLow ≥ 2` 持续 ≥ 600 s，When `needsleep()` 在主循环中被 `BmsControlTask()` 调用，Then `AppPower_EnterNormalSleep()` 被触发。

---

### REQ-CTRL-028  按键 2 s 长按关机

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppPower.c:AppPower_CheckPowerOff()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当检测到电源按键持续按下 2 秒（`getmsg() == PWR_KEY_2S`）时，系统应停止 TIM1 计时并进入 NORMAL_SLEEP 模式（`enter_sleep(NORMAL_SLEEP)`）。

**理由 / 代码依据**
> `AppPower_CheckPowerOff()` 在主循环末尾每迭代检测一次，检测到 2 s 按键消息后 `__HAL_TIM_DISABLE(&htim1)` 停时基，再调用 `enter_sleep()`。

**验收准则（可度量）**
- Given 电源键持续按下 ≥ 2 s，When `AppPower_CheckPowerOff()` 在主循环中执行，Then TIM1 停止，系统进入 NORMAL_SLEEP。

---

### REQ-CTRL-029  MOS 最终状态写入（BmsMos_Apply）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Control.c:BmsMos_Apply()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应在每次 `BmsControlTask()` 执行末尾通过 `BmsMos_Apply()` 将最终 MOS 状态写入 AFE 驱动层；最终状态由上位机控制模式（`Force_On`/`Force_Off`/`ControlByBms`）与 BMS 内部请求共同决定，以 `BmsMos_GetFinalState()` 仲裁为准。

**理由 / 代码依据**
> `BmsControlTask()` 末行 `BmsMos_Apply()`，调用 `AppAfe_SetChgMos()` / `AppAfe_SetDsgMos()` / `AppAfe_SetPreDsgMos()` 写实际硬件。整个主循环每次迭代均执行。

**验收准则（可度量）**
- Given `BmsControlTask()` 任一次执行，Then `BmsMos_Apply()` 必定在本次 `BmsControlTask()` 执行完毕前被调用一次。
- Given `UpperCmd.*MosCtrl == Force_On`，Then 对应 AFE MOS 寄存器 = 导通，不受内部 BMS 请求影响。

---

### REQ-CTRL-030  非预充状态时复位预充状态机

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Control.c:BmsControlTask()` 第 459–461 行 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当系统工作状态不为 `BMS_STA_PRECHG` 时，系统应在每次 `BmsControlTask()` 末尾复位预充状态机（`PreDischg_Reset()`），清除所有预充内部计时和状态。

**理由 / 代码依据**
> `if(Bms.sta != BMS_STA_PRECHG) { PreDischg_Reset(); }` 防止预充残留状态影响下一次预充启动。

**验收准则（可度量）**
- Given `Bms.sta ≠ BMS_STA_PRECHG`，When `BmsControlTask()` 执行到状态机 switch 之后，Then `s_pre_dsg_step = PRE_DSG_IDLE`，所有预充计时变量清零。

---

### REQ-CTRL-031  主循环任务编排顺序

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `main.c:main()` while(1) 循环 |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应始终按以下固定顺序在主循环中依次执行任务：
> 1. ADC 采样处理（`BspAdc_TakeReady/Process/Main_UpdateBmsAdc`）
> 2. AFE 采集（`AppAfe_Process`）
> 3. AFE 秒级任务（`AppAfe_OneSecondTask`）
> 4. AFE 保护位清除（`AppAfe_ProtectTask`）
> 5. 负载检测（`AppAfe_LoadCheckTask`）
> 6. 开路检测（`AppAfe_CellOpenTask`）
> 7. 均衡（`AppAfe_BalanceTask`）
> 8. 数据统计（`BmsinfoStatistics`）
> 9. SOC 计算（`SocTask`）
> 10. 告警与保护生成（`CreatWaringCode`、`CreatWaringCode2`）
> 11. 加热控制（`HeatControlTask`）
> 12. 上位机命令处理（`CMD_Task`）
> 13. **MOS 控制（`BmsControlTask`）**
> 14. LED 显示（`LedSocTask`、`LedWarnTask`）
> 15. Log 更新、参数存储、升级处理（`UpdateLogdata`、`ParaSaveTask`、`UpperComUpgradeTask`）
> 16. 20 s 周期 Flash 记录 / 2 s 周期 SD 卡记录
> 17. 关机事件检测（`AppPower_CheckPowerOff`）

**理由 / 代码依据**
> 顺序保证了 MOS 控制（第 13 步）在最新数据统计和告警评估（8–10 步）之后执行，确保决策所用数据是本轮最新值。

**验收准则（可度量）**
- Given 主循环任意一次迭代，When 代码执行，Then `BmsControlTask()` 的调用必定在 `CreatWaringCode2()` 之后、`LedSocTask()` 之前。

---

### REQ-CTRL-032  TIM1 1 ms 系统时基

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `main.c:HAL_TIM_PeriodElapsedCallback()`，`AppTime_Tick1ms()` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应使用 TIM1 中断（1 ms 周期）驱动 `AppTime_Tick1ms()`，为 `u32SysTime`（毫秒计时）和 `system_run_time_100ms`（100 ms 计时）提供时基，所有超时判断均依赖此时基。

**理由 / 代码依据**
> `HAL_TIM_PeriodElapsedCallback` 中 `if(htim->Instance == TIM1) { AppTime_Tick1ms(); }`；`u32SysTime` 宏映射 `AppTime_GetMs()`，`system_run_time_100ms` 映射 `AppTime_Get100ms()`（`common.h`）。

**验收准则（可度量）**
- Given TIM1 正常运行，When 等待 1 000 ms 物理时间，Then `u32SysTime` 增量 ≈ 1 000（允许 ±1%）。

---

### REQ-CTRL-033  AFE 唤醒脉冲初始化

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `main.c:main()` 第 134–136 行 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 在系统初始化阶段，系统应向 AFE_WAKE 引脚发送一个 1 000 ms 高电平唤醒脉冲（置高后等待 1 000 ms 再置低），以唤醒 AFE 芯片。

**理由 / 代码依据**
> `HAL_GPIO_WritePin(AFE_WAKE_GPIO_Port, AFE_WAKE_Pin, GPIO_PIN_SET); delay_ms(1000); HAL_GPIO_WritePin(..., GPIO_PIN_RESET);`

**验收准则（可度量）**
- Given 系统上电后 GPIO 初始化完成，When 初始化代码执行到 AFE 唤醒段，Then AFE_WAKE 引脚高电平持续 ≥ 1 000 ms。

---

## 存疑与观察

1. **`delay_ms()` 在主循环中的阻塞风险**（存疑）：`Limit_Task()` 内在触发限流后调用 `delay_ms(3000)`，在恢复限流时调用 `delay_ms(2000)`。这是裸机阻塞延时，期间主循环全部任务（含 AFE 采集、告警评估）均暂停执行。若此时发生短路等紧急事件，硬件保护（AFE 层）可能仍能响应，但 BMS 软件无法在这 2-3 s 内响应任何新故障。建议评估是否改为非阻塞计时（状态=存疑，疑似设计不足）。

2. **`BmsControl_MosAllOffNow()` 与 `BmsMos_AllOff()` 的差异**（观察）：上电初始化调用 `BmsControl_MosAllOffNow()`（直接调用 AFE 驱动，绕过 `UpperCmd` 控制模式检查），而 `DealFault()` / `BmsControlTask()` 内部使用 `BmsMos_AllOff()` → `BmsMos_SetXxx(0)`（受 `ControlByBms` 检查约束）。若上位机将某路 MOS 设为 `Force_On`，则 `DealFault()` 中的 `SYS_FAULT` 断开请求会被上位机覆盖（见 `BmsMos_GetFinalState()`），**形成安全漏洞**：上位机强制开 MOS 可能在系统故障期间维持导通。状态=存疑 ⚠️。

3. **`Limit_Task2()` 函数未被调用**（观察）：`Limit_Task2()` 有独立的限流逻辑（硬编码阈值 25 000 mA / 5 000 mA），但在 `BmsControlTask()` 和 `main.c` 中均未被调用（仅 `BMS_Control.h` 声明），疑为遗留调试函数或未完成的替代版本。状态=存疑。

4. **注释中有被屏蔽的 SOC90 限流逻辑**（观察）：`Limit_Task()` 中有整段注释掉的 SOC > 90% 限流触发分支（`LIMIT_REASON_SOC90`），说明曾经有 SOC 触发限流的设计意图，但当前代码未启用。应确认是有意禁用还是待实现功能。状态=缺口（待确认）。

5. **`Bms.Para.PreDiscTime` 的单位含糊**（存疑）：`PreDischg()` 的 `timeout` 参数以 `system_run_time_100ms` 为单位（100 ms 粒度），但 `USER_PARA_S.PreDiscTime` 定义为 `uint16_t` 且无单位注释，调用方传入 `Bms.Para.PreDiscTime` 直接作为 100 ms 计数使用。实际时间 = `PreDiscTime × 100 ms`，最大值约 6 553.5 s（uint16_t 上限），需在参数文档中明确。

6. **`BMS_STA_POWEROFF` 状态为空实现**（观察）：`BMS_STA_POWEROFF` 分支为空 `break`，关机后无任何 MOS 操作或状态清理。实际关机由 `AppPower_CheckPowerOff()` 在主循环末尾处理，两者无互锁。若 `Bms.sta` 先被设为 `BMS_STA_POWEROFF`，下一迭代 MOS 状态由上一迭代的 `BmsMos_Apply()` 结果决定，可能不安全。状态=存疑 ⚠️。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-CTRL-001 | 上电立即关断所有 MOS | 是 ⚠️ | `main.c:main()` | 已实现 |
| REQ-CTRL-002 | 系统工作状态枚举 | 否 | `BMS_Info.h:BMS_WORK_STA_S` | 已实现 |
| REQ-CTRL-003 | 上电默认进入 DISCONNET 状态 | 是 ⚠️ | `BMS_Info.c`, `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-004 | 上电 5 s 稳定等待 | 否 | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-005 | DISCONNET 状态下的保护优先转移 | 是 ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-006 | DISCONNET 状态向 STANDBY / PRECHG 的正常转移 | 否 | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-007 | 预充序列：先预充 MOS 后等待 Vmos 收敛 | 是 ⚠️ | `BMS_Control.c:PreDischg()` | 已实现 |
| REQ-CTRL-008 | 预充过电流保护 | 是 ⚠️ | `BMS_Control.c:PreDischg_CheckFault()` | 已实现 |
| REQ-CTRL-009 | 预充期间故障即失败 | 是 ⚠️ | `BMS_Control.c:PreDischg_CheckFault()` | 已实现 |
| REQ-CTRL-010 | 预充超时强制完成 | 否 | `BMS_Control.c:PreDischg()` | 已实现 |
| REQ-CTRL-011 | 如放电 MOS 已开则跳过预充 | 否 | `BMS_Control.c:PreDischg()` | 已实现 |
| REQ-CTRL-012 | 预充成功后转 STANDBY，失败后转 PROTECT | 是 ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-013 | 故障立即触发保护状态 | 是 ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-014 | 正常运行态下的充放电状态更新 | 否 | `BMS_Control.c:BmsControl_UpdateRunState()` | 已实现 |
| REQ-CTRL-015 | 电流状态判定阈值 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-CTRL-016 | SYS_FAULT 触发条件（全体 MOS 断开） | 是 ⚠️ | `WarningTask.c:IsBmsWarning()` | 已实现 |
| REQ-CTRL-017 | SYS_FAULT 时断开所有 MOS | 是 ⚠️ | `BMS_Control.c:DealFault()` | 已实现 |
| REQ-CTRL-018 | DISC_FAULT 时断开放电 MOS | 是 ⚠️ | `BMS_Control.c:DealFault()` | 已实现 |
| REQ-CTRL-019 | CHG_FAULT 时断开充电 MOS | 是 ⚠️ | `BMS_Control.c:DealFault()` | 已实现 |
| REQ-CTRL-020 | 上位机可强制覆盖各 MOS 控制权 | 是 ⚠️ | `BMS_Control.c:BmsMos_GetFinalState()` | 已实现 |
| REQ-CTRL-021 | 限流激活时阻止充电 MOS 开启 | 是 ⚠️ | `BMS_Control.c:BmsMos_SetChg()` | 已实现 |
| REQ-CTRL-022 | CHG_FAULT 或 SYS_FAULT 时强制关断限流器 | 是 ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-023 | 充电限流功能（Limit_Task） | 否 | `BMS_Control.c:Limit_Task()` | 已实现 |
| REQ-CTRL-024 | 满充状态下关闭限流器 | 否 | `BMS_Control.c:Limit_Task()` | 已实现 |
| REQ-CTRL-025 | PROTECT 状态下的分级 MOS 策略 | 是 ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-026 | 充电状态屏蔽放电故障、放电/预充状态屏蔽充电故障 | 是 ⚠️ | `WarningTask.c:IsBmsWarning()` | 已实现 |
| REQ-CTRL-027 | 低电量自动休眠 | 否 | `WarningTask.c:needsleep()` | 已实现 |
| REQ-CTRL-028 | 按键 2 s 长按关机 | 否 | `AppPower.c:AppPower_CheckPowerOff()` | 已实现 |
| REQ-CTRL-029 | MOS 最终状态写入（BmsMos_Apply） | 是 ⚠️ | `BMS_Control.c:BmsMos_Apply()` | 已实现 |
| REQ-CTRL-030 | 非预充状态时复位预充状态机 | 否 | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-031 | 主循环任务编排顺序 | 否 | `main.c:main()` | 已实现 |
| REQ-CTRL-032 | TIM1 1 ms 系统时基 | 否 | `main.c:HAL_TIM_PeriodElapsedCallback()` | 已实现 |
| REQ-CTRL-033 | AFE 唤醒脉冲初始化 | 否 | `main.c:main()` | 已实现 |
