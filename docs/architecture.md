# BMS 固件架构

> **文档状态约定**：本文档混合「当前实现」与「目标架构」。
> 📐 标记的小节为**设计原则（演进方向）**，其中引用的 `sys`/`diag`/`chan_diag` 等模块**尚未实现**；未标记的小节描述**当前实现（骨架）**，节内 `🎯 目标` 注指出该节相对目标架构将如何演进。
> **接触器所有权正在演进**：当前由 `protection` 驱动，目标迁移至 `sys`（`protection` 退化为纯判定器）。

## 分层

```
┌──────────────────────────────────────────────────────────┐
│ 应用层  app/src/main.c                                      │  初始化 + 编排 + 健康监测
├──────────────────────────────────────────────────────────┤
│ BMS 服务层  app/src/bms/*                                   │
│   afe  soc  protection  balancing  comm                     │  各自独立线程，互不直接调用
├──────────────────────────────────────────────────────────┤
│ 设备抽象层（芯片无关，业务层只依赖这一层）                    │
│   Zephyr 设备 API：<zephyr/drivers/{adc,can,gpio}.h>         │  统一函数 + devicetree 设备模型
├──────────────────────────────────────────────────────────┤
│ 驱动实现层                                                  │
│   Zephyr 内建：adc_stm32 / can_stm32_bxcan / gpio_stm32      │
│   out-of-tree：专用 AFE 芯片驱动（drivers/，占位）           │
├──────────────────────────────────────────────────────────┤
│ 厂商 HAL / 寄存器（业务层不直接调用）                        │
│   STM32Cube（modules/hal/stm32）/ CMSIS                      │
└──────────────────────────────────────────────────────────┘
```

**分层铁律**：BMS 服务层只 `#include <zephyr/drivers/*.h>`（设备抽象层），**绝不**直接调用 STM32 HAL/寄存器。硬件差异下沉到 **devicetree**（引脚、通道、波特率），换芯片只改驱动实现层 + dts，业务层零改动——这也是同一份业务能在 `mps2/an386`、`native_sim`、真机 `bms_f405` 三处复用的前提。

各模块通过 **zbus 消息总线**通信，互相之间没有编译期依赖，可单独裁剪（Kconfig `BMS_<MODULE>`）与单独测试。

### 数据源后端可切换（afe）

afe 的「采样实现」与「业务逻辑」分离，按 board/Kconfig 选后端，业务层不变：

| 后端 | 适用 | 数据来源 |
|------|------|----------|
| `afe_sim`（桩/仿真） | QEMU、native_sim | 桩数据或充放电模型，可复现，供单测注入 |
| `afe_adc`（真实） | `bms_f405` 真机 | `DEVICE_DT_GET` 取 ADC/专用 AFE 芯片，走设备 API |

> native_sim 还可用 Zephyr emul 外设（`adc_emul` / `gpio_emul` / 总线 EMUL）让真实驱动路径在 PC 上跑测试。

## 数据流（zbus channels）

```
            ┌──────────┐  chan_cell_meas   ┌──────────┐
   ADC/AFE →│   afe    │──────────┬───────→│   soc    │─┐
            └──────────┘          │        └──────────┘ │ chan_soc
                                  │                      ↓
                                  │        ┌──────────────────┐  控制接触器/MOS (GPIO)
                                  ├───────→│   protection     │──────────────────────→
                                  │        └──────────────────┘  chan_prot_state
                                  │                      │
                                  │        ┌──────────┐  │
                                  ├───────→│balancing │  │ 计算均衡位
                                  │        └──────────┘  │
                                  │        ┌──────────┐  │
                                  └───────→│  comm    │←─┘  CAN 上报/收命令
                                           └──────────┘
```

| Channel            | 发布者      | 订阅者                          | 负载类型              |
|--------------------|-------------|---------------------------------|-----------------------|
| `chan_cell_meas`   | afe         | soc, protection, balancing, comm| `struct bms_cell_meas`|
| `chan_soc`         | soc         | protection, comm                | `struct bms_soc`      |
| `chan_prot_state`  | protection  | comm                            | `struct bms_prot_evt` |

> 🎯 目标：删除 `chan_prot_state`，新增 `chan_prot_eval`（protection→diag）、`chan_diag`（diag→sys/comm）、`chan_sys_state`（sys→comm）；afe 测量增 `timestamp`/`validity` 字段。

## 控制平面 / 数据平面分界

> 📐 设计原则。

zbus 是**控制平面（control-plane）**总线：承载「谁需要知道什么变了 / 当前状态是什么」——事件、状态、结构化快照、命令。它采用**值拷贝语义**（`zbus_chan_pub`/`read` 各做一次 memcpy，且在 channel 信号量保护下完成），适合**小而结构化**的消息，**不承载大块 / 流式数据**。

**设计铁律**：控制走 zbus；批量字节走专用**数据平面**原语，后者只把「摘要 / 句柄」用 zbus 小事件广播出来。

| 平面 | 典型内容 | 机制 |
|------|----------|------|
| 控制平面（zbus） | 测量快照、SOC、diag 故障态、sys 状态、命令 | `zbus_chan_pub/read` + observer |
| 数据平面（非 zbus） | ADC 原始波形/录波、跨节点帧缓冲、黑匣子/OTA 固件块 | `ring_buf`+DMA、`k_mem_slab`/`net_buf` 池、flash/文件系统 |

- **尺度校准**：BMS 的结构化测量快照（16 串 ≈ 几十字节，400 串大簇全量 ≈ 1~2KB）在 F405（168MHz）上拷贝是 µs 级、而发布周期 10~100ms，**对 zbus 不算大**，照走 zbus。真正不该入 zbus 的是 KB~MB、kHz 级的原始流。
- **消化模式**：DMA/驱动灌进 `ring_buf` 或缓冲池 → 处理线程消化 → 仅把摘要/句柄经 zbus 发出。例：afe 消化原始采样后只发 `chan_cell_meas`；comm 解码现场总线帧后只发结构化结果。
- **必须经 zbus 传大块时**：消息内只放 `net_buf`/池句柄（传引用，不传负载），大缓冲存共享池，所有权/生命周期自行管理。
- **拷贝成本提醒**：`msg_subscriber` 在发布时**每个订阅者各拷一份**进 net_buf，大消息 + 多订阅者最吃 RAM；listener 锁内零拷贝访问但在发布者上下文同步执行。

> 跨节点同步（多簇 → 主控、对 EMS/PCS）属**通信协议**问题，由 comm 层经 CAN/RS485/以太网收发，再桥接进各自节点的 zbus——zbus 不跨 MCU。

## 状态机与模块间协调

> 📐 设计原则（引用的 `sys`/`diag` 为目标模块，尚未实现）。

两类「状态」问题用两种**正交**机制，不要混用——**消息总线不是状态机引擎**：

| 问题 | 机制 | 说明 |
|------|------|------|
| 模块**内**状态机（FSM 写法） | **Zephyr SMF**（`<zephyr/smf.h>`） | 层次状态 + `entry/run/exit`，公共逻辑提到 parent；替代手写 `switch(state/substate)` + 手动 timer 计数。线程循环跑 `smf_run_state()`，转移用 `smf_set_state()` |
| 模块**间**状态转移（耦合/传输） | **zbus** | 各模块发布自身状态（单一发布者），关心者订阅、事件驱动反应；替代「轮询共享状态 + 调 request 函数」式耦合 |

**安全状态机铁律**：

- **事件唤醒 + 电平驱动**：zbus 事件只负责*唤醒*状态机；醒来后**读当前所有权威输入快照**（最新 diag/接触器反馈/命令）重新计算应处状态，**绝不依赖「看全每一次变化」**。原因：zbus channel 只存最新值，普通 subscriber 可能漏掉中间状态。这同时保留了轮询式状态机的幂等/鲁棒优点。
- **唯一权威 owner**：安全相关状态机集中在**一个模块（sys）**计算决策，不让整机行为「涌现」自多模块互相对发消息——分布式涌现的状态极难验证/测试。zbus 只负责把输入汇入、把决定播出，决策始终在一处。
- **纯函数判定**：转移判定写成纯函数 `bms_sys_next(cur, inputs) → 期望状态` 供 ztest 直测，SMF 仅做编排骨架（延续「纯函数 + 薄线程」约定）。

典型范式（以 sys 为例）：

```
zbus 事件 ─唤醒→ 读最新快照(电平驱动) → 纯函数判定下一状态(可测) → SMF 执行 entry/exit → 发布 chan_sys_state + 驱动接触器
```

> 注：路径 B 的零延迟硬故障锁存优先于一切软件状态机；SMF 状态机在 `g_hw_fault_latched` 为真时绝不闭合接触器（见「保护：双路径 + 失效安全」）。

## 测量数据纪律（值 + 时间戳 + 有效性）

> 📐 设计原则（`timestamp`/`validity` 字段尚未加入 `bms_cell_meas`）。

借鉴 foxBMS database 的 header 纪律与冗余测量（MRC）范式——**不照搬其中央 broker，只采纳数据纪律**。afe 的职责由「采个值发出去」升级为「产出**可信**测量」。

**铁律**：

- **每个测量 = 值 + `timestamp` + `validity`**：核心结构（`bms_cell_meas` 等）统一带「产生时刻」与「有效位」，不只是数值。下游据此做过期检测；本地发布新鲜度可另用 `zbus_chan_pub_stats_msg_age()`（需 `CONFIG_ZBUS_CHANNEL_PUBLISH_STATS`）。
- **一致的数据一起原子发布**：必须保持一致的量**打包进同一个 channel/struct 一次发出**（如同一采样周期的 cell 电压 + 电流 + 时间戳进同一 `chan_cell_meas`），不拆成多 channel 再重新对齐——跨 channel 读非原子（与 foxBMS 跨数据块读非原子同理）。
- **校验与采集分离**：三段解耦，校验段为纯函数：
  ```
  acquire(原始, 可能多源) → validate/merge(合并 + 合理性, 纯函数) → 发 chan_cell_meas{值, 时间戳, validity}
  ```
- **冗余要多样性**：用**不同手段**测同一物理量（单体电压累加和 vs 直测总压、不同量程电流传感器）才能抓**系统性**故障；纯重复同一传感器无效。
- **下游尊重 validity（失效安全）**：validity 无效时下游**绝不照用**——电流无效则 SOC 不积分、保护不据此闭合接触器；越界 / 不一致 / 过期一律上报诊断（diag）。

冗余 / 合理性校验是**纯函数阶段**而非独立线程，由数据源「边缘」调用——契合「数据源后端可切换（afe）」：真机后端（`afe_adc`，直测多源）或仿真后端各自在采集后调同一 `validate/merge` 函数，业务层只看到已校验的可信值。

> 不引入 foxBMS 的中央 database/broker：zbus 的 **channel 列表本身就是「系统数据目录」**，per-channel 信号量比单一 broker 任务更细粒度、更低延迟。

## 模块职责

> 🎯 目标：新增 `sys`（整机状态机，独占接触器）与 `diag`（集中式诊断登记表）；`protection` 退化为纯阈值判定器（只把违例发给 diag，不再驱动接触器）。下列为当前 5 模块实现。

- **afe（电芯采样）**：周期采集每串电压、总电流、温度，发布 `chan_cell_meas`。采样实现按后端切换（`afe_sim` 仿真 / `afe_adc` 真机，见上「数据源后端可切换」），业务逻辑共用。
- **soc（SOC/SOH 估算）**：订阅测量值，估算荷电/健康状态，发布 `chan_soc`。算法接口预留（库仑积分/卡尔曼，桩实现）。
- **protection（保护状态机）**：订阅测量值+SOC，运行过压/欠压/过流/过温状态机，**失效安全：默认断开接触器**，发布 `chan_prot_state`。
- **balancing（均衡）**：订阅测量值，计算需均衡的单体（被动/主动接口预留）。
- **comm（CAN 通信）**：订阅各 channel，对外 CAN 上报；接收外部命令。native_sim 下无真实 CAN，走日志桩。

## 保护：双路径 + 失效安全

> 🎯 目标：路径 A 的接触器闭合/断开所有权迁至 `sys` 状态机，`prot_thread` 仅做判定并上报 diag；路径 B（零延迟 IRQ 硬锁存）不变，仍直接 OPEN。下文描述当前实现。

保护按故障严重度分两条响应路径，**慢路径可测试、快路径有确定性**：

```
            过压/欠压/过温、常规过流
路径 A ─┐   afe采样 → zbus → prot_thread(prio 4)判定 → 断接触器GPIO + 发 chan_prot_state
（软实时）   延迟 ≈ 采样周期 + 调度(µs级)；逻辑在纯函数 bms_protection_evaluate()，可单测

            严重短路 / 严重过流（catastrophic）
路径 B ─┘   硬件报警(AFE ALERT引脚/分流比较器) → GPIO中断(zero-latency IRQ) → ISR直接断接触器
（硬实时）   延迟 ≈ 中断延迟(亚µs级)，不经调度器、不被 irq_lock 屏蔽
```

**路径 B（zero-latency IRQ）设计约束**——这是它能"确定性"的代价：

- `CONFIG_ZERO_LATENCY_IRQS=y`；中断用 `IRQ_CONNECT(irq, prio, isr, arg, IRQ_ZERO_LATENCY)` 注册，被放到内核保留的最高优先级（priority 0，在 BASEPRI 屏蔽线之上），**`irq_lock()`/内核临界区无法屏蔽它**。
- 因可在内核临界区中途抢入，**ISR 内禁止调用任何内核 API**（无 `k_*`、无 zbus、无 LOG）。ISR 只允许：① 直接写接触器 GPIO 到 OPEN（失效安全，优先用最轻的端口寄存器写，确认驱动 ISR 安全后再调 `gpio_pin_set_dt`）；② 置一个 `volatile` 锁存标志 `g_hw_fault_latched`。
- **两路协同**：路径 B 只会强制 OPEN；锁存后，路径 A 的 `prot_thread` 必须遵守该 latch——`g_hw_fault_latched` 为真时**绝不重新闭合**接触器，并补发 `chan_prot_state`（`state=BMS_PROT_FAULT, contactor=OPEN`）让 comm 上报、写日志。复位需显式 re-arm（人工/上层命令）。

**失效安全总则**：

- 上电默认安全态（接触器断开），仅所有条件正常才闭合；硬件锁存故障优先于一切软件判定。
- 关键线程异常由看门狗（后续接入）捕获并强制进入安全态。

## 线程模型

每个模块用 `K_THREAD_DEFINE` 自启动独立线程；优先级 protection(4) > afe(6) > soc/balancing(7) > comm(8)，确保安全决策先于上报（🎯 目标加入 sys/diag 后为 sys(3) > protection(4) > diag(5) > afe(6) > soc/balancing(7) > comm(8)）。main 仅做初始化与健康打印。路径 B 的 ISR 在线程之上，不参与线程优先级排序。

## 实时性约定

- **tick 粒度**：`mps2/an386` 默认 `SYS_CLOCK_TICKS_PER_SEC=100`（10ms，限制 `k_msleep/k_timer` 超时精度）。真机 `bms_f405` 应提到 **1000（1ms）或 10000（100µs）**，否则保护去抖/采样分辨率受限；`CONFIG_TICKLESS_KERNEL=y` 下提高 tick 几乎无额外开销。
- **实时指标必须在真机实测**：QEMU 非周期精确（且跑在 25MHz，真机 168MHz），中断延迟/上下文切换须在 STM32F405 上用 DWT 周期计数器测量；栈峰值用 `CONFIG_THREAD_ANALYZER`。
- **资源基线**（mps2 当前固件实测）：Flash ≈ 27KB、RAM ≈ 14KB（其中线程栈占 ~11.6KB），相对 F405 的 1MB/192KB 余量充足；RAM 增长主要来自新增线程栈。
