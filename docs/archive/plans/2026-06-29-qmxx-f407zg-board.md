# qmxx_f407zg 板支持 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 BMS 固件新增启明欣欣 STM32F407(高配版V5.1) 开发板（STM32F407ZGT6）的 Zephyr board 支持，能编译、可在真机启动，并接入 CI build 矩阵。

**Architecture:** out-of-tree board（`boards/alientek/qmxx_f407zg/`，靠 `zephyr/module.yml` 的 `board_root: .` 自动发现），dts 派生自同 MCU 的 `boards/olimex/stm32_e407`，按本板 8MHz HSE 与实际引脚（USART1 console / LED×3 / 按键 / CAN2 / 以太网 LAN8720A / RS485）重配。BMS 业务用默认 `afe_sim` 后端（此板无真实 AFE），与现有 mps2/native_sim/bms_f405 并存。

**Tech Stack:** Zephyr v4.4.0、Zephyr HWMv2 board、STM32 devicetree（hal_stm32 pinctrl）、west build、twister。

**前置：** 全程在 `bms-app/` 下执行；分支已为 `feat/board-qmxx-f407zg`；spec 见 `docs/superpowers/specs/2026-06-29-qmxx-f407zg-board-design.md`。所有 pinctrl token（`usart1_tx_pa9`/`can2_rx_pb12`/`usart3_tx_pb10`/`eth_*` 等）已确认存在于 hal_stm32。构建命令统一：

```
west build -p always -b qmxx_f407zg app
```

成功标志：结尾打印 `Memory region ... FLASH / RAM` 用量、退出码 0、无 error。

---

### Task 1: 最小可启动 board（时钟 + USART1 console）

**Files:**
- Create: `boards/alientek/qmxx_f407zg/board.yml`
- Create: `boards/alientek/qmxx_f407zg/Kconfig.qmxx_f407zg`
- Create: `boards/alientek/qmxx_f407zg/qmxx_f407zg_defconfig`
- Create: `boards/alientek/qmxx_f407zg/qmxx_f407zg.yaml`
- Create: `boards/alientek/qmxx_f407zg/qmxx_f407zg.dts`
- Create: `app/boards/qmxx_f407zg.conf`
- Create: `app/boards/qmxx_f407zg.overlay`

- [ ] **Step 1: 写 `board.yml`**

```yaml
# 启明欣欣 STM32F407(高配版V5.1) 开发板，MCU STM32F407ZGT6（LQFP144）。
board:
  name: qmxx_f407zg
  full_name: ALIENTEK QMXX STM32F407 (high-config V5.1)
  vendor: alientek
  socs:
    - name: stm32f407xx
```

- [ ] **Step 2: 写 `Kconfig.qmxx_f407zg`**

```
# 板级 Kconfig 入口（HWMv2）
config BOARD_QMXX_F407ZG
	select SOC_STM32F407XG
```

- [ ] **Step 3: 写 `qmxx_f407zg_defconfig`**

```
# qmxx_f407zg 默认 Kconfig（最小可启动）
CONFIG_ARM_MPU=y
CONFIG_HW_STACK_PROTECTION=y

CONFIG_CLOCK_CONTROL=y
CONFIG_GPIO=y

CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
```

- [ ] **Step 4: 写 `qmxx_f407zg.yaml`（twister 元数据，外设逐步补 supported）**

```yaml
identifier: qmxx_f407zg
name: ALIENTEK QMXX STM32F407 (high-config V5.1)
type: mcu
arch: arm
vendor: alientek
toolchain:
  - zephyr
  - gnuarmemb
ram: 192
flash: 1024
supported:
  - gpio
  - uart
```

- [ ] **Step 5: 写 `qmxx_f407zg.dts`（时钟 + console）**

```dts
/*
 * 启明欣欣 STM32F407(高配版V5.1) 开发板 — STM32F407ZGT6
 * HSE 8MHz → 168MHz；console = USART1(PA9/PA10, 板载 CH340)
 */
/dts-v1/;
#include <st/f4/stm32f407Xg.dtsi>
#include <st/f4/stm32f407z(e-g)tx-pinctrl.dtsi>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

/ {
	model = "ALIENTEK QMXX STM32F407 V5.1";
	compatible = "alientek,qmxx-f407zg";

	chosen {
		zephyr,console = &usart1;
		zephyr,shell-uart = &usart1;
		zephyr,sram = &sram0;
		zephyr,flash = &flash0;
		zephyr,dtcm = &ccm0;
	};
};

&clk_hse {
	clock-frequency = <DT_FREQ_M(8)>;
	status = "okay";
};

&pll {
	div-m = <8>;
	mul-n = <336>;
	div-p = <2>;
	div-q = <7>;
	clocks = <&clk_hse>;
	status = "okay";
};

&rcc {
	clocks = <&pll>;
	clock-frequency = <DT_FREQ_M(168)>;
	ahb-prescaler = <1>;
	apb1-prescaler = <4>;
	apb2-prescaler = <2>;
};

&usart1 {
	pinctrl-0 = <&usart1_tx_pa9 &usart1_rx_pa10>;
	pinctrl-names = "default";
	current-speed = <115200>;
	status = "okay";
};
```

- [ ] **Step 6: 写 `app/boards/qmxx_f407zg.conf`（应用级，afe 默认 SIM 无需改）**

```
# qmxx_f407zg 应用配置：此板无真实 AFE，BMS 用默认 afe_sim 后端（app/Kconfig 默认）。
# 外设子系统在后续任务按需开启（CAN/NET）。
```

- [ ] **Step 7: 写 `app/boards/qmxx_f407zg.overlay`（占位，后续按需绑定）**

```dts
/* qmxx_f407zg 应用级 overlay：此板无真实 AFE/接触器，暂无 BMS 绑定。 */
/ {
};
```

- [ ] **Step 8: 构建验证**

Run: `west build -p always -b qmxx_f407zg app`
Expected: 构建成功，结尾打印 `Memory region ... FLASH / RAM` 用量，退出码 0。

- [ ] **Step 9: 提交**

```bash
git add boards/alientek/qmxx_f407zg app/boards/qmxx_f407zg.conf app/boards/qmxx_f407zg.overlay
git commit -m "feat(board): add qmxx_f407zg minimal bootable board (clock + USART1 console)"
```

---

### Task 2: LED×3 + 按键

**Files:**
- Modify: `boards/alientek/qmxx_f407zg/qmxx_f407zg.dts`（在根节点 `/ { ... }` 内、`chosen` 之后追加 `leds`/`gpio_keys`/`aliases`）

- [ ] **Step 1: 在 dts 根节点内追加 LED / 按键 / aliases**

将下列块插入到 `qmxx_f407zg.dts` 根节点 `/ {` 内（紧跟 `chosen { ... };` 之后、根节点闭合 `};` 之前）：

```dts
	leds {
		compatible = "gpio-leds";

		led0: led_0 {
			gpios = <&gpioe 3 GPIO_ACTIVE_LOW>;
			label = "LED0";
		};
		led1: led_1 {
			gpios = <&gpioe 4 GPIO_ACTIVE_LOW>;
			label = "LED1";
		};
		led2: led_2 {
			gpios = <&gpiog 9 GPIO_ACTIVE_LOW>;
			label = "LED2";
		};
	};

	gpio_keys {
		compatible = "gpio-keys";

		key0: key_0 {
			gpios = <&gpiof 9 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
			label = "KEY0";
			zephyr,code = <INPUT_KEY_0>;
		};
		key1: key_1 {
			gpios = <&gpiof 8 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
			label = "KEY1";
			zephyr,code = <INPUT_KEY_1>;
		};
		key2: key_2 {
			gpios = <&gpiof 7 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
			label = "KEY2";
			zephyr,code = <INPUT_KEY_2>;
		};
		key3: key_3 {
			gpios = <&gpiof 6 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
			label = "KEY3";
			zephyr,code = <INPUT_KEY_3>;
		};
		wkup: key_wkup {
			gpios = <&gpioa 0 GPIO_ACTIVE_HIGH>;
			label = "WK_UP";
			zephyr,code = <INPUT_KEY_WAKEUP>;
		};
	};

	aliases {
		led0 = &led0;
		led1 = &led1;
		led2 = &led2;
		sw0 = &wkup;
	};
```

- [ ] **Step 2: 构建验证**

Run: `west build -p always -b qmxx_f407zg app`
Expected: 成功（devicetree 通过校验），退出码 0。

- [ ] **Step 3: 提交**

```bash
git add boards/alientek/qmxx_f407zg/qmxx_f407zg.dts
git commit -m "feat(board): qmxx_f407zg add 3 LEDs (active-low) + keys"
```

---

### Task 3: CAN2

**Files:**
- Modify: `boards/alientek/qmxx_f407zg/qmxx_f407zg.dts`（文件末尾追加 `&can2` 节点）

- [ ] **Step 1: 在 dts 末尾追加 CAN2 节点**

在 `qmxx_f407zg.dts` 末尾（根节点 `};` 之后）追加：

```dts
/*
 * CAN2（PB12=RX / PB13=TX）。选 CAN2 以避开 USB(PA11/12) 与以太网冲突。
 * 注：STM32 bxCAN 中 CAN2 为从机，真机运行时需 CAN1 时钟使能（CAN1 master）；
 *     本任务仅验证编译，CAN1-master 依赖在 AC3 真机 CAN 测试时一并处理。
 */
&can2 {
	pinctrl-0 = <&can2_rx_pb12 &can2_tx_pb13>;
	pinctrl-names = "default";
	status = "okay";
};
```

- [ ] **Step 2: 构建验证**

Run: `west build -p always -b qmxx_f407zg app`
Expected: 成功（can2 节点通过 DT 校验；未开 `CONFIG_CAN` 时驱动不编入），退出码 0。

- [ ] **Step 3: 提交**

```bash
git add boards/alientek/qmxx_f407zg/qmxx_f407zg.dts
git commit -m "feat(board): qmxx_f407zg enable CAN2 (PB12/PB13)"
```

---

### Task 4: 以太网（RMII + LAN8720A）

**Files:**
- Modify: `boards/alientek/qmxx_f407zg/qmxx_f407zg.dts`（末尾追加 `&mac` 与 `&mdio`）

- [ ] **Step 1: 在 dts 末尾追加以太网节点**

在 `qmxx_f407zg.dts` 末尾追加（引脚与同 MCU 的 olimex_stm32_e407 一致，已对照本板原理图）：

```dts
/*
 * 以太网 RMII + LAN8720A（PHY @ addr 0）。
 * REFCLK=PA1, MDIO=PA2, MDC=PC1, CRS_DV=PA7, RXD0=PC4, RXD1=PC5,
 * TX_EN=PG11, TXD0=PG13, TXD1=PG14。
 * 待真机核实（spec §7）：PHY 地址(0/1)、PHY 复位脚是否需 reset-gpios。
 */
&mac {
	status = "okay";
	pinctrl-0 = <&eth_ref_clk_pa1
		     &eth_mdio_pa2
		     &eth_crs_dv_pa7
		     &eth_rxd0_pc4
		     &eth_rxd1_pc5
		     &eth_tx_en_pg11
		     &eth_txd0_pg13
		     &eth_txd1_pg14>;
	pinctrl-names = "default";
	phy-connection-type = "rmii";
	phy-handle = <&eth_phy>;
};

&mdio {
	status = "okay";
	pinctrl-0 = <&eth_mdc_pc1>;
	pinctrl-names = "default";

	eth_phy: ethernet-phy@0 {
		compatible = "ethernet-phy";
		reg = <0x00>;
	};
};
```

- [ ] **Step 2: 构建验证**

Run: `west build -p always -b qmxx_f407zg app`
Expected: 成功（mac/mdio/phy 节点通过 DT 校验；未开 `CONFIG_NET_*` 时以太网驱动不编入），退出码 0。

- [ ] **Step 3: 提交**

```bash
git add boards/alientek/qmxx_f407zg/qmxx_f407zg.dts
git commit -m "feat(board): qmxx_f407zg add Ethernet RMII + LAN8720A phy"
```

---

### Task 5: RS485（USART3 + DE=PG6）

**Files:**
- Modify: `boards/alientek/qmxx_f407zg/qmxx_f407zg.dts`（末尾追加 `&usart3`）

- [ ] **Step 1: 在 dts 末尾追加 USART3（RS485 数据通道）**

在 `qmxx_f407zg.dts` 末尾追加：

```dts
/*
 * RS485（MAX485）数据通道 = USART3（PB10=TX / PB11=RX）。
 * 方向控制 DE/RE = PG6，由应用以普通 GPIO 控制（spec §7：DE 机制待真机核实）。
 * 选 USART3 因 USART2(PA2/PA3) 被以太网 MDIO 占用。
 */
&usart3 {
	pinctrl-0 = <&usart3_tx_pb10 &usart3_rx_pb11>;
	pinctrl-names = "default";
	current-speed = <115200>;
	status = "okay";
};
```

- [ ] **Step 2: 构建验证**

Run: `west build -p always -b qmxx_f407zg app`
Expected: 成功，退出码 0。

- [ ] **Step 3: 提交**

```bash
git add boards/alientek/qmxx_f407zg/qmxx_f407zg.dts
git commit -m "feat(board): qmxx_f407zg add USART3 for RS485 (DE=PG6, app-controlled)"
```

---

### Task 6: 更新 twister supported + 接入 CI build 矩阵

**Files:**
- Modify: `boards/alientek/qmxx_f407zg/qmxx_f407zg.yaml`（扩充 `supported`）
- Modify: `.github/workflows/ci.yml:26-27,35`（注释 + 矩阵）

- [ ] **Step 1: 扩充 `qmxx_f407zg.yaml` 的 supported**

将 `qmxx_f407zg.yaml` 的 `supported:` 段替换为：

```yaml
supported:
  - gpio
  - uart
  - canbus
  - eth
```

- [ ] **Step 2: 把板加入 CI build 矩阵**

编辑 `.github/workflows/ci.yml`，把矩阵行（约第 35 行）：

```yaml
        board: [mps2/an386, native_sim]
```

改为：

```yaml
        board: [mps2/an386, native_sim, qmxx_f407zg]
```

并把其上方注释（约第 26-27 行）：

```yaml
  # NOTE: bms_f405 is an incomplete board template — add `enervenue/bms_f405`
  # to the matrix once its dts/defconfig TODOs are resolved.
```

更新为：

```yaml
  # NOTE: bms_f405 is an incomplete board template — add `enervenue/bms_f405`
  # to the matrix once its dts/defconfig TODOs are resolved.
  # qmxx_f407zg (ALIENTEK F407ZGT6 dev board) is build-only in CI — no hardware to run.
```

- [ ] **Step 3: 本地复核 CI 命令对本板成立**

Run: `west build -p always -b qmxx_f407zg app`
Expected: 成功（即 CI `west build -b qmxx_f407zg bms-app/app` 等价可过），退出码 0。

- [ ] **Step 4: 校验 yaml 语法**

Run: `python -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml',encoding='utf-8')); yaml.safe_load(open('boards/alientek/qmxx_f407zg/qmxx_f407zg.yaml',encoding='utf-8')); print('yaml OK')"`
Expected: 打印 `yaml OK`。

- [ ] **Step 5: 提交**

```bash
git add boards/alientek/qmxx_f407zg/qmxx_f407zg.yaml .github/workflows/ci.yml
git commit -m "ci(board): add qmxx_f407zg to build matrix; declare canbus/eth support"
```

---

### Task 7: 文档登记

**Files:**
- Modify: `docs/superpowers/specs/2026-06-29-qmxx-f407zg-board-design.md:3`（状态置为已实现）
- Modify: `CLAUDE.md`（在「项目概览」板卡说明处登记新板）

- [ ] **Step 1: 更新 spec 状态行**

把 spec 第 3 行：

```
> 状态：设计已评审通过，待落实施计划。
```

改为：

```
> 状态：已实现（board 文件 + CI build 矩阵接入）。真机外设验证（AC2/AC3）待硬件。
```

- [ ] **Step 2: 在 `CLAUDE.md` 项目概览登记新板**

在 `CLAUDE.md` 「项目概览」一节，`native_sim` 说明行附近追加一行：

```
- **真机 bring-up 板**：`boards/alientek/qmxx_f407zg/`（启明欣欣 STM32F407ZGT6 开发板），已入 CI build 矩阵；业务走 `afe_sim`，CAN/以太网/RS485 真机手动验证。
```

- [ ] **Step 3: 提交**

```bash
git add docs/superpowers/specs/2026-06-29-qmxx-f407zg-board-design.md CLAUDE.md
git commit -m "docs(board): register qmxx_f407zg; mark spec implemented"
```

---

## 收尾（实现完成后）

- 运行 `powershell -ExecutionPolicy Bypass -File scripts\check.ps1 -Fast` 确认 format+build+test 全绿（现有 mps2/native_sim 无回归）。
- `git push -u origin feat/board-qmxx-f407zg` → `gh pr create --base master`（Squash 合并；CI build 矩阵新板需通过）。
- 真机相关验收（AC2 console 启动、AC3 CAN/以太网/RS485）在拿到实物后手动完成，不阻塞本 PR 合入。
