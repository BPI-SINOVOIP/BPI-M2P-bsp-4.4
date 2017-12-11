/*
 * drivers/soc/sunxi/pm/debug/pm_debug.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * debug APIs for sunxi suspend
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/of.h>
#include "pm_debug.h"
#include "pm_errcode.h"
#include "../pm.h"

/*for io-measure time*/
#define PORT_E_CONFIG (0xf1c20890)
#define PORT_E_DATA   (0xf1c208a0)
#define PORT_CONFIG PORT_E_CONFIG
#define PORT_DATA PORT_E_DATA

/*
 * notice: dependent with perf counter to delay.
 */
void io_init(void)
{
	/*config port output */
	*(volatile unsigned int *)(PORT_CONFIG) = 0x111111;
}

void io_init_high(void)
{
	__u32 data;

	/*set port to high */
	data = *(volatile unsigned int *)(PORT_DATA);
	data |= 0x3f;
	*(volatile unsigned int *)(PORT_DATA) = data;
}

void io_init_low(void)
{
	__u32 data;

	data = *(volatile unsigned int *)(PORT_DATA);
	/*set port to low */
	data &= 0xffffffc0;
	*(volatile unsigned int *)(PORT_DATA) = data;
}

/*
 * set pa port to high, num range is 0-7;
 */
void io_high(int num)
{
	__u32 data;

	data = *(volatile unsigned int *)(PORT_DATA);
	/*pull low 10ms */
	data &= (~(1 << num));
	*(volatile unsigned int *)(PORT_DATA) = data;

	mdelay(10);

	/*pull high */
	data |= (1 << num);
	*(volatile unsigned int *)(PORT_DATA) = data;
}

#define CPUX_STATUS_CODE_INDEX (1)
#define CPUX_IRQ_STATUS_CODE_INDEX (2)
#define CPUS_STATUS_CODE_INDEX (3)

static u32 *base;
static u32 len;
static u32 mem_status_init_done;

void mem_status_init(char *name)
{
	u32 gpr_offset;
	struct device_node *np;

	pm_get_dev_info(name, 0, &base, &len);

	np = of_find_node_by_type(NULL, name);
	if (np == NULL) {
		pr_err("can not find np for %s\n", name);
	} else {
		pr_info("np name = %s\n", np->full_name);
		if (!of_property_read_u32(np, "gpr_offset", &gpr_offset)) {
			if (!of_property_read_u32(np, "gpr_len", &len)) {
				base = (gpr_offset / sizeof(u32) + base);
				pr_info("base = %p, len = %x.\n", base, len);
			}
		}
	}

	mem_status_init_done = 1;
}

void mem_status_init_nommu(void)
{
}

void mem_status_clear(void)
{
	int i = 0;

	while (i < len) {
		*(volatile int *)((phys_addr_t)(base + i)) = 0x0;
		i++;
	}
}

void mem_status_exit(void)
{
}

void save_irq_status(volatile __u32 val)
{
	if (likely(mem_status_init_done == 1)) {
		*(volatile __u32 *)((phys_addr_t)(base
				+ CPUX_IRQ_STATUS_CODE_INDEX)) = val;
		/* asm volatile ("dsb"); */
		/* asm volatile ("isb"); */
	}
}

void save_mem_status(volatile __u32 val)
{
	*(volatile __u32 *)((phys_addr_t)(base + CPUX_STATUS_CODE_INDEX)) = val;
	/* asm volatile ("dsb");*/
	/* asm volatile ("isb");*/
}

__u32 get_mem_status(void)
{
	return *(volatile __u32 *)((phys_addr_t)(base
				+ CPUX_STATUS_CODE_INDEX));
}

void parse_cpux_status_code(__u32 code)
{
	pr_info("%s.\n", pm_errstr(code));
}

void parse_cpus_status_code(__u32 code)
{
}

void parse_status_code(__u32 code, __u32 index)
{
	switch (index) {
	case CPUX_STATUS_CODE_INDEX:
		parse_cpux_status_code(code);
		break;
	case CPUS_STATUS_CODE_INDEX:
		parse_cpus_status_code(code);
		break;
	default:
		pr_err("para err, index = %x.\n", index);
	}
}

void show_mem_status(void)
{
	int i = 0;
	__u32 status_code = 0;

	while (i < len) {
		status_code = *(volatile int *)((phys_addr_t)(base + i));
		pr_info("addr %p, value = %x\n", (base + i), status_code);
		parse_status_code(status_code, i);
		i++;
	}
}

void pm_secure_mem_status_init(char *name)
{
	mem_status_init(name);
}

void pm_secure_mem_status_init_nommu(void)
{
	mem_status_init_nommu();
}

void pm_secure_mem_status_clear(void)
{
	mem_status_clear();
}

void pm_secure_mem_status_exit(void)
{
	mem_status_exit();
}

void save_pm_secure_mem_status(volatile __u32 val)
{
	save_mem_status(val);
}

__u32 get_pm_secure_mem_status(void)
{
	return get_mem_status();
}

void show_pm_secure_mem_status(void)
{
	show_mem_status();
}
