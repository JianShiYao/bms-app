/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    db.h
 * @brief   BMS 数据库（data-exchange）门面接口。
 * @ingroup SYS
 *
 * @details foxBMS 2 inspired data exchange center: modules write/read typed snapshots
 *          through this API instead of depending on each other's threads.
 */

#ifndef BMS_DB_H_
#define BMS_DB_H_

#include <stdbool.h>
#include <stdint.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 数据槽位元数据（随快照一同返回）。 */
struct bms_db_meta {
	uint32_t sequence; /**< 写入序号，每次成功写入自增（用于检测新数据） */
	bool valid;        /**< 该槽位是否已写入过有效数据 */
};

/**
 * @brief 初始化数据库：清零所有快照槽位与元数据（线程安全）。
 * @return 0 成功。
 */
int bms_db_init(void);

/**
 * @brief 写入最新电芯测量快照（线程安全）。
 * @param[in] meas 待写入测量（非空）。
 * @return 0 成功；@p meas 为 NULL 返回 -EINVAL。
 */
int bms_db_write_cell_meas(const struct bms_cell_meas *meas);

/**
 * @brief 读取最新电芯测量快照（线程安全）。
 * @param[out] meas 输出测量（非空）。
 * @param[out] meta 输出槽位元数据（可为 NULL，此时不回填）。
 * @return 0 成功；@p meas 为 NULL 返回 -EINVAL。
 */
int bms_db_read_cell_meas(struct bms_cell_meas *meas, struct bms_db_meta *meta);

/**
 * @brief 写入最新 SOC/SOH 快照（线程安全）。
 * @param[in] soc 待写入 SOC（非空）。
 * @return 0 成功；@p soc 为 NULL 返回 -EINVAL。
 */
int bms_db_write_soc(const struct bms_soc *soc);

/**
 * @brief 读取最新 SOC/SOH 快照（线程安全）。
 * @param[out] soc  输出 SOC（非空）。
 * @param[out] meta 输出槽位元数据（可为 NULL，此时不回填）。
 * @return 0 成功；@p soc 为 NULL 返回 -EINVAL。
 */
int bms_db_read_soc(struct bms_soc *soc, struct bms_db_meta *meta);

/**
 * @brief 写入最新保护事件快照（线程安全）。
 * @param[in] prot 待写入保护事件（非空）。
 * @return 0 成功；@p prot 为 NULL 返回 -EINVAL。
 */
int bms_db_write_prot(const struct bms_prot_evt *prot);

/**
 * @brief 读取最新保护事件快照（线程安全）。
 * @param[out] prot 输出保护事件（非空）。
 * @param[out] meta 输出槽位元数据（可为 NULL，此时不回填）。
 * @return 0 成功；@p prot 为 NULL 返回 -EINVAL。
 */
int bms_db_read_prot(struct bms_prot_evt *prot, struct bms_db_meta *meta);

/**
 * @brief 写入最新整机状态快照（线程安全）。
 * @param[in] state 待写入状态（非空）。
 * @return 0 成功；@p state 为 NULL 返回 -EINVAL。
 */
int bms_db_write_bms_state(const struct bms_state_snapshot *state);

/**
 * @brief 读取最新整机状态快照（线程安全）。
 * @param[out] state 输出状态（非空）。
 * @param[out] meta  输出槽位元数据（可为 NULL，此时不回填）。
 * @return 0 成功；@p state 为 NULL 返回 -EINVAL。
 */
int bms_db_read_bms_state(struct bms_state_snapshot *state, struct bms_db_meta *meta);

#ifdef __cplusplus
}
#endif

#endif /* BMS_DB_H_ */
