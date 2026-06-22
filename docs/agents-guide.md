# BMS 研发流程 Agent 体系 · 使用指南

本文说明 `.claude/agents/` 下这套覆盖**需求→架构→详细设计→编码→测试→CICD**全流程的
Claude Code subagent 怎么用。流程模型为**敏捷-V 混合**（迭代节奏 + 每个特性走 V 模型的设计↔验证）。

- 设计依据：[docs/superpowers/specs/2026-06-19-bms-agile-v-agents-design.md](superpowers/specs/2026-06-19-bms-agile-v-agents-design.md)
- 实施计划：[docs/superpowers/plans/2026-06-19-bms-agile-v-agents.md](superpowers/plans/2026-06-19-bms-agile-v-agents.md)
- 端到端样例：[docs/features/soc-coulomb/](features/soc-coulomb/)（一个完整"小 V"的全部交付物）

---

## 1. Agent 速查

| Agent | 角色 | 输入 | 产出 | 是否能跑命令(Bash) |
|-------|------|------|------|--------|
| `bms-orchestrator` | 迭代编排 | 特性描述 | `00-iteration-plan.md`（派发清单+追溯骨架） | 否 |
| `bms-requirements` | 需求分析 | 迭代计划 | `01-requirements.md`（EARS 需求+验收准则） | 否 |
| `bms-architect` | 架构设计 | 需求 | `02-architecture.md`（ADR、zbus/线程/安全） | 否 |
| `bms-designer` | 详细设计 | 架构 | `03-design.md`（签名/状态机/Kconfig/dts） | 否 |
| `bms-coder` | 编码实现 | 详细设计 | 产品代码 + 构建自检 | 是 |
| `bms-tester` | 测试验证 | 需求/设计/代码 | 测试代码 + `05-test-report.md` | 是 |
| `bms-cicd` | CI/CD | 测试与流水线 | `06-cicd.md` + 必要的 workflow 改动 | 是 |

代码评审复用环境内置的 `code-reviewer`，不单独造。

## 2. 小 V 流程

```
① 需求 ───────────────────────验证──▶ ⑥ 系统/集成测试（合并）
② 架构 ──────────────────验证──────▶ （集成验证并入⑥）
③ 详细设计 ──────验证────────────────▶ ④ 单元测试
        └──────▶ 编码实现 ──────────────┘
   贯穿：可追溯链(需求→架构→设计→代码→测试) · 代码评审 · CI 持续验证
```

## 3. 怎么用

### 3.1 前提：先重启 Claude Code 会话
自定义 agent 文件是在某次会话中新建的；**新建后需重启 Claude Code 会话**，它们才会被注册为
可调用的 agent 类型。重启后可用 `@bms-requirements` 之类直接调用，或让主会话用 Agent 工具派发。

### 3.2 编排机制（方案 A）
`bms-orchestrator` **只产出迭代计划与派发清单**，不亲自实现，也**不能**直接调用其它 agent
（subagent 不可嵌套）。真正"按顺序调用 ①~⑥"由**主会话**照计划执行。典型一轮：

1. 把一个特性交给 `bms-orchestrator` → 得到 `docs/features/<slug>/00-iteration-plan.md`
2. 主会话照清单**依次**派发 `bms-requirements` → `bms-architect` → `bms-designer`（左腿，串行依赖）
3. 派发 `bms-coder`（实现+构建自检）→ `bms-tester`（写测试+跑 twister）
4. 派发 `bms-cicd`，并用 `code-reviewer` 评审
5. 核对 `<slug>` 下追溯链无断链 → 提交

### 3.3 交付物布局
```
docs/features/<feature-slug>/
├─ 00-iteration-plan.md   # orchestrator：计划+派发清单+追溯骨架
├─ 01-requirements.md     # requirements
├─ 02-architecture.md     # architect
├─ 03-design.md           # designer
├─ 05-test-report.md      # tester
└─ 06-cicd.md             # cicd
```
产品代码与测试仍写入既有 `app/`、`tests/`；`docs/features/<slug>/` 只放过程交付物。

## 4. 工具链速记（以 Windows venv 为准）

本项目实际构建/测试链路是 **Windows 侧 venv**，不是 WSL：

```powershell
# 构建（QEMU/ARM 目标，默认板）
.venv\Scripts\python.exe -m west build -b mps2/an386 bms-app\app -p always
# 测试（自动设好 QEMU_BIN_PATH）
powershell -ExecutionPolicy Bypass -File run-tests-coverage.ps1 -Board mps2/an386
# 开 PR 前本地镜像 CI 全门
powershell -ExecutionPolicy Bypass -File bms-app\scripts\check.ps1        # 或 -Fast
```
WSL + `native_sim` 仅用于更可靠的覆盖率（CI 即走 native_sim）。

## 5. 与现有项目规范的关系（重要）

项目已有一套需求工程脚手架，**这套 agent 应当对齐它**：

- **ID 规范**（见 [docs/templates/README.md](templates/README.md)）：需求 `REQ-<域>-<NNN>`、设计 `DES-<域>-<NNN>`；
  域 = `SYS/AFE/SOC/PROT/BAL/COMM/BOARD`。例：`REQ-SOC-001`、`DES-SOC-002`。
- **模板**：[requirements-template.md](templates/requirements-template.md) /
  [design-spec-template.md](templates/design-spec-template.md) /
  [traceability-matrix-template.md](templates/traceability-matrix-template.md)。
- **追溯**：每条需求可追溯到一个验证手段；安全相关需求优先自动化测试；ztest 用
  `/* Verifies REQ-XXX-NNN: ... */` 注释标注。
- **分支/PR**（见 [development-workflow.md](development-workflow.md)）：从最新 **master** 切
  `feat/<kebab>` 分支，PR `--base master`，仅 Squash 合并，master 受 6 道 CI 门保护。

> ⚠️ 已知偏差（待对齐）：首个样例 `soc-coulomb` 使用了 `REQ-SOC-Cxx`（应为 `REQ-SOC-NNN`）、
> 把追溯表放进 `00-iteration-plan.md`（宜独立成 traceability matrix）、并从 `ci/local-quality-layering`
> 切分支（流程文档要求从 `master` 切）。后续应更新 agent 的 CKB/契约，使其引用上述模板与 ID 规范。

## 6. 扩展点（YAGNI 之外）
- 将来真机目标增多 → 把"集成测试 / 系统测试"从合并状态拆开。
- 想要"一条命令自动跑完整个小 V" → 把编排升级为 slash 命令/skill（方案 B）。
