/*
 * drivers/soc/sunxi/pm/standby/dram/mctl_standby.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _MCTL_STANDBY_H
#define _MCTL_STANDBY_H

#if defined(CONFIG_ARCH_SUN8IW11)
#include "sun8iw11p1/mctl_para-sun8iw11.h"
#elif defined(CONFIG_ARCH_SUN8IW10)
#include "sun8iw10p1/mctl_para-sun8iw10.h"
#else
#include "mctl_para-default.h"
#endif

extern unsigned int dram_power_save_process(void);
extern unsigned int dram_power_up_process(struct __dram_para_t *para);
extern void dram_enable_all_master(void);
extern void dram_disable_all_master(void);
#endif /* _MCTL_STANDBY_H */
