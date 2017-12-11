/*
 * drivers/soc/sunxi/pm/standby/axp/axp_core.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __AXP_CORE_H__
#define __AXP_CORE_H__

#include <linux/types.h>
#include <linux/power/aw_pm.h>
#include "../standby_printk.h"
#include "../standby.h"
#include "axp15.h"
#include "axp20.h"
#include "axp22.h"

#define AXP_LDO(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg, emask,\
	enval, disval)\
{                                                 \
	.min_uv       = (min) * 1000,             \
	.max_uv       = (max) * 1000,             \
	.step1_uv     = (step1) * 1000,           \
	.vol_reg      = _pmic##_##vreg,           \
	.vol_shift    = (shift),                  \
	.vol_nbits    = (nbits),                  \
	.enable_reg   = _pmic##_##ereg,           \
	.enable_mask  = (emask),                  \
	.enable_val   = enval,                    \
	.disable_val  = disval,                   \
}

#define AXP_DCDC(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg, emask,\
	enval, disval)\
{                                                 \
	.min_uv       = (min) * 1000,             \
	.max_uv       = (max) * 1000,             \
	.step1_uv     = (step1) * 1000,           \
	.vol_reg      = _pmic##_##vreg,           \
	.vol_shift    = (shift),                  \
	.vol_nbits    = (nbits),                  \
	.enable_reg   = _pmic##_##ereg,           \
	.enable_mask  = (emask),                  \
	.enable_val   = enval,                    \
	.disable_val  = disval,                   \
}

#define AXP_SW(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg, emask,\
	enval, disval)\
{                                                 \
	.min_uv       = (min) * 1000,             \
	.max_uv       = (max) * 1000,             \
	.step1_uv     = (step1) * 1000,           \
	.vol_reg      = _pmic##_##vreg,           \
	.vol_shift    = (shift),                  \
	.vol_nbits    = (nbits),                  \
	.enable_reg   = _pmic##_##ereg,           \
	.enable_mask  = (emask),                  \
	.enable_val   = enval,                    \
	.disable_val  = disval,                   \
}

#define AXP_LDO_SEL(_pmic, _id, min, max, vreg, shift, nbits, ereg,\
	emask, enval, disval, table_name)\
{                                                        \
	.min_uv      = (min) * 1000,                     \
	.max_uv      = (max) * 1000,                     \
	.vol_reg     = _pmic##_##vreg,                   \
	.vol_shift   = (shift),                          \
	.vol_nbits   = (nbits),                          \
	.enable_reg  = _pmic##_##ereg,                   \
	.enable_mask = (emask),                          \
	.enable_val  = enval,                            \
	.disable_val = disval,                           \
	.vtable      = (int *)&table_name##_table,       \
}

#define AXP_DCDC_SEL(_pmic, _id, min, max, vreg, shift, nbits, ereg,\
	emask, enval, disval, table_name)\
{                                                        \
	.min_uv      = (min) * 1000,                     \
	.max_uv      = (max) * 1000,                     \
	.vol_reg     = _pmic##_##vreg,                   \
	.vol_shift   = (shift),                          \
	.vol_nbits   = (nbits),                          \
	.enable_reg  = _pmic##_##ereg,                   \
	.enable_mask = (emask),                          \
	.enable_val  = enval,                            \
	.disable_val = disval,                           \
	.vtable      = (int *)&table_name##_table,       \
}

struct axp_regulator_info {
	int min_uv;
	int max_uv;
	int step1_uv;
	int vol_reg;
	int vol_shift;
	int vol_nbits;
	int enable_reg;
	int enable_mask;
	int enable_val;
	int disable_val;
	int *vtable;
};

struct axp_dev_info {
	u32 pmu_addr;
	u32 pmu_id_max;
	struct axp_regulator_info *pmu_regu_table;
};

extern struct axp_regulator_info *find_info(struct axp_dev_info *dev_info,
				u32 id);
extern s32 axp_set_volt(struct axp_dev_info *dev_info, u32 id, u32 voltage);
extern s32 axp_get_volt(struct axp_dev_info *dev_info, u32 id);
extern s32 axp_set_state(struct axp_dev_info *dev_info, u32 id, u32 state);
extern s32 axp_get_state(struct axp_dev_info *dev_info, u32 id);
extern s32 __twi_byte_update(u8 saddr, u8 reg, u8 val, u8 mask);

extern void dm_suspend(struct aw_pm_info *config,
				extended_standby_t *extended_config);
extern void dm_resume(extended_standby_t *extended_config);
extern void power_domain_resume(extended_standby_t *extended_config);
extern void power_domain_suspend(struct aw_pm_info *config,
				extended_standby_t *extended_config);
extern void power_enter_super_calc(struct aw_pm_info *config,
				extended_standby_t *extended_config,
				losc_enter_ss_func *func);

#endif /* __AXP_CORE_H__ */
