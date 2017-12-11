/*
 * drivers/soc/sunxi/pm/platform/sun8iw12p1/pm_config_scenelock_sun8iw12p1.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _SCENELOCK_EXSTANDBY_CFG_SUN8IW12P1_H
#define _SCENELOCK_EXSTANDBY_CFG_SUN8IW12P1_H

#include <linux/power/axp_depend.h>

struct scene_extended_standby_t extended_standby[] = {
	{
		.scene_type     = SCENE_SUPER_STANDBY,
		.name           = "super_standby",
		.soc_pwr_dep.id = SUPER_STANDBY_FLAG,
		.soc_pwr_dep.soc_pwr_dm_state.state = BITMAP(VCC_DRAM_BIT)
						| BITMAP(VDD_CPUS_BIT)
						| BITMAP(VCC_LPDDR_BIT)
						| BITMAP(VCC_PL_BIT),
		.soc_pwr_dep.soc_pwr_dm_state.volt[0]     = 0x0,
		.soc_pwr_dep.cpux_clk_state.osc_en        = 0x0,
		.soc_pwr_dep.cpux_clk_state.init_pll_dis  = BITMAP(PM_PLL_DRAM),
		.soc_pwr_dep.cpux_clk_state.exit_pll_en   = 0x0,
		.soc_pwr_dep.cpux_clk_state.pll_change    = 0x0,
		.soc_pwr_dep.cpux_clk_state.bus_change    = 0x0,
		.soc_pwr_dep.soc_dram_state.selfresh_flag = 0x1,
		.soc_pwr_dep.soc_io_state.hold_flag       = 0x1,
		/*
		 * set pl5 and pl6 output low
		 * otherwise current will leak from vcc-pl to eldo1
		 */
		.soc_pwr_dep.soc_io_state.io_state[0]     = {0x07022000, 0x0ff00000, 0x01100000},
	},
	{
		.scene_type     = SCENE_NORMAL_STANDBY,
		.name           = "normal_standby",
		.soc_pwr_dep.id = NORMAL_STANDBY_FLAG,
		.soc_pwr_dep.soc_pwr_dm_state.state = BITMAP(VCC_DRAM_BIT)
						| BITMAP(VDD_CPUS_BIT)
						| BITMAP(VCC_LPDDR_BIT)
						| BITMAP(VCC_PL_BIT)
						| BITMAP(VDD_SYS_BIT)
						| BITMAP(VDD_CPUA_BIT)
						| BITMAP(VCC_IO_BIT)
						| BITMAP(VCC_PLL_BIT),
		.soc_pwr_dep.soc_pwr_dm_state.volt[0]     = 0x0,
		.soc_pwr_dep.cpux_clk_state.osc_en        = 0x0,
		.soc_pwr_dep.cpux_clk_state.init_pll_dis = BITMAP(PM_PLL_DRAM),
		.soc_pwr_dep.cpux_clk_state.exit_pll_en   = 0x0,
		.soc_pwr_dep.cpux_clk_state.pll_change    = 0x0,
		.soc_pwr_dep.cpux_clk_state.bus_change    = 0x0,
		.soc_pwr_dep.soc_dram_state.selfresh_flag = 0x1,
		.soc_pwr_dep.soc_io_state.hold_flag       = 0x1,
		/*
		 * set pl5 and pl6 output low
		 * otherwise current will leak from vcc-pl to eldo1
		 */
		.soc_pwr_dep.soc_io_state.io_state[0]     = {0x07022000, 0x0ff00000, 0x01100000},
	},
};

#endif /* _SCENELOCK_EXSTANDBY_CFG_SUN8IW12P1_H */
