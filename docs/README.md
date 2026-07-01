# 文档索引（docs/）

bms-app 的文档导航。命名约定见 [../CLAUDE.md](../CLAUDE.md)「文档命名约定」——顶层文档用 `<category>-<topic>.md`，类别 ∈ `concept`/`process`/`guide`/`standard`/`quality`。

## 两个权威锚点

- **[../CLAUDE.md](../CLAUDE.md)** —— 项目约定**唯一来源**（工具/CI 为质量门禁最终权威）。
- **[concept-methodology.md](concept-methodology.md)** —— 方法论**根基/母文档**（安全案例驱动的敏捷+V）；一切下游流程由它衍生。

## 关系图

```
            ../CLAUDE.md  ← 约定唯一来源（被所有文档回指）
                  │
        concept-methodology.md  ← 方法论根基（Why）
                  │ 衍生
        ┌─────────┼──────────────────────────────┐
        ▼         ▼                               ▼
 process-         traceability.md            （质量门禁理念）
 workflow.md      需求追溯矩阵·活仓库·权威         │
 流程单一事实源     （与 features/<slug>/           │
   │  │            traceability.md 并行同名）       │
   │  ├── process-git.md      ← workflow 的 Git 机制细化+操作手册
   │  ├── process-agents.md   ← 小V 各阶段 agent 链使用（手动派发）
   │  │        │ 自动化版
   │  │        ▼
   │  │   process-small-v-workflow.md  ← 一条 workflow 跑完小V
   ▼
 concept-architecture.md  ← 目标架构（foxBMS 2 inspired）

 质量与构建（横切）
   ├── quality-management.md   → 引用 workflow + quality-ci-checklist + TODO
   ├── quality-ci-checklist.md → CI 门禁落地清单（P0–P3 + 供应链）
   └── guide-build.md          → 构建/QEMU/WSL/测试（命令权威；回指 CLAUDE.md）
```

## 文档清单

| 文件 | 角色 | 依据/细化谁 |
|------|------|------------|
| [concept-methodology.md](concept-methodology.md) | 方法论母文档（Why） | — |
| [concept-architecture.md](concept-architecture.md) | 目标架构（foxBMS 2 借鉴） | — |
| [process-workflow.md](process-workflow.md) | 开发流程**单一事实源**（完整生命周期） | 衍生自 methodology |
| [process-git.md](process-git.md) | Git 制度细化 + 操作/恢复手册 | 细化 workflow |
| [process-agents.md](process-agents.md) | 小V 的 subagent 链使用指南（手动） | 依据 methodology |
| [process-small-v-workflow.md](process-small-v-workflow.md) | 小V 自动编排 workflow | = process-agents §3.2 的自动化版 |
| [guide-build.md](guide-build.md) | 环境/构建/QEMU/WSL/测试 | 命令权威；回指 CLAUDE.md |
| [quality-management.md](quality-management.md) | 质量管控全景（已落地/待补） | 关联 workflow / ci-checklist / TODO |
| [quality-ci-checklist.md](quality-ci-checklist.md) | CI 门禁借鉴落地清单 | — |
| [traceability.md](traceability.md) | 需求追溯矩阵（活仓库·权威） | methodology 原则3 |

> `standard-`（规范类，如编码规范）为保留类别，当前 `docs/` 顶层暂无该文件。

## 子目录

| 目录 | 内容 |
|------|------|
| `templates/` | 各阶段产出模板（需求/设计/追溯矩阵等） |
| `requirements/` | 旧 S16100B 固件移植参考需求（按域名 `afe.md`/`soc.md`…） |
| `features/<slug>/` | 每个小V 的全部交付物（`NN-<阶段>.md` + 该特性 `traceability.md`），样例 `soc-coulomb/` |
| `superpowers/specs/` · `plans/` | 设计与实施计划（`YYYY-MM-DD-<topic>-<design\|plan>.md`） |
| `hardware/` | 硬件资料（原理图/BOM/数据手册说明；大二进制不入库） |

## 「查哪份」速查

- 为什么这么做 → **concept-methodology**；怎么走流程 → **process-workflow**（Git 细节 → process-git）。
- 怎么编译/跑（含 WSL native_sim）→ **guide-build**；系统怎么设计 → **concept-architecture**。
- 需求验证到哪 → **traceability**；质量现状/CI 路线 → **quality-management** / **quality-ci-checklist**。
- 用 agent/workflow 开发特性 → **process-agents** / **process-small-v-workflow**。
