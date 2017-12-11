/*
 * drivers/soc/sunxi/pm/standby/standby_twi.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _STANDBY_TWI_H
#define _STANDBY_TWI_H

#include <linux/types.h>

enum twi_op_type_e {
	TWI_OP_RD,
	TWI_OP_WR,
};

extern __s32 standby_twi_init(int group);
extern __s32 standby_twi_exit(void);
extern __s32 twi_byte_rw(enum twi_op_type_e op, __u8 saddr, __u8 baddr,
				__u8 *data);
extern __s32 standby_twi_init_losc(int group);

#endif /*_STANDBY_TWI_H*/
