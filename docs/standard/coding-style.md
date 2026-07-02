# BMS 固件 C 编码规范

> **文档定位**：本仓库 C（`.c/.h`）代码的编码规范，**指导性、规范性**文件。新代码须遵循，评审据此把关。
> 每个条目回答四件事：**出处 / 规定 / 为什么 / 怎么落实**。工具与 CI 为最终权威（见 `../CLAUDE.md`「质量门禁」）。
> 规则编号 `CS-NN` 稳定，可在评审与提交中引用。

## 1. 出处：规范从哪里来

| 来源 | 覆盖范围 | 性质 |
|---|---|---|
| **Zephyr / Linux 内核风格** | 命名、缩进、花括号、行宽、include 顺序 | 工具强制（`.clang-format` 为 Zephyr 上游原版、`.clang-tidy`） |
| **项目工程约定** | 文件头、接口文档、字面量、switch 纪律、入参检查 | 为可读性、可追溯、安全（safety-first） |
| **编辑器/卫生配置** | 文件名、编码、EOF、尾随空白、行尾 | `.editorconfig` + CI 卫生门 |

**优先级**（冲突时）：用户指令 / `CLAUDE.md` ＞ 工具配置（`.clang-format`/`.clang-tidy`/`.editorconfig`）＞ 本文档文字说明。

## 2. 命名

| 编号 | 规定 | 出处 | 为什么 | 怎么落实 |
|---|---|---|---|---|
| **CS-01** | 标识符用 `snake_case`：函数、变量、参数、struct/enum 类型名一律小写加下划线 | Zephyr 上游 | 与 Zephyr API 风格一致，降低阅读切换成本 | `.clang-tidy`（`*Case=lower_case`）+ 评审 |
| **CS-02** | 宏与枚举常量用 `UPPER_CASE` | Zephyr / C 传统 | 与可变实体在视觉上区分 | `.clang-tidy`（`MacroDefinitionCase/EnumConstantCase=UPPER_CASE`） |
| **CS-03** | 公开符号以模块前缀 `bms_`（及子模块名）开头 | 项目约定 | 避免全局命名冲突，符号可一眼定位归属模块 | 约定 + 评审 |
| **CS-04** | 表示物理量的变量/成员加单位后缀：`_mv`/`_ma`/`_dci`（0.1℃）等 | 项目约定 | 量纲自带，防止单位错配（mV 当 V 用等） | 约定 + 评审 |
| **CS-05** | 聚合类型不 `typedef`，直接用 `struct bms_xxx` / `enum bms_xxx` | Zephyr 上游 | 与 Zephyr 惯例一致，类型种类在使用处即可见 | `.clang-tidy`（`StructCase/EnumCase=lower_case`）+ 评审 |
| **CS-06** | 指针变量无匈牙利前缀；声明时 `*` 紧贴变量名 | Zephyr / clang-format | 命名表达用途而非类型，与内核风格一致 | clang-format（指针对齐）+ 评审 |

**CS-04 合法单位后缀**（物理量变量/成员专用；需新增量纲时在评审中扩充本表）：

| 后缀 | 量纲 | 示例 |
|---|---|---|
| `_mv` | 毫伏 mV | `cell_ov_mv` |
| `_ma` | 毫安 mA | `over_current_ma` |
| `_mah` | 毫安时 mAh | `pack_capacity_mah` |
| `_dci` | 0.1℃（deci-℃）| `over_temp_dci` |
| `_ms` | 毫秒 ms | `timestamp_ms` |
| `_permille` | ‰（千分比，SOC/SOH）| `soc_permille` |

## 3. 格式与缩进

| 编号 | 规定 | 出处 | 为什么 | 怎么落实 |
|---|---|---|---|---|
| **CS-07** | 缩进 8 列宽、使用 Tab（`IndentWidth: 8`、`UseTab: ForContinuationAndIndentation`） | `.clang-format`（Zephyr 上游） | 与 Zephyr 源码一致 | clang-format（CI `format` 门 + pre-commit 硬拒绝） |
| **CS-08** | 花括号 Linux 风格（`BreakBeforeBraces: Linux`）；**单语句块也强制花括号** | `.clang-format` | 防止后续误改引入悬空语句 | clang-format（`InsertBraces: true`） |
| **CS-09** | 行宽上限 100 列 | `.clang-format` | 防超宽行、便于并排阅读与 diff | clang-format（`ColumnLimit: 100`） |
| **CS-10** | `#include` 分组顺序：本地头 → C 标准库 → `zephyr/` → 其它；**只分组不重排** | `.clang-format` | 顺序稳定、可读 | clang-format（`IncludeCategories` + `SortIncludes: Never`） |
| **CS-11** | 仅用 ANSI-C 注释 `/* */`，不用 `//` | 项目约定 / C 传统 | 风格统一，避免行注释与块注释混用 | 约定 + 评审 |
| **CS-12** | 文件名小写；文件末尾单一空行；无尾随空格；行尾 LF | `.editorconfig` | diff 干净、跨平台一致、POSIX 合规 | `.editorconfig` + CI `editorconfig` 门 + `InsertNewlineAtEOF` |

## 4. 文件结构

| 编号 | 规定 | 出处 | 为什么 | 怎么落实 |
|---|---|---|---|---|
| **CS-13** | 每个 `.c/.h` 首行为 SPDX：`/* SPDX-License-Identifier: Apache-2.0 */` | 项目约定 | 许可可机读、合规审计友好 | CI 文件头卫生门（阻断）+ `scripts/check-file-headers.py` |
| **CS-14** | 紧随 SPDX 的文件级 doxygen：`@file`（= 文件名）、`@brief`（一句话职责）、`@ingroup`（模块域，见模板）；多行设计说明放 `@details` | 项目约定 | 供 doxygen 生成 API 文档；文件职责自解释 | CI 文件头卫生门（校验 `@file` 等于文件名、`@ingroup` ∈ 域集） |
| **CS-15** | 头文件用 include guard `BMS_<FILE>_H_`（紧随文件级 doxygen） | 项目约定 | 防止重复包含 | 约定（clang-tidy 忽略 `*_H_` 命名告警） |
| **CS-24** | `.c/.h` 用固定的**分节见出注释**划分功能块（foxBMS 风格），见出集合与顺序固定、见出文字原文照抄、空段亦保留见出行；`.c` 内实体按分节归位（含 `static`→`extern` 函数分区），并在 `Static Function Prototypes` 段**前置声明所有 `static` 函数** | 项目约定（参考 foxBMS 2） | 文件结构一眼可辨、跨模块布局一致、评审定位快；前置声明使实现顺序不影响编译，重排安全 | 约定 + 评审（模板见 §7.4） |

## 5. 注释与接口文档

| 编号 | 规定 | 出处 | 为什么 | 怎么落实 |
|---|---|---|---|---|
| **CS-16** | 公开函数加 doxygen：`@brief` 必备；返回非 `void` 必有 `@return`；指针参数用 `@param[in]`/`@param[out]`/`@param[in,out]` 标数据流向，**值传递参数不标方向**；可为 NULL 的入参须在描述中注明 | 项目约定 | 接口契约清晰，调用方无需读实现即可正确使用 | 评审（`-Wdocumentation` 自动门待定，见 `.clang-tidy` 注） |
| **CS-17** | `struct`/`enum` 的每个成员加 `/**< ... */` 行内 doxygen 注释 | 项目约定 | 数据结构语义自解释，避免误用字段 | 评审 |

## 6. 语言用法

| 编号 | 规定 | 出处 | 为什么 | 怎么落实 |
|---|---|---|---|---|
| **CS-18** | `switch` 必有 `default`；不写隐式穿透（每个 case 自终结）；`break` 与下一 `case` 间留空行 | 项目约定 | 防漏处理分支、防误穿透 | 编译器 `-Wswitch-default`/`-Wimplicit-fallthrough`（app `-Werror` 硬门）+ 评审 |
| **CS-19** | 十六进制字面量大写：`0xFF` 而非 `0xff` | 项目约定 | 可读、避免与标识符混淆 | 约定 + cppcheck |
| **CS-20** | 浮点字面量两侧都带数字（`1.0f` 而非 `1.f`/`.5f`）；优先用 `float` | 项目约定 | 防误读为整型；目标 MCU 有硬件 FPU，`float` 由硬件执行 | 评审 |
| **CS-21** | 一行只声明/初始化一个变量；尽量定义即初始化，否则注释说明原因 | 项目约定 | 避免使用未初始化值、便于 diff | 评审 |
| **CS-22** | 公开函数对指针入参必须做 NULL 检查，采用 **fail-safe 返回**（`if (p == NULL) return -EINVAL;` 或返回安全默认），**不使用会 halt 的断言**；可空入参在 doxygen 显式标注 | 安全要求（safety-first） | BMS 须优雅降级（如接触器默认 OPEN），不可因编程错误崩溃 | 评审 |
| **CS-23** | 标识符声明在尽可能窄的作用域；仅文件内使用的函数/变量声明为 `static` | Zephyr / 通用 | 限制可见性、降低耦合与误用 | clang-tidy + 评审 |

## 7. 模板

### 7.1 文件头（CS-13 + CS-14）

```c
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    protection.c
 * @brief   保护状态机模块：OV/UV/OC/OT 评估与接触器决策。
 * @ingroup PROT
 */
```

- `@ingroup` 取模块域：`SYS / AFE / SOC / PROT / BAL / COMM / BOARD`。**权威清单以 `traceability.md` 为准**；`scripts/check-file-headers.py` 的允许域集须与之同步。
- 若文件有多行设计说明，放在 `@ingroup` 后空一行的 `@details` 块中。
- 文件头与 include guard / 第一段 `#include` 间留一空行。

### 7.2 函数 doxygen（CS-16）

```c
/**
 * @brief 纯函数：根据测量与阈值评估保护状态（供线程与单测复用）。
 * @param[in]  meas   输入测量
 * @param[in]  limits 阈值
 * @param[out] out    输出保护事件（含期望接触器状态）
 * @return 0 成功，负值为 errno。
 */
int bms_protection_evaluate(const struct bms_cell_meas *meas,
                            const struct bms_prot_limits *limits, struct bms_prot_evt *out);
```

- 指针参数标方向、值参数不标；可空指针在描述中注明"可为 NULL"。
- 中文 `@brief` 可接受，但同一文件内保持语言一致。

### 7.3 写法对照（正 / 误）

**CS-18 `switch`**：

```c
/* 正确：每个 case 自终结（纯映射用 return 直出），default 必备 */
switch (state) {
case BMS_STATE_INIT:
    return BMS_STATE_STANDBY;
case BMS_STATE_NORMAL:
    return in->close_allowed ? BMS_STATE_NORMAL : BMS_STATE_STANDBY;
default:
    return BMS_STATE_LOCKED;
}

/* 错误：缺 default（-Wswitch-default 报错）；隐式穿透（-Wimplicit-fallthrough 报错）*/
switch (state) {
case A:
    do_a();      /* 落入 B：未标注的隐式穿透 */
case B:
    do_b();
    break;
}
```

**CS-20 浮点字面量**：

```c
float k = 1.0f; /* 正确：两侧带数字 + f 后缀 */
float k = 1.f;  /* 错误：小数点后无数字 */
float k = .5f;  /* 错误：小数点前无数字 */
```

**CS-22 入参检查（fail-safe，非断言）**：

```c
int bms_afe_sample(struct bms_cell_meas *out)
{
    if (out == NULL) {
        return -EINVAL; /* 正确：失效安全返回，调用方可处理 */
    }
    /* ... */
}

/* 避免：__ASSERT(out != NULL, ...) —— 触发 halt，违背 BMS 优雅降级 */
```

### 7.4 分节见出注释（CS-24）

见出行格式：`/*` + `==========`（10 个 `=`）+ ` ` + 见出文字 + ` ` + 补齐的 `=` + `*/`，整行**固定补齐到 80 列**（该见出为定宽装饰注释，与 CS-09 代码 100 列上限相互独立）。见出文字**原文照抄英文、不翻译、不改词序**；某段无内容时**仅保留见出行**（下空一行）。

**`.c` 见出集合与顺序（固定）：**

```c
/*========== Includes ========================================================*/
/*========== Macros and Definitions ==========================================*/
/*========== Static Constant and Variable Definitions ========================*/
/*========== Extern Constant and Variable Definitions ========================*/
/*========== Static Function Prototypes ======================================*/
/*========== Static Function Implementations =================================*/
/*========== Extern Function Implementations =================================*/
/*========== Externalized Static Function Implementations (Unit Test) ========*/
```

**`.h` 见出集合与顺序（固定，置于 include guard 与 `extern "C"` 之内）：**

```c
/*========== Includes ========================================================*/
/*========== Macros and Definitions ==========================================*/
/*========== Extern Constant and Variable Declarations =======================*/
/*========== Extern Function Prototypes ======================================*/
/*========== Externalized Static Function Prototypes (Unit Test) =============*/
```

**实体归位规则：**

- `LOG_MODULE_REGISTER` 归 **Macros and Definitions** 段顶（保留 Zephyr「紧跟 include」惯例）。
- 纯数据定义（`static` 变量、`static const` 表、`K_MUTEX_DEFINE`、`ZBUS_CHAN_DEFINE` 等）归 **Static Constant and Variable Definitions**。
- **Static Function Prototypes** 段**前置声明本文件全部 `static` 函数**（消除定义前引用的顺序依赖）；无 `static` 函数时保留空见出行。
- **所有 `static` 函数实现**归 **Static Function Implementations**；**所有非 `static`（extern）函数实现**归 **Extern Function Implementations**（即「先全 static、再全 extern」）。
- 引用函数的对象定义宏（`K_THREAD_DEFINE` 等）紧随其所引用函数之后放置。
- 见出**只组织、不改行为**：归位/重排属整形，须与功能改动**分开提交**。

> 上述见出行已按 80 列补齐，可直接复制；改宽度或文字后须重新补齐等号。

## 8. 强制层级一览

| 层级 | 覆盖条目 |
|---|---|
| `.clang-format`（CI `format` 门 + pre-commit） | CS-05/06、CS-07~CS-10 |
| `.clang-tidy`（CI 硬门） | CS-01/02/05、CS-23 |
| 编译器 `-Werror`（app，含 `-Wswitch-default`/`-Wimplicit-fallthrough`） | CS-18 |
| `.editorconfig`（CI `editorconfig` 门） | CS-12 |
| CI 文件头卫生门（`scripts/check-file-headers.py`） | CS-13/14 |
| cppcheck / SCA（观察 + 兜底） | CS-19 |
| 约定 + 代码评审 | CS-03/04、CS-11、CS-15~CS-17、CS-20~CS-22 |

> 工具未硬门禁的条目（CS-03/04、CS-11、CS-15~CS-17、CS-20~CS-22）由代码评审把关。
> **CS-16 自动门待定**：拟用 clang-tidy `-Wdocumentation`，但其参数注入在当前 CI 下报错（见 `.clang-tidy` 注），需本地 clang-tidy 环境调通后再启用。
