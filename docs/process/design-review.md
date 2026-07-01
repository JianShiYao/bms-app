# 设计评审流程

> **文档定位**：本文补齐设计层的评审门槛，服务于 [workflow.md](workflow.md) 的小 V 流程与 [methodology.md](../concept/methodology.md) 的“变更即再基线 / 安全案例证据”原则。

## 1. 什么时候必须评审

下列改动必须做设计评审，不能只靠编码自检：

- 架构层：`bms_task`、`bms_db`、`bms_diag`、`bms_bms`、`bms_sys` 的职责或接口变化。
- 安全层：保护阈值、接触器控制、预充、故障锁存、watchdog、zero-latency IRQ。
- 数据层：database entry 增删改、owner 变化、测量有效性语义变化。
- 任务层：线程/任务优先级、周期、栈、阻塞行为、调度模型变化。
- 硬件层：AFE/CAN/GPIO/WDT/NVM wrapper 或 devicetree 绑定变化。
- 参数层：保护阈值、SOC 参数、采样周期、通信周期等安全或标定相关配置变化。

普通注释、日志文字、无行为变化的小修可不单独评审，但 PR 自审仍需说明影响面。

## 2. 评审输入

至少提供：

- 需求或问题来源：`REQ-*`、bug、风险、架构债、TODO。
- 设计说明：`DES-*` 或对应 `docs/work/features/<slug>/03-design.md`。
- 影响面：模块、database entry、diagnosis、task、配置、测试。
- 风险与回滚：失败时如何回到安全态，如何撤销。
- 验证计划：单测、集成、native_sim/QEMU、检视、分析或后续 HIL。

## 3. 评审清单

| 维度 | 问题 |
|------|------|
| 架构边界 | 是否符合 `architecture.md` 的分层？是否引入反向依赖？ |
| 模块接口 | 是否符合 `module-interface.md`？是否新增私有长期线程？ |
| 数据所有权 | 每个 DB entry 是否有唯一 owner？读写是否值拷贝？ |
| 任务模型 | 周期、优先级、阻塞点是否合理？是否影响 safety task？ |
| 诊断路径 | 故障是否进入 `bms_diag`？严重度和锁存是否明确？ |
| 失效安全 | 任意异常输入/超时/无效测量是否回到 OPEN 或禁止 NORMAL？ |
| 测试性 | 核心逻辑是否可纯函数单测？是否有集成验证计划？ |
| 追溯 | `REQ → DES → code → test` 是否无断链？ |
| 变更再基线 | 是否同步更新受影响文档、测试和 traceability？ |

## 4. 评审结论

评审结果只能是：

| 结论 | 含义 |
|------|------|
| `APPROVED` | 可进入编码或合并 |
| `APPROVED_WITH_NOTES` | 可继续，但备注项必须进入 TODO/后续特性 |
| `BLOCKED` | 不得进入下一阶段，必须修正设计 |

安全相关改动出现以下任一情况，结论必须为 `BLOCKED`：

- 接触器可能在诊断 ERROR/CRITICAL 或测量无效时闭合。
- 故障只打日志，不进入诊断或状态机。
- safety cyclic task 引入无限等待或不可控阻塞。
- database entry owner 不唯一。
- 无测试/分析/检视任一验证手段。

## 5. 记录位置

- 小 V 特性：记录到 `docs/work/features/<slug>/gate-log.md`，并在 `02-architecture.md` 或 `03-design.md` 写明评审结论。
- 跨模块架构改动：记录到 PR 描述，并在相关 `concept-*` / `standard-*` / `process-*` 文档中同步。
- 安全相关结论：必要时回链到 [safety.md](../concept/safety.md) 的 SG 或风险项。

## 6. 单人项目执行方式

当前单人项目无法强制第二 reviewer，因此采用三层替代：

- 自审：按本文清单逐项核对。
- agent/code-reviewer：对安全或架构改动做独立评审。
- CI 与测试：构建、单测、覆盖率、SCA、clang-tidy 全绿。

团队化后，应把 `reviewer >= 1` 和 `require_code_owner_reviews` 加入分支保护。
