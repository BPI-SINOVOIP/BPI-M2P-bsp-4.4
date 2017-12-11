/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * charles <yanjianbo@allwinnertech.com>
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
#include <sys_config.h>
#include <asm/arch/timer.h>
#include <asm/armv7.h>
#include <asm/arch/platform.h>
#include <smc.h>
#include <fdt_support.h>
#include <fdtdec.h>
#include <sys_config.h>
#include <linux/err.h>
#include "boot_standby_new.h"
#include <sys_config_old.h>

DECLARE_GLOBAL_DATA_PTR;

#define SRAM_A2_SIZE            (0xc000)
#define H3_SUNXI_SECURE_MODE_WITH_SECUREOS              1

// system start mode
#define MODE_NULL				(0x0)
#define MODE_SHUTDOWN_OS		(0x1)
#define MODE_WAIT_WAKE_UP		(0x2)
#define MODE_RUN_OS				(0xf)

// run addr
#define SCRIPT_ADDR				(H3_CONFIG_SYS_SDRAM_BASE + 0x04000000)
#define BOOT_STANDBY_RUN_ADDR	(0x00040000)

extern void boot_standby_new_relocate(int addr, int reloc_off);
extern void sunxi_flush_allcaches(void);
extern int disable_interrupts(void);
extern int smc_load_arisc(uint image_addr, uint image_size, uint para_addr, uint para_size, uint para_offset);

extern unsigned int _standby_new_start__;
extern unsigned int _standby_new_end__;
extern unsigned int _start;
/*
************************************************************************************************************
*
*                                             function
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：
*
*
************************************************************************************************************
*/
static int read_start_mode(void)
{
	int mode = 0;
        mode =  smc_readl(RTC_GENERAL_PURPOSE_REG(3));
	return mode;
}

int sunxi_deassert_arisc(void)
{
    printf("set arisc reset to de-assert state\n");
    {
		volatile unsigned long value;
		value = smc_readl(SUNXI_RCPUCFG_BASE + 0x0);
		printf("In LINE %d: value = %08lx\n", __LINE__, value);
		value &= ~1;
		printf("In LINE %d: value = %08lx\n", __LINE__, value);
		smc_writel(value, SUNXI_RCPUCFG_BASE + 0x0);
		value = smc_readl(SUNXI_RCPUCFG_BASE + 0x0);
		printf("In LINE %d: value = %08lx\n", __LINE__, value);
		value |= 1;
		printf("In LINE %d: value = %08lx\n", __LINE__, value);
		smc_writel(value, SUNXI_RCPUCFG_BASE + 0x0);
		//dbg
		value = smc_readl(SUNXI_RCPUCFG_BASE + 0x0);
		printf("In LINE %d: value = %08lx\n", __LINE__, value);
    }

    return 0;
}


/*
************************************************************************************************************
*
*                                             function
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：
*
*
************************************************************************************************************
*/
static void write_start_mode(int mode)
{
	unsigned char value = mode & 0xff;

	smc_writel(value, RTC_GENERAL_PURPOSE_REG(3));
	return ;
}

/*
************************************************************************************************************
*
*                                             function
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ： index   0 :  待机灯		1：工作灯
*								status	0：	 输出低电平	1：输出高电平
*
************************************************************************************************************
*/
int standby_led_control(void)
{
	int ret = 0, nodeoffset = 0;
	char  tmp_gpio_name[16]={0};
	user_gpio_set_t tmp_gpio_config;
	int count = 0;

	nodeoffset = fdt_path_offset(working_fdt, "/soc/box_standby_led");
	if (nodeoffset < 0)
	{
		printf("[box_standby_led] can not be  used\n");
		return -1;
	}

	while(1)
	{
		sprintf(tmp_gpio_name, "gpio%d", count);
		ret = fdt_get_one_gpio("/soc/box_standby_led", tmp_gpio_name, &tmp_gpio_config);
		if(ret < 0) {
			break;
		}
		ret = gpio_request_early(&tmp_gpio_config, 1, 1);
		if(ret < 0)
		{
			printf("[box_standby_led] %s gpio_request_early failed\n", tmp_gpio_name);
			return -1;
		}
		count++;
	}
	return 0;
}

static void copy_script_to_sram(int addr)
{
	unsigned int length;

    length = *((unsigned int *)SUNXI_SYSCONFIG_SIZE_ADDR);

	if (length)
	{
		memcpy((void *)addr, (void *)SUNXI_SYSCONFIG_BUFF_ADDR, length);
	}
	else
	{
		printf("error: script's length is 0\n");
	}

}

/*
************************************************************************************************************
*
*                                             function
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：
*
*
************************************************************************************************************
*/
static int aw_suspend_cpu_die(void)
{
	unsigned long actlr;

	printf("[box standby] CPU0 go to WFI\n");
	/* step1: disable cache */
	asm("mrc    p15, 0, %0, c1, c0, 0" : "=r" (actlr) );
	actlr &= ~(1<<2);
	asm("mcr    p15, 0, %0, c1, c0, 0\n" : : "r" (actlr));

	/* step2: clean and ivalidate L1 cache */
	//sunxi_flush_allcaches();

	/* step3: execute a CLREX instruction */
	asm("clrex" : : : "memory", "cc");

	/* step4: switch cpu from SMP mode to AMP mode, aim is to disable cache coherency */
	asm("mrc    p15, 0, %0, c1, c0, 1" : "=r" (actlr) );
	actlr &= ~(1<<6);
	asm("mcr    p15, 0, %0, c1, c0, 1\n" : : "r" (actlr));

	/* step5: execute an ISB instruction */
	CP15ISB;
	/* step6: execute a DSB instruction  */
	CP15DSB;

	/* step7: execute a WFI instruction */
        while(1) {
                asm("wfi" : : : "memory", "cc");
        }

	return 0;
}

/*
************************************************************************************************************
*
*                                             function
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：
*
*
************************************************************************************************************
*/
static void clear_bss_for_boot_standby(int addr, int length)
{
	memset((void *)addr, 0x0, length);
}

/*
************************************************************************************************************
*
*                                             function
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：
*
*
************************************************************************************************************
*/
static void do_no_thing_loop(void)
{
}

/*
************************************************************************************************************
*
*                                             function
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：rtc = 1		android通过设置RTC = 1，然后重启系统，通过进入standby等待唤醒
*							 rtc = f		关机后，下次开机直接进入系统
*
************************************************************************************************************
*/
#if 0
static  void boxsdy_dump(char *str,unsigned char *data,\
	int len,int align)
{
	int i = 0;
	if(str)
		printf("\n%s: ",str);
	for(i = 0;i<len;i++)
	{
		if((i%align) == 0)
		{
			printf("\n");
		}
		printf("%02x ",*(data++));
	}
	printf("\n");
}
#endif
int do_box_standby(void)
{
	int mode;
	uint start_type;
	int standby_status = 0;
	int nodeoffset;
	int binary_len = 0;
	int arisc_addr = 0;
	if(uboot_spare_head.boot_data.work_mode != WORK_MODE_BOOT)
	{
		return 0;
	}

	mode = read_start_mode();
	printf("[box standby] read rtc = 0x%x\n", mode);

	nodeoffset = fdt_path_offset(working_fdt, "/soc/box_start_os0");
	if (IS_ERR_VALUE(fdt_path_offset(working_fdt, "/soc/box_start_os0"))) {
		return 0;
	}

	if (IS_ERR_VALUE(fdt_getprop_u32(working_fdt, nodeoffset, "start_type", &start_type))) {
		printf("[box standby] get start_type failed\n");
		return 0;
	}

	printf("[box standby] start_type = %d\n", start_type);
	if (start_type)
	{
		// 直接开机情况下，当检测到RTC = 1，无条件进入standby模式，否则启动系统
		if (mode != MODE_SHUTDOWN_OS)
		{
			printf("[box_start_os] mag be start_type no use\n");
			return 0;
		}
	}
	if (mode == MODE_SHUTDOWN_OS)
	{
		printf("[box standby] go to standby and wake up waiting ir\n");

		write_start_mode(MODE_NULL);			//清除工作模式

		standby_led_control();

		standby_status = 1;
	}
	else if (mode == MODE_RUN_OS)
	{
		printf("[box standby] start os\n");

		write_start_mode(MODE_NULL);

		return 0;
	}
	else
	{
		printf("[box standby] first start, so go to standby\n");

		standby_led_control();

		standby_status = 1;
	}
	if (standby_status)
	{
		disable_interrupts();

		printf("BOOT_STANDBY_RUN_ADDR:0x%x\n", BOOT_STANDBY_RUN_ADDR);
		binary_len = *((int *)SUNXI_CPUS_CODE_SIZE_ADDR);
		arisc_addr = SUNXI_CPUS_CODE_BUFF_ADDR;
		printf("arisc_addr:0x%x\n", arisc_addr);
		printf("binary_len:0x%x\n", binary_len);
		copy_script_to_sram(SCRIPT_ADDR);

		if(gd->securemode == H3_SUNXI_SECURE_MODE_WITH_SECUREOS)
		{
			sunxi_flush_allcaches();
			smc_load_arisc(arisc_addr, binary_len, 0, 0, 0);
		}
		else
		{
			clear_bss_for_boot_standby(BOOT_STANDBY_RUN_ADDR, SRAM_A2_SIZE);
			memcpy((void *)BOOT_STANDBY_RUN_ADDR, (void *)arisc_addr, binary_len);
		}
		sunxi_flush_allcaches();
		printf("param size:0x%x\n", *((unsigned int *)SUNXI_SYSCONFIG_SIZE_ADDR));
		/*sunxi_deassert_arisc*/
        sunxi_deassert_arisc();
		aw_suspend_cpu_die();
	}

	while (1) {
		do_no_thing_loop();
	}
}
