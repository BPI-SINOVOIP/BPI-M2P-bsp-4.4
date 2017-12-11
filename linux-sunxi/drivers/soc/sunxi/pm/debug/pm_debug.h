/*
 * drivers/soc/sunxi/pm/debug/pm_debug.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _PM_DEBUG_H
#define _PM_DEBUG_H

#include <linux/types.h>
/*
 * Copyright (c) 2011-2015 yanggq.young@allwinnertech.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

/*#define GET_CYCLE_CNT*/
#define IO_MEASURE
/* extern volatile int print_flag; */

/*
 * notice: when resume, boot0 need to clear the flag,
 * in case the data in dram be destroyed
 * result in the system is re-resume in cycle.
 */
void mem_status_init(char *name);
void mem_status_init_nommu(void);
void mem_status_clear(void);
void mem_status_exit(void);
void save_mem_flag(void);
void clear_mem_flag(void);
void save_mem_status(volatile __u32 val);
void parse_status_code(__u32 code, __u32 index);
void save_mem_status_nommu(volatile __u32 val);
void save_cpux_mem_status_nommu(volatile __u32 val);

__u32 get_mem_status(void);
void show_mem_status(void);
__u32 save_sun5i_mem_status_nommu(volatile __u32 val);
__u32 save_sun5i_mem_status(volatile __u32 val);
void save_irq_status(volatile __u32 val);

/*for secure debug, add by huangshr
 *data: 2014-10-20
 */
void pm_secure_mem_status_init(char *name);
void pm_secure_mem_status_init_nommu(void);
void pm_secure_mem_status_clear(void);
void pm_secure_mem_status_exit(void);
void show_pm_secure_mem_status(void);
void save_pm_secure_mem_status(volatile __u32 val);
void save_pm_secure_mem_status_nommu(volatile __u32 val);
__u32 get_pm_secure_mem_status(void);

void io_init(void);
void io_init_high(void);
void io_init_low(void);
void io_high(int num);

#endif /*_PM_DEBUG_H*/
