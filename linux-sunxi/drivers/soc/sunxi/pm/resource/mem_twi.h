/*
 * drivers/soc/sunxi/pm/resource/mem_twi.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __MEM_TWI_H__
#define __MEM_TWI_H__

#include <linux/types.h>

struct __twic_reg_t {
	volatile unsigned int reg_saddr;
	volatile unsigned int reg_xsaddr;
	volatile unsigned int reg_data;
	volatile unsigned int reg_ctl;
	volatile unsigned int reg_status;
	volatile unsigned int reg_clkr;
	volatile unsigned int reg_reset;
	volatile unsigned int reg_efr;
	volatile unsigned int reg_lctl;

};

extern __s32 mem_twi_save(int group);
extern __s32 mem_twi_restore(void);

#endif	/*__MEM_TWI_H__*/
