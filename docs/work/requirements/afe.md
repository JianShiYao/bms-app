# REQ-AFE：AFE 域（采集 / 均衡 / 保护位 / 断路 / 负载检测）需求规格

> 覆盖源文件：
> - `Application/AppAfe.c` / `AppAfe.h`
> - `Application/BmsAdc.c` / `BmsAdc.h`
> - `Application/BMS_Info.h`（`TOTAL_AFE_CELL_NUM`、`TOTAL_AFE_TEMP_NUM`、`BalanceConfigTypeDef`）
> - `Application/ParaSet.h`（默认参数宏）
> - `Driver/AFE/AFE.h`（`AFEDATA`、`SYSINFOR`、`Parameter`、AFE 驱动接口）
> - `Driver/AFE/AFE_Protect.h`（保护恢复阈值）
> - `Driver/AFE/Balance.h`（均衡延时宏）
> - `Driver/AFE/Calculate.h`（`AfeCalcResult_t`、`AFE_CALC_CELL_NUM/TEMP_NUM`）
> - `Driver/AFE/ChargerLoad.h`（负载/充电器检测宏）
> - `Driver/BSP/bsp_adc.h`（BSP ADC 通道结构）
> - `Driver/BSP/bsp_ntc.h`（NTC 温度扫描接口）
> - `Driver/AFE/bsp_SH3673520.h`（SPI 通信接口）
> - `Application/WarningTask.h`（告警等级定义）

---

## 需求列表

---

### REQ-AFE-001  电芯电压采集通道数与使能掩码

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.h`（`TOTAL_AFE_CELL_NUM=2`）；`AFE.h`（`AFEDATA.ssCell[20]`、`AFE_CELL1H~CELL20H`）；`AppAfe.c:AppAfe_UpdateCellEnabled()` |
| 验证方法 | 检视 / 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应支持最多 20 路电芯电压采集寄存器（SH3673520 CELL1~CELL20），但当前固件通过 `TOTAL_AFE_CELL_NUM=2` 限定应用层有效电芯数为 **2 路**。运行时通过 `Bms.CellEnabled`（uint32 位掩码）动态指定已接入的电芯通道，并由 `AppAfe_UpdateCellEnabled()` 将掩码写入驱动层，驱动层返回实际活跃电芯数 `Bms.ActiveCellNum`。

**理由 / 代码依据**
> `AFE.h` 定义 `ssCell[20]`，硬件最大支持 20 节；`BMS_Info.h` 定义 `TOTAL_AFE_CELL_NUM=2`（当前产品仅用 2 节）。`AFE_SetCellEnabled(cell_enabled, &active_cell_num)` 依据掩码重新初始化 AFE，活跃数存入 `Bms.ActiveCellNum`。

**验收准则（可度量）**
- Given `Bms.CellEnabled` 位掩码有 N 位置 1（N ≤ 20）, When 调用 `AppAfe_UpdateCellEnabled()`，Then `Bms.ActiveCellNum == N`，且 AFE 只读取对应通道的电压寄存器。
- Given `TOTAL_AFE_CELL_NUM=2`，Then 应用层数组 `ActiveCellVoltmV[2]` 只使用前 2 路采集值。

---

### REQ-AFE-002  电芯电压量纲与数据路径

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `AFE.h`（`SYSINFOR.usVCell[20]`，U16）；`Calculate.h`（`AfeCalcResult_t.cell_volt_mV[20]`，U16）；`AppAfe.c:AppAfe_UpdateBmsFromResult()` |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应以 **mV**（uint16，0~65535 mV）为单位存储和传递电芯电压。AFE 驱动层计算结果通过 `AfeCalcResult_t.cell_volt_mV[]` 输出，由 `AppAfe_UpdateBmsFromResult()` 写入 `Bms.CellInfo.ActiveCellVoltmV[]`。

**理由 / 代码依据**
> `Calculate.h` 明确 `cell_volt_mV[AFE_CALC_CELL_NUM]`；`AppAfe_UpdateBmsFromResult()` 直接赋值至 `Bms.CellInfo.ActiveCellVoltmV[0]` 和 `[1]`（仅 2 路有效）。

**验收准则（可度量）**
- Given AFE 寄存器读值对应 3600 mV 的标准电芯，When `AFEInfoProcess()` 完成，Then `Bms.CellInfo.ActiveCellVoltmV[N]` 应在 3600 ± 5 mV 范围内。

---

### REQ-AFE-003  电流采集——双路 ADC 与校准

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AFE.h`（`AFEDATA.ssVADCCurr`、`ssCADCCurr`，S16；`SYSINFOR.siCADCCurr`、`siVADCCurr`，S32）；`AppAfe.c:AppAfe_Process()`（L259-270）；`Calculate.h`（`AFECalcu_SetCurrentCalibration()`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应通过 AFE 芯片 **VADC** 和 **CADC** 两路电流检测通道采集电池电流，单位为 **mA**，充电为正值、放电为负值。采集前须以校准参数 `AdjCurRatio`（默认 16.103）和 `AdjCurOffset`（默认 289.855）进行线性校正。若 Flash 中校准值为 0 或全 0xFF（未初始化），系统应自动回退至默认校准参数。

**理由 / 代码依据**
> `AppAfe_Process()` 检测 `Bms.Para.AdjCurRatio / AdjCurOffset` 是否为 0 或 0xFFFFFFFF，若是则强制赋默认值 `APP_AFE_DEFAULT_CUR_RATIO=16.103f`、`APP_AFE_DEFAULT_CUR_OFFSET=289.855f`，然后调用 `AFECalcu_SetCurrentCalibration()` 注入校准系数。

**验收准则（可度量）**
- Given 已知外部参考电流 I_ref（单位 mA），When 采集并校准，Then `|Bms.current_mA - I_ref| ≤ 200 mA`（测试精度）。
- Given AdjCurRatio 存储为 0x00000000，When 调用 `AppAfe_Process()`，Then 系统自动使用默认值 16.103，不产生除零或非法计算。

---

### REQ-AFE-004  温度采集通道数、量纲与 NTC 传感器

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `BMS_Info.h`（`TOTAL_AFE_TEMP_NUM=4`）；`AFE.h`（`AFEDATA.usTS[4]`、`usTI`）；`Calculate.h`（`AFE_CALC_TEMP_NUM=4`，`AfeCalcResult_t.cell_temp_0p1C[4]`，S16）；`BmsAdc.h`（`mos_temp`、`env_temp`、`ptc_temp`，int16） |
| 验证方法 | 检视 / 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应采集 **4 路外部 NTC 温度**（TS1~TS4，通过 AFE 芯片）和 **1 路 AFE 内部温度**（TI），以及经 MCU ADC 采集的 **MOS 管温度**、**环境温度**、**PTC 温度**，共 7 路温度信息。AFE 外部温度量纲为 **0.1℃**（int16），通过 NTC 查表换算；`TempEnabled`（uint32，位掩码）指定实际接入的 AFE 温度通道数。

**理由 / 代码依据**
> `Calculate.h` 定义 `AFE_CALC_TEMP_NUM=4`，`cell_temp_0p1C[4]` 为 S16 类型表示 0.1℃精度。`BmsAdc.h` 中 `mos_temp`、`env_temp`、`ptc_temp` 均为 int16（同 0.1℃）。`bsp_ntc.h` 提供 `ScanTempHalf()` 返回 int16（0.5℃ 分辨率可选）。`AFECalcu_SetEnabledTempNum(Bms.TempEnabled)` 动态传入启用通道数。

**验收准则（可度量）**
- Given 外部 NTC 阻值对应 25℃，When 采集，Then `Bms.CellInfo.ActiveCellTemp[N]` 值应在 250 ± 5（单位 0.1℃）。
- Given `Bms.TempEnabled = 2`（2 路 NTC），When 调用 `AFECalcu_SetEnabledTempNum(2)`，Then 仅 2 路温度参与后续计算。

---

### REQ-AFE-005  MCU ADC 辅助采集（MOS 电压、MOS 温度、环境温度、PTC、漏液、地址）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `BmsAdc.h`（`BmsAdcSample_t`）；`BmsAdc.c:BmsAdc_Update()`；`bsp_adc.h`（`ADCNUM=7`，`BspAdcSample_t`） |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应通过 MCU 内置 ADC（7 路，`ADCNUM=7`）采集以下信号，并经 `BmsAdc_Update()` 写入 `Bms` 结构体：
> - `vmos_mv`（MOS 两端电压，mV，int32）→ `Bms.Vmos_mv`，并计算外部电压 `Bms.extvolt_mV = Bms.intvolt_mV - vmos_mv`
> - `mos_temp`（MOS 温度，int16，0.1℃）→ `Bms.MOSTemp`
> - `env_temp`（环境温度，int16，0.1℃）→ `Bms.ENVTemp`
> - `ptc_temp`（PTC 温度，int16，0.1℃）→ `Bms.PTCTemp`
> - `water_check`（漏液检测 ADC 原始值，uint16）→ `Bms.WaterCheck`
> - `addr_adc`（地址 ADC，int32）→ 经 `BmsAdc_CalcAddress()` 量化为 0~15 共 16 个地址等级 → `Bms.Address`

**理由 / 代码依据**
> `BmsAdc_Update()` 在 `vmos_valid` 为真时才更新 MOS 电压。地址计算在 tcnt=10~15 时进行一次（分压比 = addr_adc/4096），通过分段查表得到 0~15 的地址值。

**验收准则（可度量）**
- Given MCU ADC 采样 4 次（`ADCBUFSIZE=4`）均值计算后，When `BspAdc_Process()` 返回有效，Then `vmos_valid=1` 且 `vmos_mv` 在合理范围。
- Given `addr_adc/4096` 介于 0.955 和 1.0 之间，Then `Bms.Address = 0`。

---

### REQ-AFE-006  AFE SPI 通信故障检测与告警 ⚠️

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AFE.h`（`bAfeSPIRWErrFlg`、`ucSPIRWErrDelayCnt`、`ucSPIRWErrRDelayCnt`；`SYSINFOR.bAFE_ERR`）；`AppAfe.c:AppAfe_UpdateAfeWarning()`（L179-191） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 当 AFE 芯片 SPI 通信发生故障（`bAfeSPIRWErrFlg = TRUE`，经驱动层计时后置 `Info.bAFE_ERR=1`）时，系统应同时置位 `warn2_level.AFECommF = WARNING_LEVEL_3` 和 `warn2_level.AFEERRO = WARNING_LEVEL_3`；当通信恢复时，上述告警应清除至 `WARNING_LEVEL_0`。

**理由 / 代码依据**
> `AppAfe_UpdateAfeWarning()` 根据 `Info.bAFE_ERR` 的真值分支，向应用层告警结构体写入等级 3 或等级 0。SPI 通信重试次数 `TRY_TIMES=5`（`AFE.h`）。

**验收准则（可度量）**
- Given 模拟 SPI 通信中断，When 通信失败持续超过 `ucSPIRWErrDelayCnt` 计数，Then `Info.bAFE_ERR=1` 且 `warn2_level.AFECommF==3`。
- Given SPI 通信恢复，When `ucSPIRWErrRDelayCnt` 计数超时，Then `Info.bAFE_ERR=0` 且 `warn2_level.AFECommF==0`。

---

### REQ-AFE-007  AFE 硬件短路保护位读取与状态维护 ⚠️

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AFE.h`（`SYSINFOR.bSC`；`AFE_ClearScFlag()`；`AFE_SCV_SCT` 寄存器 0x4F，默认 0x04）；`AppAfe.c:AppAfe_IsScFault()`、`AppAfe_ClearScFault()`（L68-173） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 当 AFE 芯片硬件短路保护标志位 SC 被触发（`Info.bSC=1`）时，系统应通过 `AppAfe_IsScFault()` 向应用层提供状态查询接口；仅在外部条件满足后，由 `AppAfe_ClearScFault()` 调用 `AFE_ClearScFlag()` 清除硬件标志，并同步清除 `Info.bSC`。AFE 短路保护电流阈值由寄存器 `AFE_SCV_SCT`（默认 0x04，对应 2×OCD2，约 320 A）在初始化时写入芯片。

**理由 / 代码依据**
> `Parameter.E2ucAFESCV_SCT = 0x04`（`AFE.h` 默认值注释："bit4-bit5: 00=2×OCD2 ... 设为320A 2×100A 128μs"）。`AppAfe_ClearScFault()` 先调用驱动层清标志，成功后才清 `Info.bSC=0`，具有保护性检查。

**验收准则（可度量）**
- Given 电流超过短路阈值（≈320 A），When AFE 芯片产生 SC 中断，Then `Info.bSC=1` 且 `AppAfe_IsScFault()==1`。
- Given `AppAfe_ClearScFault()` 被调用，When `AFE_ClearScFlag()` 返回 1（成功），Then `Info.bSC==0`；若驱动层返回 0（失败），Then `Info.bSC` 保持为 1。

---

### REQ-AFE-008  AFE 放电二级过流保护位读取与清除 ⚠️

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AFE.h`（`SYSINFOR.bOCD2`；`AFE_OCD2V_OCD2T` 寄存器 0x4E，默认 0x07；`AFE_ClearOcd2Flag()`）；`AppAfe.c:AppAfe_IsOcd2Fault()`、`AppAfe_ClearOcd2Fault()`（L68-111） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 当 AFE 芯片放电二级过流保护（OCD2）被触发时，系统应通过 `AppAfe_IsOcd2Fault()` 提供状态查询接口；仅当驱动层 `AFE_ClearOcd2Flag()` 执行成功后，系统才清除 `Info.bOCD2`。OCD2 阈值由寄存器 `AFE_OCD2V_OCD2T`（默认 0x07，注释："设为 160 A，bit0-bit3×20A+20A"）在初始化时写入芯片。

**理由 / 代码依据**
> `AppAfe_ClearOcd2Fault()` L103-111：先 `AFE_ClearOcd2Flag()`，仅返回值为真时才 `Info.bOCD2 = 0`。AFE 硬件 OCD1 阈值 `AFE_OCD1V_OCD1T` 默认 0x0D（注释："设为 140 A"）。

**验收准则（可度量）**
- Given 放电电流超过 OCD2 阈值（≈160 A），When AFE 触发保护，Then `Info.bOCD2=1`。
- Given 保护条件解除后调用 `AppAfe_ClearOcd2Fault()`，When 驱动层成功清标志，Then `Info.bOCD2=0` 且函数返回 1。

---

### REQ-AFE-009  AFE 硬件过压/欠压/过温/充电过流保护寄存器初始化 ⚠️

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AFE.h`（`Parameter` 结构体 `E2ucAFEOVT_OVH`~`E2ucAFEOCCV_OCCT`，默认值宏 `_E2_AFE_OVT_OVH=0x33` 等）；`AFEInit()` 接口 |
| 验证方法 | 检视 / 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统初始化时，应按 `Parameter` 结构体中的 AFE 保护参数配置 SH3673520 芯片的以下硬件保护寄存器（通过 `AFEInit()` 写入），各默认值如下：

| 保护类型 | 寄存器 | 默认值 | 说明 |
|---|---|---|---|
| 单节过压 | `AFE_OVT_OVH`/`AFE_OVL` | 0x33/0x0C | 约 3900 mV |
| 单节欠压 | `AFE_UVT_UVH`/`AFE_UVL` | 0x21/0x68 | 约 1800 mV |
| OCD1 | `AFE_OCD1V_OCD1T` | 0x0D | 约 140 A |
| OCD2 | `AFE_OCD2V_OCD2T` | 0x07 | 约 160 A |
| 短路 SC | `AFE_SCV_SCT` | 0x04 | 约 320 A，128 μs |
| 充电过流 OCC | `AFE_OCCV_OCCT` | 0x3F | 约 110 A |
| 充电高温 OTC | `AFE_OTC` | 转换自 55℃ | 由温度值换算 |
| 充电低温 UTC | `AFE_UTC` | 转换自 0℃ | |
| 放电高温 OTD | `AFE_OTD` | 转换自 60℃ | |
| 放电低温 UTD | `AFE_UTD` | 转换自 -25℃ | |

**理由 / 代码依据**
> 所有默认值来自 `AFE.h` 中的 `#define _E2_AFE_*` 宏，由 `SetDefaultAFEPara()` 写入 `parameter` 结构体，再经 `AFEInit()` 送入 AFE 芯片。温度参数使用偏置编码：物理温度 T℃ = (参数值 - 2731) / 10。

**验收准则（可度量）**
- Given 使用默认参数，When `AFEInit()` 完成，Then 读回 `AFE_SCV_SCT` 寄存器值为 0x04。
- Given AFE 检测到单节电压超过约 3900 mV 且持续超过 OV 延时，Then `Info.bOV=1`。

---

### REQ-AFE-010  AFE 保护状态恢复处理任务

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AppAfe.c:AppAfe_ProtectTask()`（L278-282）；`AFE_Protect.h`（`ProtectProcess()`，保护恢复阈值宏）|
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应在每个调度周期内调用 `AppAfe_ProtectTask()`，该函数代理调用 `ProtectProcess()` 完成以下保护恢复逻辑：
> - 单节过压保护恢复：电压降至 `E2usOVRVol`（默认 4150 mV），延时 `E2ucOVRDelay×70 ms`（默认 14×70 ms ≈ 0.98 s）；
> - 单节欠压保护恢复：电压升至 `E2usUVRVol`（默认 2200 mV），延时同上；
> - 总压保护恢复：`E2usPackOVRVol×100`（默认 80000 mV）/ `E2usPackUVRVol×100`（默认 36000 mV）；
> - 温度保护恢复：充电高温恢复至 80℃（`_E2_OTCR_TEMP=3581`），充电低温恢复至 5℃（`_E2_UTCR_TEMP=2781`），放电高温恢复至 80℃，放电低温恢复至 -5℃（`_E2_UTDR_TEMP=2681`），延时 `E2ucOUTRDelay×70 ms`（默认 28×70 ms ≈ 1.96 s）；
> - AFE 芯片内部高温保护阈值：100℃（`TEMP_OTI=2731+100×10`），恢复阈值 90℃（`TEMP_OTIR`）。

**理由 / 代码依据**
> `AFE_Protect.h` 定义各恢复延时宏，均为 `parameter.E2uc*×1`（单位为 70 ms 计数步进，因 AFE 基础定时器以 70 ms 为周期）。

**验收准则（可度量）**
- Given 单节电压降至 4150 mV 以下且持续 ≥ 0.98 s，When 调用 `ProtectProcess()`，Then 过压保护标志清除。

---

### REQ-AFE-011  电流平滑——6 点去极值均值

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppAfe.c:AppAfe_OneSecondTask()`（L335-353）；`AppAfe_AvgWithoutMaxMinI32()`（L15-53） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应每 **1 秒**（由 `AppTime_Take1sFlag()` 触发）采样一次 `Bms.current_mA`，维护最近 **6 个采样**的滚动缓冲区，并采用**去掉一个最大值和一个最小值后取算术平均**的方式计算 `Bms.avg_current_mA`（单位 mA）。

**理由 / 代码依据**
> `cur[6]` 静态数组循环写入，`cur_num` 到达 6 后回绕为 0；`AppAfe_AvgWithoutMaxMinI32(buf, 6)` 在 len>2 时去掉 max/min，剩余 4 个样本取均值。若 len≤2 不去除极值。

**验收准则（可度量）**
- Given 6 次采样值为 [100, 200, 300, 400, 500, 600] mA，When 调用均值算法，Then `avg_current_mA = (200+300+400+500)/4 = 350 mA`（去除 100 和 600）。
- Given 缓冲区只有 2 个值，When 调用均值算法，Then 返回 2 个值的算术平均（不去极值）。

---

### REQ-AFE-012  负载接入检测（放电侧）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppAfe.c:AppAfe_LoadCheckTask()`（L287-293）；`ChargerLoad.h`（`LOADCHG_DELAY_CNT`、`bLoadRStatusFlg`；`LoadChkProcess()`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 当 `system_run_time_100ms % 40` 大于 20 时（即每 40×100 ms = 4 s 周期内，后半段 2 s 检测），系统应调用 `LoadChkProcess(u32SysTime)` 执行负载接入检测，并将返回结果存入模块内部变量 `HaveLoad`（1=有负载，0=无负载），供 `AppAfe_HasLoad()` 对外查询。检测防抖延时由 `E2ucLoadChgChkDelay×70 ms`（默认 14×70 ms ≈ 0.98 s）控制。

**理由 / 代码依据**
> 负载检测使用 AFE 内置上拉电流源（`AFE_RLD0=60 μA` 或 `AFE_RLD1=500 μA`），通过读取 BSTATUS 寄存器中的负载检测位判断。

**验收准则（可度量）**
- Given 外部负载接入，When `LoadChkProcess()` 被调用且延时超过 0.98 s，Then `AppAfe_HasLoad()==1`。
- Given 外部负载断开，When 延时超过 `LOADCHG_DELAY_CNT`，Then `AppAfe_HasLoad()==0`。

---

### REQ-AFE-013  充电器接入检测

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `ChargerLoad.h`（`CHGR_CHK_VOL = E2usChgRChkVol×100`，`bChgerRStatusFlg`）；`AFE.h`（`AFEDATA.usVCHGR`；`AFE_VCHGRH/L` 寄存器；默认 `_E2_CHGCHK_VOL=840→84.0 V`，`_E2_CHGRCHK_VOL=600→60.0 V`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应持续检测充电器接入端（CHGD，由 AFE 寄存器 `VCHGR` 采集）的电压：
> - 当 VCHGD 超过 **84.0 V**（`_E2_CHGCHK_VOL×0.1 V`，默认）时，经 `E2ucLoadChgChkDelay` 延时后判定充电器接入，`AppAfe_HasCharger()==1`；
> - 当 VCHGD 低于 **60.0 V**（`_E2_CHGRCHK_VOL×0.1 V`，默认）时，经同等延时后判定充电器断开，`AppAfe_HasCharger()==0`。

**理由 / 代码依据**
> `CHGR_CHK_VOL = parameter.E2usChgRChkVol×100`（单位 mV）；`SYSINFOR.uiVCHGD` 存储换算后的充电器端电压（mV）。

**验收准则（可度量）**
- Given VCHGD = 85.0 V 持续超过 0.98 s，Then `AppAfe_HasCharger()==1`。
- Given VCHGD = 55.0 V 持续超过 0.98 s，Then `AppAfe_HasCharger()==0`。

---

### REQ-AFE-014  电芯断路（开路）检测 ⚠️

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AppAfe.c:AppAfe_CellOpenTask()`（L298-308）；`Balance.h`（`CellOpenProcessByCellCount()`；`CTO_DELAY_CNT=10`，单位 s）；`AFE.h`（`AFE_OWDH/M/L` 寄存器 0x97~0x99；`SYSINFOR.uiCTOChannel`；`Parameter.bCTO_EN`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 当 `bCTO_EN=1`（配置使能）时，系统应在每个调度周期调用 `CellOpenProcessByCellCount(Bms.CellEnabled)` 执行电芯引线断路检测：若检测到断路（返回非零值），则置 `warn2_level.AFECellLineOpen = WARNING_LEVEL_3`；否则清除为 `WARNING_LEVEL_0`。断路判断基于 AFE 芯片的 OWD（Open Wire Detection）机制（`AFE_OWD` 寄存器组），断路确认延时为 **10 s**（`CTO_DELAY_CNT=10`）。

**理由 / 代码依据**
> `Parameter.bCTO_EN`（默认 0，即默认不启用）。`SYSINFOR.uiCTOChannel` 指示哪路电芯发生断路（位图）。`AFE_OWDH/M/L`（0x97-0x99）为 OWD 检测原始数据寄存器，`AFE_OWV_ALARMH`（0x47，默认 0x57）为 OWV 报警高字节。

**验收准则（可度量）**
- Given `bCTO_EN=1`，当某电芯引线断开超过 10 s，Then `warn2_level.AFECellLineOpen==3`。
- Given 引线恢复连接，Then `warn2_level.AFECellLineOpen==0`。
- Given `bCTO_EN=0`（默认），Then 断路检测功能不触发告警。

---

### REQ-AFE-015  被动均衡控制——开启条件

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppAfe.c:AppAfe_BalanceTask()`（L313-329）；`Balance.h`（`BalProcessByInput()`，`BALANCE_DELAY_CNT`，`BalUpdate_DELAY_CNT=100`）；`BMS_Info.h`（`BalanceConfigTypeDef`）；`ParaSet.h`（`DEFAULTBALANCE*` 宏） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 在 BMS 处于充电状态（`Bms.sta == BMS_STA_CHARGING`）时，系统应通过 `AppAfe_BalanceTask()` 向均衡驱动层传入以下条件，由 `BalProcessByInput()` 决定各电芯的均衡开关状态：
>
> | 参数 | 来源 | 默认值 |
> |---|---|---|
> | `enable` | `BalaceConfig.enable`（默认 1，SetDefaultPara）| 1（使能）|
> | `charging` | `Bms.sta == BMS_STA_CHARGING` | 动态 |
> | `average_temp` | `Bms.AverageTemp`（0.1℃） | 动态 |
> | `max_temp` | `BalaceConfig.MaxTemp` | 500（50.0℃） |
> | `min_temp` | `BalaceConfig.MinTemp` | 100（10.0℃） |
> | `start_volt_mV` | `BalaceConfig.StartVoltmV` | 1500 mV ⚠️ 存疑 |
> | `start_delta_mV` | `BalaceConfig.StartDeltaVoltagemV` | 300 mV |
> | `stop_delta_mV` | `BalaceConfig.StopDeltaVoltagemV` | 100 mV |
> | `cell_enabled` | `Bms.CellEnabled` | 位掩码 |
>
> 均衡进入延时：`BALANCE_DELAY_CNT = E2ucBalanceDelay×1`（默认 `_E2_BAL_DELAY=29`，即 29×70 ms ≈ 2.03 s）。均衡状态更新间隔：`BalUpdate_DELAY_CNT=100`（100×70 ms = 7 s）。

**理由 / 代码依据**
> `AppAfe.c` L318 判断 `Bms.sta == BMS_STA_CHARGING`，仅充电时 `input.charging=1`；均衡在非充电状态时 `input.charging=0`，由 `BalProcessByInput()` 决定是否执行（通常非充电不均衡）。AFE 硬件均衡寄存器 `AFE_BALANCEH/M/L`（0x55-0x57）承载均衡通道位图，`SYSINFOR.uiBALChannel` 反映当前均衡通道。

**验收准则（可度量）**
- Given `enable=1`、`charging=1`、`average_temp=200`（20℃，在 100~500 范围内）、某电芯电压 ≥ 1500 mV、最大最小压差 ≥ 300 mV，持续超过 2.03 s，Then 对应电芯均衡开关应置 1。
- Given 均衡进行中，压差降至 ≤ 100 mV（`stop_delta_mV`），Then 均衡应关闭。
- Given `average_temp < 100`（<10℃）或 `average_temp > 500`（>50℃），Then 均衡不开启。

---

### REQ-AFE-016  被动均衡控制——浮充与间隔定时

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BMS_Info.h`（`BalanceConfigTypeDef.FloatTimems`、`IntervalTimems`）；`ParaSet.h`（`DEFAULTBALANCEFLOATTIME=1×60×1000ms`、`DEFAULTBALANCEINTTIME=5×60×1000ms`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统应支持均衡的浮充等待定时：在充电进入浮充状态后，若持续 **1 分钟**（`FloatTimems` 默认值 60000 ms）才开始进行均衡判断；均衡完成一次后，须等待 **5 分钟**（`IntervalTimems` 默认值 300000 ms）的间隔时间，再允许下一次均衡。

**理由 / 代码依据**
> `BalanceConfigTypeDef` 在 `BMS_Info.h` L62-63 定义 `FloatTimems`（uint32，ms）和 `IntervalTimems`（uint32，ms），默认值由 `SetDefaultPara()` 赋值（`ParaSet.h` L49-50）。

**验收准则（可度量）**
- Given 进入浮充状态，When `FloatTimems` 未到达，Then 均衡判断不启动。
- Given 一次均衡刚结束，When 经过时间 < `IntervalTimems`，Then 下一次均衡不允许开始。

---

### REQ-AFE-017  AFE 芯片初始化与重初始化

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AFE.h`（`AFEInit()`、`AFE_InitWithCellEnabled()`、`AFE_SetCellEnabled()`）；`AppAfe.c:AppAfe_Reinit()`（L354-357） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统上电或需要重新配置电芯使能时，应调用 `AppAfe_Reinit()`，该函数以当前 `Bms.CellEnabled` 为参数调用 `AFE_InitWithCellEnabled()`，完成 AFE 寄存器的完整配置（含保护阈值、温度参数、均衡寄存器、SCONF 寄存器等）。

**理由 / 代码依据**
> `AFE_InitWithCellEnabled()` 和 `AFEInit()` 同时在 `AFE.h` 声明，前者允许传入使能掩码，后者使用全部默认配置。

**验收准则（可度量）**
- Given `Bms.CellEnabled=0x03`（2 路使能），When `AppAfe_Reinit()` 完成，Then `Bms.ActiveCellNum=2` 且 AFE 采集仅读取 2 路。

---

### REQ-AFE-018  调试注入模式——旁路 AFE 采集

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `AppAfe.c:AppAfe_Process()`（L252-257）；`BmsAdc.c:BmsAdc_Update()`（L56-70）；`UpperComTask.h`（`UpperCmd.Inject`，`UPPERCMD_ENABLE`） |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 当上位机注入命令 `UpperCmd.Inject == UPPERCMD_ENABLE` 时，系统应跳过 AFE 实际采集，将 `InjectData` 中的电流、电芯电压、温度、MOS 电压数据直接写入 `Bms`，以供工厂调试或模拟工况使用。退出注入模式后，恢复正常 AFE 采集流程。

**理由 / 代码依据**
> `AppAfe_Process()` L253-257：`if(UpperCmd.Inject == UPPERCMD_ENABLE) { AppAfe_ApplyInjectedData(); return; }`，提前返回不调用 `AFEInfoProcess()`。`BmsAdc_Update()` 同样检查注入标志。

**验收准则（可度量）**
- Given 注入模式使能，When `AppAfe_Process()` 调用，Then `Bms.current_mA == InjectData.current_mA`，且不读取 AFE 硬件寄存器。

---

### REQ-AFE-019  MOS 管温升监测与地址 ADC 自标定

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `BmsAdc.c:BmsAdc_Update()`（L82-98）；`BMS_Info.h`（`Bms.MOSTempRise`，`Bms.Address`） |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码）|

**需求描述（EARS 句式）**
> 系统上电后，应在调用计数 `tcnt` 达到 5~10 期间对 MOS 管初始温度和地址 ADC 进行滑动平均采样（每次与累计均值作 1/2 低通滤波）；`tcnt > 10` 后，持续计算 `Bms.MOSTempRise = Bms.MOSTemp - MOSTempStart`（温升，单位 0.1℃）；在 `tcnt` 10~15 期间完成地址计算（`addr_adc/4096` 分段查表 → `Bms.Address`，0~15 共 16 档），此后地址固定不再更新。

**理由 / 代码依据**
> `BmsAdc_Update()` 使用静态变量 `tcnt` 控制初始化时序，`MOSTempStart` 和 `AddrADC` 各做 `(新值+旧值)/2` 的指数滑动平均。地址编码通过 16 段电阻分压实现硬件寻址，支持 0~15 共 16 个地址。

**验收准则（可度量）**
- Given `tcnt > 10`，Then `Bms.MOSTempRise = Bms.MOSTemp - MOSTempStart`（精度 0.1℃）。
- Given `addr_adc/4096 = 0.95`（>0.866），When `tcnt` 在 10~15，Then `Bms.Address = 1`。

---

## 存疑与观察

1. **`DEFAULTBALANCESTARTVOLT=1500 mV` 疑似偏低（REQ-AFE-015 存疑）**：默认均衡启动电压仅 1500 mV，而从 `_E2_BAL_VOL=4200 mV` 看，AFE 层（SH3673520 Demo 工程参数）期望 4200 mV 触发均衡；BMS 应用层 `ParaSet.h` 中 `DEFAULTBALANCESTARTVOLT=1500` 疑似为测试值或未及时更新为产品值，实际产品需确认正确阈值。两套均衡参数（AFE 层 `_E2_BAL_*` 与应用层 `BalaceConfig.*`）的优先级关系也未在代码中明确体现。

2. **`TOTAL_AFE_CELL_NUM=2` 与硬件 20 路寄存器不匹配（REQ-AFE-001 存疑）**：`BMS_Info.h` 将应用层电芯数硬编码为 2，而 `AFE.h` 硬件最大支持 20 路，`Calculate.h` 定义 `AFE_CALC_CELL_NUM=20`。若实际产品需要 16 节，`TOTAL_AFE_CELL_NUM` 必须同步修改，否则电压统计、保护判断均受影响。

3. **`AFE_InitWithCellEnabled` 与 `AFEInit` 的关系不明（REQ-AFE-017 存疑）**：两个初始化函数都在 `AFE.h` 声明，但仅有 `.h` 文件，Driver 层 `.c` 实现未在镜像中包含（已编译为库）。无法确认 `AFE_InitWithCellEnabled` 是否在每次 CellEnabled 变化时安全重配置所有保护寄存器，还是仅修改通道选择。

4. **负载检测周期逻辑（REQ-AFE-012 存疑）**：`AppAfe_LoadCheckTask()` 使用 `(system_run_time_100ms % 40) > 20` 判断，意味着每 4 s 周期内约后 2 s 才执行检测。这种不连续检测方式若与 AFE 硬件负载检测上拉时序存在耦合，可能导致漏检；具体时序需结合 `LoadChkProcess()` 的实现（库内未开放）确认。

5. **MOS 温度告警阈值（温升 700 单位 = 70℃）**：`MACRO_MOSTEMPRISE=700`（0.1℃），即 MOS 温升超过 70℃ 时触发告警。结合 `MACRO_MOSTEMPRISE_100ms=30`（3 s 延时），需确认此告警是否触发 MOS 关断（安全相关）。代码中仅看到告警写入 `warn2_level.MOSTempRise`，未找到直接的 MOS 关断联动。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-AFE-001 | 电芯电压采集通道数与使能掩码 | 否 | `BMS_Info.h`、`AFE.h`、`AppAfe.c` | 已实现 |
| REQ-AFE-002 | 电芯电压量纲与数据路径 | 否 | `Calculate.h`、`AppAfe.c` | 已实现 |
| REQ-AFE-003 | 电流采集——双路 ADC 与校准 | 是 ⚠️ | `AFE.h`、`AppAfe.c` | 已实现 |
| REQ-AFE-004 | 温度采集通道数、量纲与 NTC 传感器 | 是 ⚠️ | `BMS_Info.h`、`Calculate.h`、`BmsAdc.h` | 已实现 |
| REQ-AFE-005 | MCU ADC 辅助采集（MOS/环境/PTC/漏液/地址） | 否 | `BmsAdc.c`、`bsp_adc.h` | 已实现 |
| REQ-AFE-006 | AFE SPI 通信故障检测与告警 | 是 ⚠️ | `AFE.h`、`AppAfe.c` | 已实现 |
| REQ-AFE-007 | AFE 硬件短路保护位读取与状态维护 | 是 ⚠️ | `AFE.h`、`AppAfe.c` | 已实现 |
| REQ-AFE-008 | AFE 放电二级过流保护位读取与清除 | 是 ⚠️ | `AFE.h`、`AppAfe.c` | 已实现 |
| REQ-AFE-009 | AFE 硬件保护寄存器初始化 | 是 ⚠️ | `AFE.h`（`Parameter` 默认值宏） | 已实现 |
| REQ-AFE-010 | AFE 保护状态恢复处理任务 | 是 ⚠️ | `AppAfe.c`、`AFE_Protect.h` | 已实现 |
| REQ-AFE-011 | 电流平滑——6 点去极值均值 | 否 | `AppAfe.c` | 已实现 |
| REQ-AFE-012 | 负载接入检测（放电侧） | 否 | `AppAfe.c`、`ChargerLoad.h` | 已实现 |
| REQ-AFE-013 | 充电器接入检测 | 否 | `ChargerLoad.h`、`AFE.h` | 已实现 |
| REQ-AFE-014 | 电芯断路（开路）检测 | 是 ⚠️ | `AppAfe.c`、`Balance.h`、`AFE.h` | 已实现 |
| REQ-AFE-015 | 被动均衡——开启条件 | 否 | `AppAfe.c`、`Balance.h`、`ParaSet.h` | 已实现（存疑：StartVolt）|
| REQ-AFE-016 | 被动均衡——浮充与间隔定时 | 否 | `BMS_Info.h`、`ParaSet.h` | 已实现 |
| REQ-AFE-017 | AFE 芯片初始化与重初始化 | 是 ⚠️ | `AFE.h`、`AppAfe.c` | 已实现 |
| REQ-AFE-018 | 调试注入模式——旁路 AFE 采集 | 否 | `AppAfe.c`、`BmsAdc.c` | 已实现 |
| REQ-AFE-019 | MOS 温升监测与地址 ADC 自标定 | 否 | `BmsAdc.c` | 已实现 |
