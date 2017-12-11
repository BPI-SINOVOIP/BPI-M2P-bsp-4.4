/*
 * drivers/soc/sunxi/pm/resource/mem_gpio.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __MEM_GPIO_H__
#define __MEM_GPIO_H__
#include "../pm.h"

struct gpio_state {
	__u32 gpio_reg_back[GPIO_REG_LENGTH];
};
__s32 mem_gpio_init(void);
__s32 mem_gpio_save(struct gpio_state *pgpio_state);
__s32 mem_gpio_restore(struct gpio_state *pgpio_state);

#if defined(CONFIG_ARCH_SUN9IW1P1)        \
	|| defined(CONFIG_ARCH_SUN8IW6P1) \
	|| defined(CONFIG_ARCH_SUN8IW8P1)
void config_gpio_clk(__u32 mmu_flag);
#endif

#endif /* __MEM_GPIO_H__ */
