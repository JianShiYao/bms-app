# REQ-PROT：保护与告警域 需求规格

> **覆盖源文件**
> - `Application/WarningTask.c`（`CreatWaring()` / `CreatWaring2()` / `WarningRecover()` / `WarningRecover2()` / `CreatWaringCode()` / `CreatWaringCode2()` / `IsBmsWarning()` / `TieTaWarnTask()` / `needsleep()`）
> - `Application/WarningTask.h`（告警枚举、`__WARNING_LEVEL` / `__WARNING2_LEVEL` / `PROT_LOCK` 结构体）
> - `Application/ParaSet.h`（全部 Macro_xxx 默认阈值、去抖时间、恢复阈值、二级/短路参数）
> - `Application/ParaSet.c`（`SetDefaultPara()` 默认值赋值）
> - `Application/AppLimit.c` / `Application/AppLimit.h` / `Driver/BSP/bsp_limit.h`（限流接口）
> - `Driver/AFE/AFE_Protect.h`（AFE 硬件保护接口宏定义）
> - `Application/AppAfe.h`（`AppAfe_IsOcd2Fault` / `AppAfe_IsScFault` / `AppAfe_ClearOcd2Fault` / `AppAfe_ClearScFault` 等接口）
> - `Application/BMS_Info.h`（`BMS_PARA_S` 中 `SecondOverCurCnt` / `DiscCurShortCnt` / `chg_limiten*` 等字段）

> **单位约定**：电压 mV；电流 mA（充电为正，放电为负）；温度 0.1℃；去抖/恢复时间单位 ×100ms（`system_run_time_100ms` 计数器，1 tick = 100ms）；SOC 1%。

---

## 需求列表

### REQ-PROT-001  告警多级体系架构（Level 0/1/3）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.h:WARNING_LEVLE_0~3 枚举`、`__WARNING_LEVEL` 结构体 |
| 验证方法 | 检视 + 测试 |
| 状态 | 已实现 |

**需求描述**
> 系统应始终维护三级告警机制：LEVEL_0（正常）、LEVEL_1（一级告警）、LEVEL_3（三级保护动作）。每个告警项以 2-bit 字段存储于 `warn_level` / `warn2_level` 结构体，并映射到对外通信寄存器 `warn1code0`（LEVEL_1 置位）/ `warn2code0`（LEVEL_2 置位，当前未使用）/ `warn3code0`（LEVEL_3 置位）/ `warn3code1`（第二类故障 LEVEL_3）。

**理由 / 代码依据**
> `WarningTask.h` 定义 `WARNING_LEVLE_0~3`；所有 `switch(warn_level.xxx)` 分支将等级映射到对应 warn_xcode0 位。注：枚举注释均写"一级"但实际值为 0/1/2/3，LEVEL_2 在当前代码中几乎未触发（`CreatWaring` 仅产生 LEVEL_1 和 LEVEL_3）。

**验收准则**
- Given BMS 正常运行，When 无任何超限，Then `warn1code0 = warn2code0 = warn3code0 = warn3code1 = 0`。
- Given 某项达到一级阈值持续去抖时间，When 触发，Then 对应位仅在 `warn1code0` 置 1，`warn3code0` 该位为 0。
- Given 某项达到三级阈值持续去抖时间，When 触发，Then 对应位仅在 `warn3code0` 置 1，`warn1code0` 该位为 0。

---

### REQ-PROT-002  故障分类与接触器保护动作

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:IsBmsWarning()` 行 1988-2067 |
| 验证方法 | 测试 + 检视 |
| 状态 | 已实现 |

**需求描述**
> 当发生三类故障时，系统应按下表分别响应：
> - **SYS_FAULT**（系统故障，断所有接触器）：单体压差大(LEVEL_3)、MOS 过温(LEVEL_3)、环境过温(LEVEL_3)、环境过低(LEVEL_3)、单体过温(LEVEL_3)、预充故障(LEVEL_3)、短路(LEVEL_3)、二级过流(LEVEL_3)、进水(LEVEL_3)。
> - **CHG_FAULT**（充电故障，断充电侧）：总压过高(LEVEL_3)、单体过高(LEVEL_3)、充电欠温(LEVEL_3)、充电过流(LEVEL_3)。
> - **DISC_FAULT**（放电故障，断放电侧）：总压过低(LEVEL_3)、单体过低(LEVEL_3)、放电欠温(LEVEL_3)、放电过流(LEVEL_3)、SOC 过低(LEVEL_3)。
>
> 在 **充电状态** 下屏蔽 DISC_FAULT；在 **放电或预充状态** 下屏蔽 CHG_FAULT。

**理由 / 代码依据**
> `IsBmsWarning()` 返回 `fault_type` 位掩码（`CHG_FAULT`=bit0, `DISC_FAULT`=bit1, `SYS_FAULT`=bit2），由状态机调用决定断开哪路接触器。

**验收准则**
- Given BMS 在充电态，When 单体过低 LEVEL_3 触发，Then DISC_FAULT 被屏蔽，充电不受影响。
- Given BMS 在放电态，When 充电过流 LEVEL_3 触发，Then CHG_FAULT 被屏蔽，放电不受影响。
- Given 短路 LEVEL_3 触发，Then SYS_FAULT 置位，所有接触器断开。

---

### REQ-PROT-003  总压过高告警与保护（LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 27-57；`ParaSet.h` 行 76-78；`IsBmsWarning()` 行 2022 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 `Bms.intvolt_mV`（总电压）持续超过阈值超过去抖时间，系统应升级告警等级，并在 LEVEL_3 时触发充电故障保护（断充电接触器/MOS）：
> - 一级告警：≥ 57000 mV，去抖 2000ms（20 × 100ms），置 `warn1code0[TotalVoltHigh]`；
> - 三级保护：≥ 57500 mV，去抖 2000ms，置 `warn3code0[TotalVoltHigh]`，产生 CHG_FAULT。
>
> 恢复条件（LEVEL_3 → LEVEL_1）：总压 ≤ 56600 mV 持续 500ms（5 × 100ms）；恢复到 LEVEL_0：≤ 56600 mV 持续 200ms（2 × 100ms）。

**理由 / 代码依据**
> `warn_threshold.TotalVoltHigh_1 = 57000`，`TotalVoltHigh_3 = 57500`；`warn_time._1/_3 = 20`；`warn_recover.TotalVoltHigh_1/3 = 56600`；`recover_time._1 = Macro_Recover2s_100ms=20`，`_3 = Macro_Recover5s_100ms=50`。

**验收准则**
- Given 总压 = 57500 mV，When 保持 2000ms，Then warn3code0[TotalVoltHigh]=1，充电断开。
- Given 总压从 57500mV 降至 56600mV，When 保持 500ms，Then warn3code0[TotalVoltHigh]=0，升级为 LEVEL_1。
- Given 总压从 57000mV 降至 56600mV，When 保持 200ms，Then warn1code0[TotalVoltHigh]=0（恢复正常）。

---

### REQ-PROT-004  总压过低告警与保护（LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 61-91；`ParaSet.h` 行 85-88；`IsBmsWarning()` 行 2041 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 `Bms.intvolt_mV` 持续低于阈值超过去抖时间，系统应：
> - 一级告警：≤ 45000 mV，去抖 2000ms；
> - 三级保护：≤ 42000 mV，去抖 2000ms，产生 DISC_FAULT（断放电接触器/MOS）。
>
> 恢复条件：总压 ≥ 47000 mV 持续 500ms（LEVEL_3→LEVEL_0 直接恢复）；LEVEL_1 恢复：≥ 47000 mV 持续 200ms。
> 附加：当 `warn_level.TotalVoltLow ≥ LEVEL_2` 时，系统触发 `needsleep` 计时（600s 后进入休眠）。

**理由 / 代码依据**
> `Macro_TotalVoltLow_1=45000`，`_3=42000`；`warn_recover.TotalVoltLow_1/3=47000`；`needsleep()` 行 2083 检查 LEVEL_2 条件。注意：LEVEL_3 的 `WarningRecover` 逻辑直接恢复到 LEVEL_0（无中间 LEVEL_1 过渡），与过高的分级恢复不同。

**验收准则**
- Given 总压 = 42000 mV，When 保持 2000ms，Then warn3code0[TotalVoltLow]=1，DISC_FAULT 断放电。
- Given 总压从 42000mV 升至 47000mV，When 持续 500ms，Then 告警清除（LEVEL_0）。

---

### REQ-PROT-005  单体电压过高告警与保护（LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 93-123；`ParaSet.h` 行 72-74 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当最高单体电压 `Bms.CellMaxVoltage_mV` 持续超过阈值超过去抖时间，系统应：
> - 一级告警：≥ 3700 mV，去抖 2000ms；
> - 三级保护：≥ 3750 mV，去抖 2000ms，产生 CHG_FAULT。
>
> 恢复：最高单体 ≤ 3600 mV 持续 500ms（LEVEL_3 → LEVEL_1）；≤ 3600 mV 持续 200ms（LEVEL_1 → LEVEL_0）。

**理由 / 代码依据**
> `Macro_CellVoltHigh_1=3700`，`_3=3750`；`Macro_Rcv_CellVoltHigh_1/3=3600`；恢复时间 2s/5s。

**验收准则**
- Given 最高单体 = 3750 mV，When 保持 2000ms，Then CHG_FAULT 触发，充电断开。
- Given 最高单体从 3750 mV 降至 3600 mV，When 保持 500ms，Then 降为 LEVEL_1。

---

### REQ-PROT-006  单体电压过低告警与保护（LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 126-156；`ParaSet.h` 行 80-83 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当最低单体电压 `Bms.CellMinVoltage_mV` 持续低于阈值超过去抖时间，系统应：
> - 一级告警：≤ 2500 mV，去抖 2000ms；
> - 三级保护：≤ 2200 mV，去抖 2000ms，产生 DISC_FAULT。
>
> 恢复：最低单体 ≥ 2800 mV 持续 500ms（LEVEL_3 → LEVEL_1）；≥ 2800 mV 持续 200ms（LEVEL_1 → LEVEL_0）。
> 附加：当 `warn_level.CellVoltLow ≥ LEVEL_2` 时，`needsleep` 开始计时。

**理由 / 代码依据**
> `Macro_CellVoltLow_1=2500`，`_3=2200`；`Macro_Rcv_CellVoltLow_1/3=2800`；`needsleep()` 行 2093 检查 LEVEL_2。

**验收准则**
- Given 最低单体 = 2200 mV，When 保持 2000ms，Then DISC_FAULT 断放电。
- Given 最低单体从 2200mV 升至 2800mV，When 保持 500ms，Then 降为 LEVEL_1。

---

### REQ-PROT-007  单体压差过高告警与保护（平台区 / 非平台区双阈值，LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 158-207；`WarningTask.c:WarningRecover()` 行 751-799；`ParaSet.h` 行 90-96 |
| 验证方法 | 测试 + 检视 |
| 状态 | 已实现 |

**需求描述**
> 系统应检测单体间最大电压差 `Bms.MaxDeltaCellVolt_mV` 并区分两种工作区域：
> - **平台区**（所有单体电压在 3100~3400 mV 范围内）：三级保护阈值 400 mV，去抖 2000ms。
> - **非平台区**（任一单体超出 3100~3400 mV）：三级保护阈值 1000 mV，去抖 2000ms。
>
> 达到 LEVEL_3 时产生 SYS_FAULT（断所有接触器）。
> 恢复：压差降至恢复阈值以下（平台区 ≤ 100 mV / 非平台区 ≤ 400 mV）持续 500ms 后恢复 LEVEL_0。

**理由 / 代码依据**
> `Macro_DeltaVoltHigh_3=400`（平台区），`Macro_DeltaVoltHigh2_3=1000`（非平台区）；`is_in_platform` 变量根据所有单体是否全在 3100~3400 mV 内计算；`Macro_Rcv_DeltaVoltHigh_3=100`，`Macro_Rcv_DeltaVoltHigh2_3=400`。注：告警仅有 LEVEL_3，无 LEVEL_1 中间级。

**验收准则**
- Given 所有单体在平台区，When 压差 = 400 mV 保持 2000ms，Then SYS_FAULT 触发。
- Given 某单体超出平台区，When 压差 = 1000 mV 保持 2000ms，Then SYS_FAULT 触发。
- Given 平台区压差从 400mV 降至 100mV，When 持续 500ms，Then 告警清除。

---

### REQ-PROT-008  单体温度过高告警与保护（合并充放电，LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 210-242；`ParaSet.h` 行 145-147 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 系统应以最大温度 `Bms.MaxTemp`（单位 0.1℃）检测单体过温，不区分充放电状态（已注释掉原来按充放电分别保护的代码）：
> - 一级告警：≥ 600（即 60.0℃），去抖 2000ms；
> - 三级保护：≥ 650（即 65.0℃），去抖 2000ms，产生 SYS_FAULT（断所有接触器）。
>
> 恢复：MaxTemp ≤ 500（50.0℃）持续 500ms（LEVEL_3 → LEVEL_1）；≤ 500 持续 200ms（LEVEL_1 → LEVEL_0）。

**理由 / 代码依据**
> `Macro_CellTempHigh_1=600`，`_3=650`；`Macro_Rcv_CellTempHigh_1/3=500`；放电过温（`DiscTempHigh`）相关代码已全部注释，功能由 `CellTempHigh` 替代。

**验收准则**
- Given MaxTemp = 650（65℃），When 保持 2000ms，Then SYS_FAULT 断所有接触器。
- Given MaxTemp 从 650 降至 500，When 保持 500ms，Then 降为 LEVEL_1。

---

### REQ-PROT-009  充电欠温告警与保护（LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 246-278；`ParaSet.h` 行 113-115 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 仅在充电电流 `Bms.current_mA ≥ 300 mA` 时检测充电欠温（以最低温度 `Bms.MinTemp` 为判据）：
> - 一级告警：≤ 50（5.0℃），去抖 2000ms；
> - 三级保护：≤ 0（0.0℃），去抖 2000ms，产生 CHG_FAULT（断充电）。
>
> 恢复（LEVEL_3 → LEVEL_0 直接跳转）：MinTemp ≥ 100（10.0℃）持续 500ms；LEVEL_1 → LEVEL_0：MinTemp ≥ 100 持续 200ms。恢复阈值含温差（10℃ 回差），避免振荡。

**理由 / 代码依据**
> `Macro_ChgTempLow_1=50`，`_3=0`；`Macro_Rcv_ChgTempLow_1=100`，`_3=50`；充电判断条件 `current_mA >= 300`（300mA）防止静置时误报。

**验收准则**
- Given 充电电流 1000mA，MinTemp = 0（0℃），When 保持 2000ms，Then CHG_FAULT 断充电。
- Given 充电电流 0（静置），MinTemp = -20（-2℃），Then 不触发充电欠温保护。
- Given LEVEL_3 后 MinTemp 升至 100，When 持续 500ms，Then 告警清除（LEVEL_0）。

---

### REQ-PROT-010  放电欠温告警与保护（LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 330-363；`ParaSet.h` 行 121-123 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 仅在放电电流 `Bms.current_mA ≤ -300 mA` 时检测放电欠温（以最低温度 `Bms.MinTemp` 为判据）：
> - 一级告警：≤ 0（0.0℃），去抖 2000ms；
> - 三级保护：≤ -100（-10.0℃），去抖 2000ms，产生 DISC_FAULT（断放电）、铁塔等级 LEVEL_4。
>
> 恢复：LEVEL_3：MinTemp ≥ 0（0℃）持续 500ms → LEVEL_1；LEVEL_1：MinTemp ≥ 50（5℃）持续 200ms → LEVEL_0。

**理由 / 代码依据**
> `Macro_DscTempLow_1=0`，`_3=-100`；`Macro_Rcv_DscTempLow_1=50`，`_3=0`；放电判断条件 `current_mA <= -300`。

**验收准则**
- Given 放电电流 -5000mA，MinTemp = -100（-10℃），When 保持 2000ms，Then DISC_FAULT 断放电。
- Given 静置（电流 -100mA），MinTemp = -200，Then 不触发保护。

---

### REQ-PROT-011  充电过流告警与保护（LEVEL_1 / LEVEL_3，含自锁次数限制）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 365-409；`WarningTask.c:WarningRecover()` 行 804-828；`ParaSet.h` 行 99-101 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 仅在充电电流 `Bms.current_mA ≥ 300 mA` 时检测充电过流（以电流绝对值为判据）：
> - 一级告警：≥ 30000 mA（30A），去抖 2000ms；
> - 三级保护：≥ 110000 mA（110A），去抖 3000ms，产生 CHG_FAULT（断充电）。
>
> 三级保护同时触发 `ChgCur.ProtCnt++`（累计保护次数）和 `ChgCur.IntTime` 累积间隔时间（5分钟内计数），用于多次触发自锁判断。
>
> 恢复：LEVEL_3 → LEVEL_1：电流绝对值 ≤ 20000 mA 持续 500ms，**且 `ChgCur.ProtCnt ≤ SecondOverCurCnt`（默认 3次）**；若超过 3 次，LEVEL_3 不恢复到 LEVEL_1（永久锁定，直到外部干预）。LEVEL_1 → LEVEL_0：电流 ≤ 20000 mA 持续 200ms，**或电流为放电方向（current_mA ≤ -300）**。

**理由 / 代码依据**
> `Macro_ChgCurHigh_3=110000`，`_1=30000`；`warn_time._3=30（3s）`，`_1=20（2s）`；`warn_recover.ChgCurHigh_1/3=20000`；`ChgCur.ProtCnt > SecondOverCurCnt` 条件阻止 LEVEL_3 自动恢复；铁塔 LEVEL_5 映射：`ChgCur.ProtCnt > SecondOverCurCnt` 时。

**验收准则**
- Given 充电电流 = 110A，When 保持 3000ms，Then CHG_FAULT，ChgCur.ProtCnt = 1。
- Given ChgCur.ProtCnt > 3 后充电过流 LEVEL_3，When 电流降至 15A 持续 500ms，Then 保护不恢复（自锁）。
- Given 充电过流 LEVEL_1，When 切换为放电（current_mA = -5000），Then LEVEL_1 在 200ms 后清除。

---

### REQ-PROT-012  放电过流告警与保护（LEVEL_1 / LEVEL_3，含自锁次数限制）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 411-456；`WarningTask.c:WarningRecover()` 行 830-855；`ParaSet.h` 行 104-106 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 仅在放电电流 `Bms.current_mA ≤ -300 mA` 时检测放电过流（以电流绝对值为判据）：
> - 一级告警：≥ 105000 mA（105A），去抖 2000ms；
> - 三级保护：≥ 110000 mA（110A），去抖 3000ms，产生 DISC_FAULT。
>
> 三级保护触发 `DiscCur.ProtCnt++`（累计），`DiscCur.IntTime` 5分钟窗口计时。
>
> 恢复：LEVEL_3 → LEVEL_1：电流绝对值 ≤ 95000 mA 持续 180s（`Macro_Recover5s_100ms*36 = 1800 ticks = 180s`），**且 `DiscCur.ProtCnt ≤ SecondOverCurCnt`（3次）**；超过 3 次则自锁。LEVEL_1 → LEVEL_0：电流 ≤ 95000mA 持续 200ms，**或切换为充电方向（current_mA ≥ 300）**。

**理由 / 代码依据**
> `Macro_DiscCurHigh_1=105000`，`_3=110000`；恢复阈值 95000；**放电过流 LEVEL_3 的恢复时间为 `recover_time.DiscCurHigh_3 = Macro_Recover5s_100ms*36 = 1800 ticks = 180秒`**，远大于其他故障的 5s，是有意的长延迟设计。

**验收准则**
- Given 放电电流 = -110A，When 保持 3000ms，Then DISC_FAULT，DiscCur.ProtCnt = 1。
- Given DiscCur.ProtCnt = 0，LEVEL_3，When 电流降至 -90A 持续 180s，Then 降为 LEVEL_1。
- Given DiscCur.ProtCnt > 3，When 电流降至 -90A 无论多长时间，Then 保护不自动恢复。

---

### REQ-PROT-013  二级放电过流保护（AFE 硬件检测，LEVEL_3，自锁次数限制）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1588-1615；`WarningTask.c:WarningRecover2()` 行 1730-1746；`ParaSet.h` 行 313-315 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 AFE 芯片检测到二级过流故障（`AppAfe_IsOcd2Fault() == 1`，默认触发阈值 100000 mA = 100A，无软件去抖），系统应立即置 `warn3code1[SecondOverCur]`，产生 SYS_FAULT（断所有接触器），并累计 `ScndDiscOverCur.ProtCnt`。
>
> 恢复条件：电流绝对值 ≤ 90000 mA（恢复阈值），距上次触发时间 > 10s（100 ticks），**且 `ScndDiscOverCur.ProtCnt ≤ SecondOverCurCnt`（3次）**，调用 `AppAfe_ClearOcd2Fault()` 清除 AFE 标志后恢复 LEVEL_0。超次则永久锁定。

**理由 / 代码依据**
> `Macro_SecondOverCur=100000`，`Macro_Rcv_SecondOverCur=90000`，`Macro_SecondOverCur_100ms=0`（无去抖，AFE 直接触发）；恢复等待 `(10*10)=100 ticks=10s`；`DEFAULT_SCND_OCTIMES=3`。

**验收准则**
- Given AFE OCD2 标志置位，Then warn3code1[SecondOverCur]=1 立即生效，所有接触器断开。
- Given ScndDiscOverCur.ProtCnt = 1，When 电流 < 90A 后等待 10s，Then AppAfe_ClearOcd2Fault() 调用成功，恢复 LEVEL_0。
- Given ScndDiscOverCur.ProtCnt = 4，When 电流降低，Then 故障不自动恢复（自锁）。

---

### REQ-PROT-014  短路保护（AFE 硬件检测，LEVEL_3，自锁次数限制）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1617-1635；`WarningTask.c:WarningRecover2()` 行 1748-1764；`ParaSet.h` 行 317-319 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 AFE 芯片检测到短路（`AppAfe_IsScFault() == 1`，默认阈值 120000 mA = 120A，无软件去抖），系统应立即置 `warn3code1[DiscCurShort]`，产生 SYS_FAULT（断所有接触器），累计 `ShortCur.ProtCnt`。
>
> 恢复条件：电流绝对值 ≤ 90000 mA，距上次触发时间 > 10s，且 `ShortCur.ProtCnt ≤ DiscCurShortCnt`（3次），调用 `AppAfe_ClearScFault()` 后恢复。超次则永久锁定。

**理由 / 代码依据**
> `Macro_DiscCurShort=120000`，`Macro_Rcv_DiscCurShort=90000`，`Macro_DiscCurShort_100ms=0`（无去抖）；`DEFAULT_SHORT_OCTIMES=3`。

**验收准则**
- Given AFE SC 标志置位，Then warn3code1[DiscCurShort]=1 立即生效，所有接触器断开。
- Given ShortCur.ProtCnt = 1，When 电流 < 90A 后等待 10s，Then 故障清除。
- Given ShortCur.ProtCnt = 4，Then 故障不自动恢复。

---

### REQ-PROT-015  MOS 温度过高告警与保护（LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 538-567；`ParaSet.h` 行 133-135 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 `Bms.MOSTemp`（MOS 管温度，0.1℃）持续超过阈值超过去抖时间：
> - 一级告警：≥ 1000（100.0℃），去抖 2000ms；
> - 三级保护：≥ 1100（110.0℃），去抖 2000ms，产生 SYS_FAULT（断所有接触器）、铁塔 LEVEL_4。
>
> 恢复：MOSTemp ≤ 900（90℃）持续 500ms（LEVEL_3 → LEVEL_1）；≤ 900 持续 200ms（LEVEL_1 → LEVEL_0）。

**理由 / 代码依据**
> `Macro_MosTempHigh_1=1000`，`_3=1100`；`Macro_Rcv_MosTempHigh_1/3=900`。

**验收准则**
- Given MOSTemp = 1100（110℃），When 保持 2000ms，Then SYS_FAULT 断所有接触器。
- Given MOSTemp 降至 900（90℃），When 保持 500ms，Then 降为 LEVEL_1。

---

### REQ-PROT-016  MOS 温升过快保护（第二类故障，LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1572-1586；`WarningTask.c:WarningRecover2()` 行 1781-1795；`ParaSet.h` 行 341-343 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 `Bms.MOSTempRise`（MOS 温升速率，0.1℃/某时间单位）≥ 700（70℃ 温升），去抖 3000ms 后，系统置 `warn3code1[MOSTempRise]`，产生 SYS_FAULT（铁塔 LEVEL_5）。
>
> 恢复：`MOSTempRise` ≤ 600 持续 `recover2_time.MOSTempRise`（需确认默认值）后恢复 LEVEL_0。

**理由 / 代码依据**
> `MACRO_MOSTEMPRISE=700`，`MACRO_Rcv_MOSTEMPRISE_ON=600`，`MACRO_MOSTEMPRISE_100ms=30（3s）`；`recover2_time.MOSTempRise` 未在 `SetDefaultPara()` 中设置默认值（存疑，见后文）。

**验收准则**
- Given MOSTempRise ≥ 700，When 保持 3000ms，Then warn3code1[MOSTempRise]=1，SYS_FAULT。
- Given MOSTempRise 降至 600，When 持续恢复延时，Then 故障清除。

---

### REQ-PROT-017  环境温度过高告警与保护（LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 505-536；`ParaSet.h` 行 137-139 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 `Bms.ENVTemp`（环境温度，0.1℃）持续超过阈值超过去抖时间：
> - 一级告警：≥ 550（55.0℃），去抖 2000ms；
> - 三级保护：≥ 600（60.0℃），去抖 2000ms，产生 SYS_FAULT（铁塔 LEVEL_4）。
>
> 恢复：ENVTemp ≤ 500（50℃）持续 500ms（LEVEL_3 → LEVEL_1）；≤ 500 持续 200ms（LEVEL_1 → LEVEL_0）。

**理由 / 代码依据**
> `Macro_EnvTempHigh_1=550`，`_3=600`；`Macro_Rcv_EnvTempHigh_1/3=500`。

**验收准则**
- Given ENVTemp = 600（60℃），When 保持 2000ms，Then SYS_FAULT。
- Given ENVTemp 降至 500，When 保持 500ms，Then 降为 LEVEL_1。

---

### REQ-PROT-018  环境温度过低告警与保护（放电时，LEVEL_1 / LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 600-633；`ParaSet.h` 行 141-143 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 仅在放电电流 `Bms.current_mA ≤ -300 mA` 时检测环境低温（以 `Bms.ENVTemp` 为判据）：
> - 一级告警：≤ -100（-10.0℃），去抖 2000ms，铁塔 LEVEL_1；
> - 三级保护：≤ -400（-40.0℃），去抖 2000ms，产生 SYS_FAULT（铁塔 LEVEL_4）。
>
> 恢复：ENVTemp ≥ 0（0℃）持续 500ms（LEVEL_3 → LEVEL_1）；≥ 0 持续 200ms（LEVEL_1 → LEVEL_0）。

**理由 / 代码依据**
> `Macro_EnvTempLow_1=-100`，`_3=-400`；`Macro_Rcv_EnvTempLow_1/3=0`；放电判断条件 `current_mA <= -300`。

**验收准则**
- Given 放电中，ENVTemp = -400（-40℃），When 保持 2000ms，Then SYS_FAULT。
- Given 静置，ENVTemp = -500，Then 不触发环境欠温保护。

---

### REQ-PROT-019  SOC 过低告警与保护（LEVEL_1 / LEVEL_3，直接映射 LEVEL_3 到放电故障）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 569-598；`WarningTask.c:CreatWaringCode()` 行 1411-1433；`ParaSet.h` 行 129-131 |
| 验证方法 | 测试 |
| 状态 | 已实现（存疑） |

**需求描述**
> 当 `Bms.Soc`（%）低于阈值持续去抖时间：
> - 一级告警：≤ 2%，去抖 2000ms（但 `warn1code0[SOCLow]` 置位代码已注释，**实际仅 LEVEL_3 对外通知**）；
> - 三级保护：≤ 0%，去抖 2000ms，置 `warn3code0[SOCLow]`，产生 DISC_FAULT（铁塔 LEVEL_3）。
>
> 恢复：SOC ≥ 1% 持续 500ms（LEVEL_3 → LEVEL_1）；SOC ≥ 1% 持续 200ms（LEVEL_1 → LEVEL_0）。

**理由 / 代码依据**
> `Macro_SOCLow_1=2`，`_3=0`；`Macro_Rcv_SOCLow_1/3=1`；`CreatWaringCode()` 中 SOCLow 的 LEVEL_1/2 case 已被注释，仅 LEVEL_3 有效，意味着 SOC=2% 的 LEVEL_1 计算但从不对外置码（存疑）。

**验收准则**
- Given SOC = 0%，When 保持 2000ms，Then warn3code0[SOCLow]=1，DISC_FAULT 断放电。
- Given SOC 升至 1%，When 保持 500ms，Then 告警清除。

---

### REQ-PROT-020  热失控检测（双条件触发，LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1707-1721 |
| 验证方法 | 测试 |
| 状态 | 已实现（存疑） |

**需求描述**
> 当同时满足以下 **两个** 条件时，系统应立即置 `warn3code1[ThermalRunaway]`，产生 SYS_FAULT：
> 1. `Bms.MaxTemp > 700`（即单体最高温 > 70.0℃）；
> 2. `Bms.ENVTemp > 1000`（即环境温 > 100.0℃）。
>
> 注：无去抖延时，无恢复逻辑（代码中未实现 `ThermalRunaway` 的恢复路径）。

**理由 / 代码依据**
> `thermal_runaway++` 计数达到 2 时置 LEVEL_3；无恢复代码，一旦触发为永久保护状态，直至复位（存疑）。

**验收准则**
- Given MaxTemp > 700 且 ENVTemp > 1000，Then warn3code1[ThermalRunaway]=1，SYS_FAULT。
- Given 仅 MaxTemp > 700 但 ENVTemp ≤ 1000，Then 不触发热失控。

---

### REQ-PROT-021  进水检测告警（ADC 低电平，LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1555-1569；`WarningTask.c:WarningRecover2()` 行 1797-1811；`ParaSet.h` 行 345-347 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 `Bms.WaterCheck`（水位检测 ADC 值）≤ 3800 持续 3000ms（30 × 100ms），系统应置 `warn3code1[WaterCheck]`，产生 SYS_FAULT（断所有接触器，铁塔 LEVEL_3）。
>
> 恢复：`Bms.WaterCheck ≥ 4000` 持续 `recover2_time.WaterCheck`（未设默认值，见存疑）后清除。

**理由 / 代码依据**
> `MACRO_WATERCHECK=3800`（触发阈值）；`MACRO_Rcv_WATERCHECK=4000`（恢复阈值）；`MACRO_WATERCHECK_100ms=30（3s）`；触发条件为 ADC 低于阈值（进水后 ADC 拉低）。

**验收准则**
- Given WaterCheck ADC ≤ 3800，When 保持 3000ms，Then SYS_FAULT 触发，所有接触器断开。
- Given WaterCheck ADC 升至 ≥ 4000，When 持续恢复延时，Then 故障清除。

---

### REQ-PROT-022  预充故障检测（LEVEL_3，30s 自动恢复）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:WarningRecover2()` 行 1767-1778；`WarningTask.c:CreatWaringCode2()` 行 1831-1841；`ParaSet.h` 行 310-311 |
| 验证方法 | 测试 + 检视 |
| 状态 | 已实现（存疑） |

**需求描述**
> 当系统检测到预充故障（由状态机在预充超时时置 `warn2_level.PreDiscF = LEVEL_3`），系统应置 `warn3code1[PreDiscF]`，产生 SYS_FAULT。
>
> 预充故障判定条件（来自 `Bms.Para.PreDiscVolt=2000mV`，`PreDiscTime=3s`，`ParaSet.c:630-631`）：预充结束后若系统侧电压仍 ≥ 2000mV（放电 MOS 两端电压差，表明预充未达目标），则视为预充故障。
>
> 恢复：预充故障触发后 30s（`system_run_time_100ms - PreDiscF_time > 300 ticks`）自动恢复 LEVEL_0（自动重试机制）。

**理由 / 代码依据**
> `Macro_PreErro_1=2000`（预充后系统侧剩余电压 mV），`Macro_PreErro_1_100ms=30（3s）`；恢复逻辑在 `WarningRecover2()` 行 1768-1778，30s 后自动清除（不依赖电气条件，为时间自动恢复）。预充故障的触发代码在 BMS_Control.c（非本文件，未在本域分析范围内）。

**验收准则**
- Given 预充故障标志被置位，Then warn3code1[PreDiscF]=1，SYS_FAULT 触发。
- Given PreDiscF 触发后等待 30s，Then warn3code1[PreDiscF] 自动清除，系统可重试预充。

---

### REQ-PROT-023  充电 MOS 短路失效检测（LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1636-1647 |
| 验证方法 | 测试 |
| 状态 | 已实现（存疑） |

**需求描述**
> 当满足以下所有条件持续 5000ms 时，系统应置 `warn3code1[ChgMOSShortF]`，报告充电 MOS 短路失效：
> 1. `AppAfe_IsChgMosCmdOn() == 0`（BMS 已发出断开充电 MOS 指令）；
> 2. `Bms.current_mA > 5000`（仍有 5A 以上充电电流）；
> 3. `Bms.lim_state == 0`（限流板已断开）。

**理由 / 代码依据**
> 条件：MOS 指令为断开但电流仍流通 → 短路失效；去抖 5000ms（50 ticks × 100ms = 5s）；`lim_state` 排除限流板未断的情况。

**验收准则**
- Given MOS 断开指令且电流 > 5A 且限流板断开，When 持续 5s，Then ChgMOSShortF 置位。
- Given 任一条件不满足，Then 故障不触发。

---

### REQ-PROT-024  充电 MOS 断路失效检测（LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1649-1669 |
| 验证方法 | 测试 |
| 状态 | 已实现（存疑） |

**需求描述**
> 当满足以下条件持续 5000ms 时，系统应置 `warn3code1[ChgMOSOpenF]`：
> 1. `Bms.Vmos_mv != 0` 且 `Bms.Vmos_mv < -3000`（MOS 两端电压差 < -3V，表明内外电压差反常）；
> 2. `AppAfe_IsChgMosOn() == 1`（充电 MOS 指令为导通）；
> 3. `Bms.current_mA < 1000`（充电电流 < 1A，MOS 已导通但无有效电流 → 断路失效）。

**理由 / 代码依据**
> `Vmos_mv < -3000` 表示系统侧电压比电池侧低 3V 以上，正常充电时 MOS 导通后电压差应极小；1000mA 门限区分有效充电与漏电。

**验收准则**
- Given ChgMOS 指令导通，Vmos < -3000mV 且电流 < 1A，When 持续 5s，Then ChgMOSOpenF 置位。

---

### REQ-PROT-025  放电 MOS 短路失效检测（LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1672-1683 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当满足以下所有条件持续 5000ms 时，系统应置 `warn3code1[DiscMOSShortF]`：
> 1. `AppAfe_IsDsgMosCmdOn() == 0`（已发断开放电 MOS 指令）；
> 2. `Bms.current_mA < -2000`（仍有 2A 以上放电电流）；
> 3. `AppAfe_IsPreDsgMosOn() == 0`（预充 MOS 也已断开）。

**理由 / 代码依据**
> 三个条件同时满足排除正常预充流路；-2000mA 门限识别有效放电。

**验收准则**
- Given DsgMOS 断开指令且预充 MOS 断开，当电流 < -2A 持续 5s，Then DiscMOSShortF 置位。

---

### REQ-PROT-026  放电 MOS 断路失效检测（LEVEL_3）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring2()` 行 1684-1703 |
| 验证方法 | 测试 |
| 状态 | 已实现（存疑） |

**需求描述**
> 当满足以下条件持续 1000ms 时，系统应置 `warn3code1[DiscMOSOpenF]`：
> 1. `Bms.Vmos_mv < 0` 且 `Bms.Vmos_mv > 1000`（条件矛盾，见存疑）；
> 2. `AppAfe_IsDsgMosOn() == 1`（放电 MOS 已导通）。

**理由 / 代码依据**
> 代码行 1688 的条件 `Bms.Vmos_mv > 1000` 在外层条件 `Vmos_mv < 0` 下永远为假（负数不可能大于 1000），导致 `DiscMOSOpenF` 故障实际**永远不会触发**。

**验收准则**
- 此需求因代码逻辑矛盾永远不触发（见存疑），验收结果预期为始终 LEVEL_0。

---

### REQ-PROT-027  铁塔通信告警等级映射（TIETA_LEVEL 0~5）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `WarningTask.c:TieTaWarnTask()` 行 2141-2247 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 系统应将内部告警映射为铁塔协议兼容的 6 级告警（TIETA_LEVEL_0~5），按优先级如下（高级覆盖低级）：
> - **LEVEL_5**（最高危）：MOS 温升过高、充/放/预充 MOS 失效、AFE 通信/失效、Flash 写入失效、外部通信故障、各 NTC 失效、短路保护、充电过流超次（ChgCur.ProtCnt > SecondOverCurCnt）。
> - **LEVEL_4**：压差大、单体过温、环境过高/低、AFE 电压线/温度线断开、放电欠温、MOS 过温。
> - **LEVEL_3**：充/放电过流(LEVEL_3)、总压/单体高低(LEVEL_3)、充电欠温、SOC 低、进水。
> - **LEVEL_2**：充/放电过流(LEVEL_1)。
> - **LEVEL_1**：总压/单体/温度类一级告警。
> - **LEVEL_0**：无故障。
>
> 同时统计所有有效告警总数 `tieta_warn_num`（含 warn1code0 和 warn3code0/1 的所有置位位数）。

**验收准则**
- Given 仅有单体过压 LEVEL_1，Then tieta_warn_level = TIETA_LEVEL_1，tieta_warn_num = 1。
- Given 短路 LEVEL_3 同时存在，Then tieta_warn_level = TIETA_LEVEL_5（优先级最高）。

---

### REQ-PROT-028  低 SOC/低单体电压 休眠触发（LEVEL_2 门限）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:needsleep()` 行 2073-2115 |
| 验证方法 | 测试 |
| 状态 | 已实现（存疑） |

**需求描述**
> 当 `warn_level.TotalVoltLow ≥ LEVEL_2` 或 `warn_level.CellVoltLow ≥ LEVEL_2` 持续 600s 时，系统应调用 `AppPower_EnterNormalSleep()` 进入休眠模式，以防止过深放电损坏电池。

**理由 / 代码依据**
> `(system_run_time_100ms - time) > 6000`（6000 × 100ms = 600s = 10分钟）；LEVEL_2 在当前 `CreatWaring()` 中**从未被设置**（只设 LEVEL_1 和 LEVEL_3），因此此保护**实际上可能永远不会触发**（存疑）。

**验收准则**
- Given TotalVoltLow = LEVEL_2（理论上），When 持续 600s，Then AppPower_EnterNormalSleep() 调用。

---

### REQ-PROT-029  充电限流功能（AppLimit 接口，软硬件协同）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/AppLimit.c`；`Application/AppLimit.h`；`Driver/BSP/bsp_limit.h`；`Application/BMS_Info.h` 行 75-78 |
| 验证方法 | 测试 |
| 状态 | 已实现（存疑） |

**需求描述**
> 系统应提供充电限流功能，通过 `AppLimit_On()` / `AppLimit_Off()` 控制限流板通断，通过 `AppLimit_EnableAdjust()` / `AppLimit_DisableAdjust()` 控制限流调节：
> - 限流使能触发阈值：`Bms.Para.chg_limiten_th`（默认 25000 mA = 25A）；
> - 限流恢复（关闭）阈值：`Bms.Para.chg_limiten_recover`（默认 15000 mA = 15A）；
> - 限流触发去抖时间：`Bms.Para.chg_limiten_time_th`（默认 30 × 100ms = 3s）；
> - 限流功能总开关：`Bms.Para.chg_limiten`（默认 1 = 使能）。
>
> 限流板状态通过 `Bms.lim_state` 反馈，用于 MOS 失效检测（REQ-PROT-023）。

**理由 / 代码依据**
> `SetDefaultPara()` 行 677-679；`AppLimit_*` 函数均透传至 `BspLimit_*`（BSP 层实现未在分析范围内）；`lim_state` 字段 `BMS_Info.h:225`。AppLimit 触发的具体调用逻辑在 BMS_Control.c（未详细分析）。

**验收准则**
- Given chg_limiten = 1，充电电流 ≥ 25A 持续 3s，Then AppLimit_On() 应被调用。
- Given 限流后电流降至 ≤ 15A，Then AppLimit_Off() 应被调用，lim_state 更新。

---

### REQ-PROT-030  告警参数持久化（Flash 保存与默认值回退）

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/ParaSet.c:SetDefaultPara()`, `CheckPara()`, `initParameter()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 系统应将所有告警阈值（`warn_threshold`）、恢复阈值（`warn_recover`）、去抖时间（`warn_time`）、第二类告警参数（`warn2_*`）及保护计数参数（`Bms.Para.SecondOverCurCnt` / `DiscCurShortCnt`）保存在 Flash 双备份区。启动时优先从主区读取，主区损坏时读备份区；两者均损坏时调用 `SetDefaultPara()` 恢复出厂参数并重新写入 Flash。

**理由 / 代码依据**
> `CheckPara()` 通过检查关键字段是否为 0xffffffff 或 0x00 判断参数有效性；`initParameter()` 实现双区读取-校验-回退逻辑；`SaveToFlash()` 写双备份。

**验收准则**
- Given Flash 主备区均损坏，Then SetDefaultPara() 被调用，系统使用 ParaSet.h 中定义的默认值运行。
- Given 正常读取成功，Then warn_threshold 与 Flash 中存储值一致。

---

### REQ-PROT-031  温差过高保护（代码已注释，当前未激活）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 459-503（注释代码） |
| 验证方法 | 检视 |
| 状态 | 缺口 |

**需求描述**
> 系统应检测单体间最大温差 `Bms.MaxDeltaTemp`；当温差超过阈值时，应按三级升级，达到 LEVEL_3 时产生 SYS_FAULT。但此功能代码已被注释，**当前版本不执行**。

**理由 / 代码依据**
> `WarningTask.c` 行 459-503 整段注释；`warn_level.DeltaTempTH` 在 `warn1code0/warn2code0/warn3code0` 的 switch 中保留但 `CreatWaring` 不设置，故始终为 LEVEL_0；铁塔告警映射也已注释 (`//WARN3CODE1_BIT(EnvTempLow)`)。

**验收准则**
- 当前版本：无论温差多大，DeltaTempTH 告警始终为 LEVEL_0（行为正确，但安全存疑）。

---

### REQ-PROT-032  放电过温保护（代码已注释，当前未激活）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:CreatWaring()` 行 281-327（注释代码） |
| 验证方法 | 检视 |
| 状态 | 缺口 |

**需求描述**
> 系统原设计单独检测放电过温（`warn_level.DiscTempHigh`），已被注释，当前统一由 `CellTempHigh`（REQ-PROT-008）覆盖，但通信字段 `warn1code0[DiscTempHigh]` 仍存在且可能被置位（`CreatWaringCode()` switch 仍保留该 case），实际值始终为 LEVEL_0（无触发路径）。

**理由 / 代码依据**
> `DiscTempHigh` 的检测逻辑（行 281-327）已全部注释；`IsBmsWarning()` 行 2056 检查 `DISC_FAULT` 的条件中已无 DiscTempHigh；通信层 switch case 保留但无意义。

**验收准则**
- 当前版本：放电过温不单独触发，由 CellTempHigh 统一代理。

---

## 存疑与观察

1. **DiscMOSOpenF 永远不触发**（REQ-PROT-026 存疑）：`CreatWaring2()` 行 1688 条件为 `(Bms.Vmos_mv < 0) && (Bms.Vmos_mv > 1000)`，负数无法大于 1000，逻辑矛盾导致放电 MOS 断路失效**从不上报**。疑似 bug，应为 `Bms.Vmos_mv > -1000`（绝对值 < 1V）。

2. **SOCLow LEVEL_1/2 对外无效**（REQ-PROT-019 存疑）：`CreatWaringCode()` 中 SOCLow 的 LEVEL_1 和 LEVEL_2 case 被注释，系统计算了 LEVEL_1 但从不将其映射到 warn1code0，外部看不到 SOC 预警，只能看到 LEVEL_3 保护。

3. **LEVEL_2 实际不可达**（REQ-PROT-001 存疑）：`CreatWaring()` 仅使用 `WARNING_LEVLE_1` 和 `WARNING_LEVLE_3`，代码从不将任何项设置为 LEVEL_2；`needsleep()` 检查 `≥ LEVEL_2` 的条件（REQ-PROT-028）在正常代码路径下永远为假——除非参数在 LEVEL_1 和 LEVEL_3 之间过渡时短暂经过 LEVEL_2，但恢复逻辑是 LEVEL_3 → LEVEL_1，不经过 LEVEL_2。

4. **recover2_time 无默认值**（REQ-PROT-016 / REQ-PROT-021 存疑）：`SetDefaultPara()` 中未对 `recover2_time.MOSTempRise` 和 `recover2_time.WaterCheck` 赋初始值，启动后若 Flash 中无有效数据，这两个字段为 0（`WarningRecover2()` 中 `VarRecover2Time` 初始值与当前时间相同，recover 条件首次检查 `0 > 0` 为假——实际会立即清除，等于无延时恢复）。

5. **热失控无恢复路径**（REQ-PROT-020 存疑）：`warn2_level.ThermalRunaway` 一旦置 LEVEL_3，`WarningRecover2()` 中无清除代码，系统须硬复位才能解除，但代码也没有对应的告警锁存和日志记录。需要确认是否为有意设计。

6. **放电过流恢复时间异常长**（REQ-PROT-012 观察）：`recover_time.DiscCurHigh_3 = Macro_Recover5s_100ms * 36 = 1800 ticks = 180s`（3分钟），远大于充电过流的 5s。这是有意的差异化设计还是 magic number 错误，需与设计者确认。

7. **充放电电流判断死区不对称**：充电激活阈值为 `current_mA >= 300`，放电激活阈值为 `current_mA <= -300`，与系统级充放电判断阈值 `CHGTHRESHmA=800 / DISCTHRESHmA=800` 不一致，可能在 300~800mA 微小电流时产生告警逻辑与状态机不一致的情况。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源文件 | 状态 |
|---|---|---|---|---|
| REQ-PROT-001 | 告警多级体系架构（Level 0/1/3） | ⚠️ | WarningTask.h | 已实现 |
| REQ-PROT-002 | 故障分类与接触器保护动作 | ⚠️ | WarningTask.c | 已实现 |
| REQ-PROT-003 | 总压过高告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-004 | 总压过低告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-005 | 单体电压过高告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-006 | 单体电压过低告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-007 | 单体压差过高（平台/非平台双阈值，L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-008 | 单体温度过高告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-009 | 充电欠温告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-010 | 放电欠温告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-011 | 充电过流告警与保护（L1/L3，自锁次数） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-012 | 放电过流告警与保护（L1/L3，180s 恢复） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-013 | 二级放电过流保护（AFE 检测，自锁） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-014 | 短路保护（AFE 检测，自锁） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-015 | MOS 温度过高告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-016 | MOS 温升过快保护（第二类，L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-017 | 环境温度过高告警与保护（L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-018 | 环境温度过低告警与保护（放电时，L1/L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-019 | SOC 过低告警与保护（L3，L1 映射缺失） | ⚠️ | WarningTask.c, ParaSet.h | 已实现（存疑）|
| REQ-PROT-020 | 热失控检测（双条件，L3，无恢复） | ⚠️ | WarningTask.c | 已实现（存疑）|
| REQ-PROT-021 | 进水检测告警（ADC 低电平，L3） | ⚠️ | WarningTask.c, ParaSet.h | 已实现 |
| REQ-PROT-022 | 预充故障检测（L3，30s 自动恢复） | ⚠️ | WarningTask.c, ParaSet.h | 已实现（存疑）|
| REQ-PROT-023 | 充电 MOS 短路失效检测（L3） | ⚠️ | WarningTask.c | 已实现（存疑）|
| REQ-PROT-024 | 充电 MOS 断路失效检测（L3） | ⚠️ | WarningTask.c | 已实现（存疑）|
| REQ-PROT-025 | 放电 MOS 短路失效检测（L3） | ⚠️ | WarningTask.c | 已实现 |
| REQ-PROT-026 | 放电 MOS 断路失效检测（L3，逻辑矛盾） | ⚠️ | WarningTask.c | 存疑 |
| REQ-PROT-027 | 铁塔通信告警等级映射（TIETA 0~5） | 否 | WarningTask.c | 已实现 |
| REQ-PROT-028 | 低 SOC/低单体电压休眠触发（L2 门限） | ⚠️ | WarningTask.c | 已实现（存疑）|
| REQ-PROT-029 | 充电限流功能（AppLimit 接口） | ⚠️ | AppLimit.c/h, bsp_limit.h | 已实现（存疑）|
| REQ-PROT-030 | 告警参数 Flash 持久化与默认值回退 | ⚠️ | ParaSet.c | 已实现 |
| REQ-PROT-031 | 温差过高保护（代码已注释，未激活） | ⚠️ | WarningTask.c | 缺口 |
| REQ-PROT-032 | 放电过温保护（代码已注释，由 CellTempHigh 代理） | ⚠️ | WarningTask.c | 缺口 |
