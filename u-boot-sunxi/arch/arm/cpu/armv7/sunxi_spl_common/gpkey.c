/*
 * (C) Copyright 2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * zhouhuacai <zhouhuacai@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/key.h>
#include <asm/arch/ccmu.h>

__attribute__((section(".data")))
static uint32_t keyen_flag = 1;


int sunxi_key_init(void)
{
	uint reg_val = 0;

	sunxi_key_clock_open();

	/*choose channel 0*/
	reg_val = *GP_CS_EN;
	reg_val |= 1;
	*GP_CS_EN= reg_val;

	/*choose continue work mode and enable ADC*/
	reg_val = *GP_CTRL;
	reg_val &= ~(1<<18);
	reg_val |= ((1<<19) | (1<<16));
	*GP_CTRL = reg_val;

	/* disable all key irq */
	*GP_DATA_INTC = 0;
	*GP_DATA_INTS= 1;

	return 0;
}


int sunxi_key_read(void)
{
	u32 ints;
	int key = -1;
	float vin;

	if(!keyen_flag)
	{
		return -1;
	}
	ints = *GP_DATA_INTS;
	/* clear the pending data */
	*GP_DATA_INTS |= (ints & 0x1);
	/* if there is already data pending,
	 read it */
	if(ints & GPADC0_DATA_PENDING)
	{
		vin = (*GP_CH0_DATA)*1.8/4095;
		if(vin > 1.6)
		{
			key = -1;
		}
		else
		{
			key = ((*GP_CH0_DATA)*63/4095);
			printf("key pressed value=0x%x\n", key);
		}
	}
	return key;
}


