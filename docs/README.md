# docs 文档入口

本文档目录只回答三个问题：**怎么做、证据在哪、事实以谁为准**。细节不要在多处重复，能链接就链接。

## 先看哪里

| 你要找 | 入口 | 说明 |
|---|---|---|
| 项目总约定 | [../CLAUDE.md](../CLAUDE.md) | 工具、流程、质量门的最终约定入口 |
| 架构与设计原则 | [concept/](concept/) | Why / What，只写稳定原则 |
| 开发流程 | [process/workflow.md](process/workflow.md) | How，一个特性从需求到合并怎么走 |
| 工程规范 | [standard/](standard/) | 代码布局、接口、编码规则 |
| 质量门禁 | [quality/gates.md](quality/gates.md) | CI job、阈值、阻断条件等易变事实 |
| 构建测试命令 | [guide/build.md](guide/build.md) | 本地构建、测试、覆盖率 |
| 活交付物 | [work/](work/) | 当前特性、追溯矩阵、测试证据 |
| 参考资料 | [reference/](reference/) | 硬件资料、旧需求、datasheet |

## 文档分层

```text
CLAUDE.md
  └─ docs/
      ├─ concept/    稳定概念和架构契约
      ├─ process/    工作流和协作规则
      ├─ standard/   必须遵守的工程规范
      ├─ quality/    门禁、质量证据和风险边界
      ├─ guide/      可执行操作手册
      ├─ work/       当前正在交付的工程证据
      └─ reference/  外部输入和历史参考
```

## 写文档规则

- **一个事实只维护一处**：CI 阈值在 `quality/gates.md`，追溯在 `work/traceability.md`，命令在 `guide/build.md`。
- **主文档短，细节下沉**：入口页只放导航和判断规则；大段解释放到专题文档或归档。
- **活文档优先**：`work/` 是当前交付证据；`reference/legacy-requirements/` 不能直接当作现行需求。
- **文档要能支持评审**：新增需求、架构、测试或安全改动时，同步更新对应追溯和证据位置。

## 常用路径

- 新特性流程：`docs/work/features/<slug>/`
- 需求追溯：`docs/work/traceability.md`
- 当前质量门：`docs/quality/gates.md`
- 本地预检：`scripts/check.ps1`
- API 文档配置：`docs/Doxyfile`
