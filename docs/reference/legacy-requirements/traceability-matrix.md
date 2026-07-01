# S16100B 需求追溯矩阵（全量）

> 需求 → 源码来源 → 状态 的总表。每条需求的「验证方法」「验收准则」「EARS 描述」见各域文件正文。
> 状态：已实现（代码中可见）/ 已实现（存疑）/ 存疑 / 缺口（代码缺失或被注释）。
> 安全相关项（⚠️）的失效/缺口风险详见 [risks-and-gaps.md](risks-and-gaps.md)。

## 统计

| 维度 | 数量 |
|---|---:|
| 需求总数 | 231 |
| 安全相关（⚠️） | 96 |
| 状态＝缺口 | 3（REQ-PROT-031/032、REQ-LED-007） |
| 状态＝存疑 / 已实现（存疑） | 见下表标注（约 20+ 项） |

---

## CTRL — 系统状态机 / 接触器控制 / 启动自检

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-CTRL-001 | 上电立即关断所有 MOS | ⚠️ | `main.c:main()` | 已实现 |
| REQ-CTRL-002 | 系统工作状态枚举 | 否 | `BMS_Info.h:BMS_WORK_STA_S` | 已实现 |
| REQ-CTRL-003 | 上电默认进入 DISCONNET 状态 | ⚠️ | `BMS_Info.c`, `BmsControlTask()` | 已实现 |
| REQ-CTRL-004 | 上电 5s 稳定等待 | 否 | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-005 | DISCONNET 状态下保护优先转移 | ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-006 | DISCONNET→STANDBY/PRECHG 正常转移 | 否 | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-007 | 预充序列：先预充 MOS 后等 Vmos 收敛 | ⚠️ | `BMS_Control.c:PreDischg()` | 已实现 |
| REQ-CTRL-008 | 预充过电流保护（-3000mA/1000ms） | ⚠️ | `BMS_Control.c:PreDischg_CheckFault()` | 已实现 |
| REQ-CTRL-009 | 预充期间故障即失败 | ⚠️ | `BMS_Control.c:PreDischg_CheckFault()` | 已实现 |
| REQ-CTRL-010 | 预充超时强制完成 | 否 | `BMS_Control.c:PreDischg()` | 已实现 |
| REQ-CTRL-011 | 放电 MOS 已开则跳过预充 | 否 | `BMS_Control.c:PreDischg()` | 已实现 |
| REQ-CTRL-012 | 预充成功转 STANDBY，失败转 PROTECT | ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-013 | 故障立即触发保护状态 | ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-014 | 正常运行态充放电状态更新 | 否 | `BMS_Control.c:BmsControl_UpdateRunState()` | 已实现 |
| REQ-CTRL-015 | 电流状态判定阈值 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-CTRL-016 | SYS_FAULT 触发条件（全 MOS 断开） | ⚠️ | `WarningTask.c:IsBmsWarning()` | 已实现 |
| REQ-CTRL-017 | SYS_FAULT 时断开所有 MOS | ⚠️ | `BMS_Control.c:DealFault()` | 已实现 |
| REQ-CTRL-018 | DISC_FAULT 时断开放电 MOS | ⚠️ | `BMS_Control.c:DealFault()` | 已实现 |
| REQ-CTRL-019 | CHG_FAULT 时断开充电 MOS | ⚠️ | `BMS_Control.c:DealFault()` | 已实现 |
| REQ-CTRL-020 | 上位机可强制覆盖各 MOS 控制权 | ⚠️ | `BMS_Control.c:BmsMos_GetFinalState()` | 已实现（见 S3） |
| REQ-CTRL-021 | 限流激活时阻止充电 MOS 开启 | ⚠️ | `BMS_Control.c:BmsMos_SetChg()` | 已实现 |
| REQ-CTRL-022 | CHG/SYS_FAULT 时强制关断限流器 | ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-023 | 充电限流功能（Limit_Task） | 否 | `BMS_Control.c:Limit_Task()` | 已实现（见 S5） |
| REQ-CTRL-024 | 满充状态下关闭限流器 | 否 | `BMS_Control.c:Limit_Task()` | 已实现 |
| REQ-CTRL-025 | PROTECT 状态下分级 MOS 策略 | ⚠️ | `BMS_Control.c:BmsControlTask()` | 已实现（见 S4） |
| REQ-CTRL-026 | 充电态屏蔽放电故障 / 放电态屏蔽充电故障 | ⚠️ | `WarningTask.c:IsBmsWarning()` | 已实现 |
| REQ-CTRL-027 | 低电量自动休眠（600s） | 否 | `WarningTask.c:needsleep()` | 已实现 |
| REQ-CTRL-028 | 按键 2s 长按关机 | 否 | `AppPower.c:AppPower_CheckPowerOff()` | 已实现 |
| REQ-CTRL-029 | MOS 最终状态写入（BmsMos_Apply） | ⚠️ | `BMS_Control.c:BmsMos_Apply()` | 已实现 |
| REQ-CTRL-030 | 非预充状态时复位预充状态机 | 否 | `BMS_Control.c:BmsControlTask()` | 已实现 |
| REQ-CTRL-031 | 主循环 17 步任务编排顺序 | 否 | `main.c:main()` | 已实现 |
| REQ-CTRL-032 | TIM1 1ms 系统时基 | 否 | `main.c:HAL_TIM_PeriodElapsedCallback()` | 已实现 |
| REQ-CTRL-033 | AFE 唤醒脉冲初始化（1000ms 高电平） | 否 | `main.c:main()` | 已实现 |

## PWR — 电源管理 / 关机 / 休眠唤醒 / RTC / 看门狗

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-PWR-001 | 上电立即拉高 PowerHold | ⚠️ | `main.c:main()` | 已实现 |
| REQ-PWR-002 | AFE 唤醒脉冲时序（1s 高脉冲） | ⚠️ | `main.c:main()` | 已实现 |
| REQ-PWR-003 | 按键 2s 长按触发软件关机 | ⚠️ | `AppPower.c:AppPower_CheckPowerOff()` | 已实现 |
| REQ-PWR-004 | 按键消抖（10ms 采样+滑动计数） | 否 | `AppTime.c:AppTime_Tick1ms()` | 已实现 |
| REQ-PWR-005 | 低电压自动关机（PoweroffVolt_mV=10000mV） | ⚠️ | `BMS_Info.h`, `ParaSet.c` | 存疑（见 G8） |
| REQ-PWR-006 | 软件复位（NVIC_SystemReset） | ⚠️ | `AppPower.c:AppPower_Reset()` | 已实现 |
| REQ-PWR-007 | TIM1 驱动 1ms 系统节拍 | 否 | `main.c:HAL_TIM_PeriodElapsedCallback()` | 已实现 |
| REQ-PWR-008 | 1s/2s/20s 周期任务标志 | 否 | `AppTime.c:AppTime_Tick1ms()` | 已实现 |
| REQ-PWR-009 | 10ms ADC 触发及外部通信超时（500ms） | 否 | `AppTime.c:AppTime_Tick1ms()` | 已实现 |
| REQ-PWR-010 | DWT 微秒精度延时 | 否 | `AppDelay.c` | 已实现 |
| REQ-PWR-011 | RTC 时钟源 LSE（32.768kHz） | 否 | `rtc.c:HAL_RTC_MspInit()` | 已实现 |
| REQ-PWR-012 | RTC 首次上电初始化（魔术字 0x32F2） | 否 | `rtc.c:MX_RTC_Init()` | 已实现 |
| REQ-PWR-013 | RTC 每秒刷新 Calendar | 否 | `rtc.c:Get_Time()` | 已实现 |
| REQ-PWR-014 | RTC 时间设置含参数校验 | 否 | `rtc.c:Set_Time()` | 已实现 |
| REQ-PWR-015 | Unix 时间戳转换（ms 精度） | 否 | `rtc.c:dateTimeToTimestamp()` | 已实现 |
| REQ-PWR-016 | 待机唤醒检测 PWR_FLAG_SB + 闹钟清除 | ⚠️ | `main.c:main()` | 已实现 |
| REQ-PWR-017 | PMU——RTC 唤醒定时器 start/stop | ⚠️ | `pmu.h` | 存疑（见 G8） |
| REQ-PWR-018 | PMU——通信唤醒初始化 | 否 | `pmu.h` | 存疑 |
| REQ-PWR-019 | PMU——进入 STM32 Standby 模式 | ⚠️ | `pmu.h`, `bsp_key.h` | 存疑（见 G8） |
| REQ-PWR-020 | 静置 X 天后自动休眠（DayToSleep，默认 0） | ⚠️ | `BMS_Info.h`, `ParaSet.c` | 存疑（见 G8） |
| REQ-PWR-021 | 休眠后 X 小时自唤醒（HourToWakeup，默认 0） | ⚠️ | `BMS_Info.h`, `ParaSet.c` | 存疑（见 G8） |
| REQ-PWR-022 | 自耗电 SOC 扣减（默认 3.0%/天） | 否 | `SocTask.c`, `BMS_Info.h` | 已实现 |
| REQ-PWR-023 | 满充等待自耗标志 | 否 | `BMS_Control.c`, `BMS_Info.h` | 已实现 |
| REQ-PWR-024 | IWDG 配置（≈32s，当前被注释） | ⚠️ | `iwdg.c:MX_IWDG_Init()` | 存疑（见 S1） |
| REQ-PWR-025 | 复位来源诊断输出 | 否 | `main.c:Error_HandlerEx()` | 已实现 |

## AFE — 采集 / 平滑 / 硬件保护 / 断路 / 负载 / 均衡

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-AFE-001 | 电芯电压采集通道数与使能掩码 | 否 | `BMS_Info.h`, `AFE.h`, `AppAfe.c` | 已实现（见 Q2） |
| REQ-AFE-002 | 电芯电压量纲与数据路径 | 否 | `Calculate.h`, `AppAfe.c` | 已实现 |
| REQ-AFE-003 | 电流采集——双路 ADC 与校准 | ⚠️ | `AFE.h`, `AppAfe.c` | 已实现 |
| REQ-AFE-004 | 温度采集通道数、量纲与 NTC | ⚠️ | `BMS_Info.h`, `Calculate.h`, `BmsAdc.h` | 已实现 |
| REQ-AFE-005 | MCU ADC 辅助采集（MOS/环境/PTC/漏液/地址） | 否 | `BmsAdc.c`, `bsp_adc.h` | 已实现 |
| REQ-AFE-006 | AFE SPI 通信故障检测与告警 | ⚠️ | `AFE.h`, `AppAfe.c` | 已实现 |
| REQ-AFE-007 | AFE 硬件短路保护位读取与维护 | ⚠️ | `AFE.h`, `AppAfe.c` | 已实现 |
| REQ-AFE-008 | AFE 放电二级过流保护位读取与清除 | ⚠️ | `AFE.h`, `AppAfe.c` | 已实现 |
| REQ-AFE-009 | AFE 硬件保护寄存器初始化 | ⚠️ | `AFE.h`（Parameter 宏） | 已实现 |
| REQ-AFE-010 | AFE 保护状态恢复处理任务 | ⚠️ | `AppAfe.c`, `AFE_Protect.h` | 已实现 |
| REQ-AFE-011 | 电流平滑——6 点去极值均值 | 否 | `AppAfe.c` | 已实现 |
| REQ-AFE-012 | 负载接入检测（放电侧） | 否 | `AppAfe.c`, `ChargerLoad.h` | 已实现 |
| REQ-AFE-013 | 充电器接入检测 | 否 | `ChargerLoad.h`, `AFE.h` | 已实现（见 S7） |
| REQ-AFE-014 | 电芯断路（开路）检测 | ⚠️ | `AppAfe.c`, `Balance.h`, `AFE.h` | 已实现 |
| REQ-AFE-015 | 被动均衡——开启条件 | 否 | `AppAfe.c`, `Balance.h`, `ParaSet.h` | 已实现（存疑，见 Q1） |
| REQ-AFE-016 | 被动均衡——浮充与间隔定时 | 否 | `BMS_Info.h`, `ParaSet.h` | 已实现 |
| REQ-AFE-017 | AFE 芯片初始化与重初始化 | ⚠️ | `AFE.h`, `AppAfe.c` | 已实现 |
| REQ-AFE-018 | 调试注入模式——旁路 AFE 采集 | 否 | `AppAfe.c`, `BmsAdc.c` | 已实现 |
| REQ-AFE-019 | MOS 温升监测与地址 ADC 自标定 | 否 | `BmsAdc.c` | 已实现 |

## SOC — SOC/SOH/SOP / 容量 / 循环 / 统计

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-SOC-001 | 上电 OCV 初始化 SOC | 否 | `SocTask.c:SocInitIfNeeded()` | 已实现 |
| REQ-SOC-002 | OCV 查表仅在静置状态执行 | 否 | `SocTask.c:GetSocFromOCVTbl()` | 已实现 |
| REQ-SOC-003 | 库仑积分（带温度补偿）更新剩余电量 | 否 | `SocTask.c:SocUpdateCoulombCount()` | 已实现 |
| REQ-SOC-004 | 温度补偿增益夹紧 | 否 | `SocTask.c:SocGetTempCompSocGainPermille()` | 已实现 |
| REQ-SOC-005 | 自耗电扣除 | 否 | `SocTask.c:SocUpdateCoulombCount()` | 已实现 |
| REQ-SOC-006 | 历史累计充放电量统计 | 否 | `SocTask.c:SocUpdateCoulombCount()` | 已实现 |
| REQ-SOC-007 | 循环次数计算 | 否 | `SocTask.c:SocPersistIfChanged()` | 已实现 |
| REQ-SOC-008 | SOC 从剩余电量计算（1% 精度） | 否 | `SocTask.c:CalSocFunc()` | 已实现 |
| REQ-SOC-009 | 充电状态 SOC 修正（充满检测与拉升） | 否 | `SocTask.c:SocHandleCharging()` | 已实现 |
| REQ-SOC-010 | 放电状态 SOC 修正（低电锚点与防拉高） | 否 | `SocTask.c:SocHandleDischarging()` | 已实现 |
| REQ-SOC-011 | 欠压保护触发时 SOC 强制归零 | ⚠️ | `SocTask.c:SocHandleDischarging()` | 已实现 |
| REQ-SOC-012 | SOC 平滑显示（步进速率限制） | 否 | `SocTask.c:SocApplySmoothSoc()` | 已实现 |
| REQ-SOC-013 | 显示 SOC 映射（底部 10% 截断） | 否 | `SocTask.c:SocCalcDisplaySoc()` | 已实现（见 G3） |
| REQ-SOC-014 | SOH 估算（低 SOC→满电容量测量法） | 否 | `SocTask.c:CalSOH()` | 已实现（见 S10） |
| REQ-SOC-015 | SOH 计算温度窗口限制 | 否 | `SocTask.c:SocIsSohTempValid()` | 已实现 |
| REQ-SOC-016 | 可放电量与可充电量计算 | 否 | `BMS_Info.h` | 存疑 |
| REQ-SOC-017 | 剩余放电时间估算 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-018 | 剩余充电时间估算 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-019 | 单体电压统计（最大/最小/极差/均值） | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-020 | 温度统计（最大/最小/极差/均值） | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现（见 Q3） |
| REQ-SOC-021 | 注入模式下的数据覆盖 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-022 | 间歇充电 SOC 阈值管理 | 否 | `SocTask.c:SocUpdateWaitSelfConsumption()` | 已实现 |
| REQ-SOC-023 | SOC/容量变化时持久化写 Flash | 否 | `SocTask.c:SocPersistIfChanged()` | 已实现 |
| REQ-SOC-024 | 额定容量与当前容量字段定义 | 否 | `BMS_Info.h`, `ParaSet.h` | 已实现 |

## PROT — 告警码与多级保护

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-PROT-001 | 告警多级体系架构（Level 0/1/3） | ⚠️ | `WarningTask.h` | 已实现 |
| REQ-PROT-002 | 故障分类与接触器保护动作（SYS/CHG/DISC） | ⚠️ | `WarningTask.c` | 已实现 |
| REQ-PROT-003 | 总压过高（L1 57.0V / L3 57.5V） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-004 | 总压过低（L1 45.0V / L3 42.0V） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-005 | 单体过高（L1 3700mV / L3 3750mV） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-006 | 单体过低（L1 2500mV / L3 2200mV） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-007 | 单体压差过高（平台 400mV / 非平台 1000mV） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-008 | 单体过温（L1 60℃ / L3 65℃） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-009 | 充电欠温（L1 5℃ / L3 0℃） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-010 | 放电欠温（L1 0℃ / L3 -10℃） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-011 | 充电过流（L1 30A / L3 110A，自锁 3 次） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-012 | 放电过流（L1 105A / L3 110A，L3 恢复 180s） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现（见 Q7） |
| REQ-PROT-013 | 二级放电过流（AFE OCD2 100A，自锁 3 次） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-014 | 短路保护（AFE SC 120A，自锁 3 次） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-015 | MOS 过温（L1 100℃ / L3 110℃） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-016 | MOS 温升过快（≥70℃ 温升，L3） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-017 | 环境过温（L1 55℃ / L3 60℃） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-018 | 环境欠温（放电时 L1 -10℃ / L3 -40℃） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-019 | SOC 过低（L3 0%，L1 映射代码已注释） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现（存疑） |
| REQ-PROT-020 | 热失控（MaxTemp>70℃ 且 ENVTemp>100℃） | ⚠️ | `WarningTask.c` | 已实现（存疑，见 S9） |
| REQ-PROT-021 | 进水检测（ADC≤3800，3s，L3） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现 |
| REQ-PROT-022 | 预充故障（L3，30s 自动恢复） | ⚠️ | `WarningTask.c`, `ParaSet.h` | 已实现（存疑） |
| REQ-PROT-023 | 充电 MOS 短路失效检测（L3） | ⚠️ | `WarningTask.c` | 已实现（存疑） |
| REQ-PROT-024 | 充电 MOS 断路失效检测（L3） | ⚠️ | `WarningTask.c` | 已实现（存疑） |
| REQ-PROT-025 | 放电 MOS 短路失效检测（L3） | ⚠️ | `WarningTask.c` | 已实现 |
| REQ-PROT-026 | 放电 MOS 断路失效检测（逻辑矛盾） | ⚠️ | `WarningTask.c` | 存疑（见 G1） |
| REQ-PROT-027 | 铁塔通信告警等级映射（TIETA 0~5） | 否 | `WarningTask.c` | 已实现 |
| REQ-PROT-028 | 低 SOC/低单体电压休眠触发（L2 门限） | ⚠️ | `WarningTask.c` | 已实现（存疑，见 G10） |
| REQ-PROT-029 | 充电限流功能（AppLimit 接口） | ⚠️ | `AppLimit.c/h`, `bsp_limit.h` | 已实现（存疑） |
| REQ-PROT-030 | 告警参数 Flash 持久化与默认值回退 | ⚠️ | `ParaSet.c` | 已实现 |
| REQ-PROT-031 | 温差过高保护（代码已注释，未激活） | ⚠️ | `WarningTask.c` | 缺口 |
| REQ-PROT-032 | 放电过温保护（代码已注释，由 CellTempHigh 代理） | ⚠️ | `WarningTask.c` | 缺口 |

## COMM — 上位机通信 / Modbus / 私有协议

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-COMM-001 | 双 RS-485 物理接口 | 否 | `usart.c` | 已实现 |
| REQ-COMM-002 | 通信端口动态选择 | 否 | `AppCom.c` | 已实现 |
| REQ-COMM-003 | 帧边界检测与最小长度过滤 | 否 | `AppCom.c`, `AppComLink.c` | 已实现 |
| REQ-COMM-004 | 私有协议帧格式与 CRC16 校验 | 否 | `AppComLink.c/.h` | 已实现 |
| REQ-COMM-005 | Modbus RTU 帧格式与 CRC16 校验 | 否 | `AppComLink.c`, `AppModbus.h` | 已实现 |
| REQ-COMM-006 | 接收缓冲区规格 | 否 | `AppComLink.h`, `bsp_usart.h` | 已实现 |
| REQ-COMM-007 | 双协议自动识别与路由 | 否 | `AppCom.c`, `AppComLink.c` | 已实现 |
| REQ-COMM-008 | Modbus 0x03 读保持寄存器 | 否 | `UpperComTask.c`, `AppModbus.c` | 已实现 |
| REQ-COMM-009 | Modbus 0x10 写多个寄存器 | ⚠️ | `UpperComTask.c`, `AppModbus.c` | 已实现 |
| REQ-COMM-010 | Modbus 寄存器地址映射 | 否 | `UpperComTask.h` | 已实现 |
| REQ-COMM-011 | 系统状态寄存器内容与编码 | 否 | `UpperComTask.c:CreatPackInfoBuf()` | 已实现（见 G4） |
| REQ-COMM-012 | 单体电压与温度上报编码 | 否 | `UpperComTask.c:CreatCellVoltBuf/TempBuf()` | 已实现 |
| REQ-COMM-013 | 上位机控制命令处理 | ⚠️ | `UpperComTask.c:CMD_Task()` | 已实现（见 G6） |
| REQ-COMM-014 | 远程强制接触器控制（充/放/预充 MOS） | ⚠️ | `UpperComTask.h/.c` | 存疑（见 G5） |
| REQ-COMM-015 | 数据注入功能（测试/标定模式） | ⚠️ | `UpperComTask.c:GetUpperFromTbl()` | 已实现 |
| REQ-COMM-016 | 日志读取二步协议（0x1A/0x1B） | 否 | `UpperComTask.c`, `AppModbus.c` | 已实现 |
| REQ-COMM-017 | 有效通信检测与外部通信活跃标志 | 否 | `AppTime.c`, `UpperComTask.c` | 已实现 |
| REQ-COMM-018 | Modbus 异常响应格式 | 否 | `AppModbus.c:ModbusSendErroAck()` | 已实现 |
| REQ-COMM-019 | 调试串口打印模式切换 | 否 | `AppCom.c` | 已实现 |
| REQ-COMM-020 | 电流偏置编码约定 | 否 | `UpperComTask.h/.c` | 已实现 |
| REQ-COMM-021 | 电流标定写入（寄存器 1541） | 否 | `UpperComTask.c:DealModbusWrite()` | 已实现 |
| REQ-COMM-022 | RTC 时间设置（通过状态区写入） | 否 | `UpperComTask.c:DealModbusWrite()` | 已实现 |
| REQ-COMM-023 | 升级会话承载（私有协议 OTA 通道） | ⚠️ | `AppCom.c`, `UpperComTask.c` | 已实现 |

## PARA — 参数管理 / 掉电存储 / Flash 映射

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-PARA-001 | 上电参数加载顺序 | ⚠️ | `ParaSet.c:initParameter()` | 已实现 |
| REQ-PARA-002 | 参数合法性校验（CheckPara） | ⚠️ | `ParaSet.c:CheckPara()` | 已实现（见 Q4） |
| REQ-PARA-003 | 单参数上下限范围校验 | ⚠️ | `ParaSet.c:initParameter()` | 已实现 |
| REQ-PARA-004 | 出厂默认参数表（SetDefaultPara） | ⚠️ | `ParaSet.c:SetDefaultPara()`, `ParaSet.h` | 已实现 |
| REQ-PARA-005 | 参数保存双区写入（SaveToFlash） | ⚠️ | `ParaSet.c:SaveToFlash()/SavePara()` | 已实现（见 Q5） |
| REQ-PARA-006 | 参数回读与自愈（ReadFlash） | ⚠️ | `ParaSet.c:ReadFlash()` | 已实现 |
| REQ-PARA-007 | 保存触发机制（needsave + 节流） | 否 | `ParaSet.c:ParaSaveTask()` | 已实现 |
| REQ-PARA-008 | SOC 掉电保存（环形追加 + 双区） | 否 | `ParaSet.c:SaveSoc()/ReadSoc()` | 已实现 |
| REQ-PARA-009 | 总充/放电量掉电保存（环形追加 + 双区） | 否 | `ParaSet.c:SaveChgcap()/SaveDisccap()` | 已实现 |
| REQ-PARA-010 | 容量（额定/当前）掉电保存（单点双区） | 否 | `ParaSet.c:SaveCap()/ReadCap()` | 已实现 |
| REQ-PARA-011 | 使能信息掉电保存（单点双区） | ⚠️ | `ParaSet.c:SaveEnabledInfo()/ReadEnabledInfo()` | 已实现 |
| REQ-PARA-012 | 设备地址掉电保存（单点双区） | 否 | `ParaSet.c:SaveDevAddr()/ReadDevAddr()` | 已实现 |
| REQ-PARA-013 | 校准值存储（单点双区，上电未加载） | 否 | `ParaSet.h`, `AppStorageMap.h` | 存疑（见 G7） |
| REQ-PARA-014 | Flash 分区布局与地址约束 | 否 | `ex_flash.h`, `AppStorageMap.h` | 已实现 |
| REQ-PARA-015 | Flash 驱动接口（SPI W25Qxx） | 否 | `ex_flash.h` | 已实现 |
| REQ-PARA-016 | 保护阈值参数来源（与 PROT 域交叉） | ⚠️ | `ParaSet.c:SetDefaultPara()` | 已实现 |

## LOG — Flash 日志 / SD 卡 CSV

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-LOG-001 | Flash 日志记录字段集 | 否 | `DataLog.h`, `DataLog.c:UpdateLogdata()` | 已实现 |
| REQ-LOG-002 | Flash 日志写入周期（20s 触发） | 否 | `main.c`, `AppTime.c` | 已实现 |
| REQ-LOG-003 | Flash 日志分区布局与容量 | 否 | `DataLog.c`, `ex_flash.h` | 已实现 |
| REQ-LOG-004 | Flash 日志初始化与断电续写 | 否 | `DataLog.c:LogInit()` | 已实现 |
| REQ-LOG-005 | Flash 日志环形覆盖策略 | 否 | `DataLog.c:SaveLogInIndex()` | 已实现 |
| REQ-LOG-006 | Flash 日志按时间段检索 | 否 | `DataLog.c:SearchIndex()` | 已实现（见 G2） |
| REQ-LOG-007 | SD 卡 CSV 写入周期（2s 触发） | 否 | `main.c`, `AppTime.c` | 已实现 |
| REQ-LOG-008 | SD 卡 CSV 文件按日期命名与自动创建 | 否 | `AppCsvLog.c:CSV_WriteData()` | 已实现 |
| REQ-LOG-009 | SD 卡 CSV 数据行字段内容（64 列） | 否 | `AppCsvLog.c`, `CSV_HEADER` | 已实现 |
| REQ-LOG-010 | SD 卡写入失败时的降级处理 | 否 | `AppCsvLog.c` | 存疑 |
| REQ-LOG-011 | 升级期间暂停日志写入 | ⚠️ | `main.c`, `UpperComTask.h` | 已实现 |
| REQ-LOG-012 | Flash 扇区分片写入与磨损管理 | 否 | `DataLog.c:SaveLogInIndex()` | 已实现 |
| REQ-LOG-013 | Flash 日志扇区位图持久化 | 否 | `DataLog.c:SaveSecMap()` | 已实现 |
| REQ-LOG-014 | SD 卡接口与文件系统初始化 | 否 | `main.c`, `mmc_sd.h` | 已实现 |
| REQ-LOG-015 | CSV 数据行缓冲区大小约束 | 否 | `AppCsvLog.c`（buffer[400]） | 存疑（见 Q6） |

## OTA — 固件升级

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-OTA-001 | 升级触发与通道 | 否 | `UpperComTask.c:UpperComUpgradeTask()` | 已实现 |
| REQ-OTA-002 | 升级请求帧长度与参数校验 | 否 | `upgrade.c:deal_ota_req()` | 已实现 |
| REQ-OTA-003 | 目标 Flash 分区擦除 | 否 | `upgrade.c:deal_ota_req()` | 已实现 |
| REQ-OTA-004 | 固件分包接收与顺序校验 | 否 | `upgrade.c:deal_rev_pkt()` | 已实现（见 G9） |
| REQ-OTA-005 | 通信帧 CRC-16/Modbus 校验 | 否 | `AppComLink.c:Is485RevUpgradeOK()` | 已实现 |
| REQ-OTA-006 | 升级完成后写升级标志并系统复位 | ⚠️ | `upgrade.c:deal_rev_pkt()` | 已实现 |
| REQ-OTA-007 | 升级超时检测与会话中止 | 否 | `upgrade.c:UpgradeOverTime()` | 已实现 |
| REQ-OTA-008 | 升级期间暂停日志写入 | 否 | `main.c`, `UpperCom_IsUpgrading()` | 已实现 |
| REQ-OTA-009 | 升级期间看门狗持续喂狗 | ⚠️ | `upgrade.c`, `USE_WDT` | 已实现 |
| REQ-OTA-010 | 版本信息读取 | 否 | `upgrade.c:read_para()` | 已实现 |
| REQ-OTA-011 | 升级目标设备选择（BCU/BMU） | 否 | `upgrade.c:deal_ota_req()` | 已实现 |
| REQ-OTA-012 | CAN 通道升级接口（仅声明未实现） | 否 | `upgrade.h` | 存疑 |
| REQ-OTA-013 | 升级完成时禁用全局中断 | ⚠️ | `upgrade.c:deal_rev_pkt()` | 已实现（见 S2） |

## HEAT — 加热控制

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-HEAT-001 | 加热模式选择（被动/主动） | 否 | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-002 | 被动加热——PCS 指令有效性判断 | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现（见 G11） |
| REQ-HEAT-003 | 被动充电加热——开/关阈值 | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-004 | 被动放电加热——开/关阈值 | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-005 | 被动模式——PCS 无请求时禁止加热 | 否 | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-006 | 主动加热——自主开启条件 | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现（见 S8） |
| REQ-HEAT-007 | 主动加热——自主关闭条件 | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-008 | 主动模式——放电时禁止加热（互锁） | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-009 | PTC 温度反馈——PWM 比较值自动调节 | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-010 | 加热输出变化时同步 heat_state | 否 | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-011 | 加热初始化 | 否 | `heatcontrol.c:HeatControlInit()` | 已实现 |
| REQ-HEAT-012 | 加热 PWM 频率与占空比可配置接口 | 否 | `heatcontrol.c`, `AppHeat.h` | 已实现 |
| REQ-HEAT-013 | 过温互锁——充电三级告警时强制关热 | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-014 | 过温互锁——放电三级告警时关热（被动） | ⚠️ | `heatcontrol.c:HeatControlTask()` | 已实现 |
| REQ-HEAT-015 | 温度点统计辅助函数 | 否 | `heatcontrol.c` | 已实现 |

## LED — 指示灯

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-LED-001 | 指示灯硬件映射（6 路枚举直映射） | 否 | `AppLed.h`, `bsp_led.h` | 已实现 |
| REQ-LED-002 | SOC 档位静态累加点亮 | 否 | `Led.c:LedSocTask()` | 已实现 |
| REQ-LED-003 | 充电时顶档 LED 500/500ms 闪烁 | 否 | `Led.c:LedSocTask()` | 已实现 |
| REQ-LED-004 | ALM 灯铁塔五级占空比映射 | ⚠️ | `Led.c:LedWarnTask()` | 已实现 |
| REQ-LED-005 | 铁塔告警等级综合优先级逻辑 | ⚠️ | `WarningTask.c:TieTaWarnTask()` | 已实现 |
| REQ-LED-006 | ALM 与 SOC 灯并存无覆盖 | 否 | `main.c`, `Led.c` | 已实现 |
| REQ-LED-007 | RUN 灯心跳（接口有但无调用） | 否 | `Led.h`, `Led.c` | 缺口（见 G12） |

## CHG — 充电管理

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-CHG-001 | 充电器接入检测——硬件标志与阈值 | ⚠️ | `ChargerLoad.h`, `AppAfe.c` | 已实现（驱动层，见 S7） |
| REQ-CHG-002 | 充电 MOS 开合——BMS 主态机控制 | ⚠️ | `BMS_Control.c` | 已实现 |
| REQ-CHG-003 | 满充判定三条件 AND 逻辑 | ⚠️ | `SocTask.c`, `ParaSet.h` | 已实现 |
| REQ-CHG-004 | 间歇充电 SOC 回差控制（100% 停 / 95% 再充） | ⚠️ | `SocTask.c`, `ParaSet.h` | 已实现 |
| REQ-CHG-005 | 充电限流电流阈值触发与恢复 | ⚠️ | `BMS_Control.c:Limit_Task()` | 已实现（见 S5） |
| REQ-CHG-006 | 充电限流与满充联动约束 | ⚠️ | `BMS_Control.c` | 已实现 |
| REQ-CHG-007 | 充电匹配 chgmatch_en——逻辑完全缺失 | ⚠️ | `BMS_Info.h`, `charger.c`（全注释） | 存疑（见 S6） |
| REQ-CHG-008 | 充电电流方向与状态判定 | 否 | `BMS_Info.c` | 已实现 |
| REQ-CHG-009 | 充电保护触发——充电 MOS 硬断 | ⚠️ | `WarningTask.c`, `BMS_Control.c` | 已实现 |
