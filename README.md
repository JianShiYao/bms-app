# BMS 固件项目（Zephyr v4.4.0）

基于 **Zephyr RTOS v4.4.0** 的电池管理系统（BMS）固件。采用 west **T2 拓扑**（本仓库即 manifest 仓库），分层架构 + zbus 消息总线解耦。

- 第一步：在 PC 上用 **QEMU（`mps2/an386`，Cortex-M4F）** 跑通架构与业务骨架。
  - 选 `mps2/an386` 是因为它与目标 STM32F405 同为 **Cortex-M4F（带硬件 FPU）**，架构忠实。
  - 注：`native_sim` 的 POSIX 架构仅支持 Linux（见 `zephyr/arch/posix/CMakeLists.txt`），
    Windows 上无法编译；如需 native_sim 请在 WSL2/Linux 下使用。
- 第二步：接入自定义 STM32F405 板（`boards/enervenue/bms_f405/`）。

## 目录结构

```
bms-app/                     # 本仓库（west manifest 仓库）
├─ west.yml                  # manifest：pin zephyr v4.4.0 + HAL
├─ app/                      # BMS 应用
│  ├─ include/bms/           # 模块公共接口（头文件）
│  └─ src/bms/{afe,soc,protection,balancing,comm}/  # 模块桩实现
├─ boards/enervenue/bms_f405/   # 第二步：自定义 STM32F405 板（模板）
├─ drivers/                  # out-of-tree 驱动占位（如 AFE 芯片）
├─ tests/bms/{soc,protection}/  # ztest 单测套件
└─ docs/architecture.md      # 架构与数据流说明
```

## 一、安装工具链（首次）

> 官方指南：https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html

1. 安装 **Python 3.10–3.12**（勾选 Add to PATH），以及 **CMake ≥ 3.20**、**Ninja**、**Git**。
2. 创建虚拟环境并安装 west：
   ```powershell
   python -m venv .venv
   .\.venv\Scripts\Activate.ps1
   pip install west
   ```
3. 安装 **Zephyr SDK**（版本由 `zephyr/SDK_VERSION` 决定，当前为 **1.0.1**）。
   workspace 初始化（第二步 `west update`）后，用 west 自动安装：
   ```powershell
   # 仅装 ARM 工具链 + host-tools（含 QEMU），版本自动匹配 SDK_VERSION
   west sdk install --install-base D:\zephyr-sdk -t arm-zephyr-eabi
   west sdk list        # 确认 arm-zephyr-eabi 与 hosttools 已安装
   ```

## 二、初始化 workspace（T2）

在**本仓库目录**下执行：

```powershell
# 以本仓库为 manifest 初始化 workspace（workspace 根为上一级目录）
west init -l .
# 拉取 zephyr v4.4.0 与所需模块
west update
# 导出 Zephyr CMake 包，安装 Python 依赖
west zephyr-export
pip install -r ..\zephyr\scripts\requirements.txt
```

## 三、构建与运行（QEMU / mps2/an386，Cortex-M4F）

```powershell
# 编译（首次会编译整个 Zephyr，约 1-2 分钟）
west build -p always -b mps2/an386 app
# 在 QEMU 中运行（看到各 BMS 线程启动与周期采样桩日志；Ctrl-A X 退出 QEMU）
west build -t run
```

> 启动日志示例：`*** Booting Zephyr OS build v4.4.0 ***` →
> `bms_main: ==== BMS firmware starting on mps2/an386 ====` → 各模块 init → 每 5s 心跳。

## 四、运行单元测试（ztest on mps2/an386）

> **Windows 必读**：twister 在 Windows 上只有设置了 `QEMU_BIN_PATH` 环境变量才会
> 真正在 QEMU 中执行测试（否则只编译、显示 “built (not run)”）。该变量指向 Zephyr
> SDK 自带的 QEMU 目录。

```powershell
# 指向 SDK 内置 QEMU（每个新终端都要设；可在系统环境变量中永久设置）
$env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
west twister -T tests -p mps2/an386 -c
```

预期：`11 of 11 executed test cases passed (100.00%)`（bms_soc 5 + bms_protection 6）。

## 五、第二步：STM32F405 板

板定义位于 `boards/enervenue/bms_f405/`（当前为模板，引脚标注 TODO）。完善 dts/defconfig 后：

```powershell
west build -b bms_f405 app
```

## 架构

详见 [docs/architecture.md](docs/architecture.md)。设计文档见 `docs/superpowers/specs/`。
