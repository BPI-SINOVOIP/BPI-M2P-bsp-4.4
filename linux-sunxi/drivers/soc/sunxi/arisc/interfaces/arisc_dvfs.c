/*
 *  drivers/arisc/interfaces/arisc_dvfs.c
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

#include "../arisc_i.h"
#include "linux/cpufreq.h"
#if defined CONFIG_SUNXI_ARISC_COM_DIRECTLY
int arisc_dvfs_cfg_vf_table(unsigned int cluster, unsigned int vf_num,
				void *vf)
{
	int index = 0;

	struct arisc_message *pmessage;
	/* allocate a message frame */
	pmessage = arisc_message_allocate(ARISC_MESSAGE_ATTR_HARDSYN);
	if (pmessage == NULL) {
		ARISC_WRN("allocate message failed\n");
		return -ENOMEM;
	}

	for (index = 0; index < vf_num; index++) {
		/* initialize message
		 *
		 * |paras[0]|paras[1]|paras[2]|paras[3]|paras[4]|
		 * | index  |  freq  |voltage |axi_div | cluster|
		 */
		pmessage->type       = ARISC_CPUX_DVFS_CFG_VF_REQ;
		pmessage->paras[0]   = index;
		pmessage->paras[1]   =
			((struct cpufreq_dvfs_table *)(vf) + index)->freq;
		pmessage->paras[2]   =
		((struct cpufreq_dvfs_table *)(vf) + index)->voltage;
		pmessage->paras[3]   =
			((struct cpufreq_dvfs_table *)(vf) + index)->axi_div;

		pmessage->state      = ARISC_MESSAGE_INITIALIZED;
		pmessage->cb.handler = NULL;
		pmessage->cb.arg     = NULL;

		ARISC_INF("v-f table: index %d freq %d vol %d axi_div %d\n",
			pmessage->paras[0], pmessage->paras[1],
			pmessage->paras[2],
			pmessage->paras[3]);

		/* send request message */
		arisc_hwmsgbox_send_message(pmessage, ARISC_SEND_MSG_TIMEOUT);

		/* check config fail or not */
		if (pmessage->result) {
			ARISC_WRN("config dvfs v-f table [%d] fail\n",
				index);
			return -EINVAL;
		}
	}

	/* free allocated message */
	arisc_message_free(pmessage);

	return 0;
}
EXPORT_SYMBOL(arisc_dvfs_cfg_vf_table);

/*
 * set specific pll target frequency.
 * @freq:    target frequency to be set, based on KHZ;
 * @pll:     which pll will be set
 * @mode:    the attribute of message, whether syn or asyn;
 * @cb:      callback handler;
 * @cb_arg:  callback handler arguments;
 *
 * return: result, 0 - set frequency successed,
 *                !0 - set frequency failed;
 */
int arisc_dvfs_set_cpufreq(unsigned int freq, unsigned int pll, unsigned int mode, arisc_cb_t cb, void *cb_arg)
{
	unsigned int          msg_attr = 0;
	struct arisc_message *pmessage;
	int                   result = 0;
	if (mode & ARISC_DVFS_SYN)
		msg_attr |= ARISC_MESSAGE_ATTR_HARDSYN;

	/* allocate a message frame */
	pmessage = arisc_message_allocate(msg_attr);
	if (pmessage == NULL) {
		ARISC_WRN("allocate message failed\n");
		return -ENOMEM;
	}

	/* initialize message
	 *
	 * |paras[0]|paras[1]|
	 * |freq    |pll     |
	 */
	pmessage->type       = ARISC_CPUX_DVFS_REQ;
	pmessage->paras[0]   = freq;
	pmessage->paras[1]   = pll;
	pmessage->state      = ARISC_MESSAGE_INITIALIZED;
	pmessage->cb.handler = cb;
	pmessage->cb.arg     = cb_arg;

	ARISC_INF("arisc dvfs request : %d\n", freq);
	arisc_hwmsgbox_send_message(pmessage, ARISC_SEND_MSG_TIMEOUT);

	/* dvfs mode : syn or not */
	if (mode & ARISC_DVFS_SYN) {
		result = pmessage->result;
		arisc_message_free(pmessage);
	}

	return result;
}
EXPORT_SYMBOL(arisc_dvfs_set_cpufreq);
#else
/*
 * set specific pll target frequency.
 * @freq:    target frequency to be set, based on KHZ;
 * @pll:     which pll will be set
 * @mode:    the attribute of message, whether syn or asyn;
 * @cb:      callback handler;
 * @cb_arg:  callback handler arguments;
 *
 * return: result, 0 - set frequency successed,
 *                !0 - set frequency failed;
 */
int arisc_dvfs_set_cpufreq(unsigned int freq, unsigned int pll, unsigned int mode, arisc_cb_t cb, void *cb_arg)
{
	int                   ret = 0;

	ARISC_INF("arisc dvfs request : %d\n", freq);
	ret = invoke_scp_fn_smc(ARM_SVC_ARISC_CPUX_DVFS_REQ, (u64)freq, (u64)pll, (u64)mode);
	if (ret) {
		if (cb == NULL) {
			ARISC_WRN("callback not install\n");
		} else {
			/* call callback function */
			ARISC_WRN("call the callback function\n");
			(*(cb))(cb_arg);
		}
	}

	return ret;
}

EXPORT_SYMBOL(arisc_dvfs_set_cpufreq);

int arisc_dvfs_cfg_vf_table(unsigned int cluster, unsigned int vf_num,
				void *vf_tbl)
{
	int result;

	result = invoke_scp_fn_smc(ARM_SVC_ARISC_CPUX_DVFS_CFG_VF_REQ, cluster,
			vf_num, (u64)vf_tbl);

	return 0;
}
EXPORT_SYMBOL(arisc_dvfs_cfg_vf_table);
#endif
