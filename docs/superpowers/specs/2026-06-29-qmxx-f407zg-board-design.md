# 新增硬件板支持：qmxx_f407zg（启明欣欣 STM32F407 高配版 V5.1）设计

> 状态：设计已评审通过，待落实施计划。
> 日期：2026-06-29

## 1. 背景与目标

为 BMS 固件新增一块**现成开发板**的 Zephyr board 支持，用于在真实 Cortex-M4F 硬件上做 bring-up 与架构验证（先于/并行于自研板 `bms_f405`）。

- 目标板：**正点原子/启明欣欣 STM32F407 高配版 V5.1** 开发板。
- MCU：**STM32F407ZGT6**（Cortex-M4F，LQFP144，Flash 1MB / RAM 192KB）。
- 原理图来源（外部，非本仓库）：`启明欣欣407开发板(高配版)V5.1原理图.pdf`（2020-11，433KB）。引脚映射依据该原理图 `pdftotext` 提取核对。
- 该板为通用开发板，**无 BMS 专用外设**（无接触器驱动 / 电芯 AFE / 均衡）。BMS 业务在其上以 `afe_sim` 后端运行，并复用其 CAN / 以太网 / RS485 练通信、用 LED 指示。

## 2. 范围

**In scope**（"核心 + BMS 相关"，已确认）：
- 时钟（HSE 8MHz → 168MHz）
- USART1 console、LED×3、按键×4 + WK_UP
- CAN2、以太网（RMII + LAN8720A）、RS485（MAX485）
- board 文件 + app overlay/conf + CI build 矩阵接入

**Out of scope**（可延后，YAGNI，本次不配）：
- USB、SD/SDIO、FSMC-LCD、SDRAM、DCMI 摄像头、NAND、EEPROM、蜂鸣器、SPI Flash(W25Q)
- 真机 CAN/以太网的 CI 自动化（无硬件在 CI，仅编译）
- 将该板绑定到某个 BMS 角色（Master/BCU）——本次仅作 bring-up 目标

## 3. board 身份与文件结构

- **board id**：`qmxx_f407zg`（启明欣欣 = QMXX）。
- **厂商目录**：`alientek`（沿用 Zephyr 既有厂商 id）。
- out-of-tree，结构仿现有 `boards/enervenue/bms_f405/`：

```
boards/alientek/qmxx_f407zg/
  board.yml                 # 板/SoC 声明（HWMv2）
  qmxx_f407zg.dts           # devicetree（时钟 + 外设节点 + pinctrl 引用）
  qmxx_f407zg.yaml          # twister 元数据
  qmxx_f407zg_defconfig     # 默认 Kconfig（基础可启动）
  Kconfig.qmxx_f407zg       # 板级 Kconfig 入口
app/boards/
  qmxx_f407zg.overlay       # 应用级 devicetree overlay（按需绑定）
  qmxx_f407zg.conf          # 应用级 Kconfig（按需开 CAN/NET/RS485）
```

dts 派生自 `boards/olimex/stm32_e407`（同 F407ZGT6）；**时钟必须按本板 8MHz HSE 重配**（olimex 为 12MHz，不可照搬）。

## 4. 时钟设计

- HSE 外部晶振 **8MHz**（原理图 Y1，标注 "8M"）。
- PLL：M=8（VCO in 1MHz）、N=336（VCO 336MHz）、P=2 → **SYSCLK 168MHz**；Q=7 → 48MHz。
- 总线：AHB /1（168M）、APB1 /4（42M）、APB2 /2（84M）。Flash 等待周期 5 WS @168MHz/3.3V。
- 以太网 RMII REF_CLK **50MHz 由 LAN8720A 外部提供至 PA1**（MCU 不产生）。

预期 devicetree 片段（实现时核定）：
```dts
&clk_hse  { clock-frequency = <DT_FREQ_M(8)>; status = "okay"; };
&pll      { div-m=<8>; mul-n=<336>; div-p=<2>; div-q=<7>; clocks=<&clk_hse>; status="okay"; };
&rcc      { clocks=<&pll>; clock-frequency=<DT_FREQ_M(168)>;
            ahb-prescaler=<1>; apb1-prescaler=<4>; apb2-prescaler=<2>; };
```

## 5. 外设与引脚映射

| 功能 | 节点 | 引脚 | 说明 |
|------|------|------|------|
| Console | `usart1` | PA9(TX) / PA10(RX) | 115200-8N1；`zephyr,console` + `zephyr,shell-uart`；板载 CH340 USB 串口 |
| LED×3 | `gpio-leds` | LED0=PE3, LED1=PE4, LED2=PG9 | **低有效**（接 VCC3.3，GPIO_ACTIVE_LOW）；alias `led0/led1/led2` |
| 按键 | `gpio-keys` | KEY0=PF9, KEY1=PF8, KEY2=PF7, KEY3=PF6（低有效 + 上拉）；WK_UP=PA0（高有效） | alias `sw0` → WK_UP |
| CAN | `can2` | PB12(RX) / PB13(TX) | bxCAN，默认 500kbps；选 CAN2 以避开 USB(PA11/12) 与 ETH 冲突 |
| 以太网 | `mac` + `mdio` | REFCLK=PA1, MDIO=PA2, MDC=PC1, CRS_DV=PA7, RXD0=PC4, RXD1=PC5, TX_EN=PG11, TXD0=PG13, TXD1=PG14 | RMII；PHY **LAN8720A** |
| RS485 | `usart3` | PB10(TX) / PB11(RX)；DE=PG6 | 半双工；DE 方向控制（GPIO 或 STM32 硬件 DE） |

时钟/console/LED/按键/CAN2 为高置信；以太网与 RS485 见第 6、7 节。

## 6. 引脚复用冲突与取舍（已决策）

启明欣欣为多功能板，多处引脚跳线复用：

- **以太网占用 PA1/PA2/PA7** → **USART2(PA2/3) 与 UART4(PA0/1) 不可与以太网同时使用**。RS485 因此不走 USART2，改用 **USART3(PB10/PB11)**。
- **CAN1(PA11/PA12) 与 USB OTG-FS 二选一** → 本设计选 **CAN2(PB12/PB13)**，两者皆不与以太网（用 PG11/13/14）冲突，可与以太网共存。
- 以太网 TX 使用 PG11/PG13/PG14（非 PB12/13），故 CAN2 的 PB12/13 空闲可用。

## 7. 实现时验证清单（对照原理图 PDF 核定，非设计悬空项）

下列项有明确预期值，实现阶段对照 PDF 页确认后定稿：

1. **LAN8720A PHY 地址**：预期 0（ALIENTEK 常见），strap 决定 → 确认 mdio 子节点 `reg`。
2. **LAN8720A 复位脚**：确认是否接 MCU GPIO 或板级 RESET → 决定是否需 `reset-gpios`。
3. **RS485 USART 归属**：预期 USART3(PB10/PB11) → 对照 MAX485 的 RO/DI 接线确认（排除 USART2，因 PA2 被 ETH 占）。
4. **RS485 DE 控制方式**：PG6 用 GPIO 控制，或 STM32 UART 硬件 DE（视驱动支持）→ 确认 dts 写法。

## 8. 与 BMS 架构的关系

- **数据源后端**：此板无真实 AFE，BMS 业务用 `afe_sim` 后端（见 architecture.md「数据源后端可切换」）。业务层零改动即可在此板运行。
- **并存**：与 `mps2/an386`、`native_sim`、`bms_f405` 并存，互不替代；多板矩阵均保留。
- **通信练手**：可用该板的 CAN2 / 以太网 / RS485 验证 comm 模块的真实链路（对应 architecture.md 跨节点同步、协议边缘讨论）。
- 不引入接触器/保护真实 GPIO（此板无），保护链路在此板仍走仿真/日志。

## 9. 集成

- **board `_defconfig`**：基础可启动 —— `CONFIG_CLOCK_CONTROL=y`、`CONFIG_GPIO=y`、`CONFIG_SERIAL=y`、`CONFIG_UART_CONSOLE=y`、`CONFIG_CONSOLE=y`。
- **app `qmxx_f407zg.conf`**：按需开 `CONFIG_CAN=y`、`CONFIG_NET_*`/`CONFIG_ETH_STM32_HAL=y`、RS485 相关；BMS 模块 Kconfig 沿用默认（`afe_sim` 后端）。
- **app `qmxx_f407zg.overlay`**：为需要的 BMS 模块提供绑定（comm→can2/eth）；afe 无真实源故不绑 ADC。
- **`qmxx_f407zg.yaml`**（twister）：`ram: 192`、`flash: 1024`、`supported: gpio uart canbus eth`、`toolchain: [zephyr, gnuarmemb]`。
- **CI**：在 `.github/workflows/ci.yml` 的 build 矩阵加入 `alientek/qmxx_f407zg`（仅编译；真机无法在 CI 运行）。`west.yml` 无需改（hal_stm32 已在 allowlist）。

## 10. 测试与验收

- **AC1（编译）**：`west build -b alientek/qmxx_f407zg app` 成功；CI build 矩阵该板通过；现有 mps2/native_sim 不受影响。
- **AC2（真机启动）**：烧录后 USART1 console（115200）打印 Zephyr banner + BMS 心跳；LED 可点亮（低有效）。
- **AC3（外设，手动）**：CAN2 自/对收发通；以太网 link up、能 ping；RS485 收发——均真机手动验证，不进 CI。
- **AC4（无回归）**：`scripts/check.ps1` 全绿；twister 现有用例不受影响。

## 11. 风险

- 原理图文本提取（pdftotext）对多列布局有偏移，已用 STM32 alt-func 与右栏 pin# 交叉校验关键脚；第 7 节四项实现时对 PDF 复核。
- 以太网在 STM32F4 + LAN8720A 上的 RMII 时钟/PHY 配置较易出错；先保证 AC1/AC2，AC3 的以太网作为后续真机验证项。
- 真机相关项（AC2/AC3）依赖实物，不阻塞 board 定义与编译合入。
