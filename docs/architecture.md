# BMS 固件架构（foxBMS 2 inspired）

> **文档定位**：本文档定义目标架构，不再以当前 5 个线程骨架作为主架构。
> 当前代码仍保留 `afe/soc/protection/balancing/comm` 线程与 zbus channel；它们是迁移起点。
> 目标架构尽量贴近 foxBMS 2 的任务框架、数据库、诊断、系统监控、BMS/SYS 状态机思想，同时用 Zephyr 原生机制落地。

## 设计目标

- **诊断与安全优先**：故障检测、锁存、恢复条件、接触器控制必须集中、可审计、可测试。
- **任务框架驱动系统**：长期运行逻辑不得各自随意循环，必须挂入统一任务框架。
- **数据库作为数据交换中心**：模块之间不直接调用、不共享裸全局变量，通过 database/data-exchange 模块交换快照。
- **BMS/SYS 状态机集中决策**：整机状态、接触器状态、预充、故障恢复由集中状态机管理。
- **硬件独立**：应用模块不直接依赖 STM32 HAL/寄存器，硬件差异下沉到 driver wrapper、devicetree、Kconfig。
- **纯函数可测**：状态转移、诊断判定、测量校验、SOC 计算等核心逻辑可脱离线程单测。

## foxBMS 2 借鉴边界

| foxBMS 2 概念 | 本项目目标模块 | 说明 |
|---------------|----------------|------|
| `FTASK` | `bms_task` | 统一任务框架，管理 1ms/10ms/100ms 等周期任务与阻塞任务 |
| `Database` | `bms_db` | 单生产者、多消费者的数据交换中心，保存最新可信快照 |
| `Diagnosis` | `bms_diag` | 诊断条目、严重度、锁存、恢复/老化、持久化记录入口 |
| `System Monitoring` | `bms_sys_mon` | 任务运行时间、心跳、看门狗喂狗门控 |
| `SYS` | `bms_sys` | 系统模式、初始化、全局命令、硬件安全条件 |
| `BMS` | `bms_bms` | 电池系统主状态机：INIT/STANDBY/PRECHARGE/NORMAL/FAULT |
| Driver wrappers | `drivers/` + `bms_hw_*` | AFE、contactor、CAN、watchdog、NVM 等硬件封装 |

> 不照搬 foxBMS 2 的 FreeRTOS、代码生成和目录结构；保留 Zephyr 的 Kconfig、devicetree、device API、ztest、Twister。

## 分层架构

```
┌────────────────────────────────────────────────────────────┐
│ Application                                                  │
│   bms_bms   bms_algorithm   bms_balancing   bms_comm         │
│   主状态机   SOC/SOH/SOE     均衡策略        CAN/命令/上报      │
├────────────────────────────────────────────────────────────┤
│ Engine                                                       │
│   bms_task  bms_db  bms_diag  bms_sys  bms_sys_mon  bms_time │
│   任务框架   数据库   诊断      系统服务   任务监控     时间基准   │
├────────────────────────────────────────────────────────────┤
│ Measurement / Control                                        │
│   bms_meas  bms_protection  bms_contactor                    │
│   测量可信化  阈值判定        接触器/预充执行                  │
├────────────────────────────────────────────────────────────┤
│ Hardware Abstraction                                         │
│   AFE wrapper  CAN wrapper  GPIO/contactor wrapper  NVM/WDT   │
│   Zephyr device API + devicetree + Kconfig                    │
├────────────────────────────────────────────────────────────┤
│ Driver / HAL                                                 │
│   adc/can/gpio/wdt/flash drivers, STM32 HAL/CMSIS             │
└────────────────────────────────────────────────────────────┘
```

**依赖方向铁律**：

- Application 可以读写 `bms_db`，可以调用 Engine 的公开服务，但不得调用其它 Application 模块内部函数。
- Application 不直接访问 Zephyr driver API；硬件访问通过 Measurement/Control 或 Hardware Abstraction。
- Engine 不依赖具体 AFE/CAN 芯片。
- Driver/HAL 不反向依赖业务模块。

## 核心数据流

```
AFE/CAN/GPIO/NVM drivers
        ↓
bms_meas / bms_comm_rx / bms_contactor_feedback
        ↓ 写入
      bms_db  ←→  bms_diag
        ↓         ↑
  bms_algorithm   bms_protection
        ↓         ↑
      bms_bms / bms_sys
        ↓
 bms_contactor / bms_balancing / bms_comm_tx
```

### 数据库原则

`bms_db` 是系统数据目录，替代“模块之间互相订阅对方内部状态”的架构中心。

- **单一生产者**：每个 database entry 有唯一 owner，例如测量快照只由 `bms_meas` 写入，SOC 只由 `bms_algorithm` 写入。
- **多消费者**：消费者读快照，不持有生产者内部指针。
- **数据带 header**：每个 entry 至少包含 `timestamp_ms`、`validity`、`sequence`、`source` 或等价元信息。
- **一致性快照**：同一采样周期内必须一致的数据放在同一 entry 中原子更新。
- **zbus 降级为实现细节**：目标架构中，zbus 可用于 database 更新通知或事件唤醒，但模块契约以 `bms_db_read/write` 为准。

示例 entry：

| Entry | Producer | Consumers | 内容 |
|-------|----------|-----------|------|
| `DB_CELL_MEAS` | `bms_meas` | algorithm, protection, balancing, comm | 单体电压、电流、温度、时间戳、有效位 |
| `DB_SOC_STATE` | `bms_algorithm` | bms, comm, diag | SOC/SOH/SOE |
| `DB_PROT_STATE` | `bms_protection` | diag, bms, comm | 阈值判定结果 |
| `DB_DIAG_STATE` | `bms_diag` | bms, sys, comm | 聚合故障、锁存、严重度 |
| `DB_BMS_STATE` | `bms_bms` | comm, balancing, sys_mon | 主状态机状态 |
| `DB_COMMAND` | `bms_comm_rx` | bms, sys | 外部请求、复位、闭合/断开命令 |
| `DB_CONTACTOR_FB` | `bms_contactor` | bms, diag | 接触器反馈、预充状态 |

## 任务框架

目标任务框架参考 foxBMS 2 的 `FTASK`：系统由少量明确任务驱动，任务再调用模块主函数。模块不自行创建长期线程。

### 任务类型

| 类型 | 周期/触发 | 约束 |
|------|-----------|------|
| 1ms fast cyclic | 1ms | 时间敏感测量、诊断快路径、CAN RX 消化、系统监控时间戳 |
| 10ms cyclic | 10ms | BMS/SYS 状态机、保护判定、接触器控制、均衡调度 |
| 100ms cyclic | 100ms | SOC 慢算法、CAN TX 周期上报、诊断老化、NVM 低频任务 |
| blocking task | 队列/中断/event | CAN RX、NVM/flash、日志等可能阻塞的工作 |
| zero-latency path | 硬件中断 | 严重短路/硬件 ALERT，仅执行最小安全动作 |

### 目标任务表

| Task | 类型 | 优先级 | 调用模块 |
|------|------|--------|----------|
| `tsk_fast_1ms` | cyclic | 2 | `bms_meas_1ms`, `bms_diag_1ms`, `bms_sys_mon_enter/exit` |
| `tsk_safety_10ms` | cyclic | 3 | `bms_protection_10ms`, `bms_bms_10ms`, `bms_contactor_10ms` |
| `tsk_app_100ms` | cyclic | 5 | `bms_algorithm_100ms`, `bms_balancing_100ms`, `bms_comm_tx_100ms` |
| `tsk_comm_rx` | blocking | 6 | `bms_comm_rx_process` |
| `tsk_engine` | cyclic/blocking | 4 | `bms_db_process`, `bms_diag_process`, `bms_sys_mon_check` |
| `tsk_background` | cyclic | 8 | health print、低优先级维护 |

> 当前各模块 `K_THREAD_DEFINE` 是过渡形态。目标是改为集中任务表：名称、周期、优先级、栈、最大运行时间、心跳超时都在一处声明。

### 任务健康与看门狗

- 每个 cyclic task 进入/退出时由 `bms_sys_mon` 记录时间戳与运行时间。
- 超过最大运行时间或心跳超时，`bms_sys_mon` 创建诊断条目。
- 看门狗只由 engine/monitor 统一喂狗；只有安全关键任务全部健康时才喂狗。
- 任务健康故障进入 `bms_diag`，再由 `bms_bms`/`bms_sys` 决定 OPEN/LOCKED。

## 诊断架构

`bms_diag` 是唯一诊断登记中心，不允许模块各自私有处理安全故障。

诊断条目建议包含：

```c
struct bms_diag_entry {
    enum bms_diag_id id;
    enum bms_diag_severity severity;
    enum bms_diag_latch latch;
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    uint16_t occurrence_count;
    bool active;
};
```

诊断来源：

- `bms_meas`：测量无效、过期、冗余不一致、AFE 通信错误。
- `bms_protection`：过压、欠压、过流、过温、绝缘/互锁等安全阈值。
- `bms_contactor`：接触器反馈不一致、预充超时、粘连检测失败。
- `bms_comm`：命令非法、通信超时、CAN bus-off。
- `bms_sys_mon`：任务超时、栈余量不足、看门狗门控失败。

严重度建议：

| Severity | 含义 | BMS 反应 |
|----------|------|----------|
| `INFO` | 非安全信息 | 记录/上报 |
| `WARNING` | 可继续运行但需上报 | 限功率或提示 |
| `ERROR` | 不允许进入 NORMAL | 断开或保持 STANDBY |
| `CRITICAL` | 必须立即 OPEN/锁存 | FAULT/LOCKED |

## BMS/SYS 状态机

目标架构中，`bms_bms` 是电池系统主状态机，`bms_sys` 是系统服务/全局条件管理。接触器最终控制权归状态机，不归 protection。

### 主状态

```
INIT
  ↓
STANDBY  ←────────────┐
  ↓ close request      │ reset/recover
PRECHARGE              │
  ↓ success            │
NORMAL                 │
  ↓ fault/open request │
FAULT ─────→ LOCKED ───┘
```

| 状态 | 接触器 | 说明 |
|------|--------|------|
| `INIT` | OPEN | 初始化 database、诊断、驱动、任务框架 |
| `STANDBY` | OPEN | 安全待机，等待合法闭合命令 |
| `PRECHARGE` | 预充路径 | 执行预充，检查电压爬升与超时 |
| `NORMAL` | CLOSED | 允许充放电，持续监控故障 |
| `FAULT` | OPEN | 故障打开，等待诊断恢复或锁存 |
| `LOCKED` | OPEN | 锁存故障，仅显式维护/上层命令可复位 |

### 状态机输入

- `DB_DIAG_STATE`
- `DB_COMMAND`
- `DB_CONTACTOR_FB`
- `DB_CELL_MEAS`
- 硬件故障 latch
- 时间/超时条件

状态转移写成纯函数：

```c
enum bms_state bms_next_state(enum bms_state cur, const struct bms_state_inputs *in);
```

线程/任务只负责读取 database、调用纯函数、执行 entry/exit 动作、写回 database。

## 测量与保护

### 测量可信化

`bms_meas` 不是简单采样线程，而是测量可信化模块：

```
driver read → raw frame → validate/merge → DB_CELL_MEAS
```

每帧测量必须包含：

- 值：cell voltage、pack current、temperature 等。
- 时间戳：产生时刻。
- 有效性：电压/电流/温度/通信/冗余一致性。
- 序号：用于丢帧/重复帧判断。

### protection 角色

`bms_protection` 只做阈值判定，不直接驱动接触器：

```
DB_CELL_MEAS + limits → DB_PROT_STATE → bms_diag → bms_bms
```

硬件严重故障例外：zero-latency IRQ 可直接 OPEN，并置 latch；随后由 `bms_diag` 和 `bms_bms` 接管上报与锁存恢复。

## 通信架构

`bms_comm` 拆成 RX 与 TX：

- RX：解析 CAN/上位机命令，写 `DB_COMMAND`，不得直接改 BMS 状态。
- TX：从 database 取快照，按配置周期上报。
- CAN bus-off、超时、非法命令进入 `bms_diag`。
- 后续 CAN 报文建议表驱动，类似 foxBMS 2 对通信信号的配置化管理。

## 配置与硬件抽象

- 系统规模：`CONFIG_BMS_CELL_COUNT`、`CONFIG_BMS_TEMP_SENSOR_COUNT`。
- 任务周期：`CONFIG_BMS_TASK_FAST_MS`、`CONFIG_BMS_TASK_SAFETY_MS`、`CONFIG_BMS_TASK_APP_MS`。
- 诊断阈值：Kconfig 或 board/profile 配置。
- 硬件绑定：devicetree 描述 AFE、CAN、GPIO、接触器反馈、预充电阻控制。
- driver wrapper 暴露稳定接口，业务模块不直接依赖具体芯片。

## 当前实现到目标架构的迁移路径

| 阶段 | 目标 | 主要动作 |
|------|------|----------|
| M0 | 保留现状可构建 | 当前 `afe/soc/protection/balancing/comm` 继续工作 |
| M1 | 引入 `bms_db` | 把 zbus channel 映射为 database entry，模块先通过 DB 读写 |
| M2 | 引入 `bms_task` | 把各模块线程收敛到 1ms/10ms/100ms 任务框架 |
| M3 | 引入 `bms_diag` | protection/afe/comm/sys_mon 故障统一登记 |
| M4 | 引入 `bms_bms`/`bms_sys` | 接触器所有权迁移到主状态机 |
| M5 | 引入 `bms_sys_mon`/WDT | 任务运行时间监控与看门狗门控 |
| M6 | 硬件安全闭环 | 接触器 GPIO、反馈、预充、zero-latency IRQ、NVM 故障记录 |

迁移期间允许 zbus 与 database 共存，但目标架构契约以 database 为准。

## 参考

- foxBMS 2 Software Architecture: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/gen2/docs/html/v1.8.0/software/architecture/architecture.html
- foxBMS 2 Operating System Configuration: https://docs.foxbms.org/software/structure/operating-system-configuration.html
- foxBMS 2 Software Modules: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/modules.html
- foxBMS 2 Database Module: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/engine/database/database.html
- foxBMS 2 FTASK Module: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/task/ftask/ftask.html
- foxBMS 2 System Monitoring Module: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/engine/sys_mon/sys_mon.html
- foxBMS 2 BMS Module: https://iisb-foxbms.iisb.fraunhofer.de/foxbms/docs/latest/software/modules/application/bms/bms.html
