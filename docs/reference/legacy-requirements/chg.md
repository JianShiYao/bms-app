# REQ-CHG：充电管理 需求规格

> 覆盖源文件：`Application/charger.c`（已全注释，仅保留注释遗迹）、`Application/charger.h`（全注释）、`Driver/AFE/ChargerLoad.h`、`Application/BMS_Info.h`（`USER_PARA_S`）、`Application/AppAfe.c`（`AppAfe_HasCharger`、`AppAfe_LoadCheckTask`）、`Application/BMS_Control.c`（`BmsControl_RequestChgByParam`、`Limit_Task`）、`Application/SocTask.c`（`SocChargeFullConditionMet`、`SocUpdateWaitSelfConsumption`）、`Application/ParaSet.h`（默认参数宏）、`Application/ParaSet.c`（参数初始化）、`Application/UpperComTask.c`（参数读写）

---

## 需求列表

### REQ-CHG-001  充电器接入检测——硬件层标志

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Driver/AFE/ChargerLoad.h`：`LOADCHG_DELAY_CNT`、`CHGR_CHK_VOL`、`bChgerRStatusFlg`；`Application/AppAfe.c:AppAfe_HasCharger()` L63–66 |
| 验证方法 | 测试 |
| 状态 | 已实现（驱动层） |

**需求描述（EARS 句式）**
> 系统应通过 AFE 驱动层的 `LoadChgerChkProcess()` 检测充电器接入/移除状态，并将结果输出到标志位 `bChgerRStatusFlg`（充电器移除标志）；应用层通过 `AppAfe_HasCharger()` 读取 `HaveCharger` 变量获取当前充电器在线状态。充电器移除判据为外部电压低于 `CHGR_CHK_VOL = parameter.E2usChgRChkVol × 100`（mV），检测延时为 `LOADCHG_DELAY_CNT = parameter.E2ucLoadChgChkDelay`（单位由驱动层定义）。

**理由 / 代码依据**
> `ChargerLoad.h` 定义了充电器检测延时宏和移除电压阈值宏，均来自可配置参数 `parameter.E2usChgRChkVol` 和 `parameter.E2ucLoadChgChkDelay`。`AppAfe.c` 的 `HaveCharger` 静态变量目前在 `AppAfe_LoadCheckTask()` 中仅更新 `HaveLoad`，`HaveCharger` 赋值路径存疑（见存疑节）。

**验收准则（可度量）**
- Given 外部电压 < `E2usChgRChkVol × 100` mV 且超过 `E2ucLoadChgChkDelay` 检测周期 Then `bChgerRStatusFlg = TRUE`，`AppAfe_HasCharger()` 返回 0。
- Given 外部电压 ≥ 充电器移除阈值 Then `AppAfe_HasCharger()` 返回 1。

---

### REQ-CHG-002  充电 MOS 开合控制——BMS 主态机

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/BMS_Control.c:BmsControl_RequestChgByParam()` L311–317；`BmsControlTask()` L342–475 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在 BMS 处于 DISCONNET / STANDBY / CHARGING / DISCHARGING / PROTECT（仅充电故障时）等状态时，当 `Bms.Para.Waite_selfConsumption == FALSE`（未满充等待状态）时开启充电 MOS；当 `Waite_selfConsumption == TRUE` 且未处于放电中时，关闭充电 MOS 以停止充电。

**理由 / 代码依据**
> `BmsControl_RequestChgByParam()` 检查 `Waite_selfConsumption == FALSE` 后调用 `BmsMos_SetChg(1)`；若 `Waite_selfConsumption != FALSE` 且非放电态，则调用 `BmsMos_SetChg(0)`（见 L408–411）。充电限流状态下（`Bms.lim_state != 0`），`BmsMos_SetChg()` 忽略 enable=1 请求（L47–50）。

**验收准则（可度量）**
- Given `Waite_selfConsumption=FALSE` 且无故障 When 进入 STANDBY When Then 充电 MOS 开通，`Bms.sta` 随电流检测变为 CHARGING。
- Given `Waite_selfConsumption=TRUE` 且非放电态 Then 充电 MOS 断开，无充电电流。

---

### REQ-CHG-003  满充判定——三条件 AND 逻辑 ⚠️

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/SocTask.c:SocChargeFullConditionMet()` L614–625；`Application/ParaSet.h` L154–157 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在充电过程中，当同时满足以下全部条件时，判定为满充并将 `Waite_selfConsumption` 置为 TRUE（停止充电）：
> 1. **电压条件（OR）**：最高单体电压 ≥ `fullcellvoltmV`（默认 3600 mV），**或**平均单体电压 ≥ `fullcellvoltmV`，**或**总电压 ≥ `fullTotalvoltmV`（默认 56500 mV）；
> 2. **电流条件（AND）**：充电电流 ≤ `fullCurmA`（默认 5000 mA，即约 0.1C）。

**理由 / 代码依据**
> `SocChargeFullConditionMet()` 实现 `((CellMaxVoltage_mV >= fullcellvoltmV) || (AverageVolt_mV >= fullcellvoltmV) || (intvolt_mV >= fullTotalvoltmV)) && (current_mA <= fullCurmA)`。默认值：`Cell_FULL_VOLT_mV = 3600`，`Pack_FULL_VOLT_mV = 56500`，`ThdFullChargeCurrent = 5000`（`ParaSet.h` L155–157）。`SocUpdateWaitSelfConsumption()` 在 SOC >= 100% 后将 `Waite_selfConsumption = TRUE`，触发 REQ-CHG-002 停充。

**验收准则（可度量）**
- Given 充电中 When CellMaxVolt=3600 mV 且 current=4900 mA Then 满充条件成立，SOC 推算至 100%，`Waite_selfConsumption = TRUE`，充电 MOS 断开。
- Given 充电中 When CellMaxVolt=3600 mV 且 current=5100 mA Then 满充条件不成立，继续充电。
- Given 充电中 When TotalVolt=56500 mV 且 current=4000 mA Then 满充条件成立（通过总压路径）。

---

### REQ-CHG-004  间歇充电——SOC 回差控制

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/SocTask.c:SocUpdateWaitSelfConsumption()` L970–991；`Application/BMS_Info.h:USER_PARA_S`：`IntermitterChgThd`、`Waite_selfConsumption`；`Application/ParaSet.h` L327–328 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应实现间歇充电策略：当 SOC ≥ 100% 时置 `Waite_selfConsumption = TRUE`（停止充电）；当 SOC 因自耗电降至 ≤ `IntermitterChgThd`（默认 95%）时，置 `Waite_selfConsumption = FALSE`（重新允许充电）；两个状态形成 5% 的 SOC 回差控制窗口（100% 停充 → 95% 再充）。

**理由 / 代码依据**
> `SocUpdateWaitSelfConsumption()` L983–990：`if (soc_ref >= 100) Waite_selfConsumption = TRUE`；`if (soc_ref <= IntermitterChgThd) Waite_selfConsumption = FALSE`。默认阈值 `DEFAULT_INTERMITTERCHGTHD = 95`（`ParaSet.h` L328）。自耗电速率 `SelfConsumption = 30`（即每日 3%，`DEFAULT_SELFCONSUMPTION=30`，1 单位 = 0.1%/天）。

**验收准则（可度量）**
- Given SOC=100% When 调用 `SocUpdateWaitSelfConsumption` Then `Waite_selfConsumption=TRUE`，充电 MOS 断开。
- Given `Waite_selfConsumption=TRUE` 且 SOC=95% When 调用 Then `Waite_selfConsumption=FALSE`，充电 MOS 重新开通。
- Given `IntermitterChgThd` 配置为 90 Then 回差窗口为 90–100%。

---

### REQ-CHG-005  充电限流功能——电流阈值触发与恢复

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/BMS_Control.c:Limit_Task()` L573–665；`Application/BMS_Info.h:USER_PARA_S`：`chg_limiten`、`chg_limiten_th`、`chg_limiten_recover`、`chg_limiten_time_th`；`Application/ParaSet.c` L677–679 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当充电限流功能使能（`chg_limiten == 1`）且未处于满充等待状态时，系统应在充电电流持续超过 `chg_limiten_th`（默认 25000 mA）且持续时间超过 `chg_limiten_time_th`（默认 30 × 100 ms = 3 s）后，激活限流（`LimitOn()`），延迟 3000 ms 后断开充电 MOS；当充电电流降至 `chg_limiten_recover`（默认 15000 mA）以下并持续 ≥ 20 × 100 ms = 2 s 后，关闭限流（`LimitOff()`），重新允许开通充电 MOS。

**理由 / 代码依据**
> `Limit_Task()` 使用 `static limit_reason_t` 跟踪触发原因；限流触发：`(system_run_time_100ms - overtime) > chg_limiten_time_th` 后调 `LimitOn()`，再 `delay_ms(3000)` 后 `BmsMos_SetChg(0)`；恢复：`current_mA < chg_limiten_recover` 且 `system_run_time_100ms - starttime > 20`（即 2 s），然后 `delay_ms(2000)`、`LimitOff()`。默认值来自 `ParaSet.c` L677–679。

**验收准则（可度量）**
- Given `chg_limiten=1` When 充电电流 = 26000 mA 持续 3.1 s Then `lim_state=1`，充电 MOS 断开（延迟 3 s 内）。
- Given `lim_state=1` When 充电电流 = 14000 mA 持续 2.1 s Then `lim_state=0`，充电 MOS 可重新开通。
- Given `chg_limiten=0` Then `lim_state` 保持 0，无限流干预。

---

### REQ-CHG-006  充电限流与满充联动

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/BMS_Control.c:Limit_Task()` L578；`BmsMos_SetChg()` L41–53 |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在满充等待状态（`Waite_selfConsumption == TRUE`）期间禁止激活限流（`Limit_Task()` 直接跳过触发逻辑），并在充电故障（`CHG_FAULT | SYS_FAULT`）时立即关闭限流（`LimitOff()`）；在限流激活状态（`lim_state != 0`）期间，充电 MOS 开通请求将被忽略。

**理由 / 代码依据**
> `Limit_Task()` 外层条件 `if(Waite_selfConsumption == FALSE)` 在满充时跳过整个限流触发逻辑（L578）；`BmsControlTask()` L464–468 在 CHG_FAULT 或 SYS_FAULT 时调 `LimitOff()`；`BmsMos_SetChg()` L47–50 在 `lim_state != 0` 时 return 不处理 enable=1。

**验收准则（可度量）**
- Given `Waite_selfConsumption=TRUE` 且电流 = 30000 mA Then `lim_state` 保持 0，不触发限流。
- Given `lim_state=1` 且发生 CHG_FAULT Then `LimitOff()` 被调用，`lim_state=0`。

---

### REQ-CHG-007  充电匹配功能——参数使能位（存疑：逻辑实现缺失）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/BMS_Info.h:USER_PARA_S.chgmatch_en` L74；`Application/UpperComTask.c` L162、L484 |
| 验证方法 | 检视 |
| 状态 | 存疑 |

**需求描述（EARS 句式）**
> 系统应（预期）在充电匹配功能使能（`chgmatch_en == 1`）时，拒绝不匹配的充电器接入充电；在 `chgmatch_en == 0` 时允许任意充电器充电。

**理由 / 代码依据**
> `chgmatch_en` 字段在 `USER_PARA_S` 中定义，通过上位机通信（`UpperComTask.c` L162、L484）可读写，但在整个代码库中未找到任何**使用** `chgmatch_en` 值来判断是否允许充电的逻辑分支。`charger.c` 中原有的 Modbus 充电器通信代码已被全部注释。

**验收准则（可度量）**
- 待充电匹配逻辑实现后，Given `chgmatch_en=1` 且接入额定电压不匹配的充电器 Then 充电 MOS 不开通。
- 当前代码无法验证此行为，属功能缺口。

---

### REQ-CHG-008  充电电流方向与状态判定

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `Application/BMS_Info.c:BmsinfoStatistics()` L63–71；`Application/BMS_Info.h:USER_PARA_S.chgthdmA/dscthdmA`；`Application/ParaSet.c` L407–408 |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应以电流正方向为充电，当 `current_mA >= chgthdmA` 时，`CurrentState = CURRENT_CHG`；当 `current_mA <= -dscthdmA` 时，`CurrentState = CURRENT_DISC`；否则为 CURRENT_FLOAT 或 CURRENT_NONE。`chgthdmA` 和 `dscthdmA` 默认值来自宏 `CHGTHRESHmA` 和 `DISCTHRESHmA`（具体数值在 `ParaSet.h` 中定义）。

**理由 / 代码依据**
> `BmsinfoStatistics()` 使用 `chgthdmA` 作为充电电流死区阈值，避免小电流被误判为充放电状态。充电状态的判定直接驱动 `BMS_WORK_STA_S`（via `BmsControl_UpdateRunState()`），并影响 LED SOC 灯的闪烁行为（REQ-LED-003）。

**验收准则（可度量）**
- Given `chgthdmA=300` When `current_mA=350` Then `CurrentState=CURRENT_CHG`。
- Given When `current_mA=100` Then `CurrentState=CURRENT_FLOAT` 或 `CURRENT_NONE`（非充非放）。

---

### REQ-CHG-009  充电保护触发——充电 MOS 硬断

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Application/WarningTask.c:IsBmsWarning()` L2019–2035；`Application/BMS_Control.c:DealFault()` L477–494 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当以下任一充电故障（CHG_FAULT）条件触发 3 级告警时，系统应立即断开充电 MOS（`BmsMos_SetChg(0)`）：总压过高（warn3code0.TotalVoltHigh）、单体过高（warn3code0.CellVoltHigh）、充电低温（warn3code0.ChgTempLow）、充电过流（warn3code0.ChgCurHigh）。系统故障（SYS_FAULT）时同时断开所有 MOS。

**理由 / 代码依据**
> `IsBmsWarning()` L2019–2035 将上述 4 项三级告警汇总为 `CHG_FAULT`，`DealFault()` 在 `CHG_FAULT` 时调 `BmsMos_SetChg(0)`（L491–493）。放电状态下屏蔽 CHG_FAULT（L2061–2063），避免充放电同时运行时的误保护。

**验收准则（可度量）**
- Given `warn3code0.TotalVoltHigh=1` Then `IsBmsWarning()` 返回 `CHG_FAULT`，充电 MOS 断开。
- Given `Bms.sta=BMS_STA_DISCHARGING` 且 `warn3code0.CellVoltHigh=1` Then `CHG_FAULT` 被屏蔽，充电 MOS 不受影响。

---

## 存疑与观察

1. **充电匹配（`chgmatch_en`）逻辑实现完全缺失**（REQ-CHG-007）：`charger.c` 中的 Modbus 充电器通信代码全部注释，`chgmatch_en` 仅在参数存取中使用，无任何业务逻辑消费此字段。这是涉及安全的重要功能缺口，需明确是"未实现"还是"通过其他机制实现"。`状态=存疑`。

2. **`Limit_Task()` 中存在阻塞 `delay_ms(3000/2000)` 调用**：`BMS_Control.c` L591、L621、L639 中调用 `delay_ms(3000)` 和 `delay_ms(2000)` 阻塞主循环，期间所有任务（含告警检测、LED 更新、SOC 计算）全部暂停，影响系统实时性。这与 RTOS/裸机轮询架构不符，`状态=存疑`，建议改为非阻塞计时。

3. **`HaveCharger` 变量赋值路径缺失**：`AppAfe.c` L55–66 定义了 `static uint8_t HaveCharger = 0` 和 `AppAfe_HasCharger()` 接口，但全局搜索未发现任何赋值 `HaveCharger` 的代码（`HaveLoad` 在 `AppAfe_LoadCheckTask()` 中有更新）。`AppAfe_HasCharger()` 始终返回 0，充电器检测功能实际未生效。`状态=存疑`。

4. **`fullCurmA` 单位与符号约定**：`ThdFullChargeCurrent = 5000` mA（充电为正），但满充判定 `current_mA <= fullCurmA` 表示充电电流"降到"5 A 以下才算满充，符合恒流→恒压的满充逻辑。需注意充电电流始终为正值，此条件在静置或放电时也满足，应确保满充判定只在充电状态下调用（`SocHandleCharging()` 已做状态限制）。

5. **`chg_limiten_time_th` 的单位**：`ParaSet.c` L679 赋值 `30`，在 `Limit_Task()` 中直接与 `system_run_time_100ms` 比较（单位 100 ms），因此 30 代表 3 s 延迟，但变量名后缀无 `_100ms` 提示，存在可读性隐患。`状态=存疑`，建议在注释中注明单位。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-CHG-001 | 充电器接入检测——硬件层标志与阈值 | 是 ⚠️ | `ChargerLoad.h`、`AppAfe.c` | 已实现（驱动层） |
| REQ-CHG-002 | 充电 MOS 开合控制——BMS 主态机 | 是 ⚠️ | `BMS_Control.c:BmsControl_RequestChgByParam()` | 已实现 |
| REQ-CHG-003 | 满充判定——三条件 AND 逻辑 | 是 ⚠️ | `SocTask.c:SocChargeFullConditionMet()`、`ParaSet.h` | 已实现 |
| REQ-CHG-004 | 间歇充电——SOC 回差控制（100% 停充 / 95% 再充） | 是 ⚠️ | `SocTask.c:SocUpdateWaitSelfConsumption()`、`ParaSet.h` | 已实现 |
| REQ-CHG-005 | 充电限流——电流阈值触发与恢复 | 是 ⚠️ | `BMS_Control.c:Limit_Task()`、`ParaSet.c` | 已实现 |
| REQ-CHG-006 | 充电限流与满充联动 | 是 ⚠️ | `BMS_Control.c:Limit_Task()`、`BmsMos_SetChg()` | 已实现 |
| REQ-CHG-007 | 充电匹配（chgmatch_en）——逻辑实现缺失 | 是 ⚠️ | `BMS_Info.h`、`charger.c`（全注释） | 存疑 |
| REQ-CHG-008 | 充电电流方向与状态判定 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-CHG-009 | 充电保护触发——充电 MOS 硬断 | 是 ⚠️ | `WarningTask.c:IsBmsWarning()`、`BMS_Control.c:DealFault()` | 已实现 |
