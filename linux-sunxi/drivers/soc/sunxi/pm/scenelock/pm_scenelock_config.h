/*
 * drivers/soc/sunxi/pm/scenelock/pm_scenelock_cfg.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _PM_SCENELOCK_CFG_H
#define _PM_SCENELOCK_CFG_H

#if defined(CONFIG_ARCH_SUN8IW11P1)
#include "../platform/sun8iw11p1/pm_config_scenelock_sun8iw11p1.h"
#elif defined(CONFIG_ARCH_SUN8IW12P1)
#include "../platform/sun8iw12p1/pm_config_scenelock_sun8iw12p1.h"
#elif defined(CONFIG_ARCH_SUN50IW1P1)
#include "../platform/sun50iw1p1/pm_config_scenelock_sun50iw1p1.h"
#elif defined(CONFIG_ARCH_SUN50IW3P1)
#include "../platform/sun50iw3p1/pm_config_scenelock_sun50iw3p1.h"
#else
#error "please select a platform\n"
#endif

extern int extended_standby_cnt;
#endif /* _PM_SCENELOCK_CFG_H */

