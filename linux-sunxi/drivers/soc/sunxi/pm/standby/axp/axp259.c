/*
 * drivers/soc/sunxi/pm/standby/axp/axp259.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * axp259 sleep control for standby
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "../standby_twi.h"

#define AXP259_ADDR                  (0x36)

int axp259_enter_sleep(void)
{
	u8 reg_val;
	s32 ret = 0;

	ret = twi_byte_rw(TWI_OP_RD, AXP259_ADDR, 0x26, &reg_val);
	if (ret)
		goto out;

	reg_val |= 0x1 << 3;

	ret = twi_byte_rw(TWI_OP_WR, AXP259_ADDR, 0x26, &reg_val);
	if (ret)
		goto out;

out:
	return ret;
}
