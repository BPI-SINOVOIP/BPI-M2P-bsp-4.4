/*
 * sound\soc\sunxi\sunxi-rwfunc.c
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

#include <linux/bitops.h>
#include "sunxi-rwfunc.h"

unsigned int read_prcm_wvalue(unsigned int addr, void __iomem *addr_pr_cfg)
{
	unsigned int reg;

	reg = readl(addr_pr_cfg);
	reg |= BIT(SUNXI_PR_CFG_RST);
	writel(reg, addr_pr_cfg);

	reg = readl(addr_pr_cfg);
	reg &= ~BIT(SUNXI_PR_CFG_WR);
	writel(reg, addr_pr_cfg);

	reg = readl(addr_pr_cfg);
	reg &= ~(SUNXI_PR_CFG_ADDR_MASK << SUNXI_PR_CFG_ADDR);
	reg |= (addr << SUNXI_PR_CFG_ADDR);
	writel(reg, addr_pr_cfg);

	reg = readl(addr_pr_cfg);
	reg &= SUNXI_PR_CFG_DATA_MASK;

	return reg;
}
EXPORT_SYMBOL_GPL(read_prcm_wvalue);

void write_prcm_wvalue(unsigned int addr, unsigned int val,
			void __iomem *addr_pr_cfg)
{
	unsigned int reg;

	reg = readl(addr_pr_cfg);
	reg |= BIT(SUNXI_PR_CFG_RST);
	writel(reg, addr_pr_cfg);

	reg = readl(addr_pr_cfg);
	reg &= ~(SUNXI_PR_CFG_ADDR_MASK << SUNXI_PR_CFG_ADDR);
	reg |= (addr << SUNXI_PR_CFG_ADDR);
	writel(reg, addr_pr_cfg);

	reg = readl(addr_pr_cfg);
	reg &= ~(SUNXI_PR_CFG_DATA_MASK << SUNXI_PR_CFG_IN);
	reg |= (val<<SUNXI_PR_CFG_IN);
	writel(reg, addr_pr_cfg);

	reg = readl(addr_pr_cfg);
	reg |= BIT(SUNXI_PR_CFG_WR);
	writel(reg, addr_pr_cfg);

	reg = readl(addr_pr_cfg);
	reg &= ~BIT(SUNXI_PR_CFG_WR);
	writel(reg, addr_pr_cfg);
}
EXPORT_SYMBOL_GPL(write_prcm_wvalue);
