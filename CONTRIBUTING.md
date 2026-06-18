# 贡献指南

完整的开发流程、提交规范、PR 流程、质量门禁与发布流程见
**[docs/development-workflow.md](docs/development-workflow.md)**。

环境安装 / 构建 / 运行 / 测试见 [README.md](README.md)。

快速上手：
1. `git config core.hooksPath scripts/hooks`（克隆后一次性）
2. 从 `master` 切 `feat/xxx` 分支开发
3. 本地自检：`scripts\format.ps1 -Check` → `west build -b mps2/an386 app` → `west twister -T tests -p mps2/an386 -c`
4. `gh pr create --base master` → CI 5 项必过 → Squash 合并
