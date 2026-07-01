# REQ-COMM：上位机通信 需求规格

> 覆盖源文件：
> - `Application/UpperComTask.c` / `UpperComTask.h`
> - `Application/AppComLink.c` / `AppComLink.h`
> - `Application/AppModbus.c` / `AppModbus.h`
> - `Application/AppCom.c` / `AppCom.h`
> - `Application/AppTime.c` / `AppTime.h`（通信活跃超时）
> - `Core/Src/usart.c`（波特率/物理层配置）

---

## 需求列表

---

### REQ-COMM-001  双 RS-485 物理接口

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `Core/Src/usart.c:MX_USART1_UART_Init()` / `MX_UART4_Init()`；`AppCom.h:AppComPort_t` |
| 验证方法 | 检视 / 测试 |
| 状态 | 已实现 |

**需求描述**
> 系统应始终通过两个独立的 RS-485 串口（USART1：PC9/PA10；UART4：PC10/PC11）对外提供上位机通信能力；两路端口参数均为 **9 600 bps、8 数据位、无校验、1 停止位（9600-8-N-1）**，并使用 DMA 进行收发。

**理由 / 代码依据**
> `MX_USART1_UART_Init()` 与 `MX_UART4_Init()` 均将 `BaudRate = 9600`、`UART_PARITY_NONE`、`UART_STOPBITS_1`；`AppComPort_t` 枚举定义 `APP_COM_PORT_USART1 = 1` 和 `APP_COM_PORT_UART4 = 4`；TX 均通过 DMA（`usart1_dma_tx_data` / `usart4_dma_tx_data`）发送。

**验收准则**
- Given BMS 上电 When 上位机以 9 600-8-N-1 向 USART1 或 UART4 发送合法帧 Then BMS 应在同一端口回应应答帧。

---

### REQ-COMM-002  通信端口动态选择

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppCom.c:AppCom_OnRs485Frame()` / `AppCom_SelectPort()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当收到来自 USART1 的帧时，系统应将当前活动端口切换为 USART1（`usart1_comm = 1`）；当收到来自 UART4 的帧时，系统应将当前活动端口切换为 UART4（`usart1_comm = 0`）；后续所有 DMA 发送均通过该活动端口输出。

**理由 / 代码依据**
> `AppCom_SelectPort()` 根据传入的 `AppComPort_t port` 设置 `usart1_comm`；`AppCom_SendDmaByCurrentPort()` 读取 `usart1_comm` 决定实际 DMA 发送端口。

**验收准则**
- Given 上位机先后分别从 UART4、USART1 发送查询帧 When 每次收到帧 Then BMS 应分别从 UART4、USART1 回应，不交叉。

---

### REQ-COMM-003  帧边界检测与最小长度过滤

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppCom.c:AppCom_OnRs485Frame()`；`AppComLink.c:Is485RevOK()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当收到的帧字节数 ≤ 4 时（不含恰好 4 字节的打印模式切换命令），系统应丢弃该帧，不进行任何解析；当收到以 `0x7E` 开头的帧时，系统应同样丢弃；当帧长度 < 6 字节时（在链路层校验时），系统应清除接收缓冲并返回错误。

**理由 / 代码依据**
> `AppCom_OnRs485Frame()` 中 `if(len <= 4) return;`（注：4 字节打印切换命令例外已单独处理）；`if(buf[0] == 0x7e) return;`；`Is485RevOK()` 中 `if (Rev485.revLen < 6)` 后执行 `memset` 清缓冲并返回。

**验收准则**
- Given 上位机发送 3 字节短帧 When BMS 接收 Then BMS 应无应答且接收缓冲清零。

---

### REQ-COMM-004  私有协议帧格式与 CRC16 校验

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `AppComLink.h`；`AppComLink.c:Is485RevOK()` |
| 验证方法 | 测试 / 检视 |
| 状态 | 已实现 |

**需求描述**
> 私有协议（`PROTO_USERDEF`）帧格式为：`[HEAD0=0xA5][HEAD1=0x5A][ADDR][CMD][LEN_L][LEN_H][DATA…][CRC_H][CRC_L]`（CRC 采用 Modbus CRC-16，覆盖从 HEAD0 到 DATA 末尾；LEN 字段含 2 字节 CRC 本身）。当帧满足 CRC 匹配、帧头 `0xA5 0x5A`、地址等于 `Bms.Address` 三个条件时，系统应接受该帧，解析为私有协议命令；否则应丢弃。

**理由 / 代码依据**
> `Is485RevOK()` 中：`cal_crc16 = Cal_CRC16ofModBus(buf, revLen-2)`；同时检查 `buf[0]==FRAME_HEAD0 && buf[1]==FRAME_HEAD1 && buf[2]==Bms.Address`；数据长度 `upperrev.datalen = Rev485.buf[4]|(Rev485.buf[5]<<8); upperrev.datalen -= 2;`（减去 CRC）。

**验收准则**
- Given 一帧私有协议帧 When CRC 字段故意篡改 1 位 Then BMS 应无应答。
- Given 一帧私有协议帧 When 地址字段不等于 Bms.Address Then BMS 应无应答。

---

### REQ-COMM-005  Modbus RTU 帧格式与 CRC16 校验

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `AppComLink.c:Is485RevOK()`；`AppModbus.h` |
| 验证方法 | 测试 / 检视 |
| 状态 | 已实现 |

**需求描述**
> Modbus RTU 帧格式为标准 `[ADDR][FUNC][DATA…][CRC_L][CRC_H]`（CRC 低字节在前）。当帧的 CRC16（Modbus 标准大端字节序检验）匹配且 `buf[0] == Bms.Address` 时，系统应接受该帧，解析为 Modbus 命令；地址不匹配则应打印错误并丢弃。

**理由 / 代码依据**
> `Is485RevOK()` 第二分支：`src_crc16_big = buf[revLen-1]<<8 | buf[revLen-2]`，与 `cal_crc16` 比较；`Bms.Address == Rev485.buf[0]` 地址比较；`upperrev.proto = PROTO_MODBUS`；`printf("CMD_ERRO\n")` 地址不匹配时输出。

**验收准则**
- Given 合法 Modbus 0x03 读帧 When 地址匹配 Then BMS 应在 100 ms 内回应 Modbus 0x03 应答帧。
- Given 合法 Modbus 帧 When 地址不匹配 Then BMS 应无应答。

---

### REQ-COMM-006  接收缓冲区规格

| 属性 | 内容 |
|---|---|
| 类型 | 约束 |
| 安全相关 | 否 |
| 来源（源码） | `AppComLink.h:_Rev485_s`；`bsp_usart.h` |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述**
> 系统应维护两个独立的 RS-485 接收缓冲区（普通通信 `Rev485` 和升级通信 `Rev485_ForUpGrade`），每个缓冲区最大容量为 **1 200 字节**；USART1 和 UART4 的硬件 RX 缓冲区均为 1 200 字节；每帧接收后应记录 `revTime`（毫秒时间戳）和 `revLen`。

**理由 / 代码依据**
> `_Rev485_s.buf[1200]`；`#define USART1_RX_BUF_SIZE 1200` / `USART4_RX_BUF_SIZE 1200`；`Rev485.revTime = u32SysTime`。

**验收准则**
- Given 最大帧（1 200 字节）发送 When BMS 接收 Then 不应发生缓冲区溢出，帧应被完整存储。

---

### REQ-COMM-007  双协议自动识别与路由

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppCom.c:AppCom_OnRs485Frame()`；`AppComLink.c:Is485RevOK()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 系统应根据帧头自动区分协议类型：以 `0xA5 0x5A` 开头的帧路由至私有协议通道（升级/OTA）；其余帧路由至 Modbus 通道（正常通信）。私有协议帧的 Modbus 通道接收缓冲（`Rev485`）不清除（保留用于可能的升级数据），Modbus 帧验证后立即清除 `Rev485`。

**理由 / 代码依据**
> `AppCom_OnRs485Frame()` 中：`if((buf[0]==FRAME_HEAD0)&&(buf[1]==FRAME_HEAD1)) { AppCom_SaveUpgradeFrame(); return; }` 优先升级通道；否则 `AppCom_SaveNormalFrame()` + `UpperComTask()`；`Is485RevOK()` 中 `if (upperrev.proto == PROTO_MODBUS) memset(&Rev485,0,...)`（私有协议路径不清缓冲）。

**验收准则**
- Given 一帧以 0xA5 0x5A 开头的帧 When 到达 Then 应路由至升级任务，不触发 Modbus 命令处理。

---

### REQ-COMM-008  Modbus 功能码 0x03：读保持寄存器

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppModbus.h:MODBUS_CMD_READ`；`UpperComTask.c:DealModbusRead()` / `ModbusRead()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当收到 Modbus 功能码 0x03（读保持寄存器）命令时，系统应根据起始寄存器地址和寄存器数量，从对应的寄存器表段中读取数据并返回；读取前应先调用 `CreatModBusBuf()` 刷新全部寄存器表；寄存器数量为 0 或 ≥ 127 时应返回错误，不发送数据；寄存器范围超出已知地址段时应返回 Modbus 异常响应码（功能码 0x83，错误码 `ERRO_REG=0x02`）。

**理由 / 代码依据**
> `UpperComTask()` 中 `case MODBUS_CMD_READ: CreatModBusBuf(); DealModbusRead(...)`；`DealModbusRead()` 中地址段 `if/else if` 路由；`ModbusRead()` 中 `if (RegNum == 0 || RegNum >= 127) { printf("RegNum Err!"); return; }`；`ModbusSendErroAck(MODBUS_CMD_READ, ERRO_REG)` 非法地址时回复。

**验收准则**
- Given 起始寄存器 300 数量 10 When 上位机发 0x03 命令 Then BMS 应回应字节数 20 的 0x03 帧。
- Given RegNum = 0 或 RegNum = 127 When 上位机发 0x03 命令 Then BMS 应无应答（内部 printf 错误）。
- Given 起始地址 2000（越界）When 上位机发 0x03 命令 Then BMS 应回应 Modbus 异常帧（0x83, 0x02）。

---

### REQ-COMM-009  Modbus 功能码 0x10：写多个寄存器

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AppModbus.h:MODBUS_CMD_WRITE`；`UpperComTask.c:DealModbusWrite()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当收到 Modbus 功能码 0x10（写多个寄存器）命令时：
> 1. 目标地址在参数区（寄存器 20–299）时，系统应更新 BMS 参数、持久化保存、重新加载告警阈值，并回应标准 0x10 应答；
> 2. 目标地址在上位机控制区（寄存器 1500–1550）时，系统应更新 `UpperCmd`（包含接触器强制控制、关机、重启、复位、数据注入等命令）并回应 0x10 应答；
> 3. 目标地址在系统状态区（寄存器 300–399）时，系统应将写入数据解析为 RTC 时间并设置系统时间；
> 4. 目标地址超出所有已知可写范围时，系统应回应 Modbus 异常响应码（0x90, `ERRO_REG`）；
> 5. 数据长度 < 7 字节时应直接返回，不处理。

**理由 / 代码依据**
> `DealModbusWrite()` 中的 `if/else if/else` 分支；`GetValueFromTbl()` + `Bms.needsave |= ...` 参数区写入；`GetUpperFromTbl()` 控制区写入；`AppRtc_Set()` 状态区时间写入；`ModbusSendErroAck(MODBUS_CMD_WRITE,ERRO_REG)` 非法地址。

**验收准则**
- Given 写入参数区合法寄存器 When 写命令执行 Then BMS 应回应 0x10 应答，且 Flash 标记 `needsave` 置位。
- Given 数据长度 < 7 字节 When 写命令到达 Then BMS 应无应答，不更新参数。
- Given 写入越界地址 When 写命令到达 Then BMS 应回应 Modbus 异常帧（0x90, 0x02）。

---

### REQ-COMM-010  Modbus 寄存器地址映射

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `UpperComTask.h`（地址宏定义） |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述**
> 系统应实现以下寄存器地址空间分区（单位：16-bit 寄存器，每个 2 字节）：

| 地址段 | 范围 | 大小 | 含义 | 读/写 |
|---|---|---|---|---|
| BMS 参数区 | 20–299 | 280 | 保护阈值、均衡参数、功能使能等 | 读写 |
| 系统状态区 | 300–399 | 100 | Pack 状态、电压、电流、SOC/SOH、告警码、RTC | 读（写 RTC） |
| 模组电压区 | 400–499 | 100 | 各模组电压 | 只读 |
| 单体电压区 | 500–550 | 51 | 各单体电芯电压（mV） | 只读 |
| 单体温度区 | 1000–1050 | 51 | 各温度传感器值（0.1℃，偏移 +400） | 只读 |
| 上位机控制区 | 1500–1550 | 51 | 接触器控制、关机/重启/复位、数据注入 | 读写 |

**理由 / 代码依据**
> `UpperComTask.h` 中 `START_REG_*` / `END_REG_*` 宏。

**验收准则**
- Given 任意合法寄存器地址 When 0x03 读命令 Then 返回对应分区数据，字节序为大端（每寄存器高字节在前）。

---

### REQ-COMM-011  系统状态寄存器（300 区）内容与编码

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `UpperComTask.c:CreatPackInfoBuf()` |
| 验证方法 | 测试 / 检视 |
| 状态 | 已实现 |

**需求描述**
> 系统状态寄存器区（起始地址 300）应按序包含以下字段：

| 偏移（基 300） | 内容 | 编码 |
|---|---|---|
| 0 | 有效单体数 | 个 |
| 1 | 有效温度数 | 个 |
| 3 | 总电压 | mV/100（即 100 mV/bit） |
| 4 | 电流 | mA/100 + 32 000（偏移量 `SND_CUR_OFFSET=32000`，100 mA/bit） |
| 5 | SOC | 1% |
| 6 | SOH | 1% |
| 16 | 系统状态 | `Bms.sta` 枚举 |
| 17 | 开关状态 | Bit0=预充 Bit1=放电 Bit2=限流 Bit3=充电 Bit4=加热 |
| 18–21 | 告警码（1/2/3 级） | 各 32 bit 拆为 2 个 16-bit 寄存器 |
| 26 | 最高单体电压 | mV |
| 29 | 最低单体电压 | mV |
| 32 | 最高温度 | 0.1℃ + 400 |
| 35 | 最低温度 | 0.1℃ + 400 |
| 39–44 | RTC 时间（年/月/日/时/分/秒） | 原始值 |
| 45–48 | 累计充/放电量 | mAh，各 32 bit 拆 2 寄存器 |
| 49–51 | 固件版本 | 3 个 16-bit 字 |
| 52–53 | 单体均衡状态 | 32 bit 拆 2 寄存器 |

所有寄存器值字节序为大端（`endian_swap()` 逐寄存器交换）。

**理由 / 代码依据**
> `CreatPackInfoBuf()` 逐字段赋值，`SND_CUR_OFFSET=32000`（`UpperComTask.h`）；`TEMP_OFFSET=400`；结尾 `endian_swap()` 循环。

**验收准则**
- Given SOC = 50%，电流 = -10 000 mA（放电）When 上位机读寄存器 304（偏移 4）Then 应返回 `(-10000/100 + 32000) = 31900`（大端）。

---

### REQ-COMM-012  单体电压与温度上报编码

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `UpperComTask.c:CreatCellVoltBuf()` / `CreatCellTempBuf()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 单体电压寄存器（500 区）应按顺序存放所有激活单体的电压值（mV，1 mV/bit）；温度寄存器（1000 区）应按序存放 4 路单体温度 + MOS 温度 + 环境温度 + PTC 温度，编码为 0.1℃ + 400（即 25.0℃ = 650，`TEMP_OFFSET=400`）；所有值字节序为大端。

**理由 / 代码依据**
> `CreatCellVoltBuf()` 遍历 `Bms.CellInfo.ActiveCellVoltmV[i]`；`CreatCellTempBuf()` 硬编码 7 路温度（`ActiveCellTemp[0..3]`、`MOSTemp`、`ENVTemp`、`PTCTemp`），均加 400。注：`CONTemp`（接触器温度）已注释掉（`// RegTbl.CellTempTbl[offset++] = Bms.CONTemp+400;`）。

**验收准则**
- Given 单体 0 电压 = 3 500 mV When 读寄存器 500 Then 应返回 3 500（大端 0x0DAC）。
- Given MOS 温度 = 30.5℃（内部值 305，0.1℃单位）When 读对应温度寄存器 Then 应返回 305 + 400 = 705。

---

### REQ-COMM-013  上位机控制命令处理

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `UpperComTask.c:CMD_Task()` / `GetUpperFromTbl()`；`UpperComTask.h:UpperCMD_TypeDef` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当上位机通过 Modbus 写入控制寄存器区（1500–1550）时，系统应在 `CMD_Task()` 中周期性执行以下控制逻辑：
> - `LimitCtrl == Force_On (1)`：强制使能限流开关（`BmsControl_LimitForceOn()`）⚠️；
> - `LimitCtrl == Force_Off (2)`：强制关闭限流开关（`BmsControl_LimitForceOff()`）⚠️；
> - `Poweroff == 0x1F (UPPERCMD_ENABLE)`：触发正常关机流程（`AppPower_EnterNormalSleep()`），执行后自清零⚠️；
> - `Restart == 0x1F`：当前仅清零，**无实际重启动作**（缺口）；
> - `Reset == 0x1F`：恢复出厂参数（`SetDefaultPara()`）、保存 Flash、重新初始化 AFE，执行后自清零⚠️。

**理由 / 代码依据**
> `CMD_Task()` 函数；`UPPERCMD_ENABLE = 0x1F`（`UpperComTask.h`）；`UpperCmd.Restart = 0` 后无重启调用（`if(UpperCmd.Restart == UPPERCMD_ENABLE) { UpperCmd.Restart = 0; }` 为空操作）。

**验收准则**
- Given `LimitCtrl = Force_Off` 写入 1502 When `CMD_Task()` 运行 Then BMS 应调用 `BmsControl_LimitForceOff()`，限流触点断开。
- Given `Poweroff = 0x1F` 写入 1505 When `CMD_Task()` 运行 Then BMS 应进入睡眠模式，1505 寄存器自清零。

---

### REQ-COMM-014  远程强制接触器控制（充/放/预充 MOS）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `UpperComTask.h:UpperCMD_TypeDef`；`UpperComTask.c:GetUpperFromTbl()`；寄存器 1500–1502 |
| 验证方法 | 测试 |
| 状态 | 存疑 |

**需求描述**
> 上位机应可通过写入控制寄存器区（寄存器 1500=ChgMosCtrl、1501=DiscMosCtrl、1502=PrecMosCtrl）对充电 MOS、放电 MOS、预充 MOS 进行强制控制（0=BMS 自主控制，1=强制开，2=强制关）。

**理由 / 代码依据**
> `UpperCMD_TypeDef` 定义了 `ChgMosCtrl`、`DiscMosCtrl`、`PrecMosCtrl` 字段，枚举 `{ControlByBms, Force_On, Force_Off}`；`GetUpperFromTbl()` 中写入；但 `CMD_Task()` 当前只处理 `LimitCtrl`，未处理 `ChgMosCtrl`/`DiscMosCtrl`/`PrecMosCtrl`，疑似未完成实现。

**验收准则**
- Given `ChgMosCtrl = Force_Off` 写入寄存器 1500 When `CMD_Task()` 运行 Then BMS 充电 MOS 应强制断开（**当前实现中无对应调用，状态=存疑**）。

---

### REQ-COMM-015  数据注入功能（测试/标定模式）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `UpperComTask.h:InjectData_TypeDef`；`UpperComTask.c:GetUpperFromTbl()`；寄存器 1508–1540 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当上位机写入注入数据到寄存器区（寄存器 1508 起）并设置 `Inject = 非零` 时，系统应接收注入的测试数据：20 路单体电压（mV）、8 路温度（0.1℃ - 400 偏移）、总电压（×100 mV）、MOS 电压、电流（×100 mA，含 32 000 偏移）、SOC（1%）。⚠️ 该功能用于标定与测试，若在正式运行期间误触发将导致 BMS 使用虚假数据保护。

**理由 / 代码依据**
> `GetUpperFromTbl()` 中 `InjectData.CellVolt[i]=...`、`InjectData.Temp[i]=RegTbl.UpperCtrlTbl[offset++]-TEMP_OFFSET`、`InjectData.current_mA=(RegTbl.UpperCtrlTbl[offset++]-SND_CUR_OFFSET)*100`。

**验收准则**
- Given `Inject = 1` 且注入单体 0 电压 = 4 200 mV When BMS 处理 Then `InjectData.CellVolt[0]` 应 = 4 200。

---

### REQ-COMM-016  日志读取二步协议（自定义功能码 0x1A/0x1B）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppModbus.h:MODBUS_CMD_LOG_STEP1/STEP2`；`UpperComTask.c:DealModbusLogStep1()` / `DealModbusLogStep2()` / `ModbusLogStep1()` / `ModbusLogStep2()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 日志读取采用两步协议：
> - **Step1（0x1A）**：上位机发送 16 字节请求（含 8 字节起始时间戳 + 8 字节结束时间戳，均为 Unix 时间，大端序），BMS 应搜索时间范围内的日志条目数，并回应包含匹配条目总数（`uint32_t`）的 8 字节帧；
> - **Step2（0x1B）**：上位机发送 4 字节请求（含 `uint32_t` 条目序号），BMS 应返回对应日志条目（132 字节，含 4 字节对齐的大端序字段），序号越界（0 或 > logcnt）时应回应错误帧（功能码 `0x1B | 0x80`）。

**理由 / 代码依据**
> `DealModbusLogStep1()` 中 `if (datalen != 16) return`；`SearchIndex(startstamp, endstamp)`；`ModbusLogStep1(logreadinfo.logcnt)`；`DealModbusLogStep2()` 中 `if (datalen != 4) return`；`ModbusLogStep2()` 中 `if((num > logreadinfo.logcnt)||(num == 0)) { ackbuf[1] = MODBUS_CMD_LOG_STEP2|0x80; }` else 回送 132 字节日志，并调用 `endian_swap()` 按 4 字节块大端转换。

**验收准则**
- Given 日志库有 10 条记录 When Step1 查询全时间范围 Then 应回应 logcnt = 10。
- Given Step2 请求序号 = 0 When 发送 Then 应回应错误帧（高位 0x80）。
- Given Step2 请求序号 = 11（越界）When 发送 Then 应回应错误帧。

---

### REQ-COMM-017  有效通信检测与外部通信活跃标志

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppTime.c:AppTime_NotifyExtComm()`；`UpperComTask.c:UpperComTask()`；`AppTime.h` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 每当收到并通过校验的有效通信帧时，系统应调用 `AppTime_NotifyExtComm()` 重置外部通信活跃计数器；计数器采用 10 ms 步进（在 1 ms 中断中每 10 ms 递增），在 **500 ms（50 × 10 ms）** 内无有效帧到达时，系统应将 `ExtCommActive` 标志清零，通知低层驱动外部通信已超时。

**理由 / 代码依据**
> `APP_TIME_EXT_COMM_TIMEOUT_TICKS = 50U`；`AppTime_Tick1ms()` 中每 10 ms 递增 `g_ext_comm_time`；`AppTime_NotifyExtComm()` 中 `g_ext_comm_time = 0; BspTimer_SetExtCommActive(1)`；超时后 `BspTimer_SetExtCommActive(0)`。

**验收准则**
- Given 上位机持续以 100 ms 间隔轮询 When 通信正常 Then `ExtCommActive` 应持续为 1。
- Given 上位机停止通信 600 ms When 超时发生 Then `ExtCommActive` 应变为 0。

---

### REQ-COMM-018  Modbus 异常响应格式

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `AppModbus.c:ModbusSendErroAck()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当 Modbus 命令无法正常处理时，系统应返回 Modbus 标准异常响应帧：`[ADDR][FuncCode|0x80][ErrorCode][CRC_L][CRC_H]`，CRC16 覆盖前 3 字节；定义的错误码为：`ERRO_FUNC=0x01`（功能码不支持）、`ERRO_REG=0x02`（寄存器地址越界）、`ERRO_OP=0x03`（操作不允许）、`ERRO_DEV=0x04`（设备错误）。

**理由 / 代码依据**
> `ModbusSendErroAck()` 中 `ackbuf[1] = FuncCode|0x80`；5 字节帧（地址+功能码+错误码+2 字节 CRC）；错误码宏在 `UpperComTask.h`。

**验收准则**
- Given 读越界寄存器 When 发送 0x03 命令 Then 应收到 5 字节异常帧，第 2 字节 = 0x83，第 3 字节 = 0x02，CRC 合法。

---

### REQ-COMM-019  调试串口打印模式切换

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `AppCom.c:AppCom_HandlePrintMode()`；`bsp_usart.h:ComMode_*` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当收到恰好 4 字节的帧且内容为 `{'P','R','T','E'}` 时，系统应切换至打印调试模式（`ComMode_Print`）；当内容为 `{'P','R','T','D'}` 时，应切换回正常通信模式（`ComMode_Normal`）；其他 4 字节帧应被丢弃，不影响通信状态。

**理由 / 代码依据**
> `AppCom_HandlePrintMode()` 函数；枚举 `{ComMode_Normal, ComMode_Print, ComMode_Upper}` 在 `bsp_usart.h`。

**验收准则**
- Given 向任一端口发送 4 字节 `50 52 54 45`（"PRTE"）When 处理 Then `commode` 应 = `ComMode_Print`（1）。

---

### REQ-COMM-020  电流偏置编码约定

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 否 |
| 来源（源码） | `UpperComTask.h:SND_CUR_OFFSET`；`UpperComTask.c:CreatPackInfoBuf()` / `GetUpperFromTbl()` |
| 验证方法 | 检视 |
| 状态 | 已实现 |

**需求描述**
> 所有经 RS-485 传输的电流值应采用偏置编码：实际电流（单位 100 mA/bit）加上偏移量 **32 000**（`SND_CUR_OFFSET`），以便在无符号 16-bit 寄存器中表示正负电流（可表示范围约 −3 200 A 至 +3 200 A）。接收端（写控制寄存器时）应减去 32 000 再乘以 100 得到 mA 单位值。

**理由 / 代码依据**
> `SND_CUR_OFFSET = 32000` 宏；`CreatPackInfoBuf()` 中 `Bms.current_mA/100 + SND_CUR_OFFSET`；`GetUpperFromTbl()` 中 `InjectData.current_mA = (RegTbl.UpperCtrlTbl[offset++]-SND_CUR_OFFSET)*100`；同样用于寄存器 1541 的电流校准写操作。

**验收准则**
- Given 电流 = -500 mA（放电）When 编码 Then 寄存器值 = (-500/100 + 32000) = 31 995。
- Given 寄存器值 = 32 100 When 解码 Then 电流 = (32100 - 32000) × 100 = 10 000 mA（充电）。

---

### REQ-COMM-021  电流标定写入（寄存器 1541）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `UpperComTask.c:DealModbusWrite()` line 914–920；`AdjustCur()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当上位机向寄存器 1541（控制区特殊地址）写入单个寄存器时，系统应将该值解析为当前实测电流（100 mA/bit + 32 000 偏置），计算 ADC 校准比例系数（`AdjCurRatio`）和偏移（`AdjCurOffset`），并持久化保存到 Flash。

**理由 / 代码依据**
> `if((startReg == 1541)&&(endReg == 1541)) { int32_t cur = (data[5]<<8 | data[6]); cur = (cur-32000)*100; AdjustCur(cur); Bms.needsave |= ...; }`；`AdjustCur()` 中 `Bms.Para.AdjCurRatio = cur/(Bms.Para.NowCurADC - Bms.Para.CurADC1)`。

**验收准则**
- Given 标定电流 = 10 000 mA（寄存器值 32 100）When 写入 1541 Then `AdjCurRatio` 应被重新计算，`needsave` 置位。

---

### REQ-COMM-022  RTC 时间设置（通过状态区写入）

| 属性 | 内容 |
|---|---|
| 类型 | 功能 |
| 安全相关 | 否 |
| 来源（源码） | `UpperComTask.c:DealModbusWrite()` line 945–963 |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当上位机写入系统状态寄存器区（300–399）时，系统应将写入数据解析为 RTC 时间（年（+2000）/月/日/时/分/秒），调用 `AppRtc_Set()` 和 `AppRtc_Refresh()` 设置并刷新硬件 RTC；数据格式每个字段占 2 字节（从 data[6] 起按序：年/月/日/时/分/秒，低字节有效）。

**理由 / 代码依据**
> `syear = data[6]; smon = data[8]; sday = data[10]; hour = data[12]; min = data[14]; sec = data[16]; AppRtc_Set(syear+2000,...);`

**验收准则**
- Given 向 300 区写入年=25 月=6 日=23 时=12 分=0 秒=0 When 写命令执行 Then `AppRtc_Set(2025,6,23,12,0,0)` 应被调用。

---

### REQ-COMM-023  升级会话承载（私有协议 OTA 通道）

| 属性 | 内容 |
|---|---|
| 类型 | 接口 |
| 安全相关 | 是 ⚠️ |
| 来源（源码） | `AppCom.c:AppCom_SaveUpgradeFrame()`；`UpperComTask.c:UpperComUpgradeTask()` / `UpperCom_IsUpgrading()` |
| 验证方法 | 测试 |
| 状态 | 已实现 |

**需求描述**
> 当通信层收到以私有协议帧头（`0xA5 0x5A`）开头的帧时，系统应将其存入升级专用缓冲区（`Rev485_ForUpGrade`），并由 `UpperComUpgradeTask()` 异步处理；OTA 处理期间 `upgrading` 标志置 1，可供其他模块查询；升级会话完成后 `upgrading` 自动清零。OTA 协议细节由 OTA 域负责，本域仅保证通信通道隔离与状态暴露。

**理由 / 代码依据**
> `AppCom_OnRs485Frame()` 中帧头匹配后路由至 `AppCom_SaveUpgradeFrame()`；`UpperComUpgradeTask()` 中 `upgrading=1; deal_ota_req(...); upgrading=0;`；`UpperCom_IsUpgrading()` 暴露状态。

**验收准则**
- Given OTA 帧（0xA5 0x5A 开头）到达 When `UpperComUpgradeTask()` 处理中 Then `UpperCom_IsUpgrading()` 应返回 1。
- Given OTA 会话完成 When `UpperComUpgradeTask()` 返回 Then `UpperCom_IsUpgrading()` 应返回 0。

---

## 存疑与观察

### OB-01  `CreatUpperSndInfo()` 声明但未实现（缺口）
`UpperComTask.h` 中声明了 `void CreatUpperSndInfo(void)`，但在所有源文件中均未找到其实现。该函数名称暗示"主动向上位机发送电池信息"的功能（对应 `CMD_SEND_BATINFO = 0x18`、`CMD_SEND_WARNING = 0x1B`、`CMD_SEND_CELLONLINE_INFO = 0x1C` 等命令码已定义但同样无实现）。**这意味着当前固件为纯被动查询模式，不支持主动上报/事件推送，是一个设计缺口**，告警发生时只能等待上位机轮询。`UpperComTask.h` 中注释"单独上传告警避免出现查询时已经恢复而丢失记录"说明该功能是设计意图。

### OB-02  ChgMosCtrl / DiscMosCtrl / PrecMosCtrl 命令无实际执行逻辑（缺口/存疑）
寄存器 1500（ChgMosCtrl）、1501（DiscMosCtrl）、1502（PrecMosCtrl）可由上位机写入，且 `GetUpperFromTbl()` 正确解析至 `UpperCmd` 结构体，但 `CMD_Task()` 中只处理了 `LimitCtrl`，对充/放/预充 MOS 的强制控制命令**没有对应的执行调用**。可能已在 BMS_Control 层实现，但从 `CMD_Task()` 看无入口，属存疑/缺口。

### OB-03  `Restart` 命令为空操作（缺口）
`CMD_Task()` 中 `if(UpperCmd.Restart == UPPERCMD_ENABLE) { UpperCmd.Restart = 0; }` — 收到重启命令后仅清零标志，**未调用任何软件复位函数**（如 `NVIC_SystemReset()`）。这可能是有意的安全保守设计，也可能是实现遗漏。

### OB-04  私有协议 PROTO_USERDEF 的正常通信命令集未启用
`UpperComTask()` 中 `else if (upperrev.proto == PROTO_USERDEF)` 分支已完整注释掉（包含 `CMD_UPGRADE_REQ` 的旧处理逻辑）。所有正常通信命令（`CMD_PARA_READ`、`CMD_PARA_SET`、`CMD_LOG_READ`、`CMD_READ_VERSION` 等在 `UpperComTask.h` 中定义）**在当前 UpperComTask 中均无实现**，仅在 `upgrade.c` 中有部分引用（用于 OTA 通道的设备信息读取）。当前正常通信完全通过 Modbus 功能码 0x03/0x10/0x1A/0x1B 实现。

### OB-05  大端字节序存在两套实现，来源混淆
私有协议帧 CRC 存储为"高字节在 `revLen-2`、低字节在 `revLen-1`"，而 Modbus 标准 CRC 为低字节在前（`ackbuf[offset++] = crc; ackbuf[offset++] = crc>>8`）。`Is485RevOK()` 同时用 `src_crc16`（私有协议字节序）和 `src_crc16_big`（Modbus 字节序）两种方式尝试匹配同一帧 CRC，逻辑正确但命名"_big"/"非_big"与实际字节序含义存在反直觉之处，易引发维护混淆。

### OB-06  CRC 检验通过但地址不匹配时仅 printf 无异常帧（存疑）
`Is485RevOK()` 中 Modbus 路径地址不匹配时仅 `printf("CMD_ERRO\n")`，不发送任何 Modbus 异常响应。这符合 Modbus 总线多从站广播场景（不该回应不属于自己的地址），但若为点对点通信，静默丢帧可能导致上位机超时等待。

---

## 本域需求索引表

| ID | 标题 | 安全 | 来源 | 状态 |
|---|---|---|---|---|
| REQ-COMM-001 | 双 RS-485 物理接口 | 否 | `Core/Src/usart.c` | 已实现 |
| REQ-COMM-002 | 通信端口动态选择 | 否 | `AppCom.c` | 已实现 |
| REQ-COMM-003 | 帧边界检测与最小长度过滤 | 否 | `AppCom.c` / `AppComLink.c` | 已实现 |
| REQ-COMM-004 | 私有协议帧格式与 CRC16 校验 | 否 | `AppComLink.c` / `AppComLink.h` | 已实现 |
| REQ-COMM-005 | Modbus RTU 帧格式与 CRC16 校验 | 否 | `AppComLink.c` / `AppModbus.h` | 已实现 |
| REQ-COMM-006 | 接收缓冲区规格 | 否 | `AppComLink.h` / `bsp_usart.h` | 已实现 |
| REQ-COMM-007 | 双协议自动识别与路由 | 否 | `AppCom.c` / `AppComLink.c` | 已实现 |
| REQ-COMM-008 | Modbus 0x03 读保持寄存器 | 否 | `UpperComTask.c` / `AppModbus.c` | 已实现 |
| REQ-COMM-009 | Modbus 0x10 写多个寄存器 | 是 ⚠️ | `UpperComTask.c` / `AppModbus.c` | 已实现 |
| REQ-COMM-010 | Modbus 寄存器地址映射 | 否 | `UpperComTask.h` | 已实现 |
| REQ-COMM-011 | 系统状态寄存器内容与编码 | 否 | `UpperComTask.c:CreatPackInfoBuf()` | 已实现 |
| REQ-COMM-012 | 单体电压与温度上报编码 | 否 | `UpperComTask.c:CreatCellVoltBuf()` / `CreatCellTempBuf()` | 已实现 |
| REQ-COMM-013 | 上位机控制命令处理 | 是 ⚠️ | `UpperComTask.c:CMD_Task()` | 已实现 |
| REQ-COMM-014 | 远程强制接触器控制（充/放/预充 MOS） | 是 ⚠️ | `UpperComTask.h` / `UpperComTask.c` | 存疑 |
| REQ-COMM-015 | 数据注入功能（测试/标定模式） | 是 ⚠️ | `UpperComTask.c:GetUpperFromTbl()` | 已实现 |
| REQ-COMM-016 | 日志读取二步协议（0x1A/0x1B） | 否 | `UpperComTask.c` / `AppModbus.c` | 已实现 |
| REQ-COMM-017 | 有效通信检测与外部通信活跃标志 | 否 | `AppTime.c` / `UpperComTask.c` | 已实现 |
| REQ-COMM-018 | Modbus 异常响应格式 | 否 | `AppModbus.c:ModbusSendErroAck()` | 已实现 |
| REQ-COMM-019 | 调试串口打印模式切换 | 否 | `AppCom.c` | 已实现 |
| REQ-COMM-020 | 电流偏置编码约定 | 否 | `UpperComTask.h` / `UpperComTask.c` | 已实现 |
| REQ-COMM-021 | 电流标定写入（寄存器 1541） | 否 | `UpperComTask.c:DealModbusWrite()` | 已实现 |
| REQ-COMM-022 | RTC 时间设置（通过状态区写入） | 否 | `UpperComTask.c:DealModbusWrite()` | 已实现 |
| REQ-COMM-023 | 升级会话承载（私有协议 OTA 通道） | 是 ⚠️ | `AppCom.c` / `UpperComTask.c` | 已实现 |
