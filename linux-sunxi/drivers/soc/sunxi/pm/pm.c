/*
 * drivers/soc/sunxi/pm/pm.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/power/aw_pm.h>
#include <linux/power/axp_depend.h>
#include <linux/power/scenelock.h>
#include <linux/console.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include "debug/pm_errcode.h"
#include "debug/pm_debug.h"
#include "resource/mem_mapping.h"
#include "resource/mem_serial.h"
#include "resource/mem_clk.h"
#include "pm.h"

#ifdef CONFIG_CPU_FREQ
int suspend_freq = SUSPEND_FREQ;
#endif
int suspend_delay_ms = SUSPEND_DELAY_MS;
standby_space_cfg_t standby_space;
struct aw_mem_para mem_para_info;
unsigned long time_to_wakeup_ms;
unsigned long cpu_resume_addr;

const struct extended_standby_manager_t *extended_standby_manager_id;

static int sunxi_suspend_valid(suspend_state_t state)
{
	pr_debug("%s\n", __func__);

	if (!((state > PM_SUSPEND_ON) && (state < PM_SUSPEND_MAX))) {
		pr_err("state (%d) invalid!\n", state);
		return 0;
	}

	return 1;
}

static int sunxi_suspend_begin(suspend_state_t state)
{
	static __u32 suspend_status;
	static int suspend_result; /* record last suspend status, 0/-1 */
	static bool bak_console_suspend_enabled;
	static bool backup_initcall_debug;
	static int backup_console_loglevel;
	static __u32 backup_debug_mask;

	pr_debug("%s\n", __func__);

	/* check rtc status, if err happened, do sth to fix it. */
	suspend_status = get_pm_secure_mem_status();
	if ((error_gen(MOD_FIRST_BOOT_FLAG, 0) != suspend_status)
		&& (suspend_status != BOOT_UPGRADE_FLAG)
		&& (error_gen(MOD_RESUME_COMPLETE_FLAG, 0) != suspend_status)) {
		suspend_result = -1;
		pr_err("suspend_err, rtc gpr as follow:\n");
		show_mem_status();
		/* disable console suspend */
		bak_console_suspend_enabled = console_suspend_enabled;
		console_suspend_enabled = 0;
		/* enable initcall_debug */
		backup_initcall_debug = initcall_debug;
		initcall_debug = 1;
		/* change loglevel to 8 */
		backup_console_loglevel = console_loglevel;
		console_loglevel = 8;
		/* change pm_debug_mask to 0xff */
		backup_debug_mask = pm_debug_mask;
		pm_debug_mask |= 0x07;
	} else {
		if (suspend_result == -1) {
			/* restore console suspend */
			console_suspend_enabled = bak_console_suspend_enabled;
			/* restore initcall_debug */
			initcall_debug = backup_initcall_debug;
			/* restore console_loglevel */
			console_loglevel = backup_console_loglevel;
			/* restore pm_debug_mask */
			pm_debug_mask = backup_debug_mask;
		}

		suspend_result = 0;
	}

	save_pm_secure_mem_status(error_gen(MOD_FREEZER_THREAD, 0));

	return 0;
}

static int sunxi_suspend_prepare(void)
{
	pr_debug("%s\n", __func__);

	save_pm_secure_mem_status(error_gen(MOD_SUSPEND_DEVICES,
				ERR_SUSPEND_DEVICES_SUSPEND_DEVICES_DONE));

	return 0;
}

static int sunxi_suspend_prepare_late(void)
{
#ifdef CONFIG_CPU_FREQ
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(0);
	if (!policy)
		return -1;

	cpufreq_driver_target(policy, suspend_freq, CPUFREQ_RELATION_L);

	cpufreq_cpu_put(policy);
#endif

	pr_debug("%s\n", __func__);

#if defined(CONFIG_SUNXI_SUSPEND_NONARISC) && defined(CONFIG_AW_AXP)
	if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_PWR_STATUS)) {
		pr_info("power status as follow:");
		/* notice:
		 * with no cpus, the axp will use twi0 which
		 * will use schedule interface to read axp status.
		 * considering schedule is prohibited
		 * when timekeeping is in suspended state.
		 * so call axp_regulator_dump() before
		 * calling timekeeping_suspend().
		 */
		axp_regulator_dump();
	}

	/* check pwr usage info: change sys_id according module usage info. */
	check_pwr_status();
	/* config sys_pwr_dm according to the sysconfig */
	config_sys_pwr();
	/* save axp regulator state if need  */
	axp_mem_save();
#endif

	save_pm_secure_mem_status(error_gen(MOD_SUSPEND_DEVICES,
				ERR_SUSPEND_DEVICES_LATE_SUSPEND_DEVICES_DONE));

	return 0;
}

static int __sunxi_suspend_enter(void)
{
	/* print_call_info(); */
	if (check_scene_locked(SCENE_BOOT_FAST))
		super_standby_para_info.event = mem_para_info.axp_event;
	else
		super_standby_para_info.event = CPUS_BOOTFAST_WAKEUP;

	/*
	 * the wakeup src is independent of the scene_lock
	 * the developer only need to care about:
	 * the scene support the wakeup src;
	 */
	if (extended_standby_manager_id) {
		super_standby_para_info.event |=
				extended_standby_manager_id->event;
		super_standby_para_info.gpio_enable_bitmap =
				extended_standby_manager_id->wakeup_gpio_map;
		super_standby_para_info.cpux_gpiog_bitmap =
				extended_standby_manager_id->wakeup_gpio_group;
	}

	if ((super_standby_para_info.event
		& (CPUS_WAKEUP_DESCEND | CPUS_WAKEUP_ASCEND)) == 0) {
#ifdef CONFIG_AW_AXP
		axp_powerkey_set(1);
#endif
	}

#if !defined(CONFIG_ARM_PSCI_FW) && !defined(CONFIG_SUNXI_SUSPEND_NONARISC)
	super_standby_para_info.resume_code_src =
		(unsigned int)(virt_to_phys((void *)&resume1_bin_start));
	super_standby_para_info.resume_code_length =
		(int)&resume1_bin_end - (int)&resume1_bin_start;
	super_standby_para_info.resume_entry = SRAM_FUNC_START_PA;
	super_standby_para_info.cpu_resume_entry = virt_to_phys(cpu_resume);
#endif

	if (extended_standby_manager_id
		&& extended_standby_manager_id->pextended_standby)
		super_standby_para_info.pextended_standby =
			(unsigned int)(standby_space.extended_standby_mem_base);
	else
		super_standby_para_info.pextended_standby = 0;

	super_standby_para_info.timeout = time_to_wakeup_ms;
	if (super_standby_para_info.timeout > 0)
		super_standby_para_info.event |= CPU0_WAKEUP_TIMEOUT
						| CPUS_WAKEUP_TIMEOUT;

	if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_STANDBY)) {
		pr_info("total dynamic config standby wakeup src config: 0x%x\n",
		super_standby_para_info.event);
		parse_wakeup_event(NULL, 0, super_standby_para_info.event);
	}

	if (standby_space.mem_size < (sizeof(super_standby_para_info) +
		sizeof(*(extended_standby_manager_id->pextended_standby)))) {
		/* judge the reserved space for mem para is enough or not. */
		pr_err("ERR: reserved space is not enough for mem_para.\n");
		pr_err("need size: %lx.\n",
		(unsigned long)(sizeof(super_standby_para_info) +
		sizeof(*(extended_standby_manager_id->pextended_standby))));
		pr_err("reserved size: %lx.\n",
				(unsigned long)(standby_space.mem_size));
		return -1;
	}

	/* clean all the data into dram */
	memcpy((void *)phys_to_virt(standby_space.standby_mem_base),
		(void *)&super_standby_para_info,
		sizeof(super_standby_para_info));

	if (extended_standby_manager_id
		&& extended_standby_manager_id->pextended_standby)
		memcpy((void *)(phys_to_virt(
				standby_space.extended_standby_mem_base)),
		(void *)(extended_standby_manager_id->pextended_standby),
		sizeof(*(extended_standby_manager_id->pextended_standby)));

#if defined(CONFIG_ARM)
	dmac_flush_range((void *)phys_to_virt(standby_space.standby_mem_base),
			 (void *)(phys_to_virt(standby_space.standby_mem_base +
				standby_space.mem_size)));
#elif defined(CONFIG_ARM64)
	__dma_flush_range((void *)phys_to_virt(standby_space.standby_mem_base),
			(void *)(phys_to_virt(standby_space.standby_mem_base +
				standby_space.mem_size)));
#endif

	mem_device_save();

#ifdef CONFIG_SUNXI_SUSPEND_NONARISC
	/* copy brom to dram address */
	standby_info.standby_para.event = CPU0_MEM_WAKEUP;
	standby_info.standby_para.event |= super_standby_para_info.event;
	standby_info.standby_para.gpio_enable_bitmap =
				super_standby_para_info.gpio_enable_bitmap;
	standby_info.standby_para.cpux_gpiog_bitmap =
				super_standby_para_info.cpux_gpiog_bitmap;

	if (super_standby_para_info.pextended_standby != 0)
		standby_info.standby_para.pextended_standby =
			(unsigned int)(phys_to_virt(
				super_standby_para_info.pextended_standby));

	standby_info.standby_para.timeout = super_standby_para_info.timeout;
	standby_info.standby_para.debug_mask = pm_debug_mask;

#ifdef CONFIG_AW_AXP
	standby_info.pmu_arg.axp_dev_count = axp_dev_register_count;
	axp_get_pwr_regu_tree((unsigned int *)(standby_info.
					   pmu_arg.soc_power_tree));
#endif
#endif

	save_pm_secure_mem_status(error_gen(MOD_SUSPEND_CPUXSYS,
				ERR_SUSPEND_CPUXSYS_CONFIG_SUPER_PARA_DONE));

	init_wakeup_src(standby_info.standby_para.event,
				standby_info.standby_para.gpio_enable_bitmap,
				standby_info.standby_para.cpux_gpiog_bitmap);
	save_pm_secure_mem_status(error_gen(MOD_SUSPEND_CPUXSYS,
				ERR_SUSPEND_CPUXSYS_CONFIG_WAKEUP_SRC_DONE));

#ifdef CONFIG_ARM_PSCI_FW
#if defined(CONFIG_ARM)
	arm_cpuidle_suspend(3);
#elif defined(CONFIG_ARM64)
	cpu_suspend(3);
#endif
#else
#ifdef CONFIG_SUNXI_SUSPEND_NONARISC
	cpu_resume_addr = virt_to_phys(cpu_resume);
	standby_info.resume_addr = virt_to_phys((void *)&cpu_brom_start);
	create_mapping();
	/* cpu_cluster_pm_enter(); */
	cpu_suspend((unsigned long)(&standby_info), sunxi_standby_enter);
	/* cpu_cluster_pm_enter(); */
	restore_mapping();
	/* report which wake source wakeup system */
	query_wakeup_source(&standby_info);
#else

	mem_clk_save();

	cpu_suspend((unsigned long)(&super_standby_para_info),
				sunxi_standby_enter);

	save_pm_secure_mem_status(error_gen(MOD_RESUME_CPUXSYS,
				ERR_RESUME_CPUXSYS_CONFIG_WAKEUP_SRC_DONE + 1));

	arisc_cpux_ready_notify();

	mem_clk_restore();

	/*
	 * we should init serial here,
	 * otherwise printk will block.
	 */
	if (!console_suspend_enabled)
		serial_init();

#endif
#endif /* CONFIG_ARM_PSCI */

	exit_wakeup_src(standby_info.standby_para.event,
				standby_info.standby_para.gpio_enable_bitmap,
				standby_info.standby_para.cpux_gpiog_bitmap);

	mem_device_restore();

	save_pm_secure_mem_status(error_gen(MOD_RESUME_CPUXSYS,
				ERR_RESUME_CPUXSYS_CONFIG_WAKEUP_SRC_DONE));

	return 0;
}

static int sunxi_suspend_enter(suspend_state_t state)
{
	pr_info("enter state %d\n", state);

	save_pm_secure_mem_status(error_gen(MOD_SUSPEND_CORE, 0));

	if (unlikely(console_suspend_enabled == 0))
		pm_debug_mask |= (PM_STANDBY_PRINT_RESUME
				| PM_STANDBY_PRINT_STANDBY);
	else
		pm_debug_mask &= ~(PM_STANDBY_PRINT_RESUME
				| PM_STANDBY_PRINT_STANDBY);

	/* show device: cpux_io, cpus_io, ccu status */
	show_dev_status();

	extended_standby_manager_id = get_extended_standby_manager();

	extended_standby_show_state();

	save_pm_secure_mem_status(error_gen(MOD_SUSPEND_CPUXSYS,
				ERR_SUSPEND_CPUXSYS_SHOW_DEVICES_STATE_DONE));

	mem_para_info.debug_mask = pm_debug_mask;
	mem_para_info.suspend_delay_ms = suspend_delay_ms;

	/* config cpus wakeup event type */
#ifdef CONFIG_SUNXI_SUSPEND_NONARISC
	mem_para_info.axp_event = 0;
#else
	if (state == PM_SUSPEND_MEM || state == PM_SUSPEND_STANDBY)
		mem_para_info.axp_event = CPUS_MEM_WAKEUP;
	else if (state == PM_SUSPEND_BOOTFAST)
		mem_para_info.axp_event = CPUS_BOOTFAST_WAKEUP;
#endif

	save_pm_secure_mem_status(error_gen(MOD_SUSPEND_CPUXSYS,
				ERR_SUSPEND_CPUXSYS_CONFIG_MEM_PARA_DONE));

	if (__sunxi_suspend_enter()) {
		pr_err("sunxi suspend failed, need to resume");
		return -1;
	}

	save_pm_secure_mem_status(error_gen(MOD_RESUME_CPUXSYS,
				ERR_RESUME_CPUXSYS_RESUME_DEVICES_DONE));

#ifdef CONFIG_SUNXI_SUSPEND_NONARISC
	mem_para_info.axp_event = standby_info.standby_para.event;
	pr_info("platform wakeup, standby wakesource is:0x%x\n",
				mem_para_info.axp_event);
	parse_wakeup_event(NULL, 0, mem_para_info.axp_event);
#else
	arisc_query_wakeup_source(&mem_para_info.axp_event);
	pr_info("platform wakeup, standby wakesource is:0x%x\n",
				mem_para_info.axp_event);
	parse_wakeup_event(NULL, 0, mem_para_info.axp_event);
#endif

	if (mem_para_info.axp_event & (CPUS_WAKEUP_LONG_KEY)) {
#ifdef CONFIG_AW_AXP
		axp_powerkey_set(0);
#endif
	}

	/* show device: cpux_io, cpus_io, ccu status */
	show_dev_status();

	save_pm_secure_mem_status(error_gen(MOD_RESUME_CPUXSYS,
				ERR_RESUME_CPUXSYS_RESUME_CPUXSYS_DONE));

	return 0;
}

static void sunxi_suspend_wake(void)
{
	pr_debug("%s\n", __func__);

#if defined(CONFIG_SUNXI_SUSPEND_NONARISC) && defined(CONFIG_AW_AXP)
	/* restore axp regulator state if need */
	axp_mem_restore();
	resume_sys_pwr_state();

	if (unlikely(pm_debug_mask & PM_STANDBY_PRINT_PWR_STATUS)) {
		pr_info("power status as follow:");
		/*
		 * notice:
		 * with no cpus, the axp will use twi0 which
		 * will use schedule interface to read axp status.
		 * considering schedule is prohibited
		 * when timekeeping is in suspended state.
		 * so call axp_regulator_dump() before
		 * calling timekeeping_suspend().
		 */
		axp_regulator_dump();
	}
#endif

	save_pm_secure_mem_status(error_gen(MOD_RESUME_PROCESSORS, 0));
}

static void sunxi_suspend_finish(void)
{
#ifdef CONFIG_CPU_FREQ
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(0);
	if (!policy)
		return;

	cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_L);

	cpufreq_cpu_put(policy);
#endif

	save_pm_secure_mem_status(error_gen(MOD_RESUME_DEVICES,
				ERR_RESUME_DEVICES_EARLY_RESUME_DEVICES_DONE));
}

static void sunxi_suspend_end(void)
{
	pr_debug("%s\n", __func__);

	save_pm_secure_mem_status(error_gen(MOD_RESUME_COMPLETE_FLAG, 0));
}

static void sunxi_suspend_recover(void)
{
	pr_debug("%s\n", __func__);

	save_pm_secure_mem_status(error_gen(MOD_SUSPEND_DEVICES,
				ERR_SUSPEND_DEVICES_SUSPEND_DEVICES_FAILED));
}

static const struct platform_suspend_ops sunxi_suspend_ops = {
	.valid        = sunxi_suspend_valid,
	.begin        = sunxi_suspend_begin,
	.prepare      = sunxi_suspend_prepare,
	.prepare_late = sunxi_suspend_prepare_late,
	.enter        = sunxi_suspend_enter,
	.wake         = sunxi_suspend_wake,
	.finish       = sunxi_suspend_finish,
	.end          = sunxi_suspend_end,
	.recover      = sunxi_suspend_recover,
};

static int __init sunxi_suspend_init(void)
{
	int ret = 0;
	u32 value[3] = { 0, 0, 0 };

	pr_debug("%s\n", __func__);

	/* init debug state */
	pm_secure_mem_status_init("rtc");

#ifdef CONFIG_AW_AXP
	config_pmu_para();
	init_sys_pwr_dm();
	config_dynamic_standby();
#endif

	standby_space.np = of_find_compatible_node(NULL, NULL,
				"allwinner,standby_space");
	if (IS_ERR(standby_space.np)) {
		pr_err("get [allwinner,standby_space] device node error\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(standby_space.np, "space1", value,
				ARRAY_SIZE(value));
	if (ret) {
		pr_err("get standby_space1 err.\n");
		return -EINVAL;
	}

	standby_space.standby_mem_base = (phys_addr_t)value[0];
	standby_space.extended_standby_mem_base =
				(phys_addr_t)(value[0] + 0x400);
	standby_space.mem_offset = (phys_addr_t) value[1];
	standby_space.mem_size = (size_t) value[2];

#ifdef CONFIG_SUNXI_SUSPEND_NONARISC
	ret = fetch_and_save_dram_para(&(standby_info.dram_para));
	if (ret) {
		pr_err("get standby dram para err.\n");
		return -EINVAL;
	}
#endif

	mem_device_init();

	suspend_set_ops(&sunxi_suspend_ops);

	/* lock super standby defaultly */
	scene_lock_state("super_standby", sizeof("super_standby"));

	return ret;
}
module_init(sunxi_suspend_init);

static void __exit sunxi_suspend_exit(void)
{
	pr_debug("%s\n", __func__);
	suspend_set_ops(NULL);
}
module_exit(sunxi_suspend_exit);
