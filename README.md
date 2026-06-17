# BMS 固件项目（Zephyr v4.4.0）

基于 **Zephyr RTOS v4.4.0** 的电池管理系统（BMS）固件。采用 west **T2 拓扑**（本仓库即 manifest 仓库），分层架构 + zbus 消息总线解耦。

- 第一步：在 PC 上用 `native_sim` 跑通架构与业务骨架。
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

> 本机当前尚未安装 Python / west / Zephyr SDK。请先完成以下安装。
> 官方指南：https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html

1. 安装 **Python 3.10+**（勾选 Add to PATH），以及 **CMake ≥ 3.20**、**Ninja**、**Git**。
2. 创建虚拟环境并安装 west：
   ```powershell
   python -m venv .venv
   .\.venv\Scripts\Activate.ps1
   pip install west
   ```
3. 安装 **Zephyr SDK 0.17.x**（含 GCC 工具链与 host 工具）。

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

## 三、构建与运行（native_sim）

```powershell
west build -b native_sim app
# 运行仿真可执行（看到各 BMS 线程启动与周期采样桩日志）
.\build\zephyr\zephyr.exe
```

## 四、运行单元测试（ztest on native_sim）

```powershell
west twister -T tests -p native_sim
```

## 五、第二步：STM32F405 板

板定义位于 `boards/enervenue/bms_f405/`（当前为模板，引脚标注 TODO）。完善 dts/defconfig 后：

```powershell
west build -b bms_f405 app
```

## 架构

详见 [docs/architecture.md](docs/architecture.md)。设计文档见 `docs/superpowers/specs/`。
