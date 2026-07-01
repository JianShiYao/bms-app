# BMS 硬件抽象模型 v0（设计契约）

> **定位**：本文细化 [architecture.md](architecture.md) 的 ADR-ARCH-006（硬件边界），是硬件抽象层（HAL wrapper）的**权威设计契约**——规定 AFE/CAN/GPIO/ADC/NVM/WDT/硬件时间等 wrapper 的**边界、接口契约、错误与失效安全、ISR/zero-latency 边界、devicetree/Kconfig 绑定与仿真桩化**。**agent 据此实现或重构 wrapper 与其消费方，代码向本契约对齐**；本文不描述现状实现。
>
> **现状与差距**不在此维护：见 [architecture.md](architecture.md) §11 迁移路径（本契约在"**真板 bring-up 前**"落地——当前以 `afe_sim`/`native_sim`/QEMU 为主，真实外设 wrapper 随 `bms_f405` bring-up 增量实现，M6 收口）。
>
> **规范措辞**：**必须 / 应 / 不得** 表示契约要求。相关：架构 [architecture.md](architecture.md) §3·§9·§10，数据契约 [data-model.md](data-model.md)（wrapper 出原始、bms_meas 可信化后入 DB），运行时 [runtime-model.md](runtime-model.md)（ISR/zero-latency、watchdog），诊断 [diagnostics-fault-model.md](diagnostics-fault-model.md)（外设错误入诊断），配置 [configuration-calibration.md](configuration-calibration.md)（板级绑定层），接口 [../standard/module-interface.md](../standard/module-interface.md)。

## 1. 硬件抽象设计原则

- **分层铁律**：业务模块（application/measurement/control）**不得**直接依赖 STM32 HAL、CMSIS 或寄存器，**不得**直接调用未经 wrapper 的 Zephyr driver（GPIO/CAN/ADC/WDT/flash）。硬件访问必须经 **wrapper + devicetree + Kconfig**（ADR-ARCH-006）。
- **wrapper 隐藏差异**：wrapper 暴露**稳定、与芯片无关**的接口，隐藏具体芯片、devicetree 节点与驱动差异；更换 MCU/板卡**应**只改 wrapper 与 dts，不改业务逻辑。
- **单向依赖**：上层调下层 wrapper 公开接口；wrapper **不得**反向依赖业务状态机、保护策略或诊断策略（Driver/HAL 层不含业务）。
- **wrapper 只做搬运，不做判定**：wrapper 返回**原始数据 + 错误码**；有效性、可信化、安全判定在其上层（`bms_meas`/`bms_protection`/`bms_bms`）完成。wrapper **不得**自行决定系统安全或直接跑业务状态机（zero-latency 最小安全动作例外，见 §6）。
- **失效安全**：外设错误/超时/缺失时，wrapper **必须**以可被上层观测的方式报错，**不得**静默吞错或返回未初始化/随机值当作有效数据。

## 2. wrapper 集与边界（契约）

每类外设一个 wrapper，边界如下（具体命名/文件在实现时定，遵循 [../standard/module-interface.md](../standard/module-interface.md)）：

| wrapper | 职责（搬运） | 上层消费者 | 里程碑 |
|---------|--------------|-----------|--------|
| AFE | 单体电压/温度/电流原始帧读取、AFE 硬件保护标志(OCD/短路)、ALERT 中断 | `bms_meas` | M6（现 `afe_sim`） |
| CAN | 帧收发、bus-off/错误状态 | `bms_comm` | M6 |
| GPIO/contactor | 接触器/预充输出、反馈电平读取 | `bms_contactor` | M6 |
| ADC（辅助） | 绝缘/进水/板载辅助模拟量 | `bms_meas` | M6 |
| NVM/flash | 标定参数与故障/日志持久化 | 参数(§configuration-calibration)、diag | M6 |
| WDT | 硬件看门狗喂狗/配置 | `bms_sys_mon`（[runtime-model.md](runtime-model.md) §7） | M5/M6 |
| 硬件时间源 | 单调计时后端 | `bms_time`（[runtime-model.md](runtime-model.md) §2） | M2/M6 |

## 3. wrapper 接口契约（通用）

- **稳定接口**：每个 wrapper 提供 `init` + 读/写/收发接口 + 明确**错误返回**；接口签名以物理语义为准（如电压 mV、电流 mA），**不得**在签名里泄露 devicetree 节点名、寄存器或芯片型号。
- **错误统一**：wrapper 以统一错误码/返回值表达"设备未就绪/超时/通信失败/校验失败"，供上层转诊断（[diagnostics-fault-model.md](diagnostics-fault-model.md) §9）。
- **不阻塞 safety**：safety cyclic 路径调用的 wrapper 接口**不得**阻塞；需要阻塞的硬件操作（NVM 写、同步长传输）只能由 blocking task 调用（[runtime-model.md](runtime-model.md) §5）。
- **初始化次序**：wrapper 由 engine 初始化协调（`bms_sys`）统一 `init`；未初始化前，其上层**必须**按失效安全处理（不据此进入 NORMAL）。

## 4. 数据流边界（契约）

```
driver read → wrapper（原始帧 + 错误码） → bms_meas 可信化（validity/timestamp/sequence） → DB_CELL_MEAS
```

- wrapper **只出原始帧与错误**，**不得**打有效位、不得判 stale、不得合并冗余——这些属 `bms_meas`（[data-model.md](data-model.md)、[architecture.md](architecture.md) §9）。
- 每帧原始数据经 `bms_meas` 加上时间戳（[runtime-model.md](runtime-model.md) §2 `bms_time`）、有效位、序号后入 DB；**无效/过期原始数据不得被上层当作 NORMAL 依据**。
- 输出侧（contactor/CAN TX）：`bms_bms`/`bms_comm` 决定意图，wrapper 只执行并回读反馈（`DB_CONTACTOR_FB`）。

## 5. ISR / zero-latency 边界（契约）

- 硬件严重故障（短路比较器、AFE ALERT）可经 wrapper 注册的 **zero-latency ISR** 触发；ISR **只做最小安全动作**（经 GPIO wrapper 强制接触器 OPEN）+ 置 **latch/event**，随后由 `bms_diag`/`bms_bms` 接管上报与恢复（[runtime-model.md](runtime-model.md) §3、[diagnostics-fault-model.md](diagnostics-fault-model.md) §10）。
- ISR **不得**直接写 `bms_db`、**不得**跑业务状态机、**不得**调用可能阻塞的接口（[data-model.md](data-model.md)）。
- 此类硬件 latch 的清除必须同时满足"硬件 latch 已解除"与诊断授权（[diagnostics-fault-model.md](diagnostics-fault-model.md) §8）。

## 6. 错误与失效安全（契约）

- wrapper 报出的错误（设备缺失/超时/CAN bus-off/校验失败/NVM 损坏）**必须**由消费模块转为 `bms_diag` 原始判定（[diagnostics-fault-model.md](diagnostics-fault-model.md) §9），**不得**只打日志。
- 硬件不可用时**必须**回到安全态：接触器默认 OPEN；无有效采样时保护不据此闭合、SOC 不积分（[safety.md](safety.md)）。
- 输出与反馈**必须**闭环校验：接触器期望态与 `DB_CONTACTOR_FB` 不一致（粘连/开路）须触发诊断。

## 7. devicetree / Kconfig 绑定（契约）

- 板级资源（AFE 总线、CAN、GPIO 引脚、接触器反馈、预充、WDT、flash 分区）经 **devicetree** 描述；启用/裁剪经 **Kconfig**（[configuration-calibration.md](configuration-calibration.md) §2 板级绑定层）。
- **只有 wrapper 读 devicetree**；业务模块**不得**直接引用 dt 节点或 `DT_*` 宏。
- 更换板卡时，新增/修改的应是 dts + defconfig + wrapper 绑定，业务逻辑与设计契约不变。

## 8. 仿真与真板（契约）

- `native_sim`/QEMU：AFE 用 `afe_sim` 后端；WDT、NVM、GPIO 等按需**桩化或关闭**；桩实现**不得**放宽安全默认（如不得让"无硬件"等价于"允许闭合"）、**不得**跳过错误路径。
- 真板（`bms_f405` 等）：按 §2 逐个实现真实 wrapper，经 dts 绑定，M6 完成硬件安全闭环（接触器 GPIO + 反馈 + 预充 + 硬件 ALERT + NVM 故障记录）。
- 同一业务逻辑**必须**在仿真与真板下不改代码即可运行（后端可切换，[architecture.md](architecture.md) §9「数据源后端可切换」）。

## 9. 可测性约束

- 业务逻辑对 **wrapper 接口**编程，wrapper 可被 **fake/mock** 替换；核心判定（可信化、保护、状态机）以注入的 fake wrapper + 注入数据脱离真实硬件单测。
- wrapper 自身的最小逻辑（错误映射、单位换算）应可在 `native_sim` 下以桩后端测试；真实寄存器/时序验证留 M6 真机/HIL。

## 10. 迁移

本契约落地阶段见 [architecture.md](architecture.md) §11：**M6 真机安全闭环**（接触器 GPIO、反馈、预充、硬件 ALERT、NVM 故障记录验证）；`bms_time` 硬件后端在 M2、WDT wrapper 在 M5/M6。迁移期允许 `afe_sim`/桩与真实 wrapper 共存，但**目标以本契约为准**；每步保持既有 CI 与 ztest 通过。

## 11. 参考

- [architecture.md](architecture.md) §3（分层）、§9（测量/边界）、§10（ADR-ARCH-006/007）、§11。
- [runtime-model.md](runtime-model.md) §2（时间基准）、§3（zero-latency）、§7（watchdog）。
- [data-model.md](data-model.md)（原始 → 可信化 → DB）、[diagnostics-fault-model.md](diagnostics-fault-model.md) §9·§10（外设错误/硬件 latch）。
- [configuration-calibration.md](configuration-calibration.md) §2（板级绑定层）、[safety.md](safety.md)（失效安全）。
- foxBMS 2 Hardware/Driver 层（链接见 [architecture.md](architecture.md) §13）。
