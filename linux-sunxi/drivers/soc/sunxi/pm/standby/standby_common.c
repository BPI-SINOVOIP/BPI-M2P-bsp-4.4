/*
 * drivers/soc/sunxi/pm/standby/standby_common.c
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
#include <linux/power/aw_pm.h>
#include "../pm_config.h"
#include "standby_printk.h"

void standby_memcpy(void *dest, void *src, int n)
{
	char *tmp_src = (char *)NULL;
	char *tmp_dst = (char *)NULL;
	u32 *src_p = (u32 *)(src);
	u32 *dst_p = (u32 *)dest;

	if (!dest || !src)
		return;

	for (; n > 4; n -= 4)
		*dst_p++ = *src_p++;

	tmp_src = (char *)(src_p);
	tmp_dst = (char *)(dst_p);

	for (; n > 0; n--)
		*tmp_dst++ = *tmp_src++;
}

s32 standby_dram_crc_enable(pm_dram_para_t *pdram_state)
{
	return pdram_state->crc_en;
}

u32 standby_dram_crc(pm_dram_para_t *pdram_state)
{
	u32 *pdata = (u32 *)0;
	u32 crc = 0;
	u32 start = 0;

	pdata = (u32 *)((pdram_state->crc_start) + 0x80000000);
	start = (u32)pdata;

	standby_printk("src:%x len:%x\n", start, pdram_state->crc_len);

	while (pdata < (u32 *)(start + pdram_state->crc_len)) {
		crc += *pdata;
		pdata++;
	}

	standby_printk("crc finish...\n");

	return crc;
}

void mem_status_init(char *name)
{
}

void mem_status_init_nommu(void)
{
}

void mem_status_exit(void)
{
}

void mem_status_clear(void)
{
	int i = 0;

	while (i < STANDBY_STATUS_REG_NUM) {
		*(volatile int *)(STANDBY_STATUS_REG + i * 4) = 0x0;
		i++;
	}
}

void save_mem_status(volatile __u32 val)
{
	*(volatile __u32 *)(STANDBY_STATUS_REG + 0x0c) = val;
	asm volatile ("dsb");
	asm volatile ("isb");
}

__u32 get_mem_status(void)
{
	return *(volatile __u32 *)(STANDBY_STATUS_REG + 0x0c);
}

void show_mem_status(void)
{
	int i = 0;

	while (i < STANDBY_STATUS_REG_NUM) {
		standby_printk("addr %x, value = %x\n",
				(STANDBY_STATUS_REG + i * 4),
				*(volatile int *)(STANDBY_STATUS_REG + i * 4));
		i++;
	}
}

void save_mem_status_nommu(volatile __u32 val)
{
	*(volatile __u32 *)(STANDBY_STATUS_REG_PA + 0x0c) = val;
}

void save_cpux_mem_status_nommu(volatile __u32 val)
{
	*(volatile __u32 *)(STANDBY_STATUS_REG_PA + 0x04) = val;
}

void save_super_flags(volatile __u32 val)
{
	*(volatile __u32 *)(STANDBY_SUPER_FLAG_REG) = val;
	asm volatile ("dsb");
	asm volatile ("isb");
}

void save_super_addr(volatile __u32 val)
{
	*(volatile __u32 *)(STANDBY_SUPER_ADDR_REG) = val;
	asm volatile ("dsb");
	asm volatile ("isb");
}
