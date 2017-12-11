/*
 * drivers/soc/sunxi/pm/pm_config.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/power/aw_pm.h>
#include <linux/power/axp_depend.h>
#include <linux/power/scenelock.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/suspend.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include "pm.h"
#include "pm_config.h"
#include "resource/mem_int.h"
#include "resource/mem_tmr.h"
#include "resource/mem_gpio.h"
#include "resource/mem_twi.h"
#include "resource/mem_usb.h"
#include "resource/mem_ir.h"
#include "resource/mem_key.h"

#define pm_printk(mask, format, args...)     \
do {                                         \
	if (unlikely(pm_debug_mask & mask))  \
		pr_info(format, ##args);     \
} while (0)

__u32 pm_debug_mask = PM_STANDBY_TEST;

struct super_standby_para super_standby_para_info;
static struct __mem_tmr_reg_t saved_tmr_state;

struct aw_pm_info standby_info = {
	.standby_para = {
		.event = CPU0_MEM_WAKEUP,
		.axp_event = CPUS_MEM_WAKEUP,
		.timeout = 0,
	},
	.pmu_arg = {
		.twi_port = 0,
		.dev_addr = 0x34,
	},
};

int pm_get_dev_info(char *name, int index, unsigned int **base,
				unsigned int *len)
{
	struct device_node *np;
	u32 reg[4 * (index + 1)];
	int ret = 0;

	np = of_find_node_by_type(NULL, name);
	if (np == NULL) {
		pr_err("can not find np for %s.\n", name);
		ret = -1;
	} else {
		of_property_read_u32_array(np, "reg", reg, ARRAY_SIZE(reg));
		*base = (u32 *) ((phys_addr_t) reg[1 + index * 4]);
		pr_debug("%s physical base = 0x%p.\n", name, *base);

		*len = reg[3 + index * 4];
		*base = of_iomap(np, index);
		pr_debug("virtual base = 0x%p.\n", *base);
	}

	return ret;
}

#ifdef CONFIG_SUNXI_SUSPEND_NONARISC
static struct clk_state saved_clk_state;
static struct gpio_state saved_gpio_state;
static struct ccm_state saved_ccm_state;
static struct sram_state saved_sram_state;

void mem_device_init(void)
{
	mem_tmr_init();
	mem_gpio_init();
	mem_sram_init();
	mem_int_init();
	mem_clk_init(1);
}

void mem_device_save(void)
{
	/* backup device state */
	mem_twi_save(0);
	mem_ccu_save(&(saved_ccm_state));
	mem_clk_save(&(saved_clk_state));
	mem_tmr_save(&(saved_tmr_state));
	mem_gpio_save(&(saved_gpio_state));
	mem_sram_save(&(saved_sram_state));
	mem_int_save();
#ifdef CONFIG_AW_AXP
	axp_mem_save();
#endif
}

void mem_device_restore(void)
{
#ifdef CONFIG_AW_AXP
	axp_mem_restore();
#endif
	mem_sram_restore(&(saved_sram_state));
	mem_gpio_restore(&(saved_gpio_state));
	mem_tmr_restore(&(saved_tmr_state));
	mem_clk_restore(&(saved_clk_state));
	mem_ccu_restore(&(saved_ccm_state));
	mem_int_restore();
	mem_twi_restore();
}

static void mem_enable_nmi(void)
{
	u32 tmp = 0;

	tmp = readl((volatile void *)SUNXI_IRQ_NMI_ADDR);
	tmp |= ((0x0000001));
	writel(tmp, (volatile void *)SUNXI_IRQ_NMI_ADDR);
}

void init_wakeup_src(unsigned int event, unsigned int gpio_enable_bitmap,
				unsigned int cpux_gpiog_bitmap)
{
	/* config int src. */
	/* initialise standby modules */
	if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
		pr_info("final standby wakeup src config = 0x%x.\n", event);

	/* init some system wake source */
	if (event & CPU0_WAKEUP_MSGBOX) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPU0_WAKEUP_MSGBOX.\n");

		mem_enable_int(INT_SOURCE_MSG_BOX);
	}

	if (event & CPU0_WAKEUP_EXINT) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPU0_WAKEUP_EXINT.\n");

		mem_enable_int(INT_SOURCE_EXTNMI);
		mem_enable_nmi();
	}

	if (event & CPU0_WAKEUP_TIMEOUT) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPU0_WAKEUP_TIMEOUT.\n");

		/* set timer for power off */
		if (standby_info.standby_para.timeout) {
			pr_info("wakeup sys in %d msec later.\n",
					standby_info.standby_para.timeout);
			mem_tmr_set(standby_info.standby_para.timeout);
			mem_enable_int(INT_SOURCE_TIMER0);
		}
	}

	if (event & CPU0_WAKEUP_ALARM) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPU0_WAKEUP_ALARM.\n");

		mem_enable_int(INT_SOURCE_ALARM);
	}

	if (event & CPU0_WAKEUP_KEY) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPU0_WAKEUP_KEY.\n");

		mem_key_init();
		mem_enable_int(INT_SOURCE_LRADC);
	}

	if (event & CPU0_WAKEUP_IR) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPU0_WAKEUP_IR.\n");

		mem_ir_init();
		mem_enable_int(INT_SOURCE_IR0);
		mem_enable_int(INT_SOURCE_IR1);
	}

	if (event & CPU0_WAKEUP_USB) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPU0_WAKEUP_USB.\n");

		mem_usb_init();
		mem_enable_int(INT_SOURCE_USBOTG);
		mem_enable_int(INT_SOURCE_USBEHCI0);
		mem_enable_int(INT_SOURCE_USBEHCI1);
		mem_enable_int(INT_SOURCE_USBEHCI2);
		mem_enable_int(INT_SOURCE_USBOHCI0);
		mem_enable_int(INT_SOURCE_USBOHCI1);
		mem_enable_int(INT_SOURCE_USBOHCI2);
	}

	if (event & CPUS_WAKEUP_GPIO) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPUS_WAKEUP_GPIO.\n");

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('A')))
			mem_enable_int(INT_SOURCE_GPIOA);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('B')))
			mem_enable_int(INT_SOURCE_GPIOB);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('C')))
			mem_enable_int(INT_SOURCE_GPIOC);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('D')))
			mem_enable_int(INT_SOURCE_GPIOD);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('E')))
			mem_enable_int(INT_SOURCE_GPIOE);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('F')))
			mem_enable_int(INT_SOURCE_GPIOF);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('G')))
			mem_enable_int(INT_SOURCE_GPIOG);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('H')))
			mem_enable_int(INT_SOURCE_GPIOH);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('I')))
			mem_enable_int(INT_SOURCE_GPIOI);

		if (cpux_gpiog_bitmap & (WAKEUP_GPIO_GROUP('J')))
			mem_enable_int(INT_SOURCE_GPIOJ);

		mem_pio_clk_src_init();
	}

	if (event & CPUS_WAKEUP_WLAN) {
		if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY))
			pr_info("enable CPU0_WAKEUP_WLAN.\n");
		mem_enable_int(INT_SOURCE_WLAN);
	}
}

void exit_wakeup_src(unsigned int event, unsigned int gpio_enable_bitmap,
				unsigned int cpux_gpiog_bitmap)
{
	/* exit standby module */
	if (event & CPUS_WAKEUP_WLAN)
		;

	if (event & CPUS_WAKEUP_GPIO)
		mem_pio_clk_src_exit();

	if (event & CPU0_WAKEUP_USB)
		mem_usb_exit();

	if (event & CPU0_WAKEUP_IR)
		mem_ir_exit();

	if (event & CPU0_WAKEUP_ALARM)
		;

	if (event & CPU0_WAKEUP_KEY)
		mem_key_exit();

	/* exit standby module */
	if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY)) {
		/* restore serial clk & gpio config.
		 * serial_exit();
		 */
	}
}

void query_wakeup_source(struct aw_pm_info *arg)
{
	arg->standby_para.event = 0;

	arg->standby_para.event |= mem_query_int(INT_SOURCE_EXTNMI)   ? 0 : CPU0_WAKEUP_EXINT;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_USBOTG)   ? 0 : CPU0_WAKEUP_USB;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_USBEHCI0) ? 0 : CPU0_WAKEUP_USB;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_USBEHCI1) ? 0 : CPU0_WAKEUP_USB;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_USBEHCI2) ? 0 : CPU0_WAKEUP_USB;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_USBOHCI0) ? 0 : CPU0_WAKEUP_USB;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_USBOHCI1) ? 0 : CPU0_WAKEUP_USB;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_USBOHCI2) ? 0 : CPU0_WAKEUP_USB;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_LRADC)    ? 0 : CPU0_WAKEUP_KEY;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_IR0)      ? 0 : CPU0_WAKEUP_IR;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_ALARM)    ? 0 : CPU0_WAKEUP_ALARM;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_TIMER0)   ? 0 : CPU0_WAKEUP_TIMEOUT;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOA)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOB)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOC)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOD)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOE)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOF)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOG)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOH)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOI)    ? 0 : CPUS_WAKEUP_GPIO;
	arg->standby_para.event |= mem_query_int(INT_SOURCE_GPIOJ)    ? 0 : CPUS_WAKEUP_GPIO;
}

int fetch_and_save_dram_para(struct __dram_para_t *pstandby_dram_para)
{
	struct device_node *np;
	s32 ret = -EINVAL;

	np = of_find_compatible_node(NULL, NULL, "allwinner,dram");
	if (IS_ERR(np)) {
		pr_err("get [allwinner, dram] device node error\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_clk",
				&pstandby_dram_para->dram_clk);
	if (ret) {
		pr_err("standby :get dram_clk err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_type",
				&pstandby_dram_para->dram_type);
	if (ret) {
		pr_err("standby :get dram_type err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_zq", &pstandby_dram_para->dram_zq);
	if (ret) {
		pr_err("standby :get dram_zq err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_odt_en",
				&pstandby_dram_para->dram_odt_en);
	if (ret) {
		pr_err("standby :get dram_odt_en err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_para1",
				&pstandby_dram_para->dram_para1);
	if (ret) {
		pr_err("standby :get dram_para1 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_para2",
				&pstandby_dram_para->dram_para2);
	if (ret) {
		pr_err("standby :get dram_para2 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_mr0",
				&pstandby_dram_para->dram_mr0);
	if (ret) {
		pr_err("standby :get dram_mr0 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_mr1",
				&pstandby_dram_para->dram_mr1);
	if (ret) {
		pr_err("standby :get dram_mr1 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_mr2",
				&pstandby_dram_para->dram_mr2);
	if (ret) {
		pr_err("standby :get dram_mr2 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_mr3",
				&pstandby_dram_para->dram_mr3);
	if (ret) {
		pr_err("standby :get dram_mr3 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr0",
				&pstandby_dram_para->dram_tpr0);
	if (ret) {
		pr_err("standby :get dram_tpr0 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr1",
				&pstandby_dram_para->dram_tpr1);
	if (ret) {
		pr_err("standby :get dram_tpr1 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr2",
				&pstandby_dram_para->dram_tpr2);
	if (ret) {
		pr_err("standby :get dram_tpr2 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr3",
				&pstandby_dram_para->dram_tpr3);
	if (ret) {
		pr_err("standby :get dram_tpr3 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr4",
				&pstandby_dram_para->dram_tpr4);
	if (ret) {
		pr_err("standby :get dram_tpr4 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr5",
				&pstandby_dram_para->dram_tpr5);
	if (ret) {
		pr_err("standby :get dram_tpr5 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr6",
				&pstandby_dram_para->dram_tpr6);
	if (ret) {
		pr_err("standby :get dram_tpr6 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr7",
				&pstandby_dram_para->dram_tpr7);
	if (ret) {
		pr_err("standby :get dram_tpr7 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr8",
				&pstandby_dram_para->dram_tpr8);
	if (ret) {
		pr_err("standby :get dram_tpr8 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr9",
				&pstandby_dram_para->dram_tpr9);
	if (ret) {
		pr_err("standby :get dram_tpr9 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr9",
				&pstandby_dram_para->dram_tpr9);
	if (ret) {
		pr_err("standby :get dram_tpr9 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr10",
				&pstandby_dram_para->dram_tpr10);
	if (ret) {
		pr_err("standby :get dram_tpr10 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr11",
				&pstandby_dram_para->dram_tpr11);
	if (ret) {
		pr_err("standby :get dram_tpr11 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr12",
				&pstandby_dram_para->dram_tpr12);
	if (ret) {
		pr_err("standby :get dram_tpr12 err.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "dram_tpr13",
				&pstandby_dram_para->dram_tpr13);
	if (ret) {
		pr_err("standby :get dram_tpr13 err.\n");
		return -EINVAL;
	}

	return ret;
}

/*
 * sunxi_standby_enter() - Enter the system sleep state
 *
 * @state: suspend state
 * @return: return 0 is process successed
 * @note: the core function for platform sleep
 */
int sunxi_standby_enter(unsigned long arg)
{
	struct aw_pm_info *para = (struct aw_pm_info *)(arg);
	int (*standby)(struct aw_pm_info *arg);
	int ret = -1;

	/* clean d cache to the point of unification.
	 * __cpuc_coherent_kern_range(0xc0000000, 0xffffffff-1);
	 */

	/* move standby code to sram */
	memcpy((void *)SRAM_FUNC_START, (void *)&standby_bin_start,
	(unsigned int)&standby_bin_end - (unsigned int)&standby_bin_start);

	dmac_flush_range((void *)SRAM_FUNC_START, (void *)(SRAM_FUNC_START +
	((unsigned int)&standby_bin_end - (unsigned int)&standby_bin_start)));

	/* flush cache data to the memory.
	 * dmac_flush_range((void *)0xc0000000, (void *)(0xdfffffff-1));
	 */

	/* clean & invalidate dcache, icache. */
	flush_cache_all();

	standby = (int (*)(struct aw_pm_info *arg))SRAM_FUNC_START;

	ret = standby(para);
	if (ret == 0)
		soft_restart(virt_to_phys(cpu_resume));

	return ret;
}

#else
void mem_device_init(void)
{
	mem_tmr_init();
	mem_int_init();
}

void mem_device_save(void)
{
	mem_int_save();
	mem_tmr_save(&(saved_tmr_state));
}

void mem_device_restore(void)
{
	mem_tmr_restore(&(saved_tmr_state));
	mem_int_restore();
}

void init_wakeup_src(unsigned int event, unsigned int gpio_enable_bitmap,
				unsigned int cpux_gpiog_bitmap)
{
	if (event & CPU0_WAKEUP_TIMEOUT) {
		pr_info("enable CPU0_WAKEUP_TIMEOUT.\n");
		/* set timer for power off */
		if (super_standby_para_info.timeout) {
			mem_tmr_set(super_standby_para_info.timeout);
			mem_enable_int(INT_SOURCE_TIMER1);
		}
	}
}

void exit_wakeup_src(unsigned int event, unsigned int gpio_enable_bitmap,
				unsigned int cpux_gpiog_bitmap)
{
}

#ifndef CONFIG_ARM_PSCI_FW
static void __sunxi_suspend_cpu_die(void)
{
	unsigned long actlr;

	gic_cpu_if_down(0);

	/* step1: disable the data cache */
	asm("mrc p15, 0, %0, c1, c0, 0" : "=r" (actlr));
	actlr &= ~(1<<2);
	asm volatile("mcr p15, 0, %0, c1, c0, 0\n" : : "r" (actlr));

	/* step2: clean and ivalidate L1 cache */
	flush_cache_all();

	/* step3: execute a CLREX instruction */
	asm("clrex" : : : "memory", "cc");

	/*
	 * step4: switch cpu from SMP mode to AMP mode,
	 * aim is to disable cache coherency
	 */
	asm("mrc p15, 0, %0, c1, c0, 1" : "=r" (actlr));
	actlr &= ~(1<<6);
	asm("mcr p15, 0, %0, c1, c0, 1\n" : : "r" (actlr));

	/* step5: execute an ISB instruction */
	isb();

	/* step6: execute a DSB instruction  */
	dsb();

	/*
	 * step7: if define trustzone, switch to secure os
	 * and then enter to wfi mode
	 */
#ifdef CONFIG_SUNXI_TRUSTZONE
	call_firmware_op(suspend);
#else
	/* step7: execute a WFI instruction */
	while (1)
		asm("wfi" : : : "memory", "cc");
#endif
}

/*
 * sunxi_standby_enter() - Enter the system sleep state
 *
 * @state: suspend state
 * @return: return 0 is process successed
 * @note: the core function for platform sleep
 */
int sunxi_standby_enter(unsigned long arg)
{
	struct super_standby_para *para = (struct super_standby_para *)(arg);

	arisc_standby_super(para, NULL, NULL);
	__sunxi_suspend_cpu_die();

	return 0;
}
#endif
#endif

#ifdef CONFIG_AW_AXP
int config_pmu_para(void)
{
	int pmux_id = 0;

	if (!config_pmux_para(0, &standby_info, &pmux_id))
		extended_standby_set_pmu_id(0, pmux_id);
	else
		pr_err("pmu0 does not exist.\n");

	if (!config_pmux_para(1, &standby_info, &pmux_id))
		extended_standby_set_pmu_id(1, pmux_id);
	else
		pr_info("pmu1 does not exist.\n");

	return 0;
}

static unsigned int pwr_dm_mask_saved;

static int save_sys_pwr_state(const char *id)
{
	int bitmap = 0;

	bitmap = axp_is_sys_pwr_dm_id(id);
	if (bitmap != (-1))
		pwr_dm_mask_saved |= (0x1 << bitmap);
	else
		pm_printk(PM_STANDBY_PRINT_PWR_STATUS, "%s: is not sys\n", id);

	return 0;
}

int resume_sys_pwr_state(void)
{
	int i = 0, ret = -1;
	char *sys_name = NULL;

	for (i = 0; i < 32; i++) {
		if (pwr_dm_mask_saved & (0x1 << i)) {
			sys_name = axp_get_sys_pwr_dm_id(i);
			if (sys_name != NULL) {
				ret = axp_add_sys_pwr_dm(sys_name);
				if (ret < 0)
					pm_printk(PM_STANDBY_PRINT_PWR_STATUS,
					"%s: resume failed\n", sys_name);
			}
		}
	}

	pwr_dm_mask_saved = 0;

	return 0;
}

static int check_sys_pwr_dm_status(char *pwr_dm)
{
	char ldo_name[20] = "\0";
	char enable_id[20] = "\0";
	int ret = 0;
	int i = 0;

	ret = axp_get_ldo_name(pwr_dm, ldo_name);
	if (ret < 0) {
		pm_printk(PM_STANDBY_PRINT_PWR_STATUS,
				"	    %s: get %s failed. ret = %d\n",
				__func__, pwr_dm, ret);
		return -1;
	}

	ret = axp_get_enable_id_count(ldo_name);
	if (ret == 0) {
		pm_printk(PM_STANDBY_PRINT_PWR_STATUS,
			"%s: no child, use by %s, property: sys.\n",
			ldo_name, pwr_dm);
	} else {
		for (i = 0; i < ret; i++) {
			axp_get_enable_id(ldo_name, i, (char *)enable_id);
			pr_info("%s: active child id %d is: %s.",
				ldo_name, i, enable_id);
			/*need to check all enabled id is belong to sys_pwr_dm */
			if (axp_is_sys_pwr_dm_id(enable_id) != (-1)) {
				pm_printk(PM_STANDBY_PRINT_PWR_STATUS,
					"property: sys\n");
			} else {
				pm_printk(PM_STANDBY_PRINT_PWR_STATUS,
					"property: module\n");
				axp_del_sys_pwr_dm(pwr_dm);
				save_sys_pwr_state(pwr_dm);
				break;
			}
		}
	}

	return 0;
}

int check_pwr_status(void)
{
	check_sys_pwr_dm_status("vdd-cpua");
	check_sys_pwr_dm_status("vdd-cpub");
	check_sys_pwr_dm_status("vcc-dram");
	check_sys_pwr_dm_status("vdd-gpu");
	check_sys_pwr_dm_status("vdd-sys");
	check_sys_pwr_dm_status("vdd-vpu");
	check_sys_pwr_dm_status("vdd-cpus");
	check_sys_pwr_dm_status("vdd-drampll");
	check_sys_pwr_dm_status("vcc-lpddr");
	check_sys_pwr_dm_status("vcc-adc");
	check_sys_pwr_dm_status("vcc-pl");
	check_sys_pwr_dm_status("vcc-pm");
	check_sys_pwr_dm_status("vcc-io");
	check_sys_pwr_dm_status("vcc-cpvdd");
	check_sys_pwr_dm_status("vcc-ldoin");
	check_sys_pwr_dm_status("vcc-pll");
	check_sys_pwr_dm_status("vcc-pc");

	return 0;
}

int init_sys_pwr_dm(void)
{
	unsigned int sys_mask = 0;

	axp_add_sys_pwr_dm("vdd-cpua");
	/*axp_add_sys_pwr_dm("vdd-cpub"); */
	axp_add_sys_pwr_dm("vcc-dram");
	/*axp_add_sys_pwr_dm("vdd-gpu"); */
	axp_add_sys_pwr_dm("vdd-sys");
	/*axp_add_sys_pwr_dm("vdd-vpu"); */
	axp_add_sys_pwr_dm("vdd-cpus");
	/*axp_add_sys_pwr_dm("vdd-drampll"); */
	axp_add_sys_pwr_dm("vcc-lpddr");
	/*axp_add_sys_pwr_dm("vcc-adc"); */
	axp_add_sys_pwr_dm("vcc-pl");
	/*axp_add_sys_pwr_dm("vcc-pm"); */
	axp_add_sys_pwr_dm("vcc-io");
	/*axp_add_sys_pwr_dm("vcc-cpvdd"); */
	/*axp_add_sys_pwr_dm("vcc-ldoin"); */
	axp_add_sys_pwr_dm("vcc-pll");
	axp_add_sys_pwr_dm("vcc-pc");

	sys_mask = axp_get_sys_pwr_dm_mask();

	pr_info("after inited: sys_mask config = 0x%x.\n", sys_mask);

	return 0;
}

int config_dynamic_standby(void)
{
	enum SUNXI_POWER_SCENE type = SCENE_DYNAMIC_STANDBY;
	struct scene_extended_standby_t *local_standby;
	int enable = 0;
	int dram_selfresh_flag = 1;
	unsigned int vdd_cpua_vol = 0;
	unsigned int vdd_sys_vol = 0;
	struct device_node *np;
	char *name = "dynamic_standby_para";
	int i = 0;
	int ret = 0;

	/*get customer customized dynamic_standby config */
	np = of_find_node_by_type(NULL, name);
	if (np == NULL) {
		pr_err("Warning: can not find np for %s.\n", name);
	} else {
		if (!of_device_is_available(np))
			enable = 0;
		else
			enable = 1;

		pr_info("Warning: %s_enable = 0x%x.\n", name, enable);

		if (enable) {
			for (i = 0; i < extended_standby_cnt; i++) {
				if (type == extended_standby[i].scene_type) {
					/*config dram_selfresh flag; */
					local_standby = &(extended_standby[i]);
					if (of_property_read_u32 (np,
						"dram_selfresh_flag",
						&dram_selfresh_flag)) {
						pr_err("%s:%d fetch err.\n",
							__func__, __LINE__);
						dram_selfresh_flag = 1;
					}

					pr_info("dram selfresh flag = 0x%x.\n",
						dram_selfresh_flag);

					if (dram_selfresh_flag == 0) {
						local_standby->soc_pwr_dep.soc_dram_state.selfresh_flag = dram_selfresh_flag;
						local_standby->soc_pwr_dep.soc_pwr_dm_state.state |= BITMAP(VDD_SYS_BIT);
						local_standby->soc_pwr_dep.cpux_clk_state.osc_en |= 0xf; /* mean all osc is on */
						/*mean pll5 is shutdowned & open by dram driver. */
						/*hsic can't closed. */
						/*periph is needed. */
						local_standby->soc_pwr_dep.cpux_clk_state.init_pll_dis |=
							(BITMAP(PM_PLL_HSIC) | BITMAP(PM_PLL_PERIPH) | BITMAP(PM_PLL_DRAM));
					}

					/*config other flag? */
				}

				/*config other extended_standby? */
			}

			if (!of_property_read_u32(np, "vdd_cpua_vol",
				&vdd_cpua_vol)) {
				pr_info("vdd_cpua_vol = 0x%x.\n", vdd_cpua_vol);
				ret = scene_set_volt(SCENE_DYNAMIC_STANDBY,
						VDD_CPUA_BIT, vdd_cpua_vol);
				if (ret < 0)
					pr_err("%s: set vdd_cpua volt failed\n",
						__func__);
			}

			if (!of_property_read_u32(np, "vdd_sys_vol",
				&vdd_sys_vol)) {
				pr_info("vdd_sys_vol = 0x%x.\n", vdd_sys_vol);
				ret = scene_set_volt(SCENE_DYNAMIC_STANDBY,
						VDD_SYS_BIT, vdd_sys_vol);
				if (ret < 0)
					pr_err("%s: set vdd_sys volt failed\n",
						__func__);
			}

			pr_info("enable dynamic_standby by customer.\n");
			scene_lock_state("dynamic_standby",
						sizeof("dynamic_standby"));
		}
	}

	return 0;
}

static int config_sys_pwr_dm(struct device_node *np, char *pwr_dm)
{
	int dm_enable = 0;

	if (!of_property_read_u32(np, pwr_dm, &dm_enable)) {
		pr_info("%s: dm_enalbe: %d.\n", pwr_dm, dm_enable);

		if (dm_enable == 0) {
			axp_del_sys_pwr_dm(pwr_dm);
			save_sys_pwr_state(pwr_dm);
		} else {
			axp_add_sys_pwr_dm(pwr_dm);
		}
	}

	return 0;
}

int config_sys_pwr(void)
{
	unsigned int sys_mask = 0;
	struct device_node *np;
	char *name = "sys_pwr_dm_para";

	np = of_find_node_by_type(NULL, name);
	if (np == NULL) {
		pr_info("info: can not find np for %s.\n", name);
	} else {
		config_sys_pwr_dm(np, "vdd-cpua");
		config_sys_pwr_dm(np, "vdd-cpub");
		config_sys_pwr_dm(np, "vcc-dram");
		config_sys_pwr_dm(np, "vdd-gpu");
		config_sys_pwr_dm(np, "vdd-sys");
		config_sys_pwr_dm(np, "vdd-vpu");
		config_sys_pwr_dm(np, "vdd-cpus");
		config_sys_pwr_dm(np, "vdd-drampll");
		config_sys_pwr_dm(np, "vcc-lpddr");
		config_sys_pwr_dm(np, "vcc-adc");
		config_sys_pwr_dm(np, "vcc-pl");
		config_sys_pwr_dm(np, "vcc-pm");
		config_sys_pwr_dm(np, "vcc-io");
		config_sys_pwr_dm(np, "vcc-cpvdd");
		config_sys_pwr_dm(np, "vcc-ldoin");
		config_sys_pwr_dm(np, "vcc-pll");
	}

	sys_mask = axp_get_sys_pwr_dm_mask();

	pr_info("after customized: sys_mask config = 0x%x.\n", sys_mask);

	return 0;
}
#endif
