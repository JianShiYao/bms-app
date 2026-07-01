# REQ-PWR：电源管理 / 关机 / 休眠唤醒 / RTC 时间 / 看门狗 需求规格

> 本域需求是对 `S16100B_Demo` 现有固件**逆向提炼**的结果（描述"代码当前做了什么"）。
> 提炼时间：2026-06-23。

## 覆盖源文件

| 文件 | 路径 | 主要关注点 |
|---|---|---|
| `AppPower.c/h` | `Application/` | PowerHold、按键关机、进入休眠 |
| `AppTime.c/h` | `Application/` | 1ms 时基、周期任务标志（1s/2s/20s） |
| `AppRtc.c/h` | `Application/` | RTC 读写、时间戳 |
| `AppDelay.c/h` | `Application/` | DWT 微秒/毫秒延时 |
| `iwdg.c/h` | `Core/Src/` `Core/Inc/` | IWDG 配置与喂狗 |
| `pmu.h` | `Driver/BSP/` | PMU 接口（enter_standby、RTC唤醒） |
| `bsp_key.h` | `Driver/BSP/` | 按键定时、PowerHold_On/Off、enter_sleep |
| `rtc.c/h` | `Core/Src/` `Core/Inc/` | RTC HAL 初始化、Get_Time、Set_Time、时间戳转换 |
| `BMS_Info.h` | `Application/` | DayToSleep、HourToWakeup、SelfConsumption、Waite_selfConsumption、PoweroffVolt_mV |
| `ParaSet.c/h` | `Application/` | 默认参数值 DEFAULT_PWEROFF_MV、DEFAULT_SELFCONSUMPTION |
| `main.c` | `Core/Src/` | PowerHold_On、AFE_WAKE 时序、PWR_FLAG_SB 检测、IWDG 注释状态 |

---

## 需求列表

---

### REQ-PWR-001  上电立即拉高 PowerHold（自锁保持）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `main.c:main()` 第 133 行 `PowerHold_On()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统上电复位后，在 GPIO 及外设初始化完成后（DWT_Init、GPIO_Init、DMA_Init、UART 初始化之后）、进入主循环之前，系统应立即调用 `PowerHold_On()` 置高 PowerHold 引脚，以维持电源自锁。

**理由 / 代码依据**
> STM32 通过 PMOS 自锁电路保持供电，`PowerHold_On()` 由 `bsp_key.h` 声明，在 `main.c:133` 最早调用，确保 MCU 在按键松开后仍能继续运行。

**验收准则（可度量）**
- Given 系统上电或从待机模式唤醒 When MCU 完成 GPIO 初始化 Then PowerHold 引脚在进入 `MX_ADC1_Init()` 调用之前已拉高（高电平 ≥ 3.0 V）。

---

### REQ-PWR-002  上电后 AFE 唤醒脉冲时序（1 s 高脉冲）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `main.c:main()` 第 134–136 行 |
| 验证方法 | 测试 / 分析 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 上电初始化期间，系统应向 AFE_WAKE 引脚输出一个宽度约为 1 000 ms（`delay_ms(1000)`）的高脉冲，之后将该引脚置低。

**理由 / 代码依据**
> `HAL_GPIO_WritePin(AFE_WAKE_GPIO_Port, AFE_WAKE_Pin, GPIO_PIN_SET)` → `delay_ms(1000)` → `GPIO_PIN_RESET`。AFE 需要 WAKE 高脉冲才能从低功耗模式退出进入正常工作。

**验收准则（可度量）**
- Given 系统上电 When AFE_WAKE 引脚被置高 Then 保持高电平时间 ≥ 900 ms，且随后 AFE_WAKE 置低。

---

### REQ-PWR-003  按键 2 s 长按触发软件关机（进入 NORMAL_SLEEP）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AppPower.c:AppPower_CheckPowerOff()` 第 13–17 行；`bsp_key.h` 宏 `PWR_KEY_2S (0x01)`、`LongPushTime2s (15)` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当电源按键（GPIOB_PIN_9）被持续按压 ≥ 2 s（计时器计数 ≥ 15 个 100 ms 周期，即 `LongPushTime2s=15`）时，系统应：
> 1. 禁用 TIM1（停止 1 ms 节拍中断）；
> 2. 调用 `enter_sleep(NORMAL_SLEEP)`，进入 NORMAL_SLEEP 状态（PowerHold_Off 切断自锁电源）。

**理由 / 代码依据**
> `getmsg()` 返回 `PWR_KEY_2S` 时，`AppPower_CheckPowerOff()` 执行关机序列。`LongPushTime2s = 15`，每 100 ms 扫描一次，共 1 500 ms ≈ 1.5 s（注意：注释写 "2s" 但实测阈值约 1.5 s，见"存疑"）。

**验收准则（可度量）**
- Given BMS 正常运行 When 按键持续按压 ≥ 1.5 s（`LongPushTime2s×100 ms`） Then 系统在 200 ms 内完成 TIM1 禁用并执行 `enter_sleep`，PowerHold 引脚置低，系统断电。

---

### REQ-PWR-004  按键消抖——10 ms 采样 + 滑动计数防抖

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppTime.c:AppTime_Tick1ms()` 第 37–68 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应每 10 ms 采样一次按键引脚（`read_key()`），并通过范围限幅计数器（取值 0–10）进行软件消抖：计数器 > 7 时认定键按下（`KeyState = SET`），否则认定键松开（`KeyState = RESET`）。

**理由 / 代码依据**
> `key_cnt` 在 `[0, 10]` 范围内递增/递减，阈值 7/10 = 70% 确认率，可抗 70% 以上的抖动采样。

**验收准则（可度量）**
- Given 按键信号含 ≤ 60 ms 抖动 When 信号稳定后 Then `KeyState` 在信号稳定后的 ≤ 80 ms 内正确更新（≥ 8 次 10 ms 采样通过阈值）。

---

### REQ-PWR-005  低电压自动关机（PoweroffVolt_mV 阈值保护）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Info.h` 第 101 行 `PoweroffVolt_mV`；`ParaSet.h` 第 64 行 `DEFAULT_PWEROFF_MV (10*1000 = 10 000 mV)`；`ParaSet.c` 第 405、759–762 行 |
| 验证方法 | 测试 / 分析 |
| 状态 | 存疑（阈值已定义，但触发关机的具体比较逻辑在本批源文件中未找到执行路径，见存疑 O-1） |

**需求描述（EARS 句式）**
> 当电池总电压低于 `Bms.Para.PoweroffVolt_mV`（默认 10 000 mV / 10 V，可由上位机写入）时，系统应进入关机（PowerOff）流程，切断输出并断电自锁。
> 参数上限校验：若 `PoweroffVolt_mV > 2 000 000 mV`（MAX_PWEROFF_MV = 2000 V），则回退至默认值 10 000 mV。

**理由 / 代码依据**
> `DEFAULT_PWEROFF_MV = 10*1000`（`ParaSet.h:64`），上限检查在 `ParaSet.c:759–762`。该参数通过 Modbus 寄存器偏移 95（`UpperComTask.c:518`）可读写。关机触发逻辑在 `BMS_Control.c` 的 `BMS_STA_POWEROFF` 分支体为空（仅 break）。

**验收准则（可度量）**
- Given 电池总电压稳定低于 `PoweroffVolt_mV` When BMS 完成一次扫描周期 Then 系统进入关机状态，所有 MOS 关闭，PowerHold 置低。
- Given 上位机写入 `PoweroffVolt_mV > 2 000 000 mV` When `LimitInit()` 或参数校验运行 Then 参数被强制恢复至 10 000 mV。

---

### REQ-PWR-006  软件复位（NVIC_SystemReset）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AppPower.c:AppPower_Reset()` 第 27–30 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当需要系统复位时，系统应调用 `NVIC_SystemReset()` 执行软件全系统复位。

**理由 / 代码依据**
> `AppPower_Reset()` 直接封装 `NVIC_SystemReset()`，供其他模块请求复位使用（当前代码中暂未见主循环对该函数的调用，属已实现备用）。

**验收准则（可度量）**
- Given `AppPower_Reset()` 被调用 When NVIC 复位指令执行 Then MCU 在 ≤ 100 ms 内重新进入 `main()` 初始化序列。

---

### REQ-PWR-007  TIM1 驱动 1 ms 系统节拍（TIM1 中断回调）

| 属性 | 内容 |
|---|---|
| 类型 | 性能 |
| 安全相关 | 否 |
| 来源（源码） | `main.c:HAL_TIM_PeriodElapsedCallback()` 第 375–377 行；`AppTime.c:AppTime_Tick1ms()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应使用 TIM1 定时中断驱动 1 ms 周期任务节拍，每次 TIM1 更新中断发生时调用 `AppTime_Tick1ms()`，驱动按键扫描、ADC 触发及周期标志置位。

**理由 / 代码依据**
> `HAL_TIM_PeriodElapsedCallback` 中 `htim->Instance == TIM1` 分支调用 `AppTime_Tick1ms()`（`main.c:375–377`）。TIM6 中断仅用于 `HAL_IncTick()`（HAL 时基）。

**验收准则（可度量）**
- Given TIM1 初始化并启动 When 1 ms 到期 Then `AppTime_Tick1ms()` 被调用，误差 ≤ 50 μs（占空比误差 < 5%）。

---

### REQ-PWR-008  1 s / 2 s / 20 s 周期任务标志（one-shot flag）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppTime.c:AppTime_Tick1ms()` 第 83–99 行；`AppTime_Take1sFlag()`、`AppTime_Take2sFlag()`、`AppTime_Take20sFlag()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在 1 ms 节拍计数器满足以下条件时置位对应的周期标志（自清零，调用 Take 函数后即清零）：
> - `delay_cnt % 1000 == 0`：置位 `g_timer_1s_flag`（同时调用 `Get_Time()` 刷新 RTC）；
> - `delay_cnt % 2000 == 0`：置位 `g_timer_2s_flag`（供 CSV 日志写入）；
> - `delay_cnt % 20000 == 0`：置位 `g_timer_20s_flag`（供 Flash 日志写入）。

**理由 / 代码依据**
> `AppTime_Take2sFlag()` 在 `main.c:243` 触发 CSV 写入，`AppTime_Take20sFlag()` 在 `main.c:239` 触发 Flash 日志。

**验收准则（可度量）**
- Given TIM1 正常运行 When `AppTime_Take2sFlag()` 返回 1 Then 距上次返回 1 的时间间隔为 2 000 ms ± 50 ms。
- Given `AppTime_Take20sFlag()` 返回 1 Then 间隔为 20 000 ms ± 200 ms。

---

### REQ-PWR-009  10 ms ADC 触发（1 ms 节拍内）

| 属性 | 内容 |
|---|---|
| 类型 | 性能 |
| 安全相关 | 否 |
| 来源（源码） | `AppTime.c:AppTime_Tick1ms()` 第 43–81 行，`delay_cnt % 10 == 0` 分支 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在 1 ms 节拍中，每 10 ms（`delay_cnt % 10 == 0`）触发一次 ADC DMA 采集（`HAL_ADC_Start_DMA`），同时更新外部通信活跃超时计数器，并根据外部通信是否在 500 ms（`APP_TIME_EXT_COMM_TIMEOUT_TICKS = 50` × 10 ms）内有刷新来设置外部通信活跃标志。

**理由 / 代码依据**
> `APP_TIME_EXT_COMM_TIMEOUT_TICKS = 50U`（`AppTime.c:9`），每 10 ms 递增，50 次 × 10 ms = 500 ms 超时。

**验收准则（可度量）**
- Given 外部通信最后一次刷新后 When 超过 500 ms 无新通信 Then `BspTimer_IsExtCommActive()` 返回 0。

---

### REQ-PWR-010  DWT 微秒精度延时（AppDelay）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppDelay.c:AppDelay_Init()`、`AppDelay_Us()`、`AppDelay_Ms()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应提供基于 ARM DWT CYCCNT 的忙等待延时接口：
> - `AppDelay_Us(us)`：延时 `us` 微秒（分辨率 = 1/SystemCoreClock ≈ 5.95 ns @ 168 MHz）；
> - `AppDelay_Ms(ms)`：延时 `ms` 毫秒（循环调用 `AppDelay_Us(1000)`）；
> - `AppDelay_Init()`：使能 DWT CYCCNT 计数器（CoreDebug + DWT CTRL）。

**理由 / 代码依据**
> `AppDelay_Init()` 在 `main.c:127` 的 `DWT_Init()` 调用（另一包装）之后执行，用于 AFE 驱动等需要精确 μs 延时的场合。

**验收准则（可度量）**
- Given SystemCoreClock = 168 MHz When `AppDelay_Us(100)` 被调用 Then 实际延时 95–110 μs（示波器验证）。

---

### REQ-PWR-011  RTC 时钟源为 LSE（32.768 kHz 外部晶振）

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `rtc.c:HAL_RTC_MspInit()` 第 128–133 行；`SystemClock_Config()` `RCC_OSCILLATORTYPE_LSE` |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应将 RTC 时钟配置为 LSE（外部低速晶振，32.768 kHz），预分频 AsynchPrediv = 127，SynchPrediv = 255，以产生 1 Hz RTC 节拍（(127+1)×(255+1) = 32 768）。

**理由 / 代码依据**
> `PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE`；`hrtc.Init.AsynchPrediv = 127; SynchPrediv = 255`。LSE 提供待机/休眠期间的持续计时。

**验收准则（可度量）**
- Given LSE 晶振正常起振 When RTC 运行 24 h Then 时间误差 ≤ ±60 s（晶振精度 ±20 ppm 范围内）。

---

### REQ-PWR-012  RTC 首次上电初始化（备份寄存器魔术字 0x32F2）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `rtc.c:MX_RTC_Init()` 第 60–87 行；`rtc.c:RTC_Init()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统上电时，应检查 RTC 备份寄存器 `RTC_BKP_DR0` 的值：
> - 若不等于魔术字 `0x32F2`（首次上电），则将 RTC 时间初始化为 2000-01-01 00:00:00，并写入魔术字；
> - 若等于 `0x32F2`（非首次），则跳过时间初始化，保留 RTC 已有时间。

**理由 / 代码依据**
> 备份寄存器在 VBat 供电时保持，可区分首次上电与 VCC 上电复位，防止每次复位时间被重置为默认值。

**验收准则（可度量）**
- Given 新硬件首次上电 When `MX_RTC_Init()` 执行 Then RTC 时间被设置为 2000-01-01 00:00:00 且 `RTC_BKP_DR0 == 0x32F2`。
- Given MCU 热复位（VBat 有效）When `MX_RTC_Init()` 执行 Then RTC 时间保持上次设置值不变。

---

### REQ-PWR-013  RTC 时间读取（GetTime 每秒刷新 Calendar 结构体）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `rtc.c:Get_Time()`；`AppTime.c:AppTime_Tick1ms()` 第 87–91 行（`delay_cnt % 1000 == 0`）；`AppRtc.c:AppRtc_GetNow()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应每 1 s 调用一次 `Get_Time()` 读取 RTC 时间，将年月日时分秒写入全局结构体 `Calendar`（`volatile CalendarObj`），供应用层通过 `AppRtc_GetNow()` 获取当前时间。

**理由 / 代码依据**
> `AppTime_Tick1ms()` 在 `delay_cnt % 1000 == 0` 时调用 `Get_Time()`（`AppTime.c:88`）。`Calendar.w_year` 存储 RTC HAL 返回的年份偏移值（0–99，代表 2000–2099）。

**验收准则（可度量）**
- Given RTC 正常运行 When `AppRtc_GetNow()` 被调用 Then 返回的时间与 RTC 实际时间误差 ≤ 1 s。

---

### REQ-PWR-014  RTC 时间设置（Set_Time，含参数校验）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `rtc.c:Set_Time()` 第 237–271 行；`AppRtc.c:AppRtc_Set()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应提供 RTC 时间写入接口 `Set_Time(year, month, day, hour, min, sec)`，写入前应执行参数合法性校验：月份 1–12、日期 1–31、时 0–23、分 0–59、秒 0–59，任一参数越界则返回 `HAL_ERROR`，不执行写入。

**理由 / 代码依据**
> `Set_Time()` 第 244–247 行执行范围检查，违规返回 `HAL_ERROR`。年份转换：`sDate.Year = syear - 2000`（支持 2000 年以后）。

**验收准则（可度量）**
- Given 传入非法月份 13 When `Set_Time()` 被调用 Then 返回 `HAL_ERROR`，RTC 时间不改变。
- Given 传入合法时间 2025-06-23 10:30:00 When `Set_Time()` 被调用 Then 返回 `HAL_OK`，随后 `Get_Time()` 读取值与写入值一致。

---

### REQ-PWR-015  Unix 时间戳转换接口（ms 精度）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `rtc.c:dateTimeToTimestamp()`、`GetdateTimeToTimestamp()`、`unix_to_datetime()`；`AppRtc.c:AppRtc_GetTimestampNow()`、`AppRtc_ToTimestamp()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应提供以下时间戳转换功能：
> - 年月日时分秒 → Unix 时间戳（秒，1970 年起）：`dateTimeToTimestamp()`，年份 < 1970 返回 0；
> - 当前 RTC 时间 → Unix 时间戳（毫秒）：`GetdateTimeToTimestamp()`（= `dateTimeToTimestamp × 1000`）；
> - Unix 时间戳（毫秒）→ 年月日时分秒：`unix_to_datetime()`（先除以 1000 转换为秒）。

**理由 / 代码依据**
> `GetdateTimeToTimestamp()` 返回 `seconds * 1000`，单位为 ms，`AppRtc_GetTimestampNow()` 暴露此接口给应用层（如日志时间戳）。

**验收准则（可度量）**
- Given RTC 时间为 2025-01-07 16:10:10 When `GetdateTimeToTimestamp()` 被调用 Then 返回值 = 1736259010 × 1000 ms（误差 0）。

---

### REQ-PWR-016  待机唤醒检测（PWR_FLAG_SB）及 RTC 闹钟清除

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `main.c:main()` 第 150–156 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统启动时，应检查待机唤醒标志 `PWR_FLAG_SB`：
> - 若标志置位（表明 MCU 从 STM32 Standby 模式被 RTC 闹钟唤醒），应清除该标志，并立即调用 `HAL_RTC_DeactivateAlarm(RTC_ALARM_A)` 以停止 RTC_ALARM_A，防止闹钟重复触发。

**理由 / 代码依据**
> 代码注释"非常关键：关闭闹钟，防止再次触发"（`main.c:154`）。未清除闹钟会导致每次唤醒后立即再次进入 Standby。

**验收准则（可度量）**
- Given MCU 从 RTC 闹钟待机唤醒 When 初始化代码运行 Then `PWR_FLAG_SB` 被清除，`RTC_ALARM_A` 被禁用，系统继续正常启动而不再度进入 Standby。

---

### REQ-PWR-017  PMU 接口——RTC 唤醒定时器 start/stop

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `pmu.h` 第 6–8 行 `start_RTCWK(uint16_t time)`、`stop_RTCWK()` |
| 验证方法 | 检视 / 分析 |
| 状态 | 存疑（pmu.c 实现在本批镜像中未提供，接口仅见头文件） |

**需求描述（EARS 句式）**
> 系统应提供 RTC 唤醒定时器接口：
> - `start_RTCWK(time)`：以 `time`（单位推断为秒或分钟，待确认）为周期启动 RTC Wakeup 定时器，用于定时从 Standby 唤醒；
> - `stop_RTCWK()`：停止 RTC Wakeup 定时器。

**理由 / 代码依据**
> 头文件声明在 `pmu.h:6–8`。`HourToWakeup` 参数（`BMS_Info.h:113`）表示休眠 X 小时后自唤醒，推断与 `start_RTCWK` 配合使用。

**验收准则（可度量）**
- Given `start_RTCWK(T)` 被调用 When T 时间到期 Then MCU 从 Standby 被唤醒，`PWR_FLAG_SB` 置位。

---

### REQ-PWR-018  PMU 接口——通信唤醒初始化（comm_wakeup_init）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `pmu.h` 第 10 行 `comm_wakeup_init()` |
| 验证方法 | 检视 |
| 状态 | 存疑（pmu.c 未提供，实现未知） |

**需求描述（EARS 句式）**
> 系统应提供通信唤醒初始化接口 `comm_wakeup_init()`，用于配置外部通信信号（如 UART RX 边沿）作为 Standby 唤醒源。

**理由 / 代码依据**
> `pmu.h:10` 声明，具体实现未在本批文件中提供。

**验收准则（可度量）**
- Given `comm_wakeup_init()` 被调用且 MCU 处于 Standby When UART 接收引脚检测到边沿 Then MCU 被唤醒。

---

### REQ-PWR-019  PMU 接口——进入 STM32 Standby 模式（enter_standby）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `pmu.h` 第 12 行 `enter_standby()`；`bsp_key.h` 第 36–40 行 `enum {NORMAL_SLEEP, RTC_SLEEP}` |
| 验证方法 | 测试 |
| 状态 | 存疑（pmu.c 实现未提供；`enter_sleep(RTC_SLEEP)` 与 `enter_standby()` 的关系未确认） |

**需求描述（EARS 句式）**
> 系统应提供进入 STM32 Standby 低功耗模式的接口 `enter_standby()`，在进入之前应确保所有必要外设（MOS、AFE）已关闭，以最小化待机电流。`RTC_SLEEP` 类型的 `enter_sleep()` 应在配置 RTC 唤醒闹钟后调用 `enter_standby()`。

**理由 / 代码依据**
> `bsp_key.h:36–40` 枚举 `NORMAL_SLEEP`（直接掉电）和 `RTC_SLEEP`（配合 RTC 唤醒待机）两种模式。`HourToWakeup` 参数（`BMS_Info.h:113`）为 `RTC_SLEEP` 提供唤醒时间。

**验收准则（可度量）**
- Given `enter_standby()` 被调用 When MCU 进入 Standby Then 工作电流降至 ≤ Standby 电流规格（STM32F4 典型 2.4 μA @ 3.3 V）。

---

### REQ-PWR-020  静置 X 天后自动进入休眠（DayToSleep）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Info.h` 第 112 行 `DayToSleep`；`ParaSet.c` 第 656 行（默认值 0）；`UpperComTask.c` 第 401、723 行（Modbus 寄存器 259） |
| 验证方法 | 测试 / 分析 |
| 状态 | 存疑（参数定义和通信接口已实现，但触发休眠的逻辑在 `BMS_Control.c` / `SocTask.c` 中未找到完整实现，见存疑 O-2） |

**需求描述（EARS 句式）**
> 当 `DayToSleep > 0` 时，系统应在 BMS 处于空闲状态（无充电、无放电、无通信）持续 `DayToSleep` 天后，自动进入休眠（Standby）模式以节省电量。
> 默认值 0 表示该功能禁用（不自动休眠）。

**理由 / 代码依据**
> `Bms.Para.DayToSleep` 默认初始化为 0（`ParaSet.c:656`），通过 Modbus 寄存器 259 可读写。`Bms.IdleTime_ms` 字段（`BMS_Info.h:187`）用于追踪空闲时长，单位 ms。

**验收准则（可度量）**
- Given `DayToSleep = 1` 且 BMS 持续 1 天空闲（`IdleTime_ms ≥ 86 400 000 ms`）When 检查逻辑运行 Then 系统进入 Standby。
- Given `DayToSleep = 0` When 任意空闲时长 Then 系统不因空闲时长触发休眠。

---

### REQ-PWR-021  休眠后 X 小时自唤醒（HourToWakeup + RTC 闹钟）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Info.h` 第 113 行 `HourToWakeup`；`ParaSet.c` 第 657 行（默认值 0）；`UpperComTask.c` 第 402、724 行（Modbus 寄存器 260） |
| 验证方法 | 测试 |
| 状态 | 存疑（参数定义和通信接口已实现，唤醒逻辑依赖 `start_RTCWK()` 实现，pmu.c 未提供，见存疑 O-3） |

**需求描述（EARS 句式）**
> 当 `HourToWakeup > 0` 时，系统在进入 Standby 休眠之前应配置 RTC 闹钟，在 `HourToWakeup` 小时后触发唤醒；唤醒后完成初始化，关闭 RTC_ALARM_A（REQ-PWR-016）后继续正常工作。
> 默认值 0 表示不设定定时唤醒（休眠后仅靠按键或通信唤醒）。

**理由 / 代码依据**
> `Bms.Para.HourToWakeup` 默认 0（`ParaSet.c:657`），Modbus 寄存器 260。与 `REQ-PWR-016` 配合构成完整的休眠/定时唤醒闭环。

**验收准则（可度量）**
- Given `HourToWakeup = 2` 且系统进入 Standby When 2 小时到期 Then MCU 从 Standby 唤醒，`PWR_FLAG_SB` 置位，`RTC_ALARM_A` 被清除。

---

### REQ-PWR-022  自耗电扣减（SelfConsumption SOC 修正）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.h` 第 107 行 `SelfConsumption`（单位：1 = 0.1%/天）；`ParaSet.h` 第 327 行 `DEFAULT_SELFCONSUMPTION = 30`（即 3.0%/天）；`SocTask.c` 第 573 行 |
| 验证方法 | 测试 / 分析 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在每次 SOC 积分计算中，从充放电电量中扣除自耗电：
> `self_discharge_per_step = SelfConsumption × CurrentCap_mAH / 24 / 1000`（mA·ms/step）
> 其中 `SelfConsumption` 单位为 0.1%/天，默认值 30（即 3.0%/天），可通过 Modbus 寄存器 262 修改。

**理由 / 代码依据**
> `SocTask.c:573`：`raw_delta_mAms -= Bms.Para.SelfConsumption * Bms.CurrentCap_mAH / 24 / 1000`。

**验收准则（可度量）**
- Given `SelfConsumption = 30`、`CurrentCap_mAH = 100 000 mAh` When 1 天运行 Then SOC 因自耗减少 3.0%（±0.1%）。

---

### REQ-PWR-023  满充等待自耗标志（Waite_selfConsumption）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.h` 第 110 行 `Waite_selfConsumption`；`BMS_Control.c` 第 313、408 行；`AppCsvLog.c` 第 133 行；`ParaSet.c` 第 662 行（默认 FALSE） |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在充电全满后，系统应将 `Waite_selfConsumption` 置为 TRUE，表示"正在等待自耗电消耗"。在此状态下：
> - 充电 MOS 禁止导通（`BMS_Control.c:408–410`）；
> - CSV 日志记录该标志状态（`AppCsvLog.c:133–136`）。

**理由 / 代码依据**
> `BmsControl_RequestChgByParam()` 在 `Waite_selfConsumption == FALSE` 时才开启充电 MOS，满充后设该标志以防过充。

**验收准则（可度量）**
- Given 电池达到满充条件 When `Waite_selfConsumption` 为 TRUE Then 充电 MOS 禁止导通，直至 SOC 因自耗降至满充阈值以下。

---

### REQ-PWR-024  IWDG 配置（Prescaler=256, Reload=4095，超时约 26.2 s）

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `iwdg.c:MX_IWDG_Init()` 第 41–42 行；`iwdg.h:FeedIwdg()` |
| 验证方法 | 分析 |
| 状态 | 存疑（IWDG 已配置但在 `main.c:143` 被注释掉未启用，见存疑 O-4） |

**需求描述（EARS 句式）**
> 系统应启用独立看门狗（IWDG），参数如下：
> - 时钟：LSI（约 32 kHz）；
> - 预分频：256；
> - 重装值：4095；
> - 超时周期：4095 × 256 / 32 000 ≈ **32.76 s**（理论值，LSI 频率存在 ±50% 偏差，实际范围约 16–49 s）；
> - 喂狗函数：`FeedIwdg()`（`HAL_IWDG_Refresh(&hiwdg)`）。
> 系统应在主循环内以 ≤ 超时周期 / 2 的间隔调用 `FeedIwdg()`，以防止系统死锁时触发看门狗复位。

**理由 / 代码依据**
> `hiwdg.Init.Prescaler = IWDG_PRESCALER_256; Reload = 4095`（`iwdg.c:41–42`）。当前 `main.c:143` 中 `MX_IWDG_Init()` 已被注释掉。

**验收准则（可度量）**
- Given IWDG 启用且主循环正常运行 When 主循环每次迭代后调用 `FeedIwdg()` Then 系统不产生 IWDG 复位（`RCC_CSR.IWDGRST = 0`）。
- Given IWDG 启用且主循环卡死超过 32 s When IWDG 超时 Then MCU 产生复位（`RCC_CSR.IWDGRST = 1`）。

---

### REQ-PWR-025  复位来源诊断输出（Error_HandlerEx）

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `main.c:Error_HandlerEx()` 第 302–361 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当发生错误（`Error_Handler` 被调用）时，系统应通过 UART 输出 RCC 状态、时钟配置及复位来源标志（PORRST、PINRST、SFTRST、IWDGRST、WWDGRST、BORRST）以供调试，并继续执行（`return`，不死循环）。

**理由 / 代码依据**
> `Error_HandlerEx()` 读取 `RCC->CSR` 的复位标志并通过 `printf` 输出，代码注释"调试版本保留 return"（`main.c:358`），正式版本应改为 LED 指示或复位。

**验收准则（可度量）**
- Given HAL 函数返回错误 When `Error_Handler()` 被触发 Then UART 在 100 ms 内输出包含 IWDGRST 等标志的诊断字符串。

---

## 存疑与观察

### O-1：低电压关机触发路径缺失（REQ-PWR-005）

`Bms.Para.PoweroffVolt_mV` 参数已定义（默认 10 000 mV），`BMS_STA_POWEROFF` 状态已在状态机中声明（`BMS_Control.c:452`），但 `case BMS_STA_POWEROFF` 体为空（仅 `break`）。状态机进入 POWEROFF 状态的条件判断（电压比较触发）在本批源文件中**未找到执行路径**。可能在 `BMS_Control.c` 中通过 `Bms.sta = BMS_STA_POWEROFF` 的赋值点来触发，但当前代码中该赋值点尚未在本批阅读范围内发现。**建议**：补充完整的低压关机触发逻辑，或确认该功能由 PROT 域的保护任务间接实现。

### O-2：DayToSleep 休眠触发逻辑缺失（REQ-PWR-020）

`DayToSleep` 参数在通信和参数层已完整实现，但在 `BMS_Control.c` 和 `SocTask.c` 中未发现根据 `Bms.IdleTime_ms` 与 `DayToSleep` 比较后触发 `enter_sleep(RTC_SLEEP)` 的代码。默认值 0（禁用），但非零时的触发逻辑属于**缺口**。

### O-3：pmu.c 未提供（REQ-PWR-017/018/019）

`pmu.h` 声明了 `start_RTCWK`、`stop_RTCWK`、`comm_wakeup_init`、`enter_standby` 四个函数，但 `pmu.c`（实现文件）未在本批镜像文件中提供。REQ-PWR-017/018/019 的实现细节（Standby 进入序列、RTC 唤醒配置）无法从代码核实。

### O-4：IWDG 在正式代码中被注释掉（REQ-PWR-024）【重要安全缺口】

`main.c:143` 行 `//MX_IWDG_Init();` 被注释，即**独立看门狗当前未被启用**。`FeedIwdg()` 函数存在但无法在 IWDG 未初始化时喂狗。代码内 `Error_HandlerEx()` 已有分析 IWDG 复位标志的逻辑（`main.c:338–340`），说明开发者有意监测 IWDG 行为，但暂时关闭了 IWDG 以便调试。正式产品版本**应启用 IWDG**，且主循环内必须有明确的 `FeedIwdg()` 调用点。此为**安全性缺口**。

### O-5：按键 2 s 时间注释与实际阈值不一致（REQ-PWR-003）

`bsp_key.h` 中 `LongPushTime2s = 15`，注释 `*100`（ms），实际阈值 = 1 500 ms（1.5 s），但宏名含"2s"、注释含"2s"，存在歧义。建议澄清实际按压时间需求，统一宏名与注释。

### O-6：CheckPwrOnEvent 被注释掉

`main.c:169` `//CheckPwrOnEvent();` 上电事件检测函数被注释，上电事件（如唤醒来源区分、充电插入唤醒等）可能缺失逻辑。建议确认是否需要恢复。

### O-7：Error_Handler 正式版本仍返回

`main.c:391–404` 原始 `Error_Handler` 函数已被注释，替换为 `Error_HandlerEx` 并在 `while(1)` 中 `return`（调试模式）。正式版本应移除 `return`，改为系统复位或安全停机，否则错误发生后系统可能继续以错误状态运行。

### O-8：sleep_time 全局变量未使用

`main.c:107` 声明 `uint16_t sleep_time;` 为全局变量，在本批文件中未见任何赋值或读取，疑似遗留未完成的设计意图（可能与 DayToSleep 相关）。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-PWR-001 | 上电立即拉高 PowerHold（自锁保持） | ⚠️ | `main.c:main()` | 已实现 |
| REQ-PWR-002 | 上电后 AFE 唤醒脉冲时序（1 s 高脉冲） | ⚠️ | `main.c:main()` | 已实现 |
| REQ-PWR-003 | 按键 2 s 长按触发软件关机（进入 NORMAL_SLEEP） | ⚠️ | `AppPower.c:AppPower_CheckPowerOff()` | 已实现 |
| REQ-PWR-004 | 按键消抖——10 ms 采样 + 滑动计数防抖 | 否 | `AppTime.c:AppTime_Tick1ms()` | 已实现 |
| REQ-PWR-005 | 低电压自动关机（PoweroffVolt_mV 阈值保护） | ⚠️ | `BMS_Info.h`、`ParaSet.h/c` | 存疑 |
| REQ-PWR-006 | 软件复位（NVIC_SystemReset） | ⚠️ | `AppPower.c:AppPower_Reset()` | 已实现 |
| REQ-PWR-007 | TIM1 驱动 1 ms 系统节拍（TIM1 中断回调） | 否 | `main.c:HAL_TIM_PeriodElapsedCallback()` | 已实现 |
| REQ-PWR-008 | 1 s / 2 s / 20 s 周期任务标志（one-shot flag） | 否 | `AppTime.c:AppTime_Tick1ms()` | 已实现 |
| REQ-PWR-009 | 10 ms ADC 触发及外部通信超时（500 ms） | 否 | `AppTime.c:AppTime_Tick1ms()` | 已实现 |
| REQ-PWR-010 | DWT 微秒精度延时（AppDelay） | 否 | `AppDelay.c` | 已实现 |
| REQ-PWR-011 | RTC 时钟源为 LSE（32.768 kHz 外部晶振） | 否 | `rtc.c:HAL_RTC_MspInit()` | 已实现 |
| REQ-PWR-012 | RTC 首次上电初始化（备份寄存器魔术字 0x32F2） | 否 | `rtc.c:MX_RTC_Init()` | 已实现 |
| REQ-PWR-013 | RTC 时间读取（GetTime 每秒刷新 Calendar） | 否 | `rtc.c:Get_Time()` | 已实现 |
| REQ-PWR-014 | RTC 时间设置（Set_Time，含参数校验） | 否 | `rtc.c:Set_Time()` | 已实现 |
| REQ-PWR-015 | Unix 时间戳转换接口（ms 精度） | 否 | `rtc.c:dateTimeToTimestamp()` 等 | 已实现 |
| REQ-PWR-016 | 待机唤醒检测（PWR_FLAG_SB）及 RTC 闹钟清除 | ⚠️ | `main.c:main()` | 已实现 |
| REQ-PWR-017 | PMU 接口——RTC 唤醒定时器 start/stop | ⚠️ | `pmu.h` | 存疑 |
| REQ-PWR-018 | PMU 接口——通信唤醒初始化（comm_wakeup_init） | 否 | `pmu.h` | 存疑 |
| REQ-PWR-019 | PMU 接口——进入 STM32 Standby 模式（enter_standby） | ⚠️ | `pmu.h`、`bsp_key.h` | 存疑 |
| REQ-PWR-020 | 静置 X 天后自动进入休眠（DayToSleep） | ⚠️ | `BMS_Info.h`、`ParaSet.c` | 存疑 |
| REQ-PWR-021 | 休眠后 X 小时自唤醒（HourToWakeup + RTC 闹钟） | ⚠️ | `BMS_Info.h`、`ParaSet.c` | 存疑 |
| REQ-PWR-022 | 自耗电扣减（SelfConsumption SOC 修正） | 否 | `SocTask.c`、`BMS_Info.h` | 已实现 |
| REQ-PWR-023 | 满充等待自耗标志（Waite_selfConsumption） | 否 | `BMS_Control.c`、`BMS_Info.h` | 已实现 |
| REQ-PWR-024 | IWDG 配置（Prescaler=256, Reload=4095，超时约 32 s） | ⚠️ | `iwdg.c:MX_IWDG_Init()` | 存疑 |
| REQ-PWR-025 | 复位来源诊断输出（Error_HandlerEx） | 否 | `main.c:Error_HandlerEx()` | 已实现 |
