/*
 * Copyright (C) 2016 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adjustable factor-based clock implementation
 */

#include "clk-sun8iw15.h"

/*
 * freq table from hardware, need follow rules
 * 1)   each table  named as
 *      factor_pll1_tbl
 *      factor_pll2_tbl
 *      ...
 * 2) for each table line
 *      a) follow the format PLLx(n, k, m, p, d1, d2, freq), and keep the
 *         factors order
 *      b) if any factor not used, skip it
 *      c) the factor is the value to write registers, not means factor + 1
 *
 *      example
 *      PLL1(9, 0, 0, 2, 60000000) means PLL1(n, k, m, p, freq)
 *      PLLVIDEO0(3, 0, 96000000) means PLLVIDEO0(n, m, freq)
 *
 */

/* PLLCPU(n, m, p, freq)	F_N8X8_M0X2_P16x2 */
struct sunxi_clk_factor_freq factor_pllcpu_tbl[] = {
PLLCPU(23,      1,     288000000U),
};
/*n  m1   m0   freq */
/* PLLDDR(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllddr_tbl[] = {
PLLDDR(11,     0,     0,     288000000U),
PLLDDR(48,     1,     1,     294000000U),
PLLDDR(24,     0,     0,     600000000U),
PLLDDR(99,     0,     0,     2400000000U),
};

/* PLLPERIPH0(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllperiph0_tbl[] = {
PLLPERIPH0(23,     0,     0,     288000000U),
PLLPERIPH0(24,     0,     0,     300000000U),
PLLPERIPH0(49,     0,     0,     600000000U),
};

/* PLLPERIPH1(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllperiph1_tbl[] = {
PLLPERIPH1(23,     0,     0,     288000000U),
PLLPERIPH1(24,     0,     0,     300000000U),
PLLPERIPH1(49,     0,     0,     600000000U),
};

/* PLLGPU(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllgpu_tbl[] = {
PLLGPU(11,     0,     0,     288000000U),
PLLGPU(48,     1,     1,     294000000U),
PLLGPU(24,     0,     1,     300000000U),
PLLGPU(24,     0,     0,     600000000U),
PLLGPU(41,     0,     0,     1008000000U),
};

/* PLLVIDEO0(n, d1, freq)	F_N8X8_D1V1X1 */
struct sunxi_clk_factor_freq factor_pllvideo0_tbl[] = {
PLLVIDEO0(47,     0,     288000000U),
PLLVIDEO0(96,     1,     291000000U),
PLLVIDEO0(48,     0,     294000000U),
PLLVIDEO0(98,     1,     297000000U),
PLLVIDEO0(49,     0,     300000000U),
PLLVIDEO0(99,     0,     600000000U),
PLLVIDEO0(200,     1,     603000000U),
};

/* PLLVIDEO1(n, d1, freq)	F_N8X8_D1V1X1 */
struct sunxi_clk_factor_freq factor_pllvideo1_tbl[] = {
PLLVIDEO1(47,     0,     288000000U),
PLLVIDEO1(96,     1,     291000000U),
PLLVIDEO1(48,     0,     294000000U),
PLLVIDEO1(98,     1,     297000000U),
PLLVIDEO1(49,     0,     300000000U),
PLLVIDEO1(99,     0,     600000000U),
PLLVIDEO1(200,     1,     603000000U),
};

/* PLLVE(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllve_tbl[] = {
PLLVE(11,     0,     0,     288000000U),
PLLVE(48,     1,     1,     294000000U),
PLLVE(24,     0,     1,     300000000U),
PLLVE(24,     0,     0,     600000000U),
PLLVE(82,     0,     1,     996000000U),
};

/* PLLDE(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllde_tbl[] = {
PLLDE(11,     0,     0,     288000000U),
PLLDE(48,     1,     1,     294000000U),
PLLDE(24,     0,     1,     300000000U),
PLLDE(24,     0,     0,     600000000U),
PLLDE(100,     1,     1,     606000000U),
PLLDE(82,     0,     1,     996000000U),
};

static unsigned int pllcpu_max, pllddr_max, pllperiph0_max,
		    pllperiph1_max, pllgpu_max, pllvideo0_max, pllvideo1_max,
		    pllve_max, pllde_max;

#define PLL_MAX_ASSIGN(name) (pll##name##_max = \
	factor_pll##name##_tbl[ARRAY_SIZE(factor_pll##name##_tbl)-1].freq)

void sunxi_clk_factor_initlimits(void)
{
	PLL_MAX_ASSIGN(cpu);
	PLL_MAX_ASSIGN(ddr);
	PLL_MAX_ASSIGN(periph0);
	PLL_MAX_ASSIGN(periph1);
	PLL_MAX_ASSIGN(gpu);
	PLL_MAX_ASSIGN(video0);
	PLL_MAX_ASSIGN(video1);
	PLL_MAX_ASSIGN(ve);
	PLL_MAX_ASSIGN(de);
}
