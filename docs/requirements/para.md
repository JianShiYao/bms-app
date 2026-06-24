# REQ-PARA：参数管理 需求规格

> 覆盖源文件：
> - `Application/ParaSet.c`（~1553 行，核心）
> - `Application/ParaSet.h`
> - `Application/AppStorage.c` / `AppStorage.h`
> - `Application/AppStorageMap.h`
> - `Driver/BSP/ex_flash.h`

---

## 需求列表

---

### REQ-PARA-001  上电参数加载顺序

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `ParaSet.c:initParameter()` 行 728–790 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当系统上电执行 `initParameter()` 时，系统应按以下顺序加载持久化数据：
> 1. 读取额定容量/当前容量（`ReadCap()`）；
> 2. 读取使能信息——激活电芯数、激活温度传感器数（`ReadEnabledInfo()`）；
> 3. 读取主参数区（`ReadPara(ADDR_PARA_SAVE)`）；
> 4. 若主参数区校验失败，读取备份参数区（`ReadPara(ADDR_PARA_BAK)`）；
> 5. 若备份区也失败，调用 `SetDefaultPara()` 写入默认值，并立即保存到 Flash（`SaveCap()`、`SaveToFlash()`、`SaveEnabledInfo()`）。

**理由 / 代码依据**
> 保证上电后运行参数来自 Flash 有效备份或出厂默认值，避免使用未初始化内存。

**验收准则（可度量）**
- Given 主/备份参数区均被人为擦为 0xFF，When 系统上电，Then 系统应调用 `SetDefaultPara()` 并将默认值写回 Flash，全程不超过 1 次掉电周期（启动流程单次执行）。

---

### REQ-PARA-002  参数合法性校验（CheckPara）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `ParaSet.c:CheckPara()` 行 370–394 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当读取参数区后调用 `CheckPara()` 时，系统应通过检测以下字段为全 `0xFF`（未擦写）或全 `0x00`（异常零值）来判断参数无效：
> - `warn_time.TotalVoltHigh_1`
> - `warn_threshold.TotalVoltHigh_1`、`warn_threshold.SOCLow_1`
> - `warn_recover.SOCLow_1`、`warn_recover.TotalVoltHigh_1`
> - `recover_time.TotalVoltHigh_1`
>
> 若任一条件满足，函数返回 0（失败）；否则返回 1（成功）。

**理由 / 代码依据**
> 代码仅抽样检查 6 个字段（非全量校验），属于轻量合法性嗅探，而非 CRC/哈希完整性检验。

**验收准则（可度量）**
- Given `warn_threshold.TotalVoltHigh_1 == 0xFFFFFFFF`，When 调用 `CheckPara()`，Then 返回值 == 0。
- Given 所有被检字段均在合法范围（非 0、非 0xFF），When 调用 `CheckPara()`，Then 返回值 == 1。

---

### REQ-PARA-003  单参数上下限范围校验（initParameter 后处理）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `ParaSet.c:initParameter()` 行 759–773 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当 `initParameter()` 加载参数完成后，系统应对以下字段执行范围钳位：
> 1. 若 `PoweroffVolt_mV > 2000000 mV`（MAX_PWEROFF_MV），则置为默认值 `10000 mV`（DEFAULT_PWEROFF_MV）；
> 2. 若 `EmptyVoltmV > 3500 mV`（MAX_EMPTY_VOLT_mV）或 `EmptyVoltmV < 100 mV`（MIN_EMPTY_VOLT_mV），则置为默认值 `2300 mV`（DEF_EMPTY_VOLT_mV）；
> 3. 若 `ActiveIc > TOTAL_IC`，则钳位到 `TOTAL_IC`。

**理由 / 代码依据**
> 防止 Flash 内参数越界导致保护逻辑使用异常阈值（与 PROT 域交叉，参数值直接决定保护触发点）。

**验收准则（可度量）**
- Given `PoweroffVolt_mV = 3000000`（超上限），When `initParameter()` 执行完成，Then `Bms.Para.PoweroffVolt_mV == 10000`。
- Given `EmptyVoltmV = 50`（低于下限 100 mV），When `initParameter()` 执行完成，Then `Bms.Para.EmptyVoltmV == 2300`。

---

### REQ-PARA-004  出厂默认参数表（SetDefaultPara）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `ParaSet.c:SetDefaultPara()` 行 396–726；`ParaSet.h` 宏定义 |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当调用 `SetDefaultPara()` 时，系统应将运行参数初始化为以下出厂默认值（单位随字段）：

| 参数字段 | 默认值 | 单位 | 宏名 |
|---|---|---|---|
| RatedCap_mAH / CurrentCap_mAH | 100 000 | mAh | RATEDCAP_MAH |
| EmptyVoltmV（放空电压） | 2 300 | mV | DEF_EMPTY_VOLT_mV |
| PoweroffVolt_mV（低压关机） | 10 000 | mV | DEFAULT_PWEROFF_MV |
| chgthdmA（充电电流判断阈值） | 800 | mA | CHGTHRESHmA |
| dscthdmA（放电电流判断阈值） | 800 | mA | DISCTHRESHmA |
| BalaceConfig.MinTemp | 100 | 0.1℃ | DEFAULTBALANCEMINTEMP |
| BalaceConfig.MaxTemp | 500 | 0.1℃ | DEFAULTBALANCEMAXTEMP |
| BalaceConfig.StartVoltmV | 1 500 | mV | DEFAULTBALANCESTARTVOLT |
| BalaceConfig.StartDeltaVoltagemV | 300 | mV | DEFAULTBALANCESTARTDELTA |
| BalaceConfig.StopDeltaVoltagemV | 100 | mV | DEFAULTBALANCESSTOPDELTA |
| BalaceConfig.FloatTimems | 60 000 | ms（1 min） | DEFAULTBALANCEFLOATTIME |
| BalaceConfig.IntervalTimems | 300 000 | ms（5 min） | DEFAULTBALANCEINTTIME |
| BalaceConfig.enable | 1（开） | — | — |
| PreDiscVolt（预充故障电压差） | 2 000 | mV | Macro_PreErro_1 |
| PreDiscTime（预充超时） | 30 | ×100 ms = 3 s | Macro_PreErro_1_100ms |
| fullcellvoltmV（单体满充电压） | 3 600 | mV | Cell_FULL_VOLT_mV |
| fullTotalvoltmV（包总满充电压） | 56 500 | mV | Pack_FULL_VOLT_mV |
| fullCurmA（满充终止电流） | 5 000 | mA（0.1C） | ThdFullChargeCurrent |
| SecondOverCurCnt（二次过流重试次数） | 3 | 次 | DEFAULT_SCND_OCTIMES |
| DiscCurShortCnt（短路重试次数） | 3 | 次 | DEFAULT_SHORT_OCTIMES |
| IntermitterChgThd（间歇充电 SOC 阈值） | 95 | % | DEFAULT_INTERMITTERCHGTHD |
| SelfConsumption（每日自耗电） | 30 | ×0.1% / day | DEFAULT_SELFCONSUMPTION |
| HeatMode | 1（主动加热） | — | MACRO_HEATMODE |
| PassiveDiscHeatOff | −150 | 0.1℃ | MACRO_PASSIVE_DiscHEAT_OFF |
| PassiveChgHeatOff | 150 | 0.1℃ | MACRO_PASSIVE_ChgHEAT_OFF |
| ActiveHeatOn | 60 | 0.1℃ | MACRO_ACTIVE_HEAT_ON |
| ActiveHeatOff | 150 | 0.1℃ | MACRO_ACTIVE_HEAT_OFF |
| CellEnabled（默认激活电芯数） | 16 | 片 | — |
| chg_limiten（充电限流使能） | 1（开） | — | — |
| chg_limiten_th | 25 000 | mA | — |
| chg_limiten_recover | 15 000 | mA | — |
| chg_limiten_time_th | 30 | ×100 ms = 3 s | — |

> 保护告警阈值（warn_threshold / warn2_threshold）及恢复阈值（warn_recover / warn2_recover）默认值见下表（与 PROT 域交叉，本域仅列默认值，不展开保护逻辑）：

| 参数 | L1 | L2 | L3 | 单位 |
|---|---|---|---|---|
| CellVoltHigh | 3 700 | 3 720 | 3 750 | mV |
| CellVoltLow | 2 500 | 2 250 | 2 200 | mV |
| TotalVoltHigh | 57 000 | 57 300 | 57 500 | mV |
| TotalVoltLow | 45 000 | 43 500 | 42 000 | mV |
| DeltaVoltHigh | 100 | 150 | 400 | mV |
| DeltaVoltHigh2 | 100 | 150 | 1 000 | mV |
| ChgCurHigh | 30 000 | 32 000 | 110 000 | mA |
| DiscCurHigh | 105 000 | 107 000 | 110 000 | mA |
| ChgTempHigh | 450 | 500 | 520 | 0.1℃ |
| ChgTempLow | 50 | 20 | 0 | 0.1℃ |
| DiscTempHigh | 450 | 500 | 520 | 0.1℃ |
| DiscTempLow | 0 | −50 | −100 | 0.1℃ |
| DeltaTempTH | 50 | 70 | 90 | 0.1℃ |
| SOCLow | 2 | 1 | 0 | % |
| MOSTempHigh | 1 000 | 1 050 | 1 100 | 0.1℃ |
| EnvTempHigh | 550 | 570 | 600 | 0.1℃ |
| EnvTempLow | −100 | −150 | −400 | 0.1℃ |
| CellTempHigh | 600 | 620 | 650 | 0.1℃ |
| SecondOverCur | 100 000 | — | — | mA |
| DiscCurShort（短路） | 120 000 | — | — | mA |
| MOSTempRise（MOS 温升） | 700 | — | — | 0.1℃ |
| WaterCheck（水位检测） | 3 800 | — | — | mV（AD 值） |

> 所有告警延时默认值均为 20×100 ms = 2 s（过流/短路 L3 = 30×100 ms = 3 s；二次过流/短路 L3 = 0 ms，即即时触发）。

**验收准则（可度量）**
- Given Flash 参数区已损坏，When `initParameter()` 完成，Then `Bms.Para.EmptyVoltmV == 2300`、`warn_threshold.CellVoltHigh_3 == 3750`、`Bms.Para.HeatMode == 1`。

---

### REQ-PARA-005  参数保存双区写入（SaveToFlash / SavePara）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `ParaSet.c:SaveToFlash()` 行 792–796；`SavePara()` 行 301–340 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当触发参数保存时，系统应将参数同时写入主区（扇区 496，地址 `0x1F0000`）和备份区（扇区 497，地址 `0x1F1000`），写入内容包括：
> `warn_threshold`、`warn_recover`、`warn_time`、`warn2_threshold`、`warn2_recover`、`warn2_time`、`Bms.Para`、`recover_time`（按序无间隔紧密排列，总大小须 ≤ 4096 字节）。

**理由 / 代码依据**
> 双区冗余防止掉电发生在写操作中间导致单区损坏而丢失参数（`SaveToFlash()` 连续调用 `SavePara()` 两次）。参数结构体大小若超过 4096 字节将静默溢出到下一扇区。

**验收准则（可度量）**
- Given 参数已修改，When 调用 `SaveToFlash()`，Then 0x1F0000 与 0x1F1000 两扇区内容一致；
- Given 主区被手动擦除为 0xFF，When 再次 `initParameter()`，Then 可从备份区恢复所有参数。

---

### REQ-PARA-006  参数回读与自愈（ReadFlash）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `ParaSet.c:ReadFlash()` 行 798–820 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当调用 `ReadFlash()` 时，系统应按以下逻辑执行参数自愈：
> 1. 读主区 → 校验通过：将当前参数写回备份区（保持双区同步）；
> 2. 读主区 → 校验失败 → 读备份区 → 校验通过：将备份区内容写回主区（修复主区）；
> 3. 主区和备份区均失败：调用 `SetDefaultPara()`，并同时写入主区和备份区。

**验收准则（可度量）**
- Given 备份区有效、主区已损坏，When `ReadFlash()`，Then 主区被修复为与备份区相同内容。

---

### REQ-PARA-007  保存触发机制（needsave 标志位与节流）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `ParaSet.c:ParaSaveTask()` 行 1500–1552；`ParaSet.h` 枚举 SAVE_ENABLED/SAVE_WARN/SAVE_CAP/SAVE_ADDR |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当 `ParaSaveTask()` 被周期性调用时，系统应按以下规则执行保存：
> 1. **节流**：若 `Bms.needsave != 0`，且距上次节流起始时间不足 5000 ms，则本次不执行任何写操作；
> 2. 若 `Bms.needsave != 0` 且已超过 5 s 节流期，按优先级依次检查并执行（每次调用最多执行一类）：
>    - `BIT(SAVE_ENABLED=0)`：保存激活电芯/温感使能信息；
>    - `BIT(SAVE_WARN=1)`：保存告警/保护参数（`SaveToFlash()`）；
>    - `BIT(SAVE_CAP=2)`：保存额定容量/当前容量（`SaveCap()`）；
>    - `BIT(SAVE_ADDR=3)`：保存设备地址并重新配置 CAN（仅 `DEV_ADDR_SET_MODE` 宏开启时）；
> 3. 执行后清除对应 bit。

**理由 / 代码依据**
> 5 s 节流防止频繁 Flash 写入加速磨损；每次仅处理一类写操作减少单次阻塞时间。

**验收准则（可度量）**
- Given `Bms.needsave |= BIT(SAVE_WARN)` 在 t=0 设置，When `ParaSaveTask()` 在 t=3000 ms 调用，Then Flash 不写入；When 在 t=5001 ms 调用，Then Flash 写入完成，`Bms.needsave & BIT(SAVE_WARN) == 0`。

---

### REQ-PARA-008  SOC 掉电保存（环形追加 + 双区 + 变化才写）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `ParaSet.c:SaveSoc()` 行 983–1050；`ReadSoc()` 行 1054–1175 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应始终以环形追加方式保存 SOC：
> - 仅当 SOC 值发生变化时才执行写操作（静态变量 `oldsoc` 去重）；
> - 每条记录 16 字节（`SOC_SAVE_SIZE`），格式为：4 字节地址自引用 + 1 字节 SOC + 时间戳；
> - 主区（`0x1FD000–0x1FDFF0`，扇区 509）与备份区（`0x1FC000–0x1FCFF0`，扇区 508）同步写入，备份区地址 = 主区地址 − `0x1000`；
> - 当写指针超出主区上限时，擦除该扇区后从头写入（循环覆盖）。

**需求描述（EARS 句式）——读取**
> 当上电调用 `ReadSoc()` 时，系统应从末尾向前扫描 SOC 记录，找到第一条有效记录（SOC 合法、时间戳合法、地址自引用一致）；
> 若主区无效则尝试备份区；若两区均无有效记录，返回 0（失败，需重新查 OCV 表）；
> 若记录有效但距保存时刻超过 90 天（`MAX_BATTERY_STANDBY_TIME = 90×24×3600 s`），同样返回 0。

**验收准则（可度量）**
- Given SOC 连续写满一个扇区（256 条），When 写第 257 条，Then 主区扇区被擦除后从头写入；
- Given 上次保存 SOC 距今 91 天，When `ReadSoc()`，Then 返回 0（强制重查 OCV 表）。

---

### REQ-PARA-009  总充/放电量掉电保存（环形追加 + 双区）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `ParaSet.c:SaveChgcap()` / `ReadChgcap()` 行 1179–1336；`SaveDisccap()` / `ReadDisccap()` 行 1339–1498 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应以与 SOC 相同的环形追加 + 双区策略分别保存总充电量（`Bms.TotalCapOfChg`，mAh）和总放电量（`Bms.TotalCapOfDisc`，mAh）：
> - 总充电量：主区 `0x86000–0x86FF0`（扇区 134），备份区 `0x85000–0x85FF0`（扇区 133），记录尺寸 16 字节（`CHGCAP_SAVE_SIZE`）；
> - 总放电量：主区 `0x88000–0x88FF0`（扇区 136），备份区 `0x87000–0x87FF0`（扇区 135），记录尺寸 16 字节（`DISCCAP_SAVE_SIZE`）；
> - 仅当值发生变化时才写入（静态变量去重）；
> - 读取时不设时间超时限制（与 SOC 不同）。

**验收准则（可度量）**
- Given 充电量未变化（与上次相同），When `SaveChgcap()` 调用，Then 不发生 Flash 写操作。

---

### REQ-PARA-010  容量（额定容量/当前容量）掉电保存（单点双区）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `ParaSet.c:SaveCap()` / `ReadCap()` 行 200–298 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应将 `Bms.RatedCap_mAH` 和 `Bms.CurrentCap_mAH` 以固定单点写入主区（`0x1EB000`，扇区 491）和备份区（`0x1EA000`，扇区 490），前缀魔数 `0x5555`；
> 读取时若主区校验失败（魔数不符或值为 `0xFFFFFFFF` 或 `0`），则尝试备份区；若均失败，则置默认值 `100000 mAh` 并立即保存。

**验收准则（可度量）**
- Given 两区均为空白 Flash，When `ReadCap()`，Then `Bms.RatedCap_mAH == 100000`，`Bms.CurrentCap_mAH == 100000`。

---

### REQ-PARA-011  使能信息（激活电芯数/温感数）掉电保存（单点双区）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `ParaSet.c:SaveEnabledInfo()` / `ReadEnabledInfo()` 行 26–119 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应将 `Bms.CellEnabled`（激活电芯数）和 `Bms.TempEnabled`（激活温感数）保存至主区（`0x1E9000`，扇区 489）和备份区（`0x1E8000`，扇区 488），前缀魔数 `0xA55A`；
> 读取失败（两区均校验不通过）时，默认为 `CellEnabled=16`，`TempEnabled=4`，并立即写回 Flash。

**理由 / 代码依据**
> 激活电芯数直接决定电压采集范围和均衡覆盖，错误将导致遗漏检测（与 AFE/PROT 域交叉）。

**验收准则（可度量）**
- Given 两区均校验失败，When `ReadEnabledInfo()`，Then `Bms.CellEnabled == 16`，`Bms.TempEnabled == 4`，且写回 Flash 完成。

---

### REQ-PARA-012  设备地址（485/CAN 地址）掉电保存（单点双区）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `ParaSet.c:SaveDevAddr()` / `ReadDevAddr()` 行 122–196 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应将 `Bms.Address`（设备地址，uint16_t）保存至主区（`0x1E7000`，扇区 487）和备份区（`0x1E6000`，扇区 486），前缀魔数 `0x5555`；
> 读取失败时，地址置为 `0xFF`（广播地址兜底）。

**验收准则（可度量）**
- Given 两区均为 0xFF，When `ReadDevAddr()`，Then `Bms.Address == 0xFF`，返回 0x01（失败标志）。

---

### REQ-PARA-013  校准值（电压/电流偏移系数）存储（单点双区）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `ParaSet.h:__CORRECT_FACTOR_S`；`AppStorageMap.h` 地址 `0x1EE000`/`0x1ED000` |
| 验证方法 | 检视 |
| 状态 | 存疑 |

**需求描述（EARS 句式）**
> 系统应提供 `SaveCorrectOffset()` / `ReadCorrectOffset()` 接口，将 `CorrectPara`（含内置电压系数 `Intvolta0/1/2`、外置电压系数 `Extvolta0/1/2`、电流系数 `Currenta0/1/2`，均为 float 型）保存至主区（`0x1EE000`，扇区 494）和备份区（`0x1ED000`，扇区 493）。

**理由 / 代码依据**
> `initParameter()` 中 `ReadCorrectOffset()` 调用已被注释掉（行 735），校准系数实际未从 Flash 加载。结构体 `__CORRECT_FACTOR_S` 已定义，`SaveCorrectOffset/ReadCorrectOffset` 在 ParaSet.h 声明但实现未在本文件中找到（可能在其他文件或已裁剪）。

**验收准则（可度量）**
- Given 校准系数已写入 Flash，When `initParameter()` 调用，Then `CorrectPara` 应从 Flash 加载（当前为缺口/存疑）。

---

### REQ-PARA-014  Flash 分区布局与地址约束

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `Driver/BSP/ex_flash.h` 注释表；`Application/AppStorageMap.h` |
| 验证方法 | 分析 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应始终遵守以下外部 SPI Flash（W25Q16 系列，总容量 2 MB = `0x200000`）分区约束：

| 区域 | 地址范围 | 扇区号 | 用途 |
|---|---|---|---|
| 升级代码区 | `0x00000–0x4AFFF` | 0–74 | OTA 固件（300 KB） |
| 故障日志区 | `0x7E000–0x7FFFF` | 126–127 | 故障记录（8 KB） |
| 日志地址指针 | `0x80000–0x81FFF` | 128–129 | 故障记录写指针（主/备） |
| 总充电量备份 | `0x85000–0x85FF0` | 133 | 充电量备份 |
| 总充电量主区 | `0x86000–0x86FF0` | 134 | 充电量主区 |
| 总放电量备份 | `0x87000–0x87FF0` | 135 | 放电量备份 |
| 总放电量主区 | `0x88000–0x88FF0` | 136 | 放电量主区 |
| 设备地址备份 | `0x1E6000–0x1E6FFF` | 486 | 地址备份 |
| 设备地址主区 | `0x1E7000–0x1E7FFF` | 487 | 地址主区 |
| 使能信息备份 | `0x1E8000–0x1E8FFF` | 488 | 电芯/温感使能备份 |
| 使能信息主区 | `0x1E9000–0x1E9FFF` | 489 | 电芯/温感使能主区 |
| 容量备份 | `0x1EA000–0x1EAFFF` | 490 | 额定/当前容量备份 |
| 容量主区 | `0x1EB000–0x1EBFFF` | 491 | 额定/当前容量主区 |
| 校准值备份 | `0x1ED000–0x1EDFFF` | 493 | 电压电流校准备份 |
| 校准值主区 | `0x1EE000–0x1EEFFF` | 494 | 电压电流校准主区 |
| 参数主区 | `0x1F0000–0x1F0FFF` | 496 | BMS 参数主区（4 KB） |
| 参数备份区 | `0x1F1000–0x1F1FFF` | 497 | BMS 参数备份区（4 KB） |
| 异常数据区 | `0x1F3000–0x1F3FFF` | 499 | 掉电异常快照（已注释） |
| SOC 备份区 | `0x1FC000–0x1FCFF0` | 508 | SOC 环形备份 |
| SOC 主区 | `0x1FD000–0x1FDFF0` | 509 | SOC 环形主区 |
| 升级标志位 | `0x1FF000–0x1FFFFF` | 511 | OTA 标志 |
| 历史信息目录 | `0x200000` | 512 | 历史信息记录扇区使用情况 |
| 历史信息记录 | `0x201000–0x390FFF` | 513–912 | 历史信息（400×4 KB） |

> Flash 最大地址 `0x3FFFFF`，最大扇区号 1023（实际使用到 912）；扇区大小固定 4096 字节（`SEC_SIZE = 0x1000`）；页大小 256 字节（`PAGE_SIZE = 256`）。

**验收准则（可度量）**
- 各分区写操作前均先通过 `AppStorage_Erase()` 按扇区擦除；任何写操作的目标地址均不得跨扇区边界写入下一分区。

---

### REQ-PARA-015  Flash 驱动接口（SPI W25Qxx）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `Driver/BSP/ex_flash.h` 行 216–232 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应通过以下 SPI Flash 驱动接口访问外部 Flash，应用层不得直接操作 SPI 寄存器：
> - `SPI_Flash_Read(buf, addr, len)`：字节读取；
> - `SPI_Flash_Write(buf, addr, len)`：字节写入（内含读-擦-改-写逻辑）；
> - `SPI_Flash_Erase_Sector(addr)`：4 KB 扇区擦除；
> - `Flash_Erase_2516(addr, size)`：按 size 字节擦除多个扇区；
> - 应用层统一通过 `AppStorage_Read/Write/Erase()` 封装层调用。

**验收准则（可度量）**
- 所有上层存储调用均经过 `AppStorage_*` 函数路由，不直接调用 `SPI_Flash_*`。

---

### REQ-PARA-016  保护阈值参数来源（与 PROT 域交叉引用）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `ParaSet.c:SetDefaultPara()` 行 422–726；`ParaSet.h` Macro_* 系列宏 |
| 验证方法 | 分析 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应始终以 `warn_threshold`、`warn_recover`、`warn_time`（L1/L2/L3 三级）及 `warn2_threshold`、`warn2_recover`、`warn2_time` 全局结构体作为保护模块（PROT 域）的唯一阈值来源；这些结构体在 `SetDefaultPara()` 中赋初值，在 `SaveToFlash()/ReadPara()` 中完整读写（保护阈值不单独存储，与 `Bms.Para` 合并保存在同一扇区）。

**验收准则（可度量）**
- 修改 `warn_threshold` 中任一字段后调用 `SaveToFlash()`，下次上电 `ReadPara()` 后该字段应反映修改后的值（非默认值）。

---

## 存疑与观察

1. **校准系数未加载（缺口）**：`initParameter()` 中 `ReadCorrectOffset()` 已被注释（`ParaSet.c:735`），导致校准参数（电压/电流线性系数）在每次上电后均不从 Flash 恢复，实际效果相当于每次上电使用硬编码的单位系数（参见 `__CORRECT_FACTOR_S` 结构体）。如果生产中曾写入校准值，此注释会导致精度丢失。

2. **SaveErro / ReadErro 已全部注释（缺口）**：异常数据快照区（扇区 499，`0x1F3000`）已保留但对应代码全部被注释（`ParaSet.c:934–979`），掉电异常时无法留存完整 BMS 状态快照。与 LOG 域交叉：需确认是否有其他机制替代。

3. **CheckPara 仅抽样 6 字段**：`CheckPara()` 未对参数块做 CRC/哈希校验，仅靠抽查少数字段的特征值（0x00/0xFF）识别损坏。参数块中其他字段若被部分覆盖（如写操作中断），无法被发现。建议补充整块 CRC32 校验（目前标 `状态=已实现`，但完整性存疑）。

4. **参数总大小未做静态断言**：参数块（`warn_threshold + warn_recover + warn_time + warn2_threshold + warn2_recover + warn2_time + Bms.Para + recover_time`）写入 4096 字节单扇区（0x1F0000），无静态 `assert` 或运行期大小检查。若结构体扩展超出 4096 字节，将静默溢出覆盖相邻扇区（497，本身是备份区，会被立即覆盖）。

5. **SaveSoc 备份区地址计算依赖偏移假设**：`BAKwraddr = wraddr - 0x1000`（写入时）与 `SocSaveAddr - 0x1000`（读取时），硬编码主/备区相差 `0x1000`（即一个扇区）。当主区地址恰好为 `0x1FD000`（起始）时，备份计算地址为 `0x1FC000`，正好对应 SOC_ADDR_BAK，逻辑正确；但这个关系完全隐含在减法常量中，无显式断言，脆弱性较高。

6. **SaveChgcap 的 printf 误称"SOC"**（`ParaSet.c:1203`）：`printf("Erase SOC SEC\n")` 在 `SaveChgcap()` 中出现，为复制粘贴遗留的日志字符串错误，不影响功能但会干扰调试定位。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-PARA-001 | 上电参数加载顺序 | 是 ⚠️ | `ParaSet.c:initParameter()` | 已实现 |
| REQ-PARA-002 | 参数合法性校验（CheckPara） | 是 ⚠️ | `ParaSet.c:CheckPara()` | 已实现 |
| REQ-PARA-003 | 单参数上下限范围校验 | 是 ⚠️ | `ParaSet.c:initParameter()` | 已实现 |
| REQ-PARA-004 | 出厂默认参数表（SetDefaultPara） | 是 ⚠️ | `ParaSet.c:SetDefaultPara()` / `ParaSet.h` | 已实现 |
| REQ-PARA-005 | 参数保存双区写入（SaveToFlash） | 是 ⚠️ | `ParaSet.c:SaveToFlash()` / `SavePara()` | 已实现 |
| REQ-PARA-006 | 参数回读与自愈（ReadFlash） | 是 ⚠️ | `ParaSet.c:ReadFlash()` | 已实现 |
| REQ-PARA-007 | 保存触发机制（needsave + 节流） | 否 | `ParaSet.c:ParaSaveTask()` | 已实现 |
| REQ-PARA-008 | SOC 掉电保存（环形追加 + 双区） | 否 | `ParaSet.c:SaveSoc()` / `ReadSoc()` | 已实现 |
| REQ-PARA-009 | 总充/放电量掉电保存（环形追加 + 双区） | 否 | `ParaSet.c:SaveChgcap()` / `SaveDisccap()` | 已实现 |
| REQ-PARA-010 | 容量（额定/当前）掉电保存（单点双区） | 否 | `ParaSet.c:SaveCap()` / `ReadCap()` | 已实现 |
| REQ-PARA-011 | 使能信息掉电保存（单点双区） | 是 ⚠️ | `ParaSet.c:SaveEnabledInfo()` / `ReadEnabledInfo()` | 已实现 |
| REQ-PARA-012 | 设备地址掉电保存（单点双区） | 否 | `ParaSet.c:SaveDevAddr()` / `ReadDevAddr()` | 已实现 |
| REQ-PARA-013 | 校准值存储（单点双区） | 否 | `ParaSet.h:__CORRECT_FACTOR_S` / `AppStorageMap.h` | 存疑 |
| REQ-PARA-014 | Flash 分区布局与地址约束 | 否 | `ex_flash.h` / `AppStorageMap.h` | 已实现 |
| REQ-PARA-015 | Flash 驱动接口（SPI W25Qxx） | 否 | `Driver/BSP/ex_flash.h` | 已实现 |
| REQ-PARA-016 | 保护阈值参数来源（与 PROT 域交叉） | 是 ⚠️ | `ParaSet.c:SetDefaultPara()` | 已实现 |
