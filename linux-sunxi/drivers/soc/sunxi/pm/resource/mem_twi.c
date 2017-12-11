/*
 * drivers/soc/sunxi/pm/resource/mem_twi.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/types.h>
#include "mem_twi.h"
#include "../pm_config.h"

static struct __twic_reg_t *TWI_REG_BASE[3] = {
	(struct __twic_reg_t *) IO_ADDRESS(AW_TWI0_BASE),
	(struct __twic_reg_t *) IO_ADDRESS(AW_TWI1_BASE),
	(struct __twic_reg_t *) IO_ADDRESS(AW_TWI2_BASE)
};

static struct __twic_reg_t *twi_reg = (struct __twic_reg_t *) (0);
static struct __twic_reg_t twi_state_backup;

__s32 mem_twi_save(int group)
{
	twi_reg = TWI_REG_BASE[group];
	twi_state_backup.reg_saddr  = twi_reg->reg_saddr;
	twi_state_backup.reg_xsaddr = twi_reg->reg_xsaddr;
	twi_state_backup.reg_data   = twi_reg->reg_data;
	twi_state_backup.reg_ctl    = twi_reg->reg_ctl;
	twi_state_backup.reg_clkr   = twi_reg->reg_clkr;
	twi_state_backup.reg_efr    = twi_reg->reg_efr;
	twi_state_backup.reg_lctl   = twi_reg->reg_lctl;

	return 0;
}

__s32 mem_twi_restore(void)
{
	/* softreset twi module  */
	twi_reg->reg_reset |= 0x1;
	/* restore */
	twi_reg->reg_saddr  = twi_state_backup.reg_saddr;
	twi_reg->reg_xsaddr = twi_state_backup.reg_xsaddr;
	twi_reg->reg_data   = twi_state_backup.reg_data;
	twi_reg->reg_ctl    = twi_state_backup.reg_ctl;
	twi_reg->reg_clkr   = twi_state_backup.reg_clkr;
	twi_reg->reg_efr    = twi_state_backup.reg_efr;
	twi_reg->reg_lctl   = twi_state_backup.reg_lctl;

	return 0;
}
