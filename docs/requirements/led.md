# REQ-LED：指示灯 需求规格

> 覆盖源文件：`Application/Led.c`、`Application/Led.h`、`Application/AppLed.c`、`Application/AppLed.h`、`Driver/BSP/bsp_led.h`、`Application/WarningTask.h`、`Application/WarningTask.c`（`TieTaWarnTask`）、`Core/Src/main.c`（主循环调度）

---

## 需求列表

### REQ-LED-001  指示灯硬件映射

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `AppLed.h:AppLedId_t`、`bsp_led.h:BspLedId_t`、`AppLed.c:AppLed_ToBspId()` |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应始终通过以下 6 路独立 LED 呈现 BMS 状态：SOC25（索引 0）、SOC50（索引 1）、SOC75（索引 2）、SOC100（索引 3）、RUN（索引 4）、ALM（索引 5）。应用层枚举 `AppLedId_t` 与 BSP 枚举 `BspLedId_t` 一一对应，直接强制类型转换（无映射表）。

**理由 / 代码依据**
> `AppLed_ToBspId()` 做 `(BspLedId_t)led`，要求两个枚举成员顺序完全一致；`AppLed_Write()` 最终调用 `BspLed_Write()`。

**验收准则（可度量）**
- Given 任何状态 When 调用 `AppLed_Write(APP_LED_SOC25, 1)` Then `BSP_LED_SOC25` 点亮。
- Given 枚举扩展 When 添加新成员时 Then 两个枚举必须同步修改。

---

### REQ-LED-002  SOC 档位显示——静态点亮规则

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `Led.c:LedSocTask()` L85–175 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在非充电状态（`Bms.sta != BMS_STA_CHARGING`）期间，系统应按 4 档累加点亮 SOC 灯：SOC ≤ 25% 时仅亮 SOC25；SOC 26–50% 时亮 SOC25 + SOC50；SOC 51–75% 时亮 SOC25 + SOC50 + SOC75；SOC 76–100% 时全亮 SOC25 + SOC50 + SOC75 + SOC100。

**理由 / 代码依据**
> `LedSocTask()` 以 `Bms.Soc <= 25 / 50 / 75 / 100` 四个区间判断，当前档 LED 常亮，低档 LED 全亮，高档 LED 全灭（累加条灯效果）。

**验收准则（可度量）**
- Given 非充电态 When Soc=1  Then SOC25=ON, SOC50=OFF, SOC75=OFF, SOC100=OFF。
- Given 非充电态 When Soc=50 Then SOC25=ON, SOC50=ON, SOC75=OFF, SOC100=OFF。
- Given 非充电态 When Soc=76 Then SOC25=ON, SOC50=ON, SOC75=ON, SOC100=ON。

---

### REQ-LED-003  SOC 档位显示——充电时顶档 LED 闪烁

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `Led.c:LedSocTask()` L87–174（`BMS_STA_CHARGING` 分支） |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在充电状态（`Bms.sta == BMS_STA_CHARGING`）期间，系统应使当前 SOC 档的顶部 LED 以 **500 ms ON / 500 ms OFF** 周期闪烁，其余低档 LED 保持常亮，高档 LED 保持熄灭。

**理由 / 代码依据**
> 使用系统毫秒计数 `u32SysTime % 1000 > 500` 实现 50% 占空比；四个 SOC 档均有相同逻辑（如 SOC ≤ 25 时 SOC25 灯闪，SOC 26–50 时 SOC25 常亮、SOC50 灯闪，依此类推）。

**验收准则（可度量）**
- Given 充电态且 Soc=30 When 连续 2 s 观测 Then SOC25=ON 常亮，SOC50 闪烁周期 1000 ms（ON 500 ms / OFF 500 ms），SOC75=OFF。
- Given 充电态且 Soc=100 When 观测 Then SOC25/50/75 常亮，SOC100 闪烁 500/500 ms。

---

### REQ-LED-004  ALM 告警灯——铁塔五级映射

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Led.c:LedWarnTask()` L196–225；`Led.c:WarnLedFlashing()` L178–194；`WarningTask.c:TieTaWarnTask()` L2141–2248 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应始终根据铁塔综合告警等级 `tieta_warn_level` 控制 ALM 灯，规则如下：

| tieta_warn_level | ALM 灯行为 | ON/OFF |
|---|---|---|
| LEVEL_0（正常） | 常灭 | — |
| LEVEL_1（一级预警） | 慢闪 | 250 ms ON / 4750 ms OFF（5 s 周期）|
| LEVEL_2（二级预警） | 慢中闪 | 500 ms ON / 2500 ms OFF（3 s 周期）|
| LEVEL_3（三级保护） | 中闪 | 500 ms ON / 1500 ms OFF（2 s 周期）|
| LEVEL_4（四级严重） | 快闪 | 250 ms ON / 250 ms OFF（0.5 s 周期）|
| LEVEL_5（五级故障） | 常亮 | — |

**理由 / 代码依据**
> `LedWarnTask()` switch 直接调用 `WarnLedFlashing(ontime, offtime)`；`WarnLedFlashing()` 以 `u32SysTime - timenow` 计算相对时间实现非对称占空比闪烁。告警等级 5 对应不可恢复故障（短路自锁超次、MOS 短路等）。

**验收准则（可度量）**
- Given tieta_warn_level=0 Then ALM 持续熄灭。
- Given tieta_warn_level=5 Then ALM 持续点亮。
- Given tieta_warn_level=4 Then ALM 250 ms 亮、250 ms 灭，误差 ≤ 10 ms。
- Given tieta_warn_level=1 Then 完整周期 5000 ms，ON 段 250 ms，OFF 段 4750 ms。

---

### REQ-LED-005  告警等级综合计算——铁塔优先级逻辑

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `WarningTask.c:TieTaWarnTask()` L2141–2248 |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应按优先级从高到低（LEVEL_5 最高）综合所有告警位，将最高告警等级输出到 `tieta_warn_level`：

- **LEVEL_5**：MOS 温升、各 MOS 短路/断路失效、AFE 通信/芯片故障、NTC 失效、短路自锁、充电过流次数超 `SecondOverCurCnt`。
- **LEVEL_4**：单体压差高（3 级）、单体温度过高（3 级）、环境温/低温（3 级）、AFE 断线、放电低温（3 级）、MOS 温度高（3 级）。
- **LEVEL_3**：充放电过流（3 级）、总压高/低（3 级）、单体高/低（3 级）、充电低温（3 级）、SOC 低（3 级）、进水。
- **LEVEL_2**：充放电过流一级告警（warn1code0）。
- **LEVEL_1**：电压/温度/SOC 一级告警（warn1code0）。
- **LEVEL_0**：无任何有效告警位。

**理由 / 代码依据**
> `TieTaWarnTask()` 通过 `WARN3CODE0_BIT`、`WARN3CODE1_BIT`、`WARN1CODE0_BIT` 宏逐级检查，以 `if-else if` 优先级链赋值 `tieta_warn_level`。

**验收准则（可度量）**
- Given 同时触发 LEVEL_4 和 LEVEL_3 条件 Then tieta_warn_level = 4。
- Given LEVEL_5 条件中任一位置位 Then tieta_warn_level = 5，ALM 常亮。

---

### REQ-LED-006  ALM 灯与 SOC 灯并存——无覆盖关系

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `Core/Src/main.c` L228–229；`Led.c:LedSocTask()`、`Led.c:LedWarnTask()` |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在主循环中先执行 `LedSocTask()`、后执行 `LedWarnTask()`；两者分别控制不同 LED（SOC25/50/75/100 vs. ALM），互不干扰。告警状态不关闭 SOC 灯，SOC 灯也不影响 ALM 灯。

**理由 / 代码依据**
> `LedSocTask()` 只操作 `APP_LED_SOC*`，`LedWarnTask()` 只操作 `APP_LED_ALM`，物理上为独立 LED，逻辑上无优先级覆盖机制。

**验收准则（可度量）**
- Given Soc=80% 且 tieta_warn_level=5 Then SOC25/50/75/100 全亮（非充电），ALM 常亮，两组灯同时激活。

---

### REQ-LED-007  RUN 灯——未在任务调度中驱动（缺口）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `Led.h:Led_Run_On/Off()`、`Led.c:Led_Run_On/Off()`；`Core/Src/main.c` 未见调用 |
| 验证方法 | 检视 |
| 状态 | 缺口 |

**需求描述（EARS 句式）**
> 系统应（预期）使用 RUN 灯指示系统运行状态（如心跳闪烁），但当前代码未在主循环或任何周期任务中调用 `Led_Run_On()` / `Led_Run_Off()`。

**理由 / 代码依据**
> `Led.h` 和 `Led.c` 提供了 `APP_LED_RUN` 的驱动接口（索引 4），但全局搜索未发现任何调用点，属于已定义但未使用的硬件资源。

**验收准则（可度量）**
- 需补充具体 RUN 灯闪烁规格后验证。

---

## 存疑与观察

1. **`WarnLedFlashing()` 静态计时变量竞争**：`static uint32_t timenow` 仅有一个实例，所有等级闪烁共用同一计时起点。在等级从 L3 切换到 L4 时，`timenow` 不会重置，会导致第一个闪烁周期长度不准确。`状态=存疑`（`Led.c:WarnLedFlashing()` L179–193）。

2. **告警等级枚举注释全部写"一级"**：`WarningTask.h` L9–23 中 `TIETA_WARNING_LEVLE_1` 至 `TIETA_WARNING_LEVLE_5` 的注释均为 `/* 一级 */`，应为各自等级，属于注释笔误，不影响运行逻辑。

3. **RUN 灯硬件存在但无任何驱动调用**（见 REQ-LED-007），属于功能缺口，需补充运行心跳灯需求。

4. **LedSocTask 中 SOC=0 情形**：代码四个区间为 `≤25 / ≤50 / ≤75 / ≤100`，SOC=0 时进入第一分支（SOC25 按充电状态闪烁/常亮），但此时可能正在 BMS_STA_PROTECT 保护状态，LedSocTask 没有 protect 状态的特殊处理，所有灯仍按 SOC 显示，可能引起误解。`状态=存疑`。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-LED-001 | 指示灯硬件映射（6 路 LED 枚举直映射） | 否 | `AppLed.h`、`bsp_led.h`、`AppLed.c` | 已实现 |
| REQ-LED-002 | SOC 档位显示——静态累加点亮 | 否 | `Led.c:LedSocTask()` | 已实现 |
| REQ-LED-003 | SOC 档位显示——充电时顶档 500/500 ms 闪烁 | 否 | `Led.c:LedSocTask()` | 已实现 |
| REQ-LED-004 | ALM 告警灯——铁塔五级占空比映射 | 是 ⚠️ | `Led.c:LedWarnTask()`、`Led.c:WarnLedFlashing()` | 已实现 |
| REQ-LED-005 | 告警等级综合计算——铁塔优先级逻辑 | 是 ⚠️ | `WarningTask.c:TieTaWarnTask()` | 已实现 |
| REQ-LED-006 | ALM 灯与 SOC 灯并存、无覆盖关系 | 否 | `main.c`、`Led.c` | 已实现 |
| REQ-LED-007 | RUN 灯心跳（缺口：接口已有但无调用） | 否 | `Led.h`、`Led.c` | 缺口 |
