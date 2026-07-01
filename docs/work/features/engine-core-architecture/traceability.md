# Engine Core 追溯矩阵

> 状态说明：本特性当前已补首批 `DB→DIAG→BMS` 集成测试；Windows 本地 `mps2/an386`
> 为 built-only，`native_sim` 被静态过滤，因此状态先记为“部分”，待 CI/WSL 可执行平台实际运行后再升为“已验证”。

| 需求 ID | 需求摘要 | 设计 | 验证方法 | 测试用例 | 状态 |
|---|---|---|---|---|---|
| REQ-ENG-001 | database typed snapshot 交换 | DES-ENG-001 | 测试 | `bms.integration.test_db_write_read_snapshot` | 部分 |
| REQ-ENG-002 | database entry 单一写入者 | DES-ENG-001 | 检视 | — | 已实现 / 待检视 |
| REQ-ENG-003 | 诊断中心聚合故障 | DES-ENG-002 | 测试 | `bms.integration.test_diag_error_blocks_normal` | 部分 |
| REQ-ENG-004 | BMS 主状态机集中决定接触器期望态 | DES-ENG-003 | 测试 | `bms.integration.test_bms_fault_opens_contactor` | 部分 |
| REQ-ENG-005 | 失效安全默认态 | DES-ENG-003 | 测试 | `bms.integration.test_bms_default_open` | 部分 |
| REQ-ENG-006 | 任务框架统一调度长期运行逻辑 | DES-ENG-004 | 检视 / 集成测试 | `bms.integration.test_task_pipeline_smoke`（待补） | 已实现 / 待验证 |
| REQ-ENG-007 | 兼容 zbus 过渡层 | DES-ENG-005 | 构建 / 检视 | 现有 `bms.*` 单测构建 | 已实现 |

## 下一步测试清单

已补 `tests/integration/db_diag_bms/`：

- `test_db_write_read_snapshot`
- `test_diag_error_blocks_normal`
- `test_bms_default_open`
- `test_bms_fault_opens_contactor`

继续补 `tests/integration/task_pipeline/`：

- `test_task_pipeline_smoke`
