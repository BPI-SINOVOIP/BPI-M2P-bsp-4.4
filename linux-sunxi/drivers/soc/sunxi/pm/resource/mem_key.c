/*
 * drivers/soc/sunxi/pm/resource/mem_key.c
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
#include "mem_key.h"
#include "../pm_config.h"

static struct __mem_key_reg_t *KeyReg;
static __u32 KeyCtrl, KeyIntc;
/*static __u32 KeyInts, KeyData0, KeyData1;*/

__s32 mem_key_init(void)
{
	/* set key register base */
	KeyReg = (struct __mem_key_reg_t *) (IO_ADDRESS(AW_LRADC01_BASE));

	/* backup LRADC registers */
	KeyCtrl = KeyReg->Lradc_Ctrl;
	KeyIntc = KeyReg->Lradc_Intc;
	KeyReg->Lradc_Ctrl = 0;

	/*note: mem_mdelay(10); */
	KeyReg->Lradc_Ctrl = (0x1 << 6) | (0x1 << 0);
	KeyReg->Lradc_Intc = (0x1 << 1);
	KeyReg->Lradc_Ints = (0x1 << 1);

	return 0;
}

__s32 mem_key_exit(void)
{
	KeyReg->Lradc_Ctrl = KeyCtrl;
	KeyReg->Lradc_Intc = KeyIntc;

	return 0;
}

__s32 mem_query_key(void)
{
	if (KeyReg->Lradc_Ints & 0x2) {
		KeyReg->Lradc_Ints = 0x2;
		return 0;
	}

	return -1;
}
