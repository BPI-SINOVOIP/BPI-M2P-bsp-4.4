/*
 * drivers/soc/sunxi/pm/resource/mem_sram.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/types.h>
#include <asm/bug.h>
#include "../pm_config.h"
#include "../pm.h"
#include "../pm_config.h"

static u32 *sram_pbase;

__s32 mem_sram_init(void)
{
#ifdef CONFIG_FPGA_V4_PLATFORM
	sram_pbase = AW_SRAMCTRL_BASE;
#else
	u32 *base = 0;
	u32 sram_len = 0;
	int ret;

	ret = pm_get_dev_info("sram_ctrl", 0, &base, &sram_len);
	if (ret) {
		ret = pm_get_dev_info("sram-controller", 0, &base, &sram_len);
		if (!ret)
			sram_pbase = base;
		else
			return -1;
	} else {
		sram_pbase = base;
	}
#endif

	return 0;
}

__s32 mem_sram_save(struct sram_state *psram_state)
{
	int i = 0;

	/*save all the sram reg */
	for (i = 0; i < (SRAM_REG_LENGTH); i++)
		psram_state->sram_reg_back[i] =
				*(volatile __u32 *)((sram_pbase) + i);

	return 0;
}

__s32 mem_sram_restore(struct sram_state *psram_state)
{
	int i = 0;

	/*restore all the sram reg */
	for (i = 0; i < (SRAM_REG_LENGTH); i++)
		*(volatile __u32 *)((sram_pbase) + i) =
				psram_state->sram_reg_back[i];

	return 0;
}
