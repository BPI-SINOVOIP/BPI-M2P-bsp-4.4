/*
 * drivers/cpufreq/sunxi-cpufreq.c
 *
 * Copyright (c) 2014 softwinner.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_opp.h>
#include <linux/arisc/arisc.h>
#include <linux/sunxi-sid.h>
#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/delay.h>

#ifdef CONFIG_SUNXI_ARISC
#include <linux/arisc/arisc.h>
#endif

#define CPUFREQ_DBG(format, args...)	\
	pr_debug("[cpu_freq] DBG: "format, ##args)
#define CPUFREQ_ERR(format, args...)	\
	pr_err("[cpu_freq] ERR: "format, ##args)

#ifdef CONFIG_DEBUG_FS
/* sunxi CPUFreq driver data structure */
static struct {
	s64 cpufreq_set_us;
	s64 cpufreq_get_us;
} sunxi_cpufreq;
#endif

/* cpufreq_dvfs_table is global default dvfs table */
static struct cpufreq_dvfs_table cpufreq_dvfs_table[DVFS_VF_TABLE_MAX] = {
	/*
	 * cluster0
	 * cpu0 vdd is 1.20v if cpu freq is (600Mhz, 1008Mhz]
	 * cpu0 vdd is 1.20v if cpu freq is (420Mhz, 600Mhz]
	 * cpu0 vdd is 1.20v if cpu freq is (360Mhz, 420Mhz]
	 * cpu0 vdd is 1.20v if cpu freq is (300Mhz, 360Mhz]
	 * cpu0 vdd is 1.20v if cpu freq is (240Mhz, 300Mhz]
	 * cpu0 vdd is 1.20v if cpu freq is (120Mhz, 240Mhz]
	 * cpu0 vdd is 1.20v if cpu freq is (60Mhz,  120Mhz]
	 * cpu0 vdd is 1.20v if cpu freq is (0Mhz,   60Mhz]
	 */
	/* freq         voltage     axi_div */
	{900000000,     1200,       3},
	{600000000,     1200,       3},
	{420000000,     1200,       3},
	{360000000,     1200,       3},
	{300000000,     1200,       3},
	{240000000,     1200,       3},
	{120000000,     1200,       3},
	{60000000,      1200,       3},
	{0,             1200,       3},
	{0,             1200,       3},
	{0,             1200,       3},
	{0,             1200,       3},
	{0,             1200,       3},
	{0,             1200,       3},
	{0,             1200,       3},
	{0,             1200,       3}
};

static unsigned int sunxi_cpufreq_get(unsigned int cpu)
{
	unsigned int current_freq = 0;
#ifdef CONFIG_DEBUG_FS
	ktime_t calltime = ktime_get();
#endif

	current_freq = cpufreq_generic_get(cpu);

#ifdef CONFIG_DEBUG_FS
	sunxi_cpufreq.cpufreq_get_us =
				ktime_to_us(ktime_sub(ktime_get(), calltime));
#endif

	return current_freq;
}

#ifdef CONFIG_SUNXI_ARISC
static int sunxi_set_cpufreq_and_voltage(struct cpufreq_policy *policy,
					unsigned long freq)
{
#ifdef CONFIG_SUNXI_CPUFREQ_ASYN
	unsigned long timeout;
	/* use asynchronous working */
	arisc_dvfs_set_cpufreq(freq, ARISC_DVFS_PLL1, ARISC_DVFS_ASYN,
					NULL, NULL);
	/* CPUS max latency for cpu freq*/
	timeout = 15;
	while (timeout-- && (clk_get_rate(policy->clk) != freq*1000))
		msleep(1);
	if (clk_get_rate(policy->clk) != freq*1000)
		return -1;
	return 0;
#else
	return arisc_dvfs_set_cpufreq(freq, ARISC_DVFS_PLL1, ARISC_DVFS_SYN,
					NULL, NULL);
#endif
}
#else
static int sunxi_set_cpufreq_and_voltage(struct cpufreq_policy *policy,
					unsigned long freq)
{
	struct device *cpu_dev;

	cpu_dev = get_cpu_device(policy->cpu);


	return dev_pm_opp_set_rate(cpu_dev, freq);
}
#endif

static int sunxi_cpufreq_target_index(struct cpufreq_policy *policy,
					unsigned int index)
{
	int ret = 0;
	unsigned long freq;
#ifdef CONFIG_DEBUG_FS
	ktime_t calltime;
#endif

	freq = policy->freq_table[index].frequency;

#ifdef CONFIG_DEBUG_FS
	calltime = ktime_get();
#endif
	/* try to set cpu frequency */
	ret = sunxi_set_cpufreq_and_voltage(policy, freq);
	if (ret)
		CPUFREQ_ERR("Set cpu frequency to %luKHz failed!\n", freq);

#ifdef CONFIG_DEBUG_FS
	sunxi_cpufreq.cpufreq_set_us = ktime_to_us(ktime_sub(ktime_get(), calltime));
#endif

	return ret;
}
static int sunxi_cpufreq_set_vf(struct cpufreq_frequency_table *table,
				unsigned int cpu)
{
	struct cpufreq_frequency_table *pos;
	struct dev_pm_opp *opp;
	struct device *dev;
	unsigned long freq;
	int ret, num = 0;
	int max_opp_num = 0;
#if defined(CONFIG_ARCH_SUN50IW1P1) || defined(CONFIG_ARCH_SUN50IW3P1)
	void *kvir = NULL;
#endif

	dev = get_cpu_device(cpu);
	max_opp_num = dev_pm_opp_get_opp_count(dev);

#if defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN50IW1P1) || \
	defined(CONFIG_ARCH_SUN50IW3P1)
	for (pos = table; max_opp_num > 0; --max_opp_num) {
		freq = pos[max_opp_num - 1].frequency * 1000;
#else
	cpufreq_for_each_valid_entry(pos, table) {
		freq = pos->frequency * 1000;
#endif
		CPUFREQ_DBG("freq: %lu\n", freq);
		rcu_read_lock();
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR(opp)) {
			ret = PTR_ERR(opp);
			dev_err(dev,
				"%s: failed to find OPP for freq %lu (%d)\n",
				__func__, freq, ret);
			rcu_read_unlock();
			return ret;
		}
		rcu_read_unlock();

		cpufreq_dvfs_table[num].voltage =
			dev_pm_opp_get_voltage(opp) / 1000;
		cpufreq_dvfs_table[num].freq = dev_pm_opp_get_freq(opp);
		cpufreq_dvfs_table[num].axi_div =
				dev_pm_opp_axi_bus_divide_ratio(opp);
		CPUFREQ_DBG("num:%d, volatge:%d, freq:%d, axi_div:%d ,%s\n",
				num, cpufreq_dvfs_table[num].voltage,
				cpufreq_dvfs_table[num].freq,
			cpufreq_dvfs_table[num].axi_div, __func__);
		num++;
	}

#if defined(CONFIG_ARCH_SUN50IW1P1) || defined(CONFIG_ARCH_SUN50IW3P1)
	/*
	 * for SUN50IW1P1 the code next has do nothing, because the atf and
	 * cpus have no code to deal with ARM_SVC_ARISC_CPUX_DVFS_CFG_VF_REQ;
	 * the space of cpus has to small to put down the processing code.
	 */
	kvir =
	kmalloc(num * sizeof(struct cpufreq_frequency_table), GFP_KERNEL);
	if (kvir == NULL) {
		CPUFREQ_ERR("kmalloc error for transmiting vf table\n");
		return -1;
	}
	memcpy((void *)kvir, (void *)cpufreq_dvfs_table,
			num * sizeof(struct cpufreq_frequency_table));

	__dma_flush_range((void *)kvir,
		(void *)((struct cpufreq_frequency_table *)kvir + num - 1));

	arisc_dvfs_cfg_vf_table(0, num, (void *)virt_to_phys(kvir));

	kfree(kvir);

	kvir = NULL;
#else
	arisc_dvfs_cfg_vf_table(0, num, cpufreq_dvfs_table);
	if (ret != 0) {
		CPUFREQ_ERR("arisc_dvfs_cfg_vf_table has wrong %d\n", ret);
		return ret;
	}
#endif
	return 0;
}

static int sunxi_cpufreq_init(struct cpufreq_policy *policy)
{
	struct device *cpu_dev;
	struct cpufreq_frequency_table *freq_table;
	struct dev_pm_opp *suspend_opp;
	struct clk *cpu_clk;
#if !defined(CONFIG_ARCH_SUN8IW7P1) && !defined(CONFIG_ARCH_SUN50IW1P1) && \
	!defined(CONFIG_ARCH_SUN50IW3P1)
	const char *regulator_name;
#endif
	unsigned int transition_latency;
	int ret, soc_bin;
	unsigned int table_count;
	struct device_node *dvfs_main_np;

	dvfs_main_np = of_find_node_by_path("/opp_dvfs_table");
	if (!dvfs_main_np) {
		CPUFREQ_ERR("No opp dvfs table node found\n");
		return -ENODEV;
	}

	if (of_property_read_u32(dvfs_main_np, "opp_table_count",
	&table_count)) {
		CPUFREQ_ERR("get vf_table_count failed\n");
		return -EINVAL;
	}

	if (table_count == 1) {
		pr_info("%s: only one opp_table\n", __func__);
		soc_bin = 0;
	} else {
		soc_bin = sunxi_get_soc_bin();
		pr_info("%s: support more opp_table and soc bin is %d\n",
							__func__, soc_bin);
	}

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		CPUFREQ_ERR("Failed to get cpu%d device\n", policy->cpu);
		return -ENODEV;
	}

	cpu_clk = clk_get(cpu_dev, NULL);
	if (IS_ERR_OR_NULL(cpu_clk)) {
		ret = PTR_ERR(cpu_clk);
		CPUFREQ_ERR("Unable to get PLL CPU clock\n");
		return ret;
	}
	policy->clk = cpu_clk;
#if !defined(CONFIG_ARCH_SUN8IW7P1) && !defined(CONFIG_ARCH_SUN50IW1P1) && \
	!defined(CONFIG_ARCH_SUN50IW3P1)
	regulator_name = of_get_property(cpu_dev->of_node, "regulators", NULL);
	if (!regulator_name) {
		CPUFREQ_ERR("Unable to get regulator\n");
		ret = PTR_ERR(regulator_name);
		return ret;
	}

	ret = dev_pm_opp_set_regulator(cpu_dev, regulator_name);
	if (ret) {
		CPUFREQ_ERR("Failed to set regulator for cpu\n");
		goto out_err_clk_pll;
	}
#endif
	/* set policy->cpus according to operating-points-v2 */
	ret = dev_pm_opp_of_get_sharing_cpus_by_soc_bin(cpu_dev,
						policy->cpus, soc_bin);
	if (ret) {
		CPUFREQ_ERR("OPP-v2 opp-shared Error\n");
		goto out_err_clk_pll;
	}

	ret = dev_pm_opp_of_cpumask_add_table_by_soc_bin(policy->cpus,
								soc_bin);
	if (ret) {
		CPUFREQ_ERR("Failed to add opp table\n");
		goto out_err_clk_pll;
	}

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		CPUFREQ_ERR("Failed to init cpufreq table: %d\n", ret);
		goto out_err_free_opp;
	}

	ret = cpufreq_table_validate_and_show(policy, freq_table);
	if (ret) {
		CPUFREQ_ERR("Invalid frequency table: %d\n", ret);
		goto out_err_free_opp;
	}

	transition_latency = dev_pm_opp_get_max_transition_latency(cpu_dev);
	if (!transition_latency)
		transition_latency = CPUFREQ_ETERNAL;
	policy->cpuinfo.transition_latency = transition_latency;

	rcu_read_lock();
	suspend_opp = dev_pm_opp_get_suspend_opp(cpu_dev);
	if (suspend_opp)
		policy->suspend_freq = dev_pm_opp_get_freq(suspend_opp) / 1000;
	rcu_read_unlock();

	ret = sunxi_cpufreq_set_vf(freq_table, policy->cpu);
	if (ret) {
		CPUFREQ_ERR("sunxi_cpufreq_set_vf failed: %d\n", ret);
		goto out_err_free_opp;
	}

	return 0;

out_err_free_opp:
	dev_pm_opp_of_cpumask_remove_table(policy->cpus);
out_err_clk_pll:
	clk_put(cpu_clk);

	return ret;
}

static int sunxi_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct device *cpu_dev;

	cpu_dev = get_cpu_device(policy->cpu);

	dev_pm_opp_free_cpufreq_table(cpu_dev, &policy->freq_table);

	dev_pm_opp_of_cpumask_remove_table(policy->related_cpus);

	dev_pm_opp_put_regulator(cpu_dev);

	clk_put(policy->clk);

	return 0;
}

static struct freq_attr *sunxi_cpufreq_attr[] = {
	 &cpufreq_freq_attr_scaling_available_freqs,
	 NULL,
};

static struct cpufreq_driver sunxi_cpufreq_driver = {
	.name         = "cpufreq-sunxi",
	.flags        = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.attr         = sunxi_cpufreq_attr,
	.init         = sunxi_cpufreq_init,
	.get          = sunxi_cpufreq_get,
	.target_index = sunxi_cpufreq_target_index,
	.exit         = sunxi_cpufreq_exit,
	.verify       = cpufreq_generic_frequency_table_verify,
	.suspend      = cpufreq_generic_suspend,
};

#ifdef CONFIG_ARCH_SUN8IW7P1
static int set_pll_cpu_lock_time(void)
{
	unsigned int value;
	void __iomem *lock_time_vbase = NULL;
#define PLL_CPU_LOCK_TIME_REG	(0x01c20000 + 0x204)

	lock_time_vbase = ioremap(PLL_CPU_LOCK_TIME_REG, 4);
	if (lock_time_vbase == NULL) {
		pr_err("ioremap pll cpu lock time error\n");
		return -1;
	}

	value = readl(lock_time_vbase);
	value &= ~(0xffff);
	value |= 0x400;
	writel(value, lock_time_vbase);

	iounmap(lock_time_vbase);
	return 0;
}
#endif

static int __init sunxi_cpufreq_initcall(void)
{
	int ret;

#ifdef CONFIG_DEBUG_FS
	sunxi_cpufreq.cpufreq_set_us = 0;
	sunxi_cpufreq.cpufreq_get_us = 0;
#endif

#ifdef CONFIG_ARCH_SUN8IW7P1
	if (set_pll_cpu_lock_time())
		return -1;
#endif
	ret = cpufreq_register_driver(&sunxi_cpufreq_driver);
	if (ret)
		CPUFREQ_ERR("Failed register driver\n");

	return ret;
}

static void __exit sunxi_cpufreq_exitcall(void)
{
	cpufreq_unregister_driver(&sunxi_cpufreq_driver);
}

module_init(sunxi_cpufreq_initcall);
module_exit(sunxi_cpufreq_exitcall);

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

static struct dentry *debugfs_cpufreq_root;

static int cpufreq_debugfs_gettime_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%lld\n", sunxi_cpufreq.cpufreq_get_us);
	return 0;
}

static int cpufreq_debugfs_gettime_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpufreq_debugfs_gettime_show, inode->i_private);
}

static const struct file_operations cpufreq_debugfs_gettime_fops = {
	.open = cpufreq_debugfs_gettime_open,
	.read = seq_read,
};

static int cpufreq_debugfs_settime_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%lld\n", sunxi_cpufreq.cpufreq_set_us);
	return 0;
}

static int cpufreq_debugfs_settime_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpufreq_debugfs_settime_show, inode->i_private);
}

static const struct file_operations cpufreq_debugfs_settime_fops = {
	.open = cpufreq_debugfs_settime_open,
	.read = seq_read,
};

static int __init cpufreq_debugfs_init(void)
{
	int err = 0;

	debugfs_cpufreq_root = debugfs_create_dir("cpufreq", 0);
	if (!debugfs_cpufreq_root)
		return -ENOMEM;

	if (!debugfs_create_file("get_time", 0444, debugfs_cpufreq_root, NULL,
				&cpufreq_debugfs_gettime_fops)) {
		err = -ENOMEM;
		goto out;
	}

	if (!debugfs_create_file("set_time", 0444, debugfs_cpufreq_root, NULL,
				&cpufreq_debugfs_settime_fops)) {
		err = -ENOMEM;
		goto out;
	}

	return 0;

out:
	debugfs_remove_recursive(debugfs_cpufreq_root);
	return err;
}

static void __exit cpufreq_debugfs_exit(void)
{
	debugfs_remove_recursive(debugfs_cpufreq_root);
}

late_initcall(cpufreq_debugfs_init);
module_exit(cpufreq_debugfs_exit);

#endif /* CONFIG_DEBUG_FS */

MODULE_DESCRIPTION("cpufreq driver for sunxi SOCs");
MODULE_LICENSE("GPL");
