# REQ-OTA：固件升级（OTA）需求规格

> **覆盖源文件**
> - `Application/upgrade.c`
> - `Application/upgrade.h`
> - `Application/UpperComTask.c`（`UpperComUpgradeTask()`、`UpperCom_IsUpgrading()`）
> - `Application/AppComLink.c`（`Is485RevUpgradeOK()`）
> - `Application/AppStorageMap.h`
> - `Driver/BSP/ex_flash.h`
> - `Core/Src/main.c`（主循环升级调度、日志暂停逻辑）

---

## 需求列表

### REQ-OTA-001  升级触发与通道

| 属性 | 内容 |
|---|---|
| 类型 | 功能 / 接口 |
| 安全相关 | 否 |
| 来源（源码） | `UpperComTask.c:UpperComUpgradeTask()`（L1154–1179）；`upgrade.h`（`OTA_REQ=0x02`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 当系统在主循环中检测到升级专用串口（RS-485/UART，`Rev485_ForUpGrade`）收到协议帧且帧协议类型为 `PROTO_USERDEF`、命令码为 `CMD_UPGRADE_REQ`（`OTA_REQ=0x02`）时，系统应进入升级流程（调用 `deal_ota_req()`），并置标志 `upgrading=1` 以通知其他任务。

**理由 / 代码依据**
> `UpperComUpgradeTask()` 在每次主循环被调用；命令通过专用升级端口（`Rev485_ForUpGrade`）接收，与正常 Modbus 通信端口分离，避免升级帧与业务帧混淆。`PROTO_USERDEF=0x01` 区别于 `PROTO_MODBUS=0x00`。

**验收准则**
- Given 系统处于正常运行态 When 从升级串口收到正确地址的 `OTA_REQ` 帧 Then `upgrading` 标志置 1，系统进入 `deal_ota_req()` 处理。

---

### REQ-OTA-002  升级请求帧长度与参数校验

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `upgrade.c:deal_ota_req()`（L288–381） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 当系统接收到升级请求（`OTA_REQ`）时，系统应按以下顺序校验 payload（9 字节），任何一项不符即返回对应错误码并中止升级：
> 1. payload 长度必须为 9 字节（否则返回 `DATA_LENGTH_ERRO=0x00`）；
> 2. 升级目标设备字段中，BCU 位与 BMU 位不得同时置位（否则返回 `UP_DEV_ERROR=0x08`）；
> 3. 固件总大小（`totalsize`，4 字节大端）不得超过 `MAX_CODE_SPACE=0x4B000`（300 KB，否则返回 `CODE_SIZE_ERRO=0x01`）；
> 4. 分包大小（`pktsize`，2 字节）不得超过 `MAX_PKT_SIZE=1024` 字节（否则返回 `PKT_SIZE_ERRO=0x02`）；
> 5. 上位机声明的分包数量（`pktnum`，2 字节）必须等于 `ceil(totalsize / pktsize)`（否则返回 `PKT_NUM_ERRO=0x03`）。

**理由 / 代码依据**
> 全部校验在 `deal_ota_req()` 中按序执行（L296–348）。通过后才擦除目标 Flash 区域并回复 `REV_PKT_OK=0x80`，随即进入分包接收循环。

**验收准则**
- Given 升级请求到来 When payload 不足 9 字节 Then 回复 `DATA_LENGTH_ERRO`，不进入升级。
- Given 请求合法 When `totalsize > 0x4B000` Then 回复 `CODE_SIZE_ERRO`，不擦 Flash。
- Given 请求合法 When `pktsize > 1024` Then 回复 `PKT_SIZE_ERRO`。
- Given 请求合法 When 声明 `pktnum` ≠ `ceil(totalsize/pktsize)` Then 回复 `PKT_NUM_ERRO`。
- Given 所有校验通过 Then 回复 `REV_PKT_OK` 并将 `upgradeInfo.Step` 置为 `PKT_SEND`。

---

### REQ-OTA-003  目标 Flash 分区擦除

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `upgrade.c:deal_ota_req()`（L352）；`AppStorageMap.h`（L7–8） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 当升级请求参数校验全部通过后，系统应在开始接收分包之前，对片外 SPI Flash（W25Q16）的固件缓存区（起始地址 `CODE_SAVE_ADDR_25Q16=0x00000`，大小 `MAX_CODE_SPACE_25Q16=0x4B000`，即 300 KB，sec 0–75）执行整区擦除操作（`AppStorage_Erase`）。

**理由 / 代码依据**
> 擦除在 `deal_ota_req()` L352 执行，确保旧固件数据不残留。擦除后才回复 ACK 并切换状态 `PKT_SEND`。

**验收准则**
- Given 升级请求参数合法 When 系统开始分包接收前 Then 地址 `0x00000`–`0x4AFFF` 的 SPI Flash 内容被全部擦除（读回均为 `0xFF`）。

---

### REQ-OTA-004  固件分包接收与顺序校验

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `upgrade.c:deal_rev_pkt()`（L171–264）；`upgrade.h`（`OTA_REV_PKT=0x03`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 在 `PKT_SEND` 状态下，系统应对每个到来的分包帧（`OTA_REV_PKT=0x03`）执行以下处理：
> 1. 校验数据内容长度（不含 2 字节帧序号）不超过协商的 `pktsize`，否则返回 `DATA_LENGTH_ERRO`；
> 2. 校验帧中携带的 2 字节序号（大端，`CurPktNum`）等于已接收包计数 +1（`revcodeInfo.code.pktnum+1`），否则返回 `PKT_NUM_ERRO=0x03`；
> 3. 将数据写入 Flash 偏移地址 `FLASH_CODE_PAGE + (CurPktNum-1) × pktsize`；
> 4. 回写校验（`check_data()`）：逐字节比对，若不一致则返回 `RESEND_CUR_PKT=0x05` 要求重发；
> 5. 写入成功后，`revcodeInfo.code.pktnum++`，`revcodeInfo.code.totalsize += codelen`，并回复 `REV_PKT_OK=0x80`。

**理由 / 代码依据**
> 顺序强制机制确保不乱序；回写校验（read-back verify）检测 Flash 写入可靠性；`RESEND_CUR_PKT` 使上位机重传当前包而不需重启升级会话。

**验收准则**
- Given 系统处于 `PKT_SEND` When 分包序号与期望不符 Then 回复 `PKT_NUM_ERRO`，当前序号计数不变。
- Given 分包序号正确 When Flash 写入后回读不一致 Then 回复 `RESEND_CUR_PKT`，包计数不前进。
- Given 分包正确写入 Then 回复 `REV_PKT_OK`，`revcodeInfo.code.pktnum` 加 1。

---

### REQ-OTA-005  通信帧 CRC-16/Modbus 校验

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `AppComLink.c:Is485RevUpgradeOK()`（L105–160）；`upgrade.c:Send_Ack()`（L30–53） |
| 验证方法 | 测试 / 检视 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 系统应对所有升级通道上收到的帧执行 CRC-16/Modbus 校验；帧头字节 `0xA5 0x5A`（`HEAD0/HEAD1`）及设备地址必须匹配，且尾部 2 字节 CRC 必须与计算值一致，否则丢弃该帧。发送的所有 ACK 帧也应附加 CRC-16/Modbus（2 字节大端，附于帧尾）。

**理由 / 代码依据**
> `Is485RevUpgradeOK()` 同时兼容大端/小端 CRC 比对（`src_crc16` 和 `src_crc16_big`），以应对上位机字节序差异（存疑，见"存疑与观察"）。ACK 通过 `Cal_CRC16ofModBus()` 计算并追加。

**验收准则**
- Given 收到升级帧 When CRC 不符 Then 帧被丢弃，不触发任何升级动作。
- Given CRC 正确、帧头正确、地址匹配 Then `Is485RevUpgradeOK()` 返回 `0x80`。

---

### REQ-OTA-006  升级完成后写升级标志并系统复位

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `upgrade.c:deal_rev_pkt()`（L237–263） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 当所有分包接收完毕（`revcodeInfo.code.totalsize == upgradeInfo.code.totalsize` 且 `revcodeInfo.code.pktnum == upgradeInfo.code.pktnum`）时，系统应：
> 1. 将升级标志 `NEED_UPGRADE=0xAA`（2 字节）写入片外 Flash `UPDATE_FLAG_ADDR=0x1FF000`（扇区 511）；
> 2. 写入后回读校验，若失败则重试，最多重试 20 次；
> 3. 20 次写入均失败时放弃升级并返回（不复位）；
> 4. 写入成功后延时 2000 ms，随后关中断（`__disable_irq()`），调用 `AppPower_Reset()`（`NVIC_SystemReset()`）触发软件复位，交由 Bootloader 执行固件搬运。

**理由 / 代码依据**
> Bootloader 上电后读取 `UPDATE_FLAG_ADDR`；若值为 `NEED_UPGRADE=0xAA` 则执行从 SPI Flash 到内部 Flash 的代码搬运，完成升级。此路径直接导致系统复位，属于安全相关操作。

**验收准则**
- Given 所有分包接收完毕 When 升级标志写入且回读为 `0xAA` Then 2000 ms 后系统复位，进入 Bootloader 升级流程。
- Given 升级标志写入 20 次均失败 Then 系统不复位，升级状态复位为 `PKT_NONE`。

---

### REQ-OTA-007  升级超时检测与会话中止

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `upgrade.c:UpgradeOverTime()`（L266–285）；`upgrade.h`（`MAXOPTIME=10000 ms`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 在升级分包传输过程中，若连续 10 000 ms（`MAXOPTIME=10*1000`）无新数据包到达，系统应中止当前升级会话，将 `upgradeInfo.Step` 置为 `PKT_NONE`，并向上位机回复 `REV_OVER_TIME=0x06`，退出分包接收循环。

**理由 / 代码依据**
> `UpgradeOverTime()` 在 `deal_ota_req()` 内的 `while(1)` 循环中每次迭代被调用（L378–379）；`upgradeInfo.ServerOPTime` 在每收到一个有效分包时刷新（`deal_rev_pkt()` L187），也在请求成功时初始化（L363）。

**验收准则**
- Given 系统处于 `PKT_SEND` When 超过 10 000 ms 无分包到达 Then `upgradeInfo.Step` 置 `PKT_NONE`，上位机收到 `REV_OVER_TIME` 应答，退出升级循环。
- Given 每收到有效分包 Then 超时计时器重置（`ServerOPTime` 更新为当前 `u32SysTime`）。

---

### REQ-OTA-008  升级期间暂停日志写入

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `Core/Src/main.c`（L238–246）；`UpperComTask.c:UpperCom_IsUpgrading()`（L1150–1153） |
| 验证方法 | 测试 / 检视 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 在升级进行期间（`UpperCom_IsUpgrading() != 0`），系统应暂停对片外 Flash 的周期性日志写入操作：
> - 每 20 s 一次的 Flash 日志索引写入（`SaveLogInIndex()`）被跳过；
> - 每 2 s 一次的 SD 卡 CSV 数据记录（`CSV_WriteData()`）被跳过。

**理由 / 代码依据**
> 升级期间 `deal_ota_req()` 内的 `while(1)` 循环独占执行，主循环被阻塞，`UpperCom_IsUpgrading()` 返回 1。日志写入使用同一片外 Flash，与固件数据写入存在地址冲突风险；暂停可避免数据覆盖。

**验收准则**
- Given `upgrading=1` When 20 s 定时器到期 Then `SaveLogInIndex()` 不被调用。
- Given `upgrading=1` When 2 s 定时器到期 Then `CSV_WriteData()` 不被调用。

---

### REQ-OTA-009  升级期间看门狗持续喂狗

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `upgrade.c:deal_ota_req()`（L375–377）；`upgrade.c:deal_rev_pkt()`（L251–253）；`upgrade.c:CanSetReStartFlag()`（L403–405）；编译条件 `USE_WDT` |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 在升级分包接收循环及升级标志写入重试循环中，系统应在每次迭代时调用 `IWDG_ReloadCounter()` 以防止看门狗超时复位；该行为受编译宏 `USE_WDT` 控制。

**理由 / 代码依据**
> 升级期间主循环被 `deal_ota_req()` 内的 `while(1)` 独占，若不在循环体内喂狗，IWDG 到期会触发非预期复位，导致升级中断且 Flash 处于不一致状态。

**验收准则**
- Given `USE_WDT` 编译宏有效 When 升级分包循环运行超过 IWDG 超时周期 Then 系统不因看门狗超时而复位。
- Given 写升级标志重试循环运行 When 20 次重试期间看门狗到期前 Then `IWDG_ReloadCounter()` 在每次重试时被调用。

---

### REQ-OTA-010  版本信息读取

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `upgrade.c:read_para()`（L91–145）；`upgrade.h`（`CMD_PARA_READ=0x11`，`bcmsversion[]`）；`UpperComTask.c`（L116–118） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 当系统收到版本信息读取命令（`CMD_PARA_READ=0x11`）时，应返回 4 个寄存器（地址 0–3，`MinDevInfoREGADDR=0`，`MaxDevInfoREGADDR=3`），内容为固件版本号数组 `bcmsversion[]` 的前 4 字节（当前值 `{26,5,27,1,0,3}`，含日期与主次版本）；版本信息同时也通过 Modbus 寄存器（PackSta 表偏移 349–351）对外发布。

**理由 / 代码依据**
> `bcmsversion[]` 在 `upgrade.c` L28 硬编码。`read_para()` 仅响应寄存器范围 0–3；越界时回复 `ERRO_REG=0x02`。Modbus 侧由 `UpperComTask.c` L116–118 填充。

**验收准则**
- Given 上位机发送 `CMD_PARA_READ`，寄存器范围 0–3 When 系统处理 Then 返回 `bcmsversion[0..3]` 各 2 字节（低字节为版本值，高字节为 0）。
- Given 请求寄存器范围超出 0–3 When 处理 Then 返回错误码 `ERRO_REG`。

---

### REQ-OTA-011  升级目标设备选择（BCU/BMU）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `upgrade.c:deal_ota_req()`（L304，L311–317）；`upgrade.h`（`DEV_BCU=0`，`DEV_BMU0=1`，`DEV_BMU1=2`） |
| 验证方法 | 测试 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 升级请求 payload 首字节为目标设备位掩码，支持 BCU（bit 0）、BMU0（bit 1）、BMU1（bit 2）；BCU 位与任意 BMU 位不得同时置位，违反则返回 `UP_DEV_ERROR=0x08` 并中止升级。

**理由 / 代码依据**
> `upgradeInfo.Device` 存储设备掩码；`DEV_BMUMASK = BIT(DEV_BMU0)|BIT(DEV_BMU1)` 用于检测 BCU 与 BMU 同时置位的非法组合（L311–317）。

**验收准则**
- Given `Device` 同时设置 BCU 位和 BMU 位 When 系统处理升级请求 Then 返回 `UP_DEV_ERROR`，不进入升级。

---

### REQ-OTA-012  CAN 通道升级接口（声明存在，未在主循环调用）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `upgrade.h`（`CanRevUpdatePara`、`CanRevUpdatePkt`、`CanSetReStartFlag`、`CanRestartSys`、`CanCheckUpdateOverTime` 函数声明） |
| 验证方法 | 检视 |
| 状态 | 存疑 |

**需求描述**
> `upgrade.h` 声明了 CAN 通道升级相关接口（`CanRevUpdatePara`、`CanRevUpdatePkt`、`CanSetReStartFlag`、`CanRestartSys`、`CanCheckUpdateOverTime`），表明设计上支持通过 CAN 总线对 BMU 子节点进行固件升级，但当前 `upgrade.c` 中未发现这些函数的实现体，主循环也无调用。

**理由 / 代码依据**
> 头文件中仅有声明，实现文件中不存在对应定义，推测为计划实现功能或位于尚未包含的源文件中。

**验收准则**
- Given CAN 升级接口被实现时 When 主循环调用 `CanCheckUpdateOverTime()` Then 应具备与 RS-485 升级同等的超时保护和写标志逻辑。

---

### REQ-OTA-013  升级完成时禁用全局中断

| 属性 | 内容 |
|---|---|
| 类型 | 安全 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `upgrade.c:deal_rev_pkt()`（L261） |
| 验证方法 | 检视 |
| 状态 | 已实现（来自代码） |

**需求描述**
> 在升级完成、准备执行软件复位之前，系统应调用 `__disable_irq()` 禁用全局中断，以确保复位操作的原子性，防止中断服务程序在复位序列中修改系统状态。

**理由 / 代码依据**
> `deal_rev_pkt()` L261：`__disable_irq()` 紧接在 `delay_ms(2000)` 之后、`AppPower_Reset()` 之前执行。

**验收准则**
- Given 升级标志写入成功 When 2000 ms 延时结束 Then 全局中断被禁用后立即执行 `NVIC_SystemReset()`。

---

## 存疑与观察

1. **CRC 大端/小端双重比对（REQ-OTA-005）**：`Is485RevUpgradeOK()` 中同时计算了 `src_crc16`（小端读）和 `src_crc16_big`（大端读），但判断条件只用了 `src_crc16 == cal_crc16`（L138），`src_crc16_big` 被计算但未使用。疑为调试残留或兼容性占位，存在死代码风险，建议清理或补充注释说明意图。

2. **`PKT_COMP` 状态从未被赋值**：`upgrade.h` 定义了四态状态机 `UPGRADE_STEP_S`（`PKT_NONE/PKT_REQ/PKT_SEND/PKT_COMP`），但 `PKT_COMP`（发送完成）在 `upgrade.c` 全文中从未被赋值使用。全分包接收完毕后直接置回 `PKT_NONE`（L241），跳过了 `PKT_COMP` 状态，导致外部通过 `GetCurrentUpdataSta()` 查询时无法观察到"传输完成待复位"这一中间状态。

3. **升级期间不禁止充放电/不进安全态**：代码中注释掉了 `PowerDownAllRelayOff()`（`UpperComTask.c` L1167）和设置 `Bms.sta = STANDBY`（L1171），即升级期间继续维持正常充放电状态。升级循环独占主循环，`BmsControlTask`、保护任务、`HeatControlTask` 等均不会被调度，存在升级期间保护逻辑失效的安全风险。建议评估是否需要在进入升级前断开主接触器或进入安全态。⚠️

4. **断电续传缺失**：当前实现无断电续传机制。若升级途中断电，`revcodeInfo` 中的进度（已接收包计数、总大小）存于 RAM，上电后全部丢失；SPI Flash 的固件缓存区也不会自动恢复，需重新发起完整升级。代码中无持久化进度标记，属于功能缺口。

5. **版本号一致性**：`bcmsversion[]` 在 `upgrade.c` 硬编码为 `{26,5,27,1,0,3}`，同时也在 `UpperComTask.c` Modbus 寄存器表（L116–118）中发布，两处使用同一数组，一致性有保证；但版本含义（字节含义为年、月、日、主版本等）无官方注释，仅有行内注释"2023年 5月 19日 版本 v1.0"，建议补充字段说明。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-OTA-001 | 升级触发与通道 | 否 | `UpperComTask.c:UpperComUpgradeTask()` | 已实现 |
| REQ-OTA-002 | 升级请求帧长度与参数校验 | 否 | `upgrade.c:deal_ota_req()` | 已实现 |
| REQ-OTA-003 | 目标 Flash 分区擦除 | 否 | `upgrade.c:deal_ota_req()` | 已实现 |
| REQ-OTA-004 | 固件分包接收与顺序校验 | 否 | `upgrade.c:deal_rev_pkt()` | 已实现 |
| REQ-OTA-005 | 通信帧 CRC-16/Modbus 校验 | 否 | `AppComLink.c:Is485RevUpgradeOK()` | 已实现 |
| REQ-OTA-006 | 升级完成后写升级标志并系统复位 | 是 ⚠️ | `upgrade.c:deal_rev_pkt()` | 已实现 |
| REQ-OTA-007 | 升级超时检测与会话中止 | 否 | `upgrade.c:UpgradeOverTime()` | 已实现 |
| REQ-OTA-008 | 升级期间暂停日志写入 | 否 | `main.c`；`UpperComTask.c:UpperCom_IsUpgrading()` | 已实现 |
| REQ-OTA-009 | 升级期间看门狗持续喂狗 | 是 ⚠️ | `upgrade.c:deal_ota_req()` / `deal_rev_pkt()` | 已实现 |
| REQ-OTA-010 | 版本信息读取 | 否 | `upgrade.c:read_para()` | 已实现 |
| REQ-OTA-011 | 升级目标设备选择（BCU/BMU） | 否 | `upgrade.c:deal_ota_req()` | 已实现 |
| REQ-OTA-012 | CAN 通道升级接口（声明存在，未实现） | 否 | `upgrade.h` | 存疑 |
| REQ-OTA-013 | 升级完成时禁用全局中断 | 是 ⚠️ | `upgrade.c:deal_rev_pkt()` | 已实现 |
