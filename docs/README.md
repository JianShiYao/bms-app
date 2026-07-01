# 文档索引（docs/）

bms-app 的文档导航。文档按**两条正交轴**组织：

- **学科目录**：软件设计契约集中在 `design/`，硬件资料在 `hardware/`（与 `design/` 并行）；跨学科的流程/方法/质量/指南类文档放 `docs/` 根。
- **命名约定**（权威）：`<category>-<topic>.md`（小写 kebab），`category` ∈ `concept`/`process`/`guide`/`standard`/`quality`。前缀与所在目录**正交**（如 `design/concept-architecture.md`、`design/standard-module-interface.md`）。

例外（各按自身约定，详见下方「子目录」）：`traceability.md`、`features/<slug>/`（`NN-<阶段>.md`）、`requirements/`（按域名）、`superpowers/specs|plans/`（`YYYY-MM-DD-…`）、`Doxyfile`。

## 两个权威锚点

- **[../CLAUDE.md](../CLAUDE.md)** —— 项目约定**唯一来源**（工具/CI 为质量门禁最终权威）。
- **[concept-methodology.md](concept-methodology.md)** —— 方法论**根基/母文档**（安全案例驱动的敏捷+V）；一切下游流程由它衍生。

## 关系图

```
  ../CLAUDE.md            约定唯一来源（被所有文档回指）
        |
  concept-methodology.md  方法论根基（Why）— 以下全部由它衍生
        |
        v   两条轴：学科目录（design/·hardware/） × 命名前缀（<category>-<topic>）

  docs/design/   软件设计契约（agent 据此实现/重构代码）
      concept-architecture ★ · concept-runtime-model · concept-data-model
      concept-safety · concept-diagnostics-fault-model · standard-module-interface
  docs/ 根        跨学科流程 / 方法 / 质量 / 指南
      concept-methodology · concept-documentation-system
      process-workflow ☆ · git · agents · design-review · small-v-workflow
      quality-management · ci-checklist · integration-test-strategy · traceability (*)
      guide-build
  docs/hardware/ 硬件资料（原理图/BOM/数据手册；与 design/ 并行学科目录）

  ★   concept-architecture = 软件架构基线；runtime-model/data-model/safety/diagnostics 与 standard-module-interface 细化它
  ☆   process-workflow = 流程单一事实源(SSOT)；git/agents/design-review/small-v-workflow 细化或落地它
  (*) traceability.md 不带类别前缀，与 features/<slug>/traceability.md 有意并行同名
```

## 文档清单

### `docs/design/` —— 软件设计契约（agent 实现代码的权威依据）

| 文件 | 角色 | 依据/细化谁 |
|------|------|------------|
| [concept-architecture.md](design/concept-architecture.md) | 软件架构基线 v0（ADR 风格） | methodology / foxBMS 2 借鉴 |
| [concept-runtime-model.md](design/concept-runtime-model.md) | 运行时模型设计契约（`bms_task`/`bms_time`/`bms_sys_mon`/watchdog 目标形态） | architecture ADR-ARCH-003 |
| [concept-data-model.md](design/concept-data-model.md) | `bms_db` 数据契约基线（entry/owner/validity/sequence/stale） | architecture ADR-ARCH-002 |
| [concept-safety.md](design/concept-safety.md) | 安全概念（危害→安全目标→安全功能，轻量初版） | methodology §6.1 |
| [concept-diagnostics-fault-model.md](design/concept-diagnostics-fault-model.md) | 诊断与故障模型设计契约（登记表/severity/去抖·锁存·老化/→bms_bms） | architecture ADR-ARCH-005 |
| [standard-module-interface.md](design/standard-module-interface.md) | 模块接口标准（task/db/diag/安全默认态） | architecture |

### `docs/` 根 —— 流程 / 方法 / 质量 / 指南

| 文件 | 角色 | 依据/细化谁 |
|------|------|------------|
| [concept-methodology.md](concept-methodology.md) | 方法论母文档（Why） | — |
| [concept-documentation-system.md](concept-documentation-system.md) | 研发文档/证据体系骨架 | methodology + CLAUDE |
| [process-workflow.md](process-workflow.md) | 开发流程**单一事实源**（完整生命周期） | 衍生自 methodology |
| [process-git.md](process-git.md) | Git 制度细化 + 操作/恢复手册 | 细化 workflow |
| [process-agents.md](process-agents.md) | 小V 的 subagent 链使用指南（手动） | 依据 methodology |
| [process-small-v-workflow.md](process-small-v-workflow.md) | 小V 自动编排 workflow | = process-agents §3.2 的自动化版 |
| [process-design-review.md](process-design-review.md) | 设计评审流程与 checklist | workflow + methodology 原则6/7 |
| [guide-build.md](guide-build.md) | 环境/构建/QEMU/WSL/测试 | 命令权威；回指 CLAUDE.md |
| [quality-management.md](quality-management.md) | 质量管控全景（已落地/待补） | 关联 workflow / ci-checklist / TODO |
| [quality-ci-checklist.md](quality-ci-checklist.md) | CI 门禁借鉴落地清单 | — |
| [quality-integration-test-strategy.md](quality-integration-test-strategy.md) | 多模块集成测试策略 | quality-management |
| [traceability.md](traceability.md) | 需求追溯矩阵（活仓库·权威） | methodology 原则3 |

> `standard-` 为规范类文档；当前已从模块接口标准开始（在 `design/`），后续可补诊断、配置/标定、编码细则。

## 子目录

| 目录 | 内容 |
|------|------|
| `design/` | **软件设计契约**（架构/运行时/数据/安全/接口标准）——agent 实现或重构代码的权威依据 |
| `hardware/` | 硬件资料（原理图/BOM/数据手册说明；大二进制不入库），与 `design/` 并行学科目录 |
| `templates/` | 各阶段产出模板（需求/设计/追溯矩阵等） |
| `requirements/` | 旧 S16100B 固件移植参考需求（按域名 `afe.md`/`soc.md`…） |
| `features/<slug>/` | 每个小V 的全部交付物（`NN-<阶段>.md` + 该特性 `traceability.md`），当前重点样例：`soc-coulomb/`、`engine-core-architecture/` |
| `superpowers/specs/` · `plans/` | 设计与实施计划（`YYYY-MM-DD-<topic>-<design\|plan>.md`） |

## 「查哪份」速查

- 为什么这么做 → **concept-methodology**；怎么走流程 → **process-workflow**（Git 细节 → process-git）。
- 文档体系怎么理解 → **concept-documentation-system**；系统怎么设计 → **design/concept-architecture**；DB 数据契约 → **design/concept-data-model**。
- 运行时/任务/调度/看门狗怎么定 → **design/concept-runtime-model**；诊断/故障/severity/锁存怎么定 → **design/concept-diagnostics-fault-model**；模块接口怎么写 → **design/standard-module-interface**。
- 怎么编译/跑（含 WSL native_sim）→ **guide-build**。
- 危害/安全目标/失效安全为什么这么定 → **design/concept-safety**。
- 需求验证到哪 → **traceability**；engine 核心证据链 → **features/engine-core-architecture**；质量现状/CI 路线 → **quality-management** / **quality-ci-checklist**；集成测试怎么补 → **quality-integration-test-strategy**。
- 用 agent/workflow 开发特性 → **process-agents** / **process-small-v-workflow**。
