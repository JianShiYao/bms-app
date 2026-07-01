<!--
  S16100B 需求提炼 · SOC 域
  逆向提炼自固件源码（描述"代码当前做了什么"），状态均为「已实现」，
  有疑问或缺口处单独标注。
-->

# REQ-SOC：电量估算与容量管理 需求规格

> 覆盖源文件：
> - `Application/SocTask.c`（SOC 主逻辑，含库仑积分、SOH 计算、充放电修正、平滑显示）
> - `Application/SocTask.h`（锚点/温补/SOH 启动宏定义）
> - `Application/BMS_Info.c`（`BmsinfoStatistics()`：统计量计算、剩余时间估算）
> - `Application/BMS_Info.h`（`BMS_PARA_S` 数据结构定义）
> - `Driver/AFE/Calculate.h`（`AfeCalcResult_t` 采集结果结构体）
> - `Application/ParaSet.h`（默认参数宏：额定容量、自耗电、充满阈值等）

---

## 需求列表

### REQ-SOC-001  上电 OCV 初始化 SOC

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocInitIfNeeded()`，第 507–538 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当系统上电后 500 个 `u32SysTime` 计数（约 500 ms × 任务调度周期）后首次进入 SOC 任务时，系统应通过 OCV–温度二维查表（`FloatChargeOCVTbl`，21 × 8）将当前平均单体电压与平均温度映射为理论 SOC，并用该值作为初始 SOC（仅当 Flash 未读到有效存储值时覆盖）；同时从 Flash 恢复历史累计充放电量（`TotalCapOfChg`/`TotalCapOfDisc`）及当前容量（`CurrentCap_mAH`），并同步刷新 `RemainEnergy_mAH`、`Soh`、DisplaySoc。

**理由 / 代码依据**
> `SocInitIfNeeded()` 检查 `soc_init_flag`，延时 500 计数后执行一次；调用 `GetSocFromOCVTbl()` 做 OCV 查表，`ReadSoc()` 返回 0 时才用查表结果覆盖。充放电量、容量由 `ReadChgcap()`/`ReadDisccap()`/`ReadCap()` 读取。

**验收准则（可度量）**
- Given 上电静置状态（非充放电），When 系统运行 ≥500 ms，Then `Bms.Soc` 与 OCV 查表结果的偏差 ≤ 5%SOC（一个表格步长）。
- Given Flash 有有效存储 SOC，When 上电初始化，Then `Bms.Soc` 保留 Flash 读取值，不被 OCV 值覆盖。

---

### REQ-SOC-002  OCV 查表仅在静置状态执行

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:GetSocFromOCVTbl()`，第 88–136 行 |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在 BMS 处于充电（`BMS_STA_CHARGING`）或放电（`BMS_STA_DISCHARGING`）状态期间，系统应直接返回当前 `Soc`，不做 OCV 查表更新；仅在其他状态（待机、静置等）下才执行 OCV–温度二维插值。

**理由 / 代码依据**
> `GetSocFromOCVTbl()` 的 `switch(bms_state)` 对 `CHARGING`/`DISCHARGING` 直接 `return Soc`，否则执行温度索引+电压索引的双层查表。

**验收准则（可度量）**
- Given `Bms.sta == BMS_STA_CHARGING`，When 调用 `GetSocFromOCVTbl()`，Then 返回值 == 入参 `Soc`，无查表运算。

---

### REQ-SOC-003  库仑积分（带温度补偿）更新剩余电量

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocUpdateCoulombCount()`，第 562–611 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在每次 SOC 任务周期内，将采集电流（`Bms.current_mA`，充正放负，mA）乘以经过时间（`timcap`，ms）得到原始增量（mAms），扣除自耗电分量（`SelfConsumption × CurrentCap_mAH / 24 / 1000 × timcap` mAms），再乘以温度补偿增益（`SocGetTempCompSocGainPermille()` / 1000，范围 940‰–1160‰），累加到温补积分器 `cap_mAms`；当积分器绝对值超过 `SOC_ACCUM_TRIGGER_mAms`（36 000 000 mAms = 10 mAh × 3600 s）时以 1 mAh 为步长更新 `RemainEnergy_mAH`，余量留在积分器。

**理由 / 代码依据**
> `raw_delta_mAms` 为原始库仑；减去自耗电后经 `SocApplyTempGainToDelta()` 得 `soc_delta_mAms`；分两路积分：`raw_cap_mAms` 用于 SOH/历史统计，`cap_mAms` 用于 SOC；触发阈值 `SOC_ACCUM_TRIGGER_mAms = 36000000L`；步长 `SOC_STEP_CAP_mAms = 3600000L` 即 1 mAh。

**验收准则（可度量）**
- Given 恒流 1 000 mA 放电 36 s（= 10 mAh），When SOC 任务持续运行，Then `RemainEnergy_mAH` 减少 10 mAh（误差 < 1 mAh）。
- Given 平均温度 = 15℃（250 + (15-25)×10 = 250-100 = 150，单位 0.1℃），When 温度补偿增益计算，Then gain = 1000 + 100×20/50 = 1040‰（在 940‰–1160‰ 范围内）。

---

### REQ-SOC-004  温度补偿增益夹紧

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocGetTempCompSocGainPermille()`，第 329–356 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应始终将温度补偿增益限制在 940‰–1160‰ 范围内：以 25℃（`SOC_TEMP_REF_0P1C = 250`，0.1℃）为基准，每低 5℃ 增益 +20‰（低温放大库仑），每高 5℃ 增益 -10‰（高温缩小库仑）；超出边界时夹紧至边界值。

**理由 / 代码依据**
> 宏 `SOC_GAIN_MIN_PERMILLE = 940`，`SOC_GAIN_MAX_PERMILLE = 1160`，`SOC_GAIN_COLD_PER_5C = 20`，`SOC_GAIN_HOT_PER_5C = 10`。

**验收准则（可度量）**
- Given `AverageTemp = -300`（-30℃），When 计算增益，Then gain = min(1000 + 550×20/50, 1160) = min(1220, 1160) = 1160‰。
- Given `AverageTemp = 600`（60℃），When 计算增益，Then gain = max(1000 - 350×10/50, 940) = max(930, 940) = 940‰。

---

### REQ-SOC-005  自耗电扣除

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocUpdateCoulombCount()`，第 573 行；`ParaSet.h` 第 327 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在库仑积分中持续扣除系统自耗电：自耗电等效电流 = `SelfConsumption（默认 30，单位 0.1%/天）× CurrentCap_mAH / 24 / 1000` mA，在每个积分周期内扣减相应 mAms 以补偿静置自放电。

**理由 / 代码依据**
> 代码第 573 行：`raw_delta_mAms -= Bms.Para.SelfConsumption * Bms.CurrentCap_mAH / 24 / 1000;`（注释说明此行单位计算）；`DEFAULT_SELFCONSUMPTION = 30`，即每天 3% 自耗。

**验收准则（可度量）**
- Given `SelfConsumption = 30`，`CurrentCap_mAH = 100000 mAh`，静置 24 小时，Then 自耗扣减量 = 30 × 100000 / 24 / 1000 × 24 × 3600 × 1000 ms / 3600000 = 3000 mAh（3%）。

---

### REQ-SOC-006  历史累计充放电量统计

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocUpdateCoulombCount()`，第 581–594 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应基于原始库仑积分（`raw_cap_mAms`，不含温补）累计历史充放电量：当积分绝对值超过 `SOC_ACCUM_TRIGGER_mAms`（36 000 000 mAms）时，正方向（充电）加入 `Bms.TotalCapOfChg`，负方向（放电）累加 `ABS` 值到 `Bms.TotalCapOfDisc`，单位 mAh，步长 10 mAh；SOC 变化时持久化到 Flash。

**理由 / 代码依据**
> `cap_mAH = ctx->raw_cap_mAms / SOC_STEP_CAP_mAms`（1 mAh/步）；当 `raw_cap_mAms > 36000000` 时触发；正负分别累加 `TotalCapOfChg`/`TotalCapOfDisc`。`SocPersistIfChanged()` 在 SOC 改变时调 `SaveChgcap()`/`SaveDisccap()`。

**验收准则（可度量）**
- Given 充电 1 A 持续 1 h，Then `TotalCapOfChg` 增量 ≈ 1000 mAh（误差 < 10 mAh，由步长决定）。
- Given SOC 发生变化，When `SocPersistIfChanged()` 执行，Then `TotalCapOfChg` 和 `TotalCapOfDisc` 均被写入 Flash。

---

### REQ-SOC-007  循环次数计算

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocPersistIfChanged()`，第 1003 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在每次 SOC 变化后实时计算循环次数：`CycleTimes = TotalCapOfDisc / CurrentCap_mAH`（等效为总放电量除以当前满电容量，单位：次）。

**理由 / 代码依据**
> `Bms.CycleTimes = Bms.TotalCapOfDisc / Bms.CurrentCap_mAH;` 位于 `SocPersistIfChanged()` 中，每次 SOC 变化后更新。`CycleTimes` 类型为 `uint16_t`，最大 65535 次。

**验收准则（可度量）**
- Given `TotalCapOfDisc = 100000 mAh`，`CurrentCap_mAH = 100000 mAh`，Then `CycleTimes = 1`。
- Given `TotalCapOfDisc = 5000000 mAh`，Then `CycleTimes = 50`。

---

### REQ-SOC-008  SOC 从剩余电量计算（1% 精度）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:CalSocFunc()`，第 75–85 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应通过 `RemainEnergy_mAH × 1000 / CurrentCap_mAH`（内部精度 0.1%），四舍五入后得到 1% 精度的 SOC（`+5` 后 `/10`），结果上限夹紧至 120（内部运算上界，用于检测过充异常）。

**理由 / 代码依据**
> `soc = Bms.RemainEnergy_mAH*1000/Bms.CurrentCap_mAH; soc += 5; soc /= 10; if (soc > 120) soc = 120;`。内部精度 ×10（0.1%），输出为 1%。

**验收准则（可度量）**
- Given `RemainEnergy = 50050 mAh`，`CurrentCap = 100000 mAh`，Then CalSocFunc = 50（`50050×1000/100000=500`，`+5=505`，`/10=50`）。
- Given 计算结果 > 120，Then 返回 120。

---

### REQ-SOC-009  充电状态 SOC 修正（充满检测与拉升）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocHandleCharging()`，第 668–720 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在充电状态期间：
> 1. 当单体最高电压 ≥ `SOC_FORCE_FULL_CELL_mV`（3650 mV）时，系统应强制将 SOC 设为 100%；
> 2. 当满足"实际充满"条件（`SocChargeFullConditionMet()`：最高单体电压 ≥ `fullcellvoltmV` 或平均电压 ≥ `fullcellvoltmV` 或总压 ≥ `fullTotalvoltmV`，且充电电流 ≤ `fullCurmA`）但库仑 SOC < 100% 时，系统应每 `CORRECT_TIME_ms`（1000 ms）递增 1%（约 1%/s 速率拉升至 100%）；
> 3. 当库仑 SOC ≥ 100% 但尚未检测到充满条件时，SOC 应锁定在 99%。

**理由 / 代码依据**
> `SocHandleCharging()` 三条分支分别对应强制满电（硬电压判断）、实际满电拉升（`SocIncreaseOnePercent()`，1%/s）、以及 99% 保持。`CORRECT_TIME_ms = 1000`。

**验收准则（可度量）**
- Given `CellMaxVoltage_mV = 3650 mV`，When 处于充电状态，Then `Bms.Soc = 100`，在 1 个任务周期内生效。
- Given 满足充满条件但库仑 SOC = 97，Then SOC 每 1 s 递增 1%，约 3 s 后到达 100%。
- Given 库仑 SOC = 100 但单体最高电压 < 3650 mV 且未满足充满条件，Then `Bms.Soc = 99`，不显示 100%。

---

### REQ-SOC-010  放电状态 SOC 修正（低电锚点与防拉高）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocHandleDischarging()`，第 839–951 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在放电状态期间，系统应根据单体最低电压与两个温度修正锚点（`anchor1`≈5%SOC；`anchor2`≈10%SOC）执行三段式 SOC 下修正：
> - 单体最低电压 > anchor2 时：SOC 显示允许向上平滑，但不允许库仑 SOC 低于 10% 时拉低 SOC（防下冲）；
> - 单体最低电压介于 anchor1 与 anchor2 之间时：SOC 不允许超过 10%，若库仑 SOC > 10% 则每 1 s 递减 1%；
> - 单体最低电压 ≤ anchor1 时：SOC 不允许超过 5%，若仍高于 5% 则每 1 s 递减 1%；抵达温补空电电压（`empty_volt_mV`）时强制 SOC = 0。

**理由 / 代码依据**
> `SocHandleDischarging()` 中三段 `if` 分支；`anchor1 = SocGetLowSocAnchor1()`（基于 `Bat_1_PRECET_VOLT_mV = 2800 mV` 温补）；`anchor2 = SocGetLowSocAnchor2()`（基于 `Bat_2_PRECET_VOLT_mV = 3000 mV` 温补）；`SocDecreaseOnePercent()` 每 1 s 降 1%。

**验收准则（可度量）**
- Given 放电中单体最低电压穿越 anchor2（约 3000 mV 温补），When SOC = 15%，Then SOC 在锚点区开始每 1 s 递减 1%，直到 ≤ 10%。
- Given 单体最低电压 ≤ `empty_volt_mV`（温补后值），Then `Bms.Soc = 0`，`RemainEnergy_mAH = 0`，在 1 个任务周期内生效。

---

### REQ-SOC-011  欠压保护触发时 SOC 强制归零

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `SocTask.c:SocHandleDischarging()`，第 851–857 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当放电状态下三级欠压警告（`warn_level.CellVoltLow == WARNING_LEVLE_3`）触发时，系统应立即将 `Bms.Soc = 0`，`Bms.RemainEnergy_mAH = 0`，不执行平滑过渡。

**理由 / 代码依据**
> 放电修正函数最高优先级分支：`if (warn_level.CellVoltLow == WARNING_LEVLE_3)` 直接赋零，绕过所有锚点判断。注意：此处 SOC 归零是**信息上报**行为，接触器断开由保护域（PROT）独立控制；SOC 本身不直接触发保护动作。

**验收准则（可度量）**
- Given `warn_level.CellVoltLow == WARNING_LEVLE_3`，When 处于放电状态执行 SOC 修正，Then `Bms.Soc == 0`，`RemainEnergy_mAH == 0`，在同一任务调用内完成。

---

### REQ-SOC-012  SOC 平滑显示（步进速率限制）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocApplySmoothSoc()`，第 462–504 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在常规状态（非强制赋值）下，系统应对 SOC 变化进行平滑处理：每 `SOC_SMOOTH_STEP_ms`（500 ms）最多变化 1%，即最大变化速率 2%/s；强制赋值（`force_apply != 0`）时跳过平滑直接到达目标值。

**理由 / 代码依据**
> `SocApplySmoothSoc()` 中 `smooth_time_accum` 累加时间，每 500 ms 步进 1 个 `steps`；`force_apply` 为非零时直接返回 `targetSoc`。`SOC_SMOOTH_STEP_ms = 500`。

**验收准则（可度量）**
- Given 目标 SOC 与当前 SOC 差值 = 10%，When 经过 5 s（10 个步进周期），Then `Bms.Soc` 恰好到达目标值。
- Given `force_apply = TRUE`，When 调用，Then 一个周期内 SOC 直接等于目标值。

---

### REQ-SOC-013  显示 SOC 映射（底部 10% 截断）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocCalcDisplaySoc()`，第 299–320 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应将内部 SOC（0–100%）映射为对外显示 SOC：内部 SOC ≤ 10% 时显示 0%；内部 SOC ≥ 100% 时显示 100%；10%~100% 线性映射到 0%~99%（公式：`display = (soc - 10) × 99 / 89`，+44 四舍五入）。

**理由 / 代码依据**
> `SocCalcDisplaySoc()` 中 `display_soc = ((uint32_t)(soc - 10) * 99U + 44U) / 89U`，结果上限 99。注意：`SocRefreshDisplaySoc()` 调用了该函数但**未将返回值写回任何字段**（见存疑条目 Q-1）。

**验收准则（可度量）**
- Given `Bms.Soc = 10`，Then DisplaySoc = 0。
- Given `Bms.Soc = 55`，Then DisplaySoc = `(55-10)×99/89 ≈ 50`。
- Given `Bms.Soc = 100`，Then DisplaySoc = 100。

---

### REQ-SOC-014  SOH 估算（从低 SOC 到满电的容量测量法）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:CalSOH()`，第 173–261 行；`SocTask.c:SocRefreshSohByCapacity()`，第 288–296 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应执行基于充电容量测量的 SOH 估算：
> 1. **启动条件**：处于充电状态，且 SOC ≤ `SOH_START_MAX_SOC`（10%），且单体最低电压 ≤ `SOH_START_MAX_CELLV_mV`（3050 mV），且平均温度在 `SOH_TEMP_MIN_0P1C`（15℃）–`SOH_TEMP_MAX_0P1C`（35℃） 之间；
> 2. **充电计量**：从启动 SOC 开始累积原始充电 mAhs（每 10 mAh 步进，放电 < 100 mAh 时忽略放电量）；
> 3. **结束判定**：单体最高电压 ≥ `SOC_FORCE_FULL_CELL_mV`（3650 mV）或满足充满条件；
> 4. **容量更新**：折算得到 `newSohCap = 测量充电量 × 100 / (100 - StartSoc)`；仅当 `newSohCap < CurrentCap × 0.97` 且 `newSohCap ≠ 0` 时更新 `CurrentCap_mAH` 并写入 Flash；
> 5. **SOH 刷新**：`Soh = CurrentCap_mAH × 100 / RatedCap_mAH`（1% 精度）。

**理由 / 代码依据**
> `CalSOH()` 状态机 `SOH_NONE → SOH_START → SOH_COMPLETE`；异常退出（非充电、温度越界）回到 `SOH_NONE`；只有容量下降 ≥ 3% 才更新，防止误跳。`SocRefreshSohByCapacity()` 用 0.1% 内部精度（`×1000/RatedCap`，+5 四舍五入）。

**验收准则（可度量）**
- Given 电池老化后充电测量 95 000 mAh，`RatedCap = 100 000 mAh`，When 满足所有启动条件且到达充满，Then `CurrentCap_mAH` 更新为 95 238 mAh（折算），`Soh = 95`。
- Given `newSohCap = 98 000 mAh`（< 旧 `CurrentCap × 0.97 = 97 000 mAh` 不成立），Then `CurrentCap_mAH` 不更新（仅下降时更新）。

---

### REQ-SOC-015  SOH 计算温度窗口限制

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocIsSohTempValid()`，第 139–150 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应仅在平均温度处于 15℃–35℃（`SOH_TEMP_MIN_0P1C = 150`，`SOH_TEMP_MAX_0P1C = 350`，单位 0.1℃）范围内时允许 SOH 计算进行或继续；超出温度窗口时 SOH 计算状态应立即复位到 `SOH_NONE`。

**理由 / 代码依据**
> `SocIsSohTempValid()` 检查 `Bms.AverageTemp`；`CalSOH()` 在 `SOH_START` 状态每次调用均检查温度有效性，失败则 `SohCalFlag = SOH_NONE`。

**验收准则（可度量）**
- Given SOH 正在计算中（`SOH_START`），当 `AverageTemp` 降至 140（14℃），Then SOH 状态在下一次调用中复位，累积容量丢弃。

---

### REQ-SOC-016  可放电量与可充电量计算

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.h`，第 184–185 行（字段定义）；需结合 `UpperComTask.c` 查赋值 |
| 验证方法 | 检视 |
| 状态 | 存疑 |

**需求描述（EARS 句式）**
> 系统应维护 `Bms.CapToDisc_mAH`（可放电量，mAh）和 `Bms.CapToChg_mAH`（可充电量，mAh），用于上报给上位机；可放电量 = `RemainEnergy_mAH`，可充电量 = `CurrentCap_mAH - RemainEnergy_mAH`。

**理由 / 代码依据**
> `BMS_PARA_S` 中字段已定义，`BMS_Info.c` 中未见赋值逻辑（可能在 `UpperComTask.c` 或其他位置），来源待确认。

**验收准则（可度量）**
- Given `RemainEnergy = 30000 mAh`，`CurrentCap = 100000 mAh`，Then `CapToDisc = 30000 mAh`，`CapToChg = 70000 mAh`。

---

### REQ-SOC-017  剩余放电时间估算

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.c:BmsinfoStatistics()`，第 51–53 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在系统运行 80 s（`u32SysTime > 80000`）后，当处于放电状态时，系统应估算剩余放电时间：`RemainDscTime_m = RemainEnergy_mAH × 60 / ABS(avg_current_mA)` 分钟；同时清零 `RemainChgTime_m`。

**理由 / 代码依据**
> 使用 `avg_current_mA`（平均电流）而非瞬时电流，减少估算抖动；80 s 启动延时确保电流已稳定。

**验收准则（可度量）**
- Given `RemainEnergy = 50000 mAh`，`avg_current_mA = -10000 mA`（放电），When 处于放电状态且 `u32SysTime > 80000`，Then `RemainDscTime_m = 50000×60/10000 = 300 min`（5 h）。
- Given 放电状态，Then `RemainChgTime_m = 0`。

---

### REQ-SOC-018  剩余充电时间估算

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.c:BmsinfoStatistics()`，第 55–59 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在系统运行 80 s 后，当处于充电状态时，系统应估算剩余充电时间：`RemainChgTime_m = (CurrentCap_mAH - RemainEnergy_mAH) × 60 / ABS(avg_current_mA)` 分钟；同时清零 `RemainDscTime_m`。

**理由 / 代码依据**
> 计算待充电量（`CurrentCap - RemainEnergy`）除以平均充电电流；80 s 启动延时同 REQ-SOC-017。

**验收准则（可度量）**
- Given `CurrentCap = 100000 mAh`，`RemainEnergy = 30000 mAh`，`avg_current_mA = 20000 mA`（充电），When 系统运行 80 s 后处于充电状态，Then `RemainChgTime_m = 70000×60/20000 = 210 min`（3.5 h）。

---

### REQ-SOC-019  单体电压统计（最大/最小/极差/均值）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.c:BmsinfoStatistics()`，第 85–122 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在每次 `BmsinfoStatistics()` 调用时，对所有使能单体（`CellEnabled` 位控制）计算：
> - `CellMaxVoltage_mV`：最高单体电压（mV），记录最高电压单体编号（`MaxVoltCellNum`，1 起始）；
> - `CellMinVoltage_mV`：最低单体电压（mV），记录最低电压单体编号（`MinVoltCellNum`，1 起始）；
> - `MaxDeltaCellVolt_mV`：最高与最低之差（mV）；
> - `AverageVolt_mV`：使能单体电压算术均值（mV）；
> - `SumVoltmV`：使能单体电压之和（mV）。
> 若无使能单体，上述统计量均置 0。

**理由 / 代码依据**
> 遍历 `TOTAL_AFE_CELL_NUM = 2` 个通道，跳过 `CellEnabled <= i` 的禁用通道；编号 `i+1`（从 1 起）。

**验收准则（可度量）**
- Given 两只单体电压分别为 3200 mV 和 3300 mV，均使能，Then `CellMax = 3300`，`CellMin = 3200`，`MaxDelta = 100`，`Average = 3250`，`Sum = 6500`。

---

### REQ-SOC-020  温度统计（最大/最小/极差/均值）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.c:BmsinfoStatistics()`，第 125–155 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在每次 `BmsinfoStatistics()` 调用时，对所有使能温度传感器（`TempEnabled` 控制，最多 `TOTAL_AFE_TEMP_NUM = 4`）计算：
> - `MaxTemp`：最高温度（0.1℃，有符号），记录传感器编号（`MaxTempCellNum`，1 起始）；
> - `MinTemp`：最低温度（0.1℃，有符号），记录传感器编号（`MinTempCellNum`，1 起始）；
> - `MaxDeltaTemp`：最高与最低之差（0.1℃，`uint16_t`）；
> - `AverageTemp`：使能传感器算术均值（0.1℃，`uint16_t`，**有符号/无符号混用，见存疑 Q-2**）。

**理由 / 代码依据**
> 遍历 `TOTAL_AFE_TEMP_NUM = 4`，跳过禁用通道；`MaxDeltaTemp` 类型为 `uint16_t`，存储 `MaxTemp - MinTemp` 可能溢出（见 Q-2）。

**验收准则（可度量）**
- Given 4 路温度 [200, 250, 300, 350]（均 = 20℃/25℃/30℃/35℃，0.1℃），均使能，Then `MaxTemp = 350`，`MinTemp = 200`，`MaxDeltaTemp = 150`，`AverageTemp = 275`。

---

### REQ-SOC-021  注入模式下的数据覆盖

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.c:BmsinfoStatistics()`，第 32–46 行 |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当上位机注入命令使能（`UpperCmd.Inject == UPPERCMD_ENABLE`）时，系统应用注入数据（`InjectData.CellVolt[]`、`InjectData.Temp[]`、`InjectData.intvolt_mV`）覆盖 AFE 实测数据，再执行统计计算；此功能用于仿真/调试，生产版本应确保此入口不可被意外使能。

**验收准则（可度量）**
- Given `UpperCmd.Inject == UPPERCMD_ENABLE`，When `BmsinfoStatistics()` 执行，Then `CellInfo.ActiveCellVoltmV` 使用 `InjectData` 值而非 AFE 测量值。

---

### REQ-SOC-022  间歇充电 SOC 阈值管理

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocUpdateWaitSelfConsumption()`，第 970–991 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在每次 SOC 任务周期内维护间歇充电等待标志（`Waite_selfConsumption`）：当 SOC ≥ 100% 时置位（禁止充电等待自耗放电）；当 SOC ≤ `IntermitterChgThd`（默认 95%）时清除（允许重新充电）；支持注入模式下使用注入 SOC 作为参考。

**理由 / 代码依据**
> `DEFAULT_INTERMITTERCHGTHD = 95`；`Waite_selfConsumption = TRUE/FALSE`；充满后等待 SOC 降至 95% 以下才允许再充，实现 LFP 间歇充电延寿策略。

**验收准则（可度量）**
- Given `Bms.Soc` 从 99% 升至 100%，Then `Waite_selfConsumption = TRUE`。
- Given `Bms.Soc` 降至 95%（= `IntermitterChgThd`），Then `Waite_selfConsumption = FALSE`。

---

### REQ-SOC-023  SOC/容量变化时持久化写 Flash

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `SocTask.c:SocPersistIfChanged()`，第 994–1005 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当 `Bms.Soc` 与上一次记录值（`ctx->oldSoc`）不同时，系统应将 `TotalCapOfChg`、`TotalCapOfDisc`、`Soc` 三项数据写入 Flash（`SaveChgcap()`/`SaveDisccap()`/`SaveSoc()`），确保掉电后可恢复。

**理由 / 代码依据**
> 以 SOC 变化（1% 步进）为触发条件，避免过于频繁写 Flash；同时也是循环次数 `CycleTimes` 的更新时机。

**验收准则（可度量）**
- Given `Bms.Soc` 从 50% 变为 51%，When `SocPersistIfChanged()` 执行，Then `SaveSoc()`、`SaveChgcap()`、`SaveDisccap()` 均被调用一次。
- Given `Bms.Soc` 未变化，Then 上述 Flash 写函数不被调用。

---

### REQ-SOC-024  额定容量与当前容量字段定义

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.h`，第 161–162 行；`ParaSet.h`，第 332 行 |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应维护两个容量字段：`RatedCap_mAH`（额定容量，mAh，初始值由参数 `RATEDCAP_MAH = 100000 mAh` 确定，不随 SOH 更新）和 `CurrentCap_mAH`（当前容量，mAh，随 SOH 计算更新，用于所有 SOC 计算的基准）。

**验收准则（可度量）**
- Given 系统初始化，Then `RatedCap_mAH = 100000 mAh`（默认值）。
- Given SOH 更新后 `CurrentCap_mAH = 95000 mAh`，Then `RatedCap_mAH` 保持 100000 mAh 不变。

---

## 存疑与观察

### Q-1  `SocRefreshDisplaySoc()` 返回值未使用（疑似 Bug）

`SocTask.c` 第 323–326 行：
```c
static void SocRefreshDisplaySoc(void)
{
    (void)SocCalcDisplaySoc(Bms.Soc);
}
```
`SocCalcDisplaySoc()` 计算出的显示 SOC 被 `(void)` 丢弃，**未写回任何字段**。`BMS_PARA_S` 结构体中也不存在 `DisplaySoc` 字段。这意味着对外上报的 SOC 就是内部 `Bms.Soc`（0–100%），底部 10% 截断的显示映射实际上从未生效。可能是遗留代码（映射逻辑已实现但字段未定义），或者显示 SOC 在通信层做了二次处理但未体现在此文件中。**建议确认 `UpperComTask.c` 中 SOC 上报逻辑是否对此重新处理。**

### Q-2  `AverageTemp` / `MaxDeltaTemp` 有符号与无符号混用

`BMS_Info.h` 中：
- `MaxTemp`、`MinTemp` 类型为 `int16_t`（支持负温度）
- `MaxDeltaTemp` 类型为 `uint16_t`：当 `MaxTemp - MinTemp` 为正时无问题；若 `MaxTemp` 为负（如 -10℃）、`MinTemp` 更负（如 -20℃），结果 `(-100) - (-200) = 100`，uint16 无问题；但若整体赋值路径有隐式截断则存在风险。
- `AverageTemp` 类型为 `uint16_t`：`BmsinfoStatistics()` 中 `Bms.AverageTemp = sum / tempcnt`，而 `sum` 是 `int32_t`（可为负），赋给 `uint16_t` 在低温场景（均值 < 0）时会发生无符号回绕（例如 -5℃ = -50 在 0.1℃ 单位下，赋给 uint16 = 65486）。`SocTask.c` 中 `SocGetAverageTemp0p1C()` 将 `AverageTemp` 强转为 `int16_t` 来恢复符号，若 `AverageTemp > 32767`（即 > 3276.7℃，不可能）则无问题；但若中间计算逻辑依赖 `uint16_t` 的 `AverageTemp` 直接比较（如温度告警），则低温时行为异常。**建议将 `AverageTemp` 改为 `int16_t`。**

### Q-3  SOH 只更新下降方向，且无上限保护

`CalSOH()` 第 222–237 行仅在 `newSohCap < oldSohCap × 0.97` 时更新。注释中有一行被屏蔽的上限检查（`newSohCap_mAH > (oldSohCap_mAH * 70 / 100)`），即下限过滤被注释掉了，意味着即使测量出极小的容量值（如噪声导致的异常）也会被写入。极端情况下 `CurrentCap_mAH` 可能被异常置为接近零，导致 `SOC = RemainEnergy / CurrentCap` 计算溢出（除数趋零）。**建议恢复下限 70% 检查。**

### Q-4  `CapToDisc_mAH` / `CapToChg_mAH` 赋值位置未确认

`BMS_Info.h` 定义了这两个字段，`BMS_Info.c` 中未见赋值；可能在 `UpperComTask.c` 或通信上报时临时计算。需确认赋值逻辑以完善 REQ-SOC-016。

### Q-5  剩余时间估算除零风险

`BmsinfoStatistics()` 第 52、57 行直接除以 `ABS(Bms.avg_current_mA)`，未做除零保护。若 `avg_current_mA = 0`（刚进入放/充电状态，电流采样尚未稳定），将发生除以零的未定义行为。**建议添加 `if (ABS(avg_current_mA) > 0)` 保护。**

### Q-6  SocTask 调度时序：u32SysTime 单位未明确

代码中 `if (u32SysTime < 500)` 注释为"4s 后开始计算SOC"，但宏 `CORRECT_TIME_ms = 1000` 用于 1 s 校正周期。如果 `u32SysTime` 以 ms 为单位，则 500 表示 500 ms 而非 4 s，注释有误。需确认 `u32SysTime` 的实际单位与递增步长。

---

## 本域需求索引表

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
| REQ-SOC-011 | 欠压保护触发时 SOC 强制归零 | 是 ⚠️ | `SocTask.c:SocHandleDischarging()` | 已实现 |
| REQ-SOC-012 | SOC 平滑显示（步进速率限制） | 否 | `SocTask.c:SocApplySmoothSoc()` | 已实现 |
| REQ-SOC-013 | 显示 SOC 映射（底部 10% 截断） | 否 | `SocTask.c:SocCalcDisplaySoc()` | 已实现 |
| REQ-SOC-014 | SOH 估算（从低 SOC 到满电容量测量法） | 否 | `SocTask.c:CalSOH()` | 已实现 |
| REQ-SOC-015 | SOH 计算温度窗口限制 | 否 | `SocTask.c:SocIsSohTempValid()` | 已实现 |
| REQ-SOC-016 | 可放电量与可充电量计算 | 否 | `BMS_Info.h`（字段定义） | 存疑 |
| REQ-SOC-017 | 剩余放电时间估算 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-018 | 剩余充电时间估算 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-019 | 单体电压统计（最大/最小/极差/均值） | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-020 | 温度统计（最大/最小/极差/均值） | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-021 | 注入模式下的数据覆盖 | 否 | `BMS_Info.c:BmsinfoStatistics()` | 已实现 |
| REQ-SOC-022 | 间歇充电 SOC 阈值管理 | 否 | `SocTask.c:SocUpdateWaitSelfConsumption()` | 已实现 |
| REQ-SOC-023 | SOC/容量变化时持久化写 Flash | 否 | `SocTask.c:SocPersistIfChanged()` | 已实现 |
| REQ-SOC-024 | 额定容量与当前容量字段定义 | 否 | `BMS_Info.h`、`ParaSet.h` | 已实现 |
