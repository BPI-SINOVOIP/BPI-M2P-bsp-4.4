/*
 *    Filename: cve.h
 *     Version: 1.0
 * Description: Some include file and define used in driver.
 *     License: GPLv2
 *
 *		Author  : gaofeng <gaofeng@allwinnertech.com>
 *		Date    : 2017/04/07
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#ifndef CVE_H
#define CVE_H
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/preempt.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/rmap.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <linux/mm.h>
#include <asm/siginfo.h>
#include <asm/signal.h>
#include <linux/clk/sunxi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include "cve_api_include.h"
#include <linux/regulator/consumer.h>

#define DEVICE_NAME "sunxi_cve"

#define CVE_REGISTER_LENGTH			255

#define CVE_BASE				0x01600000

#define CCMU_BASE				0x03001000
#define PLL_CVE_CONTROL_REGISTER		0x00c8
#define CVE_CLOCK_REGISTER			0x0660
#define REG_AHB_CVE				0x066c
#define REG_MCLK_SYS				0x0804

/*****************************************************/
/*    Bit Position Definition                        */
/*****************************************************/
#define BIT0      		0x00000001
#define BIT1		  	0x00000002
#define BIT2		  	0x00000004
#define BIT3		  	0x00000008
#define BIT4		  	0x00000010
#define BIT5		  	0x00000020
#define BIT6		  	0x00000040
#define BIT7		  	0x00000080
#define BIT8		  	0x00000100
#define BIT9		  	0x00000200
#define BIT10		  	0x00000400
#define BIT11		  	0x00000800
#define BIT12		  	0x00001000
#define BIT13		  	0x00002000
#define BIT14		  	0x00004000
#define BIT15		  	0x00008000
#define BIT16		  	0x00010000
#define BIT17		  	0x00020000
#define BIT18		  	0x00040000
#define BIT19		  	0x00080000
#define BIT20		  	0x00100000
#define BIT21		  	0x00200000
#define BIT22		  	0x00400000
#define BIT23		  	0x00800000
#define BIT24		  	0x01000000
#define BIT25		  	0x02000000
#define BIT26		  	0x04000000
#define BIT27		  	0x08000000
#define BIT28		  	0x10000000
#define BIT29		  	0x20000000
#define BIT30		  	0x40000000
#define BIT31		  	0x80000000

#endif
