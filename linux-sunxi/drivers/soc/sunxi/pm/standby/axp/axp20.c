/*
 * drivers/soc/sunxi/pm/standby/axp/axp20.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * axp209 regulator control for standby
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "axp20.h"
#include "axp_core.h"
#include "../standby.h"
#include "../standby_twi.h"

static struct axp_regulator_info axp20_regulator_info[] = {
	AXP_DCDC(AXP20, 0,  700, 2275,  25,  BUCK2, 0,  6, BUCK2EN, 0x10, 0, 0),
	AXP_DCDC(AXP20, 1,  700, 3500,  25,  BUCK3, 0,  7, BUCK3EN,  0x2, 0, 0),
	AXP_LDO(AXP20,  2, 3300, 3300,   0,   LDO1, 0,  0,  LDO1EN,    0, 0, 0),
	AXP_LDO(AXP20,  3, 1800, 3300, 100,   LDO2, 4,  4,  LDO2EN,  0x6, 0, 0),
	AXP_LDO(AXP20,  4,  700, 3500,  25,   LDO3, 0,  7,  LDO3EN, 0x60, 0, 0),
	AXP_LDO(AXP20,  5, 1250, 3300,   0,   LDO4, 0,  4,  LDO4EN,  0x8, 0, 0),
	AXP_LDO(AXP20,  6, 1800, 3300, 100, LDOIO0, 4,  4, LDOIOEN,    0, 0, 0),
};

struct axp_dev_info axp20_dev_info = {
	.pmu_addr       = AXP20_ADDR,
	.pmu_id_max     = AXP20_ID_MAX,
	.pmu_regu_table = &axp20_regulator_info[0],
};

/* AXP common operations */
s32 axp20_set_volt(u32 id, u32 voltage)
{
	return axp_set_volt(&axp20_dev_info, id, voltage);
}

s32 axp20_get_volt(u32 id)
{
	return axp_get_volt(&axp20_dev_info, id);
}

s32 axp20_set_state(u32 id, u32 state)
{
	return axp_set_state(&axp20_dev_info, id, state);
}

s32 axp20_get_state(u32 id)
{
	return axp_get_state(&axp20_dev_info, id);
}

s32 __axp20_set_wakeup(u8 saddr, u8 reg)
{
	u8 reg_val;
	s32 ret = 0;

	ret = twi_byte_rw(TWI_OP_RD, saddr, reg, &reg_val);
	if (ret)
		goto out;

	reg_val |= 0x1 << 3;

	ret = twi_byte_rw(TWI_OP_WR, saddr, reg, &reg_val);

out:
	return ret;
}

static u8 dcdc_disable_val;
static u8 dcdc_enable_mask;

void axp20_losc_enter_ss(void)
{
	s32 ret = 0;

	standby_printk("%s: dcdc_disable_val=0x%x, dcdc_enable_mask=0x%x\n",
		__func__, dcdc_disable_val, dcdc_enable_mask);

	ret = __twi_byte_update(AXP20_ADDR, AXP20_BUCK2EN,
			dcdc_disable_val, dcdc_enable_mask);
	if (ret)
		standby_printk("axp20 dcdc failed\n");

}

s32 axp20_suspend_calc(u32 id, losc_enter_ss_func *func)
{
	struct axp_regulator_info *info = NULL;
	u32 i = 0;
	s32 ret = 0;

	standby_printk("%s: id=0x%x\n", __func__, id);

	__axp20_set_wakeup(AXP20_ADDR, 0x31);

	for (i = 0; i <= AXP20_ID_MAX; i++) {
		if (id & (0x1 << i)) {
			info = find_info(&axp20_dev_info, (0x1 << i));
			if (i < AXP20_DCDC_SUM) {
				dcdc_disable_val |= info->disable_val;
				dcdc_enable_mask |= info->enable_mask;
				continue;
			} else {
				standby_printk("close non-cpu src : 0x%x 0x%x, 0x%x\n",
						info->enable_reg,
						info->disable_val,
						info->enable_mask);
						ret = __twi_byte_update(
						AXP20_ADDR,
						info->enable_reg,
						info->disable_val,
						info->enable_mask);
				if (ret)
					standby_printk("axp20-%d failed\n", i);
			}
		}
	}

	*func = (void (*)(void))axp20_losc_enter_ss;
	axp20_losc_enter_ss();
	asm("b .");

	return ret;
}
