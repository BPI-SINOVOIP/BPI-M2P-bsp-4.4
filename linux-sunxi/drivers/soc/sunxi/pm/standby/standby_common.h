/*
 * drivers/soc/sunxi/pm/standby/standby_common.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __STANDBY_COMMON_H__
#define __STANDBY_COMMON_H__

#include <linux/power/aw_pm.h>

#define STANDBY_START			(0x00)
#define RESUME0_START			(0x20)
#define RESUME1_START			(0x40)
#define SHUTDOWN_FLOW			(0x10)
#define OTA_FLOW			(0x30)

static inline __u64 standby_uldiv(__u64 dividend, __u32 divisior)
{
	__u64 tmpDiv = (__u64) divisior;
	__u64 tmpQuot = 0;
	__s32 shift = 0;

	if (!divisior) {
		/* divide 0 error abort */
		return 0;
	}

	while (!(tmpDiv & ((__u64) 1 << 63))) {
		tmpDiv <<= 1;
		shift++;
	}

	do {
		if (dividend >= tmpDiv) {
			dividend -= tmpDiv;
			tmpQuot = (tmpQuot << 1) | 1;
		} else {
			tmpQuot = (tmpQuot << 1) | 0;
		}

		tmpDiv >>= 1;
		shift--;
	} while (shift >= 0);

	return tmpQuot;
}

void standby_memcpy(void *dest, void *src, int n);
void standby_delay_cycle(int cycle);

s32 standby_dram_crc_enable(pm_dram_para_t *pdram_state);
u32 standby_dram_crc(pm_dram_para_t *pdram_state);

void mem_status_init_nommu(void);
void mem_status_clear(void);
void mem_status_exit(void);
void save_irq_status(volatile __u32 val);
void save_mem_status(volatile __u32 val);
void save_super_flags(volatile __u32 val);
void save_super_addr(volatile __u32 val);
__u32 get_mem_status(void);
void parse_status_code(__u32 code, __u32 index);
void show_mem_status(void);

#endif /*__STANDBY_COMMON_H__*/
