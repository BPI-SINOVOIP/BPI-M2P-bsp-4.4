/*
 * drivers/soc/sunxi/pm/sysfs/pm_sysfs.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * pm sysfs for sunxi suspend
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/power/scenelock.h>
#include <linux/device.h>
#include "../debug/pm_debug.h"
#include "../pm.h"

static struct kobject *sunxi_suspend_kobj;

#ifdef CONFIG_CPU_FREQ
ssize_t suspend_freq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", suspend_freq);
}

static ssize_t suspend_freq_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int val, err;

	err = kstrtoint(buf, 10, &val);
	if (err)
		return err;

	suspend_freq = val;

	return size;
}
#endif

ssize_t suspend_delay_ms_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", suspend_delay_ms);
}

static ssize_t suspend_delay_ms_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int val, err;

	err = kstrtoint(buf, 10, &val);
	if (err)
		return err;

	suspend_delay_ms = val;

	return size;
}

ssize_t time_to_wakeup_ms_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", time_to_wakeup_ms);
}

static ssize_t time_to_wakeup_ms_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int val, err;

	err = kstrtoint(buf, 10, &val);
	if (err)
		return err;

	time_to_wakeup_ms = val;

	return size;
}

ssize_t extended_standby_parse_bitmap_en_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", parse_bitmap_en);
}

static ssize_t extended_standby_parse_bitmap_en_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int val, err;

	err = kstrtoint(buf, 16, &val);
	if (err)
		return err;

	parse_bitmap_en = val;

	return size;
}

ssize_t extended_standby_dram_crc_paras_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;

	s += sprintf(buf, "dram_crc_paras: enable,start,len == [%x][%x][%x]\n",
		temp_standby_data.soc_dram_state.crc_en,
		temp_standby_data.soc_dram_state.crc_start,
		temp_standby_data.soc_dram_state.crc_len);

	return s - buf;
}

static ssize_t extended_standby_dram_crc_paras_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned int dram_crc_en;
	unsigned int dram_crc_start;
	unsigned int dram_crc_len;
	int num;

	num = sscanf(buf, "%x %x %x\n", &dram_crc_en,
				&dram_crc_start, &dram_crc_len);
	if (num <= -1) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return size;
	}

	if ((dram_crc_en != 0) && (dram_crc_en != 1)) {
		pr_err("invalid paras for dram_crc: [%x] [%x] [%x]\n",
				dram_crc_en, dram_crc_start, dram_crc_len);
		return size;
	}

	temp_standby_data.soc_dram_state.crc_en = dram_crc_en;
	temp_standby_data.soc_dram_state.crc_start = dram_crc_start;
	temp_standby_data.soc_dram_state.crc_len = dram_crc_len;

	return size;
}

static char *parse_debug_mask(unsigned int bitmap, char *s, char *end)
{
	int i = 0, counted = 0, count = 0;
	unsigned int bit_event = 0;

	for (i = 0; i < 32; i++) {
		bit_event = (1 << i & bitmap);
		switch (bit_event) {
		case 0:
			break;
		case PM_STANDBY_PRINT_STANDBY:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_STANDBY         ",
					PM_STANDBY_PRINT_STANDBY);
			count++;
			break;
		case PM_STANDBY_PRINT_RESUME:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_RESUME          ",
					PM_STANDBY_PRINT_RESUME);
			count++;
			break;
		case PM_STANDBY_ENABLE_JTAG:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_ENABLE_JTAG           ",
					PM_STANDBY_ENABLE_JTAG);
			count++;
			break;
		case PM_STANDBY_PRINT_PORT:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_PORT            ",
					PM_STANDBY_PRINT_PORT);
			count++;
			break;
		case PM_STANDBY_PRINT_IO_STATUS:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_IO_STATUS       ",
					PM_STANDBY_PRINT_IO_STATUS);
			count++;
			break;
		case PM_STANDBY_PRINT_CACHE_TLB_MISS:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_CACHE_TLB_MISS  ",
					PM_STANDBY_PRINT_CACHE_TLB_MISS);
			count++;
			break;
		case PM_STANDBY_PRINT_CCU_STATUS:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_CCU_STATUS      ",
					PM_STANDBY_PRINT_CCU_STATUS);
			count++;
			break;
		case PM_STANDBY_PRINT_PWR_STATUS:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_PWR_STATUS      ",
					PM_STANDBY_PRINT_PWR_STATUS);
			count++;
			break;
		case PM_STANDBY_PRINT_CPUS_IO_STATUS:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_CPUS_IO_STATUS  ",
					PM_STANDBY_PRINT_CPUS_IO_STATUS);
			count++;
			break;
		case PM_STANDBY_PRINT_CCI400_REG:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_CCI400_REG      ",
					PM_STANDBY_PRINT_CCI400_REG);
			count++;
			break;
		case PM_STANDBY_PRINT_GTBUS_REG:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_GTBUS_REG       ",
					PM_STANDBY_PRINT_GTBUS_REG);
			count++;
			break;
		case PM_STANDBY_TEST:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_TEST                  ",
					PM_STANDBY_TEST);
			count++;
			break;
		case PM_STANDBY_PRINT_RESUME_IO_STATUS:
			s += scnprintf(s, end - s, "%-34s bit 0x%x\t",
					"PM_STANDBY_PRINT_RESUME_IO_STATUS",
					PM_STANDBY_PRINT_RESUME_IO_STATUS);
			count++;
			break;
		default:
			break;
		}

		if (counted != count && 0 == count % 2) {
			counted = count;
			s += scnprintf(s, end - s, "\n");
		}
	}

	s += scnprintf(s, end - s, "\n");

	return s;
}

ssize_t debug_mask_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = buf + PAGE_SIZE;

	s += sprintf(buf, "0x%x\n", pm_debug_mask);
	s = parse_debug_mask(pm_debug_mask, s, end);

	s += sprintf(s, "%s\n", "pm_debug_mask usage help info:");
	s += sprintf(s, "%s\n",
		"target: for enable checking the io, ccu... suspended status.");
	s += sprintf(s, "%s\n",
		"bitmap: each bit corresponding one module, as follow:");
	s = parse_debug_mask(0xffff, s, end);

	return s - buf;
}

ssize_t debug_mask_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int val, err;

	err = kstrtoint(buf, 16, &val);
	if (err)
		return err;

	pm_debug_mask  = val;

	return count;
}

ssize_t scenelock_debug_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;

	s += sprintf(buf, "0x%x\n", scenelock_debug_mask);

	return s - buf;
}

ssize_t scenelock_debug_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int val, err;

	err = kstrtoint(buf, 16, &val);
	if (err)
		return err;

	scenelock_debug_mask = val;

	return count;
}

ssize_t parse_status_code_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned int status_code = 0, index = 0;
	int num;

	num = sscanf(buf, "%x %x\n", &status_code, &index);
	if (num <= -1) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return size;
	}

	parse_status_code(status_code, index);

	return size;
}

ssize_t parse_status_code_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;

	show_mem_status();

	return s - buf;
}

static ssize_t scene_lock_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = buf + PAGE_SIZE;

	s = show_scene_state(s, end);

	return (s - buf);
}

static ssize_t scene_lock_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return scene_lock_state(buf, count);
}

static ssize_t scene_unlock_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return scene_unlock_state(buf, count);
}

static ssize_t scene_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = buf + PAGE_SIZE;

	s = show_scene_state(s, end);

	return (s - buf);
}

static ssize_t wakeup_src_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	enum CPU_WAKEUP_SRC src;
	__u32 para = 0, enable = 0;
	int num;

	num = sscanf(buf, "%x %x %x\n", (__u32 *)&src, (__u32 *)&para,
				(__u32 *)&enable);
	if (num <= -1) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return count;
	}

	if (enable)
		enable_wakeup_src(src, para);
	else
		disable_wakeup_src(src, para);

	return count;
}

static ssize_t wakeup_src_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = buf + PAGE_SIZE;
	const struct  extended_standby_manager_t *manager = NULL;
	char *help = "========wakeup src setting usage help info========";
	char *usage = "echo wakeup_src_e para (1:enable)/(0:disable) > /sys/power/sunxi/wakeup_src";
	char *demo  = "demo: echo 0x2000 0x200 1 > /sys/power/sunxi/wakeup_src";

	manager = get_extended_standby_manager();

	if (manager) {
		s += scnprintf(s, end - s, "%s\n", "dynamic wakeup_src config");
		s += scnprintf(s, end - s, "wakeup_src 0x%lx\n",
						manager->event);
		s += parse_wakeup_event(s, end - s, manager->event);
		s += scnprintf(s, end - s, "wakeup_gpio_map 0x%lx\n",
						manager->wakeup_gpio_map);
		s += parse_wakeup_gpio_map(s, end - s,
				manager->wakeup_gpio_map);
		s += scnprintf(s, end - s, "wakeup_gpio_group 0x%lx\n",
						manager->wakeup_gpio_group);
		s += parse_wakeup_gpio_group_map(s, end - s,
						manager->wakeup_gpio_group);

		if (manager->pextended_standby)
			s += scnprintf(s, end - s, "extended_standby id=0x%x\n",
						manager->pextended_standby->id);
	}

	s += scnprintf(s, end - s, "%s\n", help);
	s += scnprintf(s, end - s, "%s\n", usage);
	s += scnprintf(s, end - s, "%s\n", demo);
	s += scnprintf(s, end - s, "%s\n", "wakeup_src_e para info: ");
	s += parse_wakeup_event(s, end - s, 0xffffffff);
	s += scnprintf(s, end - s, "%s\n", "gpio para info: ");
	s += show_gpio_config(s, end - s);

	return (s - buf);
}

#ifdef CONFIG_AW_AXP
static ssize_t sys_pwr_dm_mask_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = (char *)((ptrdiff_t)buf + (ptrdiff_t)PAGE_SIZE);
	__u32 dm = 0;
	char *help = "======sys pwr_dm mask setting usage help info======";
	char *usage = "echo pwr_dm (1:enable)/(0:disable) > /sys/power/sunxi/sys_pwr_dm_mask";
	char *demo  = "demo: for add cpub to sys_pwr_dm: echo 0x2 1 > /sys/power/sunxi/sys_pwr_dm_mask";

	dm = axp_get_sys_pwr_dm_mask();

	s += scnprintf(s, end - s, "0x%x\n", dm);
	s += parse_pwr_dm_map(s, end - s, dm);
	s += scnprintf(s, end - s, "%s\n", help);
	s += scnprintf(s, end - s, "%s\n", usage);
	s += scnprintf(s, end - s, "%s\n", demo);
	s += scnprintf(s, end - s, "%s\n", "sys pwr dm info: ");
	s += parse_pwr_dm_map(s, end - s, 0xffffffff);

	return (s - buf);
}

static ssize_t sys_pwr_dm_mask_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int i = 0;
	__u32 dm = 0, enable = 0;
	int num;

	num = sscanf(buf, "%x %x\n", (__u32 *)&dm, (__u32 *)&enable);
	if (num <= -1) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return count;
	}

	for (i = 0; i < sizeof(dm) * 8; i++) {
		if (dm & (0x1 << i))
			axp_set_sys_pwr_dm_mask(i, enable);
	}

	return count;
}
#endif

#ifdef CONFIG_CPU_FREQ
static DEVICE_ATTR(suspend_freq, S_IRUGO | S_IWUSR, suspend_freq_show,
				suspend_freq_store);
#endif

static DEVICE_ATTR(suspend_delay_ms, S_IRUGO | S_IWUSR, suspend_delay_ms_show,
				suspend_delay_ms_store);

static DEVICE_ATTR(time_to_wakeup_ms, S_IRUGO | S_IWUSR, time_to_wakeup_ms_show,
				time_to_wakeup_ms_store);

static DEVICE_ATTR(parse_bitmap_en, S_IRUGO | S_IWUSR,
				extended_standby_parse_bitmap_en_show,
				extended_standby_parse_bitmap_en_store);

static DEVICE_ATTR(dram_crc_paras, S_IRUGO | S_IWUSR,
				extended_standby_dram_crc_paras_show,
				extended_standby_dram_crc_paras_store);

static DEVICE_ATTR(pm_debug_mask, S_IRUGO | S_IWUSR, debug_mask_show,
				debug_mask_store);
static DEVICE_ATTR(scenelock_debug, S_IRUGO | S_IWUSR, scenelock_debug_show,
				scenelock_debug_store);
static DEVICE_ATTR(parse_status_code, S_IRUGO | S_IWUSR, parse_status_code_show,
				parse_status_code_store);

static DEVICE_ATTR(scene_lock,      S_IRUGO | S_IWUSR, scene_lock_show,
				scene_lock_store);
static DEVICE_ATTR(scene_unlock,         S_IWUSR, NULL,
				scene_unlock_store);
static DEVICE_ATTR(scene_state,               S_IRUGO, scene_state_show,
				NULL);
static DEVICE_ATTR(wakeup_src,      S_IRUGO | S_IWUSR, wakeup_src_show,
				wakeup_src_store);
#ifdef CONFIG_AW_AXP
static DEVICE_ATTR(sys_pwr_dm_mask, S_IRUGO | S_IWUSR, sys_pwr_dm_mask_show,
				sys_pwr_dm_mask_store);
#endif

static struct attribute *suspend_attrs[] = {
#ifdef CONFIG_CPU_FREQ
	&dev_attr_suspend_freq.attr,
#endif
	&dev_attr_suspend_delay_ms.attr,
	&dev_attr_time_to_wakeup_ms.attr,
	&dev_attr_parse_bitmap_en.attr,
	&dev_attr_pm_debug_mask.attr,
	&dev_attr_scenelock_debug.attr,
	&dev_attr_parse_status_code.attr,
	&dev_attr_dram_crc_paras.attr,
	&dev_attr_scene_lock.attr,
	&dev_attr_scene_unlock.attr,
	&dev_attr_scene_state.attr,
	&dev_attr_wakeup_src.attr,
#ifdef CONFIG_AW_AXP
	&dev_attr_sys_pwr_dm_mask.attr,
#endif
	NULL,
};

static struct attribute_group suspend_attr_group = {
	.attrs = suspend_attrs,
};

static s32 __init suspend_sysfs_init(void)
{
	sunxi_suspend_kobj = kobject_create_and_add("sunxi", power_kobj);
	if (!sunxi_suspend_kobj)
		return -ENOMEM;

	return sysfs_create_group(sunxi_suspend_kobj, &suspend_attr_group);
}
arch_initcall(suspend_sysfs_init);

