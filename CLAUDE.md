# CLAUDE.md

本文件是给 Claude Code / 开发者的**精简锚点**：只放必须立刻知道的规则与指针。
细节（命令、门禁、架构、测试）一律在 `docs/` 权威文档，**不在此复制**——避免多处维护导致漂移。

> workspace 根的 `../CLAUDE.md`、`../.claude/CLAUDE.md` 与本仓库 `.claude/CLAUDE.md` 仅为指向本文件的指针。

## 1. 项目定位

基于 **Zephyr RTOS** 的电池管理系统（BMS）固件。所有开发在 **`bms-app/`**（应用 + west manifest，唯一 git 仓库）下进行；命令默认在此目录、已激活 venv 的 PowerShell 中执行（`zephyr/`、`modules/` 由 `west update` 拉到上一级）。

## 2. 必读权威文档

- [docs/README.md](docs/README.md) —— 文档索引与关系图（入口，先看这个）
- [docs/process/workflow.md](docs/process/workflow.md) —— 开发流程**单一事实源**
- [docs/quality/gates.md](docs/quality/gates.md) —— CI/本地**门禁事实表**（门与阈值只此一处）
- [docs/concept/architecture.md](docs/concept/architecture.md) —— 软件架构基线 v0（engine core）
- [docs/concept/data-model.md](docs/concept/data-model.md) —— `bms_db` 数据契约（entry/owner/validity/sequence/stale）

## 3. 不可违反的规则

- **失效安全**：接触器默认 **OPEN**，仅判定 NORMAL 才 CLOSED；改保护/阈值/默认态代码须格外谨慎（安全概念见 [docs/concept/safety.md](docs/concept/safety.md)）。
- **不手改依赖**：`../zephyr/`、`../modules/` 由 `west update` 管理，**勿手动编辑**。
- **CI 为权威**：分层质量门禁不可绕过；具体门与阈值以 [docs/quality/gates.md](docs/quality/gates.md) 为准，**本文件不复制门禁细节**。
- **不写死测试数量**：文档/注释/PR 不固定测试总数，以 `west twister` 与 CI 报告为准。
- **不编辑生成物**：`build/`、`twister-out*/`、`doxygen-out/` 等为只读参考，需要时重新生成，勿手改。

## 4. 架构红线

- 业务逻辑优先写成**纯函数**，线程只做调度 / IO / 发布，ztest 直接覆盖纯函数。
- 数据流遵守 **owner 规则**：单一写入者，经 `bms_db` / zbus 传递，不做跨模块隐式共享。
- **安全链优先级最高**：采样有效性 → 诊断聚合 → BMS 状态机 → 接触器输出，不得绕过 fail-safe 默认态。
- **改业务行为必须同步测试；安全/需求相关改动必须同步追溯**（细则见 [docs/process/workflow.md](docs/process/workflow.md) 与 [docs/work/traceability.md](docs/work/traceability.md)）。

## 5. 常用命令（Windows / PowerShell，`bms-app/` 下）

```powershell
& ..\.venv\Scripts\Activate.ps1                               # 每个新终端先激活 venv
west build -p always -b mps2/an386 app                        # 构建（QEMU 主力板）
$env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
west twister -T tests -p mps2/an386 -c                        # 测试（含单元/集成，以 Twister 报告为准）
powershell -ExecutionPolicy Bypass -File scripts\check.ps1    # 提交前本地预检（-Fast 跳重门）
powershell -ExecutionPolicy Bypass -File scripts\format.ps1   # 格式化（-Check 只检查）
```

完整环境搭建 / 多板构建 / WSL native_sim / 排错见 [docs/guide/build.md](docs/guide/build.md)。

## 6. 开发流程

从最新 `master` 切 `<type>/<kebab>` → Conventional Commits `<type>(<scope>): 摘要` → PR（`--base master`）→ CI 全绿 → **Squash 合并**。详见 [docs/process/workflow.md](docs/process/workflow.md)（Git 机制与恢复见 [docs/process/git.md](docs/process/git.md)）。

## 7. Agent 协作

可选，见 [docs/process/agents.md](docs/process/agents.md)。

## 8. 文档命名约定

新增 `docs/` 顶层文档须遵守 [docs/README.md](docs/README.md) 的命名约定（`<category>-<topic>.md`）。
