/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */
/*
**********************************************************************************************************************
*
*						           the Embedded Secure Bootloader System
*
*
*						       Copyright(C), 2006-2014, Allwinnertech Co., Ltd.
*                                           All Rights Reserved
*
* File    :
*
* By      :
*
* Version : V2.00
*
* Date	  :
*
* Descript:
**********************************************************************************************************************
*/

#ifndef _TZASC_SMC_H_
#define _TZASC_SMC_H_

#include <asm/arch/platform.h>

#define SMC_CONFIG_REG                (SUNXI_SMC_BASE + 0x0000)
#define SMC_ACTION_REG                (SUNXI_SMC_BASE + 0x0004)
#define SMC_LKDW_RANGE_REG            (SUNXI_SMC_BASE + 0x0008)
#define SMC_LKDW_SELECT_REG           (SUNXI_SMC_BASE + 0x000c)
#define SMC_INT_STATUS_REG            (SUNXI_SMC_BASE + 0x0010)
#define SMC_INT_CLEAR_REG             (SUNXI_SMC_BASE + 0x0014)

#define SMC_MASTER_BYPASS0_REG        (SUNXI_SMC_BASE + 0x0018)
#define SMC_MASTER_SECURITY0_REG      (SUNXI_SMC_BASE + 0x001c)
#define SMC_FAIL_ADDR_REG             (SUNXI_SMC_BASE + 0x0020)
#define SMC_FAIL_CTRL_REG             (SUNXI_SMC_BASE + 0x0028)
#define SMC_FAIL_ID_REG               (SUNXI_SMC_BASE + 0x002c)

#define SMC_SPECULATION_CTRL_REG      (SUNXI_SMC_BASE + 0x0030)
#define SMC_INVER_EN_REG              (SUNXI_SMC_BASE + 0x0034)

#define SMC_MST_ATTRI_REG             (SUNXI_SMC_BASE + 0x0048)
#define SMC_DRAM_EN_REG               (SUNXI_SMC_BASE + 0x0050)

#define SMC_DRAM_ILLEGAL_ACCESS0_REG  (SUNXI_SMC_BASE + 0x0058)

#define SMC_LOW_SADDR_REG             (SUNXI_SMC_BASE + 0x0060)
#define SMC_LOW_EADDR_REG             (SUNXI_SMC_BASE + 0x0068)

#define SMC_REGIN_SETUP_LOW_REG(x)    (SUNXI_SMC_BASE + 0x100 + 0x10*(x))
#define SMC_REGIN_SETUP_HIGH_REG(x)   (SUNXI_SMC_BASE + 0x104 + 0x10*(x))
#define SMC_REGIN_ATTRIBUTE_REG(x)    (SUNXI_SMC_BASE + 0x108 + 0x10*(x))

#define SUNXI_SMC_PBASE SUNXI_SMC_BASE

#define SUNXI_SMC_MASTER_BYPASS(id)   (SUNXI_SMC_PBASE + 0x18)
#define SUNXI_SMC_MASTER_SECURITY(id) (SUNXI_SMC_PBASE + 0x1C)
#define SUNXI_SMC_DRM_ENABLE          (SUNXI_SMC_PBASE + 0x50)
#define SUNXI_SMC_DRM_START_ADDR      (SUNXI_SMC_PBASE + 0x60)
#define SUNXI_SMC_DRM_END_ADDR        (SUNXI_SMC_PBASE + 0x68)

typedef enum hw_master_id
{
	/* master name          id_number */
	SMC_MASTER_CPU = 0 	,	//0
	SMC_MASTER_GPU     	,	//1
	SMC_MASTER_CPUS	   	,	//2
	SMC_MASTER_ATH	   	,	//3
	SMC_MASTER_USBOTG  	,	//4
	SMC_MASTER_MMC0	   	,	//5
	SMC_MASTER_MMC1   	,	//6
	SMC_MASTER_MMC2	   	,	//7
	SMC_MASTER_USB0   	,	//8
	SMC_MASTER_USB1   	,	//9
	SMC_MASTER_EMAC   	,	//10

	SMC_MASTER_VE = 12  ,	//12
	SMC_MASTER_CSI   	,	//13
	SMC_MASTER_NAND	   	,	//14
	SMC_MASTER_SS   	,	//15
	SMC_MASTER_DE_RT_MIXER0	,	//16
	SMC_MASTER_DE_RT_MIXER1	,	//17
	SMC_MASTER_DE_RT_WB     ,	//18

	SMC_MASTER_USB3 = 20    ,	//20
	SMC_MASTER_TS       ,	//21
	SMC_MASTER_DE_INTERLACE ,	//22
	SMC_MASTER_MAX       	//23
} hw_master_id_e;

typedef enum hw_drm_master_id
{
	DRM_MASTER_VE_DEC = 0,

	DRM_MASTER_DE_RT_MIXER0 = 4,
	DRM_MASTER_DE_RT_MIXER1,
	DRM_MASTER_DE_RT_WB,
	DRM_MASTER_DE_ROT,

	DRM_MASTER_GPU_WRITE_EN = 12,
	DRM_MASTER_GPU_READ_EN,

	DRM_MASTER_MAX    ,

} hw_drm_master_id_e;


int sunxi_smc_config(uint dram_size, uint secure_region_size);
int sunxi_drm_config(u32 drm_start, u32 dram_size);

#endif    /*  #ifndef _TZASC_SMC_H_  */
