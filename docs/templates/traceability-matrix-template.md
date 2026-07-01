<!--
  需求↔设计↔测试 追溯矩阵（活文档）。复制到 docs/work/traceability.md 维护。
  原则：每条 REQ 至少有一个验证手段；安全相关需求优先有自动化测试。
  状态：草稿 / 已实现 / 已验证 / 缺口。
-->
# 需求追溯矩阵

> 维护约定：新增/变更需求时同步更新本表；CI 通过即可把对应行标为「已验证」。
> 「缺口」= 尚无验证手段，需补测试或说明替代验证（分析/检视）。

| 需求 ID | 需求摘要 | 设计 | 验证方法 | 测试用例 | 状态 |
|---|---|---|---|---|---|
| REQ-\<域\>-001 | … | DES-\<域\>-001 | 测试 | `<套件>.<用例>` | 草稿 |
| REQ-\<域\>-002 | … | DES-\<域\>-001 | 分析 | —（见分析报告） | 缺口 |

## 示例（可删除）

| 需求 ID | 需求摘要 | 设计 | 验证方法 | 测试用例 | 状态 |
|---|---|---|---|---|---|
| REQ-PROT-001 | 单体过压 → 接触器 OPEN | DES-PROT-001 | 测试 | `bms.protection.test_overvoltage_opens` | 已验证 |
| REQ-PROT-002 | 欠压 → 接触器 OPEN | DES-PROT-001 | 测试 | `bms.protection.test_undervoltage_opens` | 已验证 |
| REQ-PROT-003 | 过流 → 接触器 OPEN | DES-PROT-001 | 测试 | `bms.protection.test_overcurrent_opens` | 已验证 |
| REQ-PROT-004 | 过温 → 接触器 OPEN | DES-PROT-001 | 测试 | `bms.protection.test_overtemp_opens` | 已验证 |
| REQ-PROT-005 | 正常 → 接触器 CLOSED | DES-PROT-001 | 测试 | `bms.protection.test_normal_closes_contactor` | 已验证 |
| REQ-SOC-001 | 满电压 → SOC 100% | DES-SOC-001 | 测试 | `bms.soc.test_full_charge` | 已验证 |
| REQ-SOC-002 | 空电压 → SOC 0% | DES-SOC-001 | 测试 | `bms.soc.test_empty` | 已验证 |
| REQ-AFE-001 | 周期采样写入 `DB_CELL_MEAS` | DES-AFE-001 | 测试 | —（待补 afe 单测） | 缺口 |

> 统计：示例中 7/8 已验证，AFE 采样为已知测试缺口（见 [../../TODO.md](../../TODO.md)）。
