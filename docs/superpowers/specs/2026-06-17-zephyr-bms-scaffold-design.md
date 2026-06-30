# 设计：Zephyr v4.4.0 BMS 项目脚手架

- 日期：2026-06-17
- 状态：已批准，实施中

## 目标

从零搭建基于 Zephyr v4.4.0 的 BMS 固件脚手架。先用 `native_sim` 跑通分层架构与业务骨架，第二步接入自定义 STM32F405 板。提供清晰分层、模块边界、可编译/可测试骨架，后续填实现。

## 决策记录

| 维度       | 决策                                                       |
|-----------|------------------------------------------------------------|
| 硬件       | 第一步 native_sim；第二步自定义 STM32F405 板                |
| west 拓扑  | T2，本仓库作为 manifest 仓库（self.path=bms-app）           |
| 范围       | 分层骨架 + 模块桩（空接口 + 线程框架）                       |
| BMS 模块   | afe、soc/soh、protection、balancing、comm(CAN)              |
| 模块解耦   | zbus 消息总线（channel 通信，无编译期互相依赖）              |
| 测试       | ztest，native_sim 上运行（soc、protection 两套示例）         |

## 架构

三层：应用层（main）→ BMS 服务层（5 模块，各独立线程）→ HAL/驱动层（Zephyr 内建 ADC/CAN/GPIO + out-of-tree AFE 占位）。模块间经 zbus channel：`chan_cell_meas`、`chan_soc`、`chan_prot_state`。详见 `docs/concept-architecture.md`。

## 模块统一形态

- 公共头 `app/include/bms/<m>.h`：`int bms_<m>_init(void);` + 极简 API。
- 实现 `app/src/bms/<m>/<m>.c`：`LOG_MODULE_REGISTER`、`K_THREAD_DEFINE` 线程、zbus 订阅/发布、桩逻辑、TODO 标注。
- Kconfig 开关 `BMS_<M>`，模块可裁剪。

## 失效安全

protection 上电默认安全态（接触器断开），仅全部正常才闭合。

## 测试

- `tests/bms/soc`：构造测量值，断言 SOC 输出范围/合理性。
- `tests/bms/protection`：构造过压/欠压/过流/过温，断言进入对应状态且输出安全态。

## 环境前提

本机暂无 Python/west/Zephyr SDK（有 cmake/ninja）。脚手架文件可直接创建；`west update` 与编译需用户先装工具链（README 已写步骤）。

## 范围之外（YAGNI）

- 真实 SOC 算法、真实 AFE 驱动、真实 CAN 协议矩阵——仅留接口与桩。
- 第二步不绑定真实引脚——board 引脚留 TODO。
