/*
 * drivers/soc/sunxi/pm/standby/axp/axp_core.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * axp common APIs for standby
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "axp_core.h"
#include "../standby.h"
#include "../standby_twi.h"
#include "../standby_printk.h"

static u32 dm_on;
static u32 dm_off;

/* power domain */
#define IS_DM_ON(dm)       ((dm_on >> dm) & 0x1)
#define IS_DM_OFF(dm)      (!((dm_off >> dm) & 0x1))
#define POWER_VOL_OFF      (0)
#define POWER_VOL_ON       (1)

static s32 volt_bak[VCC_MAX_INDEX];
static unsigned int (*power_regu_tree)[VCC_MAX_INDEX];

static s32 pmu_get_voltage(u32 pmux_id, u32 tree)
{
	s32 ret = -1;

	if (tree == 0) {
		standby_printk("%s:tree is 0\n", __func__);
		return ret;
	}

	pmux_id &= 0xf;
	tree &= 0x0fffffff;

	switch (pmux_id) {
	case AXP_19X_ID:
		break;
	case AXP_209_ID:
		ret = axp20_get_volt(tree);
		break;
	case AXP_22X_ID:
		ret = axp22_get_volt(tree);
		break;
	case AXP_806_ID:
		break;
	case AXP_808_ID:
		break;
	case AXP_809_ID:
		break;
	case AXP_803_ID:
		break;
	case AXP_813_ID:
		break;
	case AXP_152_ID:
		ret = axp15_get_volt(tree);
		break;
	default:
		standby_printk("%s:pmu id err, tree = 0x%x\n", __func__, tree);
		return -1;
	}

	return ret;
}

static s32 pmu_set_voltage(u32 pmux_id, u32 tree, u32 voltage)
{
	s32 ret = -1;

	if (tree == 0) {
		standby_printk("%s: tree is 0\n", __func__);
		return ret;
	}

	pmux_id &= 0xf;
	tree &= 0x0fffffff;

	switch (pmux_id) {
	case AXP_19X_ID:
		break;
	case AXP_209_ID:
		ret = axp20_set_volt(tree, voltage);
		break;
	case AXP_22X_ID:
		ret = axp22_set_volt(tree, voltage);
		break;
	case AXP_806_ID:
		break;
	case AXP_808_ID:
		break;
	case AXP_809_ID:
		break;
	case AXP_803_ID:
		break;
	case AXP_813_ID:
		break;
	case AXP_152_ID:
		ret = axp15_set_volt(tree, voltage);
		break;
	default:
		standby_printk("%s:pmu id err, tree = 0x%x\n", __func__, tree);
		return -1;
	}

	if (ret != 0)
		standby_printk("%s faied\n", __func__);

	return ret;
}

static s32 pmu_get_state(u32 pmux_id, u32 tree)
{
	s32 ret = -1;

	if (tree == 0) {
		standby_printk("%s: tree is 0\n", __func__);
		return ret;
	}

	pmux_id &= 0xf;
	tree &= 0x0fffffff;

	switch (pmux_id) {
	case AXP_19X_ID:
		break;
	case AXP_209_ID:
		ret = axp20_get_state(tree);
		break;
	case AXP_22X_ID:
		ret = axp22_get_state(tree);
		break;
	case AXP_806_ID:
		break;
	case AXP_808_ID:
		break;
	case AXP_809_ID:
		break;
	case AXP_803_ID:
		break;
	case AXP_813_ID:
		break;
	case AXP_152_ID:
		ret = axp15_get_state(tree);
		break;
	default:
		standby_printk("%s:pmu id err, tree = 0x%x\n", __func__, tree);
		return -1;
	}

	if (ret != 0)
		standby_printk("%s faied\n", __func__);

	return ret;
}

static s32 pmu_set_state(u32 pmux_id, u32 tree, u32 state)
{
	s32 ret = -1;

	if (tree == 0) {
		standby_printk("%s: tree is 0\n", __func__);
		return ret;
	}

	pmux_id &= 0xf;
	tree &= 0x0fffffff;

	switch (pmux_id) {
	case AXP_19X_ID:
		break;
	case AXP_209_ID:
		ret = axp20_set_state(tree, state);
		break;
	case AXP_22X_ID:
		ret = axp22_set_state(tree, state);
		break;
	case AXP_806_ID:
		break;
	case AXP_808_ID:
		break;
	case AXP_809_ID:
		break;
	case AXP_803_ID:
		break;
	case AXP_813_ID:
		break;
	case AXP_152_ID:
		ret = axp15_set_state(tree, state);
		break;
	default:
		standby_printk("%s:pmu id err, tree = 0x%x\n", __func__, tree);
		return -1;
	}

	if (ret != 0)
		standby_printk("%s faied\n", __func__);

	return ret;
}

#ifdef CONFIG_DUAL_AXP_USED
static int secondary_pmu_id;
#endif

static void pmu_suspend_calc(u32 pmux_id, u32 pmu_cnt, u32 mask,
				losc_enter_ss_func *func)
{
	s32 ret = -1;
	s32 tmpctrl = 1;

	if (mask == 0) {
		standby_printk("%s: tree is 0\n", __func__);
		return;
	}

#ifdef CONFIG_DUAL_AXP_USED
	secondary_pmu_id = (pmux_id >> 8) & 0xff;
#endif

	pmux_id &= 0xff;
	mask &= 0x0fffffff;

	switch (pmux_id) {
	case AXP_19X_ID:
		break;
	case AXP_209_ID:
		ret = axp20_suspend_calc(mask, func);
		break;
	case AXP_22X_ID:
		ret = axp22_suspend_calc(mask, func);
		break;
	case AXP_806_ID:
		break;
	case AXP_808_ID:
		break;
	case AXP_809_ID:
		break;
	case AXP_803_ID:
		break;
	case AXP_813_ID:
		break;
	case AXP_152_ID:
		ret = axp15_suspend_calc(pmu_cnt, mask, func);
		break;
	default:
		standby_printk("%s:pmu id err, tree = 0x%x\n", __func__, mask);
		return;
	}

	if (ret != 0)
		standby_printk("%s faied\n", __func__);
}

#ifdef CONFIG_DUAL_AXP_USED
s32 secondary_pmu_enter_sleep(void)
{
	s32 ret = -1;

	switch (secondary_pmu_id) {
#ifdef CONFIG_AW_AXP259
	case AXP_259_ID:
		ret = axp259_enter_sleep();
		break;
#endif
	default:
		standby_printk("%s: pmu id err, id=%d\n",
				__func__, secondary_pmu_id);
		return -1;
	}

	if (ret != 0)
		standby_printk("%s: faied\n", __func__);

	return ret;
}
#endif

static u32 close_mask;
void power_enter_super_calc(struct aw_pm_info *config,
				extended_standby_t *extended_config,
				losc_enter_ss_func *func)
{
	int dm;
	u32 on_mask_adjust = 0;

	power_regu_tree = &(config->pmu_arg.soc_power_tree);
	close_mask = 0;

	dm_on = extended_config->soc_pwr_dm_state.sys_mask
		& extended_config->soc_pwr_dm_state.state;
	dm_off = (~(extended_config->soc_pwr_dm_state.sys_mask)
		| extended_config->soc_pwr_dm_state.state) | dm_on;

	for (dm = VCC_MAX_INDEX - 1; dm >= 0; dm--) {
		if (IS_DM_OFF(dm))
			close_mask |= (*power_regu_tree)[dm];
		else if (IS_DM_ON(dm))
			on_mask_adjust |= (*power_regu_tree)[dm];
	}

	standby_printk("dm_on = 0x%x, dm_off = 0x%x, close_mask = 0x%x\n",
				dm_on, dm_off, close_mask);
	close_mask &= ~on_mask_adjust;
	standby_printk("adjust on mask = 0x%x, adjust close mask = 0x%x\n",
				on_mask_adjust, close_mask);

	pmu_suspend_calc(extended_config->pmu_id, config->pmu_arg.axp_dev_count,
				close_mask, func);
}

void power_domain_suspend(struct aw_pm_info *config,
				extended_standby_t *extended_config)
{
	int dm;
	u32 on_mask_adjust = 0;

	close_mask = 0;

	standby_printk("extended_config->pmu_id = 0x%x.\n",
				extended_config->pmu_id);

	power_regu_tree = &(config->pmu_arg.soc_power_tree);

	dm_on = extended_config->soc_pwr_dm_state.sys_mask
		& extended_config->soc_pwr_dm_state.state;
	dm_off = (~(extended_config->soc_pwr_dm_state.sys_mask)
		| extended_config->soc_pwr_dm_state.state) | dm_on;

	for (dm = VCC_MAX_INDEX - 1; dm >= 0; dm--) {
		if (IS_DM_ON(dm)) {
			if (extended_config->soc_pwr_dm_state.volt[dm] != 0) {
				volt_bak[dm] = pmu_get_voltage(
						extended_config->pmu_id,
						(*power_regu_tree)[dm]);
				if (volt_bak[dm] > 0)
					standby_printk("volt_bak[%d]=%d\n",
							dm, volt_bak[dm]);
				pmu_set_voltage(extended_config->pmu_id,
						(*power_regu_tree)[dm],
				extended_config->soc_pwr_dm_state.volt[dm]);
			}
		}
	}

	for (dm = VCC_MAX_INDEX - 1; dm >= 0; dm--) {
		if (IS_DM_OFF(dm))
			close_mask |= (*power_regu_tree)[dm];
		else if (IS_DM_ON(dm))
			on_mask_adjust |= (*power_regu_tree)[dm];
	}

	close_mask &= ~on_mask_adjust;
	for (dm = VCC_MAX_INDEX - 1; dm >= 0; dm--) {
		if (IS_DM_OFF(dm)) {
			if ((*power_regu_tree)[dm] & close_mask)
				pmu_set_state(extended_config->pmu_id,
					(*power_regu_tree)[dm], POWER_VOL_OFF);
		}
	}
}

void power_domain_resume(extended_standby_t *extended_config)
{
	u32 dm;

	for (dm = 0; dm < VCC_MAX_INDEX; dm++) {
		if (IS_DM_ON(dm)) {
			if (extended_config->soc_pwr_dm_state.volt[dm] != 0) {
				pmu_set_voltage(extended_config->pmu_id,
						(*power_regu_tree)[dm],
						volt_bak[dm]);
			}
		}
	}

	for (dm = 0; dm < VCC_MAX_INDEX; dm++) {
		if (IS_DM_OFF(dm))
			pmu_set_state(extended_config->pmu_id,
				(*power_regu_tree)[dm], POWER_VOL_ON);
	}
}

struct axp_regulator_info *find_info(struct axp_dev_info *dev_info, u32 id)
{
	struct axp_regulator_info *ri = NULL;
	int i = 0;

	for (i = 0; i <= dev_info->pmu_id_max; i++) {
		if (id & (0x1 << i)) {
			ri = dev_info->pmu_regu_table + i;
			break;
		}
	}

	return ri;
}

/* AXP common operations */
s32 axp_set_volt(struct axp_dev_info *dev_info, u32 id, u32 voltage)
{
	struct axp_regulator_info *info = NULL;
	u8 val, mask, reg_val;
	s32 ret = -1;

	info = find_info(dev_info, id);

	val = (voltage - info->min_uv + info->step1_uv - 1)
			/ info->step1_uv;
	val <<= info->vol_shift;
	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;

	ret = twi_byte_rw(TWI_OP_RD, dev_info->pmu_addr,
					info->vol_reg, &reg_val);
	if (ret)
		return ret;

	if ((reg_val & mask) != val) {
		reg_val = (reg_val & ~mask) | val;
		ret = twi_byte_rw(TWI_OP_WR, dev_info->pmu_addr,
					info->vol_reg, &reg_val);
	}

	return ret;
}

s32 axp_get_volt(struct axp_dev_info *dev_info, u32 id)
{
	struct axp_regulator_info *info = NULL;
	u8 val, mask;
	s32 ret, vol;

	info = find_info(dev_info, id);

	ret = twi_byte_rw(TWI_OP_RD, dev_info->pmu_addr, info->vol_reg, &val);
	if (ret)
		return ret;

	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	val = (val & mask) >> info->vol_shift;
	vol = info->min_uv + info->step1_uv * val;

	return vol;
}

s32 axp_set_state(struct axp_dev_info *dev_info, u32 id, u32 state)
{
	struct axp_regulator_info *info = NULL;
	s32 ret = -1;
	uint8_t reg_val = 0;

	info = find_info(dev_info, id);

	if (state == 0)
		reg_val = info->disable_val;
	else
		reg_val = info->enable_val;

	return __twi_byte_update(dev_info->pmu_addr, info->enable_reg,
				reg_val, info->enable_mask);
}

s32 axp_get_state(struct axp_dev_info *dev_info, u32 id)
{
	struct axp_regulator_info *info = NULL;
	s32 ret = -1;
	uint8_t reg_val;

	info = find_info(dev_info, id);

	ret = twi_byte_rw(TWI_OP_RD, dev_info->pmu_addr,
				info->enable_reg, &reg_val);
	if (ret)
		return ret;

	if ((reg_val & info->enable_mask) == info->enable_val)
		return 1;
	else
		return 0;
}

s32 __twi_byte_update(u8 saddr, u8 reg, u8 val, u8 mask)
{
	u8 reg_val = 0;
	s32 ret = 0;

	ret = twi_byte_rw(TWI_OP_RD, saddr, reg, &reg_val);
	if (ret)
		goto out;

	if ((reg_val & mask) != val) {
		reg_val = (reg_val & ~mask) | val;
		ret = twi_byte_rw(TWI_OP_WR, saddr, reg, &reg_val);
	}

out:
	return ret;
}
