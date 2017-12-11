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
#include <common.h>
#include <asm/io.h>
#include <asm/arch/smc.h>

/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static uint __tzasc_calc_2_power(uint data)
{
	uint ret = 0;

	do {
		data >>= 1;
		ret++;
	} while (data);

	ret = ret - 1;
	return ret;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :  dram_start: e.g. 0x20000000
*
*    return        :  dram_size : Mbytes
*
*    note          :  secure_region_size: Mbytes
*
*
************************************************************************************************************
*/
int sunxi_smc_config(uint dram_size, uint secure_region_size)
{
	uint region_size, permission, region_start;

	writel(0, SMC_MASTER_BYPASS0_REG);
	writel(0xffffffff, SMC_MASTER_SECURITY0_REG);
	region_size = (__tzasc_calc_2_power(dram_size*1024/32) + 0b001110) << 1;
	permission  = 0b1111 << 28;

	writel(0, SMC_REGIN_SETUP_LOW_REG(1));
	writel(0, SMC_REGIN_SETUP_HIGH_REG(1));
	writel(permission | region_size | 1 , SMC_REGIN_ATTRIBUTE_REG(1));

	region_size = (__tzasc_calc_2_power(secure_region_size*1024/32) + 0b001110) << 1;
	permission  = 0b1100 << 28;

	region_start = dram_size - secure_region_size;
	if (region_start <= (4 * 1024)) {
		writel((region_start * 1024 * 1024) & 0xffff8000, SMC_REGIN_SETUP_LOW_REG(2));
		writel(0, SMC_REGIN_SETUP_HIGH_REG(2));
	} else {
		unsigned long long long_regin_start;
		long_regin_start = region_start;
		long_regin_start = long_regin_start * 1024 * 1024;
		writel(long_regin_start & 0xffff8000, SMC_REGIN_SETUP_LOW_REG(2));
		writel((long_regin_start >> 32) & 0xffffffff, SMC_REGIN_SETUP_HIGH_REG(2));
	}
	/*0 not enable, drm ta enable later*/
	writel(permission | region_size | 0 , SMC_REGIN_ATTRIBUTE_REG(2));
	return 0;
}

/*becasue of secure boot benifit, it is ok to config here*/
int sunxi_drm_config(u32 dram_end, u32 secure_region_size)
{
	if (dram_end > secure_region_size)
	{
		writel((dram_end - secure_region_size), SMC_LOW_SADDR_REG);
		writel(dram_end, SMC_LOW_EADDR_REG);
	}
	return 0;
}

