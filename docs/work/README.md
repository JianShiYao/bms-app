# work/ —— 活的工程产物

- **这里放什么**：随产品演进的**活产物**与证据。需求↔设计↔测试的**唯一权威活矩阵** `traceability.md`，与各小 V 的证据包 `features/<slug>/`。新固件需求随小 V 记入 `features/<slug>/01-requirements.md`，它是**该特性的证据包快照**（非常青规范）；跨特性的权威现状以 `traceability.md` 为准。设计权威是 `../concept/` 契约（architecture / runtime-model / data-model / …），requirements 不凌驾其上。
- **不要放什么**：按日期沉淀的历史记录、常青规范文档（→ `concept/`·`process/`·`standard/`·`quality/`）、**旧固件逆向参考需求**（→ `../reference/legacy-requirements/`，那是参考输入不是活产物）。
- **权威文件**：`traceability.md`（需求验证到哪的活矩阵）；每特性看 `features/<slug>/`。
