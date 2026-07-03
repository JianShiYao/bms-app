# 开发流程

本文是 bms-app 的流程入口。它只定义**必须遵守的路径和门禁时机**；命令见 [build.md](../guide/build.md)，CI 事实见 [gates.md](../quality/gates.md)，方法论背景见 [methodology.md](../concept/methodology.md)。

## 1. 一条特性的默认路径

```text
需求 -> 架构 -> 详细设计 -> 编码 -> 测试 -> CI -> PR -> 合并
```

每个特性默认对应：

- 一条分支：`feat/<kebab>` / `fix/<kebab>` / `docs/<kebab>` 等。
- 一个工作目录：`docs/work/features/<slug>/`。
- 一个 PR：过程文档、代码、测试、追溯一起评审。

最小交付物：

| 阶段 | 文件 | 要求 |
|---|---|---|
| 计划 | `00-iteration-plan.md` | 目标、非目标、风险、任务拆分 |
| 需求 | `01-requirements.md` | `REQ-<域>-NNN`，可验收 |
| 架构 | `02-architecture.md` | 模块边界、数据流、影响面 |
| 设计 | `03-design.md` | 接口、状态、错误路径、测试点 |
| 测试 | `05-test-report.md` | 执行结果和缺口 |
| CI | `06-cicd.md` | 门禁结果、跳过项、后续项 |
| 追溯 | 	raceability.md | 需求 -> 设计 -> 代码 -> 测试 |

示例见 [work/features/soc-coulomb/](../work/features/soc-coulomb)。

## 2. 追溯规则

```text
REQ-<域>-NNN -> DES-<域>-NNN -> code/test -> evidence
```

- 域名使用 `SYS/AFE/SOC/PROT/BAL/COMM/BOARD`。
- 安全相关需求必须有测试、分析或评审证据；优先自动化测试。
- ztest 用例顶部写 `/* Verifies REQ-<域>-NNN: ... */`。
- 追溯矩阵独立放在 `traceability.md`，不要塞进计划文档。

## 3. DoR / DoD

进入迭代前（DoR）：

- 依赖已明确：DB entry、zbus 通道、配置项、硬件前提等。
- 基线可构建、可测试。
- `00-iteration-plan.md` 已定义目标、非目标和风险。

完成迭代前（DoD）：

- 需求、架构、设计、代码、测试、CI 证据齐备。
- 追溯链无空洞。
- 安全相关路径有失效安全用例或明确风险接受。
- 本地预检和 CI 门禁按 [gates.md](../quality/gates.md) 通过。
- 非目标没有夹带实现。

## 4. 安全相关改动

涉及测量、保护、诊断、BMS 状态机、接触器、watchdog、阈值或参数的改动，按安全相关处理：

- 先有需求或契约，再改代码。
- 默认安全态必须清楚，例如接触器默认 OPEN，仅 NORMAL 才 CLOSED。
- 先补失败路径测试，再实现或重构。
- PR 中说明风险、影响面、验证证据和回滚办法。

## 5. 分支、提交、PR

分支：

```text
<type>/<kebab-description>
```

`type` 使用 `feat/fix/docs/ci/style/chore/refactor/test/build/perf`。

提交：

```text
<type>(<scope>): <summary>
```

PR：

- 从最新 `master` 切出。
- 合并策略为 Squash。
- `master` 必须保持可构建、CI 绿、线性历史。
- 多个不相关任务并行时，用 `git worktree` 隔离；一个任务一个 worktree、一个分支、一个 PR。

## 6. 本地自检

开 PR 前运行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check.ps1
```

快速反馈可用：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 -Fast
```

本地结果用于提前发现问题，CI 仍是合并事实源。Windows 上可能跳过 Linux/native_sim 相关检查，具体以 [gates.md](../quality/gates.md) 为准。

## 7. 质量门分层

| 层级 | 时机 | 目的 |
|---|---|---|
| 编辑器 | 写码时 | 格式和基础风格即时反馈 |
| pre-commit | 提交时 | 秒级格式检查 |
| pre-push | 推送时 | 低成本静态检查和告警 |
| 本地 check | 开 PR 前 | 构建、测试、静态分析预检 |
| CI / PR | push / PR | 分支保护事实源 |
| Release | tag | 版本、制品、校验和 |

重检查不放进 `pre-commit`；慢门禁放到本地 check 或 CI。

## 8. 发布

发布路径：

```text
改 VERSION/CHANGELOG -> PR -> CI -> squash -> tag vX.Y.Z -> release.yml
```

0.x 阶段约定：`0.MINOR.PATCH`，新功能或可能破坏兼容进 MINOR，修复进 PATCH。真实目标板稳定后再进入 `1.0.0`。
