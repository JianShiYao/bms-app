# BMS 安全概念（轻量初版）

> **文档定位**：本项目的**安全概念（safety concept）**——[methodology.md](methodology.md) §6.1「风险分析与安全需求」要求的产物。它为"安全案例驱动"方法论提供地基：把**危害 → 安全目标 → 安全功能/需求 → 代码/测试**的链条显式化，使下游需求、保护设计、测试强度都能回指安全目标。
> **状态**：**轻量初版（draft）**。按 methodology §6.1「早期可轻量，但必须能追溯到安全需求」。本版给出危害清单与安全目标的**首个基线**；HARA 风险定级（S/E/C → ASIL 或等价）、FMEA/FTA、保护响应时间预算等**重型分析**待接真板、明确安全目标后按 methodology §6 补强。
> **依据来源**：危害与阈值取材于既有保护需求 [requirements/prot.md](../reference/legacy-requirements/prot.md)（逆向自 S16100B，**参考性**）与跨域风险清单 [requirements/risks-and-gaps.md](../reference/legacy-requirements/risks-and-gaps.md)；安全态/状态机取自 [architecture.md](architecture.md)。权威约定见 [../CLAUDE.md](../../CLAUDE.md)。

## 1. 范围与 Item 定义

- **被保护对象**：锂电池 Pack（多串电芯 + 模组），及其充放电能量路径。
- **BMS 的安全职责**：在电芯/Pack 进入危险工况（过压/过流/过温/短路等）时，**断开能量路径（接触器 / MOS）使系统回到安全态**，并对外可观测地上报。
- **执行器（安全相关输出）**：主接触器 / 充电路 / 放电路 MOS、预充回路。
- **传感输入**：单体/总电压、电流、温度（电芯/MOS/环境）、AFE 硬件保护标志（OCD2/短路）、进水/绝缘相关 ADC、接触器/MOS 状态反馈。
- **边界外**（本概念不覆盖、由系统/PCS 承担）：高压互锁 HVIL 物理设计、绝缘监测硬件、Pack 机械与泄压设计、充电机本体安全。
- **当前实现范围**：`native_sim`/QEMU 业务骨架（`afe_sim` 数据源）；目标板 `bms_f405` 未 bring-up；`bms-app` 的保护当前为 OV/UV/OC/OT 评估骨架（阈值为桩，见 [architecture.md](architecture.md)）。本概念覆盖**目标安全范围**，并逐条标注当前实现状态。

## 2. 运行模式与安全态

| BMS 状态 | 接触器 | 说明 |
|---|---|---|
| INIT / STANDBY | OPEN | 上电/待机，默认断开 |
| PRECHARGE | 预充回路 | 受控预充，主接触器未闭合 |
| NORMAL | CLOSED | 唯一允许闭合的状态 |
| FAULT | OPEN | 故障，回安全态 |
| LOCKED | OPEN | 锁存故障，需外部干预 |

- **失效安全默认态 = 接触器 OPEN**（methodology 原则4）。仅 `NORMAL` 闭合；任何不确定（采样无效、保护非 NORMAL、断开请求、诊断达 ERROR/CRITICAL）一律转 FAULT/LOCKED → OPEN。
- **路径粒度安全态**：充电类危害断**充电路径**、放电类断**放电路径**、系统类危害断**全部**（对应既有 `CHG_FAULT/DISC_FAULT/SYS_FAULT`，见 REQ-PROT-002）。`bms-app` 骨架当前为单一接触器 OPEN/CLOSED，路径粒度待硬件抽象补齐。

## 3. 危害清单（HAZ）

> 安全相关：均为 ⚠️。后果为定性描述；S/E/C 定级与 ASIL 等价评估待 HARA（§7）。

| ID | 危害 | 触发 / 失效模式 | 潜在后果 |
|---|---|---|---|
| HAZ-01 | 电芯/总压**过充** | OV，充电不停 | 析锂、产气、起火/爆炸 |
| HAZ-02 | 电芯/总压**过放** | UV，放电不停 | 容量损伤、铜枝晶内短 |
| HAZ-03 | **过流**（充/放） | OC，电流超限 | 过热、起火 |
| HAZ-04 | **短路** | 外短/内短，AFE 短路标志 | 瞬时大能量释放、起火 |
| HAZ-05 | **过温**（电芯/MOS/环境） | OT | 加速老化、触发热失控 |
| HAZ-06 | **低温充电** | 欠温仍充电 | 析锂 → 内短 |
| HAZ-07 | **热失控** | 高温 + 环境异常并发 | 起火/爆炸、蔓延 |
| HAZ-08 | **采样失效** | 卡死/漂移/越界/AFE 通信异常 | 误判/漏判保护，保护完整性丧失 |
| HAZ-09 | **执行器失效** | 接触器/MOS 粘连(不能断)或断路(不能合) | 无法进入安全态 |
| HAZ-10 | **预充失败** | 预充未达目标即闭合 | 涌流、拉弧、器件损伤 |
| HAZ-11 | **控制器失活** | 死锁/跑飞、看门狗未启用 | 保护停摆，危害不被响应 |
| HAZ-12 | **进水 / 绝缘失效** | 进水 ADC 触发 | 短路、电击 |
| HAZ-13 | **参数 / NVM 损坏** | Flash 损坏、校准未加载 | 用错保护阈值，保护失准 |
| HAZ-14 | **安全指令被覆盖 / 通信丢失** | 上位机 Force_On 覆盖故障断开；CAN 丢失 | 故障时该路不断开 |

## 4. 安全目标（SG）

| ID | 安全目标 | 关联危害 | 安全态 | 建议验证强度 |
|---|---|---|---|---|
| SG-01 | 检测过充并断开**充电路径** | HAZ-01 | 充电路 OPEN | 边界 + 故障注入 + 高覆盖 |
| SG-02 | 检测过放并断开**放电路径** | HAZ-02 | 放电路 OPEN | 边界 + 故障注入 |
| SG-03 | 检测过流/短路并断开相应/**全部**路径 | HAZ-03,04 | 短路→全 OPEN | 边界 + 故障注入 + 响应时间 |
| SG-04 | 检测过温并断开能量路径、抑制热失控 | HAZ-05,07 | 全 OPEN | 边界 + 故障注入 |
| SG-05 | 低温**禁充** | HAZ-06 | 充电路 OPEN | 边界（温度×电流象限） |
| SG-06 | **测量可信化**：采样无效不得据其闭合 | HAZ-08 | 维持/回 OPEN | 等价类 + 坏帧注入（已实现，见 §5） |
| SG-07 | **执行器完整性**：检测粘连/断路并报故障 | HAZ-09 | 进 FAULT/LOCKED | 故障注入 + 反馈对比 |
| SG-08 | **受控预充**：未达标不闭合主接触器 | HAZ-10 | 主接触器 OPEN | 时序 + 故障注入 |
| SG-09 | **控制器存活**：死锁经看门狗硬复位到安全态 | HAZ-11 | 复位 → OPEN | 注入死锁 + watchdog 验证 |
| SG-10 | 进水/绝缘失效 → 进安全态 | HAZ-12 | 全 OPEN | 阈值 + 检视 |
| SG-11 | **参数完整性**：NVM 损坏回退安全默认阈值 | HAZ-13 | 安全默认参数 | CRC + 回退测试 |
| SG-12 | **安全优先于远程指令**：故障断开不可被远程 Force_On 覆盖 | HAZ-14 | 故障强制 OPEN | 检视 + 测试 |

## 5. 安全目标 → 安全功能 / 需求 追溯

> 闭合 methodology 原则3 的链：`SG → 安全需求 → 代码 → 测试`。「当前实现」列反映 `bms-app` 骨架现状（区别于 `reference/legacy-requirements/` 中 S16100B 参考需求）。
> ⚠️ **勿误读**：「安全需求（参考/目标）」列的 `REQ-PROT-NNN` 等来自 `reference/legacy-requirements/`（S16100B 逆向），标示"目标应覆盖什么"，**非活仓库已承诺需求**；活仓库承诺现状以 [work/traceability.md](../work/traceability.md) 为准。只有「bms-app 当前实现」列代表已落地行为，"未实现"即尚未承诺落地。

| SG | 安全需求（参考/目标） | bms-app 当前实现 | 测试 |
|---|---|---|---|
| SG-01 | REQ-PROT-003/005 | `bms_protection_evaluate` OV（单接触器） | `tests/bms/measurement-control/protection` |
| SG-02 | REQ-PROT-004/006 | `bms_protection_evaluate` UV | `tests/bms/measurement-control/protection` |
| SG-03 | REQ-PROT-011/012/013/014 | OC（绝对值）；短路/二级过流**未实现** | 部分 |
| SG-04 | REQ-PROT-008/015/017 | OT（单体）；MOS/环境温**未实现** | 部分 |
| SG-05 | REQ-PROT-009 | **未实现** | — |
| SG-06 | architecture「测量数据纪律」 | `bms_afe_validate` + `bms_protection_evaluate`：validity 不全 → FAULT/OPEN | `tests/bms/afe/afe`、measurement-control/protection |
| SG-07 | REQ-PROT-023/024/025/026 | **未实现**（MOS 失效检测） | — |
| SG-08 | REQ-PROT-022 | PRECHARGE 状态骨架已实现（`bms_next_state`：complete→NORMAL / timeout→FAULT / 撤销→STANDBY）；**执行经 AFE 预充待 M6**（bms_f405 经 SH3673520 SPI） | `tests/bms/application/state` |
| SG-09 | REQ-PWR-024（看门狗） | 喂狗门控策略已实现（`bms_sys_mon`：仅安全关键任务健康才喂）；**IWDG 接线待 M6** | `tests/bms/engine/sys_mon_wdt` |
| SG-10 | REQ-PROT-021 | **未实现** | — |
| SG-11 | REQ-PROT-030 | **未实现**（无 NVM/参数管理） | — |
| SG-12 | REQ-CTRL-017/020 | `bms_next_state` 故障强制 FAULT；远程覆盖路径未引入 | `tests/bms`（状态机） |

> **核心已落地的安全机制**：失效安全默认态（接触器 OPEN）、保护线程最高优先级、测量可信化（SG-06）、状态机故障收敛（FAULT/LOCKED）。其余多为目标范围，随模块补全与真板 bring-up 推进。

## 6. 已知安全缺口 / 风险接受待裁（来自 risks-and-gaps）

下列为**需产品/安全负责人裁定**的开放项（非本概念新发现，集中登记以纳入安全案例）：

| 风险 | 关联 SG / HAZ | 摘要 |
|---|---|---|
| S1 看门狗未启用 | SG-09 / HAZ-11 | `MX_IWDG_Init()` 注释，死锁无硬件恢复 |
| S2 升级期保护盲区 | SG-01~04 / 多 | OTA `while(1)` 独占，保护停摆 |
| S3 远程 Force_On 覆盖故障断开 | SG-12 / HAZ-14 | 故障时该路可能不断开 |
| S9 热失控无恢复/无日志 | SG-04 / HAZ-07 | 触发后永久保持、无锁存日志 |
| S10 SOH 下限保护注释、除零风险 | — | 容量异常 → SOC 溢出 |

> 上述源自 S16100B 既有固件分析；新架构 `bms-app` 在实现对应安全功能时**必须显式规避**，并在对应 SG 的证据包中记录处置（修复/缓解/风险接受）。

## 7. 验证强度与后续（对接 methodology §6.3）

- **安全相关功能**按 methodology §6.3 阶梯推进：`unit → native_sim/QEMU → SIL/PIL → HIL/bench`。
- 高风险目标（SG-01~04）至少要求：**边界值 + 等价类 + 坏数据/故障注入 + 失效安全默认态验证**；接真板后补**保护响应时间**与 HIL 故障注入；视最终安全等级决定是否对保护判定路径要求 **MC-DC** 覆盖。
- **本概念的后续重型化**（触发：真板 bring-up / 客户审核 / 明确安全目标）：HARA 风险定级（S/E/C）、FMEA/FTA、安全需求元数据补全（methodology §6.2）、保护响应时间预算、安全发布基线（§6.6）。

## 8. 维护（治理）

- **Owner**：[.github/CODEOWNERS](../../.github/CODEOWNERS)（当前单人项目即维护者）。
- **版本**：`v0.1`（轻量初版，2026-06-30）。危害/安全目标（§3·§4）变更升次版本，追溯/注脚补充升修订。
- **变更流程**：标准 docs PR（从 `master` 切 `docs/<kebab>` → PR → 评审 → Squash）。
- **再基线**：新增/变更安全相关需求时，须回链到本文某条 SG；新增危害须评估是否派生新 SG 与安全功能（methodology 原则6/7）。
