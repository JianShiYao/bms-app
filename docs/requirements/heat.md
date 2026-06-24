# REQ-HEAT：加热控制 需求规格

> 覆盖源文件：
> - `Application/heatcontrol.c`、`Application/heatcontrol.h`（加热主控逻辑：`HeatControlInit()`、`HeatControlTask()`）
> - `Application/AppHeat.c`、`Application/AppHeat.h`（应用层 PWM/输出接口）
> - `Driver/BSP/bsp_heat.h`（BSP 层接口声明）
> - `Application/BMS_Info.h`（`USER_PARA_S` 加热参数字段、`WARN3CODE0_BIT` 宏、`PTCTemp`）
> - `Application/ParaSet.h`（默认值宏 `MACRO_HEATMODE` 等）
> - `Application/ParaSet.c`（参数初始化）
> - `Application/UpperComTask.h`（`MAX_PCS_POLLING_TIME`、`UpperCMD_TypeDef.HeatCtrl`）
> - `Application/UpperComTask.c`（PCS 指令解析）

---

## 需求列表

---

### REQ-HEAT-001  加热模式选择（被动 / 主动）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 83–148；`BMS_Info.h:USER_PARA_S.HeatMode`；`ParaSet.h:MACRO_HEATMODE` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应始终根据参数 `HeatMode`（0 = 被动加热 `PASSIVE_HEAT`，1 = 主动加热 `ACTIVE_HEAT`）选择对应的加热控制策略；若 `HeatMode` 为其他值，则系统应禁止加热输出。

**理由 / 代码依据**
> `HeatControlTask()` 以 `Bms.Para.HeatMode` 为分支条件，分别执行被动加热逻辑（依赖 PCS 外部指令）和主动加热逻辑（BMS 自主决策）；默认值 `MACRO_HEATMODE = 1`（主动）。

**验收准则（可度量）**
- Given `HeatMode = 0`，When PCS 无有效指令（超时），Then 加热输出关闭（`heat_flag = 0`）。
- Given `HeatMode = 1`，When 满足主动加热触发条件，Then 加热自主开启，无需 PCS 指令。
- Given `HeatMode = 2`（非法值），Then `heat_flag` 恒为 0，`AppHeat_SetOutput(0)` 被调用。

---

### REQ-HEAT-002  被动加热模式——PCS 指令有效性判断

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 85；`UpperComTask.h` 行 148 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `HeatMode = PASSIVE_HEAT`（被动）时，系统应仅在满足以下条件时响应加热指令：距上次收到 PCS 指令的时间差 `(u32SysTime - dcdcCmd.RevTimems) < MAX_PCS_POLLING_TIME`（60,000 ms = 60 s）；若超时，系统应强制关闭加热输出。

**理由 / 代码依据**
> `MAX_PCS_POLLING_TIME = 1*1*60*1000UL`（ms），定义于 `UpperComTask.h` 行 148，注释为"加热指令超时"。超时后 `heat_flag = 0`，防止因 PCS 通信中断导致加热失控。

**验收准则（可度量）**
- Given 被动模式，When PCS 指令到达后 59 s 内，Then 加热可根据温度条件正常控制。
- Given 被动模式，When 距上次 PCS 指令 ≥ 60 s，Then `heat_flag = 0`，加热立即关闭。

---

### REQ-HEAT-003  被动充电加热——开启与关闭阈值

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 87–98；`BMS_Info.h:USER_PARA_S.PassiveChgHeatOff`；`ParaSet.h:MACRO_PASSIVE_ChgHEAT_OFF` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `HeatMode = PASSIVE_HEAT` 且 PCS 发出充电加热请求（`PsssiveChgHeatReq ≠ 0`）且 PCS 指令未超时时：
> - 若**全部** `TOTAL_AFE_TEMP_NUM`（4）个温度点均 `> PassiveChgHeatOff`，**或**充电过温保护告警 `WARN3CODE0_BIT(ChgTempHigh)` 置位，系统应关闭加热（`heat_flag = 0`）；
> - 若存在**至少 1** 个温度点 `< (PassiveChgHeatOff - 10)`（即滞环下限），系统应开启加热（`heat_flag = 1`）。
>
> 默认参数：`PassiveChgHeatOff = 150`（单位 0.1℃，即 15.0℃）；开启滞环 = 150 - 10 = 140（14.0℃）。

**理由 / 代码依据**
> 充电场景下 PCS 请求加热（`PsssiveChgHeatReq`），BMS 用 `PassiveChgHeatOff` 作关断点，`PassiveChgHeatOff - 10` 作开启点（滞环 1.0℃），防止频繁开关。

**验收准则（可度量）**
- Given 被动充电加热、PCS 未超时，When 4 路温度全部 > 150（15.0℃）**或** `ChgTempHigh` 保护，Then 加热输出关闭。
- Given 同上，When 至少 1 路温度 < 140（14.0℃），Then 加热输出开启。
- Given `ChgTempHigh`（三级告警）置位时，Then 加热立即关断，不受温度滞环影响。

---

### REQ-HEAT-004  被动放电加热——开启与关闭阈值

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 100–113；`BMS_Info.h:USER_PARA_S.PassiveDiscHeatOff`；`ParaSet.h:MACRO_PASSIVE_DiscHEAT_OFF` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `HeatMode = PASSIVE_HEAT` 且 PCS 发出放电加热请求（`PsssiveDscHeatReq ≠ 0`）且 PCS 指令未超时时：
> - 若**全部** 4 个温度点均 `> PassiveDiscHeatOff`，**或**放电过温保护 `WARN3CODE0_BIT(DiscTempHigh)` 置位，系统应关闭加热；
> - 若存在**至少 1** 个温度点 `< (PassiveDiscHeatOff - 10)`，系统应开启加热。
>
> 默认参数：`PassiveDiscHeatOff = -150`（0.1℃，即 -15.0℃）；开启滞环 = -150 - 10 = -160（-16.0℃）。

**理由 / 代码依据**
> 放电场景 PCS 请求加热，阈值与充电场景独立，允许不同策略。默认 -15℃ 远低于充电默认 +15℃，体现放电低温容忍度更高。

**验收准则（可度量）**
- Given 被动放电加热、PCS 未超时，When 4 路温度全 > -150（-15.0℃）**或** `DiscTempHigh` 三级告警，Then 加热关断。
- Given 同上，When 至少 1 路温度 < -160（-16.0℃），Then 加热开启。

---

### REQ-HEAT-005  被动模式——PCS 无加热请求时禁止加热

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 113–116 |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `HeatMode = PASSIVE_HEAT` 且 PCS 未发出充电或放电加热请求（`PsssiveChgHeatReq = 0` 且 `PsssiveDscHeatReq = 0`）时，系统应保持加热关闭（`heat_flag = 0`）。

**理由 / 代码依据**
> `else` 分支在 `PsssiveChgHeatReq` 和 `PsssiveDscHeatReq` 均为 0 时直接设 `heat_flag = 0`，无需额外条件。

**验收准则（可度量）**
- Given 被动模式、PCS 未下发任何加热请求，Then 加热输出始终为关闭状态。

---

### REQ-HEAT-006  主动加热模式——自主开启条件

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 129–135；`BMS_Info.h:USER_PARA_S.ActiveHeatOn`；`ParaSet.h:MACRO_ACTIVE_HEAT_ON` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `HeatMode = ACTIVE_HEAT` 时，若满足以下**全部**条件，系统应自主开启加热（`heat_flag = 1`）：
> 1. 存在**至少 1** 个温度点 `< ActiveHeatOn`；
> 2. 当前电流状态 `Bms.CurrentState ≠ CURRENT_DISC`（即非放电状态）。
>
> 默认参数：`ActiveHeatOn = 60`（0.1℃，即 6.0℃）。

**理由 / 代码依据**
> 主动模式下 BMS 自主决策，无需 PCS 指令。注意原代码中 `&&(HaveCharger)` 条件被注释掉，意味着即使无充电机接入，只要非放电状态就允许加热。此为存疑点（见存疑 Q1）。

**验收准则（可度量）**
- Given 主动模式、至少 1 路温度 < 60（6.0℃）、电池非放电状态，Then 加热输出开启。
- Given 主动模式、所有温度 ≥ 60（6.0℃），Then 不开启加热（不满足开启条件）。

---

### REQ-HEAT-007  主动加热模式——自主关闭条件

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 136–143；`BMS_Info.h:USER_PARA_S.ActiveHeatOff`；`ParaSet.h:MACRO_ACTIVE_HEAT_OFF` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `HeatMode = ACTIVE_HEAT` 时，若满足以下**任一**条件，系统应关闭加热（`heat_flag = 0`）：
> 1. 充电过温保护告警 `WARN3CODE0_BIT(ChgTempHigh)` 置位；
> 2. 存在**至少 2** 个温度点 `> ActiveHeatOff`；
> 3. 当前电流状态 `Bms.CurrentState == CURRENT_DISC`（放电中）。
>
> 默认参数：`ActiveHeatOff = 150`（0.1℃，即 15.0℃）。

**理由 / 代码依据**
> 关闭条件比开启条件（>= 1 点低温）更严格（>= 2 点高温才关），形成滞环以避免振荡。放电时强制关闭加热，防止加热与大电流放电叠加风险。注意 `HaveCharger == 0` 的关闭条件也被注释掉。

**验收准则（可度量）**
- Given 主动加热中，When `ChgTempHigh` 三级告警，Then 立即关断加热。
- Given 主动加热中，When ≥ 2 路温度 > 150（15.0℃），Then 关断加热。
- Given 主动加热中，When `CurrentState = CURRENT_DISC`（放电），Then 关断加热。

---

### REQ-HEAT-008  主动模式——放电时禁止加热（互锁）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 131、139 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 在主动加热模式下，当系统进入放电状态（`CurrentState == CURRENT_DISC`）时，系统应立即禁止加热开启（不满足开启条件），且若加热已开启应立即关闭。

**理由 / 代码依据**
> 开启条件中 `&&(Bms.CurrentState != CURRENT_DISC)` 和关闭条件中 `||(Bms.CurrentState == CURRENT_DISC)` 形成双重互锁，确保放电期间加热功率不与放电电流叠加。

**验收准则（可度量）**
- Given 主动加热正在运行，When `CurrentState` 切换为 `CURRENT_DISC`，Then `heat_flag = 0`，`AppHeat_SetOutput(0)` 在本次任务周期内调用。

---

### REQ-HEAT-009  PTC 温度反馈——PWM 比较值自动调节

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 72–79；`BMS_Info.h` 行 244 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应在每个 `HeatControlTask()` 调用周期内，根据加热元件 PTC 温度 `Bms.PTCTemp`（单位 0.1℃）调整 PWM 比较值：
> - 若 `PTCTemp ≤ 450`（45.0℃），则调用 `AppHeat_SetCompare(9999)`（最大占空比，全功率加热）；
> - 若 `PTCTemp ≥ 550`（55.0℃），则调用 `AppHeat_SetCompare(0)`（零占空比，停止加热功率输出）。

**理由 / 代码依据**
> `Bms.PTCTemp` 来自 `BmsAdc.c:Bms.PTCTemp = adc->ptc_temp`，由 ADC 采样 PTC 热敏电阻温度。此逻辑独立于 `heat_flag`，无论加热是否被允许均会执行，用于防止 PTC 过热。

**验收准则（可度量）**
- Given `PTCTemp = 400`（40.0℃），Then `SetCompare(9999)` 调用（全功率）。
- Given `PTCTemp = 600`（60.0℃），Then `SetCompare(0)` 调用（零功率）。
- Given `PTCTemp = 500`（50.0℃，介于 450~550 之间），Then 本次 `SetCompare` 不调用（保持上次设置）。

---

### REQ-HEAT-010  加热输出状态变化时同步更新 heat_state

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 150–166；`BMS_Info.h` 行 226 |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当 `heat_flag` 发生变化时，系统应同步更新 `Bms.heat_state`（1 = 加热中，0 = 停止）并调用 `AppHeat_SetOutput(heat_flag)` 驱动硬件输出。

**理由 / 代码依据**
> `heat_flag != old_heat_flag` 触发状态机更新，避免每个周期重复写硬件。`Bms.heat_state` 供上位机通信和日志使用。

**验收准则（可度量）**
- Given `heat_flag` 从 0 变 1，Then `Bms.heat_state = 1` 且 `AppHeat_SetOutput(1)` 被调用。
- Given `heat_flag` 从 1 变 0，Then `Bms.heat_state = 0` 且 `AppHeat_SetOutput(0)` 被调用。
- Given `heat_flag` 未变化，Then 本周期内 `AppHeat_SetOutput` 不被调用。

---

### REQ-HEAT-011  加热初始化

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `heatcontrol.c:HeatControlInit()` 行 47–50；`AppHeat.c:AppHeat_Init()`；`bsp_heat.h:BspHeat_Init()` |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统在上电初始化阶段应调用 `HeatControlInit()` 完成加热硬件（PWM 定时器 TIM3_CH2）的初始化配置。

**理由 / 代码依据**
> 调用链：`HeatControlInit()` → `AppHeat_Init()` → `BspHeat_Init()`，三层抽象。BSP 实现文件未在源镜像中提供（仅有 `.h`），具体 TIM3 配置参数不可见。

**验收准则（可度量）**
- Given 系统上电，When `HeatControlInit()` 被调用，Then 后续 `AppHeat_SetOutput()`/`AppHeat_SetPwm()` 调用不产生 HardFault。

---

### REQ-HEAT-012  加热 PWM 频率与占空比可配置接口

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `heatcontrol.c` 行 52–65；`AppHeat.h`；`bsp_heat.h` |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应提供以下加热 PWM 控制接口：
> - `TIM3_CH2_PWM_Set(float freq_hz, float duty_percent)`：同时设置频率（Hz）和占空比（%）；
> - `TIM3_CH2_SetDuty(float duty_percent)`：仅更新占空比；
> - `TIM3_CH2_PWM_OutputEnable(uint8_t enable)`：使能或禁止 PWM 输出；
> - `AppHeat_SetCompare(uint32_t compare)`：直接设置 TIM3 比较寄存器值。

**理由 / 代码依据**
> 这些接口封装了 BSP 层，供上层逻辑灵活控制加热功率。`HeatControlTask()` 中实际只使用 `AppHeat_SetOutput()` 和 `AppHeat_SetCompare()`，`SetPwm`/`SetDuty` 接口在当前任务中未被直接调用（可能供测试/调试用）。

**验收准则（可度量）**
- Given 调用 `AppHeat_SetCompare(9999)`，Then TIM3_CH2 输出最大占空比（通过示波器验证）。
- Given 调用 `AppHeat_SetCompare(0)`，Then TIM3_CH2 输出占空比为 0。

---

### REQ-HEAT-013  过温保护互锁——充电温度三级告警时强制关热

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 90、136；`BMS_Info.h` 行 18（`WARN3CODE0_BIT` 宏） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 当充电温度三级保护告警（`Bms.warn3code0` 的 `ChgTempHigh` 位）置位时，无论当前处于何种加热模式（被动充电加热或主动加热），系统应立即强制关闭加热输出。

**理由 / 代码依据**
> `WARN3CODE0_BIT(ChgTempHigh)` 是三级（最高级）充电过温告警，在被动充电加热（行 90）和主动加热（行 136）的关断条件中均有检查，形成跨模式安全互锁。

**验收准则（可度量）**
- Given 加热正在运行（任意模式），When `warn3code0[ChgTempHigh]` 置 1，Then 当前 `HeatControlTask()` 周期内 `heat_flag = 0`，加热关断。

---

### REQ-HEAT-014  过温保护互锁——放电温度三级告警时强制关热（被动模式）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `heatcontrol.c:HeatControlTask()` 行 103 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 在被动放电加热模式下，当放电温度三级告警 `WARN3CODE0_BIT(DiscTempHigh)` 置位时，系统应立即关闭加热。

**理由 / 代码依据**
> 对应被动模式放电分支的关断条件（行 103）。注意主动加热模式关断条件（行 136）中**未检查** `DiscTempHigh`，仅检查 `ChgTempHigh`（见存疑 Q2）。

**验收准则（可度量）**
- Given 被动放电加热进行中，When `warn3code0[DiscTempHigh]` 置 1，Then 加热立即关断。

---

### REQ-HEAT-015  温度点统计辅助函数

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `heatcontrol.c` 行 175–199；`heatcontrol.h` 行 15–16 |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述（EARS 句式）**
> 系统应提供两个辅助函数：
> - `OverTempPointCnt(int16_t temp)`：返回 `ActiveCellTemp[]` 中大于阈值 `temp` 的温度点数量；
> - `UnderTempPointCnt(int16_t temp)`：返回小于阈值 `temp` 的温度点数量。
>
> 遍历范围为 `i = 0` 到 `TOTAL_AFE_TEMP_NUM - 1`（即 4 个温度通道）。

**理由 / 代码依据**
> 这两个函数是所有加热阈值判断的基础，温度单位与 `ActiveCellTemp[]` 一致（0.1℃）。

**验收准则（可度量）**
- Given `ActiveCellTemp = {100, 200, 300, 400}`，When `OverTempPointCnt(250)` 调用，Then 返回 2（300、400 超过 250）。
- Given 同上，When `UnderTempPointCnt(250)` 调用，Then 返回 2（100、200 低于 250）。

---

## 存疑与观察

**Q1 — 主动加热的充电机在线条件被注释掉（存疑）**
> `heatcontrol.c` 行 130：`//&&(HaveCharger)` 和行 138：`//||(HaveCharger == 0)` 均被注释。
> 意味着主动模式下即使没有充电机，只要非放电状态（静置/浮充）就可以自主加热。这可能导致电池在完全静置、无能量补充的情况下持续消耗电量加热，存在过放风险。**建议确认意图并恢复/删除此条件。**

**Q2 — 主动加热模式缺少放电过温互锁（存疑）**
> 被动放电加热在行 103 检查了 `WARN3CODE0_BIT(DiscTempHigh)`，但主动加热的关断条件（行 136）仅检查 `ChgTempHigh`，未检查 `DiscTempHigh`。若在静置/浮充状态下温度异常升高触发 `DiscTempHigh` 告警，主动加热不会因此关断。这可能是设计疏漏或故意区分场景，**建议确认。**

**Q3 — PTC 温度比较值调节与 heat_flag 独立（观察）**
> `PTCTemp` 的 `SetCompare` 调节逻辑（行 72–79）与 `heat_flag` 控制逻辑相互独立，在 `heat_flag = 0`（加热关断）时仍会执行。若 `AppHeat_SetOutput(0)` 关断输出后 `SetCompare(9999)` 仍被调用，不会产生危险，但逻辑上存在冗余——`PTCTemp = 450~550` 区间时既无 SetCompare 调用也无其他占空比设置，行为取决于上次的比较值。**建议明确此区间的预期占空比。**

**Q4 — `PsssiveChgHeatReq` / `PsssiveDscHeatReq` 赋值来源不明（存疑）**
> 全文搜索仅发现两变量在 `heatcontrol.c` 中定义并在 `HeatControlTask()` 中读取，未找到任何 `.c` 文件对其赋值。`heatcontrol.h` 定义了常量宏 `PassiveChgHeatReq = 1`、`PassiveDiscHeatReq = 2`（与变量同名但不同），同时声明了未使用的 `extern uint8_t PsssiveHeatReq`。推测赋值应在 PCS/通信解析代码（`UpperComTask.c` 的 `CMD_Task()` 或 Modbus 写处理）中完成，但未在现有源码中找到。**此为显著代码缺口，需进一步确认。**

**Q5 — dcdcCmd 仅在 heatcontrol.c 内部定义，无对外设置接口（观察）**
> `DCDC_MOTION dcdcCmd`（含 `RevTimems`、`ChgHeatUp`、`DscHeatUp`）声明为文件内部全局变量，未提供外部访问接口。`RevTimems` 的更新时机未在当前代码中体现，与 Q4 类似，推测由通信层在接收到 PCS 帧时刷新，但代码中未体现。

**Q6 — 加热滞环硬编码 10（存疑）**
> 被动充电和放电加热的滞环均为硬编码值 `-10`（即 1.0℃），不可通过参数调整。实际应用中不同电池的热特性差异可能需要不同滞环宽度，建议将其提取为可配置参数。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-HEAT-001 | 加热模式选择（被动 / 主动） | 否 | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-002 | 被动加热模式——PCS 指令有效性判断 | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-003 | 被动充电加热——开启与关闭阈值 | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-004 | 被动放电加热——开启与关闭阈值 | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-005 | 被动模式——PCS 无请求时禁止加热 | 否 | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-006 | 主动加热模式——自主开启条件 | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-007 | 主动加热模式——自主关闭条件 | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-008 | 主动模式——放电时禁止加热（互锁） | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-009 | PTC 温度反馈——PWM 比较值自动调节 | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-010 | 加热输出状态变化时同步更新 heat_state | 否 | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-011 | 加热初始化 | 否 | `heatcontrol.c:HeatControlInit()` | 已实现 |
| REQ-HEAT-012 | 加热 PWM 频率与占空比可配置接口 | 否 | `heatcontrol.c`；`AppHeat.h` | 已实现 |
| REQ-HEAT-013 | 过温互锁——充电三级告警时强制关热 | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-014 | 过温互锁——放电三级告警时关热（被动） | 是 ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-015 | 温度点统计辅助函数 | 否 | `heatcontrol.c:OverTempPointCnt()`/`UnderTempPointCnt()` | 已实现 |
