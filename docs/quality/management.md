# 质量管理速览

本文只保留质量体系的**判断框架和证据地图**。CI job、阈值、构建矩阵等易变事实以 [gates.md](gates.md) 为准；需求追溯以 [work/traceability.md](../work/traceability.md) 为准。

## 1. 质量目标

| 目标 | 判断标准 |
|---|---|
| 正确性 | 需求有实现、有验证、有边界条件证据 |
| 失效安全 | 无效、过期、诊断错误、任务失活时进入安全态 |
| 可追溯 | 需求能追到设计、代码、测试和结果 |
| 可维护 | 模块边界、数据 owner、任务 owner、参数来源清楚 |
| 可复现 | 构建、测试、版本和发布产物可回溯 |
| 可移植 | 业务逻辑可在 native_sim/QEMU/目标板之间复用 |

## 2. 核心原则

1. 安全红线优先于功能完成度。
2. 需求必须有验收方式；安全需求优先自动化测试。
3. 架构约束要阻止错误依赖，而不是只靠约定。
4. 工具守底线，评审看意图。
5. 仿真能左移问题，但不能替代真板/HIL 证据。
6. 文档不重复维护可变事实。

## 3. 证据地图

| 问题 | 查哪里 |
|---|---|
| 当前 CI 拦什么 | [gates.md](gates.md) |
| 怎么开发和评审一个特性 | [workflow.md](../process/workflow.md) |
| 某条需求有没有测试 | [work/traceability.md](../work/traceability.md) 和 `tests/` |
| 架构边界是什么 | [architecture.md](../concept/architecture.md) |
| 安全目标和失效安全原则 | [safety.md](../concept/safety.md) |
| 任务、watchdog、sys_mon | [runtime-model.md](../concept/runtime-model.md) |
| 诊断 severity/latch/clear | [diagnostics-fault-model.md](../concept/diagnostics-fault-model.md) |
| 参数、标定、阈值治理 | [configuration-calibration.md](../concept/configuration-calibration.md) |
| C 代码应该怎么写 | [coding-style.md](../standard/coding-style.md) |
| 模块接口规则 | [module-interface.md](../standard/module-interface.md) |
| 特性过程证据 | `docs/work/features/<slug>/` |
| 发布产物证据 | `release.yml`、Release artifact、`VERSION`、固件版本日志 |

## 4. 质量控制点

| 环节 | 必看点 |
|---|---|
| 需求 | 是否可验收，是否标出安全需求和非目标 |
| 架构 | 是否保持分层、owner、硬件抽象和诊断中心化 |
| 设计 | 是否覆盖状态、边界、失败路径和测试点 |
| 编码 | 是否符合布局、接口、风格和无告警要求 |
| 测试 | 是否覆盖正常、边界、异常、stale、timeout |
| CI | 是否通过阻断门，跳过项是否有原因 |
| 发布 | 版本、制品、校验和、变更记录是否一致 |
| 硬件 | 真板/HIL 是否覆盖真实 IO、时序、复位和保护响应 |

## 5. 自动化门禁分类

| 类型 | 用途 | 处置 |
|---|---|---|
| 阻断门 | 失败不得合并或发布 | 修复、正式 suppression/deviation，或撤回变更 |
| 预检门 | 本地提前暴露风险 | 用于缩短反馈，不能替代 CI |
| 流程门 | DoR/DoD、追溯、评审、安全说明 | PR 中显式回答，逐步自动化 |

## 6. 当前不能证明的事

这些是风险边界，不是 CI 失败项：

- 仿真和 build-only 不能证明真实 AFE、MOS/接触器、RS485、ADC、NVM 行为。
- 当前逻辑测试不能证明最终保护响应时间。
- 参数/NVM 损坏后的回退策略仍需随真实存储验证。
- Release artifact 签名不等于设备端启动验签和 OTA 回滚闭环。
- 单人自审不能等价于独立评审。

## 7. PR 评审最小清单

- 是否影响安全链：测量、保护、诊断、BMS 状态机、接触器、watchdog、参数。
- 是否有对应需求或架构契约。
- 是否更新追溯矩阵。
- 是否覆盖失败路径，或明确记录风险接受。
- 是否保持 owner 边界，没有绕过 DB、DIAG、HAL wrapper。
- 是否有构建、测试、CI 或硬件证据。

## 8. 维护规则

- CI 事实变更先改 [gates.md](gates.md)，本文只在原则或证据位置变化时更新。
- 新增安全目标或架构契约时，同步检查 `safety.md`、`architecture.md`、`traceability.md`。
- 风险项的执行状态放 [TODO.md](../../TODO.md) 或特性证据包，本文只保留风险类别。
