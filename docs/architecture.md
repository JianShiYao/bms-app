# BMS 固件架构

## 分层

```
┌───────────────────────────────────────────────┐
│ 应用层  app/src/main.c                          │  初始化 + 编排 + 健康监测
├───────────────────────────────────────────────┤
│ BMS 服务层  app/src/bms/*                        │
│   afe  soc  protection  balancing  comm          │  各自独立线程，互不直接调用
├───────────────────────────────────────────────┤
│ HAL/驱动层                                       │
│   Zephyr 内建：ADC / CAN / GPIO                  │
│   out-of-tree：AFE 芯片驱动（drivers/，占位）     │
└───────────────────────────────────────────────┘
```

各模块通过 **zbus 消息总线**通信，互相之间没有编译期依赖，可单独裁剪（Kconfig `BMS_<MODULE>`）与单独测试。

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

## 模块职责

- **afe（电芯采样）**：周期采集每串电压、总电流、温度，发布 `chan_cell_meas`。native_sim 下用桩数据；真实硬件接 ADC 或专用 AFE 芯片（驱动留 `drivers/`）。
- **soc（SOC/SOH 估算）**：订阅测量值，估算荷电/健康状态，发布 `chan_soc`。算法接口预留（库仑积分/卡尔曼，桩实现）。
- **protection（保护状态机）**：订阅测量值+SOC，运行过压/欠压/过流/过温状态机，**失效安全：默认断开接触器**，发布 `chan_prot_state`。
- **balancing（均衡）**：订阅测量值，计算需均衡的单体（被动/主动接口预留）。
- **comm（CAN 通信）**：订阅各 channel，对外 CAN 上报；接收外部命令。native_sim 下无真实 CAN，走日志桩。

## 失效安全原则

- 保护模块上电默认进入安全态（接触器断开），只有所有条件正常才闭合。
- 任一关键线程异常应被看门狗（后续接入）捕获并进入安全态。

## 线程模型

每个模块用 `K_THREAD_DEFINE` 自启动独立线程；优先级：protection > afe > soc/balancing > comm。main 仅做初始化与健康打印。
