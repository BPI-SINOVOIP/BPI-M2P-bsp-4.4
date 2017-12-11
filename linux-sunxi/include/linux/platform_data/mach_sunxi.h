/*
 * Generic base addresses for Allwinner SunXi SoCs
 * These are used by SMP and Idle.
 *
 * Copyright (C) 2016 East yang
 *
 * East yang <yangdong@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __MACH_SUNXI_H
#define __MACH_SUNXI_H

#if defined(CONFIG_ARCH_SUN8IW11)
#define CPU_SOFT_ENTRY_REG0 (0xbc)
#elif defined(CONFIG_ARCH_SUN8IW7)
#define CPU_SOFT_ENTRY_REG0 (0x1a4)
#else
#define CPU_SOFT_ENTRY_REG0 (0x1bc)
#endif

extern void __iomem *sunxi_cpucfg_base;
extern void __iomem *sunxi_cpuscfg_base;
extern void __iomem *sunxi_sysctl_base;
extern void __iomem *sunxi_rtc_base;
extern void __iomem *sunxi_soft_entry_base;

#endif /* __MACH_SUNXI_H */
