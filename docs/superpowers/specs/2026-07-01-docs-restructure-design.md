# 设计：docs/ 目录按类别重构（目录树自导航）

- **日期**：2026-07-01
- **状态**：设计已确认，待写实施计划
- **动机**：当前 `docs/` 顶层是 ~13 个带前缀散落文件 + 6 个文件夹混杂；分类信息编码在文件名前缀里，**目录树本身不导航**——须懂命名约定或读 README 才知道去哪，未充分服务研发流程。

## 1. 决策

| 决策 | 结论 | 依据 |
|------|------|------|
| 顶层组织轴 | **按文档类别**（"我要哪类文档"），前缀提升为文件夹 | 用户选定 |
| `design/` 学科目录 | **解散**（纯单轴，消除双轴混用） | 用户选定 |
| 工作产物层 | **两层**：类别文件夹 + `work/` + `reference/` | 用户选定 |
| 文件名前缀 | **去前缀**（目录已承载类别，前缀冗余） | 用户选定 |
| 子目录自解释 | 每个顶层子目录补一份短 `README.md`，只答三件事 | 用户选定 |

## 2. 目标结构

```
docs/
  README.md                     顶层索引（改写）
  Doxyfile                      doxygen 工具配置（留根，见 §5）
  concept/                      为什么这么做 / 目标模型
    README.md
    methodology.md  documentation-system.md
    architecture.md  runtime-model.md  data-model.md
    diagnostics-fault-model.md  safety.md
  process/                     研发活动怎么走、门在哪
    README.md
    workflow.md  git.md  design-review.md  agents.md  small-v-workflow.md
  standard/                    必须遵守的工程契约
    README.md
    module-interface.md  coding-style.md
  quality/                     如何证明做得够好（规范）
    README.md
    gates.md  ci-checklist.md  integration-test-strategy.md  management.md
  guide/                       操作怎么做
    README.md
    build.md
  work/                        活的工程产物 / 证据
    README.md
    requirements/  features/  specs/  plans/  traceability.md
  reference/                   参考资料 / 支撑
    README.md
    hardware/  templates/
```

## 3. 文件迁移映射（old → new）

**concept/**（去 `concept-` 前缀；`design/` 解散）
- `concept-methodology.md` → `concept/methodology.md`
- `concept-documentation-system.md` → `concept/documentation-system.md`
- `design/concept-architecture.md` → `concept/architecture.md`
- `design/concept-runtime-model.md` → `concept/runtime-model.md`
- `design/concept-data-model.md` → `concept/data-model.md`
- `design/concept-diagnostics-fault-model.md` → `concept/diagnostics-fault-model.md`
- `design/concept-safety.md` → `concept/safety.md`

**standard/**
- `design/standard-module-interface.md` → `standard/module-interface.md`
- `coding-style.md` → `standard/coding-style.md`

**process/**
- `process-workflow.md` → `process/workflow.md`
- `process-git.md` → `process/git.md`
- `process-design-review.md` → `process/design-review.md`
- `process-agents.md` → `process/agents.md`
- `process-small-v-workflow.md` → `process/small-v-workflow.md`

**quality/**
- `quality-gates.md` → `quality/gates.md`
- `quality-ci-checklist.md` → `quality/ci-checklist.md`
- `quality-integration-test-strategy.md` → `quality/integration-test-strategy.md`
- `quality-management.md` → `quality/management.md`

**guide/**
- `guide-build.md` → `guide/build.md`

**work/**（`superpowers/` 不透明名消失）
- `requirements/` → `work/requirements/`
- `features/` → `work/features/`
- `superpowers/specs/` → `work/specs/`
- `superpowers/plans/` → `work/plans/`
- `traceability.md` → `work/traceability.md`

**reference/**
- `hardware/` → `reference/hardware/`
- `templates/` → `reference/templates/`

**留在原地**
- `docs/README.md`（改写以反映新结构）
- `docs/Doxyfile`（工具配置，见 §5）

## 4. 每个顶层子目录的 README（三段式）

每份只答：**这里放什么 / 不要放什么 / 权威文件是哪份**。初稿：

- **concept/**：放=为什么这么做与目标模型（方法论、文档体系、架构/运行时/数据/诊断/安全设计契约）；不放=操作步骤(→guide)、流程门(→process)、可执行接口/编码约束(→standard)、活产物(→work)；权威=`methodology.md`（方法论母文档）。
- **process/**：放=研发活动怎么走、门在哪（生命周期、Git、评审、agent 编排）；不放=门禁阈值事实表(→quality)、为什么(→concept)；权威=`workflow.md`（流程单一事实源）。
- **standard/**：放=必须遵守的工程契约（模块接口、编码风格）；不放=目标模型讨论(→concept)、如何证明(→quality)；权威=`module-interface.md`。
- **quality/**：放=如何证明做得够好（门禁事实表、CI 清单、集成测试策略、质量管控全景）；不放=活的证据/矩阵(→work)、流程步骤(→process)；权威=`gates.md`（门与阈值唯一事实源）。
- **guide/**：放=具体操作怎么做（环境/构建/测试/WSL）；不放=规则与约定(→concept/standard)；权威=`build.md`。
- **work/**：放=活的工程产物与证据（需求、特性交付物、设计 spec、实施 plan、追溯矩阵）；不放=常青规范文档(→ 概念/流程/标准/质量)；权威=`traceability.md`（需求↔测试活矩阵），每特性见 `features/<slug>/`。
- **reference/**：放=参考资料与模板（硬件原理图/BOM/数据手册、文档模板）；不放=常青规范、活产物；权威=`hardware/__00_readme.md`、`templates/README.md`。

## 5. 待迁移期核实的点

- **Doxyfile**：默认留 `docs/Doxyfile`。迁移前须 grep 脚本/CI 对 `docs/Doxyfile`、`docs/superpowers`、`INPUT` 路径的引用；若无强约束可再议。
- **脚本/CI 路径引用**：`scripts/*.ps1`、`.github/workflows/*` 可能引用 `docs/...` 路径（如 doxygen INPUT、追溯检查），须一并纳入引用修正面。

## 6. 引用修正策略（迁移核心难点）

本次**几乎所有文档都移动**，相对链接的正确新目标取决于"源新位置 × 目标新位置"，不能简单前缀替换。策略：

1. 以 §3 的 old→new 映射为唯一事实源，构建映射表。
2. 对每个 markdown 文件的每条本地链接：把 old 目标解析为仓库绝对路径 → 过映射表得到 new 绝对路径 → 按该文件的 **new 位置**重算相对路径。
3. 代码注释/Kconfig/脚本里的 `docs/...` 路径式引用：按映射表直接替换绝对路径段。
4. 裸文件名 prose 提及（反引号、无路径）：文件名会变（去前缀 + 可能改目录），需评估——本次去前缀使裸名也变，故这类**也要处理或标注**（与前两次不同）。
5. 校验：脚本逐链按所在目录解析，`broken == 0`；无 `docs/docs`、无遗留旧路径；末行换行/CRLF/editorconfig；构建+测试由 CI 兜底。

## 7. 同步改写的元文档

- `docs/README.md`：索引、关系图、命名约定（改为"目录=类别、文件名去前缀"）、速查、子目录表。
- `concept/documentation-system.md`：§2 五类前缀 → 五类**目录**；删/改 §2b 学科目录（design/ 已解散）；权威链树。
- `CLAUDE.md`：§2 必读文档链接、§8 命名约定。
- `TODO.md`、根 `README.md`：文档链接。

## 8. 影响与代价（诚实记录）

- 本次**推翻上一个 commit 刚建的 `docs/design/`**（方向由用户改定）。这是第三次目录调整；写本 spec 即为固定"为什么是这个结构"，避免第四次返工。
- 引用面大（代码注释、CLAUDE、TODO、features/specs/plans 互链、脚本/CI），须脚本分批 + 逐链校验，风险集中在相对路径重算与脚本路径引用。

## 9. 非目标

- 不改文档**内容**（仅移动 + 去前缀 + 补子目录 README + 修引用 + 改写元文档）。
- 不清理与本次无关的既存失联链接（如 `quality-verification.md` 等历史死链）。
