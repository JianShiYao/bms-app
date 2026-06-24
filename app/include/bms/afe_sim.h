/*
 * AFE 仿真后端（afe_sim）—— 纯函数核心接口
 *
 * 设计来源：docs/architecture.md「数据源后端可切换（afe）」。
 * afe 的「采样实现」与「业务逻辑」分离：本后端在 QEMU / native_sim（无真实
 * ADC/AFE 芯片）下产出"会动"的可复现测量，使 soc / protection / balancing
 * 拿到真实变化的输入；真机后端见 afe_adc.c。
 *
 * 延续项目「纯函数 + 薄线程」约定（CLAUDE.md「可测试性约定」）：
 * 仿真模型核心 bms_afe_sim_step() 为有状态纯函数——时钟由入参注入、状态由
 * 调用方持有，无 k_uptime/全局副作用，故对 ztest 完全确定可测；afe_sim.c 的
 * bms_afe_backend_read() 仅做「取 k_uptime + 持有 static 状态」的薄包装。
 */
#ifndef BMS_AFE_SIM_H_
#define BMS_AFE_SIM_H_

#include <stdint.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 仿真后端跨帧状态（后端私有；不进 types.h，非 zbus 载荷）。
 *
 * 不变式（invariant）：
 *  - soc_permille ∈ [0,1000]（充放电积分后恒夹紧）。
 *  - last_ms == 0 视为"首帧"：本帧 Δt 记为 0、不积分（避免上电首帧跳变）。
 *  - lcg 为伪随机噪声种子，仅影响叠加在测量上的小幅噪声；同一初值 + 同一
 *    (now_ms) 序列 → 逐位可复现（确定性，供单测断言）。
 *
 * 以 static 实例化时位于 BSS（全零）→ 等价于"未复位首帧"，但 soc_permille=0、
 * 种子=0；正常路径应在 init 时调 bms_afe_sim_state_reset() 取确定起点。
 */
struct bms_afe_sim_state {
	int32_t soc_permille; /**< 仿真荷电状态 ‰，恒夹紧于 [0,1000]。 */
	uint32_t last_ms;     /**< 上一步进时刻（ms）；0=首帧。 */
	uint32_t lcg;         /**< LCG 伪随机噪声种子。 */
};

/**
 * @brief 复位仿真状态到确定起点（便于上电/单测）。
 *
 * 置 soc_permille=500（50%）、last_ms=0（下次为首帧）、lcg=固定种子。
 * @param st 为 NULL 时安全返回（无操作）。
 */
void bms_afe_sim_state_reset(struct bms_afe_sim_state *st);

/**
 * @brief 仿真步进核心（有状态纯函数，供薄线程与单测复用）。
 *
 * 模型（全整数运算，单位与 struct bms_cell_meas 一致）：
 *  1) 电流：以 now_ms 驱动三角形充放电循环（前半周期 +充 / 后半周期 -放）+ 小噪声。
 *  2) SOC ：库仑积分 ΔSOC(‰) = I(mA)·Δt(ms) / (容量(mAh)×3600)，夹紧 [0,1000]。
 *  3) 电压：SOC 线性映射 OCV（3000mV 空 ~ 4200mV 满）+ 内阻压降 I·R + 串间偏移 + 小噪声。
 *  4) 温度：基础 25.0℃ + 随 |电流| 升温 + 小噪声。
 * 首帧（st->last_ms==0）Δt=0：只产出电压/温度快照，不积分。
 *
 * @param st     跨帧状态（非空，调用方持有，单测可栈上构造）。本函数为其唯一读写入口。
 * @param now_ms 当前时刻（ms，由调用方注入；线程传 k_uptime_get_32()）。
 * @param out    输出测量（非空）；out->timestamp_ms 置为 now_ms。
 * @return 0 成功；-EINVAL（st 或 out 为 NULL，不触状态、不写 out）。
 */
int bms_afe_sim_step(struct bms_afe_sim_state *st, uint32_t now_ms, struct bms_cell_meas *out);

/*
 * 注：后端无关的 bms_afe_backend_read() seam 声明在 bms/afe.h（afe_sim.c 在此实现其仿真版本）。
 */

#ifdef __cplusplus
}
#endif

#endif /* BMS_AFE_SIM_H_ */
