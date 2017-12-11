
#ifndef _LINUX_SCENELOCK_DATA_SUN8IW7P1_H
#define _LINUX_SCENELOCK_DATA_SUN8IW7P1_H

#include <linux/power/axp_depend.h>

/* pwr_dm_en: bit0:pwr_sys, bit1:pwr_io, bit2:pwr_cpu
 * bus_change: bit0:axi, bit1:ahb1/apb1, bit2:ahb2
 * osc_en: bit0:HOSC
 */
struct scene_extended_standby_t extended_standby[8] = {
	{
		{
			.id				= TALKING_STANDBY_FLAG,
			.pwr_dm_en		= 0xfb,       /* pwr_cpu is closed */
			.osc_en 		= 0xf,        /* mean Hosc is on */
			.init_pll_dis	= 0x1fff,     /* disable all plls */
			.exit_pll_en	= 0x1fff,
			.pll_change		= 0,          /* all pll are closed */
			.bus_change		= 0x3,
			.bus_factor[0]	= {0, 0, 0, 0, 0}, /* switch cpu/axi src to IOSC */
			.bus_factor[1]	= {0, 0, 1, 0, 0}, /* switch ahb1/apb1 src to IOSC */
		},
		.scene_type	= SCENE_TALKING_STANDBY,
		.name		= "talking_standby",
	},
	{
		{
			.id				= USB_STANDBY_FLAG,
			.pwr_dm_en		= 0xfb,       /* pwr_cpu is closed */
			.osc_en			= 0xf,        /* mean Hosc is on */
			.init_pll_dis	= 0x1fff,     /* disable all plls */
			.exit_pll_en	= 0x1fff,
			.pll_change		= 0,          /* all pll are closed */
			.bus_change		= 0x3,
			.bus_factor[0]	= {0, 0, 0, 0, 0}, /* switch cpu/axi src to IOSC */
			.bus_factor[1]	= {0, 0, 1, 0, 0}, /* switch ahb1/apb1 src to IOSC */
		},
		.scene_type	= SCENE_USB_STANDBY,
		.name		= "usb_standby",
	},
	{
		{
			.id	= MP3_STANDBY_FLAG,
		},
		.scene_type	= SCENE_MP3_STANDBY,
		.name		= "mp3_standby",
	},
	{
		{
			.id	= BOOT_FAST_STANDBY_FLAG,
		},
		.scene_type	= SCENE_BOOT_FAST,
		.name		= "boot_fast",
	},
	{
		{
			.id				= SUPER_STANDBY_FLAG,
			.pwr_dm_en		= 0xfb,       /* pwr_cpu is closed */
			.osc_en			= 0x0,        /* mean Hosc is off */
			.init_pll_dis	= 0x1fff,     /* disable all plls */
			.exit_pll_en	= 0x1fff,
			.pll_change		= 0,          /* all pll are closed */
			.bus_change		= 0x3,
			.bus_factor[0]	= {0, 0, 0, 0, 0}, /* switch cpu/axi src to IOSC*/
			.bus_factor[1]	= {0, 0, 1, 0, 0}, /* switch ahb1/apb1 src to IOSC */
		},
		.scene_type	= SCENE_SUPER_STANDBY,
		.name		= "super_standby",
	},
	{
		{
			.id				= NORMAL_STANDBY_FLAG,
			.pwr_dm_en		= 0xfb,       /* pwr_cpu is closed */
			.osc_en			= 0xf,        /* mean Hosc is on */
			.init_pll_dis	= 0x1fff,     /* disable all plls */
			.exit_pll_en	= 0x1fff,
			.pll_change		= 0,          /* all pll are closed */
			.bus_change		= 0x3,
			.bus_factor[0]	= {0, 0, 0, 0, 0}, /* switch cpu/axi src to IOSC */
			.bus_factor[1]	= {0, 0, 1, 0, 0}, /* switch ahb1/apb1 src to IOSC */
		},
		.scene_type	= SCENE_NORMAL_STANDBY,
		.name		= "normal_standby",
	},
	{
		{
			.id				= GPIO_STANDBY_FLAG,
			.pwr_dm_en		= 0xfb,       /* pwr_cpu is closed */
			.osc_en			= 0xf,        /* mean Hosc is on */
			.init_pll_dis	= 0x1fff,     /* disable all plls */
			.exit_pll_en	= 0x1fff,
			.pll_change		= 0,          /* all pll are closed */
			.bus_change		= 0x3,
			.bus_factor[0]	= {0, 0, 0, 0, 0}, /* switch cpu/axi src to IOSC */
			.bus_factor[1]	= {0, 0, 1, 0, 0}, /* switch ahb1/apb1 src to IOSC */
		},
		.scene_type	= SCENE_GPIO_STANDBY,
		.name		= "gpio_standby",
	},
	{
		{
			.id		= MISC_STANDBY_FLAG,
		},
		.scene_type	= SCENE_MISC_STANDBY,
		.name		= "misc_standby",
	},
};
#endif


