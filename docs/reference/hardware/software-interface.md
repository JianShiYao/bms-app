# S16100B BMS 主控板 — 软件资源清单（接口与协议）

> **来源**：本文件是 `软件资源清单.docx` 的**仓库内 markdown 版**（.docx 为二进制、不入库；本文为可追溯/可链接/可 diff 的权威副本）。
> **依据**：原理图 `BPCB-PCB-MB-PD-S16100B V0.4`，参考 Demo 工程 `Application/` 源码；主控 STM32F405RGT6；编制 2026-06-22；用途：面向后续软件开发的接口/协议资源整理。
> **在本项目中的定位**：本板（S16100B / STM32F405RGT6）即架构里的 **`bms_f405` 真板目标**；其外设即 hal/ 层 wrapper 的具体规格（见 [../../concept/hardware-abstraction.md](../../concept/hardware-abstraction.md)）。**参考输入，不等同新固件承诺**。
> **文档基准说明**：硬件资源（引脚/外设/芯片）以原理图 V0.4 为准；协议与软件逻辑参考 Demo 工程。实测固件与原理图一致，差异仅原理图几处网络名标注笔误（见 §9 末）。

文档主线：**各功能模块 → 用什么接口/串口 → 走什么协议**。§1–§2 是总览（最常查），§3–§6 是接口与协议详解，§9 是引脚速查附录。

---

## 1. 板卡与软件架构概述

| 项目 | 内容 |
|------|------|
| 用途 | 电池组 BMS 主控板（电芯采集/保护/均衡、充放电管理、加热、通信上报、数据记录） |
| 主控 MCU | STM32F405RGT6（Cortex-M4F / 168 MHz / 1 MB Flash / 64 脚） |
| 软件架构 | 裸机前后台（无 RTOS）；main 初始化后 `while(1)` 顺序轮询各任务；1 ms 节拍由 TIM 中断提供 |
| 采集核心 | AFE 芯片 **SH3673520**（最多 20 串电压 + 4 路温度 + 电流 + 保护 + MOS 驱动 + 均衡） |
| 对外通信 | 两路隔离 RS485（与上位机 / FSU） |

**既有 Demo 软件分层结构**（供本项目 Model C 分层参照）：

```
┌────────────────────────────────────────────────────────────┐
│ 业务/应用层 Application  │ BMS控制、SOC、告警、加热、充电、通信协议、日志、升级
├────────────────────────────────────────────────────────────┤
│ 中间件（硬件抽象层，屏蔽硬件差异·统一接口）─ 见 §6            │
│  ├ App* 适配层 (Application/，源码)  │ AppCom/AppAfe/AppPower/AppRtc/AppTime/BmsAdc/AppModbus…
│  └ BSP/驱动层 (S16100B_Driver.lib，预编译) │ bsp_usart·spi·adc·key·led·heat / pmu / ex_flash / sd / AFE驱动
├────────────────────────────────────────────────────────────┤
│ 组件库 Components（第三方，被中间件调用） │ FatFs 文件系统、MALLOC 内存池
├────────────────────────────────────────────────────────────┤
│ 底层 STM32F4 HAL + CMSIS │ 外设寄存器抽象、内核支持
└────────────────────────────────────────────────────────────┘
```

> 中间件 = 夹在 HAL 与业务之间、屏蔽硬件差异并向上提供统一接口的通用代码层（本工程分 App* 适配层 + BSP/驱动层）。业务层只调中间件接口，不直接碰寄存器/HAL。

## 2. 模块功能 × 接口 × 协议 总表（核心）

| 功能模块 | 接口 / 串口 | 物理层 | 通信协议 | 方向 | 说明 |
|---|---|---|---|---|---|
| 上位机 / FSU 通信 | USART1 收发(PA9/PA10) + GPIO 方向(PA11) | 隔离 RS485 | Modbus RTU + 私有(0xA5 0x5A) | 收/发 | 主通信口；9600 8N1；DMA 收发 |
| 第二路通信 / 级联 | UART4 收发(PC10/PC11) + GPIO 方向(PC12) | 隔离 RS485 | Modbus RTU + 私有(0xA5 0x5A) | 收/发 | 备用/级联；9600 8N1；DMA 收发 |
| 电芯采集/保护/均衡/MOS | SPI2(PB12~15) | 板内 SPI | AFE 私有命令(0x01/0x02/0x0B) | MCU→AFE 主从 | 读电压/温度/电流，写保护参数，驱动 MOS |
| 参数/日志/容量存储 | SPI1(PA5~7, CS PC4) | 板内 SPI | W25Q32 标准 SPI 指令 | 读/写 | 外部 NOR Flash 存参数、日志、容量、地址 |
| 数据记录（CSV） | SPI1(PA5~7, CS PC2) | 板内 SPI | SD 卡 SPI 模式 + FatFs | 读/写 | 运行数据写 CSV 到 TF 卡 |
| 温度/电压/进水采集 | ADC1(PA1/2/4, PB0/1, PC0/1) | 模拟 | — | 输入 | MOS/环境/PTC 温度、Vmos、进水、地址 |
| 加热控制 | TIM3_CH2(PC7) | PWM | — | 输出 | 加热膜功率调节 |
| 充电控制 | TIM2_CH4(PA3) + GPIO(PC5) | PWM + 电平 | — | 输出 | 充电电流 PWM 调节 + 充电使能 |
| 指示灯 | GPIO ×6 | 电平 | — | 输出 | SOC 25/50/75/100% + Run + Alm |
| 电源/按键/唤醒 | GPIO + WKUP(PA0) | 电平 | — | 输入/输出 | 电源自锁、开机键、休眠唤醒 |
| 固件升级 (OTA) | USART1 / UART4 | RS485 | RS485 私有升级协议 | 收/发 | 分包下载固件、写 Flash、置升级标志 |
| 时间/时间戳 | RTC (LSE 32.768k) | — | — | — | 给日志打时间戳、定时唤醒 |

> 两路 RS485 通信能力基本一致（均支持 Modbus RTU 与私有协议）；区别在默认用途：RS485-1(USART1) → 上位机/FSU，RS485-2(UART4) → 第二通信口，且可切换成调试打印模式（见 §5.5）。

## 3. 通信协议清单

| 协议 | 承载接口 | 用途 | 关键格式 |
|---|---|---|---|
| Modbus RTU | RS485 (USART1/UART4) | 与上位机/FSU 读写数据、下控制命令 | 地址 + 功能码(0x03读/0x10写) + 数据 + CRC16 |
| 私有协议(参数/升级) | RS485 (USART1/UART4) | 固件升级、参数读写、读版本/日志/告警 | 帧头 0xA5 0x5A + 命令 + 数据 |
| AFE SPI 私有命令 | SPI2 | AFE 寄存器读写、状态采集、控制 | 读/写/复位(0x01/0x02/0x0B) + 寄存器地址(+数据) + CRC8 |
| W25Qx SPI NOR 指令 | SPI1 | 外部 Flash 读/写/擦除 | 标准 SPI NOR 指令集(0x03/0x06/0x02/0x20/0x9F 等) |
| SD 卡 SPI 模式 + FatFs | SPI1 | TF 卡读写、FAT 文件系统 | SD SPI 命令(CMD0/CMD17/CMD24…) + FatFs |
| 调试打印模式切换 | RS485 | 切换串口调试打印 | ASCII 4 字节 "PRTE"=开 / "PRTD"=关 |

## 4. 接口资源明细

### 4.1 RS485 串口（两路隔离，对外通信）

隔离芯片 **CA-IS2092A ×2**，隔离侧独立 ISO_GND / ISO+5V（PCB 注明爬电间距 > 2 mm）。

| 参数 | 主通信口 | 第二路 |
|---|---|---|
| MCU 串口 | USART1 | UART4 |
| TX | PA9 (485_LUTx) | PC10 (485_U3Tx) |
| RX | PA10 (485_LURx) | PC11 (485_U3Rx) |
| 方向控制 (DE/RE) | PA11 GPIO (485_LUDir) | PC12 GPIO (485_U3Dir) |
| 通信参数 | 9600, 8N1（Demo 默认） | 同左 |
| 收发方式 | DMA 收发；方向脚发前置高、发后置低 | 同左 |
| 用途 | 接 FSU / 上位机（主） | 备用 / 级联 |

### 4.2 SPI2 → AFE（SH3673520，采集核心）

| 信号 | 引脚 | 说明 |
|---|---|---|
| SCK | PB13 | SPI2 时钟 |
| MISO | PB14 | AFE→MCU (SDO) |
| MOSI | PB15 | MCU→AFE (SDI) |
| CS (NSS) | PB12 | 片选，GPIO 手动控制 |
| AFE_WAKE | PC6 | 唤醒 AFE（上电拉高 1 s 再拉低） |
| AFE_RESET | NRST | AFE 复位（与 MCU NRST 同网络） |
| ALARM→CHG_WAKE | — | AFE 报警 / 充电插入唤醒 MCU |

### 4.3 SPI1 → 外部 Flash + SD 卡（共享总线）

| 信号 | 引脚 | 说明 |
|---|---|---|
| SCK / MISO / MOSI | PA5 / PA6 / PA7 | SPI1 三线（Flash 与 SD 共用） |
| FLASH_CS | PC4 | W25Q32 片选 |
| SD_CS | PC2 | TF 卡片选 |
| SD_DET | PC3 | TF 卡插入检测（输入） |

> 两个片选分时选通；外设：U600 **W25Q32JV**（32 Mbit NOR）+ CARD600 TF 卡座。

### 4.4 ADC1（模拟采集，多通道 + DMA）

| 信号 | 引脚 | 通道 | 测量对象 |
|---|---|---|---|
| PTC_T | PA1 | IN1 | PTC 加热膜温度 NTC |
| MOS_T | PA2 | IN2 | 功率 MOS 温度 NTC |
| ENV_T | PA4 | IN4 | 环境温度 NTC |
| Vmos | PB0 | IN8 | MOS 两端电压 |
| WARTER_CHECK2 | PC0 | IN10 | 进水/漏液检测 2 |
| WARTER_CHECK1 | PC1 | IN11 | 进水/漏液检测 1 |

> NTC 型号 MF52-A-103-F-3950（10k / B3950）。另有地址拨码 SW200 经分压采集，定 RS485 从机地址。
> ⚠️ 引脚归属见 §9 注：ADC 通道分配以 §9 引脚总表与原理图为准（本节 PA1 列为 PTC_T，§9 引脚表 PA1=BmuAddr、PB1=PTC_T，存在文档内不一致，需以原理图 V0.4 核定）。

### 4.5 定时器 / PWM

| 信号 | 引脚 | 定时器 | 用途 |
|---|---|---|---|
| HeatingPWM | PC7 | TIM3_CH2 | 加热膜功率 PWM |
| ChgCurPWMAdj | PA3 | TIM2_CH4 | 充电电流调节 PWM（送 TL494） |
| 系统节拍 | — | TIM1 (1 ms) | 软件 1ms / 1s / 2s / 20s 节拍 |

### 4.6 GPIO（控制与指示）

| 类别 | 信号(引脚) | 说明 |
|---|---|---|
| 电源 | PWhold(PD2) | 电源自锁，上电后须立即拉住 |
| 按键/唤醒 | PWKeyCheck(PB3)、WKUP(PA0) | 开机键检测、总唤醒入口 |
| 充电 | ChargerEn(PC5) | 充电使能 |
| AFE | AFE_WAKE(PC6) | AFE 唤醒 |
| 指示灯 | Soc50(PB5)/Soc75(PB6)/Soc100(PB7)/Run(PB8)/Alm(PB9)（另 Soc25=PB4） | SOC + 运行 + 告警 |
| 采样使能 | VmosSSen(PB2) | Vmos 采样使能开关（原理图 PB1/PB2 标注不一，见 §9 末） |

## 5. 协议详解

> 本章命令字、Modbus 寄存器地图、Flash 分区地址等均源自 Demo 工程源码；**具体命令字 / 寄存器地址 / 数值需以 S16100B V0.4 最终固件为准**。

### 5.1 Modbus RTU（与上位机/FSU）

| 功能码 | 含义 |
|---|---|
| 0x03 | 读保持寄存器 |
| 0x10 | 写多个寄存器 |
| 0x1A / 0x1B | 日志读取（扩展） |

校验：Modbus CRC16（`Cal_CRC16ofModBus()`）。寄存器地图：

| 地址段 | 内容 |
|---|---|
| 0~3 | 版本号 |
| 20~299 | BMS 系统参数（保护阈值/均衡/加热/限流，可读写） |
| 300~399 | Pack 状态信息 |
| 400~499 | Pack / 模组电压 |
| 500~550 | 单体电芯电压 |
| 1000~1050 | 单体温度 |
| 1500~1550 | 上位机下行控制（开关 MOS / 限流 / 加热、关机 / 重启 / 复位 / 数据注入） |

### 5.2 私有协议（参数 / 固件升级 OTA）

帧头 `0xA5 0x5A`，主要命令：

| 命令 | 含义 |
|---|---|
| 0x02 | 升级请求 |
| 0x03 | 发送数据包（分包，最大 1024 字节/包） |
| 0x11 / 0x12 | 参数读 / 参数写 |
| 0x13 | 读日志 |
| 0x19 | 读版本号 |
| 0x1B | 读告警记录 |
| 0x1C / 0x1D | 读 / 写电芯在线信息 |

升级应答码：0x80 包OK / 0x81 文件OK / 0x82 升级标志OK；错误码 0x00~0x09（长度/包号/超时等）。升级期间暂停写 SD/Flash 日志。

### 5.3 AFE SPI 协议（SH3673520）

| 命令字 | 含义 |
|---|---|
| 0x01 | 写寄存器（配置 / MOS 控制 / 均衡） |
| 0x02 | 读寄存器（采样值 / 状态标志） |
| 0x0B | 软复位 |

访问时序：拉低 CS → 发命令字 + 寄存器地址(+数据) → 拉高 CS。底层接口 `SPIWriteAFE / SPIReadAFE / SPIResetAFE`。

- **采集数据**：电芯电压(CELL1~20，AFE 最多 20 串，本板采样线 16 串 CV0~16)、4 路外部温度(TEMP1~4) + 内部温度、电流(CADC/VADC 双路)、总压(VTOP/B+)、充电端电压(VCHGR)、断线检测(OWD)。
- **配置/保护寄存器**：SCONF1~7(系统配置)、OV/UV(过欠压)、OCD1/OCD2/SC/OCC(放电过流/短路/充电过流)、OTC/OTD/UTC/UTD(高低温)、BALANCE(均衡)、FLAG/BSTATUS(标志状态)。
- 默认阈值见 `AFE.h` 的 `_E2_xxx` 宏，开机 `SetDefaultAFEPara()` 下发，运行期可经 Modbus 写参数寄存器修改。

### 5.4 存储协议

**外部 Flash（W25Q32, SPI1）**：标准 SPI NOR 指令；分区（含主区+备份区双写防掉电损坏）：

| 区域 | 地址 |
|---|---|
| 程序备份区（OTA 暂存） | 0x00000~0x4B000 |
| 运行日志 主/备 | 0x7E000 / 0x7F000 |
| 日志索引 主/备 | 0x80000 / 0x81000 |
| 充电/放电容量 主/备 | 0x86000 / 0x88000 区 |
| 485 地址、使能信息、校准偏移 主/备 | 0x1E7000 ~ 0x1EE000 |
| 系统参数 主/备 | 0x1F0000 / 0x1F1000 |
| 错误记录、SOC 保存 | 0x1F3000 / 0x1FDFF0 区 |

**SD 卡（SPI1）**：SD SPI 模式 + FatFs 文件系统，运行数据每 2 秒写一条 CSV。

### 5.5 调试打印模式

上位机发 4 字节 ASCII："PRTE" 开启串口调试打印、"PRTD" 关闭。便于开发期通过 RS485 观察 `printf` 输出。

## 6. 中间件（硬件抽象层）

夹在 STM32 HAL（底层）与业务逻辑（上层）之间、屏蔽硬件差异并向上提供统一接口的通用代码层。业务层只调本层接口，不直接操作寄存器或 HAL——硬件改动时上层基本不动。本工程分两小层。

### 6.1 BSP / 驱动层（S16100B_Driver.lib，预编译，直接封装 HAL 与各芯片）

| 模块 | 屏蔽的硬件 | 向上提供的统一接口（示例） |
|---|---|---|
| bsp_usart | USART1/UART4 + DMA + 收发方向脚 | RS485_TxData/RxData、usartX_send、Usart1_InitWithDMA |
| bsp_spi | SPI1/SPI2 + 片选时序 | spi_write_read、SPI2_CS_LOW/HIGH |
| bsp_adc / bsp_ntc | ADC1 + DMA + NTC 换算 | BspAdc_Process、温度换算 |
| bsp_key / pmu | 按键 GPIO、电源自锁、RTC/Standby 唤醒 | getmsg、PowerHold_On/Off、enter_sleep、start_RTCWK |
| bsp_led / bsp_heat / bsp_limit | LED GPIO、加热 PWM、限流 | LED 开关、加热/限流接口 |
| ex_flash / sd_spi / mmc_sd | W25Q32 Flash、SD/TF 卡 | SPI_Flash_Init、Flash 读写、SD_Init、读扇区 |
| AFE 驱动 | SH3673520 的 SPI 时序与寄存器 | AFEReadReg/AFEWriteReg、AFEChgMosEn/DsgMosEn 等 |

### 6.2 App* 适配层（Application/，源码开放，进一步统一接口）

把驱动再封装成"与业务无关、口径统一"的应用接口，是上层与硬件之间的隔离带。

| 适配模块 | 封装对象 | 统一接口（示例） | 屏蔽 / 统一了什么 |
|---|---|---|---|
| AppCom / AppComLink | bsp_usart（两路） | AppCom_Send(port,…)、AppCom_OnRs485Frame | 上层只认"端口号" |
| AppAfe | AFE 驱动 | AppAfe_Process、AppAfe_SetChgMos/DsgMos… | 屏蔽 SH3673520 SPI 与寄存器细节 |
| BmsAdc | bsp_adc | BmsAdc_Update（统一采样结构体） | 屏蔽 ADC 通道与换算 |
| AppPower | bsp_key / pmu | AppPower_Init、AppPower_CheckPowerOff | 屏蔽按键 / 休眠唤醒底层 |
| AppRtc | RTC HAL | AppRtc_GetNow、AppRtc_GetTimestampNow | 屏蔽 RTC 寄存器，提供日历/时间戳 |
| AppTime | TIM 中断 | AppTime_GetMs、AppTime_Take2sFlag | 统一软件时基（ms/1s/2s/20s） |
| AppStorage | ex_flash + 分区表 | 按"参数/日志/容量"读写 | 屏蔽 Flash 物理地址与扇区 |
| AppModbus | — | ModbusRead、ModbusSendOKAck/ErroAck | 统一 Modbus 组帧/CRC/应答 |
| AppLed / AppLimit / AppHeat / AppDelay | bsp_led/limit/heat/DWT | 应用级开关/延时接口 | 屏蔽底层 GPIO / 定时器 |

### 6.3 第三方组件库（Components/，被中间件/应用调用）

| 组件 | 位置 | 版本/类型 | 说明 |
|---|---|---|---|
| FatFs 文件系统 | Components/FATFS | FatFs R0.11 系列（Rev 29000，ALIENTEK 移植） | 挂 SD 卡(SPI1)：LFN、单卷单分区、扇区512、非重入；AppCsvLog 每 2s 写 CSV |
| MALLOC 内存管理 | Components/MALLOC | ALIENTEK 分块内存池（块 32B / 池 2KB） | mymalloc/myfree/myrealloc；供 FatFs 长文件名等动态分配 |

> CubeMX 习惯把 FatFs 称作 "Middleware"，但按分层定义它们是被调用的第三方库，不承担硬件屏蔽职责，故单列。
> ⚠️ **授权提醒**：FatFs、MALLOC、exfuns 为 ALIENTEK（正点原子）移植代码，头文件注明"仅供学习使用"。**量产/商用前需确认授权**，或替换为官方 FatFs（自由许可）+ 自研内存管理。

## 7. 原理图阅读导引

### 7.1 分页导读（拿到 PDF 先看哪页）

| 页 / 图纸 | 内容 | 软件关注点 |
|---|---|---|
| pt0_Top | 顶层方块图 | 先看这页建立整体连接 |
| pt1_Power | 降压(XL7045)、LDO(H7233)、电源锁止、按键检测 | PWhold 自锁、按键、唤醒 |
| pt2_MCU | STM32F405 + 8M/32.768k 晶振 + SWD + 地址拨码 SW200 | ★ 所有 MCU 引脚分配看这页 |
| pt3_com | 两路隔离 RS485（CA-IS2092A） | 通信口、方向脚、隔离地 |
| pt4_SH3673520 | AFE 采集 + 保护 + 均衡 + 充放电 MOS 阵列 | ★ 采集/保护/MOS 核心 |
| pt5_ctr | 指示灯 + 加热膜控制 + 灭火(Extinguishment) 输出 | LED、加热 PWM |
| pt6_Flash | W25Q32 SPI Flash + TF/SD 卡座 | 存储 SPI1、片选 |
| pt7_IO | 温度采集(NTC) + 进水检测 | ADC 通道 |
| pt9_Charger | 充电 DC-DC(TL494) + 充电 MOS | 充电使能、电流 PWM |
| pt10_Connector | 对外接插件 | 各口接什么（见 §7.3） |

> 图纸为 pt0~pt7、pt9、pt10（无 pt8）。

### 7.2 电源树（上电依赖）

```
电池 BAT+ ──> XL7045E1 降压 ──> +5V_M(5.3V) ──> H7233 LDO ──> +3.3V_M
                                                    │
                              Q109(SI2302) 开关 ──> +3.3V_MCU / +3.3V_AFE
隔离侧: DC-DC ──> ISO+5V_1 / ISO+5V_2 (ISO_GND) 给两路 485 收发器
AFE 侧: SH3673520 内部 LDO ──> LDO_P / +3.3V_AFE
```

> **关键**：上电由电池/按键/充电/通信触发，MCU 启动后**必须立即 `PowerHold_On` 拉住自锁脚**，否则松开按键即掉电。

### 7.3 接插件速查（pt10）

| 连接器 | 用途 |
|---|---|
| P1（TB4-30P FPC） | 采样线：电芯电压 CV0~16 + 温度 TS1~4 + BAT_SS+ |
| P3（M3 螺柱） | BAT+ 电池正（功率） |
| J1/J2、J4/J5 | 电池负 / 负载负 PACK-（功率） |
| P2/P4、RJ2（RJ45） | RS485 通信 |
| P5/P7 | 加热膜 |
| P8/P9/P10 | PTC NTC / 进水传感器 |
| SW1 | 开机按键 |

## 8. 软件开发上手（Demo 工程）

### 8.1 上电初始化顺序（main.c，按此顺序，勿随意调换）

```
HAL_Init → SystemClock_Config(168MHz) → DWT_Init → GPIO 初始化 → Led_Run_On
 → DMA → UART4 / USART1 初始化
 → ★ PowerHold_On() 电源自锁，必须尽早
 → AFE_WAKE 拉高 1s 再拉低 唤醒 SH3673520
 → ADC1 / SPI1 / SPI2 / TIM2 / TIM1 / RTC / TIM3 初始化
 → Get_Time()，若从 Standby 唤醒则清标志、关 RTC 闹钟
 → 停加热/充电 PWM（上电先保安全）
 → Usart1/Usart4 DMA 收发使能，TIM1 1ms 节拍中断启动
 → AppPower_Init / LedInit / HeatControlInit / SetDefaultAFEPara
 → VmosSSen_Init，SD_CS / FLASH_CS 置高
 → SD_Init / SPI_Flash_Init / initParameter / AppAfe_Reinit
 → InitVar / LimitInit / CMD_Init
 → ★ BmsControl_MosAllOffNow() 上电先全关 MOS
 → LogInit / Warn_Init → 进入 while(1)
```

### 8.2 主循环任务调度（while(1)，裸机顺序轮询）

```
ADC采样处理 → AFE采集 → 1秒任务(均衡) → AFE保护复位 → 负载检测 → 断线检测 → 均衡
→ BMS信息统计 → SOC估算 → 生成告警码/保护(×2) → 加热控制 → 上位机命令(CMD_Task)
→ MOS控制(BmsControlTask) → LED显示SOC → LED显示告警
→ 写Flash日志 → 参数保存 → OTA升级任务
→ 每20s存日志分片到Flash，每2s写CSV到SD卡 → 检测关机事件(长按2s→休眠)
```

> 节拍来自 TIM1 的 1 ms 中断（AppTime 提供 1ms/1s/2s/20s 标志）。

### 8.3 开发环境与上手步骤（Demo）

| 步骤 | 操作 |
|---|---|
| 1. 工具 | Keil MDK-ARM（需装 Keil::STM32F4xx_DFP） |
| 2. 打开工程 | MDK-ARM\S16100B_Demo.uvprojx（链接预编译 S16100B_Driver.lib） |
| 3. 编译 | Build，输出 MDK-ARM\S16100B_Demo\ 下 .hex/.axf/.bin |
| 4. 接调试器 | SWD：PA13(SWDIO)/PA14(SWCLK)+GND（P200 排针），ST-Link/J-Link |
| 5. 供电 | ★ 需接电池 BAT+ 真正上电(PowerHold 自锁)；仅 ST-Link 供电不一定能驱动整板 |
| 6. 烧录 | Keil 配好 SWD 后 Download |
| 7. 看输出 | RS485 发 "PRTE" 开打印 / "PRTD" 关；看 printf |
| 8. 调通信 | USB-RS485 接主口，9600/8N1，发 Modbus 0x03 读 0~3 版本号验证链路 |
| 9. 改逻辑 | 只改 Application\ 源码；驱动在 .lib 内，需改要拿完整工程重编库 |

**常见坑**：① 起来就掉电 → PowerHold_On 没执行到/自锁脚没拉住；② AFE 不通 → 检查 SPI2 与 AFE_WAKE 上电时序（那 1s 拉高很关键）；③ MOS 不动作 → 上电默认全关，需自检+协议命令/状态机才开；④ 看不到日志 → SD 卡(SPI1/FatFs)未初始化成功，先开 PRTE 或看 Flash 日志。

## 9. 引脚分配总表（附录速查，按端口排序）

| 引脚 | 外设/功能 | 信号名 | 方向 | 用途 |
|---|---|---|---|---|
| PA0 | SYS_WKUP | WAKE_IN | 输入 | 总唤醒入口 |
| PA1 | ADC1_IN1 | BmuAddr | 模拟入 | 地址拨码采集(定 485 从机地址) |
| PA2 | ADC1_IN2 | MOS_T | 模拟入 | 功率 MOS 温度 |
| PA3 | TIM2_CH4 | ChgCurPwmAdj | 输出 | 充电电流 PWM |
| PA4 | ADC1_IN4 | ENV_T | 模拟入 | 环境温度 |
| PA5/6/7 | SPI1 | SCK/MISO/MOSI | — | Flash + SD 共用总线 |
| PA9 | USART1_TX | 485_LUTx | — | 主 RS485 发 |
| PA10 | USART1_RX | 485_LURx | — | 主 RS485 收 |
| PA11 | GPIO 输出 | RS485_LU_Dir | 输出 | 主 RS485 方向(DE/RE) |
| PA13/14 | SWD | SWDIO/SWCLK | — | 调试口 |
| PB0 | ADC1_IN8 | Vmos | 模拟入 | MOS 两端电压 |
| PB1 | ADC1_IN9 | PTC_T | 模拟入 | PTC 加热膜温度 |
| PB2 | GPIO 输出 | VmosSSen | 输出 | Vmos 采样使能 |
| PB3 | GPIO 输入 | PWKeyCheck | 输入 | 开机键检测 |
| PB4 | GPIO 输出 | Soc25_LED | 输出 | SOC 25% 指示灯 |
| PB5 | GPIO 输出 | Soc50_LED | 输出 | SOC 50% 指示灯 |
| PB6 | GPIO 输出 | Soc75_LED | 输出 | SOC 75% 指示灯 |
| PB7 | GPIO 输出 | Soc100_LED | 输出 | SOC 100% 指示灯 |
| PB8 | GPIO 输出 | Run_LED | 输出 | 运行灯 |
| PB9 | GPIO 输出 | Alm_LED | 输出 | 告警灯 |
| PB12 | GPIO 输出 | SPI2_CS | 输出 | AFE 片选 |
| PB13/14/15 | SPI2 | SCK/MISO/MOSI | — | AFE 通信 |
| PC0 | ADC1_IN10 | WARTER_CHECK2 | 模拟入 | 进水/漏液检测 2 |
| PC1 | ADC1_IN11 | WARTER_CHECK1 | 模拟入 | 进水/漏液检测 1 |
| PC2 | GPIO 输出 | SD_CS | 输出 | SD 卡片选 |
| PC3 | GPIO 输入 | SD_DET | 输入 | SD 卡插入检测 |
| PC4 | GPIO 输出 | FLASH_CS | 输出 | 外部 Flash 片选 |
| PC5 | GPIO 输出 | ChargerEn | 输出 | 充电使能 |
| PC6 | GPIO 输出 | AFE_WAKE | 输出 | AFE 唤醒 |
| PC7 | TIM3_CH2 | HeatingPWM1 | 输出 | 加热膜 PWM |
| PC10 | UART4_TX | 485_U3Tx | — | 第二路 RS485 发 |
| PC11 | UART4_RX | 485_U3Rx | — | 第二路 RS485 收 |
| PC12 | GPIO 输出 | UART4_Dir | 输出 | 第二路 RS485 方向(DE/RE) |
| PC14/15 | RCC OSC32 | OSC32k | — | 32.768kHz / RTC |
| PD2 | GPIO 输出 | PW_HOLD | 输出 | 电源自锁(上电须尽早拉住) |
| PH0/1 | RCC OSC | OSC8M | — | 8MHz 主晶振 |
| NRST / VBAT / BOOT0 | — | — | — | 复位 / RTC 后备 / 启动 |

> **原理图待确认（2 处，与软件无关，建议问硬件）**：
> 1. 两处原理图网络名不一致（笔误）。
> 2. `VmosSSen`：pt1(电源页) 标 **PB1**、pt2(MCU页) 标 **PB2**，两页不一致——本文档采用 **PB2**。
