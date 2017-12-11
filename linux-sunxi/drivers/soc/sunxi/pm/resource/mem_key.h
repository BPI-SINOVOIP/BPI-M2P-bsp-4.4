/*
 * drivers/soc/sunxi/pm/resource/mem_key.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __MEM_KEY_H__
#define __MEM_KEY_H__

/*define key controller registers*/
struct __mem_key_reg_t {
	/* offset:0x00 */
	volatile __u32 Lradc_Ctrl;
	volatile __u32 Lradc_Intc;
	volatile __u32 Lradc_Ints;
	volatile __u32 Lradc_Data0;
	volatile __u32 Lradc_Data1;
};

extern __s32 mem_key_init(void);
extern __s32 mem_key_exit(void);
extern __s32 mem_query_key(void);

#endif				/* __MEM_KEY_H__ */
