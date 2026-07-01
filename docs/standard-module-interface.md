# 模块接口标准

> **文档定位**：本文定义 BMS 业务模块、engine 模块、硬件抽象模块的接口契约。目标是让代码逐步靠拢 [concept-architecture.md](concept-architecture.md) 的 foxBMS 2 inspired 架构，并避免模块重新退回“互相直接调用/各自开线程”的旧形态。

## 1. 模块分类

| 分类 | 典型模块 | 职责 |
|------|----------|------|
| Engine | `bms_task`、`bms_db`、`bms_diag`、`bms_sys_mon` | 调度、数据交换、诊断、健康监控 |
| Application | `bms_bms`、`bms_algorithm`、`bms_comm`、`bms_balancing` | 主状态机、算法、通信、均衡策略 |
| Measurement/Control | `bms_meas`、`bms_protection`、`bms_contactor` | 测量可信化、保护判定、接触器/预充执行 |
| Hardware Abstraction | `bms_hw_*`、Zephyr driver wrapper | 设备访问、devicetree/Kconfig 适配 |

## 2. 统一接口形态

每个模块优先提供三类接口：

| 接口类型 | 命名 | 说明 |
|----------|------|------|
| 初始化 | `bms_<module>_init()` | 初始化内部状态、静态配置、日志；不得启动长期私有线程 |
| 周期/任务入口 | `bms_<module>_<period>()` 或由 `bms_task` 内调用 | 由任务框架统一调度，模块不得自行无限循环 |
| 纯函数核心 | `bms_<module>_<verb>()` | 无硬件/线程依赖，供 ztest 直接覆盖 |

示例：

```c
int bms_afe_sample(struct bms_cell_meas *out);
int bms_protection_evaluate(const struct bms_cell_meas *meas,
			    const struct bms_prot_limits *limits,
			    struct bms_prot_evt *out);
enum bms_state bms_next_state(enum bms_state cur,
			      const struct bms_state_inputs *in);
```

## 3. 任务所有权

- 长期运行线程只允许由 `bms_task` 或明确登记的 engine 模块创建。
- 业务模块不得使用 `K_THREAD_DEFINE` 自启动后台循环。
- 周期、优先级、栈、最大运行时间、心跳超时必须能在任务表或等价位置集中审计。
- 阻塞操作必须进入 blocking task 或专用数据平面，不得放入 safety cyclic task。

## 4. 数据所有权

目标架构以 `bms_db` 为数据交换中心。

| 数据 | 唯一写入者 | 主要读取者 |
|------|------------|------------|
| `DB_CELL_MEAS` | `bms_meas` / 当前过渡期 `bms_task` 调用 `bms_afe_sample` 后写入 | protection、algorithm、balancing、comm |
| `DB_SOC_STATE` | `bms_algorithm` / 当前 `bms_task` 调用 SOC 核心后写入 | bms、comm、diag |
| `DB_PROT_STATE` | `bms_protection` / 当前 `bms_task` 调用 protection 核心后写入 | diag、bms、comm |
| `DB_DIAG_STATE` | `bms_diag` | bms、sys、comm |
| `DB_BMS_STATE` | `bms_bms` / 当前 `bms_task` 调用 `bms_next_state` 后写入 | comm、balancing、sys_mon |

规则：

- 一个 database entry 只能有一个 owner。
- 读者不得缓存可变指针；只能读取值拷贝。
- 同一采样周期内一致的数据必须一次性写入同一个 entry。
- zbus 仅作为兼容/通知层，不作为新的模块契约。

## 5. 错误与诊断

- 模块函数返回 `0` 表示成功，负值使用 `-errno`。
- 安全相关故障不得只打日志，必须进入 `bms_diag`。
- `LOG_*` 是观测手段，不是状态机输入。
- 诊断 ID、严重度、锁存策略应由 `standard-diagnostics.md` 后续统一定义；在该文件完成前，以 `bms/diag.h` 为临时权威。

## 6. 安全默认态

- 接触器默认 `OPEN`。
- `bms_bms`/`bms_sys` 是接触器最终 owner。
- `bms_protection` 只做保护判定，不直接闭合接触器。
- 测量无效、诊断 ERROR/CRITICAL、任务健康异常、硬件故障 latch 均不得允许进入 `NORMAL`。

## 7. Zephyr 使用边界

允许业务模块使用：

- `zephyr/logging/log.h` 记录日志。
- `zephyr/kernel.h` 的轻量时间读取/基础类型，前提是不制造私有调度模型。

不允许业务模块直接使用：

- STM32 HAL/寄存器。
- 未经 wrapper 的 GPIO/CAN/ADC/WDT/flash 设备访问。
- 未登记的长期线程、无限循环、`K_FOREVER` 阻塞。

## 8. 新模块准入清单

新增模块前必须回答：

- 属于 Engine/Application/Measurement/Control/HW 哪一层？
- 是否需要 database entry？entry owner 是谁？
- 是否需要诊断 ID？严重度与锁存策略是什么？
- 由哪个 task 调用？周期、优先级和最大运行时间是什么？
- 哪些核心逻辑可以做成纯函数并单测？
- 安全默认态是什么？异常输入如何降级？
- 对应 `REQ-*`、`DES-*`、测试用例如何追溯？
