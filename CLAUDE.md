# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> 本文件是项目指南的**唯一来源**。workspace 根（`../CLAUDE.md`、`../.claude/CLAUDE.md`）与 `.claude/CLAUDE.md` 仅为指针。
> 下文命令默认在**本仓库目录 `bms-app/`** 下执行；涉及 workspace 根的用 `..\` 标注。

## 工作原则

- 执行动作时，输出每一步「做什么」及「为什么这么做」。
- 安全优先（safety-first）：保护逻辑默认失效安全（contactor 默认 OPEN），改动保护/阈值相关代码须格外谨慎。
- 质量优于速度：分层质量门禁不可绕过（见「质量门禁」）。

## 项目概览

基于 **Zephyr RTOS v4.4.0** 的电池管理系统（BMS）固件，CMake ≥ 3.20。采用 west **T2 拓扑**——本仓库（`bms-app/`）即 manifest 仓库（也是唯一的 git 仓库），Zephyr 与 HAL 作为依赖拉取到 workspace 上一级目录。

- **当前阶段**：在 PC 上用 **QEMU（`mps2/an386`，Cortex-M4F）** 跑通架构与业务骨架。选 `mps2/an386` 因其与目标 STM32F405 同为 Cortex-M4F（带硬件 FPU）。
- **下一步**：接入自定义 STM32F405 板 `boards/enervenue/bms_f405/`（当前为模板，dts/defconfig 待完善，CI 中暂注释）。
- `native_sim` 仅 Linux 可编译；Windows 上做 native_sim 须在 WSL2 下。

## Workspace 布局

```
bms-workspace/              # workspace 根（非 git 仓库）
├─ bms-app/                 # ★ 本仓库 = 应用 + manifest（git 仓库，所有开发在此）
├─ zephyr/  modules/        # west update 拉取的依赖（勿手改）
├─ .venv/                   # west / clang-format 所在的 Python venv
└─ run-tests-coverage.ps1   # Windows 上跑 twister + gcovr 覆盖率
```

本仓库内部：`app/`（应用）、`boards/`、`drivers/`（out-of-tree 驱动占位）、`tests/bms/`（ztest）、`docs/`、`scripts/`、`.github/workflows/`。

## 常用命令（Windows / PowerShell，在 `bms-app/` 下）

```powershell
# 首次：初始化 workspace
west init -l . ; west update ; west zephyr-export
pip install -r ..\zephyr\scripts\requirements.txt

# 构建并在 QEMU 运行
west build -p always -b mps2/an386 app
west build -t run                       # Ctrl-A X 退出 QEMU

# 单元测试（Windows 必须设 QEMU_BIN_PATH，否则 twister 只编译不运行）
$env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
west twister -T tests -p mps2/an386 -c  # 预期 11/11 通过

# 测试 + 覆盖率（封装上面流程，自动设置 QEMU_BIN_PATH；脚本在 workspace 根）
powershell -ExecutionPolicy Bypass -File ..\run-tests-coverage.ps1
#   -NoCoverage 只跑测试；-Board native_sim 换板

# 跑单个测试套件：限定 -T 路径
west twister -T tests/bms/soc -p mps2/an386 -c

# 提交前本地质量检查（镜像 CI 全部门禁）
powershell -ExecutionPolicy Bypass -File scripts\check.ps1
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 -Fast   # 仅 format+build+test

# 代码格式化（Zephyr clang-format，作用域 app/ drivers/ tests/）
powershell -ExecutionPolicy Bypass -File scripts\format.ps1          # 修正
powershell -ExecutionPolicy Bypass -File scripts\format.ps1 -Check   # 只检查
```

> **clang-format 优先用 venv 内的版本**（`..\.venv\Scripts\clang-format.exe`），保证与 CI（pin 22.1.5）一致。
> CI 在 Linux 用 `native_sim` 跑测试与覆盖率；Windows 本地用 `mps2/an386` + QEMU。

## 架构（big picture）

详见 `docs/architecture.md`。五个模块经 **zbus 消息总线** 完全解耦——模块间无直接函数调用、无编译期依赖，每个模块可由 `CONFIG_BMS_*` 单独开关。

### 数据流（每条 channel 单一发布者）

```
ADC/AFE → [afe] ─chan_cell_meas─┬→ [soc] ──chan_soc──┐
                                ├→ [protection] ─chan_prot_state─┤
                                ├→ [balancing] → GPIO bit-mask   ├→ [comm] → CAN/日志
                                └────────────────────────────────┘
```

| 模块 | 文件 | 职责 | 线程优先级 |
|------|------|------|-----------|
| afe | `app/src/bms/afe/` | 周期采样 cell 电压/电流/温度，发布 `chan_cell_meas` | 6 |
| soc | `app/src/bms/soc/` | 库仑计 SOC/SOH 估算，订阅 meas，发布 `chan_soc` | 7 |
| protection | `app/src/bms/protection/` | OV/UV/OC/OT 状态机 + 接触器控制（**最高优先级，安全关键**） | 4 |
| balancing | `app/src/bms/balancing/` | 单体均衡，算出需放电的 bit-mask | 7 |
| comm | `app/src/bms/comm/` | 快照各 channel 经 CAN 上报（native_sim 下打日志） | 8 |

- **线程模型**：`app/src/main.c` 按 Kconfig 调用各 `bms_*_init()`，模块用 `K_THREAD_DEFINE` 自启动工作线程（stack 1024）；main 之后每 5s 心跳。优先级顺序 protection(4) > afe(6) > soc/balancing(7) > comm(8)，确保安全决策先于上报。
- **zbus channel** 声明于 `app/include/bms/channels.h`，定义于 `app/src/bms/channels.c`：`chan_cell_meas` / `chan_soc` / `chan_prot_state`。
- **核心数据结构** 在 `app/include/bms/types.h`：`bms_cell_meas`（mV/mA/0.1°C）、`bms_soc`（permille 0–1000）、`bms_prot_evt`（state + contactor）。

### 可测试性约定（重要）

每个模块把核心逻辑放在**纯函数**里（无副作用），线程与单测共用：
- `bms_afe_sample()`、`bms_soc_estimate()` / `bms_soc_coulomb_step()`（带 `bms_soc_coulomb_state`）、`bms_protection_evaluate()`（接收 `bms_prot_limits`）、`bms_balancing_compute()`。
- 新增逻辑时延续此模式：纯函数 + 薄线程包装，便于 ztest 直接测纯函数。

### Kconfig 关键项（`app/Kconfig`，默认见 `app/prj.conf`）

`BMS_CELL_COUNT`(16) 与 `BMS_TEMP_SENSOR_COUNT`(4) 决定数组长度；`BMS_*_PERIOD_MS` 控制各周期（afe 100 / protection 50 / comm 200）；`BMS_SOC_PACK_CAPACITY_MAH`、`BMS_SOC_INIT_FROM_VOLTAGE` 等控制 SOC 行为。`prj.conf` 启用 `CONFIG_ZBUS=y`、`CONFIG_LOG`、`CONFIG_CBPRINTF_FP_SUPPORT`（浮点日志）。

## 测试（ztest）

套件位于 `tests/bms/<module>/`，每个含 `testcase.yaml`（`platform_allow: mps2/an386, native_sim` + tags）、`CMakeLists.txt`（链接被测的 `app/src/bms/<module>/*.c` 与 `app/include`）、`prj.conf`（`CONFIG_ZTEST=y`）、`src/main.c`。

- 套件命名 `ZTEST_SUITE(bms_<module>, ...)`，用例 `ZTEST(bms_<module>, test_<场景>_<预期>)`。
- 用例顶部用注释回链需求：`/* Verifies REQ-<域>-NNN */`。当前 soc 5 + protection 6 = 11 例；afe/balancing/comm 尚缺测试。

## 质量门禁（分层，不可绕过）

防御分层（轻在前、权威在后），CI 为最终权威：

1. **编辑器**：`.clang-format` + `.editorconfig` 实时。
2. **pre-commit**：仅对暂存的 app/drivers/tests 下 `.c/.h` 跑 clang-format（秒级，**硬拒绝**）。
3. **pre-push**：format + 增量 clang-tidy（有 build 时）+ cppcheck/MISRA（**warn-only**）。
4. **check.ps1**：本地全量镜像 CI。
5. **CI**（`.github/workflows/ci.yml`，PR/push 触发）：`format` → 并行 `build(mps2/an386)`、`build(native_sim)`、`test-coverage`、`sca-gcc`、`clang-tidy`，外加卫生门 `editorconfig`、`yamllint`（阻断作业，合并后入分支保护必过列）。覆盖率门槛 **行 ≥ 55% / 分支 ≥ 30%**（基线 61%/39%）。
6. **release**（`release.yml`，打 `vX.Y.Z` tag 触发）：校验 tag == `VERSION` → 构建 → 发布固件 + SHA256SUMS。

**首次克隆须启用 hooks**（`core.hooksPath` 是本地配置，不随提交携带）：
```powershell
git config core.hooksPath scripts/hooks   # 在 bms-app/ 下执行一次
```

- 重检查（clang-tidy/cppcheck）刻意不放进 pre-commit（会被绕过），改由 pre-push 轻量补 + CI 权威补。
- **clang-tidy 在 Windows 上标 SKIP**（需 native_sim host flags，Windows 无法配置）；要本地对齐请用 WSL2。
- cppcheck 的 **MISRA addon 不入库**，本地需 `bash scripts/setup-cppcheck-misra.sh` 拉取；噪声/deviation 在 `.cppcheck-suppressions` 维护。
- 配置文件：`.clang-format`（LLVM 基、100 列、8 空格缩进）、`.clang-tidy`（cert-* + readability-*，`WarningsAsErrors='*'`，仅分析 `app/include/bms/*.h`）。

## 开发流程与提交规范

方法论根基：`docs/development-methodology.md`（敏捷+V 研发方法论,一切流程由其衍生）。
操作权威：`docs/development-workflow.md`（务必先读）。

- **分支**：从最新 `master` 切出 `<type>/<kebab-描述>`（feat/fix/docs/ci/refactor/test/chore…）。
- **提交**：Conventional Commits——`<type>(<scope>): <祈使句摘要>`，scope 用模块名（soc/protection/afe/balancing/comm/board/ci/docs）。
- **PR**：`gh pr create --base master`，**Squash 合并**保持线性历史；CI 门禁须全绿。
- **版本**：SemVer 0.x（`0.MINOR.PATCH`）；发布时改 `VERSION` 与 `CHANGELOG.md` 后打 tag。
- 行尾由 `.gitattributes` 统一为 LF。

## Agent 协作（可选）

项目定义了端到端特性开发的子 agent 链（见 `docs/agents-guide.md`）：orchestrator → requirements → architect → designer →（coder ∥ tester）→ cicd。产物落在 `docs/features/<slug>/`（`00-iteration-plan` … `06-cicd`），并维护 `REQ-<域>-NNN` → `DES-<域>-NNN` → ztest 注释的可追溯链（域：SYS/AFE/SOC/PROT/BAL/COMM/BOARD）。
