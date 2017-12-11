/*
 *  drivers/arisc/arisc.c
 *
 * Copyright (c) 2012 Allwinner.
 * 2012-05-01 Written by sunny (sunny@allwinnertech.com).
 * 2012-10-01 Written by superm (superm@allwinnertech.com).
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

#include "arisc_i.h"
#include <linux/sunxi-sid.h>
#include <linux/sunxi-smc.h>

#if defined CONFIG_ARCH_SUN8IW7P1
#include <asm/firmware.h>
extern char *arisc_binary_start;
extern char *arisc_binary_end;
unsigned long arisc_sram_a2_vbase =
	(unsigned long)IO_ADDRESS(SUNXI_SRAM_A2_PBASE);
#endif
#define DEBUG_POWER_TREE 0
struct arisc_cfg arisc_cfg;

#define ARISC_RESERVE_MEMSIZE (0x4000)
static unsigned int arisc_debug_baudrate = 115200;
unsigned int arisc_debug_dram_crc_en;
unsigned int arisc_debug_dram_crc_srcaddr = 0x40000000;
unsigned int arisc_debug_dram_crc_len = (1024 * 1024);
unsigned int arisc_debug_dram_crc_error;
unsigned int arisc_debug_dram_crc_total_count;
unsigned int arisc_debug_dram_crc_error_count;
unsigned int arisc_debug_level = 2;
static unsigned char arisc_version[40] = "arisc default version";
static unsigned int arisc_pll;

static struct arisc_rsb_block_cfg block_cfg;
static u32 devaddr;
static u8 regaddr;
static u32 data;
static u32 datatype;
static volatile u32 secure_board;

#if defined CONFIG_SUNXI_ARISC_COM_DIRECTLY
static atomic_t arisc_suspend_flag;

int arisc_suspend_flag_query(void)
{
	return atomic_read(&arisc_suspend_flag);
}

#if 0
static void sunxi_arisc_shutdown(struct platform_device *dev)
{
	atomic_set(&arisc_suspend_flag, 1);
	while (arisc_semaphore_used_num_query())
			msleep(1);
}
#endif

#else
static void sunxi_arisc_shutdown(struct platform_device *dev)
{
	/* do nothing */
}
#endif

/* for save power check configuration */
struct standby_info_para arisc_powchk_back;

static ssize_t arisc_version_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%s\n", arisc_version);

	return size;
}
static void *arisc_version_store(const void *src, size_t count)
{
	memcpy((void *)arisc_version, src, count);

	return (void *)arisc_version;
}

static ssize_t arisc_debug_mask_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", arisc_debug_level);

	return size;
}

static ssize_t arisc_debug_mask_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	u32 value = 0;

	sscanf(buf, "%u", &value);
	if ((value < 0) || (value > 3)) {
		ARISC_WRN("invalid arisc debug mask [%d] to set\n", value);
		return size;
	}

	arisc_debug_level = value;
	arisc_set_debug_level(arisc_debug_level);
	ARISC_LOG("debug_mask change to %d\n", arisc_debug_level);

	return size;
}

static ssize_t arisc_debug_baudrate_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", arisc_debug_baudrate);

	return size;
}

static ssize_t arisc_debug_baudrate_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	u32 value = 0;

	sscanf(buf, "%u", &value);
	if ((value != 115200) && (value != 57600) && (value != 9600)) {
		ARISC_WRN("invalid arisc uart baudrate [%d] to set\n", value);
		return size;
	}

	arisc_debug_baudrate = value;
	arisc_set_uart_baudrate(arisc_debug_baudrate);
	ARISC_LOG("debug_baudrate change to %d\n", arisc_debug_baudrate);

	return size;
}

static ssize_t arisc_dram_crc_paras_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "enable:0x%x srcaddr:0x%x lenght:0x%x\n",
		       arisc_debug_dram_crc_en, arisc_debug_dram_crc_srcaddr,
		       arisc_debug_dram_crc_len);

	return size;
}

static ssize_t arisc_dram_crc_paras_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	u32 dram_crc_en = 0;
	u32 dram_crc_srcaddr = 0;
	u32 dram_crc_len = 0;

	sscanf(buf, "%x %x %x\n", &dram_crc_en, &dram_crc_srcaddr,
	       &dram_crc_len);

	if ((dram_crc_en != 0) && (dram_crc_en != 1)) {
		ARISC_WRN("invalid arisc debug dram crc paras [%x] [%x] [%x] "
			  "to set\n",
			  dram_crc_en, dram_crc_srcaddr, dram_crc_len);

		return size;
	}

	arisc_debug_dram_crc_en = dram_crc_en;
	arisc_debug_dram_crc_srcaddr = dram_crc_srcaddr;
	arisc_debug_dram_crc_len = dram_crc_len;
	arisc_set_dram_crc_paras(arisc_debug_dram_crc_en,
				 arisc_debug_dram_crc_srcaddr,
				 arisc_debug_dram_crc_len);
	ARISC_LOG(
	    "dram_crc_en=0x%x, dram_crc_srcaddr=0x%x, dram_crc_len=0x%x\n",
	    arisc_debug_dram_crc_en, arisc_debug_dram_crc_srcaddr,
	    arisc_debug_dram_crc_len);

	return size;
}

static ssize_t arisc_dram_crc_result_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	arisc_query_dram_crc_result(
	    (unsigned long *)&arisc_debug_dram_crc_error,
	    (unsigned long *)&arisc_debug_dram_crc_total_count,
	    (unsigned long *)&arisc_debug_dram_crc_error_count);
	return sprintf(buf, "dram info:\n"
			    "  enable %u\n"
			    "  error %u\n"
			    "  total count %u\n"
			    "  error count %u\n"
			    "  src:%x\n"
			    "  len:0x%x\n",
		       arisc_debug_dram_crc_en, arisc_debug_dram_crc_error,
		       arisc_debug_dram_crc_total_count,
		       arisc_debug_dram_crc_error_count,
		       arisc_debug_dram_crc_srcaddr, arisc_debug_dram_crc_len);
}

static ssize_t arisc_dram_crc_result_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	u32 error = 0;
	u32 total_count = 0;
	u32 error_count = 0;

	sscanf(buf, "%u %u %u", &error, &total_count, &error_count);
	if ((error != 0) && (error != 1)) {
		ARISC_WRN(
		    "invalid arisc dram crc result [%d] [%d] [%d] to set\n",
		    error, total_count, error_count);
		return size;
	}

	arisc_debug_dram_crc_error = error;
	arisc_debug_dram_crc_total_count = total_count;
	arisc_debug_dram_crc_error_count = error_count;
	arisc_set_dram_crc_result(
	    (unsigned long)arisc_debug_dram_crc_error,
	    (unsigned long)arisc_debug_dram_crc_total_count,
	    (unsigned long)arisc_debug_dram_crc_error_count);
	ARISC_LOG("debug_dram_crc_result change to error:%u total count:%u "
		  "error count:%u\n",
		  arisc_debug_dram_crc_error, arisc_debug_dram_crc_total_count,
		  arisc_debug_dram_crc_error_count);

	return size;
}

static ssize_t arisc_power_enable_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "power enable 0x%x\n",
		       arisc_powchk_back.power_state.enable);
}

static ssize_t arisc_power_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value) < 0) {
		ARISC_ERR("illegal value, only one para support\n");
		return -EINVAL;
	}

	if (value &
	    ~(CPUS_ENABLE_POWER_EXP | CPUS_WAKEUP_POWER_STA |
	      CPUS_WAKEUP_POWER_CSM)) {
		ARISC_ERR(
		    "invalid format, 'enable' should:\n"
		    "  bit31:enable power check during standby\n"
		    "  bit1: enable wakeup when power state exception\n"
		    "  bit0: enable wakeup when power consume exception\n");
		return size;
	}

	arisc_powchk_back.power_state.enable = value;
	arisc_set_standby_power_cfg(&arisc_powchk_back);
	ARISC_LOG("standby_power_set enable:0x%x\n",
		  arisc_powchk_back.power_state.enable);

	return size;
}

static ssize_t arisc_power_state_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "power regs 0x%x\n",
		       arisc_powchk_back.power_state.power_reg);
}

static ssize_t arisc_power_state_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value) < 0) {
		ARISC_ERR("illegal value, only one para support");
		return -EINVAL;
	}
	arisc_powchk_back.power_state.power_reg = value;

	arisc_set_standby_power_cfg(&arisc_powchk_back);
	ARISC_LOG("standby_power_set power_state 0x%x\n",
		  arisc_powchk_back.power_state.power_reg);

	return size;
}

static ssize_t arisc_power_consum_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "power consume %dmw\n",
		       arisc_powchk_back.power_state.system_power);
}

static ssize_t arisc_power_consum_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value) < 0) {
		if (1 != sscanf(buf, "%lu", &value)) {
			ARISC_ERR("illegal value, only one para support");
			return -EINVAL;
		}
	}
	arisc_powchk_back.power_state.system_power = value;

	arisc_set_standby_power_cfg(&arisc_powchk_back);
	ARISC_LOG("standby_power_set power_consum %dmw\n",
		  arisc_powchk_back.power_state.system_power);

	return size;
}

#if (defined CONFIG_ARCH_SUN9IW1P1)
#define SST_POWER_MASK 0xffffffff
static const unsigned char pmu_powername[32][8] = {
	"dc5ldo", "dcdc1", "dcdc2", "dcdc3", "dcdc4", "dcdc5", "aldo1",
	"aldo2",
	"eldo1",  "eldo2", "eldo3", "dldo1", "dldo2", "aldo3", "swout",
	"dc1sw",
	"dcdca",  "dcdcb", "dcdcc", "dcdcd", "dcdce", "aldo1", "aldo2",
	"aldo3",
	"bldo1",  "bldo2", "bldo3", "bldo4", "cldo1", "cldo2", "cldo3",
	"swout",
};
#elif (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW3P1) ||    \
	(defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW7P1) || \
	(defined CONFIG_ARCH_SUN8IW9P1) || (defined CONFIG_ARCH_SUN50IW1P1) || \
	(defined CONFIG_ARCH_SUN50IW2P1)
#define SST_POWER_MASK 0x07ffff
static const unsigned char pmu_powername[20][8] = {
	"dc5ldo", "dcdc1", "dcdc2", "dcdc3",  "dcdc4",  "dcdc5", "aldo1",
	"aldo2",  "eldo1", "eldo2", "eldo3",  "dldo1",  "dldo2", "dldo3",
	"dldo4",  "dc1sw", "aldo3", "io0ldo", "io1ldo",
};
#elif (defined CONFIG_ARCH_SUN8IW6P1)
#define SST_POWER_MASK 0xffffff
static const unsigned char pmu_powername[26][10] = {
	"dcdc1",     "dcdc2", "dcdc3",     "dcdc4",     "dcdc5",  "dcdc6",
	"dcdc7",
	"reserved0", "eldo1", "eldo2",     "eldo3",     "dldo1",  "dldo2",
	"dldo3",
	"dldo4",     "dc1sw", "reserved1", "reserved2", "fldo1",  "fldo2",
	"fldo3",
	"aldo1",     "aldo2", "aldo3",     "io0ldo",    "io1ldo",
};
#elif (defined CONFIG_ARCH_SUN8IW12P1)
#define SST_POWER_MASK 0x1ff
static const unsigned char pmu_powername[9][10] = {
	"dcdc1", "dcdc2", "dcdc3", "dcdc4",  "aldo1",
	"aldo2", "dldo1", "dldo2", "io0ldo",
};
#endif

static ssize_t arisc_power_trueinfo_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	unsigned int i, count, power_reg;
	standby_info_para_t sst_info;
	unsigned char pmu_name[320];
	unsigned char *pbuf;

	arisc_query_standby_power(&sst_info);
	power_reg = sst_info.power_state.power_reg;
	power_reg &= SST_POWER_MASK;

#if (defined CONFIG_ARCH_SUN9IW1P1)
	/* print the pmu power on state */
	strcpy(pmu_name, "these power on during standby:\n  axp809:");
	pbuf = pmu_name + strlen("these power on during standby:\n  axp809:");
	count = 0;
	for (i = 0; i < 16; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}

	strcpy(pbuf, "\n  axp806:");
	pbuf += strlen("\n  axp806:");
	count = 0;
	for (i = 16; i < 32; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, "\n"); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}

	/* print the pmu power off state */
	power_reg ^= SST_POWER_MASK;

	strcpy(pbuf, "\nthese power off during standby:\n  axp809:");
	pbuf += strlen("\nthese power off during standby:\n  axp809:");
	count = 0;
	for (i = 0; i < 16; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}
	strcpy(pbuf, "\n  axp806:");
	pbuf += strlen("\n  axp806:");
	count = 0;
	for (i = 16; i < 32; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}
#else
	/* print the pmu power on state */
	strcpy(pmu_name, "these power on during standby:\n  axp:");
	pbuf = pmu_name + strlen("these power on during standby:\n  axp:");
	count = 0;
	for (i = 0; i < 32; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}

	/* print the pmu power off state */
	power_reg ^= SST_POWER_MASK;

	strcpy(pbuf, "\nthese power off during standby:\n  axp:");
	pbuf += strlen("\nthese power off during standby:\n  axp:");
	count = 0;
	for (i = 0; i < 32; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}
#endif

	return sprintf(buf, "power info:\n"
			    "  enable 0x%x\n"
			    "  regs 0x%x\n"
			    "  power consume %dmw\n"
			    "%s\n",
		       sst_info.power_state.enable,
		       sst_info.power_state.power_reg,
		       sst_info.power_state.system_power, pmu_name);
}

static ssize_t arisc_freq_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	struct clk *pll = NULL;

	pll = clk_get(NULL, "pll_cpu");

	if (!pll || IS_ERR(pll)) {
		ARISC_ERR("try to get pll%u failed!\n", arisc_pll);
		return size;
	}

	size = sprintf(buf, "%u\n", (unsigned int)clk_get_rate(pll));

	return size;
}

static ssize_t arisc_freq_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	u32 freq = 0;
	u32 pll = 0;
	u32 mode = 0;
	u32 ret = 0;

	sscanf(buf, "%u %u", &pll, &freq);

	if ((pll != 1) || (freq < 0) || (freq > 3000000)) {
		ARISC_WRN("invalid pll [%u] or freq [%u] to set, this platform "
			  "only support pll1, freq [0, 3000000]KHz\n",
			  pll, freq);
		ARISC_WRN("pls echo like that: echo pll freq > freq\n");
		return size;
	}

	arisc_pll = pll;
	ret = arisc_dvfs_set_cpufreq(freq, pll, mode, NULL, NULL);
	if (ret) {
		ARISC_ERR("pll%u freq set to %u fail\n", pll, freq);
	} else {
		ARISC_LOG("pll%u freq set to %u success\n", pll, freq);
	}
	return size;
}

static ssize_t arisc_rsb_read_block_data_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	ssize_t size = 0;
	u32 ret = 0;

	if ((block_cfg.regaddr == NULL) || (block_cfg.data == NULL) ||
	    (block_cfg.devaddr > 0xff) ||
	    ((block_cfg.datatype != RSB_DATA_TYPE_BYTE) &&
	     (block_cfg.datatype != RSB_DATA_TYPE_HWORD) &&
	     (block_cfg.datatype != RSB_DATA_TYPE_WORD))) {
		ARISC_WRN("invalid rsb paras, devaddr:0x%x, regaddr:0x%x, "
			  "datatype:0x%x\n",
			  block_cfg.devaddr,
			  block_cfg.regaddr ? *block_cfg.regaddr : 0,
			  block_cfg.datatype);
		ARISC_WRN("pls echo like that: echo devaddr regaddr datatype > "
			  "rsb_read_block_data\n");
		return size;
	}

	ret = arisc_rsb_read_block_data(&block_cfg);
	if (ret) {
		ARISC_LOG(
		    "rsb read data:0x%x from devaddr:0x%x regaddr:0x%x fail\n",
		    *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	} else {
		ARISC_LOG("rsb read data:0x%x from devaddr:0x%x regaddr:0x%x "
			  "success\n",
			  *block_cfg.data, block_cfg.devaddr,
			  *block_cfg.regaddr);
	}
	size = sprintf(buf, "%x\n", data);

	return size;
}

static ssize_t arisc_rsb_read_block_data_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t size)
{
	sscanf(buf, "%x %x %x", &devaddr, (u32 *)&regaddr, &datatype);
	if ((devaddr > 0xff) || ((datatype != RSB_DATA_TYPE_BYTE) &&
				 (datatype != RSB_DATA_TYPE_HWORD) &&
				 (datatype != RSB_DATA_TYPE_WORD))) {
		ARISC_WRN("invalid rsb paras to set, devaddr:0x%x, "
			  "regaddr:0x%x, datatype:0x%x\n",
			  devaddr, regaddr, datatype);
		ARISC_WRN("pls echo like that: echo devaddr regaddr datatype > "
			  "rsb_read_block_data\n");
		return size;
	}

	block_cfg.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	block_cfg.datatype = datatype;
	block_cfg.len = 1;
	block_cfg.devaddr = devaddr;
	block_cfg.regaddr = &regaddr;
	block_cfg.data = &data;

	ARISC_LOG("rsb read data from devaddr:0x%x regaddr:0x%x\n", devaddr,
		  regaddr);

	return size;
}

static ssize_t arisc_rsb_write_block_data_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	ssize_t size = 0;
	u32 ret = 0;

	if ((block_cfg.regaddr == NULL) || (block_cfg.data == NULL) ||
	    (block_cfg.devaddr > 0xff) ||
	    ((block_cfg.datatype != RSB_DATA_TYPE_BYTE) &&
	     (block_cfg.datatype != RSB_DATA_TYPE_HWORD) &&
	     (block_cfg.datatype != RSB_DATA_TYPE_WORD))) {
		ARISC_WRN("invalid rsb paras, devaddr:0x%x, regaddr:0x%x, "
			  "datatype:0x%x\n",
			  block_cfg.devaddr,
			  block_cfg.regaddr ? *block_cfg.regaddr : 0,
			  block_cfg.datatype);
		ARISC_WRN("pls echo like that: echo devaddr regaddr data "
			  "datatype > rsb_write_block_data\n");
		return size;
	}

	ret = arisc_rsb_read_block_data(&block_cfg);
	if (ret) {
		ARISC_ERR(
		    "rsb read data:0x%x from devaddr:0x%x regaddr:0x%x fail\n",
		    *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	} else {
		ARISC_LOG("rsb read data:0x%x from devaddr:0x%x regaddr:0x%x "
			  "success\n",
			  *block_cfg.data, block_cfg.devaddr,
			  *block_cfg.regaddr);
	}
	size = sprintf(buf, "%x\n", data);

	return size;
}

static ssize_t arisc_rsb_write_block_data_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	u32 ret = 0;

	sscanf(buf, "%x %x %x %x", &devaddr, (u32 *)&regaddr, (u32 *)&data,
	       &datatype);
	if ((devaddr > 0xff) || ((datatype != RSB_DATA_TYPE_BYTE) &&
				 (datatype != RSB_DATA_TYPE_HWORD) &&
				 (datatype != RSB_DATA_TYPE_WORD))) {
		ARISC_WRN("invalid rsb paras, devaddr:0x%x, regaddr:0x%x, "
			  "data:0x%x, datatype:0x%x\n",
			  devaddr, regaddr, data, datatype);
		ARISC_WRN("echo devaddr regaddr data"
				" datatype > rsb_write_block_data\n");
		return size;
	}

	block_cfg.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	block_cfg.datatype = datatype;
	block_cfg.len = 1;
	block_cfg.devaddr = devaddr;
	block_cfg.regaddr = &regaddr;
	block_cfg.data = &data;
	ret = arisc_rsb_write_block_data(&block_cfg);
	if (ret) {
		ARISC_ERR(
		    "rsb write data:0x%x to devaddr:0x%x regaddr:0x%x fail\n",
		    *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	} else {
		ARISC_LOG("rsb write data:0x%x to devaddr:0x%x regaddr:0x%x "
				"success\n",
			  *block_cfg.data, block_cfg.devaddr,
			  *block_cfg.regaddr);
	}

	return size;
}

#ifdef CONFIG_PM
static int sunxi_arisc_suspend(struct device *dev)
{
	return 0;
}

static int sunxi_arisc_resume(struct device *dev)
{
	u32 wake_event;
	standby_info_para_t sst_info;

#if defined CONFIG_SUNXI_ARISC_COM_DIRECTLY
	atomic_set(&arisc_suspend_flag, 0);
#endif
	arisc_query_wakeup_source(&wake_event);
	if (wake_event & CPUS_WAKEUP_POWER_EXP) {
		ARISC_LOG("power exception during standby, enable:0x%x" \
			  " expect state:0x%x, expect consumption:%dmw",
			  arisc_powchk_back.power_state.enable,
			  arisc_powchk_back.power_state.power_reg,
			  arisc_powchk_back.power_state.system_power);
		arisc_query_standby_power(&sst_info);
		ARISC_LOG(" real state:0x%x, real consumption:%dmw\n",
			  sst_info.power_state.power_reg,
			  sst_info.power_state.system_power);
	}

	return 0;
}

static const struct dev_pm_ops sunxi_arisc_dev_pm_ops = {
	.suspend = sunxi_arisc_suspend, .resume = sunxi_arisc_resume,
};

#define SUNXI_ARISC_DEV_PM_OPS (&sunxi_arisc_dev_pm_ops)
#else
#define SUNXI_ARISC_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static struct device_attribute sunxi_arisc_attrs[] = {
	__ATTR(version, S_IRUGO, arisc_version_show, NULL),
	__ATTR(debug_mask, S_IRUGO | S_IWUSR, arisc_debug_mask_show,
	   arisc_debug_mask_store),
	__ATTR(debug_baudrate, S_IRUGO | S_IWUSR, arisc_debug_baudrate_show,
	   arisc_debug_baudrate_store),
	__ATTR(dram_crc_paras, S_IRUGO | S_IWUSR, arisc_dram_crc_paras_show,
	   arisc_dram_crc_paras_store),
	__ATTR(dram_crc_result, S_IRUGO | S_IWUSR, arisc_dram_crc_result_show,
	   arisc_dram_crc_result_store),
	__ATTR(sst_power_enable_mask, S_IRUGO | S_IWUSR,
		arisc_power_enable_show, arisc_power_enable_store),
	__ATTR(sst_power_state_mask, S_IRUGO | S_IWUSR, arisc_power_state_show,
	   arisc_power_state_store),
	__ATTR(sst_power_consume_mask, S_IRUGO | S_IWUSR,
		arisc_power_consum_show, arisc_power_consum_store),
	__ATTR(sst_power_real_info, S_IRUGO, arisc_power_trueinfo_show, NULL),
	__ATTR(freq, S_IRUGO | S_IWUSR, arisc_freq_show, arisc_freq_store),
#if (defined CONFIG_ARCH_SUN50IW2P1)
	__ATTR(regulator_state, S_IRUGO | S_IWUSR, arisc_regulator_state_show,
	   arisc_regulator_state_store),
	__ATTR(regulator_voltage, S_IRUGO | S_IWUSR,
		arisc_regulator_voltage_show, arisc_regulator_voltage_store),
	__ATTR(twi_read_block_data, S_IRUGO | S_IWUSR,
	   arisc_twi_read_block_data_show, arisc_twi_read_block_data_store),
	__ATTR(twi_write_block_data, S_IRUGO | S_IWUSR,
	   arisc_twi_write_block_data_show, arisc_twi_write_block_data_store),
#else
	__ATTR(rsb_read_block_data, S_IRUGO | S_IWUSR,
	   arisc_rsb_read_block_data_show, arisc_rsb_read_block_data_store),
	__ATTR(rsb_write_block_data, S_IRUGO | S_IWUSR,
	   arisc_rsb_write_block_data_show, arisc_rsb_write_block_data_store),
#endif
};

static void sunxi_arisc_sysfs(struct platform_device *pdev)
{
	unsigned int i;
	memset((void *)&block_cfg, 0, sizeof(block_cfg));
	for (i = 0; i < ARRAY_SIZE(sunxi_arisc_attrs); i++) {
		device_create_file(&pdev->dev, &sunxi_arisc_attrs[i]);
	}
}

static int sunxi_arisc_clk_cfg(struct platform_device *pdev)
{
	ARISC_INF("device [%s] clk resource request enter\n",
		  dev_name(&pdev->dev));

	if (clk_prepare_enable(arisc_cfg.core.losc)) {
		ARISC_ERR("try to enable losc output failed!\n");
		return -EINVAL;
	}

	if (clk_prepare_enable(arisc_cfg.core.iosc)) {
		ARISC_ERR("try to enable iosc output failed!\n");
		return -EINVAL;
	}

	if (clk_prepare_enable(arisc_cfg.core.hosc)) {
		ARISC_ERR("try to enable hosc output failed!\n");
		return -EINVAL;
	}

	if (clk_prepare_enable(arisc_cfg.core.pllperiph0)) {
		ARISC_ERR("try to enable pll_periph0 output failed!\n");
		return -EINVAL;
	}

	if (clk_prepare_enable(arisc_cfg.dram.pllddr0)) {
		ARISC_ERR("try to enable pll_ddr0 output failed!\n");
		return -EINVAL;
	}

	ARISC_INF("device [%s] clk resource request ok\n",
		  dev_name(&pdev->dev));

	return 0;
}

static int sunxi_arisc_pin_cfg(struct platform_device *pdev)
{
	struct platform_device *pdev_suart, *pdev_sjtag;
	struct platform_device __maybe_unused *pdev_stwi;
	struct platform_device __maybe_unused *pdev_srsb;
	struct pinctrl *pinctrl = NULL;

	ARISC_INF("device [%s] pin resource request enter\n",
		  dev_name(&pdev->dev));
	/* s_uart0 gpio */
	if (arisc_cfg.suart.status) {
		pdev_suart = of_find_device_by_node(arisc_cfg.suart.np);
		if (!pdev_suart) {
			ARISC_ERR("get s_uart0 platform_device error!\n");
			return -EINVAL;
		}
		pinctrl = pinctrl_get_select_default(&pdev_suart->dev);
		if (!pinctrl || IS_ERR(pinctrl)) {
			ARISC_ERR("set s_uart0 pin error!\n");
			return -EINVAL;
		}

		ARISC_INF("set s_uart0 pin OK\n");
	}

	/* s_jtag0 gpio */
	if (arisc_cfg.sjtag.status) {
		pdev_sjtag = of_find_device_by_node(arisc_cfg.sjtag.np);
		if (!pdev_sjtag) {
			ARISC_ERR("get s_jtag0 platform_device error!\n");
			return -EINVAL;
		}
		pinctrl = pinctrl_get_select_default(&pdev_sjtag->dev);
		if (!pinctrl || IS_ERR(pinctrl)) {
			ARISC_ERR("set s_jtag0 pin error!\n");
			return -EINVAL;
		}
	}

	ARISC_INF("device [%s] pin resource request ok\n",
		  dev_name(&pdev->dev));

	return 0;
}

#if DEBUG_POWER_TREE
void hexdump(char *name, char *base, int len)
{
	u32 i;
	printk("%s :\n", name);
	for (i = 0; i < len; i += 4) {
		if (!(i & 0xf))
			printk("\n0x%8p : ", base + i);
		printk("%8x ", readl(base + i));
	}
	printk("\n");
}
#endif

static int sunxi_arisc_parse_cfg(struct platform_device *pdev)
{
	struct resource res;
	u32 ret;

	union space_union space;
	struct device_node *np;

	arisc_cfg.core.np = pdev->dev.of_node;

	/* parse arisc node */
	arisc_cfg.core.losc = of_clk_get_by_name(arisc_cfg.core.np, "losc");
	if (!arisc_cfg.core.losc || IS_ERR(arisc_cfg.core.losc)) {
		ARISC_ERR("try to get losc failed!\n");
		return -EINVAL;
	}

	arisc_cfg.core.iosc = of_clk_get_by_name(arisc_cfg.core.np, "iosc");
	if (!arisc_cfg.core.iosc || IS_ERR(arisc_cfg.core.iosc)) {
		ARISC_ERR("try to get iosc failed!\n");
		return -EINVAL;
	}

	arisc_cfg.core.hosc = of_clk_get_by_name(arisc_cfg.core.np, "hosc");
	if (!arisc_cfg.core.hosc || IS_ERR(arisc_cfg.core.hosc)) {
		ARISC_ERR("try to get hosc failed!\n");
		return -EINVAL;
	}

	arisc_cfg.core.pllperiph0 =
	    of_clk_get_by_name(arisc_cfg.core.np, "pll_periph0");
	if (!arisc_cfg.core.pllperiph0 || IS_ERR(arisc_cfg.core.pllperiph0)) {
		ARISC_ERR("try to get pll_periph0 failed!\n");
		return -EINVAL;
	}

	/* parse dram node */
	arisc_cfg.dram.np =
	    of_find_compatible_node(NULL, NULL, "allwinner,dram");
	if (IS_ERR(arisc_cfg.dram.np)) {
		ARISC_ERR("get [allwinner,dram] device node error\n");
		return -EINVAL;
	}

	arisc_cfg.dram.pllddr0 =
	    of_clk_get_by_name(arisc_cfg.dram.np, "pll_ddr");
	if (!arisc_cfg.dram.pllddr0 || IS_ERR(arisc_cfg.dram.pllddr0)) {
		ARISC_ERR("try to get pll_ddr0 failed!\n");
		return -EINVAL;
	}

	/* parse s_uart node */
	arisc_cfg.suart.np =
	    of_find_compatible_node(NULL, NULL, "allwinner,s_uart");
	if (IS_ERR(arisc_cfg.suart.np)) {
		ARISC_ERR("get [allwinner,s_uart] device node error\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(arisc_cfg.suart.np, 0, &res);
	if (ret || !res.start) {
		ARISC_ERR("get suart pbase error\n");
		return -EINVAL;
	}
	arisc_cfg.suart.pbase = res.start;
	arisc_cfg.suart.size = resource_size(&res);

	arisc_cfg.suart.vbase = of_iomap(arisc_cfg.suart.np, 0);
	if (!arisc_cfg.suart.vbase)
		panic("Can't map suart registers");

	arisc_cfg.suart.irq = irq_of_parse_and_map(arisc_cfg.suart.np, 0);
	if (arisc_cfg.suart.irq <= 0)
		panic("Can't parse suart IRQ");

	arisc_cfg.suart.status = of_device_is_available(arisc_cfg.suart.np);

	ARISC_INF(
	    "suart pbase:0x%p, vbase:0x%p, size:0x%zx, irq:0x%x, status:%u\n",
	    (void *)arisc_cfg.suart.pbase, arisc_cfg.suart.vbase,
	    arisc_cfg.suart.size, arisc_cfg.suart.irq, arisc_cfg.suart.status);

	/* parse msgbox node */
	arisc_cfg.msgbox.np =
	    of_find_compatible_node(NULL, NULL, "allwinner,msgbox");
	if (IS_ERR(arisc_cfg.msgbox.np)) {
		ARISC_ERR("get [allwinner,msgbox] device node error\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(arisc_cfg.msgbox.np, 0, &res);
	if (ret || !res.start) {
		ARISC_ERR("get msgbox pbase error\n");
		return -EINVAL;
	}
	arisc_cfg.msgbox.pbase = res.start;
	arisc_cfg.msgbox.size = resource_size(&res);

	arisc_cfg.msgbox.vbase = of_iomap(arisc_cfg.msgbox.np, 0);
	if (!arisc_cfg.msgbox.vbase)
		panic("Can't map msgbox registers");

	arisc_cfg.msgbox.irq = irq_of_parse_and_map(arisc_cfg.msgbox.np, 0);
	if (arisc_cfg.msgbox.irq <= 0)
		panic("Can't parse msgbox IRQ");

	arisc_cfg.msgbox.status = of_device_is_available(arisc_cfg.msgbox.np);

	ARISC_INF(
	    "msgbox pbase:0x%p, vbase:0x%p, size:0x%x, irq:0x%x, status:%u\n",
	    (void *)arisc_cfg.msgbox.pbase, arisc_cfg.msgbox.vbase,
	    arisc_cfg.msgbox.size, arisc_cfg.msgbox.irq,
	    arisc_cfg.msgbox.status);

	/* parse s_cpucfg node */
	arisc_cfg.cpuscfg.np =
	    of_find_compatible_node(NULL, NULL, "allwinner,s_cpuscfg");
	if (IS_ERR(arisc_cfg.cpuscfg.np)) {
		ARISC_ERR("get [allwinner,s_cpuscfg] device node error\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(arisc_cfg.cpuscfg.np, 0, &res);
	if (ret || !res.start) {
		ARISC_ERR("get s_cpuscfg pbase error\n");
		return -EINVAL;
	}
	arisc_cfg.cpuscfg.pbase = res.start;
	arisc_cfg.cpuscfg.size = resource_size(&res);

	arisc_cfg.cpuscfg.vbase = of_iomap(arisc_cfg.cpuscfg.np, 0);
	if (!arisc_cfg.cpuscfg.vbase)
		panic("Can't map cpuscfg registers");
	ARISC_INF("cpuscfg pbase:0x%p, vbase:0x%p, size:0x%x\n",
		  (void *)arisc_cfg.cpuscfg.pbase, arisc_cfg.cpuscfg.vbase,
		  arisc_cfg.cpuscfg.size);

	np = of_find_compatible_node(NULL, NULL, "allwinner,arisc_space");
	if (IS_ERR(np)) {
		ARISC_ERR("get [allwinner,arisc_space] device node error\n");
		return -EINVAL;
	}
	ret = of_property_read_u32_array(np, "space4", space.value, 3);
	if (ret) {
		ARISC_ERR("get arisc_space1 error.\n");
		return -EINVAL;
	}

	arisc_cfg.space[0].vbase = phys_to_virt(space.info.dst);
	arisc_cfg.space[0].size = space.info.size;

	ret = of_property_read_u32_array(np, "space2", space.value, 3);
	if (ret) {
		ARISC_ERR("get arisc_space2 error.\n");
		return -EINVAL;
	}

	arisc_cfg.space[1].vbase = phys_to_virt(space.info.dst);
	arisc_cfg.space[1].size = space.info.size;

	/* parse s_jtag node */
	arisc_cfg.sjtag.np =
	    of_find_compatible_node(NULL, NULL, "allwinner,s_jtag");
	if (IS_ERR(arisc_cfg.sjtag.np)) {
		ARISC_ERR("get [allwinner,s_jtag] device node error\n");
		return -EINVAL;
	}
	arisc_cfg.sjtag.status = of_device_is_available(arisc_cfg.sjtag.np);

	ARISC_INF("sjtag status:%u\n", arisc_cfg.sjtag.status);

	return 0;
}
static s32 sunxi_arisc_para_init(struct arisc_para *para)
{
	if (arisc_cfg.suart.status == 1)
		para->uart_pin_used = 1;
	else
		para->uart_pin_used = 0;

	return 0;
}

static void sunxi_arisc_setup_para(struct arisc_para *para)
{
	void *dest;

	if (secure_board == 1) {
		dest = (void *)((phys_to_virt)(ARISC_SECURE_PARAS_STORE));
		/* copy arisc parameters to target address */
		memcpy(dest, (void *)para, ARISC_PARA_SIZE);
		flush_cache_all();
#ifdef CONFIG_SUNXI_SMC
		sunxi_smc_copy_arisc_paras(
			(phys_addr_t)(SUNXI_SRAM_A2_PBASE
				+ ARISC_PARA_ADDR_OFFSET),
			(phys_addr_t)(ARISC_SECURE_PARAS_STORE),
			ARISC_PARA_SIZE);
#endif
	} else {
		dest = (void *)(arisc_sram_a2_vbase + ARISC_PARA_ADDR_OFFSET);
		memcpy(dest, (void *)para, ARISC_PARA_SIZE);
	}
	ARISC_INF("setup arisc para sram_a2 finished\n");
}

static int sunxi_arisc_probe(struct platform_device *pdev);
#if (defined CONFIG_SUNXI_ARISC_COM_DIRECTLY)

int sunxi_deassert_arisc(void)
{
	volatile unsigned long value;

	ARISC_INF("set arisc reset to de-assert state\n");

	if (secure_board == 1) {
		value = sunxi_readl((phys_addr_t)
				    (arisc_cfg.cpuscfg.pbase + 0x0));
		value &= ~1;
		sunxi_writel(value,
			    (phys_addr_t)(arisc_cfg.cpuscfg.pbase + 0x0));
		value = sunxi_readl(
			    (phys_addr_t)(arisc_cfg.cpuscfg.pbase + 0x0));
		value |= 1;
		sunxi_writel(value,
			     (phys_addr_t)(arisc_cfg.cpuscfg.pbase + 0x0));
	} else {
		value = readl(arisc_cfg.cpuscfg.vbase + 0x0);
		value &= ~1;
		writel(value, arisc_cfg.cpuscfg.vbase + 0x0);
		value = readl(arisc_cfg.cpuscfg.vbase + 0x0);
		value |= 1;
		writel(value, arisc_cfg.cpuscfg.vbase + 0x0);
	}


	return 0;
}

static int arisc_wait_ready(unsigned int timeout)
{
	unsigned long expire;

	expire = msecs_to_jiffies(timeout) + jiffies;

	/* wait arisc startup ready */
	while (1) {
		/*
		 * linux cpu interrupt is disable now,
		 * we should query message by hand.
		 */
		struct arisc_message *pmessage = arisc_hwmsgbox_query_message();
		if (pmessage == NULL) {
			if (time_is_before_eq_jiffies(expire)) {
				return -ETIMEDOUT;
			}
			/* try to query again */
			continue;
		}
		/* query valid message */
		if (pmessage->type == ARISC_STARTUP_NOTIFY) {
			/* check arisc software and driver version match or not
			 */
			if (pmessage->paras[0] != ARISC_VERSIONS) {
	ARISC_ERR("arisc firmware:%d and driver version:%d not matched\n"
					, pmessage->paras[0], ARISC_VERSIONS);
				return -EINVAL;
			} else {
				/* printf the main and sub version string */
				ARISC_LOG(
				    "arisc version: [%s]\n",
				    (char *)arisc_version_store(
					(const void *)(&(pmessage->paras[1])),
					40));
			}

			/* received arisc startup ready message */
			ARISC_INF("arisc startup ready\n");
			if ((pmessage->attr & ARISC_MESSAGE_ATTR_SOFTSYN) ||
			    (pmessage->attr & ARISC_MESSAGE_ATTR_HARDSYN)) {
				/* synchronous message, just feedback it */
				ARISC_INF(
				    "arisc startup notify message feedback\n");
				pmessage->paras[0] = 0;
				arisc_hwmsgbox_feedback_message(
				    pmessage, ARISC_SEND_MSG_TIMEOUT);
			} else {
				/* asyn message, free message directly */
		ARISC_INF("arisc startup notify message free directly\n");
				arisc_message_free(pmessage);
			}
			break;
		}
		/*
		 * invalid message detected, ignore it.
		 * by sunny at 2012-7-6 18:34:38.
		 */
		ARISC_WRN("arisc startup waiting ignore message\n");
		if ((pmessage->attr & ARISC_MESSAGE_ATTR_SOFTSYN) ||
		    (pmessage->attr & ARISC_MESSAGE_ATTR_HARDSYN)) {
			/* synchronous message, just feedback it */
			arisc_hwmsgbox_send_message(pmessage,
						    ARISC_SEND_MSG_TIMEOUT);
		} else {
			/* asyn message, free message directly */
			arisc_message_free(pmessage);
		}
		/* we need waiting continue */
	}

	return 0;
}
#if defined CONFIG_ARCH_SUN8IW7P1
u32 sunxi_load_arisc(void *image, u32 image_size, void *para, u32 para_size)
{
	void *dest;
	/*
	if (sunxi_soc_is_secure()) {
			ret = call_firmware_op(load_arisc, image, image_size,
						para, para_size,
	   ARISC_PARA_ADDR_OFFSET);
		} else {
	*/
	/* clear sram_a2 area */
	ARISC_INF("IMG ADDR %p  srama2 %lx", image, arisc_sram_a2_vbase);
	memset((void *)arisc_sram_a2_vbase, 0, SUNXI_SRAM_A2_SIZE);

	/* load arisc system binary data to sram_a2 */
	memcpy((void *)arisc_sram_a2_vbase, image, image_size);
	ARISC_INF("load arisc image finish\n");

	/* setup arisc parameters */
	dest = (void *)(arisc_sram_a2_vbase + ARISC_PARA_ADDR_OFFSET);
	memcpy(dest, (void *)para, para_size);
	ARISC_INF("setup arisc para finish\n");
	flush_cache_all();

	/* relese arisc reset */
	sunxi_deassert_arisc();
	ARISC_INF("release arisc reset finish\n");
	/*	}*/
	ARISC_INF("load arisc finish\n");

	return 0;
}
#endif

#ifdef CONFIG_ARCH_SUN8IW7P1
static int arisc_pmu_config(void)
{
	int result = 0;
	unsigned int pmu_type;
	struct device_node *pmu_config_node = NULL;
	struct arisc_message *pmessage;

	/* allocate a message frame */
	pmessage = arisc_message_allocate(ARISC_MESSAGE_ATTR_HARDSYN);
	if (pmessage == NULL) {
		ARISC_WRN("allocate message failed\n");
		return -ENOMEM;
	}

/* [pmuic_type]:0~2, 0:none, 1:gpio, 2:i2c
 * [gpio0_cfg ]:bit0~15:gpio num, bit16:used
 * [gpio1_cfg ]:bit0~15:gpio num, bit16:used
 * [pmu_level0]:bit0~15:voltage(mV), bit16~19:gpio0 state, bit20~23:gpio1 state
 * [pmu_level1]:bit0~15:voltage(mV), bit16~19:gpio0 state, bit20~23:gpio1 state
 * [pmu_level2]:bit0~15:voltage(mV), bit16~19:gpio0 state, bit20~23:gpio1 state
 * [pmu_level3]:bit0~15:voltage(mV), bit16~19:gpio0 state, bit20~23:gpio1 state
 */
	/*get pmuic type */
	pmu_config_node = of_find_node_by_type(NULL, "dvfs_table");
	if (pmu_config_node == NULL) {
		arisc_message_free(pmessage);
		ARISC_ERR("pmu config get node error\n");
		return -EINVAL;
	}

	if (of_property_read_u32(pmu_config_node, "pmuic_type", &pmu_type)) {
		arisc_message_free(pmessage);
		ARISC_ERR("pmu config get type error\n");
		return -EINVAL;
	}

	pmessage->paras[0] = pmu_type;

	if (pmessage->paras[0] != 0) {
		arisc_message_free(pmessage);
		ARISC_ERR("pmu has the wrong type\n");
		return -EINVAL;

	}

	pmessage->type = ARISC_CPUX_DVFS_CFG_REQ;
	pmessage->state = ARISC_MESSAGE_INITIALIZED;
	pmessage->cb.handler = NULL;
	pmessage->cb.arg = NULL;

	arisc_hwmsgbox_send_message(pmessage, ARISC_SEND_MSG_TIMEOUT);

	if (pmessage->result) {
		ARISC_WRN("config dvfs cfg fail\n");
		result = -EINVAL;
	}

	/* free allocated message */
	arisc_message_free(pmessage);
	return result;
}
#endif
static void sunxi_rest_cfg(void)
{
	struct arisc_para para;
	u32 message_addr;
	u32 message_phys;
	u32 message_size;

	secure_board = sunxi_soc_is_secure();

	/* initialize hwmsgbox */
	ARISC_LOG("hwmsgbox initialize\n");


	/* setup arisc parameter */
	memset(&para, 0, sizeof(struct arisc_para));
	sunxi_arisc_para_init(&para);
	/* allocate shared message buffer,
	 * the shared buffer should be non-cacheable.
	 * secure    : sram-a2 last 4k byte;
	 * non-secure: dram non-cacheable buffer.
	 */
	if (secure_board == 1) {
		ARISC_INF("sunxi_soc_is_secure\n");
		message_addr = (u32)dma_alloc_coherent(
		    NULL, PAGE_SIZE, &(message_phys), GFP_KERNEL);
		message_size = PAGE_SIZE;
		para.message_pool_phys = message_phys;
		para.message_pool_size = message_size;
	} else {
		ARISC_INF("sunxi_soc_is_not_secure\n");
#if (defined CONFIG_ARCH_SUN8IW7P1)
		/* use sram-a2 last 4k byte */
		message_addr =
		    (u32)arisc_sram_a2_vbase + ARISC_MESSAGE_POOL_START;
		message_size =
		    ARISC_MESSAGE_POOL_END - ARISC_MESSAGE_POOL_START;
		para.message_pool_phys = ARISC_MESSAGE_POOL_START;
		para.message_pool_size =
		    ARISC_MESSAGE_POOL_END - ARISC_MESSAGE_POOL_START;
#else
		message_addr = (u32)arisc_cfg.space[0].vbase;
		message_size = 0x1000;
#endif
	}

	/* initialize message manager */
	ARISC_LOG("message manager initialize start:%x, end:%x\n", message_addr,
		  message_size);
	arisc_hwmsgbox_init();
	arisc_message_manager_init((void *)message_addr, message_size);

	sunxi_arisc_setup_para(&para);
	flush_cache_all();
	sunxi_deassert_arisc();

	ARISC_INF("release arisc reset finish\n");
	/* wait arisc ready */
	ARISC_INF("wait arisc ready....\n");
	if (arisc_wait_ready(10000)) {
		ARISC_LOG("arisc startup failed\n");
	}

	/* enable arisc asyn tx interrupt */
	arisc_hwmsgbox_enable_receiver_int(ARISC_HWMSGBOX_ARISC_ASYN_TX_CH,
					   AW_HWMSG_QUEUE_USER_AC327);

	/* enable arisc syn tx interrupt */
	arisc_hwmsgbox_enable_receiver_int(ARISC_HWMSGBOX_ARISC_SYN_TX_CH,
					   AW_HWMSG_QUEUE_USER_AC327);

	atomic_set(&arisc_suspend_flag, 0);

	/*config pmu config paras*/
	if (arisc_pmu_config()) {
		ARISC_WRN("config pmu paras failed\n");
	}

	/* config ir config paras */
	if (arisc_sysconfig_ir_paras()) {
		ARISC_WRN("config ir paras failed\n");
	}

	/* config dram config paras */
	if (arisc_config_dram_paras()) {
		ARISC_WRN("config dram paras failed\n");
	}

	if (!pm_power_off)
		pm_power_off = arisc_power_off;
	else
		ARISC_WRN("pm_power_off aleardy been registered!\n");
}
#else
static void sunxi_rest_cfg(void)
{
	/* do nothing */
}
#endif

#ifdef CONFIG_AW_AXP
#if (defined CONFIG_ARCH_SUN8IW5P1)
static int axp_notify_call(struct notifier_block *nfb, unsigned long action,
			   void *parg)
{
	return NOTIFY_OK;
}
#else
static int axp_notify_call(struct notifier_block *nfb, unsigned long action,
			   void *parg)
{
	switch (action) {
	case AXP_READY:
		/* axp ready now, should send power tree to cpus */
		/* get power regulator tree */
		axp_get_pwr_regu_tree(arisc_cfg.power_regu_tree);
		arisc_set_pwr_tree(arisc_cfg.power_regu_tree);
#if DEBUG_POWER_TREE
		hexdump("tree", arisc_cfg.power_regu_tree,
			sizeof(arisc_cfg.power_regu_tree));
#endif
		break;
	}

	return NOTIFY_OK;
}
#endif

static struct notifier_block axp_notifier = {&axp_notify_call, NULL, 0};
#endif

static int sunxi_arisc_probe(struct platform_device *pdev)
{
	int ret;

	ARISC_INF("arisc initialize\n");

	sunxi_arisc_parse_cfg(pdev);

	/* cfg sunxi arisc clk */
	ret = sunxi_arisc_clk_cfg(pdev);
	if (ret) {
		ARISC_ERR("sunxi-arisc clk cfg failed\n");
		return -EINVAL;
	}
	ret = sunxi_arisc_pin_cfg(pdev);
	if (ret) {
		ARISC_ERR("sunxi-arisc pin cfg failed\n");
		return -EINVAL;
	}

	sunxi_arisc_sysfs(pdev);
/* register axp regulator notify, so the axp regulator will
 * call the arisc set power tree callback function when it ready
 */
#ifdef CONFIG_AW_AXP
	ret = raw_notifier_chain_register(&axp_regu_notifier, &axp_notifier);
#endif

	sunxi_rest_cfg();
	/* arisc init ok */
	arisc_notify(ARISC_INIT_READY, NULL);

	/* arisc initialize succeeded */
	ARISC_LOG("sunxi-arisc driver v%s startup succeeded\n", DRV_VERSION);

	return ret;
}

static const struct of_device_id sunxi_arisc_match[] = {
	{
		.compatible = "allwinner,sunxi-arisc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sunxi_arisc_match);

static struct platform_driver sunxi_arisc_driver = {
	.probe = sunxi_arisc_probe,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = SUNXI_ARISC_DEV_PM_OPS,
		.of_match_table = sunxi_arisc_match,
	},
};

static int __init arisc_init(void)
{
	int ret;

	ARISC_LOG("sunxi-arisc driver v%s\n", DRV_VERSION);

	ret = platform_driver_register(&sunxi_arisc_driver);
	if (IS_ERR_VALUE(ret)) {
		ARISC_ERR("register sunxi arisc platform driver failed\n");
		goto err_platform_driver_register;
	}

	return 0;

err_platform_driver_register:
	platform_driver_unregister(&sunxi_arisc_driver);
	return -EINVAL;
}

static void __exit arisc_exit(void)
{
	platform_driver_unregister(&sunxi_arisc_driver);
	ARISC_LOG("module unloaded\n");
}

subsys_initcall(arisc_init);
module_exit(arisc_exit);

MODULE_DESCRIPTION("SUNXI ARISC Driver");
MODULE_AUTHOR("Superm Wu <superm@allwinnertech.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:sunxi arisc driver");
