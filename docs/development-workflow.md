# 开发流程与质量审查

本文是 bms-app 的**开发流程单一事实源**。环境安装/构建/运行/测试的命令见 [README](../README.md)；
本文聚焦"如何协作开发、如何保证质量、如何发版"。

---

## 1. 分支模型

- `master`：唯一受保护主干，始终可构建、CI 全绿。**不可直接 push**（受分支保护）。
- 工作分支：`<type>/<kebab-描述>`，type 对齐提交规范：
  `feat/ fix/ docs/ ci/ style/ chore/ refactor/ test/ build/ perf/`
  例：`feat/soc-coulomb-counting`、`fix/protection-uv-threshold`、`ci/add-release-workflow`。

## 2. 提交信息规范（Conventional Commits）

```
<type>(<scope>): <祈使句摘要>

<可选正文：为什么这么改>
<footer：BREAKING CHANGE: ... / Refs #n>
```
- `type`：`feat fix docs style refactor test chore ci build perf`
- `scope`：模块名 —— `soc protection afe balancing comm board ci docs`
- 版本影响：`feat`→次版本，`fix`→修订，`BREAKING CHANGE`→主版本（0.x 见 §7）。

## 3. 克隆后一次性设置

```powershell
git config core.hooksPath scripts/hooks   # 启用 pre-commit 格式钩子
```
详见 [README 第六节](../README.md#六代码格式化与提交检查)。

## 4. 提交前自检（pre-PR 三连）

在开 PR 前本地先过一遍，避免 CI 才发现问题：
```powershell
powershell -ExecutionPolicy Bypass -File scripts\format.ps1 -Check   # 格式
west build -b mps2/an386 app                                         # 能编
west twister -T tests -p mps2/an386 -c                               # 测试 11/11
```
（覆盖率本地走 `..\run-tests-coverage.ps1`；QEMU 路线覆盖率不可靠，可靠覆盖率见 CI / WSL2+native_sim。）

## 5. PR 流程

```
git switch -c feat/xxx          # 从最新 master 切分支
# ... 编码 + 本地自检 ...
git push -u origin feat/xxx
gh pr create --base master      # 开 PR（填模板）
# CI 自动跑 → 5 个必过检查全绿 → Squash 合并 → 自动删分支
```
- 合并策略：**仅 Squash**（master 线性、每 PR 一条规范提交）。
- master 受保护：CI 必过才能合并；禁直推、禁强推、要求线性历史。

## 6. 五道质量关

| 关 | 时机 | 工具 | 阻断点 |
|---|---|---|---|
| ① 编辑时 | 写码 | `.clang-format` + `.editorconfig`（编辑器即时） | 软约束 |
| ② 提交前 | `git commit` | `scripts/hooks/pre-commit`（暂存 .c/.h 格式校验） | 本地拒绝提交 |
| ③ CI / PR | push / PR | `.github/workflows/ci.yml` | 分支保护拦合并 |
| ④ 发布 | 打 tag | `.github/workflows/release.yml`（tag `v*`） | 发布失败即无 Release |
| ⑤ 审计 | 持续 | `dependabot` / `CODEOWNERS` / `CHANGELOG` / PR 模板 | 软约束 |

CI（③）当前 6 道门禁：`format` → `build (mps2/an386)` + `build (native_sim)` + `test-coverage`(native_sim 覆盖率) + `sca-gcc`(gcc 静态分析) + `clang-tidy`(CERT/可读性)。
SCA/clang-tidy/覆盖率的路线图见 [ci-borrow-checklist.md](ci-borrow-checklist.md)。

## 7. 分支保护说明

master 受保护，必过检查（6 项）：`format`、`build (mps2/an386)`、`build (native_sim)`、`test-coverage`、`sca-gcc`、`clang-tidy`。
- `clang-tidy` 已**硬门禁**（`.clang-tidy` 开启 `WarningsAsErrors`，当前 0 告警）并加入必过列。
- **单人项目说明**：必需 reviewer = 0（无法要求他人评审），用「PR + CI 必过 + 自审 diff」替代第二双眼；
  团队化后改 reviewer ≥ 1、启用 `require_code_owner_reviews`、考虑 `enforce_admins`。

## 8. 发布流程（CD）

`VERSION` 文件 + git tag + `release.yml` 联动：
1. 开 `chore(release): bump vX.Y.Z` 分支；
2. 改 [VERSION](../VERSION)（`PATCHLEVEL` 或 `VERSION_MINOR` +1），把 `CHANGELOG.md` 的 `[Unreleased]` 落为 `## [X.Y.Z] - 日期`；
3. PR → CI 过 → squash 合并；
4. master 上 `git tag -a vX.Y.Z -m "release vX.Y.Z"` → `git push origin vX.Y.Z`；
5. tag push 触发 `release.yml`：校验 tag == VERSION → Release 构建 → 发布固件制品 + `SHA256SUMS`。

**SemVer（0.x 固件约定）**：`0.MINOR.PATCH` —— MINOR 进位 = 新功能或可能破坏性变更，PATCH = 修复。
上真实 STM32F405 板量产稳定后再升 `1.0.0`。
