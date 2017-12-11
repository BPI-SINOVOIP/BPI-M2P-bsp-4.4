/*
 * drivers/soc/sunxi/pm/pm_config.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _PM_CONFIG_H
#define _PM_CONFIG_H

#include "uapi/linux/const.h"
#include "asm-generic/sizes.h"

/*hardware resource description*/
#if defined(CONFIG_ARCH_SUN8IW11P1)
#include "platform/sun8iw11p1/pm_config_sun8iw11p1.h"
#elif defined(CONFIG_ARCH_SUN8IW12P1)
#include "platform/sun8iw12p1/pm_config_sun8iw12p1.h"
#elif defined(CONFIG_ARCH_SUN50IW1P1)
#include "platform/sun50iw1p1/pm_config_sun50iw1p1.h"
#elif defined(CONFIG_ARCH_SUN50IW3P1)
#include "platform/sun50iw3p1/pm_config_sun50iw3p1.h"
#else
#error "please select a platform\n"
#endif

#ifndef IO_ADDRESS
#define IO_ADDRESS(x)  ((x) + 0xf0000000)
#endif

/*#define CHECK_IC_VERSION*/

/*#define RETURN_FROM_RESUME0_WITH_MMU    //suspend: 0xf000, resume0: 0xc010, resume1: 0xf000*/
/*#define RETURN_FROM_RESUME0_WITH_NOMMU // suspend: 0x0000, resume0: 0x4010, resume1: 0x0000*/
/*#define DIRECT_RETURN_FROM_SUSPEND //not support yet*/
/*#define ENTER_SUPER_STANDBY_WITH_NOMMU //not support yet, suspend: 0x0000, resume0: 0x4010, resume1: 0x0000*/
/*#define WATCH_DOG_RESET*/

#if defined(CONFIG_ARCH_SUN8I) || defined(CONFIG_ARCH_SUN9IW1P1)
#define CORTEX_A7
#endif

/*interrupt src definition.*/
#define AW_IRQ_TIMER0			(SUNXI_IRQ_TIMER0)

/*platform independent src config.*/

#define AW_SRAM_A1_BASE			(SUNXI_SRAM_A1_PBASE)
#define AW_SRAM_A2_BASE			(SUNXI_SRAM_A2_PBASE)
#define AW_SRAM_A3_BASE			(SUNXI_SRAM_A3_PBASE)

#define AW_SRAM_C_BASE			(SUNXI_SRAM_C_PBASE)
#define AW_PIO_BASE			(SUNXI_PIO_PBASE)

#ifndef CONFIG_ARCH_SUN8IW8P1
#define AW_R_PRCM_BASE			(SUNXI_R_PRCM_PBASE)
#define AW_TWI2_BASE			(SUNXI_TWI2_PBASE)
#define AW_MSGBOX_BASE			(SUNXI_MSGBOX_PBASE)
#define AW_SPINLOCK_BASE		(SUNXI_SPINLOCK_PBASE)
#define AW_R_PIO_BASE			(SUNXI_R_PIO_PBASE)
#endif

#define AW_R_CPUCFG_BASE		(SUNXI_R_CPUCFG_PBASE)
#define AW_UART0_BASE			(SUNXI_UART0_PBASE)

#define AW_TWI0_BASE			(SUNXI_TWI0_PBASE)
#define AW_TWI1_BASE			(SUNXI_TWI1_PBASE)
#define AW_CPUCFG_P_REG0		(SUNXI_CPUCFG_P_REG0)
#define AW_CPUCFG_GENCTL		(SUNXI_CPUCFG_GENCTL)
#define AW_CPUX_PWR_CLAMP(x)		(SUNXI_CPUX_PWR_CLAMP(x))
#define AW_CPUX_PWR_CLAMP_STATUS(x)	(SUNXI_CPUX_PWR_CLAMP_STATUS(x))
#define AW_CPU_PWROFF_REG		(SUNXI_CPU_PWROFF_REG)

#define SRAM_CTRL_REG1_ADDR_PA		(0x01c00004)
#define SRAM_CTRL_REG1_ADDR_VA		IO_ADDRESS(SRAM_CTRL_REG1_ADDR_PA)

#define RUNTIME_CONTEXT_SIZE		(14) /*r0-r13 */

#define DRAM_COMPARE_DATA_ADDR		(0xc0100000) /*1Mbytes offset */
#define DRAM_COMPARE_SIZE		(0x10000) /*? */

#define UL(x)				_AC(x, UL)

#define SUSPEND_FREQ			(720000) /*720M */
#define SUSPEND_DELAY_MS		(10000)

#endif /*_PM_CONFIG_H*/
