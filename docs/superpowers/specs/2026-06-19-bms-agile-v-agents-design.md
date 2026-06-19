# BMS 敏捷-V 研发流程 Agent 体系 — 设计文档

- 日期：2026-06-19
- 项目：EnerVenue BMS 固件（Zephyr 4.4.0 + CMake，自定义板 `bms_f405` / `native_sim` 仿真）
- 形态：`.claude/agents/*.md` 自定义 subagent（深度定制 BMS/Zephyr）
- 产出语言：所有 agent 的文档/注释/交付物默认**中文**，与项目现有风格一致

## 1. 目标

为本 BMS 项目搭建一套覆盖**需求 → 架构 → 详细设计 → 编码 → 测试 → CI/CD**全流程的专业 subagent
体系，并以**敏捷-V 混合**流程把它们串起来：用迭代节奏推进，每个特性走 V 模型的"设计↔验证"
严谨性与可追溯性。

## 2. 研发流程模型：敏捷-V 混合

**迭代层（敏捷）**：以 backlog 驱动，每个迭代交付一个特性增量。
例：`补全 SOC 库仑计数`、`接触器 GPIO 驱动`、`CAN 上报真实帧`。

**每个特性走一个"小 V"**（左腿设计 ↔ 右腿验证，逐层对应）：

```
左腿（分解·设计）                                右腿（验证·集成）
① 需求分析 ───────────────────────────验证──▶ ⑥ 系统/集成测试（合并）
   EARS需求·验收准则·可追溯ID                     native_sim多模块·验收准则回归
② 架构设计 ──────────────────────验证──────▶ （集成验证并入⑥）
   zbus通道·线程模型·失效安全·模块边界
③ 详细设计 ────────验证────────────────────▶ ④ 单元测试
   状态机·接口·Kconfig/devicetree·数据结构        Twister ztest·覆盖率
        └──────────────▶ 编码实现 ──────────────────┘
                         Zephyr惯例·TDD·zbus范式
   贯穿：可追溯性(需求ID→设计→代码→测试) · 代码评审 · CI/CD持续验证
```

**关键特征**

- **可追溯性**是 V 模型的灵魂：右腿测试回溯到左腿对应层的需求/设计，每个特性保留一条 ID 链。
- **CI/CD 横贯**：每次提交自动跑构建+测试+覆盖率（已有 `.github/workflows/ci.yml`），作为持续验证关卡。
- **失效安全**是 BMS 全程红线（如默认接触器 OPEN），需求与测试都要显式覆盖。
- **集成测试与系统测试合并**（YAGNI）：当前仅单一 `native_sim` 目标 + 模块化结构，合并为一个验证
  环节即可；将来真机目标增多再拆。

## 3. Agent 名册（1 编排 + 6 阶段专家）

| # | Agent | 角色 | 核心职责（BMS/Zephyr 深度定制） |
|---|-------|------|------|
| 🎯 | **bms-orchestrator** | 迭代编排 | 管理 backlog，把特性拆成"小 V"，规划各阶段调用顺序，维护可追溯链，把关迭代准入/准出 |
| ① | **bms-requirements** | 需求分析 | EARS 格式需求、验收准则、可追溯 ID；显式覆盖失效安全场景 |
| ② | **bms-architect** | 架构设计 | zbus 通道设计、线程/优先级模型、模块边界、失效安全架构 |
| ③ | **bms-designer** | 详细设计 | 状态机、模块接口、Kconfig 开关、devicetree、数据结构 |
| ④ | **bms-coder** | 编码实现 | Zephyr 惯例 + zbus 范式 + TDD；按 `K_THREAD_DEFINE`/`zbus_chan_pub` 模板 |
| ⑤ | **bms-tester** | 测试验证 | Twister 单元测试 + native_sim 集成/系统测试 + 覆盖率；右腿回溯左腿 |
| ⑥ | **bms-cicd** | CI/CD | GitHub Actions 构建/测试/覆盖率门禁/发布；维护 `ci.yml` |

**代码评审**：复用环境已内置的 `code-reviewer` agent（DRY），不另造 `bms-reviewer`。若后续需要 BMS
专门化评审视角再扩展。

## 4. 编排机制（方案 A）

自定义 subagent 由**主线程**用 Agent 工具调用，且 subagent **不能嵌套调用**其它 subagent。因此：

- `bms-orchestrator` 作为 subagent，负责**产出迭代计划 + 可追溯链 + 阶段派发顺序清单**（结构化交付物）。
- 真正"按顺序调用 ①~⑥"的动作，由**主线程（Claude 主会话）照编排计划执行**。
- 编排计划是一份可读、可勾选的清单（见 §6 交付物），主线程据此逐阶段派发对应 subagent，并在每阶段
  完成后回填可追溯链与准出判断。

> 取舍：若将来想要"一条命令自动跑完整个小 V"，可把编排升级为 slash 命令/skill（方案 B）。本设计先做 A，
> 保持纯 subagent 形态、职责清晰。

## 5. 各 Agent 规格

每个 agent 的 `.md` 文件遵循 Claude Code subagent 规范：YAML frontmatter（`name`/`description`/
`tools`/可选 `model`）+ 正文系统提示。正文统一包含四段：**角色与边界 / 项目知识（BMS·Zephyr）/
输入与输出契约 / 工作准则与禁忌**。

### 5.1 公共项目知识（注入每个 agent）

- 架构：zbus 总线解耦，5 模块（afe/soc/protection/balancing/comm），`main.c` 只做 init，
  模块用 `K_THREAD_DEFINE` 自启线程。
- 通信：`zbus_chan_pub` 发布 / `ZBUS_SUBSCRIBER_DEFINE`+`zbus_sub_wait` 订阅；通道在
  `channels.c` 用 `ZBUS_CHAN_DEFINE` 定义。
- 数据类型：`bms/types.h`（`bms_cell_meas`/`bms_soc`/`bms_prot_evt` 等）。
- 配置：`app/Kconfig` 模块开关与参数；`app/boards/*.conf|*.overlay`；板定义
  `boards/enervenue/bms_f405/`。
- 测试：`tests/bms/*` 用 Twister + ztest；纯逻辑函数与线程分离以便单测（范例
  `bms_protection_evaluate`）。
- 构建：Zephyr 装在 **WSL**，用 `west build`；仿真目标 `native_sim`。
- 红线：失效安全（默认接触器 OPEN，仅 NORMAL 才闭合）。

### 5.2 各 agent 输入/输出契约（摘要）

| Agent | 输入 | 输出交付物 |
|-------|------|-----------|
| bms-orchestrator | 特性描述/backlog 条目 | 迭代计划 + 小 V 派发清单 + 可追溯链骨架 |
| bms-requirements | 特性目标 + 编排清单 | EARS 需求 + 验收准则 + 需求 ID（含失效安全场景） |
| bms-architect | 需求文档 | 架构决策（zbus 通道/线程/模块边界/安全）+ 架构↔需求追溯 |
| bms-designer | 架构文档 | 详细设计（状态机/接口/Kconfig/devicetree/数据结构）+ 设计↔架构追溯 |
| bms-coder | 详细设计 | 实现代码（TDD，遵循 zbus/线程范式）+ 代码↔设计追溯 |
| bms-tester | 需求+设计+代码 | Twister 单元 + 集成/系统测试 + 覆盖率 + 测试↔需求/设计回溯 |
| bms-cicd | 测试与构建产物 | GitHub Actions 配置/门禁/发布；维护 `ci.yml` |

工具权限按最小必要授予：需求/架构/设计类以读+写文档为主（Read/Write/Edit/Glob/Grep），
coder/tester/cicd 额外需要 Bash（`west`/`twister`/git）。

## 6. 交付物与文件布局

```
bms-app/
├─ .claude/agents/                       # 7 个 subagent 定义
│  ├─ bms-orchestrator.md
│  ├─ bms-requirements.md
│  ├─ bms-architect.md
│  ├─ bms-designer.md
│  ├─ bms-coder.md
│  ├─ bms-tester.md
│  └─ bms-cicd.md
├─ docs/
│  ├─ superpowers/specs/                  # 本设计文档
│  └─ features/<feature-slug>/            # 每个特性的小 V 交付物（按需创建）
│     ├─ 00-iteration-plan.md             # orchestrator：计划+派发清单+可追溯链
│     ├─ 01-requirements.md
│     ├─ 02-architecture.md
│     ├─ 03-design.md
│     ├─ 05-test-report.md
│     └─ traceability.md                  # 需求ID→架构→设计→代码→测试 全链
```

代码与测试仍写入既有 `app/`、`tests/` 目录结构；`docs/features/<slug>/` 仅存放流程交付物。

## 7. Agent 自身的验证

agent 体系本身也要"可用性验证"：

- 选一个**真实小特性**（推荐：`补全 SOC 库仑计数`）端到端跑一遍小 V，确认 7 个 agent 能按方案 A 串起来。
- 验收：产出齐全的 `docs/features/soc-coulomb/` 交付物 + 通过的 Twister 测试 + 一条完整可追溯链。

## 8. 范围与非目标（YAGNI）

- **不做**：重量级功能安全文档（ISO 26262 工作产物全集）、独立 `bms-reviewer`、集成/系统测试拆分、
  方案 B 的命令式自动编排。
- **保留扩展点**：将来可加真机目标后拆分测试、把编排升级为 slash 命令。

## 9. 已决决策清单

1. 形态 = 自定义 subagent 文件
2. 深度 = BMS/Zephyr 深度定制
3. 流程 = 敏捷-V 混合
4. 编排 = 独立 `bms-orchestrator`，方案 A（产出计划，主线程执行）
5. 测试 = 集成+系统合并为一个验证 agent
6. 评审 = 复用现有 `code-reviewer`
7. 语言 = 中文交付物
8. 位置 = `bms-app/`（git 仓库内）
