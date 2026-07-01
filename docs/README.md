# 文档索引（docs/）

bms-app 的文档导航。**目录即分类**——打开一个子目录，先看它的 `README.md`（三段式：**放什么 / 不放什么 / 权威文件**），再进具体文件。文件名**不带类别前缀**（类别由所在目录表达，如 `concept/architecture.md`）。

## 顶层地图

| 目录 | 是什么 | 权威文件 |
|------|--------|----------|
| [concept/](concept/) | 为什么这么做 / 目标模型（方法论、架构/运行时/数据/诊断/安全设计契约） | `methodology.md` |
| [process/](process/) | 研发活动怎么走、门在哪 | `workflow.md` |
| [standard/](standard/) | 必须遵守的工程契约（接口、编码） | `module-interface.md` |
| [quality/](quality/) | 如何证明做得够好 | `gates.md` |
| [guide/](guide/) | 具体操作怎么做 | `build.md` |
| [work/](work/) | 活的工程产物（需求 / 特性 / 追溯矩阵） | `traceability.md` |
| [archive/](archive/) | 历史归档（设计 spec / 实施 plan，按日期沉淀） | —（快照） |
| [reference/](reference/) | 参考阅读资料（硬件原理图/BOM/数据手册） | `hardware/__00_readme.md` |
| [templates/](templates/) | 可复制的产出骨架 | `README.md` |
| `Doxyfile` | doxygen 工具配置（生成 API 文档，留 `docs/` 根） | — |

## 两个权威锚点

- **[../CLAUDE.md](../CLAUDE.md)** —— 项目约定**唯一来源**（工具/CI 为质量门禁最终权威）。
- **[concept/methodology.md](concept/methodology.md)** —— 方法论**根基/母文档**（安全案例驱动的敏捷+V）；一切下游流程由它衍生。

## 关系图（谁依据谁）

```
../CLAUDE.md               约定唯一来源（被所有文档回指）
      |
concept/methodology.md     方法论根基（Why）— 以下由它衍生
      |
      ├─ concept/    architecture ★ · runtime-model · data-model · diagnostics-fault-model · safety · documentation-system
      ├─ standard/   module-interface · coding-style
      ├─ process/    workflow ☆ · git · design-review · agents · small-v-workflow
      ├─ quality/    gates ● · ci-checklist · integration-test-strategy · management
      └─ guide/      build

产物 / 参考（非常青规范，不在上面的派生链里）：
   work/      requirements · features/<slug> · traceability
   archive/   specs · plans（YYYY-MM-DD 快照）
   templates/ 可复制骨架        reference/ 硬件资料

★ architecture = 软件架构基线；runtime-model/data-model/diagnostics-fault-model/safety 与 standard/module-interface 细化它
☆ workflow = 流程单一事实源(SSOT)；git/design-review/agents/small-v-workflow 细化或落地它
● gates = 门与阈值唯一事实源；quality/management 仅总览，不抢权威
```

## 「查哪份」速查

- 为什么这么做 → **concept/methodology**；系统怎么设计 → **concept/architecture**；运行时/任务/调度/看门狗 → **concept/runtime-model**；诊断/故障/severity/锁存 → **concept/diagnostics-fault-model**；DB 数据契约 → **concept/data-model**；危害/安全目标/失效安全 → **concept/safety**；文档体系怎么理解 → **concept/documentation-system**。
- 怎么走流程 → **process/workflow**（Git 细节 → **process/git**）；用 agent/workflow 开发特性 → **process/agents** · **process/small-v-workflow**；设计评审 → **process/design-review**。
- 模块接口怎么写 → **standard/module-interface**；编码风格 → **standard/coding-style**。
- 门禁与阈值 → **quality/gates**；质量现状全景 → **quality/management**；集成测试怎么补 → **quality/integration-test-strategy**；CI 借鉴清单 → **quality/ci-checklist**。
- 怎么编译/跑（含 WSL native_sim）→ **guide/build**。
- 需求验证到哪 → **work/traceability**；某特性证据链 → **work/features/<slug>**（如 `engine-core-architecture`）；需求基线 → **work/requirements**。
- 历史设计/实施计划 → **archive/specs** · **archive/plans**。
- 硬件资料 → **reference/hardware**；文档模板 → **templates/**。
