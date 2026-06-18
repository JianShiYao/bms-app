# Changelog

本项目的所有重要变更记录于此。
格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本遵循 [语义化版本](https://semver.org/lang/zh-CN/)（0.x 约定见 [开发流程 §8](docs/development-workflow.md#8-发布流程cd)）。

## [Unreleased]

## [0.1.0] - 2026-06-18

首个里程碑：在 QEMU 上跑通的 Zephyr v4.4.0 BMS 固件骨架，并建立完整的开发流程与质量门禁。

### Added
- Zephyr v4.4.0 BMS 固件骨架（west T2 拓扑），5 个模块 afe/soc/protection/balancing/comm，zbus 解耦。
- QEMU（`mps2/an386`，Cortex-M4F）跑通：模块初始化 + 周期心跳。
- ztest 单元测试 2 套（soc 5 例 + protection 6 例，11/11 通过）。
- 自定义 STM32F405 板模板 `boards/enervenue/bms_f405`（待完善）。
- 代码格式化基线：Zephyr 风格 `.clang-format`、`.gitattributes`（统一 LF）、`.editorconfig`、`scripts/format.ps1`、pre-commit 钩子。
- CI 质量门禁（GitHub Actions）：`format` → `build (mps2/an386, native_sim)` + `test-coverage`(native_sim 覆盖率) + `sca-gcc`(gcc 静态分析) + `clang-tidy`(软门禁)。
- CD：`release.yml`，打 tag `v*` 自动构建固件制品 + `SHA256SUMS` 并发布 GitHub Release。
- 开发流程治理：`docs/development-workflow.md`、`CONTRIBUTING.md`、PR 模板、`CODEOWNERS`、`dependabot.yml`、本 `CHANGELOG.md`。
- master 分支保护（PR + 5 项 CI 必过、线性历史、禁直推/强推）。

[Unreleased]: https://github.com/JianShiYao/bms-app/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/JianShiYao/bms-app/releases/tag/v0.1.0
