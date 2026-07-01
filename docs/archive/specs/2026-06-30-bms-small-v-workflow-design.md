# BMS 小 V 自动编排工作流（`bms-small-v`）— 设计文档

- 日期：2026-06-30
- 目标读者：本项目研发者 / 维护 agent
- 状态：设计已与用户确认（待写实施计划）
- 关联：[concept-methodology.md](../../concept-methodology.md)（方法论母文档，§3 小 V）、
  [process-agents.md](../../process-agents.md)（7 个 agent 与 §3.3 阶段门）、
  [process-workflow.md](../../process-workflow.md)（操作/质量门）

## 1. 目标

把现有"敏捷-V 小 V"从**主会话手动逐步派发**（方案 A）升级为**一次调用、自动跑完整个特性**的确定性编排，
借鉴 `agency-agents` 项目的"编排者 + QA 门 + 重试"模式，落地为 Claude Code 的 **Workflow 脚本**。

**成功判据**：对一个特性执行一次 `bms-small-v`，自动产出该特性 `docs/features/<slug>/` 的全部小 V 交付物
+ 产品代码 + 测试，且：测试在 venv/CI 同源工具上通过、覆盖率达门槛、追溯矩阵无断链——全程无需人工逐步派发。

## 2. 借鉴 agency-agents（对照落地）

`agency-agents` 的编排模型（经调研）：通用 agent schema → 转工具格式；**不自动执行**，靠人手动激活或部署一个
**一等公民"编排者 agent"**（`specialized/agents-orchestrator.md`）自主跑流水线——任务分解 → 派发专家 →
QA 门 → 失败重试（≤3）→ 状态跟踪；跨 agent 上下文用可选 MCP memory（remember/recall）交接。

映射到本项目：

| agency-agents 机制 | 本项目落地 |
|---|---|
| 一等公民"编排者 agent"（绕开 subagent 不能嵌套） | **Workflow 脚本本身**作为一等驱动者，由 Workflow 运行时派发各 subagent |
| QA 门 + 重试 ≤3（Dev↔QA 循环） | **phase 2 的 tester↔coder 循环**（§5） |
| MCP memory 交接上下文 | **`docs/features/<slug>/` 文件交接**（已有，**不引入 MCP**） |

## 3. 形态与输入

- **命名工作流** `bms-small-v`，脚本置于 `.claude/workflows/bms-small-v.js`（或等价位置），用 Workflow 工具调用。
- **输入** `args = { slug, feature, riskTier }`：
  - `slug`：特性目录名（如 `contactor-gpio`）。
  - `feature`：一句话特性目标。
  - `riskTier`：`"safety"` | `"normal"`，决定 TDD 握手严格度（§5、对应 agents-guide §3.7）。

## 4. 编排管线（确定性，写在脚本里）

```
phase 0 编排   bms-orchestrator → 00-iteration-plan.md + 初始化 traceability.md
phase 1 左腿   requirements → architect → designer（串行依赖；每步后过阶段门）
phase 2 TDD    riskTier=safety: tester(红灯) → coder(实现+venv构建) → tester(复验)
               riskTier=normal: coder(实现+最小测试+构建) → tester(复验)
               复验失败 → 带报错回 coder，重试 ≤3（agency-agents Dev↔QA + retry≤3）
phase 3 收尾   bms-cicd（把新测试纳入 CI 校验）  +  code-reviewer（评审门）
phase 4 验收   脚本断言 traceability.md 无断链 + 汇总小 V 报告
```

- **agent 调用方式**：每个 `agent()` 调用让子代理 **Read 对应 `.claude/agents/bms-<role>.md` 并完全扮演该角色**，
  再对 `docs/features/<slug>/` 读写。理由：`.claude/agents/*.md` 是单一事实源，且不依赖"自定义 agent 已在会话注册"
  （subagent 类型注册需重启会话）。**待 agent 注册后**可改用 `agent(..., { agentType: 'bms-<role>' })` 优化。
- **左腿串行**：requirements→architect→designer 有上下游依赖，必须串行，不并行。
- **单工作树**：coder 与 tester 顺序执行（非并行），不产生文件冲突，无需 worktree 隔离。

## 5. 阶段门（分层锚定真值 + 门即数据）

**定义**：阶段门是两阶段之间的检查关卡，自动化 agents-guide §3.3 的"调用后必须检查 / 不通过回退"。
做三件事：①验产出 ②判 pass/fail ③不过则带 gaps 反馈回退重试。

**实现原则——尽量锚在"真值信号"（实际跑出来的结果）上，弃用"生产者自报 passed"（自评不可信）**：

- **能跑的阶段 → 机械断言 + 真值**：
  - coder 门 = `west build`（venv，mps2/an386）**退出码**。
  - tester 门 = `run-tests-coverage.ps1` / twister **0 failures** + 覆盖率 **≥ 门槛**（与 CI 同源）。
  - 追溯门 = 脚本断言矩阵**无空格** 且 每条 REQ 映射的测试用例名**在测试文件中真实存在**（交叉核对）。
- **只产文档的阶段（requirements/architect/designer）→ 机械断言 + 独立对抗验证**：
  - 机械断言：文件存在、`REQ-<域>-NNN`/`DES-<域>-NNN` 正则、EARS 句式、安全需求已标注、追溯对应列已回填。
  - 独立验证：另起一个**与生产者不同**的验证子代理，对抗式 prompt（"找缺漏，拿不准判 FAIL"），返回
    结构化 verdict `{ passed, gaps[] }`。

**门即数据**：每个阶段门声明为一份显式规格，便于审计与一致：
```
GATE[stage] = {
  requires:   [必需产物路径],
  asserts:    [(name, 机械断言函数)],     // 结构/正则/真值
  groundTruth: 命令与判定（可选，如 build/test 退出码、覆盖率阈值）,
  verify:     独立验证者 prompt（仅文档阶段）,
}
```

**门记录持久化**：每次门的 `{stage, pass/fail, gaps, attempt}` 追加到 `docs/features/<slug>/gate-log.md`，
失败与重试可追溯。

**重试与回退**：phase 2 失败回 coder（≤3 次）；文档阶段门 fail 则带 gaps 重跑该阶段（≤2 次）；
超限则 workflow 以"BLOCKED + 已写 gate-log"停止，交主会话裁决（不静默放过）。

## 6. 边界与非目标（YAGNI）

- **不碰 git**：workflow 只把文档/代码/测试写入工作树，**不 commit / 不 push / 不开 PR**；分支与提交由主会话/人
  在 workflow 结束后处理。**git 阶段为后期单独补充**（见 §8）。
- **不引入 MCP memory**：文件交接已够。
- **不并行多特性**：单工作树串行一个小 V。
- **不自动改分支保护 / CI 必过列**。

## 7. 成本与约束（如实告知）

- coder/tester 阶段在子代理里跑真实 `west build`（~2min/次）与 twister；全流程含重试可能 **10–20+ 分钟、token 较多**——
  这是"自动跑完整特性"的固有成本。
- Workflow 运行需用户显式 opt-in（已满足：用户明确要求自动编排）。
- 子代理跑 Windows venv 命令需 Bash 工具与正确路径（`..\.venv\Scripts\python.exe -m west ...`，见 agents-guide §4）。

## 8. 扩展点（后期）

- **git 阶段**：在 phase 4 之后增加可选的"切特性分支 → 提交各阶段产物 → 开 PR `--base master`"，遵循
  development-workflow §3/§5；先确保编排稳定再加。
- agent 注册后用 `agentType:'bms-<role>'` 替代"读 md 扮演"。
- 真机目标增多后拆分集成/系统测试（当前合并）。

## 9. 已决决策清单

1. 路径 = A）Workflow 编排脚本（命名工作流 `bms-small-v`）。
2. 阶段门 = 分层锚定真值（机械断言 + 能跑阶段锚 build/test/覆盖率；文档阶段加独立对抗验证），弃用自报 passed。
3. 门即数据 + gate-log 持久化。
4. 重试：tester↔coder ≤3；文档门 ≤2；超限 BLOCKED 交主会话。
5. 交接 = 文件（不引入 MCP memory）。
6. git = 暂不碰，后期单独补 git 阶段。
7. agent 调用 = 读 `.claude/agents/bms-<role>.md` 扮演（注册后改 agentType）。
