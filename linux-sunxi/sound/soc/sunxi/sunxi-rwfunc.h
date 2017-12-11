/*
 * sound\soc\sunxi\sunxi-rwfunc.h
 * (C) Copyright 2014-2017
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * wolfgang huang <huangjinhui@allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef __SUNXI_RWFUNC_H__
#define __SUNXI_RWFUNC_H__
#include <linux/io.h>
#include <linux/module.h>

/* SUNXI_PR_CFG register */
#define SUNXI_PR_CFG_RST	28
#define SUNXI_PR_CFG_WR		24
#define SUNXI_PR_CFG_ADDR	16
#define SUNXI_PR_CFG_IN		8
#define SUNXI_PR_CFG_OUT	0

#if defined(CONFIG_ARCH_SUN50IW3) || defined(CONFIG_ARCH_SUN8IW12)
#define SUNXI_PR_CFG_ADDR_MASK		0x3F
#else
#define SUNXI_PR_CFG_ADDR_MASK		0x1f
#endif
#define SUNXI_PR_CFG_DATA_MASK		0xFF
#define SUNXI_PR_CFG_DATA_WIDTH		8

/* Sunxi Embedded Analog register read interface */
extern unsigned int read_prcm_wvalue(unsigned int, void __iomem *);
/* Sunxi Embedded Analog register write interface */
extern void write_prcm_wvalue(unsigned int, unsigned int, void __iomem *);

#endif	/* __SUNXI_RWFUNC_H__ */
