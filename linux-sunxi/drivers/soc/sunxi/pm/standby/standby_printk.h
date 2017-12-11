/*
 * drivers/soc/sunxi/pm/standby/standby_printk.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __MEM_PRINTK_H__
#define __MEM_PRINTK_H__

#include <linux/types.h>

#define DEBUG_BUFFER_SIZE (256)

/*other module may define printk, while its declaration may not be the same with this.*/
/*so, it not proper to export this symbols to global.*/
__s32 standby_printk(const char *format, ...);
__s32 standby_printk_nommu(const char *format, ...);

#endif /*__MEM_PRINTK_H__*/
