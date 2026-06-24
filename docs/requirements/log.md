<!-- S16100B 需求提炼 · LOG 域 · 逆向提炼自现有固件 -->
# REQ-LOG：数据记录（Flash 日志 / SD 卡 CSV / 分片索引 / 文件系统）需求规格

> **覆盖源文件：**
> - `Application/DataLog.c` / `Application/DataLog.h`
> - `Application/AppCsvLog.c` / `Application/AppCsvLog.h`
> - `Application/AppStorageMap.h`
> - `Application/AppTime.c`（定时旗标）
> - `Driver/BSP/ex_flash.h`（Flash 布局注释）
> - `Driver/SD/mmc_sd.h`、`Driver/BSP/sd_spi.h`（SD 卡接口）
> - `Application/UpperComTask.h`（`UpperCom_IsUpgrading()` 声明）
> - `Core/Src/main.c`（调度入口，20 s / 2 s 旗标消费）

---

## 需求列表

---

### REQ-LOG-001  Flash 日志记录字段集

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `DataLog.h:struct LogData`，`DataLog.c:UpdateLogdata()` |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应始终维护一份运行时日志快照（`logdata`），每次主循环调用 `UpdateLogdata()` 时刷新，包含以下字段：
> - **时间戳**：64 位 RTC 时间戳（`TimestampH`/`TimestampL`，拼接为 `unsigned long long`）
> - **电气量**：总内压 `intvolt_mV`（mV）、总外压 `extvolt_mV`（mV）、电流 `current_mA`（mA，充正放负）
> - **单体电压**：20 路 `cell_volt[20]`（uint16\_t，mV）
> - **温度**：NTC1–4 单体温度 `temp[0..3]`（int16\_t，0.1 ℃），MOS 温度 `temp[4]`，接触器温度 `temp[5]`，环境温度 `temp[6]`（共 7 路；`temp[7]` 字段存在但未赋值）
> - **统计量**：最高/最低单体电压及编号、最大压差、最高/最低温度及编号、最大温差
> - **能量统计**：累计充电容量 `TotalCapOfChg`（mAh）、累计放电容量 `TotalCapOfDisc`（mAh）
> - **状态**：SOC（1 %）、SOH（1 %）、系统状态 `sys_sta`、均衡状态 `bla_state`、限流状态 `lim_state`、加热状态 `heat_state`
> - **告警码**：`warn1code`（一级告警）、`warn2code`（二级告警）、`warn3code0`/`warn3code1`（三级告警）

**理由 / 代码依据**
> `UpdateLogdata()` 逐字段将 `Bms.*` 拷贝至全局 `logdata`，供 Flash 日志与上位机读取共用。
> 结构体大小 160 字节（代码注释明确：`LOG_SIZE = 160`），对齐设计为每扇区（4096 字节）恰好存 25 条（`TOLTAL_NUM_IN_SEC = 25`，余 96 字节用于扇区尾部位图）。

**验收准则（可度量）**
- Given 系统正常运行，When 主循环执行 `UpdateLogdata()`，Then `logdata.TimestampH:L` 与 RTC 当前时间一致，所有 20 路电压字段均已填充。

---

### REQ-LOG-002  Flash 日志写入周期（20 s 定时触发）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 / 性能 |
| 安全相关 | 否 |
| 来源（源码） | `Core/Src/main.c:main()` 第 238–241 行，`Application/AppTime.c:AppTime_Tick1ms()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当系统定时器 20 s 旗标有效（`AppTime_Take20sFlag() != 0`）且系统不处于 OTA 升级状态（`UpperCom_IsUpgrading() == 0`）时，系统应调用 `SaveLogInIndex()` 将当前 `logdata` 写入片外 Flash 日志区，写入周期为 20 s。

**理由 / 代码依据**
> `AppTime_Tick1ms()` 中 `delay_cnt % 20000U == 0` 每 20 s 置 `g_timer_20s_flag = 1`；main.c 主循环以 Take 语义消费该旗标，保证单次调用。
> `UpperCom_IsUpgrading()` 守卫防止升级期间 Flash 写入竞争。

**验收准则（可度量）**
- Given 系统运行且未升级，When 累计时间达到 20 s，Then Flash 日志区新增 1 条 160 字节记录，`LogIndex` 加 1。
- Given 系统正在 OTA 升级（`UpperCom_IsUpgrading() == 1`），When 20 s 旗标触发，Then 不执行 Flash 写入。

---

### REQ-LOG-003  Flash 日志分区布局与容量

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `DataLog.c` 文件头注释，`Driver/BSP/ex_flash.h` 布局注释，`DataLog.h` 宏定义 |
| 验证方法 | 检视 / 分析 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应将历史日志存储于片外 SPI Flash（W25Q16/W25Q64，4 MB 可寻址空间）的专用区域，布局约束如下：
> - **位图区**（主）：扇区 512，地址 `0x200000`，大小 4 KB；存储 `LogSecMap[408]`（每 uint32\_t 对应一个日志扇区，bit 为 0 表示已使用）
> - **位图区**（备份）：扇区 513，地址 `0x201000`（DataLog.c 注释中的旧方案；当前实现已改为将位图存储于每个日志扇区末尾 4 字节 `LOG_SECMAP_OFFSET = 4092`）
> - **日志数据区**：扇区 514–921，地址 `0x202000`–`0x399FFF`，共 408 个扇区
> - 每扇区容纳 25 条日志（`TOLTAL_NUM_IN_SEC = 25`），每条 160 字节，合计最大 10 200 条记录（约 57 小时 @20 s/条）
> - 扇区尾部 4 字节（偏移 4092）用作本扇区位图；全 0xFF 表示扇区全空，`0xFE000000` 表示扇区已满（25 bit 全清 = 0x00，结合初始 MSB 守卫，实际满扇区标志依 bit 清零顺序确定）

**理由 / 代码依据**
> `DataLog.c` 宏：`LOG_START_ADDR=0x202000`，`LOG_MAX_ADDR=0x399FFF`，`TOLTAL_LOG_SEC_MAP_NUM=408`；
> `LOG_SECMAP_OFFSET=4092`；
> `SaveSecMap()` 每次写入后更新对应扇区的 4 字节位图。

**验收准则（可度量）**
- Given Flash 全空，When 系统启动执行 `LogInit()`，Then `LogIndex = 0`；全部写满后 `LogIndex` 循环回 0 并执行扇区 0 擦除。

---

### REQ-LOG-004  Flash 日志初始化与断电续写

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `DataLog.c:LogInit()`，`Core/Src/main.c` 第 197 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当系统上电启动时，应调用 `LogInit()` 恢复上次写入位置（`LogIndex`），保证断电后续写不覆盖已有记录：
> 1. 逐扇区读取末尾 4 字节位图（`ReadLogMap2()`）；
> 2. 若所有 408 个扇区均为 `0xFFFFFFFF`（全空），则 `LogIndex = 0` 并全部擦除；
> 3. 否则从扇区 0 顺序扫描，找到第一个未满扇区，在其中找到第一个 bit 仍为 1（未使用）的位置，恢复 `LogIndex`。

**理由 / 代码依据**
> `LogInit()` 调用 `ReadLogMap2()`，后者逐扇区读取 `LOG_SECMAP_OFFSET` 处 4 字节，统计 0xFFFFFFFF 数量；
> 若 `num == TOLTAL_LOG_SEC_MAP_NUM` 则判定记录全空（返回 0x01），执行全擦初始化。

**验收准则（可度量）**
- Given 上次写到 LogIndex=150，When 掉电后重启执行 `LogInit()`，Then `LogIndex` 恢复为 150（误差 ≤1 条）。
- Given Flash 全空，When 执行 `LogInit()`，Then `LogIndex = 0`，所有 408 扇区已被擦除。

---

### REQ-LOG-005  Flash 日志环形覆盖策略

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `DataLog.c:SaveLogInIndex()` 第 306–318 行 |
| 验证方法 | 测试 / 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当 Flash 日志写满（`LogIndex` 到达第 408 个扇区末尾，即 `LogIndex / TOLTAL_NUM_IN_SEC == TOLTAL_LOG_SEC_MAP_NUM`）时，系统应：
> 1. 擦除扇区 0（`EraseLogSec(0)`）；
> 2. 将 `LogSecMap[0]` 重置为 `0xFFFFFFFF`；
> 3. 将 `LogIndex` 回绕为 0，从头开始覆盖最旧记录。
> 在写满之前进入下一扇区时，系统应预先擦除该扇区（`EraseLogSec(next_sec)`）并重置其位图为 `0xFFFFFFFF`。

**理由 / 代码依据**
> `SaveLogInIndex()` 在 `LogIndex++` 后检查是否到达扇区边界；到达最后扇区末尾时执行扇区 0 的擦除与回绕，否则擦除下一扇区。擦除在写入前执行（预清除），保证下次写入时目标扇区已被擦除。

**验收准则（可度量）**
- Given `LogIndex = 10175`（最后一条），When `SaveLogInIndex()` 执行后，Then `LogIndex = 0`，扇区 0 位图 = 0xFFFFFFFF。
- Given 写入到扇区 N 的最后一条，When 执行后，Then 扇区 N+1 已被擦除，位图重置为 0xFFFFFFFF。

---

### REQ-LOG-006  Flash 日志按时间段检索

| 属性 | 内容 |
|---|---|
| 类型 | 功能 / 接口 |
| 安全相关 | 否 |
| 来源（源码） | `DataLog.c:SearchIndex()`，`DataLog.c:ReadLogInInNum()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当上位机请求日志查询（`SearchIndex(startstamp, endstamp)`）时，系统应遍历所有非空扇区的全部记录，筛选时间戳在 `[startstamp, endstamp]` 范围内的记录，并将结果集的最小时间戳位置（`minsec`/`minposition`）、最大时间戳位置（`maxsec`/`maxposition`）、总数（`logcnt`）、环绕标志（`loopflag`）写入 `logreadinfo`。
> 后续调用 `ReadLogInInNum(num)` 应按从 `minsec:minposition` 起的线性偏移读取第 `num` 条记录（自动处理跨扇区边界的环绕）。

**理由 / 代码依据**
> `SearchIndex()` 全量线性扫描 408×25 个槽位（仅跳过位图全 0xFF 的空扇区），时间复杂度 O(N)（最大约 10 200 次 Flash 随机读）。
> `ReadLogInInNum()` 计算 `index = minsec*25 + minposition + num - 1`，并对超过 408 扇区的索引进行模运算处理环绕。

**验收准则（可度量）**
- Given 已存储 200 条记录，When `SearchIndex(t1, t2)` 调用，Then `logreadinfo.logcnt` 等于 t1–t2 区间内的记录数，误差为 0。
- Given `logreadinfo` 有效，When `ReadLogInInNum(1)`，Then 读出的 `readlogdata.TimestampH:L` 对应最小时间戳记录。

---

### REQ-LOG-007  SD 卡 CSV 日志写入周期（2 s 定时触发）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 / 性能 |
| 安全相关 | 否 |
| 来源（源码） | `Core/Src/main.c:main()` 第 243–246 行，`Application/AppTime.c:AppTime_Tick1ms()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当系统定时器 2 s 旗标有效（`AppTime_Take2sFlag() != 0`）且系统不处于 OTA 升级状态（`UpperCom_IsUpgrading() == 0`）时，系统应调用 `CSV_WriteData()` 向 SD 卡写入一行 CSV 数据，写入周期为 2 s。

**理由 / 代码依据**
> `AppTime_Tick1ms()` 中 `delay_cnt % 2000U == 0` 每 2 s 置 `g_timer_2s_flag = 1`；main.c 以 Take 语义消费旗标，保证单次调用。升级守卫与 Flash 日志一致。

**验收准则（可度量）**
- Given 系统正常运行且 SD 卡已就绪，When 累计 2 s，Then CSV 文件追加 1 行；连续 60 s 内追加 30 行。
- Given 系统处于 OTA 升级，When 2 s 旗标触发，Then CSV_WriteData 不被调用。

---

### REQ-LOG-008  SD 卡 CSV 文件按日期命名与自动创建

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppCsvLog.c:CSV_WriteData()` 第 38–55 行 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 当 `CSV_WriteData()` 被调用时，系统应以当前 RTC 日期（年月日）构造文件路径 `0:/YYYYMMDD.csv`（格式：`%04d%02d%02d.csv`，最大 24 字节），并以 `FA_OPEN_ALWAYS`（不存在则创建，已存在则追加）模式打开该文件。
> 若文件为新建（`f_size(file) == 0`），系统应先写入 CSV 表头行（64 个字段，含中文列名，以 `\r\n` 结尾）再写入数据行；否则直接追加数据行。

**理由 / 代码依据**
> `snprintf(path, 24, "0:/%04d%02d%02d.csv", year, month, date)`；
> `f_open(file, path, FA_WRITE | FA_OPEN_ALWAYS)` 后 `f_lseek(file, f_size(file))` 定位到末尾；
> `if(f_size(file) == 0)` 判断新文件写表头。

**验收准则（可度量）**
- Given SD 卡已挂载且当前日期为 2025-01-15，When `CSV_WriteData()` 首次调用，Then SD 根目录创建 `20250115.csv`，首行为 CSV 表头，第二行为数据。
- Given 跨日（00:00:00 后），When `CSV_WriteData()` 调用，Then 新建当日文件，旧文件完整保留。

---

### REQ-LOG-009  SD 卡 CSV 数据行字段内容

| 属性 | 内容 |
|---|---|
| 类型 | 功能 / 接口 |
| 安全相关 | 否 |
| 来源（源码） | `AppCsvLog.c:CSV_WriteData()` 第 59–254 行，`AppCsvLog.c:CSV_HEADER` |
| 验证方法 | 检视 / 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 每次写入 CSV 时，系统应按以下顺序格式化一行数据（共约 64 列，`\r\n` 结尾，Windows 兼容）：
>
> | 组 | 字段 | 格式/单位 |
> |---|---|---|
> | 时间 | `时:分:秒` | `%u:%u:%u` |
> | PACK 信息 | 地址、总内压(mV)、总电流(mA)、SOC(%)、总外压(mV) | `%u,%u,%d,%u,%u` |
> | 单体极值 | 最高单体电压(mV)、编号、最低单体电压(mV)、编号 | `%u,%u,%u,%u` |
> | 温度 | 最高单体温(℃ = val/10)、最低单体温、环境温、MOS 温、NTC1–4 | `%d`×8 |
> | 电池统计 | 循环次数、剩余容量(mAh)、剩余放电时长(min)、剩余充电时长(min) | `%u`×4 |
> | 累计容量 | 累计充电量(mAh)、累计放电量(mAh) | `%u,%u` |
> | 单体电压 | 前 16 路 `ActiveCellVoltmV[0..15]`（mV） | `%u,`×16 |
> | 系统状态 | STANDBY/CHARGING/DISCHARGING/满充/PROTECT/PRECHG（各 0/1） | `%u,`×6 |
> | MOS/控制状态 | 放电 MOS、充电 MOS、预充 MOS、限流、加热、均衡（0/1，均衡为 `0x%04x`） | 混合格式 |
> | 告警字 | 电压/电流/温度/MOS/采样/其他（各 `0x%04x`）、告警等级、告警数量 | `0x%04x`×6，`%u,%u` |
>
> 数据行使用固定大小缓冲区（`char buffer[400]`）格式化，最终以 `f_write` + `f_sync` + `f_close` 完成写入。

**理由 / 代码依据**
> CSV 表头（`CSV_HEADER`）与 `CSV_WriteData()` 中的 `snprintf` 段逐一对应；温度字段除以 10 转换为 ℃（0.1 ℃ → 1 ℃），代码注释明确。

**验收准则（可度量）**
- Given 系统运行，When 读取 CSV 文件，Then 每行字段数与表头列数一致（64 列），时间字段格式为 `HH:MM:SS`，总内压与实际 mV 值一致。

---

### REQ-LOG-010  SD 卡写入失败时的降级处理

| 属性 | 内容 |
|---|---|
| 类型 | 功能 / 约束 |
| 安全相关 | 否 |
| 来源（源码） | `AppCsvLog.c:CSV_WriteData()` 第 44–47、259–267 行 |
| 验证方法 | 测试 |
| 状态 | 存疑 |

**需求描述（EARS 句式）**
> 如果 SD 卡文件打开操作失败（`f_open` 返回 `res != FR_OK`），系统应立即返回错误码（`return res`），放弃本次 CSV 写入，且不影响主循环其他任务执行。
> 如果写入操作（`f_write`）失败，系统应跳过 `f_sync`，但仍执行 `f_close`，并返回错误码。

**理由 / 代码依据**
> `f_open` 失败直接 `return res`；`f_write` 失败后 `f_sync` 仅在 `res == FR_OK` 时执行；`f_close` 无条件调用。
> 但代码对 `snprintf` 缓冲区溢出（`written >= remaining`）的处理为**空分支**（`{ }`），溢出时数据截断但无告警/降级，属存疑。

**验收准则（可度量）**
- Given SD 卡未插入（`f_open` 失败），When `CSV_WriteData()` 调用，Then 函数在 10 ms 内返回非 FR_OK，主循环不阻塞。
- Given `f_write` 失败，When `CSV_WriteData()` 返回，Then `f_close` 已调用，文件句柄不泄漏。

---

### REQ-LOG-011  升级期间暂停日志写入

| 属性 | 内容 |
|---|---|
| 类型 | 功能 / 约束 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `Core/Src/main.c:main()` 第 238–246 行，`Application/UpperComTask.h:UpperCom_IsUpgrading()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 在系统 OTA 升级期间（`UpperCom_IsUpgrading() != 0`），系统应禁止执行 Flash 日志写入（`SaveLogInIndex()`）和 SD 卡 CSV 写入（`CSV_WriteData()`），防止日志 I/O 与 OTA Flash 操作产生竞争，危及固件完整性。

**理由 / 代码依据**
> `main.c` 中两处日志触发均有 `&& (UpperCom_IsUpgrading() == 0)` 守卫：
> ```c
> if((AppTime_Take20sFlag() != 0) && (UpperCom_IsUpgrading() == 0)) { SaveLogInIndex(); }
> if((AppTime_Take2sFlag()  != 0) && (UpperCom_IsUpgrading() == 0)) { CSV_WriteData();  }
> ```
> 升级完成后守卫解除，日志恢复正常写入。

**验收准则（可度量）**
- Given 系统进入 OTA 升级状态，When 20 s 旗标触发，Then Flash 写入次数增量为 0。
- Given 系统进入 OTA 升级状态，When 2 s 旗标触发，Then SD 卡 CSV 文件无新数据行写入。

---

### REQ-LOG-012  Flash 扇区分片写入与磨损管理

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `DataLog.c:SaveLogInSec()`、`SaveLogInIndex()`、`EraseLogSec()` |
| 验证方法 | 分析 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应采用分片写入策略降低 Flash 扇区磨损：每条 160 字节日志按扇区内偏移（`addr = LogIndex % 25`）顺序写入，同一扇区填满 25 条后才触发下一扇区擦除（`EraseLogSec`），单个扇区最少经历 1 次擦除后被复用。
> 环形写满一周（408 扇区 × 25 条 = 10 200 次写入）后，平均每扇区擦除 1 次；连续运行 57 小时（@20 s/条）擦除一轮。

**理由 / 代码依据**
> `SaveLogInIndex()` 中 `if((LogIndex % TOLTAL_NUM_IN_SEC) == 0)` 判断跨扇区，仅在此时执行 `EraseLogSec`；
> 每次写入后仅更新对应扇区末尾 4 字节位图（`SaveSecMap()`），无整扇区改写，最大程度减少擦除次数。

**验收准则（可度量）**
- Given 连续写入 25 条（1 个扇区），When `LogIndex % 25 == 0`，Then 执行 1 次 `EraseLogSec`，之前 25 条不触发擦除。

---

### REQ-LOG-013  Flash 日志扇区位图持久化

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `DataLog.c:SaveSecMap()` 第 160–168 行，`DataLog.c:ReadLogMap2()` |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 每次写入日志记录后，系统应立即将对应扇区的 4 字节位图更新到该扇区末尾偏移 4092 处（`LOG_SECMAP_OFFSET = 4092`），保证位图与数据区共处同一物理扇区，支持掉电后通过逐扇区读取位图恢复 `LogIndex`。

**理由 / 代码依据**
> `SaveSecMap(num)` 计算地址 `LOG_START_ADDR + num*SEC_SIZE + LOG_SECMAP_OFFSET` 并写入 4 字节；
> `ReadLogMap2()` 启动时逐扇区读取同一偏移处 4 字节，还原 `LogSecMap[408]`。
> 注：早期方案（`ReadLogMap()`、`InitSaveSecMap()`）已注释废弃，现行方案以每扇区自带位图为准。

**验收准则（可度量）**
- Given 写入第 N 条记录，When 读取扇区 `N/25` 末尾偏移 4092 处，Then 对应 bit 为 0（已使用）。

---

### REQ-LOG-014  SD 卡接口与文件系统初始化

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `Core/Src/main.c` 第 177 行（`SD_Init()`），`Driver/SD/mmc_sd.h`，`Driver/BSP/sd_spi.h` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述（EARS 句式）**
> 系统应在启动时调用 `SD_Init()` 初始化 SD 卡（SPI 接口，片选由 GPIO `SD_CS` 管理），通过 SPI 识别卡类型（MMC/SDv1/SDv2/SDv2HC）并挂载 FATFS 文件系统（驱动器号 `"0:"`），为 CSV 日志提供文件系统访问能力。

**理由 / 代码依据**
> `sd_spi.h` 提供 `SD_SPI_Init()`、`SD_SPI_ReadBlocks()`、`SD_SPI_WriteBlocks()` 接口；
> `mmc_sd.h` 提供 SD 协议层（CMD0/CMD8/CMD17/CMD24 等）；
> `AppCsvLog.c` 包含 `ff.h`（FatFS），使用 `f_open/f_write/f_sync/f_close` API。

**验收准则（可度量）**
- Given SD 卡已插入，When 系统上电，Then `SD_Init()` 在 1 s 内返回成功，`CSV_WriteData()` 可正常创建文件。
- Given SD 卡未插入，When `CSV_WriteData()` 调用，Then `f_open` 失败并返回错误码，主循环继续运行。

---

### REQ-LOG-015  CSV 数据行缓冲区大小约束

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `AppCsvLog.c` 第 24 行（`static char buffer[400]`） |
| 验证方法 | 分析 |
| 状态 | 存疑 |

**需求描述（EARS 句式）**
> 系统应为 CSV 数据行格式化提供大小不小于当前最大行长度的静态缓冲区（当前为 400 字节）；当格式化内容超出缓冲区时，系统不得产生未定义行为（缓冲区溢出）。

**理由 / 代码依据**
> `buffer[400]` 为静态局部变量；代码中每次 `snprintf` 后检查 `written >= remaining` 并进入**空分支**——溢出时既不截断输出也不报错，后续写入可能写入未定义内容。
> 64 列数据估算行长度：16 个单体电压（约 5×16=80 字节）+ 8 列温度 + 时间/电流/状态字等，实际可超过 350 字节，接近上限，需评估是否充裕。

**验收准则（可度量）**
- Given 所有字段填满最大值（电压 5 位、电流 6 位等），When `CSV_WriteData()` 执行，Then `remaining` 在最终 `f_write` 前 > 0（无溢出）。

---

## 存疑与观察

1. **`SearchIndex()` 中 LoopFlag 判断逻辑疑似 Bug**（`DataLog.c` 第 383–392 行）：
   第二个 `else if(minsec < maxsec)` 与第一个 `if(minsec < maxsec)` 条件完全相同，永远无法进入，导致环绕标志 `loopflag` 在 `minsec == maxsec` 时依赖 `minposition <= maxposition` 分支判断，但 `minsec > maxsec` 的真正环绕情况（日志已绕圈）**永远不会设置 `LoopFlag = 1`**。此为疑似 Bug，应将第二个条件改为 `else if(minsec > maxsec)`。
   `状态=存疑`

2. **CSV 缓冲区溢出无处理**（`AppCsvLog.c` `snprintf` 空 `if` 分支）：
   所有缓冲区满判断分支均为空体 `{ }`，溢出时静默截断，可能导致 CSV 行不完整但无任何错误上报。建议补充 FlashWriteF 类告警或至少返回错误码。
   `状态=存疑`

3. **位图方案迭代遗留**：`DataLog.c` 中 `ReadLogMap()`、`InitSaveSecMap()` 整段代码已注释废弃，但注释中描述的旧方案（集中位图扇区 512/513）与当前实现（分散位图于各数据扇区末尾）存在架构不一致。`LogSecMap` 数组在当前实现中仅作为运行时缓存，不再独立持久化。需确认是否彻底清理旧方案代码。
   `状态=存疑`

4. **Flash 日志区 `minsec`/`maxsec` 未初始化**（`DataLog.c:SearchIndex()`）：
   当 `searchcnt == 0`（无匹配记录）时，`minsec`/`maxsec`/`minposition`/`maxposition` 为未初始化局部变量，被直接赋值给 `logreadinfo`，上位机可能读到垃圾位置。建议在 `searchcnt == 0` 时对 `logreadinfo` 归零或设置无效标志。
   `状态=存疑`

5. **掉电完整性**：`SaveLogInIndex()` 在 `AppStorage_Write()`（Flash 写）和 `SaveSecMap()`（位图更新）之间如果掉电，数据已写但位图未更新，下次 `LogInit()` 会将该位置视为空槽重写。代码中未见原子写或校验保护。**此为低风险缺口**，标记为观察项。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-LOG-001 | Flash 日志记录字段集 | 否 | `DataLog.h`/`DataLog.c:UpdateLogdata()` | 已实现 |
| REQ-LOG-002 | Flash 日志写入周期（20 s 定时触发） | 否 | `main.c`/`AppTime.c` | 已实现 |
| REQ-LOG-003 | Flash 日志分区布局与容量 | 否 | `DataLog.c`/`ex_flash.h` | 已实现 |
| REQ-LOG-004 | Flash 日志初始化与断电续写 | 否 | `DataLog.c:LogInit()` | 已实现 |
| REQ-LOG-005 | Flash 日志环形覆盖策略 | 否 | `DataLog.c:SaveLogInIndex()` | 已实现 |
| REQ-LOG-006 | Flash 日志按时间段检索 | 否 | `DataLog.c:SearchIndex()`/`ReadLogInInNum()` | 已实现 |
| REQ-LOG-007 | SD 卡 CSV 日志写入周期（2 s 定时触发） | 否 | `main.c`/`AppTime.c` | 已实现 |
| REQ-LOG-008 | SD 卡 CSV 文件按日期命名与自动创建 | 否 | `AppCsvLog.c:CSV_WriteData()` | 已实现 |
| REQ-LOG-009 | SD 卡 CSV 数据行字段内容 | 否 | `AppCsvLog.c:CSV_WriteData()`/`CSV_HEADER` | 已实现 |
| REQ-LOG-010 | SD 卡写入失败时的降级处理 | 否 | `AppCsvLog.c:CSV_WriteData()` | 存疑 |
| REQ-LOG-011 | 升级期间暂停日志写入 | 是 ⚠️ | `main.c`/`UpperComTask.h` | 已实现 |
| REQ-LOG-012 | Flash 扇区分片写入与磨损管理 | 否 | `DataLog.c:SaveLogInIndex()` | 已实现 |
| REQ-LOG-013 | Flash 日志扇区位图持久化 | 否 | `DataLog.c:SaveSecMap()`/`ReadLogMap2()` | 已实现 |
| REQ-LOG-014 | SD 卡接口与文件系统初始化 | 否 | `main.c`/`mmc_sd.h`/`sd_spi.h` | 已实现 |
| REQ-LOG-015 | CSV 数据行缓冲区大小约束 | 否 | `AppCsvLog.c` buffer[400] | 存疑 |
