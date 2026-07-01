# 研发文档体系骨架

> **文档定位**：本文说明 `docs/` 里各类文档如何组成一个可治理的研发体系。它不替代 [README.md](../README.md) 的导航作用，而是解释“为什么这些文档存在、谁是上游、什么算完成、变更时如何不散架”。
> **依据**：[methodology.md](methodology.md) 的敏捷+小 V+安全案例模型，[../CLAUDE.md](../../CLAUDE.md) 的项目约定与质量门禁。

## 1. 体系目标

本项目文档不是“写给人看的说明书”这么简单，而是安全相关 BMS 研发证据链的一部分：

- 让需求、架构、设计、代码、测试、CI 结果能互相回指。
- 让安全相关改动有明确的默认态、诊断路径、验证强度和评审记录。
- 让当前原型阶段能快速迭代，同时为真板、HIL、量产和安全案例留好接口。
- 让新增文档知道该放哪里、依据谁、完成标准是什么。

## 2. 五类顶层文档

| 类别 | 文件前缀 | 回答的问题 | 典型文件 |
|------|----------|------------|----------|
| 概念 | `concept-` | 为什么这么做、目标模型是什么 | `methodology.md`、`architecture.md`、`safety.md` |
| 流程 | `process-` | 研发活动怎么走、门在哪里 | `workflow.md`、`git.md`、`design-review.md` |
| 指南 | `guide-` | 某个操作怎么做 | `build.md` |
| 标准 | `standard-` | 必须遵守的工程契约 | `module-interface.md` |
| 质量 | `quality-` | 如何证明做得够好 | `management.md`、`ci-checklist.md`、`integration-test-strategy.md` |

判断一个新文档该用哪个前缀：

- 讲原则与模型，用 `concept-`。
- 讲流程顺序、准入准出、评审门，用 `process-`。
- 讲命令和操作步骤，用 `guide-`。
- 讲接口、命名、数据结构、配置、诊断这些硬约束，用 `standard-`。
- 讲测试、覆盖率、门禁、证据、成熟度，用 `quality-`。

## 2b. 学科目录（与命名前缀正交）

命名前缀回答「这是哪类文档」，**目录**回答「这属于哪个学科」，两者正交：

- `docs/design/` —— **软件设计契约**：架构、运行时、数据、安全概念、模块接口标准等 agent 据以实现/重构代码的权威件（`concept-` 与 `standard-` 前缀混居于此）。
- `docs/reference/hardware/` —— 硬件资料（原理图/BOM/数据手册），与 `design/` 并行。
- `docs/` 根 —— 跨学科的流程/方法/质量/指南（`process-`/`guide-`/`quality-` 及方法论、文档体系两篇 `concept-`）。

因此完整路径形如 `design/architecture.md`、`design/module-interface.md`；新增软件设计契约放 `design/`，其余按前缀留根。目录随学科增长（如将来 `systems/`），前缀不变。

## 3. 权威链

```
../CLAUDE.md
  项目约定、质量门禁、工具链最终权威
        │
methodology.md
  方法论母文档：敏捷 + 小 V + 安全案例
        │
        ├─ architecture.md
        │    目标软件架构：foxBMS 2 inspired / Zephyr native
        │
        ├─ safety.md
        │    安全概念：Hazard → Safety Goal → Safety Function
        │
        ├─ workflow.md
        │    生命周期流程、DoR/DoD、PR、发布
        │
        ├─ standard-*.md
        │    模块接口、诊断、配置、编码等工程契约
        │
        └─ quality-*.md
             测试、CI、覆盖率、集成/HIL、证据包
```

**规则**：下游文档不得反向覆盖上游原则。若流程或标准与方法论冲突，先改方法论或在变更说明中明确例外。

## 4. 小 V 证据包结构

每个 `docs/work/features/<slug>/` 是一个小 V 证据包，至少包含：

| 阶段 | 文件 | 作用 |
|------|------|------|
| 编排 | `00-iteration-plan.md` | 目标、范围、非目标、DoR/DoD |
| 需求 | `01-requirements.md` | `REQ-*`、验收标准、风险/安全属性 |
| 架构 | `02-architecture.md` | 架构决策、边界、影响面 |
| 详细设计 | `03-design.md` | `DES-*`、接口、异常处理、测试点 |
| 测试 | `05-test-report.md` | 单测/集成/覆盖率结果 |
| CI/CD | `06-cicd.md` | 是否纳入门禁、流水线结果 |
| 追溯 | `traceability.md` | `REQ → DES → code → test → status` |
| 过程门 | `gate-log.md` | 阶段门与重试历史 |

缺任一关键链路，不应宣称该特性“完成”。

## 5. 三种追溯层级

| 层级 | 位置 | 用途 |
|------|------|------|
| 系统活仓库追溯 | `docs/work/traceability.md` | 当前新固件已实现需求的权威总表 |
| 特性追溯 | `docs/work/features/<slug>/traceability.md` | 单个小 V 的局部闭环 |
| 旧固件参考需求 | `docs/work/requirements/*.md` | S16100B 逆向参考，不自动等同于新固件承诺 |

新增代码时优先更新“特性追溯”；合并稳定后，将已实现且可验证的需求汇入系统活仓库追溯。

## 6. 变更治理

文档变更分三类：

| 变更类型 | 例子 | 要求 |
|----------|------|------|
| 上游原则变更 | 改 methodology 原则、改安全目标 | 必须评估 README 关系图、流程、模板、TODO 是否同步 |
| 架构/标准变更 | 改 DB owner、任务优先级、诊断 ID 规则 | 必须触发设计评审，更新相关 `standard-*` 与追溯 |
| 操作性修订 | 命令、路径、说明修正 | 更新对应 guide/process，必要时跑命令验证 |

变更发生后要回答四个问题：

- 哪些需求/设计/测试受影响？
- 哪些安全目标或诊断路径受影响？
- 哪些 CI/本地命令能证明没有破坏？
- 哪些文档需要再基线？

## 7. 当前薄弱环节

按本文骨架衡量，当前优先补强顺序为：

1. `module-interface.md`：先把 foxBMS 2 inspired 模块接口契约定住。
2. `design-review.md`：让架构/安全/接口改动有明确评审门。
3. `bms_task` / `bms_db` / `bms_diag` / `bms_bms` 的需求、设计、追溯。
4. `integration-test-strategy.md` 与首批多模块集成测试。
5. `standard-config-calibration.md`：保护阈值、周期、容量、诊断参数的治理。
6. 安全案例继续重型化：FMEA/HARA/FTA/HIL 证据。

## 8. 维护

- 本文由 `docs/README.md` 引用，作为文档体系解释层。
- 新增顶层文档时，应同步更新 `docs/README.md` 的文档清单。
- 本文提到的薄弱环节若未在本次变更完成，应登记到 [../TODO.md](../../TODO.md)。
