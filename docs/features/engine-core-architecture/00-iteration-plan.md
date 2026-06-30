# 迭代计划：Engine Core 架构证据链

> 特性 slug：`engine-core-architecture`
> 范围：`bms_task` / `bms_db` / `bms_diag` / `bms_bms`
> 目标：为 foxBMS 2 inspired engine 核心补齐需求、设计、追溯，并为后续集成测试提供验收基线。

## 1. 背景

项目架构已从“模块各自线程 + zbus 为中心”转向 foxBMS 2 inspired 的 engine 架构：

- `bms_task`：统一任务框架。
- `bms_db`：typed snapshot database。
- `bms_diag`：诊断中心。
- `bms_bms`：主状态机与接触器允许条件。

这些模块已在代码中形成骨架，但还缺正式 `REQ → DES → TEST` 证据链。该缺口已登记在 [../../../TODO.md](../../../TODO.md)。

## 2. 本迭代目标

- 为 engine core 建立第一版需求规格。
- 定义 engine core 架构与详细设计约束。
- 建立 traceability 矩阵，标出当前已实现项与测试缺口。
- 明确下一步集成测试主题：`DB→DIAG→BMS→contactor OPEN/NORMAL`。

## 3. 非目标

- 不在本迭代实现新的生产代码。
- 不接入真实接触器 GPIO / 预充 / WDT。
- 不完成 HIL、FMEA、MC-DC。
- 不替换现有单元测试结构。

## 4. 交付物

| 文件 | 作用 |
|------|------|
| `01-requirements.md` | Engine core 需求 |
| `02-architecture.md` | Engine core 架构决策 |
| `03-design.md` | DB/DIAG/BMS/TASK 详细设计 |
| `traceability.md` | 需求-设计-测试追溯 |

## 5. DoR / DoD

DoR：

- `concept-architecture.md` 已定义 engine 目标架构。
- `standard-module-interface.md` 已定义模块接口标准。
- 当前代码已有 `bms_task` / `bms_db` / `bms_diag` / `bms_bms` 骨架。

DoD：

- 每条 `REQ-ENG-*` 至少映射一个 `DES-ENG-*`。
- 每条需求有验证方法，自动化测试缺口必须显式标注。
- 安全相关需求必须明确失效安全默认态。
- 后续集成测试主题可直接从 traceability 中导出。

