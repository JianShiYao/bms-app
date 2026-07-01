# 集成测试策略

> **文档定位**：本文补齐“多模块集成验证”薄弱环节。单元测试验证纯函数，集成测试验证 `bms_task + bms_db + bms_diag + bms_bms` 这些模块之间的闭环。

## 1. 目标

集成测试重点证明：

- 任务框架按预期调用模块服务。
- database 快照在模块间正确传播。
- 诊断触发后能影响 BMS 主状态机。
- 故障路径最终禁止接触器闭合。
- 通信/均衡/SOC 等非安全信息流不拖累安全链。

## 2. 测试分层

| 层级 | 位置 | 目的 |
|------|------|------|
| 单元测试 | `tests/bms/<module>` | 纯函数、边界、异常输入 |
| 组件集成 | `tests/integration/<topic>` | 两到三个模块组合，如 DB+DIAG+BMS |
| 系统仿真 | `native_sim` / QEMU | task 调度、日志、整机烟雾 |
| HIL/台架 | 后续自托管 runner | AFE、GPIO、接触器、真实故障注入 |

当前优先补“组件集成”，不等真机。

## 3. 首批集成测试主题

| ID | 场景 | 期望 |
|----|------|------|
| INT-DB-001 | 写入 `DB_CELL_MEAS` 后读取 | sequence 增加，valid=true，值一致 |
| INT-DIAG-001 | 上报 ERROR/CRITICAL 诊断 | `bms_diag_has_error()` 为真 |
| INT-BMS-001 | protection NORMAL + 无诊断 | 状态可从 INIT/STANDBY 走向 NORMAL |
| INT-BMS-002 | 测量无效或 protection FAULT | BMS 进入 FAULT/LOCKED，接触器 OPEN |
| INT-TASK-001 | safety task 跑一轮 | DB 中出现 cell/prot/bms 快照 |
| INT-COMM-001 | DB 快照上报 | comm TX 不改变 DB，不影响安全状态 |

## 4. 测试实现原则

- 优先测纯集成函数和 database API，避免依赖真实时间。
- 对 task 线程测试要提供可控入口；若当前只有无限循环线程，先抽出 `bms_task_run_once_*()` 之类的内部服务函数。
- 不依赖日志内容作为唯一断言。
- 对安全路径必须断言接触器 `OPEN`。
- 每个测试用例注释 `/* Verifies REQ-... */`，没有 REQ 时先补需求或标记为架构约束检视。

## 5. 推荐目录

```
tests/
  integration/
    db_diag_bms/
      CMakeLists.txt
      prj.conf
      testcase.yaml
      src/main.c
    task_pipeline/
      CMakeLists.txt
      prj.conf
      testcase.yaml
      src/main.c
```

## 6. CI 门

短期：

- 集成测试进入 Twister，但可先只跑 `native_sim`。
- 覆盖率纳入现有 app coverage 门。

中期：

- safety 相关集成测试失败即阻断合并。
- 对 `bms_task`、`bms_db`、`bms_diag`、`bms_bms` 设置最低覆盖率目标。

长期：

- HIL 冒烟测试作为非阻断观察门。
- 接真板稳定后，关键保护路径 HIL 升必过门。

## 7. 准出标准

一次涉及 engine/safety 的 PR 至少满足：

- 相关单元测试通过。
- 涉及 DB/DIAG/BMS/TASK 的改动有对应集成测试或明确替代验证。
- 失效安全路径至少有一条自动化测试覆盖。
- traceability 中能看到对应验证方法。
