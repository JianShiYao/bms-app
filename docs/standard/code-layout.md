# BMS 代码布局标准 v0（设计契约）

> **定位**：本文把 [../concept/architecture.md](../concept/architecture.md) §3 的分层基线落成**代码目录组织的强制约定**——规定 `app/src/bms/` 与 `app/include/bms/` 按**层**分目录（层内见模块）、hal/ 的 wrapper 清单、跨层依赖铁律、straddler 拆分、CMake/Kconfig 组织。**agent 新增/重构代码时以本文为准**；本文规定**目标布局**，不描述现状实现。
>
> **现状与差距**不在此维护：现有 `app/src/bms/` 为按模块平铺的过渡形态；本布局**随 [../concept/architecture.md](../concept/architecture.md) §11 的 M1–M6 逐步归位**，非一次性大爆炸重排。
>
> **规范措辞**：**必须 / 应 / 不得**。相关：分层基线 [../concept/architecture.md](../concept/architecture.md) §3，模块接口 [module-interface.md](module-interface.md)，硬件抽象 [../concept/hardware-abstraction.md](../concept/hardware-abstraction.md)，真板规格 [../reference/hardware/software-interface.md](../reference/hardware/software-interface.md)。

## 1. 布局设计原则

- **目录即层**：顶层目录**必须**对应分层基线的层；打开目录即知模块归哪一层、可依赖谁。
- **层内见模块**：每个模块是其层目录下的源文件；模块有多个源文件时**应**用自己的子目录（如 `hal/afe/`）。
- **结构表达依赖**：目录结构服务于 §5 的依赖铁律——布局本身让越权依赖显眼、易审。
- **只组织、不改行为**：布局调整**不得**改变业务行为；归位与功能改动分开提交。
- **测试镜像层**：`tests/bms/` **必须**按层镜像被测源——单元测试置 `tests/bms/<层>/<模块>/`（层同 §3 的 `application`/`engine`/`measurement-control`/`hal`，模块名同被测源目录）；**跨层**集成测试置 `tests/integration/<场景>/`（无单一层归属，不镜像层）。测试树随源树的 M-阶段归位同步下沉。

## 2. 顶层分层目录（契约）

我方代码分 4 层目录（Driver/HAL 层为 Zephyr/vendor 外部代码，不在本仓建目录）：

```
app/src/bms/
  application/            业务意图与决策
    bms.c                 主状态机、接触器期望态 owner
    algorithm/            SOC/SOH/SOE 等（原 soc）
    balancing.c           均衡策略
    comm/                 CAN/上位机命令与上报
  engine/                 横切核心服务（共享脊柱）
    task.c db.c diag.c    调度 / 数据交换 / 诊断
    sys.c sys_mon.c time.c  系统模式 / 任务健康·watchdog门控 / 时间基准
  measurement-control/    感知与执行（基线的 “Measurement / Control” 一层）
    meas/                 测量可信化：raw frame → validated snapshot
    protection.c          阈值/边界判定，输出保护状态
    contactor.c           接触器/预充执行与反馈采集
  hal/                    硬件抽象 wrapper（见 §4）
    afe/ comm/ contactor_io/ adc/ storage/ pwm/ gpio/ rtc/ wdt/ time/
  main.c                  应用入口
```

- `app/include/bms/` **必须镜像**同一层级：`include/bms/<层>/<模块>.h`（公共头）。私有头与其 `.c` 同目录。**跨层共享的公共类型头**（如 `types.h`，非单一模块所有）留 `include/bms/` 根。
- `channels.c`（zbus 过渡兼容）归 `engine/`，随迁移逐步退场。

## 3. 层↔目录映射（契约）

| 层（[architecture.md](../concept/architecture.md) §3） | 目录 | 典型模块 |
|---|---|---|
| Application | `application/` | `bms`、`algorithm`、`balancing`、`comm` |
| Engine | `engine/` | `task`、`db`、`diag`、`sys`、`sys_mon`、`time` |
| Measurement / Control | `measurement-control/` | `meas`、`protection`、`contactor` |
| Hardware Abstraction | `hal/` | 见 §4 wrapper 清单 |
| Driver / HAL | —（Zephyr/vendor，外部） | 不在本仓 |

## 4. `hal/` wrapper 清单（契约）

hal/ 子目录按**能力**划分，与 [../concept/hardware-abstraction.md](../concept/hardware-abstraction.md) §2 的能力集一一对应；**按板取用**（非每板全建）。

| hal/ 子目录 | 能力 | 对应 concept 契约 |
|---|---|---|
| `afe/` | AFE 原始帧/标志/ALERT（含后端 adc/sim/stub） | hardware-abstraction §4·§8 |
| `comm/` | CAN 或 RS485/UART 帧收发、总线错误 | hardware-abstraction §2 |
| `contactor_io/` | 接触器/MOS 输出与反馈（GPIO 或经 AFE） | hardware-abstraction §2 |
| `adc/` | 辅助模拟量（温度/进水/Vmos…） | hardware-abstraction §2 |
| `storage/` | NVM/flash（+ 可选 SD/FatFs）持久化 | configuration-calibration §6 |
| `pwm/` | 加热/充电 PWM（按板） | software-interface §4.5 |
| `gpio/` | 指示灯/按键/电源自锁（按板） | software-interface §4.6 |
| `rtc/` | 时间戳/定时唤醒（按板） | software-interface §2 |
| `wdt/` | 硬件看门狗 | runtime-model §7 |
| `time/` | `bms_time` 的单调计时后端 | runtime-model §2 |

> 具体到 `bms_f405`（=S16100B）的外设/引脚/协议见 [../reference/hardware/software-interface.md](../reference/hardware/software-interface.md)；`qmxx_f407zg` 的 `comm/` 为 CAN，`bms_f405` 的为 RS485。

## 5. 跨层依赖铁律（契约）

`#include` 与调用**必须**遵守下表（✓ 允许，✗ 禁止）；跨**业务层**的数据一律经 `engine/db`（`bms_db`），**不得**直接 include 对方内部。

| 从 \ 到 | application | engine | measurement-control | hal | Zephyr driver |
|---|:--:|:--:|:--:|:--:|:--:|
| **application** | 自身 | ✓ | ✗（经 `bms_db`） | ✗ | ✗ |
| **engine** | ✗ | 自身 | ✗ | ✓（time/wdt 后端） | ✗（经 hal） |
| **measurement-control** | ✗ | ✓ | 自身 | ✓ | ✗（经 hal） |
| **hal** | ✗ | ✗ | ✗ | 自身 | ✓ |

- **engine 是共享脊柱**：被 application 与 measurement-control **向下依赖**，自身**不得**依赖任何业务层。
- **hal 最底**：只依赖 Zephyr driver / devicetree / Kconfig，**不得** include 任何 `bms` 业务层。
- 业务模块**不得**直接访问 Zephyr driver API（`GPIO/CAN/UART/ADC/WDT/flash`）——一律经 `hal/`（ADR-ARCH-006）。
- 执行**先靠**目录+命名+评审；**层依赖 CI 检查**列为后续可选（不在本契约强上）。

## 6. straddler 拆分规则（契约）

一个功能若**跨层**，**必须**按层拆分、而非塞进单一模块：

- **`afe`**（示例）：硬件访问（SPI 时序/寄存器/后端 adc·sim·stub）属 `hal/afe/`；测量可信化（校验/有效位/合并）属 `measurement-control/meas/`。二者经 `bms_db`/接口衔接，**不得**在 hal 里做可信化、也**不得**在 meas 里碰寄存器。
- 通用规则：**搬运/寄存器 → hal**；**判定/可信化/策略 → 上层**（[../concept/hardware-abstraction.md](../concept/hardware-abstraction.md) §1·§4）。
- 拆分是**目标**；现有 `afe/` 的实际拆分随 M1–M6 迁移执行，本契约只定终态。

## 7. CMake / Kconfig 组织（契约）

- **CMake**：`app/CMakeLists.txt` 按层组织源文件（可按层/模块分组 `target_sources`）；新增模块加入其**所属层**分组。
- **Kconfig**：模块启用/裁剪用 `CONFIG_BMS_<模块>`；板级能力（含 `hal/` 各 wrapper 是否启用）用 Kconfig + devicetree 表达（[../concept/configuration-calibration.md](../concept/configuration-calibration.md) §2）。
- **仿真桩**：`hal/` 各 wrapper 在 `native_sim`/QEMU 下可选桩后端（如 `afe/afe_sim`），桩**不得**放宽安全默认（[../concept/hardware-abstraction.md](../concept/hardware-abstraction.md) §8）。

## 8. 命名与文件约定（契约）

- 符号前缀 `bms_`；模块内公共符号 `bms_<模块>_*`（[module-interface.md](module-interface.md)）。
- 源文件小写、与模块名一致；公共头置镜像的 `include/bms/<层>/` 下。
- `main.c` 只做启动编排（调各层 `init`），**不得**含业务逻辑。

## 9. 迁移

本布局随 [../concept/architecture.md](../concept/architecture.md) §11 的 **M1–M6 逐步归位**：新代码**必须**落到正确层目录；既有过渡代码在其被 M-阶段触及时归位（如 M2 引 `engine/time`、M3 重构 `engine/diag`、afe 在 M6 前拆 `hal/afe`+`measurement-control/meas`）。**不做独立的大爆炸重排**；每步保持 CI 与 ztest 通过、且布局调整与功能改动分离提交。

## 10. 参考

- [../concept/architecture.md](../concept/architecture.md) §3（分层基线）、§11（迁移）。
- [module-interface.md](module-interface.md)（模块接口/owner/安全默认态）。
- [../concept/hardware-abstraction.md](../concept/hardware-abstraction.md)（wrapper 能力集与边界）。
- [../reference/hardware/software-interface.md](../reference/hardware/software-interface.md)（`bms_f405`/S16100B 具体外设）。
