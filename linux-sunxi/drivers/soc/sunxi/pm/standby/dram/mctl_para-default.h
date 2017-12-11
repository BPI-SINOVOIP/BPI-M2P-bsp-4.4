/*
 * drivers/soc/sunxi/pm/standby/dram/mctl_para-default.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _MCTL_PARA_DEFAULT_H
#define _MCTL_PARA_DEFAULT_H

struct __dram_para_t {
	/* normal configuration */
	unsigned int dram_clk;
	/* dram_type DDR2:2 DDR3:3 LPDDR2:6 LPDDR3:7 DDR3L:31 */
	unsigned int dram_type;
	/* LPDDR2 type S4:0 S2:1 NVM:2 */
	/* unsigned int lpddr2_type; */
	/*do not need */
	unsigned int dram_zq;
	unsigned int dram_odt_en;

	/*control configuration */
	unsigned int dram_para1;
	unsigned int dram_para2;

	/*timing configuration */
	unsigned int dram_mr0;
	unsigned int dram_mr1;
	unsigned int dram_mr2;
	unsigned int dram_mr3;
	unsigned int dram_tpr0;	/* DRAMTMG0 */
	unsigned int dram_tpr1;	/* DRAMTMG1 */
	unsigned int dram_tpr2;	/* DRAMTMG2 */
	unsigned int dram_tpr3;	/* DRAMTMG3 */
	unsigned int dram_tpr4;	/* DRAMTMG4 */
	unsigned int dram_tpr5;	/* DRAMTMG5 */
	unsigned int dram_tpr6;	/* DRAMTMG8 */

	/*reserved for future use */
	unsigned int dram_tpr7;
	unsigned int dram_tpr8;
	unsigned int dram_tpr9;
	unsigned int dram_tpr10;
	unsigned int dram_tpr11;
	unsigned int dram_tpr12;
	unsigned int dram_tpr13;
};

#endif	/* _MCTL_PARA_DEFAULT_H */
