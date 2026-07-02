# BMS 软件架构基线 v0

> **定位**：本文是当前 BMS 固件的软件架构决策基线（ADR 风格）。它冻结“先按什么架构实现”的共识，避免在代码演进中同时改变目标。本文定义 v0 决策、边界、暂不做事项和迁移顺序；详细接口约束见 [module-interface.md](../standard/module-interface.md)。
>
> **当前代码状态**：代码仍保留 `afe/soc/protection/balancing/comm` 的过渡实现和 zbus channel；它们是迁移起点，不是最终架构契约。
>
> **目标来源**：选择性吸收 foxBMS 2 的 task / database / diagnosis / system monitoring / BMS state machine 思想，用 Zephyr 原生 Kconfig、devicetree、device API、ztest、Twister 落地；不照搬 FreeRTOS、目录结构或代码生成体系。

## 1. 决策摘要

| ID | 决策 | v0 结论 |
|---|---|---|
| ADR-ARCH-001 | 架构中心 | 采用 engine core：`bms_task` / `bms_db` / `bms_diag` / `bms_bms`，zbus 仅作过渡/通知机制 |
| ADR-ARCH-002 | 模块通信 | 模块间数据交换以 `bms_db` entry 为契约；每个 entry 单一 owner，读者拿值拷贝 |
| ADR-ARCH-003 | 运行时模型 | 业务模块不自建长期线程，长期运行逻辑由 `bms_task` 集中调度 |
| ADR-ARCH-004 | 安全决策 | `bms_bms` 是接触器最终 owner；`bms_protection` 只输出保护判定，不直接闭合接触器 |
| ADR-ARCH-005 | 诊断模型 | 安全相关故障必须进入 `bms_diag`，不得只打日志或私有处理 |
| ADR-ARCH-006 | 硬件边界 | 业务逻辑不得直接依赖 STM32 HAL/寄存器；硬件访问经 wrapper、devicetree、Kconfig |
| ADR-ARCH-007 | 可测试性 | 状态转移、测量校验、保护判定、SOC/均衡策略优先写成纯函数，任务只做编排 |

## 2. 架构目标与非目标

### 目标

- **安全默认态可证明**：接触器默认 OPEN，仅 `bms_bms` 在满足条件时输出 CLOSED 期望态。
- **数据 owner 清晰**：每个系统数据只有一个写入者，多消费者只读快照。
- **诊断集中**：故障检测、严重度、锁存、恢复条件集中登记并可追溯。
- **任务集中调度**：周期、优先级、栈、最大运行时间、心跳超时可集中审计。
- **硬件可替换**：QEMU/native_sim、`qmxx_f407zg`、未来 `bms_f405` 不改变业务逻辑。
- **测试优先**：核心逻辑可脱离线程、硬件和 zbus 单独 ztest。

### 非目标（v0 暂不做）

- 不实现完整 foxBMS 2 目录、命名前缀、FreeRTOS 抽象或代码生成体系。
- 不把 HIL、FMEA、MCUboot、安全启动作为每个 PR 的默认硬门；触发条件见 [methodology.md](methodology.md)。
- 不在 v0 一次性铺开所有硬件 wrapper；先定义边界，随真板 bring-up 增量实现。
- 不在 `architecture.md` 维护 CI 门禁和阈值；当前门禁事实见 [gates.md](../quality/gates.md)。

## 3. 分层基线

```
Application
  bms_bms          主状态机、接触器期望态 owner
  bms_algorithm    SOC/SOH/SOE 等算法
  bms_balancing    均衡策略
  bms_comm         通信（CAN 或 RS485/Modbus）/上位机命令与上报

Engine
  bms_task         统一任务调度
  bms_db           数据交换中心
  bms_diag         诊断中心
  bms_sys          系统模式、全局条件、初始化协调
  bms_sys_mon      任务健康、运行时间、watchdog 门控
  bms_time         时间基准

Measurement / Control
  bms_meas         测量可信化：raw frame -> validated snapshot
  bms_protection   阈值/边界判定，输出保护状态
  bms_contactor    接触器/预充执行与反馈采集

Hardware Abstraction
  bms_hw_* / wrappers over Zephyr device API, devicetree, Kconfig

Driver / HAL
  Zephyr drivers, STM32 HAL/CMSIS, board support
```

依赖方向：

- 上层可以调用下层公开接口；下层不得反向依赖上层业务。
- Application 可以读写自己的 `bms_db` entry，可以调用 Engine 服务，不得直接调用其他 Application 模块内部函数。
- Application 不直接访问 Zephyr driver API；硬件访问通过 Measurement/Control 或 Hardware Abstraction。
- Driver/HAL 不包含业务状态机、保护策略或诊断策略。

## 4. ADR-ARCH-001：engine core 是架构中心

### 决策

采用 `bms_task` / `bms_db` / `bms_diag` / `bms_bms` 作为 v0 软件架构中心。

### 理由

- 当前五模块线程 + zbus 骨架适合快速起步，但不适合作为安全相关系统长期架构。
- BMS 的关键复杂度在数据可信性、诊断聚合、状态机决策和任务健康，不在单个模块内部。
- foxBMS 2 的 FTASK / Database / Diagnosis / BMS / System Monitoring 思想适合本项目，但实现应保留 Zephyr 原生机制。

### 约束

- zbus 可以作为过渡兼容层或 DB 更新通知机制，但模块契约以 `bms_db_*` API 为准。
- 新特性不得新增长期私有线程作为默认方案。
- 新模块必须说明所属层、DB entry、诊断 ID、任务入口和安全默认态。

## 5. ADR-ARCH-002：数据模型以 bms_db entry 为契约

### 决策

`bms_db` 是系统数据目录。模块间不传可变内部指针，不直接订阅对方内部状态；通过 entry 读写值快照。

### v0 entry 集

| Entry | 唯一 owner | 主要读取者 | 内容 |
|---|---|---|---|
| `DB_CELL_MEAS` | `bms_meas` | protection, algorithm, balancing, comm, bms | 单体电压、电流、温度、有效位、时间戳、序号 |
| `DB_SOC_STATE` | `bms_algorithm` | bms, comm, diag | SOC/SOH/SOE、估算状态 |
| `DB_PROT_STATE` | `bms_protection` | diag, bms, comm | 阈值判定、边界状态、输入有效性结果 |
| `DB_DIAG_STATE` | `bms_diag` | bms, sys, comm | 聚合故障、严重度、锁存状态 |
| `DB_BMS_STATE` | `bms_bms` | comm, balancing, sys_mon | 主状态机状态、接触器期望态 |
| `DB_COMMAND` | `bms_comm_rx` / system input adapter | bms, sys | 外部请求、复位、闭合/断开命令 |
| `DB_CONTACTOR_FB` | `bms_contactor` | bms, diag | 接触器反馈、预充状态 |
| `DB_TASK_HEALTH` | `bms_sys_mon` | diag, sys, comm | 任务运行时间、心跳、超时标志 |

### entry header 原则

每个 entry 至少具备等价元数据：

- `timestamp_ms`：数据产生或更新时间。
- `sequence`：更新序号，用于 stale / drop 检测。
- `validity`：数据有效性位，支持 partial validity。
- `source`：数据来源或后端，便于调试和追溯。

### 一致性与并发原则

- DB API 必须让读者获得一致的值拷贝；读者不得缓存 DB 内部可变指针。
- 同一采样周期内必须一致的数据应在同一 entry 内原子更新。
- ISR / zero-latency 路径默认不直接写 DB；只允许执行最小安全动作并设置可被后续任务接管的 latch / event。
- stale 判断由 `timestamp_ms`、`sequence` 和模块期望周期共同决定。

## 6. ADR-ARCH-003：运行时由 bms_task 集中调度

### 决策

长期运行逻辑由 `bms_task` 的任务表集中声明和调度。业务模块提供 `init`、周期入口和纯函数核心，不自行创建无限循环线程。

### v0 任务类别

| 类别 | 周期/触发 | v0 用途 | 说明 |
|---|---|---|---|
| fast cyclic | 目标上限 1ms，可选 | 时间敏感采样、快速诊断、系统监控时间戳 | 是否启用由安全需求和硬件能力决定，当前不为所有模块强制 1ms |
| safety cyclic | 10ms 级 | protection、BMS 状态机、接触器控制 | 必须高于 app/comm/background |
| app cyclic | 100ms 级 | SOC、均衡、周期上报、诊断老化 | 可按配置拆分 |
| blocking task | 队列/事件 | CAN RX、NVM/flash、日志等可能阻塞工作 | 不得阻塞 safety cyclic |
| background | 低优先级 | 健康打印、维护任务 | 不参与安全决策 |
| zero-latency path | 硬件中断 | 严重短路/硬件 ALERT 最小安全动作 | 只做 OPEN/latch/event，后续由 diag/bms 接管 |

### 优先级原则

- 表中的优先级是**相对顺序目标**，不是立即硬编码值；Zephyr 中数值越小优先级越高，最终值由任务表/Kconfig 实现统一声明。
- safety cyclic 必须高于 app、comm、background。
- blocking task 不得长期占用 CPU；必须通过阻塞等待、队列或异步接口让出执行权。
- 任务最大运行时间和心跳超时必须能被 `bms_sys_mon` 监控。

### 任务健康与 watchdog

- 每个被监控任务进入/退出时由 `bms_sys_mon` 记录时间戳与运行时间。
- 超过最大运行时间、心跳超时、栈余量不足等进入 `bms_diag`。
- watchdog 只由 engine/monitor 统一喂狗；只有安全关键任务健康时才喂狗。

## 7. ADR-ARCH-004：bms_bms 持有接触器最终决策权

### 决策

`bms_bms` 是电池系统主状态机和接触器期望态 owner。`bms_protection` 不直接闭合接触器，只输出保护判定；`bms_contactor` 执行 `bms_bms` 的期望态并反馈实际状态。

### bms_sys 与 bms_bms 边界

| 模块 | 负责 | 不负责 |
|---|---|---|
| `bms_sys` | 初始化协调、系统模式、全局命令合法性、硬件安全条件、维护/复位授权 | 不直接决定接触器 CLOSED |
| `bms_bms` | INIT/STANDBY/PRECHARGE/NORMAL/FAULT/LOCKED、接触器期望态、故障后的状态迁移 | 不直接访问 GPIO/driver，不私有处理诊断 |

### v0 主状态

| 状态 | 接触器期望态 | 说明 |
|---|---|---|
| `INIT` | OPEN | 初始化 DB、诊断、驱动、任务框架 |
| `STANDBY` | OPEN | 安全待机，等待合法闭合命令 |
| `PRECHARGE` | 预充路径 | 执行预充，检查电压爬升与超时 |
| `NORMAL` | CLOSED | 允许充放电，持续监控故障 |
| `FAULT` | OPEN | 故障打开，等待诊断恢复或锁存 |
| `LOCKED` | OPEN | 锁存故障，仅显式维护/上层命令可复位 |

状态转移核心写成纯函数，例如：

```c
enum bms_state bms_next_state(enum bms_state cur, const struct bms_state_inputs *in);
```

任务只负责读 DB、调用纯函数、执行 entry/exit action、写回 DB。

## 8. ADR-ARCH-005：诊断中心化

### 决策

`bms_diag` 是唯一诊断登记中心。安全相关异常不得只通过日志、局部变量或模块私有状态处理。

### 诊断来源

- `bms_meas`：测量无效、过期、冗余不一致、AFE 通信错误。
- `bms_protection`：过压、欠压、过流、过温、绝缘/互锁等安全阈值。
- `bms_contactor`：反馈不一致、预充超时、粘连检测失败。
- `bms_comm`：非法命令、通信超时、CAN bus-off。
- `bms_sys_mon`：任务超时、栈余量不足、watchdog 门控失败。

### severity 与 latch

| Severity | 含义 | BMS 反应 |
|---|---|---|
| `INFO` | 非安全信息 | 记录/上报 |
| `WARNING` | 可继续运行但需上报 | 限功率或提示 |
| `ERROR` | 不允许进入 NORMAL | 断开或保持 STANDBY/FAULT |
| `CRITICAL` | 必须立即 OPEN/锁存 | FAULT/LOCKED |

每个诊断项必须定义：

- set condition：何时置 active。
- clear condition：何时允许清除。
- latch policy：是否锁存，是否需要维护命令/上电复位/诊断老化。
- reaction：对 `bms_bms` 状态机的影响。

安全相关 clear 不得只因为信号瞬间恢复就自动清除；LOCKED 退出必须有显式授权路径。

## 9. ADR-ARCH-006：测量、保护、通信边界

### 测量可信化

`bms_meas` 不是简单采样线程，而是测量可信化模块：

```
driver read -> raw frame -> validate/merge -> DB_CELL_MEAS
```

每帧测量至少包含值、时间戳、有效性、序号。无效或过期测量不得被 protection 当作 NORMAL 依据。

### protection 角色

```
DB_CELL_MEAS + limits -> DB_PROT_STATE -> bms_diag -> bms_bms
```

`bms_protection` 只做阈值和边界判定，不直接驱动接触器。硬件严重故障例外：zero-latency IRQ 可直接 OPEN 并置 latch/event，随后由 `bms_diag` 和 `bms_bms` 接管上报与恢复。

### 通信角色

- RX：解析 CAN/上位机命令，写 `DB_COMMAND`，不得直接改 BMS 状态。
- TX：从 DB 取快照，按配置周期上报。
- CAN bus-off、超时、非法命令进入 `bms_diag`。

## 10. ADR-ARCH-007：配置、标定与硬件抽象边界

### 配置分层

- **编译期配置**：系统规模、启用模块、板级能力，使用 Kconfig / devicetree。
- **板级绑定**：AFE、通信总线（CAN 或 RS485）、GPIO、接触器/功率 MOS 反馈、预充控制，使用 devicetree 和 wrapper。
- **运行/标定参数**：保护阈值、滤波参数、容量、诊断老化、通信周期等需要版本、单位、合法范围、默认值、变更验证。

Kconfig 可以作为早期参数载体，但不得把“参数治理”简化为“加一个 Kconfig”。参数/标定治理契约见 [configuration-calibration.md](configuration-calibration.md)。

### 硬件抽象

- 业务模块不直接依赖 STM32 HAL/寄存器。
- 业务模块不直接操作未经 wrapper 的 GPIO/通信总线/ADC/WDT/flash。
- wrapper 暴露稳定接口，隐藏具体芯片、devicetree 节点和驱动差异。

### 板级具体绑定（bms_f405 / S16100B）

> 文中 `CAN`、GPIO 接触器等为**通用示例**；具体总线/执行器经 hal/ wrapper 抽象（[hardware-abstraction.md](hardware-abstraction.md) §2），业务逻辑与设计契约芯片无关。首个真板 `bms_f405`（S16100B / STM32F405RGT6，规格见 [../reference/hardware/software-interface.md](../reference/hardware/software-interface.md)）的板级 backend：
>
> - **通信**：两路隔离 **RS485 + Modbus RTU / 私有(0xA5 0x5A)**（非 CAN）。
> - **功率通路**：充放电 MOS 与**预充**均由 **AFE(SH3673520) 经 SPI2** 驱动（非独立 GPIO 接触器/预充回路）；`bms_bms` 的接触器期望态经 AFE 命令执行、实际状态经 AFE 寄存器回读，`PRECHARGE` 的 `precharge_complete/timeout` 由 AFE 预充状态/电压回读得出。
> - **看门狗**：STM32 **内部 IWDG**。
> - **测量/采集**：AFE(SH3673520) 私有 SPI（16 串电压 + 4 温 + 电流 + 总压）+ ADC1（NTC 温度 / Vmos / 进水）。

wrapper 边界、接口契约、ISR/zero-latency 与仿真桩化的权威契约见 [hardware-abstraction.md](hardware-abstraction.md)。

## 11. 当前状态与迁移路径

### 当前状态（v0 基线时刻）

- `afe/soc/protection/balancing/comm` 过渡实现仍可构建、可测。
- `bms_db` / `bms_diag` / `bms_bms` / `bms_task` 方向已确定，首批 engine core 证据链和 `DB -> DIAG -> BMS` 集成测试已存在。
- `qmxx_f407zg` 已作为 bring-up 构建目标进入 CI；执行型验证仍以 `native_sim` / QEMU 为主。
- `bms_f405` 目标板仍待 dts/defconfig 完善。

### 迁移顺序

| 阶段 | 目标 | 完成判据 |
|---|---|---|
| M0 | 保持现有代码可构建可测 | 当前测试与 CI 不回退 |
| M1 | 固化 `bms_db` entry 和 owner | zbus channel 有对应 DB entry；读写走 DB API |
| M2 | 引入 `bms_task` 任务表 | 新长期逻辑不再新增私有线程；至少 task pipeline smoke test 通过 |
| M3 | 诊断中心化到 `bms_diag` | protection/meas/comm/sys_mon 故障进入诊断 entry |
| M4 | `bms_bms` 持有接触器期望态 | protection 不直接闭合接触器；失效安全状态机测试通过 |
| M5 | `bms_sys_mon` / watchdog 门控 | 任务超时进入诊断；watchdog 统一喂狗策略明确 |
| M6 | 真机安全闭环 | 接触器/功率 MOS 执行、反馈、预充、硬件 ALERT、NVM 故障记录验证（bms_f405：MOS/预充经 AFE） |

迁移期间允许 zbus 与 DB 共存，但目标架构契约以 DB 为准。每一步必须保持现有 CI 与相关 ztest 通过。

## 12. 细化本基线的专题设计契约

下列 concept 专题契约细化本架构的对应决策，**agent 实现时以它们为准**（文档导航总览见 [../README.md](../README.md)，此处只给"哪篇细化哪个 ADR + 落地里程碑"的关系）：

| 契约 | 细化 | 落地里程碑（见 §11） |
|---|---|---|
| [data-model.md](data-model.md) | ADR-ARCH-002：DB entry / owner / validity / sequence / stale / copy-by-value | M1 |
| [runtime-model.md](runtime-model.md) | ADR-ARCH-003：任务表 / 时间基准 / 绝对节拍 / blocking 边界 / sys_mon / watchdog | M2·M5 |
| [diagnostics-fault-model.md](diagnostics-fault-model.md) | ADR-ARCH-005：severity / 去抖 / latch / aging / 故障→bms_bms | M3 |
| [configuration-calibration.md](configuration-calibration.md) | ADR-ARCH-007：参数分层 / 登记表元数据 / 校验钳制 / 来源治理 | 引入可变安全参数前 |
| [hardware-abstraction.md](hardware-abstraction.md) | ADR-ARCH-006：wrapper 边界 / 接口契约 / ISR·zero-latency / dt·Kconfig 绑定 | M6 |

安全概念见 [safety.md](safety.md)；模块接口标准见 [../standard/module-interface.md](../standard/module-interface.md)。

## 13. 参考

- foxBMS 2 Software Modules: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/modules.html
- foxBMS 2 Database Module: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/engine/database/database.html
- foxBMS 2 FTASK Module: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/task/ftask/ftask.html
- foxBMS 2 System Monitoring Module: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/engine/sys_mon/sys_mon.html
- foxBMS 2 BMS Module: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/application/bms/bms.html
