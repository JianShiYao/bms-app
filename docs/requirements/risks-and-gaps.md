# S16100B 固件 · 跨域风险 / 缺口 / 疑似 Bug 清单

> 来自各域逆向提炼时的交叉发现。本清单**不是需求**，而是提炼过程暴露的、需产品/安全负责人裁定的问题。
> 严重度：🔴 安全关键（可能危及电池安全）/ 🟠 功能缺口（功能宣称存在但实际未生效）/ 🟡 代码质量/待确认。
> 每项给出关联需求 ID 与源码位置，便于回查。

## 🔴 安全关键

| # | 问题 | 关联 | 源码 | 说明 |
|---|---|---|---|---|
| S1 | **独立看门狗未启用** | REQ-PWR-024 | `main.c:143` | `MX_IWDG_Init()` 被注释，`FeedIwdg()` 无法触发。系统死锁/跑飞无硬件恢复。正式产品重大缺口。 |
| S2 | **升级期间保护进入盲区** | REQ-OTA-006/013, REQ-LOG-011 | `upgrade.c:deal_ota_req()` | 升级以 `while(1)` 独占主循环，所有保护任务停摆；进安全态/断接触器代码（`PowerDownAllRelayOff`、置 STANDBY）已被注释。升级中电池无软件保护。 |
| S3 | **上位机 Force_On 可覆盖故障断开** | REQ-CTRL-020, REQ-CTRL-017 | `BMS_Control.c:BmsMos_GetFinalState()` / `DealFault()` | SYS_FAULT 时 `BmsMos_AllOff()` 受 `ControlByBms` 约束；若上位机将某 MOS 设为 Force_On，故障时该路**不会断开**。与上电 `BmsControl_MosAllOffNow()`（直写硬件、绕过检查）行为不一致。 |
| S4 | **关机状态(POWEROFF)无明确安全关断** | REQ-CTRL-025 | `BMS_Control.c:BmsControlTask()` | `BMS_STA_POWEROFF` 分支为空 `break`，MOS 维持前一迭代状态，与上电强制全断不对称。 |
| S5 | **主循环阻塞 3s/2s** | REQ-CTRL-023, REQ-CHG-005 | `BMS_Control.c:Limit_Task()` | 裸机 `delay_ms(3000)`/`delay_ms(2000)` 期间所有软件保护/告警/SOC/LED 暂停，仅 AFE 硬件保护兜底。实时性安全隐患。 |
| S6 | **充电匹配保护完全失效** | REQ-CHG-007 | `charger.c`（整段注释） | `chgmatch_en` 仅在参数读写出现，无任何业务逻辑使用；不匹配充电器无法被拦截。 |
| S7 | **充电器在线检测应用层无效** | REQ-CHG-001, REQ-HEAT-006 | `AppAfe_HasCharger()` | `HaveCharger` 从未被赋值，恒返回 0；依赖它的逻辑（如加热的充电机条件）失准。对比 `HaveLoad` 有正常更新路径。 |
| S8 | **主动加热可在静置时持续耗电** | REQ-HEAT-006 | `heatcontrol.c:HeatControlTask()` | 主动加热中 `HaveCharger` 开/关条件被注释，无充电机静置时也能自主加热 → 过放风险。需确认是否有意。 |
| S9 | **热失控无恢复路径、无日志** | REQ-PROT-020 | `WarningTask.c` | `ThermalRunaway` 触发后永久保持，仅硬复位可解，且无日志记录，安全链可追溯性存疑。 |
| S10 | **SOH 下限保护被注释，存在除零风险** | REQ-SOC-014 | `SocTask.c:CalSOH()` | 防异常容量的下限检查（`> oldCap×70%`）注释掉；`CurrentCap_mAH` 可被更新为极小值 → `SOC=RemainEnergy/CurrentCap` 溢出/除零。 |

## 🟠 功能缺口（定义存在但未生效）

| # | 问题 | 关联 | 源码 | 说明 |
|---|---|---|---|---|
| G1 | **确认 Bug：放电 MOS 断路故障永不上报** | REQ-PROT-026 | `WarningTask.c` | 条件 `Vmos_mv < 0 && Vmos_mv > 1000` 自相矛盾恒为假。疑似应为 `Vmos_mv > -1000`。 |
| G2 | **确认 Bug：日志环绕检索分支** | REQ-LOG-006 | `DataLog.c:SearchIndex()` 383-389 | 两个 `if/else if` 条件相同（均 `minsec<maxsec`），真正环绕（`minsec>maxsec`）永不置 `loopflag=1`，跨环读日志顺序错误。 |
| G3 | **显示 SOC 映射从未生效** | REQ-SOC-013 | `SocTask.c:SocRefreshDisplaySoc()` | 映射函数返回值被 `(void)` 丢弃，且无 `DisplaySoc` 字段；对外上报原始内部 SOC（底部 10% 截断未实现）。 |
| G4 | **主动上报缺失（纯被动查询）** | REQ-COMM-011 | `UpperComTask.c:CreatUpperSndInfo()` | `CMD_SEND_BATINFO/WARNING` 命令码已定义但无实现；告警须上位机轮询，与"避免告警恢复后丢记录"的注释意图矛盾。 |
| G5 | **充/放 MOS 强制控制命令有结构无执行** | REQ-COMM-014 | `UpperComTask.c:CMD_Task()` | 寄存器 1500/1501/1502 写入路径完整，但无对应执行调用。安全相关命令的实现缺口（与 S3 需一并裁定）。 |
| G6 | **上位机 Restart 命令为空操作** | REQ-COMM-013 | `UpperComTask.c` | 写 `Restart=0x1F` 仅清标志，未调用任何复位（无 `NVIC_SystemReset()`）。 |
| G7 | **校准系数上电未加载** | REQ-PARA-013 | `ParaSet.c:initParameter()` 735 | `ReadCorrectOffset()` 注释，电压/电流校准系数每次上电恢复默认，生产标定值被忽略 → 测量精度。 |
| G8 | **休眠触发逻辑与参数脱节** | REQ-PWR-005/020/021 | `BMS_Control.c` / `pmu.c`(缺失) | `PoweroffVolt_mV`、`DayToSleep`、`HourToWakeup` 参数可读写，但比较/进休眠调用逻辑在发布代码中找不到；`pmu.c` 实现缺失，休眠入口无法核实（出口 `PWR_FLAG_SB` 唤醒已实现）。 |
| G9 | **断电续传缺失** | REQ-OTA-004 | `upgrade.c` | 升级进度仅存 RAM（`revcodeInfo`），断电须重新完整升级。 |
| G10 | **LEVEL_2 是僵尸等级** | REQ-PROT-001 | `WarningTask.c:CreatWaring()` | 从不设置任何项到 LEVEL_2，依赖 LEVEL_2 门限的 `needsleep()` 休眠保护永不触发。 |
| G11 | **被动加热请求变量无赋值来源** | REQ-HEAT-002/003/004 | `heatcontrol.c` | `PsssiveChgHeatReq`/`PsssiveDscHeatReq` 只有定义和读取，无写入路径；无法确认 PCS 如何触发被动加热（疑在库内）。 |
| G12 | **RUN 灯心跳接口无调用** | REQ-LED-007 | `Led.c` | 心跳接口存在但主循环未调用。 |

## 🟡 代码质量 / 待确认

| # | 问题 | 关联 | 源码 | 说明 |
|---|---|---|---|---|
| Q1 | **均衡启动电压两套参数冲突** | REQ-AFE-015 | `ParaSet.h` vs AFE 参数 | 应用层 `DEFAULTBALANCESTARTVOLT=1500mV` 与 AFE 层 `_E2_BAL_VOL=4200mV` 相差 2.8V；前者疑为测试残留，电压极低即触发均衡。须定哪套为准。 |
| Q2 | **电芯数硬编码 = 2** | REQ-AFE-001 | `BMS_Info.h:TOTAL_AFE_CELL_NUM` | AFE 芯片与计算层支持 20 路，应用层仅 2；与实际串数不符会截断数据、影响保护完整性。 |
| Q3 | **AverageTemp 类型低温溢出** | REQ-SOC-020 | `BMS_Info.h` | `uint16_t` 存可能为负的均值，低温无符号回绕；SOC 侧已强转规避，其他直读路径有风险。 |
| Q4 | **参数完整性仅抽样无 CRC** | REQ-PARA-002 | `ParaSet.c:CheckPara()` | 仅嗅探 6 个字段特征值；部分字段损坏而被检字段幸存时，损坏被静默忽略 → 用错保护阈值。 |
| Q5 | **参数块无大小静态断言** | REQ-PARA-005 | `ParaSet.c` | 8 个结构体合写单个 4096B 扇区，无 `static_assert`；扩展超限会静默溢出覆盖备份扇区，双区保护同时失效。 |
| Q6 | **CSV 缓冲溢出静默截断** | REQ-LOG-015 | `AppCsvLog.c` | `snprintf` 溢出检查分支为空体；64 列接近 400B 上限，溢出无告警。 |
| Q7 | **放电过流 L3 恢复 180s 不对称** | REQ-PROT-012 | `WarningTask.c` | 放电过流 L3 恢复 180s，充电过流 5s，差 36 倍且无注释；需确认是否有意。 |
| Q8 | **PKT_COMP 死状态 / CRC 大端死代码** | REQ-OTA-004/005 | `upgrade.c`, `AppComLink.c` | "发送完成"态从未赋值；`src_crc16_big` 计算后未用。 |

---

## 建议处置优先级

1. **立即裁定 🔴 全部**：S1（看门狗）、S2（升级保护盲区）、S3+G5（远程控制覆盖安全）三项直接关系功能安全，应在任何量产/移植前定论。
2. **确认两个 Bug**：G1（放电 MOS 断路条件）、G2（日志环绕检索）逻辑明确错误，建议直接修复或在新固件中规避。
3. **澄清"注释失效"项**：S6/S7/S8/G7/G11 等大量功能被注释屏蔽——需确认是"未完成"还是"有意停用"，避免新固件照搬失效逻辑。
4. **参数与存储健壮性**：Q4/Q5 关系到保护阈值可信度，重构时应引入 CRC + 静态断言。
