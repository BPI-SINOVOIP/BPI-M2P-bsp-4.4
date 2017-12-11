/*
 * drivers/soc/sunxi/pm/standby/standby_clock.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "standby.h"
#include "standby_clock.h"
#include "../resource/mem_clk.h"
#include "../resource/mem_timing.h"
#include "../pm_config.h"

static struct __ccmu_reg_list_t *CmuReg;

static union __ccmu_pll1_reg0000_t CmuReg_Pll1Ctl_tmp;
static union __ccmu_sysclk_ratio_reg0050_t CmuReg_SysClkDiv_tmp;
#define PIO_INT_DEB_REG (AW_GPIO_BASE_PA + 0x258)
static __u32 pio_int_deb_back;
static union __ccmu_ahb1_ratio_reg0054_t CmuReg_ahb1_tmp;
static union __ccmu_ahb1_ratio_reg0054_t CmuReg_ahb1_backup;
static union __ccmu_apb2_ratio_reg0058_t CmuReg_apb2_tmp;
static union __ccmu_apb2_ratio_reg0058_t CmuReg_apb2_backup;

#ifdef CONFIG_ARCH_SUN8IW10P1
static __u32 CmuReg_ee_backup;
static __u32 CmuReg_de_backup;
#endif

__s32 standby_clk_init(void)
{
	CmuReg = (struct __ccmu_reg_list_t *) IO_ADDRESS(AW_CCM_BASE);

	return 0;
}

__s32 standby_clk_exit(void)
{
	return 0;
}

__s32 standby_clk_core2losc(void)
{
	unsigned int tmp;

	/*CmuReg->SysClkDiv.CpuClkSrc = 0; */
	/* cpu frequency is internal Losc hz */
	tmp = readl(&(CmuReg->SysClkDiv));
	tmp &= (~(0x00030000));
	writel(tmp, &(CmuReg->SysClkDiv));

	return 0;
}

__s32 standby_clk_core2hosc(void)
{
	CmuReg_SysClkDiv_tmp.dwval = CmuReg->SysClkDiv.dwval;
	CmuReg_SysClkDiv_tmp.bits.CpuClkSrc = AC327_CLKSRC_HOSC; /*26M OSC */
	CmuReg->SysClkDiv.dwval = CmuReg_SysClkDiv_tmp.dwval;

	return 0;
}

__s32 standby_clk_core2pll(void)
{
	CmuReg_SysClkDiv_tmp.dwval = CmuReg->SysClkDiv.dwval;
	CmuReg_SysClkDiv_tmp.bits.CpuClkSrc = AC327_CLKSRC_PLL1; /*pll cpu */
	CmuReg->SysClkDiv.dwval = CmuReg_SysClkDiv_tmp.dwval;

	return 0;
}

__s32 standby_clk_plldisable(void)
{
	unsigned int tmp;
#if 0
	/*use old mode */
	/*pll lock */
	tmp = readl(&(CmuReg->PllLockCtl));
	tmp |= ((0x10000000));
	writel(tmp, &(CmuReg->Pll1Ctl));
#endif

	/*pll1 */
	tmp = readl(&(CmuReg->Pll1Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll1Ctl));

	/*pll6: for periph0 */
	tmp = readl(&(CmuReg->Pll6Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll6Ctl));

#ifndef CONFIG_ARCH_SUN8IW11P1
	/*pll9: periph 1 */
	tmp = readl(&(CmuReg->Pll9Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll9Ctl));

	/*pll 24M: */
	tmp = readl(&(CmuReg->Pll24M));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll24M));
#else
	/*pll9: periph 1 */
	tmp = readl(&(CmuReg->Pll7Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll7Ctl));
#endif

#ifdef CONFIG_ARCH_SUN8IW10P1
	/*pll2: for audio */
	tmp = readl(&(CmuReg->Pll2Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll2Ctl));

	/*pll3: for video */
	tmp = readl(&(CmuReg->Pll3Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll3Ctl));

	/*pll10: for de */
	tmp = readl(&(CmuReg->Pll10Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll10Ctl));
#else
	/*pll4: for ve */
	tmp = readl(&(CmuReg->Pll4Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll4Ctl));

	/*pll5: for ddr */
	tmp = readl(&(CmuReg->Pll5Ctl));
	tmp &= (~(0x80000000));
	writel(tmp, &(CmuReg->Pll5Ctl));
#endif

	return 0;
}

__s32 standby_clk_pllenable(void)
{
	unsigned int tmp;

#ifndef CONFIG_ARCH_SUN8IW11P1
	/*pll 24M: */
	tmp = readl(&(CmuReg->Pll24M));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll24M));
#endif

	/*pll1 */
	tmp = readl(&(CmuReg->Pll1Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll1Ctl));

	/*pll6: for periph0 */
	tmp = readl(&(CmuReg->Pll6Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll6Ctl));

#ifndef CONFIG_ARCH_SUN8IW11P1
	/*pll9: periph 1 */
	tmp = readl(&(CmuReg->Pll9Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll9Ctl));

	/*pll10: de */
	tmp = readl(&(CmuReg->Pll10Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll10Ctl));
#else
	/*pll9: periph 1 */
	tmp = readl(&(CmuReg->Pll7Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll7Ctl));
#endif

#if 0
	/*pll2: for audio */
	tmp = readl(&(CmuReg->Pll2Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll2Ctl));

	/*pll3: for video */
	tmp = readl(&(CmuReg->Pll3Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll3Ctl));

	/*pll4: for ve */
	tmp = readl(&(CmuReg->Pll4Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll4Ctl));

	/*pll5: for ddr */
	tmp = readl(&(CmuReg->Pll5Ctl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->Pll5Ctl));

	/*pll7: for isp */
	tmp = readl(&(CmuReg->PllIspCtl));
	tmp |= ((0x80000000));
	writel(tmp, &(CmuReg->PllIspCtl));
#endif

	return 0;
}

#if 0
__s32 standby_clk_pll1enable(void)
{
	CmuReg_Pll1Ctl_tmp.dwval = CmuReg->Pll1Ctl.dwval;
	CmuReg_Pll1Ctl_tmp.bits.PLLEn = 1;
	CmuReg->Pll1Ctl.dwval = CmuReg_Pll1Ctl_tmp.dwval;

	return 0;
}
#endif

void standby_clk_set_keyfield(void)
{
	unsigned int tmp;

	/*keyfield set to: 0xa7 */
	tmp = readl((void *)(0xf1c00000 + 0xf4));
	tmp &= (~(0xff000000));
	tmp |= ((0xa7000000));
	writel(tmp, (0xf1c00000 + 0xf4));
}

void standby_clk_unset_keyfield(void)
{
	unsigned int tmp;

	/*keyfield set to: 0 */
	tmp = readl((0xf1c00000 + 0xf4));
	tmp &= (~(0xff000000));
	writel(tmp, (0xf1c00000 + 0xf4));

}

__s32 standby_clk_hoscdisable(void)
{
	unsigned int tmp;

	/*disable dcxo: band gap */
	tmp = readl((0xf1c20328));
	tmp &= (~(0x80000000));
	writel(tmp, (0xf1c20328));

	/*disable dcxo */
	tmp = readl((0xf1c20324));
	tmp &= (~(0x80020000));
	writel(tmp, (0xf1c20324));

	/*0xf1c00000 + 0xf4 (system_ctrl: pll ctrl reg1) */
	/*bit2: hosc; */
	/*bit1: ldo for analog; */
	/*bit0: ldo for digital */

	/*disable hosc */
	tmp = readl((0xf1c00000 + 0xf4));
	tmp &= (~(0x00000004));
	writel(tmp, (0xf1c00000 + 0xf4));

	return 0;
}

__s32 standby_clk_hoscenable(void)
{
	unsigned int tmp;

	/*0xf1c00000 + 0xf4 (system_ctrl: pll ctrl reg1) */
	/*bit2: hosc; */
	/*bit1: ldo for analog; */
	/*bit0: ldo for digital */

	/*enable hosc */
	tmp = readl((0xf1c00000 + 0xf4));
	tmp |= ((0x00000004));
	writel(tmp, (0xf1c00000 + 0xf4));

	/*enable dcxo */
	tmp = readl((0xf1c20324));
	tmp |= (0x80020000);
	writel(tmp, (0xf1c20324));

	/*enable dcxo: band gap */
	tmp = readl((0xf1c20328));
	tmp |= (0x80000000);
	writel(tmp, (0xf1c20328));

	return 0;
}

__s32 standby_clk_ldodisable(void)
{
	unsigned int tmp;

	/*0xf1c00000 + 0xf4 (system_ctrl: pll ctrl reg1) */
	/*bit2: hosc; */
	/*bit1: ldo for analog; */
	/*bit0: ldo for digital */

	/*disable ldo */
	tmp = readl((0xf1c00000 + 0xf4));
	tmp &= (~(0x00000003));
	writel(tmp, (0xf1c00000 + 0xf4));

	return 0;
}

__s32 standby_clk_ldoenable(void)
{
	unsigned int tmp;

	/*0xf1c00000 + 0xf4 (system_ctrl: pll ctrl reg1) */
	/*bit2: hosc; */
	/*bit1: ldo for analog; */
	/*bit0: ldo for digital */

	/*enable ldo */
	tmp = readl((0xf1c00000 + 0xf4));
	tmp |= ((0x00000003));
	writel(tmp, (0xf1c00000 + 0xf4));

	return 0;
}

__s32 standby_clk_setdiv(struct standby_clk_div_t *clk_div)
{
	unsigned int tmp;

	if (!clk_div)
		return -1;

	/*ahb1 */
	CmuReg_ahb1_tmp.dwval = CmuReg->Ahb1Div.dwval;
	CmuReg_ahb1_tmp.bits.Ahb1Div = clk_div->ahb_div;
	CmuReg_ahb1_tmp.bits.Ahb1PreDiv = clk_div->ahb_pre_div;
	CmuReg_ahb1_tmp.bits.Apb1Div = clk_div->apb_div;
	CmuReg->Ahb1Div.dwval = CmuReg_ahb1_tmp.dwval;

	/*axi */
	CmuReg_SysClkDiv_tmp.dwval = CmuReg->SysClkDiv.dwval;
	CmuReg_SysClkDiv_tmp.bits.AXIClkDiv = clk_div->axi_div;
	CmuReg->SysClkDiv.dwval = CmuReg_SysClkDiv_tmp.dwval;

	/*apb2 */
	CmuReg_apb2_tmp.dwval = CmuReg->Apb2Div.dwval;
	CmuReg_apb2_tmp.bits.DivM = clk_div->apb2_div;
	CmuReg_apb2_tmp.bits.DivN = clk_div->apb2_pre_div;
	CmuReg->Apb2Div.dwval = CmuReg_apb2_tmp.dwval;

	return 0;
}

__s32 standby_clk_getdiv(struct standby_clk_div_t *clk_div)
{
	if (!clk_div)
		return -1;

	/*axi */
	CmuReg_SysClkDiv_tmp.dwval = CmuReg->SysClkDiv.dwval;
	clk_div->axi_div = CmuReg_SysClkDiv_tmp.bits.AXIClkDiv;

	/*ahb1 */
	CmuReg_ahb1_tmp.dwval = CmuReg->Ahb1Div.dwval;
	clk_div->ahb_div = CmuReg_ahb1_tmp.bits.Ahb1Div;
	clk_div->apb_div = CmuReg_ahb1_tmp.bits.Apb1Div;
	clk_div->ahb_pre_div = CmuReg_ahb1_tmp.bits.Ahb1PreDiv;

	/*apb2 */
	CmuReg_apb2_tmp.dwval = CmuReg->Apb2Div.dwval;
	clk_div->apb2_div = CmuReg_apb2_tmp.bits.DivM;
	clk_div->apb2_pre_div = CmuReg_apb2_tmp.bits.DivN;

	return 0;
}

__s32 standby_clk_set_pll_factor(struct pll_factor_t *pll_factor)
{
	union __ccmu_pll1_reg0000_t pll1_ctrl;

	if (!pll_factor)
		return -1;

	pll1_ctrl.dwval = readl(&(CmuReg->Pll1Ctl));

	pll1_ctrl.bits.FactorN = pll_factor->FactorN;
	pll1_ctrl.bits.FactorK = pll_factor->FactorK;
	pll1_ctrl.bits.FactorM = pll_factor->FactorM;
	pll1_ctrl.bits.FactorP = pll_factor->FactorP;

	writel(pll1_ctrl.dwval, &(CmuReg->Pll1Ctl));
	/*busy_waiting(); */

	return 0;
}

__s32 standby_clk_get_pll_factor(struct pll_factor_t *pll_factor)
{
	union __ccmu_pll1_reg0000_t pll1_ctrl;

	if (!pll_factor)
		return -1;

	pll1_ctrl.dwval = readl(&(CmuReg->Pll1Ctl));

	pll_factor->FactorN = pll1_ctrl.bits.FactorN;
	pll_factor->FactorK = pll1_ctrl.bits.FactorK;
	pll_factor->FactorM = pll1_ctrl.bits.FactorM;
	pll_factor->FactorP = pll1_ctrl.bits.FactorP;

	/*busy_waiting(); */

	return 0;
}

__s32 standby_clk_apb2losc(void)
{
	unsigned int tmp;

	/*apb2 */
	CmuReg_apb2_tmp.dwval = CmuReg->Apb2Div.dwval;
	CmuReg_apb2_tmp.bits.ClkSrc = APB2_CLKSRC_LOSC;
	CmuReg->Apb2Div.dwval = CmuReg_apb2_tmp.dwval;

	return 0;
}

__s32 standby_clk_apb2hosc(void)
{
	unsigned int tmp;

	/*apb2 */
	CmuReg_apb2_tmp.dwval = CmuReg->Apb2Div.dwval;
	CmuReg_apb2_tmp.bits.ClkSrc = APB2_CLKSRC_HOSC;
	CmuReg->Apb2Div.dwval = CmuReg_apb2_tmp.dwval;

	return 0;
}

__s32 standby_clk_bus_src_backup(void)
{
	/*backup bus src cfg */
	/* backup ahb clk src */
	CmuReg_ahb1_backup.dwval = CmuReg->Ahb1Div.dwval;
	/* backup apb clk src */
	CmuReg_apb2_backup.dwval = CmuReg->Apb2Div.dwval;

#ifdef CONFIG_ARCH_SUN8IW10P1
	/* backup de/ee clk src */
	CmuReg_de_backup = CmuReg->De0;
	CmuReg_ee_backup = CmuReg->Ee0;
#endif

	return 0;
}

__s32 standby_clk_bus_src_set(void)
{
	/* change ahb1 clock to axi */
	CmuReg_ahb1_tmp.dwval = CmuReg->Ahb1Div.dwval;
	CmuReg_ahb1_tmp.bits.Ahb1ClkSrc = AHB1_CLKSRC_AXI;
	CmuReg->Ahb1Div.dwval = CmuReg_ahb1_tmp.dwval;
	/* printk("CmuReg_ahb1_backup, %x!\n", CmuReg_ahb1_backup); */

#ifdef CONFIG_ARCH_SUN8IW10P1
	/* switch de clk to ahb */
	CmuReg->De0 = 0x82000000;
#endif

	return 0;
}

__s32 standby_clk_bus_src_restore(void)
{
	/* restore ahb clk src */
	CmuReg->Ahb1Div.dwval = CmuReg_ahb1_backup.dwval;
	/* restore apb clk src */
	CmuReg->Apb2Div.dwval = CmuReg_apb2_backup.dwval;

#ifdef CONFIG_ARCH_SUN8IW10P1
	/* retore de/ee clk */
	CmuReg->De0 = CmuReg_de_backup;
	CmuReg->Ee0 = CmuReg_ee_backup;
#endif

	return 0;
}

void standby_clk_dramgating(int onoff)
{
	unsigned int tmp = readl(&(CmuReg->Pll5Ctl));

	if (onoff)
		tmp |= (0x80000000);
	else
		tmp &= (~0x80000000);

	writel(tmp, &(CmuReg->Pll5Ctl));
}

__u32 standby_clk_get_cpu_freq(void)
{
	__u32 FactorN = 1;
	__u32 FactorK = 1;
	__u32 FactorM = 1;
	__u32 FactorP = 1;
	__u32 reg_val = 0;
	__u32 cpu_freq = 0;

	CmuReg_SysClkDiv_tmp.dwval = CmuReg->SysClkDiv.dwval;
	/*get runtime freq: clk src + divider ratio */
	/*src selection */
	reg_val = CmuReg_SysClkDiv_tmp.bits.CpuClkSrc;
	if (reg_val == 0) {
		/*32khz osc */
		cpu_freq = 32;
	} else if (reg_val == 1) {
		/*hosc, 24Mhz */
		cpu_freq = 24000;	/*unit is khz */
	} else if (reg_val == 2 || reg_val == 3) {
		CmuReg_Pll1Ctl_tmp.dwval = CmuReg->Pll1Ctl.dwval;
		FactorN = CmuReg_Pll1Ctl_tmp.bits.FactorN + 1;
		FactorK = CmuReg_Pll1Ctl_tmp.bits.FactorK + 1;
		FactorM = CmuReg_Pll1Ctl_tmp.bits.FactorM + 1;
		FactorP = 1 << (CmuReg_Pll1Ctl_tmp.bits.FactorP);
		cpu_freq = raw_lib_udiv(24000 * FactorN * FactorK,
				FactorP * FactorM);
	}

	/* printk("cpu_freq = dec(%d).\n", cpu_freq); */
	/*busy_waiting(); */

	return cpu_freq;
}
