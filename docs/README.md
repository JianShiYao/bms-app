# 文档索引（docs/）

bms-app 的文档导航。**顶层文档命名约定**（权威）：`<category>-<topic>.md`（小写 kebab），`category` ∈ `concept`/`process`/`guide`/`standard`/`quality`。例外（各按自身约定，详见下方「子目录」）：`traceability.md`、`features/<slug>/`（`NN-<阶段>.md`）、`requirements/`（按域名）、`superpowers/specs|plans/`（`YYYY-MM-DD-…`）、`Doxyfile`。

## 两个权威锚点

- **[../CLAUDE.md](../CLAUDE.md)** —— 项目约定**唯一来源**（工具/CI 为质量门禁最终权威）。
- **[concept-methodology.md](concept-methodology.md)** —— 方法论**根基/母文档**（安全案例驱动的敏捷+V）；一切下游流程由它衍生。

## 关系图

```
  ../CLAUDE.md            约定唯一来源（被所有文档回指）
        |
  concept-methodology.md  方法论根基（Why）— 以下全部由它衍生
        |
        v   按文档 5 类横向展开（命名 <category>-<topic>；topic 详见下方清单）

  concept/    ->  architecture · data-model · safety · documentation-system
  process/    ->  workflow ★ · git · agents · design-review · small-v-workflow
  standard/   ->  module-interface
  quality/    ->  management · ci-checklist · integration-test-strategy · traceability (*)
  guide/      ->  build

  ★   process-workflow = 流程单一事实源(SSOT)；git/agents/design-review/small-v-workflow 细化或落地它
      （small-v-workflow = process-agents 的自动编排版）
  (*) traceability.md 不带类别前缀，与 features/<slug>/traceability.md 有意并行同名
```

## 文档清单

| 文件 | 角色 | 依据/细化谁 |
|------|------|------------|
| [concept-methodology.md](concept-methodology.md) | 方法论母文档（Why） | — |
| [concept-documentation-system.md](concept-documentation-system.md) | 研发文档/证据体系骨架 | methodology + CLAUDE |
| [concept-architecture.md](concept-architecture.md) | 软件架构基线 v0（ADR 风格） | methodology / foxBMS 2 借鉴 |
| [concept-data-model.md](concept-data-model.md) | `bms_db` 数据契约基线（entry/owner/validity/sequence/stale） | architecture ADR-ARCH-002 |
| [concept-safety.md](concept-safety.md) | 安全概念（危害→安全目标→安全功能，轻量初版） | methodology §6.1 |
| [process-workflow.md](process-workflow.md) | 开发流程**单一事实源**（完整生命周期） | 衍生自 methodology |
| [process-git.md](process-git.md) | Git 制度细化 + 操作/恢复手册 | 细化 workflow |
| [process-agents.md](process-agents.md) | 小V 的 subagent 链使用指南（手动） | 依据 methodology |
| [process-small-v-workflow.md](process-small-v-workflow.md) | 小V 自动编排 workflow | = process-agents §3.2 的自动化版 |
| [process-design-review.md](process-design-review.md) | 设计评审流程与 checklist | workflow + methodology 原则6/7 |
| [standard-module-interface.md](standard-module-interface.md) | 模块接口标准（task/db/diag/安全默认态） | architecture |
| [guide-build.md](guide-build.md) | 环境/构建/QEMU/WSL/测试 | 命令权威；回指 CLAUDE.md |
| [quality-management.md](quality-management.md) | 质量管控全景（已落地/待补） | 关联 workflow / ci-checklist / TODO |
| [quality-ci-checklist.md](quality-ci-checklist.md) | CI 门禁借鉴落地清单 | — |
| [quality-integration-test-strategy.md](quality-integration-test-strategy.md) | 多模块集成测试策略 | quality-management |
| [traceability.md](traceability.md) | 需求追溯矩阵（活仓库·权威） | methodology 原则3 |

> `standard-` 为规范类文档；当前已从模块接口标准开始，后续可补诊断、配置/标定、编码细则。

## 子目录

| 目录 | 内容 |
|------|------|
| `templates/` | 各阶段产出模板（需求/设计/追溯矩阵等） |
| `requirements/` | 旧 S16100B 固件移植参考需求（按域名 `afe.md`/`soc.md`…） |
| `features/<slug>/` | 每个小V 的全部交付物（`NN-<阶段>.md` + 该特性 `traceability.md`），当前重点样例：`soc-coulomb/`、`engine-core-architecture/` |
| `superpowers/specs/` · `plans/` | 设计与实施计划（`YYYY-MM-DD-<topic>-<design\|plan>.md`） |
| `hardware/` | 硬件资料（原理图/BOM/数据手册说明；大二进制不入库） |

## 「查哪份」速查

- 为什么这么做 → **concept-methodology**；怎么走流程 → **process-workflow**（Git 细节 → process-git）。
- 文档体系怎么理解 → **concept-documentation-system**；系统怎么设计 → **concept-architecture**；DB 数据契约 → **concept-data-model**。
- 怎么编译/跑（含 WSL native_sim）→ **guide-build**；模块接口怎么写 → **standard-module-interface**。
- 危害/安全目标/失效安全为什么这么定 → **concept-safety**。
- 需求验证到哪 → **traceability**；engine 核心证据链 → **features/engine-core-architecture**；质量现状/CI 路线 → **quality-management** / **quality-ci-checklist**；集成测试怎么补 → **quality-integration-test-strategy**。
- 用 agent/workflow 开发特性 → **process-agents** / **process-small-v-workflow**。
