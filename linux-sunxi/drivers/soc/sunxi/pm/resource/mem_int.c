/*
 * drivers/soc/sunxi/pm/resource/mem_int.c
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
#include <linux/printk.h>
#include "mem_int.h"
#include "../pm.h"

static void *GicDDisc;
static void *GicCDisc;
static u32 gic_d_len;
static u32 gic_c_len;

__s32 mem_int_init(void)
{
	u32 *base = 0;

	pm_get_dev_info("gic", 0, &base, &gic_d_len);
	GicDDisc = base;

	pm_get_dev_info("gic", 1, &base, &gic_c_len);
	GicCDisc = base;

	return 0;
}

__s32 mem_int_save(void)
{
#ifdef CONFIG_SUNXI_SUSPEND_NONARISC
	__u32 i = 0;
	/*printk("gic iar == 0x%x.\n", *(volatile __u32 *)(IO_ADDRESS(SUNXI_GIC_CPU_PBASE)+0x0c)); */

	/* initialise interrupt enable and mask for mem */

	/*
	 * Disable all interrupts.  Leave the PPI and SGIs alone
	 * as these enables are banked registers.
	 */
	for (i = 4; i < (GIC_400_ENABLE_LEN); i += 4)
		*(volatile __u32 *)(GicDDisc + GIC_DIST_ENABLE_CLEAR + i) =
				0xffffffff;

	/*config cpu interface */
#if 0
	*(volatile __u32 *)(GicCDisc + GIC_CPU_PRIMASK) = 0xf0;
	*(volatile __u32 *)(GicCDisc + GIC_CPU_CTRL) = 0x1;
#endif

#if 1
	/* clear external irq pending: needed */
	for (i = 4; i < (GIC_400_ENABLE_LEN); i += 4)
		*(volatile __u32 *)(GicDDisc + GIC_DIST_PENDING_CLEAR + i) =
				0xffffffff;
#endif
	/*the print info just to check the pending state, actually, after u read iar, u need to access end of interrupt reg; */
	i = *(volatile __u32 *)(GicCDisc + 0x0c);

	if (i != 0x3ff) {
		/*u need to */
		*(volatile __u32 *)(GicCDisc + 0x10) = i;
		pr_info("notice: gic iar == 0x%x.\n", i);
	}
#endif

	return 0;
}

__s32 mem_int_restore(void)
{
#ifdef CONFIG_SUNXI_SUSPEND_NONARISC
	int i = 0;
	volatile __u32 enable_bit = 0;

	/*all the disable-int-src pending, need to be clear */
	for (i = 0; i < GIC_400_ENABLE_LEN; i += 4) {
		enable_bit = *(volatile __u32 *)
				(GicDDisc + GIC_DIST_ENABLE_SET + i);
		*(volatile __u32 *)(GicDDisc + GIC_DIST_PENDING_CLEAR + i) &=
				(~enable_bit);
	}
#endif

	return 0;
}

__s32 mem_enable_int(enum interrupt_source_e src)
{
	__u32 tmpGrp = (__u32) src >> 5;
	__u32 tmpSrc = (__u32) src & 0x1f;

	if (src == 0)
		return -1;

	/*enable interrupt source */
	/*printk("GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp*4 = 0x%p. tmpGrp = 0x%x.\n", GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp*4, tmpGrp); */
	/*printk("GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp*4 = 0x%x. tmpGrp = 0x%x.\n", *(volatile __u32 *)(GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp*4), tmpGrp); */
	*(volatile __u32 *)(GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp * 4) |=
				(1 << tmpSrc);
	/*printk("GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp*4 = 0x%p. tmpGrp = 0x%x.\n", GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp*4, tmpGrp); */
	/*printk("GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp*4 = 0x%x. tmpGrp = 0x%x.\n", *(volatile __u32 *)(GicDDisc + GIC_DIST_ENABLE_SET + tmpGrp*4), tmpGrp); */
	/*printk("tmpSrc = 0x%x.\n", tmpSrc); */

	/*need to care mask or priority? */

	return 0;
}

__s32 mem_query_int(enum interrupt_source_e src)
{
	__s32 result = 0;
	__u32 tmpGrp = (__u32) src >> 5;
	__u32 tmpSrc = (__u32) src & 0x1f;

	if (src == 0)
		return -1;

	result = *(volatile __u32 *)(GicDDisc + GIC_DIST_PENDING_SET
				+ tmpGrp * 4) & (1 << tmpSrc);

	/*printk("GicDDisc + GIC_DIST_PENDING_SET + tmpGrp*4 = 0x%x. tmpGrp = 0x%x.\n", GicDDisc + GIC_DIST_PENDING_SET + tmpGrp*4, tmpGrp); */
	/*printk("tmpSrc = 0x%x. result = 0x%x.\n", tmpSrc, result); */

	return result ? 0 : -1;
}
