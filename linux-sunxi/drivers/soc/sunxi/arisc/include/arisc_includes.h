/*
 *  arch/arm/mach-sunxi/arisc/include/arisc_includes.h
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

#ifndef __ARISC_INCLUDES_H
#define __ARISC_INCLUDES_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/delay.h>

/* configure and debugger */
#include "./../arisc_i.h"
#include "./arisc_cfgs.h"
#include "./arisc_dbgs.h"
#include "./arisc_para.h"
#include "./arisc_type.h"
#include "./arisc_messages.h"
#include "./arisc_hwmsgbox.h"
/* global functions */
#if defined CONFIG_SUNXI_ARISC_COM_DIRECTLY
extern int arisc_axp_int_notify(struct arisc_message *pmessage);
extern int arisc_audio_perdone_notify(struct arisc_message *pmessage);
extern int arisc_report_error_info(struct arisc_message *pmessage);
#endif
extern int arisc_set_debug_level(unsigned int level);
extern int arisc_dvfs_cfg_vf_table(unsigned int cluster, unsigned int vf_num,
					void *vf);
extern int arisc_set_uart_baudrate(u32 baudrate);
extern int arisc_set_dram_crc_paras(unsigned int dram_crc_en, unsigned int dram_crc_srcaddr, unsigned int dram_crc_len);
extern int arisc_sysconfig_sstpower_paras(void);
extern void flush_message_pool(void);

/* global vars */
extern unsigned long arisc_sram_a2_vbase;
extern struct arisc_cfg arisc_cfg;

#endif /* __ARISC_INCLUDES_H */
