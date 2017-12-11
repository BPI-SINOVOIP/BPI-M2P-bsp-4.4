/*
 * drivers/soc/sunxi/pm/pm_extended_standby.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Liming <liming@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/sunxi-gpio.h>
#include <linux/power/aw_pm.h>
#include <linux/power/scenelock.h>
#include "pm.h"

static DEFINE_SPINLOCK(data_lock);

extended_standby_t temp_standby_data = {
	.id = 0,
};

static struct extended_standby_manager_t exstandby_manager = {
	.pextended_standby = NULL,
	.event = 0,
	.wakeup_gpio_map = 0,
	.wakeup_gpio_group = 0,
};

static bool calculate_pll(int index,
				struct scene_extended_standby_t *standby_data)
{
	__u32 standby_rate;
	__u32 tmp_standby_rate;
	__u32 dividend;
	__u32 divisor;
	cpux_clk_para_t *pclk_para;
	cpux_clk_para_t *ptmp_clk_para;

	pclk_para = &(standby_data->soc_pwr_dep.cpux_clk_state);
	ptmp_clk_para = &(temp_standby_data.cpux_clk_state);

	switch (index) {
	case PM_PLL_C0:
	case PM_PLL_C1:
	case PM_PLL_AUDIO:
	case PM_PLL_VIDEO0:
	case PM_PLL_VE:
	case PM_PLL_DRAM:
		pr_err("%s err: not ready.\n", __func__);
		break;
	case PM_PLL_PERIPH:
		dividend = pclk_para->pll_factor[index].factor1
			* (pclk_para->pll_factor[index].factor2 + 1);
		divisor = pclk_para->pll_factor[index].factor3 + 1;
		standby_rate = do_div(dividend, divisor);

		dividend = ptmp_clk_para->pll_factor[index].factor1
			* (ptmp_clk_para->pll_factor[index].factor2 + 1);
		divisor = ptmp_clk_para->pll_factor[index].factor3 + 1;
		tmp_standby_rate = do_div(dividend, divisor);

		if (standby_rate > tmp_standby_rate)
			return true;
		else
			return false;
	case PM_PLL_GPU:
	case PM_PLL_HSIC:
	case PM_PLL_DE:
	case PM_PLL_VIDEO1:
		pr_err("%s err: not ready.\n", __func__);
		break;
	default:
		pr_err("%s err: input para.\n", __func__);
		break;
	}

	return false;
}

static bool calculate_bus(int index,
				struct scene_extended_standby_t *standby_data)
{
	cpux_clk_para_t *pclk_para = &standby_data->soc_pwr_dep.cpux_clk_state;
	cpux_clk_para_t *ptmp_clk_para = &temp_standby_data.cpux_clk_state;

	if (index >= BUS_NUM) {
		if (pclk_para->bus_factor[index].src
				> ptmp_clk_para->bus_factor[index].src)
			return true;
		else
			return false;
	} else {
		pr_info("%s: input para err.\n", __func__);
	}

	return false;
}

/*
 * function: make dependency check,
 * for make sure the pwr dependency is reasonable.
 * return 0: if reasonable.
 *        -1: if not reasonable.
 */
static int check_cfg(void)
{
	int ret = 0;
	int i = 0;
	cpux_clk_para_t *ptmp_clk_para = &(temp_standby_data.cpux_clk_state);

	/* make sure bus parent is exist. */
	if (ptmp_clk_para->bus_change != 0) {
		for (i = 0; i < BUS_NUM; i++) {
			if ((ptmp_clk_para->bus_factor[i].src == CLK_SRC_LOSC)
				&& !(ptmp_clk_para->osc_en
				& BITMAP(OSC_LOSC_BIT))) {
				ret = -1;
			}

			if ((ptmp_clk_para->bus_factor[i].src == CLK_SRC_HOSC)
				&& !(ptmp_clk_para->osc_en
				& BITMAP(OSC_HOSC_BIT))) {
				ret = -2;
			}

			if ((ptmp_clk_para->bus_factor[i].src == CLK_SRC_PLL6)
				&& !(ptmp_clk_para->init_pll_dis
				& BITMAP(PM_PLL_PERIPH))) {
				ret = -3;
			}
		}
	}

	/* check hold_flag is reasonable. */
	if (temp_standby_data.soc_io_state.hold_flag == 1
		&& (temp_standby_data.soc_pwr_dm_state.state
		& temp_standby_data.soc_pwr_dm_state.sys_mask
		& BITMAP(VDD_SYS_BIT))) {
		/* when vdd_sys is ON, no need to set hold_flag; */
		ret = -11;
	}

	/* make sure selfresh flag is reasonable */
	if (temp_standby_data.soc_dram_state.selfresh_flag == 0) {
		/* when selfresh is disable, then VDD_SYS_BIT is needed */
		if (!(temp_standby_data.soc_pwr_dm_state.state
				& temp_standby_data.soc_pwr_dm_state.sys_mask
				& BITMAP(VDD_SYS_BIT))) {
			ret = -21;
		}
	}

	if (ret == -1) {
		pr_info("func: %s, ret = %d.\n", __func__, ret);
		dump_stack();
	}

	return ret;
}

/*
 * return value:
 *        0: merge io done for ptmp_io_para->io_state[i]
 *        -1: exist new io config, need to remerge.
 */
static int merge_io(soc_pwr_dep_t *ppwr_dep, int j,
				soc_io_para_t *ptmp_io_para, int i)
{
	int new_config_flag = 0;
	soc_io_para_t *io_state = &(ppwr_dep->soc_io_state);
	int ret = -1;

	/* orig configed */
	/* when io has not been initialized. */
	if (ptmp_io_para->io_state[i].paddr == 0) {
		ptmp_io_para->io_state[i].paddr = io_state->io_state[j].paddr;
		ptmp_io_para->io_state[i].value_mask =
				io_state->io_state[j].value_mask;
		ptmp_io_para->io_state[i].value = io_state->io_state[j].value;
		ret = 0;
	} else {
		if ((ptmp_io_para->io_state[i].paddr
				== io_state->io_state[j].paddr)
				&& (ptmp_io_para->io_state[i].value_mask
				== io_state->io_state[j].value_mask)) {
			if (ptmp_io_para->io_state[i].value
				!= io_state->io_state[j].value) {
				pr_info("NOTICE: io config conflict.\n");
				dump_stack();
			} else {
				pr_info("NOTICE: io config is the same.\n");
				new_config_flag = 0;
				ret = 0;
			}
		} else {
			/* new config? */
			new_config_flag = 1;
		}
	}

	if (new_config_flag == 1)
		pr_err("NOTICE: exist new io config.\n");

	return ret;
}

/* notice: how to merge io config
 * not supprt add io config,
 * this code just for checking io config.
 */
static void merge_io_config(soc_pwr_dep_t *ppwr_dep,
				soc_io_para_t *ptmp_io_para)
{
	int i = 0;
	int j = 0;

	/* unhold_flag has higher level priority. */
	ptmp_io_para->hold_flag &= ppwr_dep->soc_io_state.hold_flag;

	for (j = 0; j < IO_NUM; j++) {
		/* new added */
		if (ppwr_dep->soc_io_state.io_state[j].paddr == 0) {
			pr_info("io config is not in effect.\n");
			continue;
		}

		for (i = 0; i < IO_NUM; i++) {
			if (merge_io(ppwr_dep, j, ptmp_io_para, i) != 0)
				continue;
			else
				break;
		}
	}
}

/* only update voltage when new config has pwr on info;
 * for stable reason, remain the higher voltage;
 */
static void merge_volt_config(soc_pwr_dep_t *ppwr_dep,
				pwr_dm_state_t *ptmp_pwr_dep)
{
	int i = 0;

	if ((ptmp_pwr_dep->state & ptmp_pwr_dep->sys_mask) != 0) {
		for (i = 0; i < VCC_MAX_INDEX; i++) {
			if (ppwr_dep->soc_pwr_dm_state.volt[i]
				> ptmp_pwr_dep->volt[i]) {
				ptmp_pwr_dep->volt[i] =
					ppwr_dep->soc_pwr_dm_state.volt[i];
			}
		}
	}
}

static void merge_clk_config(cpux_clk_para_t *ptmp_clk_para,
				cpux_clk_para_t *pclk_para,
				struct scene_extended_standby_t *standby_data)
{
	int i = 0;

	ptmp_clk_para->osc_en |= pclk_para->osc_en;

	/* 0 is disable, enable have higher priority. */
	ptmp_clk_para->init_pll_dis |= pclk_para->init_pll_dis;
	ptmp_clk_para->exit_pll_en |= pclk_para->exit_pll_en;

	if (pclk_para->pll_change) {
		for (i = 0; i < PLL_NUM; i++) {
			if (pclk_para->pll_change & (0x1 << i)) {
				if (!(ptmp_clk_para->pll_change & (0x1 << i))
					|| calculate_pll(i, standby_data))
					ptmp_clk_para->pll_factor[i] =
						pclk_para->pll_factor[i];
			}
		}

		ptmp_clk_para->pll_change |= pclk_para->pll_change;
	}

	if (pclk_para->bus_change) {
		for (i = 0; i < BUS_NUM; i++) {
			if (pclk_para->bus_change & (0x1 << i)) {
				if (!(ptmp_clk_para->bus_change & (0x1 << i))
					|| calculate_bus(i, standby_data))
					ptmp_clk_para->bus_factor[i] =
						pclk_para->bus_factor[i];
			}
		}

		ptmp_clk_para->bus_change |= pclk_para->bus_change;
	}
}

static int copy_extended_standby_data(struct scene_extended_standby_t *standby_data)
{
	int i = 0;
	cpux_clk_para_t *pclk_para = &standby_data->soc_pwr_dep.cpux_clk_state;
	cpux_clk_para_t *ptmp_clk_para = &temp_standby_data.cpux_clk_state;
	soc_pwr_dep_t *ppwr_dep = &standby_data->soc_pwr_dep;
	pwr_dm_state_t *ptmp_pwr_dep = &temp_standby_data.soc_pwr_dm_state;
	soc_io_para_t *ptmp_io_para = &temp_standby_data.soc_io_state;

	if (!standby_data) {
		temp_standby_data.id = 0;
		ptmp_pwr_dep->state = 0;
		ptmp_pwr_dep->sys_mask = 0;

		memset(&ptmp_pwr_dep->volt, 0, sizeof(ptmp_pwr_dep->volt));

		ptmp_clk_para->osc_en = 0;
		ptmp_clk_para->init_pll_dis = 0;
		ptmp_clk_para->exit_pll_en = 0;
		ptmp_clk_para->pll_change = 0;
		ptmp_clk_para->bus_change = 0;

		memset(&ptmp_clk_para->pll_factor, 0,
				sizeof(ptmp_clk_para->pll_factor));
		memset(&ptmp_clk_para->bus_factor, 0,
				sizeof(ptmp_clk_para->bus_factor));

		ptmp_io_para->hold_flag = 0;
		memset(&ptmp_io_para->io_state, 0,
				sizeof(ptmp_io_para->io_state));

		temp_standby_data.soc_dram_state.selfresh_flag = 0;
	} else {
		if ((temp_standby_data.id != 0)
			&& (!((ppwr_dep->id) & (temp_standby_data.id)))) {
			temp_standby_data.id |= ppwr_dep->id;
			ptmp_pwr_dep->state |= ppwr_dep->soc_pwr_dm_state.state;

			merge_volt_config(ppwr_dep, ptmp_pwr_dep);
			merge_clk_config(ptmp_clk_para, pclk_para, standby_data);
			merge_io_config(ppwr_dep, ptmp_io_para);

			/* un_selfresh_flag has higher level priority. */
			temp_standby_data.soc_dram_state.selfresh_flag &=
					ppwr_dep->soc_dram_state.selfresh_flag;
		} else if (temp_standby_data.id == 0) {
			/* update sys_mask:
			 * when scene_unlock happened or scene_lock cnt > 0
			 */
#if defined(CONFIG_AW_AXP)
			ptmp_pwr_dep->sys_mask = axp_get_sys_pwr_dm_mask();
#endif
			temp_standby_data.id = ppwr_dep->id;
			ptmp_pwr_dep->state = ppwr_dep->soc_pwr_dm_state.state;

			if ((ptmp_pwr_dep->state & ptmp_pwr_dep->sys_mask)
				!= 0) {
				for (i = 0; i < VCC_MAX_INDEX; i++)
					ptmp_pwr_dep->volt[i] =
					ppwr_dep->soc_pwr_dm_state.volt[i];
			} else {
				memset(&ptmp_pwr_dep->volt, 0,
						sizeof(ptmp_pwr_dep->volt));
			}

			ptmp_clk_para->osc_en = pclk_para->osc_en;
			ptmp_clk_para->init_pll_dis = pclk_para->init_pll_dis;
			ptmp_clk_para->exit_pll_en = pclk_para->exit_pll_en;
			ptmp_clk_para->pll_change = pclk_para->pll_change;

			if (pclk_para->pll_change != 0) {
				for (i = 0; i < PLL_NUM; i++) {
					ptmp_clk_para->pll_factor[i] =
						pclk_para->pll_factor[i];
				}
			} else {
				memset(&ptmp_clk_para->pll_factor, 0,
					sizeof(ptmp_clk_para->pll_factor));
			}

			ptmp_clk_para->bus_change = pclk_para->bus_change;
			if (pclk_para->bus_change != 0) {
				for (i = 0; i < BUS_NUM; i++) {
					ptmp_clk_para->bus_factor[i] =
						pclk_para->bus_factor[i];
				}
			} else {
				memset(&ptmp_clk_para->bus_factor, 0,
					sizeof(ptmp_clk_para->bus_factor));
			}

			ptmp_io_para->hold_flag =
					ppwr_dep->soc_io_state.hold_flag;

			for (i = 0; i < IO_NUM; i++)
				ptmp_io_para->io_state[i] =
					ppwr_dep->soc_io_state.io_state[i];

			temp_standby_data.soc_dram_state.selfresh_flag =
					ppwr_dep->soc_dram_state.selfresh_flag;
		}
	}

	return check_cfg();
}

/**
 *	get_extended_standby_manager -
 *	get the exstandby_manager pointer
 *
 *	Return	: if the exstandby_manager is effective,
 *		return the exstandby_manager pointer;
 *		else return NULL;
 *	Notes	: you can check the configuration from the pointer.
 */
const struct extended_standby_manager_t *get_extended_standby_manager(void)
{
	unsigned long irqflags;
	struct extended_standby_manager_t *manager_data = NULL;

	spin_lock_irqsave(&data_lock, irqflags);
	manager_data = &exstandby_manager;
	spin_unlock_irqrestore(&data_lock, irqflags);

	if ((manager_data != NULL)
		&& (manager_data->pextended_standby != NULL)) {
#if defined(CONFIG_AW_AXP)
		/* update sys_mask */
		manager_data->pextended_standby->soc_pwr_dm_state.sys_mask =
				axp_get_sys_pwr_dm_mask();
#endif
		pr_info("leave %s : id 0x%x\n", __func__,
				manager_data->pextended_standby->id);
	}

	return manager_data;
}

/**
 *	set_extended_standby_manager - set the exstandby_manager;
 *	manager@: the manager config.
 *
 *      return value: if the setting is correct, return true.
 *		      else return false;
 *      notes: the function will check
 *				the struct member: pextended_standby and event.
 *		if the setting is not proper, return false.
 */
bool set_extended_standby_manager(struct scene_extended_standby_t *local_standby)
{
	unsigned long irqflags;
	bool ret = true;

	pr_info("enter %s\n", __func__);

	if (local_standby
		&& (local_standby->soc_pwr_dep.soc_pwr_dm_state.state == 0))
		goto out;

	if (!local_standby) {
		spin_lock_irqsave(&data_lock, irqflags);
		copy_extended_standby_data(NULL);
		exstandby_manager.pextended_standby = NULL;
		spin_unlock_irqrestore(&data_lock, irqflags);
		ret = false;
		goto out;
	} else {
		spin_lock_irqsave(&data_lock, irqflags);
		copy_extended_standby_data(local_standby);
		exstandby_manager.pextended_standby = &temp_standby_data;
		spin_unlock_irqrestore(&data_lock, irqflags);
	}

	if (exstandby_manager.pextended_standby != NULL)
		pr_info("leave %s : id 0x%x\n", __func__,
				exstandby_manager.pextended_standby->id);

out:
	return ret;
}

/**
 *	extended_standby_set_pmu_id   -     set pmu_id for suspend modules.
 *
 *	@num:	pmu serial number;
 *	@pmu_id: corresponding pmu_id;
 */
int extended_standby_set_pmu_id(unsigned int num, unsigned int pmu_id)
{
	unsigned int tmp;

	if (num > 4 || num < 0) {
		pr_err("num: %ux not valid.\n", num);
		return -1;
	}

	tmp = temp_standby_data.pmu_id;
	tmp &= ~(0xff << ((num) * 8));
	tmp |= (pmu_id << ((num) * 8));

	temp_standby_data.pmu_id = tmp;

	return 0;
}

/**
 *	extended_standby_get_pmu_id   -
 *  get specific pmu_id for suspend modules.
 *
 *	@num:	pmu serial number;
 */
int extended_standby_get_pmu_id(unsigned int num)
{
	unsigned int tmp;

	if (num > 4 || num < 0) {
		pr_err("num: %ux not valid.\n", num);
		return -1;
	}

	tmp = temp_standby_data.pmu_id;
	tmp >>= ((num) * 8);
	tmp &= (0xff);

	return tmp;
}

/**
 *	extended_standby_enable_wakeup_src   -	enable the wakeup src.
 *
 *	function:	the device driver care about the wakeup src.
 *				if the device driver do want the system
 *				be wakenup while in standby state.
 *				the device driver should use this function
 *              to enable corresponding intterupt.
 *	@src:		wakeup src.
 *	@para:		if wakeup src need para, be the para of wakeup src,
 *				else ignored.
 *	notice:		1. for gpio intterupt, only access the enable bit,
 *					mean u need care about other config,
 *					such as: int mode, pull up
 *						or pull down resistance, etc.
 *				2. At a31, only gpio��pa, pb, pe, pg,
 *                 pl, pm��int wakeup src is supported.
 */
int extended_standby_enable_wakeup_src(enum CPU_WAKEUP_SRC src, int para)
{
	unsigned long irqflags;
	unsigned long gpio_map = exstandby_manager.wakeup_gpio_map;
	unsigned long gpio_group = exstandby_manager.wakeup_gpio_group;

	spin_lock_irqsave(&data_lock, irqflags);
	exstandby_manager.event |= src;
	if (CPUS_GPIO_SRC & src) {
		if (para >= AXP_PIN_BASE) {
			gpio_map |= (WAKEUP_GPIO_AXP((para - AXP_PIN_BASE)));
		} else if (para >= SUNXI_PM_BASE) {
			gpio_map |= (WAKEUP_GPIO_PM((para - SUNXI_PM_BASE)));
		} else if (para >= SUNXI_PL_BASE) {
			gpio_map |= (WAKEUP_GPIO_PL((para - SUNXI_PL_BASE)));
		} else if (para >= SUNXI_PH_BASE) {
			gpio_group |= (WAKEUP_GPIO_GROUP('H'));
		} else if (para >= SUNXI_PG_BASE) {
			gpio_group |= (WAKEUP_GPIO_GROUP('G'));
		} else if (para >= SUNXI_PF_BASE) {
			gpio_group |= (WAKEUP_GPIO_GROUP('F'));
		} else if (para >= SUNXI_PE_BASE) {
			gpio_group |= (WAKEUP_GPIO_GROUP('E'));
		} else if (para >= SUNXI_PD_BASE) {
			gpio_group |= (WAKEUP_GPIO_GROUP('D'));
		} else if (para >= SUNXI_PC_BASE) {
			gpio_group |= (WAKEUP_GPIO_GROUP('C'));
		} else if (para >= SUNXI_PB_BASE) {
			gpio_group |= (WAKEUP_GPIO_GROUP('B'));
		} else if (para >= SUNXI_PA_BASE) {
			gpio_group |= (WAKEUP_GPIO_GROUP('A'));
		} else {
			pr_info("cpux need care gpio %d. not support it.\n",
				para);
		}
	}
	spin_unlock_irqrestore(&data_lock, irqflags);

	pr_info("leave %s : event 0x%lx\n", __func__, exstandby_manager.event);
	pr_info("leave %s : wakeup_gpio_map 0x%lx\n", __func__, gpio_map);
	pr_info("leave %s : wakeup_gpio_group 0x%lx\n", __func__, gpio_group);

	exstandby_manager.wakeup_gpio_group = gpio_group;
	exstandby_manager.wakeup_gpio_map = gpio_map;

	return 0;
}

/**
 *	extended_standby_disable_wakeup_src  -	disable the wakeup src.
 *
 *	function:	if the device driver do not want the system
 *               be wakenup while in standby state again.
 *			     the device driver should use this function
 *               to disable the corresponding intterupt.
 *
 *	@src:		wakeup src.
 *	@para:		if wakeup src need para, be the para of wakeup src,
 *			     else ignored.
 *	notice:		for gpio intterupt, only access the enable bit,
 *               mean u need care about other config,
 *			     such as: int mode, pull up or
 *               pull down resistance, etc.
 */
int extended_standby_disable_wakeup_src(enum CPU_WAKEUP_SRC src, int para)
{
	unsigned long irqflags;
	struct extended_standby_manager_t *pextend_mnger = &(exstandby_manager);
	unsigned long gpio_map = exstandby_manager.wakeup_gpio_map;
	unsigned long gpio_group = exstandby_manager.wakeup_gpio_group;

	spin_lock_irqsave(&data_lock, irqflags);
	pextend_mnger->event &= (~src);
	if (CPUS_GPIO_SRC & src) {
		if (para >= AXP_PIN_BASE) {
			gpio_map &= (~(WAKEUP_GPIO_AXP((para - AXP_PIN_BASE))));
		} else if (para >= SUNXI_PM_BASE) {
			gpio_map &= (~(WAKEUP_GPIO_PM((para - SUNXI_PM_BASE))));
		} else if (para >= SUNXI_PL_BASE) {
			gpio_map &= (~(WAKEUP_GPIO_PL((para - SUNXI_PL_BASE))));
		} else if (para >= SUNXI_PH_BASE) {
			gpio_group &= (~(WAKEUP_GPIO_GROUP('H')));
		} else if (para >= SUNXI_PG_BASE) {
			gpio_group &= (~(WAKEUP_GPIO_GROUP('G')));
		} else if (para >= SUNXI_PF_BASE) {
			gpio_group &= (~(WAKEUP_GPIO_GROUP('F')));
		} else if (para >= SUNXI_PE_BASE) {
			gpio_group &= (~(WAKEUP_GPIO_GROUP('E')));
		} else if (para >= SUNXI_PD_BASE) {
			gpio_group &= (~(WAKEUP_GPIO_GROUP('D')));
		} else if (para >= SUNXI_PC_BASE) {
			gpio_group &= (~(WAKEUP_GPIO_GROUP('C')));
		} else if (para >= SUNXI_PB_BASE) {
			gpio_group &= (~(WAKEUP_GPIO_GROUP('B')));
		} else if (para >= SUNXI_PA_BASE) {
			gpio_group &= (~(WAKEUP_GPIO_GROUP('A')));
		} else {
			pr_info("cpux need care gpio %d. not support it.\n",
				para);
		}
	}
	spin_unlock_irqrestore(&data_lock, irqflags);

	pr_info("leave %s : event 0x%lx\n", __func__, pextend_mnger->event);
	pr_info("leave %s : wakeup_gpio_map 0x%lx\n", __func__, gpio_map);
	pr_info("leave %s : wakeup_gpio_group 0x%lx\n", __func__, gpio_group);

	exstandby_manager.wakeup_gpio_group = gpio_group;
	exstandby_manager.wakeup_gpio_map = gpio_map;

	return 0;
}

/**
 *	extended_standby_check_wakeup_state
 *        -   to get the corresponding
 *            wakeup src intterupt state, enable or disable.
 *
 *	@src:	wakeup src.
 *	@para:	if wakeup src need para, be the para of wakeup src,
 *		else ignored.
 *
 *	return value:
 *              enable,		return 1,
 *		disable,	return 2,
 *		error:		return -1.
 */
int extended_standby_check_wakeup_state(enum CPU_WAKEUP_SRC src, int para)
{
	unsigned long irqflags;
	int ret = -1;

	spin_lock_irqsave(&data_lock, irqflags);
	if (exstandby_manager.event & src)
		ret = 1;
	else
		ret = 2;
	spin_unlock_irqrestore(&data_lock, irqflags);

	return ret;
}

/**
 *
 *	function:		standby state including locked_scene,
 *  power_supply dependency, the wakeup src.
 *
 *	return value:
 *             succeed, return 0,
 *             else return -1.
 */
int extended_standby_show_state(void)
{
	unsigned long irqflags;
	int i = 0;
	unsigned int pwr_on_bitmap = 0;
	unsigned int pwr_off_bitmap = 0;
	extended_standby_t *pextend_standby =
				exstandby_manager.pextended_standby;
	cpux_clk_para_t *pclk_para = &pextend_standby->cpux_clk_state;

	standby_show_state();

	spin_lock_irqsave(&data_lock, irqflags);

	pr_info("dynamic config wakeup_src: 0x%16lx\n",
				exstandby_manager.event);
	parse_wakeup_event(NULL, 0, exstandby_manager.event);

	pr_info("wakeup_gpio_map 0x%16lx\n", exstandby_manager.wakeup_gpio_map);
	parse_wakeup_gpio_map(NULL, 0, exstandby_manager.wakeup_gpio_map);

	pr_info("wakeup_gpio_group 0x%16lx\n",
				exstandby_manager.wakeup_gpio_group);
	parse_wakeup_gpio_group_map(NULL, 0,
				exstandby_manager.wakeup_gpio_group);

	if (pextend_standby) {
		pr_info("extended_standby id = 0x%16x\n", pextend_standby->id);
		pr_info("extended_standby pmu_id = 0x%16x\n",
				pextend_standby->pmu_id);
		pr_info("extended_standby soc_id = 0x%16x\n",
				pextend_standby->soc_id);
		pr_info("extended_standby pwr dep as follow:\n");
		pr_info("pwr dm state as follow:\n");

		pr_info("    pwr dm state = 0x%8x.\n",
				pextend_standby->soc_pwr_dm_state.state);
		parse_pwr_dm_map(NULL, 0,
				pextend_standby->soc_pwr_dm_state.state);

		pr_info("    pwr dm sys mask = 0x%8x.\n",
				pextend_standby->soc_pwr_dm_state.sys_mask);
		parse_pwr_dm_map(NULL, 0,
				pextend_standby->soc_pwr_dm_state.sys_mask);

		pwr_on_bitmap = pextend_standby->soc_pwr_dm_state.sys_mask
				& pextend_standby->soc_pwr_dm_state.state;
		pr_info("    pwr on = 0x%x.\n", pwr_on_bitmap);
		parse_pwr_dm_map(NULL, 0, pwr_on_bitmap);

		pwr_off_bitmap = (~pextend_standby->soc_pwr_dm_state.sys_mask)
				| pextend_standby->soc_pwr_dm_state.state;
		pr_info("    pwr off = 0x%x.\n", pwr_off_bitmap);
		parse_pwr_dm_map(NULL, 0, (~pwr_off_bitmap));


		if (pextend_standby->soc_pwr_dm_state.state
			& pextend_standby->soc_pwr_dm_state.sys_mask) {
			for (i = 0; i < VCC_MAX_INDEX; i++) {
				if (pextend_standby->soc_pwr_dm_state.volt[i]) {
					pr_info("pwr on volt which need adjusted:\n");
					pr_info("    index = %d, volt = %d.\n",
						i, pextend_standby->soc_pwr_dm_state.volt[i]);
				}
			}
		}

		pr_info("cpux clk state as follow:\n");
		pr_info("    cpux osc en: 0x%8x.\n", pclk_para->osc_en);
		pr_info("    cpux pll init disabled config: 0x%8x.\n",
						pclk_para->init_pll_dis);
		pr_info("    cpux pll exit enable config: 0x%8x.\n",
						pclk_para->exit_pll_en);

		if (pclk_para->pll_change) {
			for (i = 0; i < PLL_NUM; i++)
				pr_info("    pll%i:f1=%d f2=%d f3=%d f4=%d\n",
					i, pclk_para->pll_factor[i].factor1,
					pclk_para->pll_factor[i].factor2,
					pclk_para->pll_factor[i].factor3,
					pclk_para->pll_factor[i].factor4);
		} else {
			pr_info("    pll_change = 0: no pll need change.\n");
		}

		if (pclk_para->bus_change) {
			for (i = 0; i < BUS_NUM; i++)
				pr_info("    bus%i: src=%d pre_div=%d div_ratio=%d n=%d m=%d\n", i,
					pclk_para->bus_factor[i].src,
					pclk_para->bus_factor[i].pre_div,
					pclk_para->bus_factor[i].div_ratio,
					pclk_para->bus_factor[i].n,
					pclk_para->bus_factor[i].m);
		} else {
			pr_info("    bus_change = 0: no bus need change.\n");
		}

		pr_info("cpux io state as follow:\n");
		pr_info("    hold_flag = %d.\n",
				pextend_standby->soc_io_state.hold_flag);

		for (i = 0; i < IO_NUM; i++) {
			if (pextend_standby->soc_io_state.io_state[i].paddr)
				pr_info("    count %4d io config: addr 0x%x, value_mask 0x%8x, value 0x%8x\n", i,
					pextend_standby->soc_io_state.io_state[i].paddr,
					pextend_standby->soc_io_state.io_state[i].value_mask,
					pextend_standby->soc_io_state.io_state[i].value);
		}

		pr_info("soc dram state as follow:\n");
		pr_info("    selfresh_flag = %d.\n",
				pextend_standby->soc_dram_state.selfresh_flag);

	}

	spin_unlock_irqrestore(&data_lock, irqflags);

	return 0;
}

