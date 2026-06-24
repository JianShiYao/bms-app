# 需求工程模板

本目录提供从**需求 → 设计 → 测试 → 追溯**的轻量模板，用于落地
[质量管控全景](../quality-management.md)中标注的「需求管理 / 验收标准 / 需求↔测试追溯」。

> 本套 ID / 追溯规范是研发方法论**原则3(可追溯性)**的落地;方法论根基见 [../development-methodology.md](../development-methodology.md)。

## 模板清单

| 模板 | 用途 |
|---|---|
| [requirements-template.md](requirements-template.md) | 需求规格（含验收标准、验证方法、追溯字段），EARS 句式 |
| [design-spec-template.md](design-spec-template.md) | 设计/规格文档（关联需求、接口、失效处理、风险） |
| [traceability-matrix-template.md](traceability-matrix-template.md) | 需求↔设计↔测试 追溯矩阵（活文档） |

## ID 命名规范

- 需求：`REQ-<域>-<NNN>`，设计：`DES-<域>-<NNN>`。
- 域：`SYS`(系统) / `AFE`(采样) / `SOC` / `PROT`(保护) / `BAL`(均衡) / `COMM`(通信) / `BOARD`。
- 例：`REQ-PROT-001`、`DES-SOC-002`。

## 工作流

1. **写需求**：复制 requirements-template，按 EARS 句式写，给出**可度量的验收标准**与**验证方法**。
2. **写设计**：复制 design-spec-template，在 `满足需求` 字段写明覆盖了哪些 `REQ-*`。
3. **实现 + 测试**：在 ztest 用例里**用注释标注覆盖的需求 ID**，并让测试名可读地对应需求。约定：
   ```c
   /* Verifies REQ-PROT-001: cell over-voltage opens the contactor. */
   ZTEST(bms_protection, test_overvoltage_opens) { ... }
   ```
4. **更新追溯矩阵**：在 traceability-matrix 里为每条 `REQ-*` 填上设计、测试用例、状态。
   原则：**每条需求都应可追溯到一个验证手段**（测试/分析/检视/演示）；安全相关需求优先有自动化测试。

## 与现有体系的关系

- 验证手段落到 CI：测试由 [twister](../development-workflow.md) 跑、覆盖率门禁见 [quality-management](../quality-management.md)。
- 安全相关需求（保护/接触器路径）应有高覆盖率，见 quality-management 的「功能安全视角」。
- 这些模板是**过程脚手架**，非强制门禁；随项目接入真实硬件/认证目标再决定严格程度。
