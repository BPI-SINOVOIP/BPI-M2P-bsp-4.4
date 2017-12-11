/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/key.h>
#include <asm/arch/sys_proto.h>
#include <sys_config.h>
#include <power/sunxi/pmu.h>
#include <fdt_support.h>
#include <asm/arch/ccmu.h>
#include <asm/arch/timer.h>



__attribute__((section(".data")))
static uint32_t keyen_flag = 1;

/*__weak int sunxi_key_clock_open(void)
{
	return 0;
}

__weak int sunxi_key_clock_close(void)
{
	return 0;
}*/

int sunxi_key_init(void)
{
	uint reg_val = 0;
	int nodeoffset;

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

	nodeoffset = fdt_path_offset(working_fdt,FDT_PATH_KEY_DETECT);
	if(nodeoffset > 0)
	{
		fdt_getprop_u32(working_fdt,nodeoffset,"keyen_flag",&keyen_flag);
	}

	return 0;
}

int sunxi_key_exit(void)
{
	*GP_CTRL = 0;
	/* disable all key irq */
	*GP_DATA_INTC = 0;
	*GP_DATA_INTS = 0;

	sunxi_key_clock_close();

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


int do_key_test(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	u32 power_key = 0;
	int nodeoffset;

	nodeoffset = fdt_path_offset(working_fdt,FDT_PATH_KEY_DETECT);
	if(nodeoffset > 0)
	{
		fdt_getprop_u32(working_fdt,nodeoffset,"keyen_flag",&keyen_flag);
	}
	if(!keyen_flag)
	{
		puts("warnning: not support,please set keyen_flag=1 in sys_config.fex\n");
		return -1;
	}
	puts(" press a key:\n");
	*GP_DATA_INTS = 1;
	while(!ctrlc())
	{
		sunxi_key_read();
		power_key = axp_probe_key();
		if(power_key > 0) {
			break;
		}
	}

	return 0;

}

U_BOOT_CMD(
	key_test, 1, 0,	do_key_test,
	"Test the key value\n",
	""
);

