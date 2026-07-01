# `bms-small-v` 自动编排工作流 · 操作指导

一条调用自动跑完一个特性的敏捷-V 小 V（编排→需求→架构→详细设计→TDD→CICD→评审→追溯校验）。
脚本：[`.claude/workflows/bms-small-v.js`](../../.claude/workflows/bms-small-v.js)。

- 设计依据：[docs/archive/specs/2026-06-30-bms-small-v-workflow-design.md](../archive/specs/2026-06-30-bms-small-v-workflow-design.md)
- 实施计划：[docs/archive/plans/2026-06-30-bms-small-v-workflow.md](../archive/plans/2026-06-30-bms-small-v-workflow.md)
- 借鉴来源：`agency-agents` 的"编排者 agent + QA 门 + 重试"——这里把"编排者"实现为 **Workflow 脚本本身**（绕开 Claude Code subagent 不能嵌套），QA门+重试=tester↔coder 真值门循环，跨阶段交接=`docs/work/features/<slug>/` 文件（非 MCP memory）。
- 与手动方式的关系：本工作流是 [agents.md](agents.md) §3.2「方案 A 主会话手动派发」的**自动化版本**；二者产物布局一致。

---

## 1. 它做什么（一图）

```
phase 0 编排    bms-orchestrator → 00-iteration-plan.md + 初始化 traceability.md
phase 1 左腿    requirements → architect → designer（串行；每步过"文档门"，不过则带 gaps 重试 ≤2）
phase 2 TDD     [safety:先写红灯] → coder(实现+venv构建真值门) → tester(全量 twister 真值门)
                复验失败带报错回 coder，重试 ≤3
phase 3 收尾    bms-cicd（纳入 CI）+ code-reviewer（独立评审门）
phase 4 验收    追溯门：断言 traceability.md 6 列无空格 + 用例名在 tests/ 真实存在 → 返回总结
```

- **阶段门**分层锚定真值：能跑的阶段锚 `west build`/twister 退出码；文档阶段用独立验证者对抗式审。
- 每道门把结论追加到 `docs/work/features/<slug>/gate-log.md`（可追溯重试历史）。
- **不碰 git**：只把文档/代码/测试写进工作树，提交由人事后处理。

## 2. 前提（调用前确认）

```powershell
# 1) 7 个 agent 在位（脚本靠它们扮演各阶段角色）
ls bms-app\.claude\agents\bms-*.md          # 应 7 个
# 2) venv 工具链可用（coder/tester 真值门要真跑）
& D:\__00_WorkSpace\__06_Study\bms-workspace\.venv\Scripts\python.exe -m west --version   # West v1.5.0
# 3) 语法自检（可选，本机有 node 时）
node --check bms-app\.claude\workflows\bms-small-v.js
```
- **Workflow 工具需显式 opt-in**：因为它会派多个子代理、真跑构建/测试（贵）。直接要求"用 bms-small-v 跑某特性"即视为 opt-in。

## 3. 怎么调用

用 Workflow 工具，`scriptPath` 指向脚本，`args` 传特性三元组：

```
Workflow({
  scriptPath: "d:/__00_WorkSpace/__06_Study/bms-workspace/bms-app/.claude/workflows/bms-small-v.js",
  args: { slug: "<特性目录名>", feature: "<一句话目标>", riskTier: "normal" | "safety" }
})
```

- `slug`：特性目录名（kebab，如 `contactor-gpio`）→ 产物落在 `docs/work/features/<slug>/`。
- `feature`：一句话目标（越具体越好，含默认值/单位）。
- `riskTier`：
  - `safety`：protection/阈值/接触器/采样等安全相关 → **强制 tester 先写红灯用例**再实现。
  - `normal`：普通特性 → coder 自带最小测试，tester 复验。
- 脚本对 `args` 容错：即便被序列化成字符串也会 `JSON.parse`。
- **后台运行**：调用即返回 run ID，完成时有通知；`/workflows` 看实时进度树。
- 中途改脚本后续跑可用 `resumeFromRunId`（已过阶段命中缓存，只重跑改动段）。

## 4. 读结果（返回对象）

工作流返回一个对象，`status` 是总判：

| status | 含义 | 处理 |
|--------|------|------|
| `DONE` | 全流程通过、追溯无断链 | 进入第 5 节独立验证 |
| `BLOCKED` | 某阶段门连续重试仍不过（`stage`/`gaps` 指明） | 看 gaps 修脚本提示或 agent 契约，resume 重跑该段 |
| `REVIEW_BLOCKED` | 代码评审发现 Blocker | 按 gaps 回对应阶段修正 |
| `TRACE_GAP` | 追溯矩阵有断链/用例不存在 | 补回填 traceability.md |

附带 `tdd:{failures, covLine}`、`traceability`、`deliverables`、`note`。
> `covLine` 在 QEMU(mps2/an386) 路线常为 **-1**（gcov 截断）——**这是已知现象，覆盖率以 CI(native_sim) 为准，不据此判失败**。

## 5. 执行结果验证（务必独立复核，别只信自报）

工作流自报 `DONE` 后，**主会话/人**按下面独立验证（真值优先）：

```powershell
# (a) 交付物齐全
ls bms-app\docs\features\<slug>\
#   期望：00-iteration-plan / 01-requirements / 02-architecture / 03-design /
#         05-test-report / 06-cicd / traceability / gate-log  共 8 份

# (b) 门历史可追溯
cat bms-app\docs\features\<slug>\gate-log.md

# (c) 独立复跑全量测试（地面真值，不信工作流自报）
powershell -ExecutionPolicy Bypass -File run-tests-coverage.ps1 -Board mps2/an386 -NoCoverage
```

判读 (c)：
- 关注 **"X of Y executed test cases passed"**（用例级）与 **"configurations: N, failures: M"**（配置级）。
- ⚠️ **QEMU 偶发超时 = flaky，非真回归**：若某配置 FAILED 且日志是 `Timeout (qemu 60s)`、而**用例级 100% passed**，多半是 Windows QEMU 偶发超时。**单套件重跑确认**：
  ```powershell
  $env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
  & .venv\Scripts\python.exe -m west twister -T bms-app\tests\bms\<suite> -p mps2/an386 -O twister-out-x --clobber-output
  ```
  重跑通过即坐实 flaky；重跑仍失败才是真问题。

- (d) 追溯抽查：打开 `traceability.md`，确认每条 REQ 的"测试用例"列用例名能在 `tests/bms/<module>/src/main.c` 找到。

## 6. 成本、边界与已知事项

- **成本**：一次完整跑通常 **10–20+ 分钟、可能上百万 token**（含真实 west 构建 ~2min/次 + twister + 重试）。这是"自动跑完整特性"的固有代价。
- **不碰 git**：工作流只写工作树。产物（含验证特性的代码/测试）需人审阅后手动提交；后期会单独补"git 阶段"（自动切分支/提交/PR）。
- **真值门口径**：`passed` 以**断言失败数==0**为准；QEMU flaky 超时不计、覆盖率不在此门控（见 §4）。tester 跑**全量**套件以抓跨套件回归。
- **gate-log**：5 类门（4 文档门 + coder + tester + review + trace）均落盘到 `gate-log.md`。
- **agent 注册**：脚本让子代理 `Read .claude/agents/bms-<role>.md` 扮演角色（单一事实源，且不依赖会话已注册自定义 agent）。将来 agent 注册后可改 `agentType:'bms-<role>'`。

## 7. 实跑参考（首个验证特性）

`slug=comm-report-period-kconfig`（normal，"comm 上报周期 Kconfig 化"）一次跑通：17 个 agent、约 69 分钟；
4 个文档门一次过、TDD 一轮过、`bms.comm` PASSED、57/57 用例过、追溯无断链、返回 `DONE`。
独立复跑时遇到的 `bms.soc` 失败经单套件重跑确认为 **QEMU flaky 超时**（与本特性无关）——即 §5 的典型案例。

## 8. 故障排查速查

| 现象 | 多半原因 | 处理 |
|------|----------|------|
| 秒挂、报 `需要 args` | args 没传或格式错 | 按 §3 传 `{slug, feature, riskTier}` |
| 某阶段 `BLOCKED` | 门判据未过 / agent 产出不达标 | 看 `gaps` 与 `gate-log.md`，修提示后 `resumeFromRunId` 重跑 |
| 测试"built (not run)" | 没设 `QEMU_BIN_PATH` | 用 `run-tests-coverage.ps1`（自动设），或手动 set 后再 twister |
| 某套件 Timeout 失败但用例全过 | QEMU flaky 超时 | 单套件重跑确认（§5） |
| `covLine=-1` | QEMU gcov 截断 | 正常；覆盖率以 CI(native_sim) 为准 |
