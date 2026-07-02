# M6 规划：真板硬件闭环（bms_f405 / S16100B / STM32F405RGT6）

> **定位**：本文是 M6「真机安全闭环」（[../../../concept/architecture.md](../../../concept/architecture.md) §11）的**工作规划活产物**——分解 M6、标注各项「本环境可做 / 需真板 HIL」、给出切片顺序。**非常青规范**：设计权威仍是 `../../../concept/` 契约（architecture / hardware-abstraction / safety / …）；本文随 M6 推进更新，切片落地后其需求↔实现↔测试进 [../../traceability.md](../../traceability.md)。
>
> **硬件依据**：真板即 `bms_f405` = **S16100B / STM32F405RGT6**，软硬接口权威见 [../../../reference/hardware/software-interface.md](../../../reference/hardware/software-interface.md)（原理图 `BPCB-PCB-MB-PD-S16100B V0.4`）。

## A. 现状起点

- 引擎侧安全链 **M0–M5 齐全**（bms_db / task / diag 生命周期 / bms_bms 含 PRECHARGE 骨架 / sys_mon + watchdog 门控策略），等真实 IO 后端接入。
- 板 port `boards/enervenue/bms_f405/` 有**最小骨架**（usart1 console + 时钟/GPIO），AFE/SPI/ADC/IWDG/RTC 节点与 defconfig 多为 TODO。
- `app/src/bms/afe/` 有占位后端（`afe.c` 边缘 + `afe_adc/afe_sim/afe_stub` + `afe_validate`），但**未拆**到 `hal/`。
- **尚无** `src/bms/hal/`、`measurement-control/meas/`、`measurement-control/contactor`。

## B. ⚠️ 关键设计对账（M6 动手前必须先对齐——design-first）

真板与现有架构契约有**实质偏差**；不先在契约层调和，后续实现会对不上。这是 **Phase 0** 的内容：

| # | 偏差 | 现契约假设 | 真板实际（S16100B） | 处理方向 |
|---|---|---|---|---|
| 1 | **通信** | 多处假设 **CAN** | 两路隔离 **RS485 + Modbus RTU / 私有(0xA5 0x5A)** | comm/hal 契约从「CAN」泛化为「传输无关帧」，或明确本板 RS485 backend |
| 2 | **「接触器」** | 独立 **GPIO 接触器 + 反馈** | **AFE 驱动充放电 MOS**（`AFEChg/DsgMosEn`）+ ChargerEn(PC5) | `contactor` 抽象落到「经 AFE 的 MOS 使能 + 状态回读」，非 GPIO |
| 3 | **预充** | M4 已建 PRECHARGE 态 + `precharge_complete/timeout` 输入 | safety.md SG-08 标「预充逻辑未实现」，接口未见独立预充回路/反馈 | 需向硬件确认本板有无预充硬件；否则 `precharge_complete` 语义按本板 MOS 使能时序实际定义 |
| 4 | **WDT** | 通用 WDT wrapper | STM32 **内部 IWDG** | hal/wdt 落 Zephyr stm32 wdt driver |
| 5 | **AFE** | 通用 AFE | **SH3673520** 私有 SPI（0x01/02/0B + CRC8，16 串 CV0~16 + 4 温 + 电流 + 总压） | 真实 backend 待写；现 sim/stub 占位 |

> 结论：**Phase 0 是一个纯文档 PR**，改 `concept/architecture.md`、`concept/hardware-abstraction.md`、`concept/safety.md` 相关条款以反映 `bms_f405` 真板，再进 Phase 1。

## C. M6 分解（标注「本环境可做」vs「需真板 / HIL」）

| 模块 | 内容 | 本环境可做 | 需真板 HIL |
|---|---|:---:|:---:|
| 结构重构 | `afe`→`hal/afe` + `measurement-control/meas`；`include/bms` 镜像分层；tests 树同步 | ✅ 纯结构 | — |
| hal/ wrapper 接口 + fake | afe / comm / contactor_io / adc / storage / wdt / rtc | ✅ 接口+fake | 真实 backend |
| measurement-control/meas | raw frame → validated snapshot（时间戳 / 有效位 / 序号） | ✅ 纯逻辑 | — |
| measurement-control/contactor | 经 AFE 的 MOS 期望态 + 反馈回读接口 + fake | ✅ 抽象+fake | 实测动作 |
| 板 dts / defconfig | SPI1/2、ADC、IWDG、RTC、GPIO、spi-nor 节点（对原理图 V0.4 核引脚） | ✅ 编译级 | 真实时钟/引脚验证 |
| SH3673520 SPI backend | 私有命令 / CRC8 / 唤醒 1s 时序 | ⚠️ 可写逻辑 | 时序/链路 |
| W25Q32 NVM | 参数 / 故障记录（spi-nor + nvs，双写防掉电） | ⚠️ 可写 | 掉电/分区验证 |
| IWDG 喂狗接线 | M5 门控 → hal/wdt → stm32 wdt | ⚠️ 可写 | 复位行为 |
| RS485 / Modbus comm | 协议逻辑 | ⚠️ 可写 | 链路 |
| 上电时序 | PowerHold(PD2) 早置、AFE_WAKE 1s、上电全关 MOS、硬件 ALERT ISR | ❌ | 全需 HW |
| ADC NTC | 温度 / Vmos / 进水换算 | ⚠️ 换算可测 | 采集 |

## D. 建议切片顺序

- **Phase 0（对账）**：落 B 节契约调和（纯文档 PR）。
- **Phase 1（软件结构，本环境可全绿）**：
  1. `afe` 拆 `hal/afe`（backends）+ `measurement-control/meas`（可信化）
  2. `include/bms` 镜像分层 + tests 树同步下沉
  3. `measurement-control/contactor` 抽象（经 AFE MOS 期望态 + 反馈）+ fake，接 M4 PRECHARGE / bms_bms 接触器期望态
  4. 其余 hal/ wrapper 接口 + fake（wdt / rtc / storage / comm / adc）
- **Phase 2（板 port，编译级）**：
  5. `bms_f405` dts/defconfig 补外设节点（对原理图 V0.4 核引脚）
  6. IWDG 喂狗接线（M5 门控 → hal/wdt → stm32 wdt）
- **Phase 3（真板 bring-up / HIL，需物理板——本环境做不了）**：
  7. SH3673520 真实 SPI backend
  8. W25Q32 NVM 故障记录 + 参数持久化
  9. RS485 / Modbus comm backend
  10. 上电时序 + MOS 全关自检 + 预充/反馈闭环 + 硬件 ALERT ISR

## E. 本环境边界（如实）

- **能**：Phase 0 全部、Phase 1 全部（纯逻辑/结构/fake，twister 可全绿）、Phase 2 编译级（build 门，无功能验证）。
- **不能**：Phase 3 一切需物理板 / 示波器 / HIL 的功能验证（SPI 时序、复位行为、掉电、真实采集、MOS 动作）——只能在有 S16100B 板 + SWD 调试器时做。

## F. 下一步

**Phase 0 设计对账**（B 节）——纯文档 PR，改 concept 契约以反映 `bms_f405` 真板（尤其 CAN→RS485、接触器→MOS、预充语义、WDT=IWDG）。之后进 Phase 1-①（afe 拆分）。
