/*
 *  arch/arm/mach-sun6i/arisc/arisc_i.h
 *
 * Copyright (c) 2012 Allwinner.
 * 2012-05-01 Written by sunny (sunny@allwinnertech.com).
 * 2012-10-01 Written by superm (superm@allwinnertech.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARISC_I_H__
#define __ARISC_I_H__

#include "./include/arisc_includes.h"
#include <asm/atomic.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <linux/reboot.h>
#include <linux/arisc/arisc.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/arisc/arisc-notifier.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/power/axp_depend.h>
#include <linux/sunxi-sid.h>
#include "hwmsgbox/hwmsgbox_i.h"
#include "message_manager/message_manager_i.h"
#include "include/arisc_dbgs.h"
#define DRV_NAME    "sunxi-arisc"
#define DEV_NAME    "sunxi-arisc"

#if defined CONFIG_ARCH_SUN8IW1P1
#define DRV_VERSION "1.00"
#elif defined CONFIG_ARCH_SUN8IW3P1
#define DRV_VERSION "1.01"
#elif defined CONFIG_ARCH_SUN8IW5P1
#define DRV_VERSION "1.02"
#elif defined CONFIG_ARCH_SUN8IW6P1
#define DRV_VERSION "1.03"
#elif defined CONFIG_ARCH_SUN8IW7P1
#define DRV_VERSION "1.04"
#include "arisc_platform/memory-sun8iw7p1.h"
#include "arisc_platform/reg-sun8iw7p1.h"
#elif defined CONFIG_ARCH_SUN8IW9P1
#define DRV_VERSION "1.05"
#elif defined CONFIG_ARCH_SUN8IW12P1
#define DRV_VERSION "1.06"
#elif defined CONFIG_ARCH_SUN8IW17P1
#define DRV_VERSION "1.17"
#elif defined CONFIG_ARCH_SUN50IW1P1
#define DRV_VERSION "1.10"
#elif defined CONFIG_ARCH_SUN50IW2P1
#define DRV_VERSION "1.20"
#elif defined CONFIG_ARCH_SUN50IW3P1
#define DRV_VERSION "1.30"
#elif defined CONFIG_ARCH_SUN50IW6P1
#define DRV_VERSION "1.60"
#elif defined CONFIG_ARCH_SUN9IW1P1
#define DRV_VERSION "2.00"
#else
#error "please select a platform\n"
#endif

extern unsigned int arisc_debug_dram_crc_en;
extern unsigned int arisc_debug_dram_crc_srcaddr;
extern unsigned int arisc_debug_dram_crc_len;
extern unsigned int arisc_debug_dram_crc_error;
extern unsigned int arisc_debug_dram_crc_total_count;
extern unsigned int arisc_debug_dram_crc_error_count;
extern unsigned int arisc_debug_level;
extern struct standby_info_para arisc_powchk_back;

extern void arisc_power_off(void);
extern char *arisc_binary_start;
extern char *arisc_binary_end;
/* local functions */
extern int arisc_config_dram_paras(void);
extern int arisc_sysconfig_ir_paras(void);
extern int arisc_config_pmu_paras(void);
extern int arisc_suspend_flag_query(void);
#ifdef CONFIG_ARM
extern asmlinkage int invoke_scp_fn_smc(u32 function_id, u32 arg0, u32 arg1,
					 u32 arg2);
#endif

#ifdef CONFIG_ARM64
extern asmlinkage int invoke_scp_fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2);
#endif
#if (defined CONFIG_ARCH_SUN8IW7P1)
extern void arisc_power_off(void);
#endif
#endif  /* __ARISC_I_H__ */
