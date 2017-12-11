/*
 * drivers/soc/sunxi/pm/standby/standby_clock.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __STANDBY_CLOCK_H__
#define __STANDBY_CLOCK_H__

#include "../pm.h"

struct standby_clk_div_t {
	__u32 cpu_div:4;      /* division of cpu clock, divide core_pll */
	__u32 axi_div:4;      /* division of axi clock, */
	__u32 ahb_div:4;      /* division of ahb clock, */
	__u32 ahb_pre_div:4;  /* division of ahb clock, */
	__u32 apb_div:4;      /* division of apb clock, */
	__u32 apb_pre_div:4;  /* division of apb clock, */
	__u32 apb2_div:4;     /* division of apb2 clock, */
	__u32 apb2_pre_div:4; /* division of apb2 clock, */
};

#define PLL_CTRL_REG0_OFFSET	(0x40)
#define PLL_CTRL_REG1_OFFSET	(0x44)

__s32 standby_clk_init(void);
__s32 standby_clk_exit(void);
void standby_clk_set_keyfield(void);
void standby_clk_unset_keyfield(void);
__s32 standby_clk_core2losc(void);
__s32 standby_clk_core2hosc(void);
__s32 standby_clk_core2pll(void);
__s32 standby_clk_plldisable(void);
__s32 standby_clk_pllenable(void);
__s32 standby_clk_hoscdisable(void);
__s32 standby_clk_hoscenable(void);
__s32 standby_clk_setdiv(struct standby_clk_div_t *clk_div);
__s32 standby_clk_getdiv(struct standby_clk_div_t *clk_div);
__s32 standby_clk_apbinit(void);
__s32 standby_clk_apbexit(void);
__s32 standby_clk_apb2losc(void);
__s32 standby_clk_apb2hosc(void);
__s32 standby_clk_bus_src_backup(void);
__s32 standby_clk_bus_src_set(void);
__s32 standby_clk_bus_src_restore(void);
void standby_clk_dramgating(int onoff);
__u32 standby_clk_get_cpu_freq(void);

extern __u32 cpu_ms_loopcnt;
extern __s32 standby_clk_ldoenable(void);
extern __s32 standby_clk_get_pll_factor(struct pll_factor_t *pll_factor);
extern __s32 standby_clk_set_pll_factor(struct pll_factor_t *pll_factor);
extern __s32 standby_clk_ldodisable(void);

#endif /* __STANDBY_CLOCK_H__ */
